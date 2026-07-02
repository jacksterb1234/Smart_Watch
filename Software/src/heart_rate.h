/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SMARTWATCH_HEART_RATE_H_
#define SMARTWATCH_HEART_RATE_H_

#include <stdbool.h>
#include <stdint.h>

enum heart_rate_state {
	HEART_RATE_DISABLED,
	HEART_RATE_IDLE,
	HEART_RATE_SEARCHING,
	HEART_RATE_MEASURING,
	HEART_RATE_READY,
	HEART_RATE_NO_FINGER,
	HEART_RATE_POOR_SIGNAL,
	HEART_RATE_SENSOR_ERROR,
};

struct heart_rate_status {
	enum heart_rate_state state;
	bool bpm_valid;
	uint16_t bpm;
	uint8_t quality;
};

int heart_rate_init(void);
int heart_rate_start_measurement(void);
void heart_rate_get_status(struct heart_rate_status *status);

#endif /* SMARTWATCH_HEART_RATE_H_ */
