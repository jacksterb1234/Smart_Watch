/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SMARTWATCH_APP_SETTINGS_H_
#define SMARTWATCH_APP_SETTINGS_H_

#include <stdbool.h>
#include <stdint.h>

struct app_settings_snapshot {
	bool haptics_enabled;
	uint8_t haptic_intensity_percent;
	uint16_t display_timeout_s;
};

int app_settings_init(void);
void app_settings_get(struct app_settings_snapshot *settings);
bool app_settings_haptics_enabled(void);
uint8_t app_settings_haptic_intensity_percent(void);
int app_settings_set_haptics(bool enabled, uint8_t intensity_percent);

#endif /* SMARTWATCH_APP_SETTINGS_H_ */
