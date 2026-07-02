/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "activity.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "display_ui.h"

LOG_MODULE_REGISTER(activity, LOG_LEVEL_INF);

#define BMI270_NODE DT_NODELABEL(bmi270)

#define ACTIVITY_SETTINGS_KEY "activity/steps"
#define ACTIVITY_SETTINGS_SUBTREE "activity"
#define ACTIVITY_SETTINGS_STEPS_NAME "steps"

#define ACTIVITY_SAMPLE_PERIOD_MS 100
#define ACTIVITY_ACCEL_ODR_HZ 25
#define ACTIVITY_ACCEL_FULL_SCALE_G 4
#define STEP_HIGH_DELTA_MG 180
#define STEP_LOW_DELTA_MG 60
#define STEP_REFRACTORY_MS 300
#define STEP_SAVE_DELTA 16
#define STEP_SAVE_DELAY_MS 30000

static const struct device *const bmi270 = DEVICE_DT_GET(BMI270_NODE);

static uint32_t step_count;
static uint32_t saved_step_count;
static uint32_t last_step_ms;
static uint32_t sensor_error_count;
static int32_t baseline_mg = -1;
static bool above_step_threshold;
static bool activity_ready;

static uint32_t isqrt64(uint64_t value)
{
	uint64_t bit = UINT64_C(1) << 62;
	uint64_t result = 0U;

	while (bit > value) {
		bit >>= 2;
	}

	while (bit != 0U) {
		if (value >= result + bit) {
			value -= result + bit;
			result = (result >> 1) + bit;
		} else {
			result >>= 1;
		}

		bit >>= 2;
	}

	return (uint32_t)result;
}

static int32_t accel_magnitude_mg(const struct sensor_value accel[3])
{
	const int32_t x_mg = sensor_ms2_to_mg(&accel[0]);
	const int32_t y_mg = sensor_ms2_to_mg(&accel[1]);
	const int32_t z_mg = sensor_ms2_to_mg(&accel[2]);
	const int64_t x_sq = (int64_t)x_mg * x_mg;
	const int64_t y_sq = (int64_t)y_mg * y_mg;
	const int64_t z_sq = (int64_t)z_mg * z_mg;

	return (int32_t)isqrt64((uint64_t)(x_sq + y_sq + z_sq));
}

static void activity_schedule_save(bool soon);

static void activity_save_work_handler(struct k_work *work)
{
	int err;

	ARG_UNUSED(work);

	if (!IS_ENABLED(CONFIG_SETTINGS)) {
		return;
	}

	if (saved_step_count == step_count) {
		return;
	}

	err = settings_save_one(ACTIVITY_SETTINGS_KEY, &step_count, sizeof(step_count));
	if (err != 0) {
		LOG_WRN("Step count save failed (%d)", err);
		return;
	}

	saved_step_count = step_count;
	LOG_INF("Step count saved: %u", step_count);
}

K_WORK_DELAYABLE_DEFINE(activity_save_work, activity_save_work_handler);

static void activity_process_sample(const struct sensor_value accel[3])
{
	const int32_t mag_mg = accel_magnitude_mg(accel);
	const uint32_t now_ms = k_uptime_get_32();
	int32_t delta_mg;

	if (baseline_mg < 0) {
		baseline_mg = mag_mg;
		return;
	}

	delta_mg = mag_mg - baseline_mg;
	baseline_mg = ((baseline_mg * 31) + mag_mg) / 32;

	if (!above_step_threshold) {
		const uint32_t elapsed_ms = now_ms - last_step_ms;

		if ((delta_mg > STEP_HIGH_DELTA_MG) &&
		    (elapsed_ms >= STEP_REFRACTORY_MS)) {
			step_count++;
			last_step_ms = now_ms;
			above_step_threshold = true;
			LOG_INF("Step count: %u", step_count);
			display_ui_request_update();
			activity_schedule_save((step_count - saved_step_count) >= STEP_SAVE_DELTA);
		}
	} else if (delta_mg < STEP_LOW_DELTA_MG) {
		above_step_threshold = false;
	}
}

