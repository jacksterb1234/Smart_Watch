#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include "activity.h"
#include "backlight.h"
#include "ble_media.h"
#include "buttons.h"
#include "display_ui.h"
#include "heart_rate.h"
#include "haptics.h"
#include "power.h"

LOG_MODULE_REGISTER(smart_watch, LOG_LEVEL_INF);

#define LOG_DEVICE_READY(node_id, name)                                            \
	do {                                                                       \
		const struct device *const dev = DEVICE_DT_GET(node_id);           \
		LOG_INF("%s: %s", name, device_is_ready(dev) ? "ready" : "not ready"); \
	} while (false)

int main(void)
{
	int err;

	LOG_INF("Custom smartwatch firmware starting");

	LOG_DEVICE_READY(DT_NODELABEL(pmic), "nPM1300");
	LOG_DEVICE_READY(DT_NODELABEL(pmic_charger), "nPM1300 charger");
	LOG_DEVICE_READY(DT_NODELABEL(bmi270), "BMI270");
	LOG_DEVICE_READY(DT_NODELABEL(max30102), "MAX30102");
	LOG_DEVICE_READY(DT_NODELABEL(lps22df), "LPS22DF");
	LOG_DEVICE_READY(DT_NODELABEL(display), "GC9A01");
	LOG_DEVICE_READY(DT_NODELABEL(w25q32), "W25Q32");

	err = backlight_init(false);
	if (err != 0) {
		LOG_ERR("Backlight init failed (%d)", err);
	}

	err = haptics_init();
	if (err != 0) {
		LOG_ERR("Haptics init failed (%d)", err);
	}

	err = ble_media_init();
	if (err != 0) {
		LOG_ERR("BLE media init failed (%d)", err);
	}

	err = buttons_init();
	if (err != 0) {
		LOG_ERR("Button service init failed (%d)", err);
	}

	err = activity_init();
	if (err != 0) {
		LOG_ERR("Activity tracking init failed (%d)", err);
	}

	err = heart_rate_init();
	if (err != 0) {
		LOG_ERR("Heart-rate service init failed (%d)", err);
	}

	err = power_init();
	if (err != 0) {
		LOG_ERR("Power telemetry init failed (%d)", err);
	}

	err = display_ui_init();
	if (err != 0) {
		LOG_ERR("Display UI init failed (%d)", err);
	}

	return 0;
}
