/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "backlight.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(backlight, LOG_LEVEL_INF);

#define DISPLAY_BACKLIGHT_NODE DT_ALIAS(display_backlight)

static const struct gpio_dt_spec display_backlight =
	GPIO_DT_SPEC_GET(DISPLAY_BACKLIGHT_NODE, gpios);
static bool backlight_ready;

int backlight_set(bool enabled)
{
	int err;

	if (!backlight_ready) {
		return -ENODEV;
	}

	err = gpio_pin_set_dt(&display_backlight, enabled ? 1 : 0);
	if (err != 0) {
		LOG_ERR("Backlight GPIO update failed (%d)", err);
	}

	return err;
}

int backlight_init(bool enabled)
{
	int err;

	if (!gpio_is_ready_dt(&display_backlight)) {
		LOG_ERR("Backlight GPIO is not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&display_backlight,
				    enabled ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_ERR("Backlight GPIO configure failed (%d)", err);
		return err;
	}

	backlight_ready = true;
	LOG_INF("Backlight GPIO ready");
	return 0;
}
