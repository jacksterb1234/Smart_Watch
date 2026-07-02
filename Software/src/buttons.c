/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "buttons.h"

#include <stdint.h>

#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_settings.h"
#include "ble_media.h"
#include "heart_rate.h"
#include "haptics.h"
#include "power_manager.h"

LOG_MODULE_REGISTER(buttons, LOG_LEVEL_INF);

#define BUTTON_HAPTIC_DURATION_MS 30

enum button_action {
	BUTTON_ACTION_PLAY_PAUSE,
	BUTTON_ACTION_NEXT_TRACK,
	BUTTON_ACTION_PREVIOUS_TRACK,
	BUTTON_ACTION_HEART_RATE,
};

K_MSGQ_DEFINE(button_msgq, sizeof(enum button_action), 4, 1);

static bool action_from_input_code(uint16_t code, enum button_action *action)
{
	switch (code) {
	case INPUT_KEY_0:
		*action = BUTTON_ACTION_PLAY_PAUSE;
		return true;
	case INPUT_KEY_1:
		*action = BUTTON_ACTION_NEXT_TRACK;
		return true;
	case INPUT_KEY_2:
		*action = BUTTON_ACTION_PREVIOUS_TRACK;
		return true;
	case INPUT_KEY_3:
		*action = BUTTON_ACTION_HEART_RATE;
		return true;
	default:
		return false;
	}
}

static void button_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	enum button_action action;

	while (k_msgq_get(&button_msgq, &action, K_NO_WAIT) == 0) {
		power_manager_notify_user_activity();

		if (app_settings_haptics_enabled()) {
			(void)haptics_pulse(BUTTON_HAPTIC_DURATION_MS,
					    app_settings_haptic_intensity_percent());
		}

		switch (action) {
		case BUTTON_ACTION_PLAY_PAUSE:
			(void)ble_media_send(BLE_MEDIA_PLAY_PAUSE);
			break;
		case BUTTON_ACTION_NEXT_TRACK:
			(void)ble_media_send(BLE_MEDIA_NEXT_TRACK);
			break;
		case BUTTON_ACTION_PREVIOUS_TRACK:
			(void)ble_media_send(BLE_MEDIA_PREVIOUS_TRACK);
			break;
		case BUTTON_ACTION_HEART_RATE:
			if (heart_rate_start_measurement() != 0) {
				LOG_WRN("Heart-rate measurement could not start");
			}
			break;
		default:
			break;
		}
	}
}

K_WORK_DEFINE(button_work, button_work_handler);

static void button_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	enum button_action action;

	if ((evt->sync == 0U) || (evt->type != INPUT_EV_KEY) || (evt->value == 0)) {
		return;
	}

	if (!action_from_input_code(evt->code, &action)) {
		return;
	}

	if (k_msgq_put(&button_msgq, &action, K_NO_WAIT) == 0) {
		(void)k_work_submit(&button_work);
	}
}

INPUT_CALLBACK_DEFINE(NULL, button_input_cb, NULL);

int buttons_init(void)
{
	LOG_INF("Button service ready");
	return 0;
}