static void activity_sample_work_handler(struct k_work *work)
{
	struct sensor_value accel[3];
	int err;

	ARG_UNUSED(work);

	if (!activity_ready) {
		return;
	}

	err = sensor_sample_fetch_chan(bmi270, SENSOR_CHAN_ACCEL_XYZ);
	if (err != 0) {
		sensor_error_count++;
		if ((sensor_error_count % 32U) == 1U) {
			LOG_WRN("BMI270 accel fetch failed (%d)", err);
		}
		goto reschedule;
	}

	err = sensor_channel_get(bmi270, SENSOR_CHAN_ACCEL_XYZ, accel);
	if (err != 0) {
		sensor_error_count++;
		if ((sensor_error_count % 32U) == 1U) {
			LOG_WRN("BMI270 accel read failed (%d)", err);
		}
		goto reschedule;
	}

	sensor_error_count = 0U;
	activity_process_sample(accel);

reschedule:
	(void)k_work_reschedule(k_work_delayable_from_work(work),
				K_MSEC(ACTIVITY_SAMPLE_PERIOD_MS));
}

K_WORK_DELAYABLE_DEFINE(activity_sample_work, activity_sample_work_handler);

static void activity_schedule_save(bool soon)
{
	const k_timeout_t delay = soon ? K_SECONDS(1) : K_MSEC(STEP_SAVE_DELAY_MS);

	if (!IS_ENABLED(CONFIG_SETTINGS)) {
		return;
	}

	(void)k_work_reschedule(&activity_save_work, delay);
}

static int activity_settings_set(const char *name, size_t len,
				 settings_read_cb read_cb, void *cb_arg)
{
	const char *next;
	uint32_t loaded_steps;
	ssize_t read_len;

	if (!settings_name_steq(name, ACTIVITY_SETTINGS_STEPS_NAME, &next) || next != NULL) {
		return -ENOENT;
	}

	if (len != sizeof(loaded_steps)) {
		return -EINVAL;
	}

	read_len = read_cb(cb_arg, &loaded_steps, sizeof(loaded_steps));
	if (read_len < 0) {
		return (int)read_len;
	}

	if (read_len != sizeof(loaded_steps)) {
		return -EINVAL;
	}

	step_count = loaded_steps;
	saved_step_count = loaded_steps;
	LOG_INF("Step count restored: %u", step_count);
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(activity, ACTIVITY_SETTINGS_SUBTREE, NULL,
			       activity_settings_set, NULL, NULL);

int activity_init(void)
{
	struct sensor_value odr = {
		.val1 = ACTIVITY_ACCEL_ODR_HZ,
		.val2 = 0,
	};
	struct sensor_value full_scale;
	int err;

	if (!device_is_ready(bmi270)) {
		LOG_ERR("BMI270 is not ready for activity tracking");
		return -ENODEV;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		err = settings_load_subtree(ACTIVITY_SETTINGS_SUBTREE);
		if (err != 0) {
			LOG_WRN("Activity settings load failed (%d)", err);
		}
	}

	err = sensor_attr_set(bmi270, SENSOR_CHAN_ACCEL_XYZ,
			      SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
	if (err != 0) {
		LOG_WRN("BMI270 accel ODR set failed (%d)", err);
	}

	sensor_g_to_ms2(ACTIVITY_ACCEL_FULL_SCALE_G, &full_scale);
	err = sensor_attr_set(bmi270, SENSOR_CHAN_ACCEL_XYZ,
			      SENSOR_ATTR_FULL_SCALE, &full_scale);
	if (err != 0) {
		LOG_WRN("BMI270 accel full-scale set failed (%d)", err);
	}

	activity_ready = true;
	(void)k_work_schedule(&activity_sample_work, K_NO_WAIT);
	LOG_INF("Activity tracking ready: %u steps", step_count);
	return 0;
}

uint32_t activity_get_steps(void)
{
	return step_count;
}

int activity_reset_steps(void)
{
	step_count = 0U;
	saved_step_count = 0U;
	baseline_mg = -1;
	above_step_threshold = false;
	activity_schedule_save(true);
	display_ui_request_update();
	LOG_INF("Step count reset");
	return 0;
}
