/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SMARTWATCH_BACKLIGHT_H_
#define SMARTWATCH_BACKLIGHT_H_

#include <stdbool.h>

int backlight_init(bool enabled);
int backlight_set(bool enabled);

#endif /* SMARTWATCH_BACKLIGHT_H_ */
