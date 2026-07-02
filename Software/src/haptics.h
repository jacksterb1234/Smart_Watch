/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SMARTWATCH_HAPTICS_H_
#define SMARTWATCH_HAPTICS_H_

#include <stdint.h>

int haptics_init(void);
int haptics_pulse(uint16_t duration_ms, uint8_t intensity_percent);
int haptics_stop(void);

#endif /* SMARTWATCH_HAPTICS_H_ */
