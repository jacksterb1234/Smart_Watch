/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SMARTWATCH_DISPLAY_UI_H_
#define SMARTWATCH_DISPLAY_UI_H_

#include <stdbool.h>

int display_ui_init(void);
int display_ui_set_awake(bool awake);
bool display_ui_is_awake(void);
void display_ui_request_update(void);

#endif /* SMARTWATCH_DISPLAY_UI_H_ */
