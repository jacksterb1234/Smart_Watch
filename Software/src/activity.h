/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SMARTWATCH_ACTIVITY_H_
#define SMARTWATCH_ACTIVITY_H_

#include <stdint.h>

int activity_init(void);
uint32_t activity_get_steps(void);
int activity_reset_steps(void);

#endif /* SMARTWATCH_ACTIVITY_H_ */
