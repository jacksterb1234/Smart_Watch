/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SMARTWATCH_POWER_H_
#define SMARTWATCH_POWER_H_

#include <stdbool.h>
#include <stdint.h>

struct power_status {
	bool charger_ready;
	bool sample_valid;
	bool vbus_present;
	bool charging;
	bool fault;
	bool battery_percent_valid;
	uint8_t battery_percent;
	int32_t battery_mv;
	int32_t battery_current_ma;
	int32_t battery_temp_c;
	uint8_t charger_status;
	uint8_t charger_error;
	uint8_t vbus_status;
};

int power_init(void);
void power_get_status(struct power_status *status);

#endif /* SMARTWATCH_POWER_H_ */
