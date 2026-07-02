/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "power_manager.h"

#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_settings.h"
#include "display_ui.h"

LOG_MODULE_REGISTER(power_manager, LOG_LEVEL_INF);

static bool power_manager_ready;

static k_timeout_t display_timeout(void)
{
	struct app_settings_snapshot settings;

	app_settings_get(&settings);
	return K_SECONDS(settings.display_timeout_s);
}

static void display_sleep_work_handler(struct k_work *work)
{
	int err;

	ARG_UNUSED(work);

	if (!power_manager_ready || !display_ui_is_awake()) {
		return;
	}

	err = display_ui_set_awake(false);
	if (err != 0) {
		LOG_WRN("Display sleep failed (%d)", err);
		return;
	}

	LOG_INF("Display entered off state");
}

K_WORK_DELAYABLE_DEFINE(display_sleep_work, display_sleep_work_handler);

int power_manager_init(void)
{
	power_manager_ready = true;
	power_manager_notify_user_activity();
	LOG_INF("Power manager ready");
	return 0;
}

void power_manager_notify_user_activity(void)
{
	int err;

	if (!power_manager_ready) {
		return;
	}

	if (!display_ui_is_awake()) {
		err = display_ui_set_awake(true);
		if (err != 0) {
			LOG_WRN("Display wake failed (%d)", err);
		}
	}

	(void)k_work_reschedule(&display_sleep_work, display_timeout());
}
