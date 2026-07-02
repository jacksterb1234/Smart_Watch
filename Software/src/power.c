/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "power.h"

#include <errno.h>
#include <stddef.h>

#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/npm13xx_charger.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "display_ui.h"

LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);

#define PMIC_CHARGER_NODE DT_NODELABEL(pmic_charger)

#define POWER_SAMPLE_PERIOD_MS 30000
#define POWER_ERROR_LOG_INTERVAL 8U

static const struct device *const pmic_charger = DEVICE_DT_GET(PMIC_CHARGER_NODE);

static struct power_status latest_status;
static struct k_spinlock status_lock;
static bool power_ready;
static uint32_t sample_error_count;

struct soc_point {
	int16_t mv;
	uint8_t percent;
};

static const struct soc_point lipo_soc_table[] = {
	{ 4200, 100 },
	{ 4100, 90 },
	{ 4000, 80 },
	{ 3900, 65 },
	{ 3800, 50 },
	{ 3700, 35 },
	{ 3600, 20 },
	{ 3500, 10 },
	{ 3300, 0 },
};

static uint8_t estimate_lipo_percent(int32_t mv)
{
	if (mv >= lipo_soc_table[0].mv) {
		return 100U;
	}

	for (size_t i = 1U; i < ARRAY_SIZE(lipo_soc_table); i++) {
		const struct soc_point *const high = &lipo_soc_table[i - 1U];
		const struct soc_point *const low = &lipo_soc_table[i];

		if (mv >= low->mv) {
			const int32_t mv_span = high->mv - low->mv;
			const int32_t pct_span = high->percent - low->percent;
			const int32_t pct_offset = ((mv - low->mv) * pct_span) / mv_span;

			return (uint8_t)(low->percent + pct_offset);
		}
	}

	return 0U;
}

static void update_latest_status(const struct power_status *status)
{
	k_spinlock_key_t key = k_spin_lock(&status_lock);

	latest_status = *status;
	k_spin_unlock(&status_lock, key);
}

static int read_channel(enum sensor_channel chan, struct sensor_value *value)
{
	const int err = sensor_channel_get(pmic_charger, chan, value);

	if (err != 0) {
		LOG_DBG("PMIC charger channel %u read failed (%d)", (uint32_t)chan, err);
	}

	return err;
}

static void update_ble_battery(const struct power_status *status)
{
	if (!IS_ENABLED(CONFIG_BT_BAS) || !status->battery_percent_valid) {
		return;
	}

	const int err = bt_bas_set_battery_level(status->battery_percent);

	if (err != 0) {
		LOG_DBG("BLE BAS update failed (%d)", err);
	}
}

static int power_sample_once(void)
{
	struct power_status status = {
		.charger_ready = true,
	};
	struct sensor_value value;
	int64_t converted;
	int err;

	err = sensor_sample_fetch(pmic_charger);
	if (err != 0) {
		sample_error_count++;
		if ((sample_error_count % POWER_ERROR_LOG_INTERVAL) == 1U) {
			LOG_WRN("PMIC charger sample failed (%d)", err);
		}
		return err;
	}

	sample_error_count = 0U;
	status.sample_valid = true;

	err = read_channel(SENSOR_CHAN_GAUGE_VOLTAGE, &value);
	if (err == 0) {
		converted = sensor_value_to_milli(&value);
		status.battery_mv = (int32_t)CLAMP(converted, 0, INT32_MAX);
		status.battery_percent = estimate_lipo_percent(status.battery_mv);
		status.battery_percent_valid = true;
	}

	err = read_channel(SENSOR_CHAN_GAUGE_AVG_CURRENT, &value);
	if (err == 0) {
		converted = sensor_value_to_milli(&value);
		status.battery_current_ma = (int32_t)CLAMP(converted, INT32_MIN, INT32_MAX);
	}

	err = read_channel(SENSOR_CHAN_GAUGE_TEMP, &value);
	if (err == 0) {
		converted = sensor_value_to_milli(&value);
		status.battery_temp_c = (int32_t)CLAMP(converted / 1000, INT32_MIN, INT32_MAX);
	}

	err = read_channel(SENSOR_CHAN_NPM13XX_CHARGER_STATUS, &value);
	if (err == 0) {
		status.charger_status = (uint8_t)value.val1;
	}

	err = read_channel(SENSOR_CHAN_NPM13XX_CHARGER_ERROR, &value);
	if (err == 0) {
		status.charger_error = (uint8_t)value.val1;
	}

	err = read_channel(SENSOR_CHAN_NPM13XX_CHARGER_VBUS_STATUS, &value);
	if (err == 0) {
		status.vbus_status = (uint8_t)value.val1;
	}

	err = sensor_attr_get(pmic_charger, SENSOR_CHAN_NPM13XX_CHARGER_VBUS_STATUS,
			      SENSOR_ATTR_NPM13XX_CHARGER_VBUS_PRESENT, &value);
	if (err == 0) {
		status.vbus_present = value.val1 != 0;
	} else {
		status.vbus_present = (status.vbus_status & BIT(0)) != 0U;
	}

	status.charging = status.vbus_present && (status.battery_current_ma > 0);
	status.fault = status.charger_error != 0U;

	update_latest_status(&status);
	update_ble_battery(&status);
	display_ui_request_update();

	LOG_INF("Battery %u%%, %d mV, %d mA, VBUS %s",
		status.battery_percent_valid ? status.battery_percent : 0U,
		status.battery_mv, status.battery_current_ma,
		status.vbus_present ? "present" : "absent");

	return 0;
}

static void power_sample_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (power_ready) {
		(void)power_sample_once();
		(void)k_work_reschedule(k_work_delayable_from_work(work),
					K_MSEC(POWER_SAMPLE_PERIOD_MS));
	}
}

K_WORK_DELAYABLE_DEFINE(power_sample_work, power_sample_work_handler);

int power_init(void)
{
	struct power_status initial_status = { 0 };
	int err;

	if (!device_is_ready(pmic_charger)) {
		initial_status.charger_ready = false;
		update_latest_status(&initial_status);
		LOG_ERR("nPM1300 charger telemetry device is not ready");
		return -ENODEV;
	}

	initial_status.charger_ready = true;
	update_latest_status(&initial_status);

	err = power_sample_once();
	if (err != 0) {
		LOG_WRN("Initial PMIC charger telemetry sample failed (%d)", err);
	}

	power_ready = true;
	(void)k_work_schedule(&power_sample_work, K_MSEC(POWER_SAMPLE_PERIOD_MS));
	LOG_INF("Power telemetry ready");
	return 0;
}

void power_get_status(struct power_status *status)
{
	k_spinlock_key_t key;

	if (status == NULL) {
		return;
	}

	key = k_spin_lock(&status_lock);
	*status = latest_status;
	k_spin_unlock(&status_lock, key);
}
