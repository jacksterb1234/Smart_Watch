/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SMARTWATCH_BLE_MEDIA_H_
#define SMARTWATCH_BLE_MEDIA_H_

enum ble_media_command {
	BLE_MEDIA_PLAY_PAUSE,
	BLE_MEDIA_NEXT_TRACK,
	BLE_MEDIA_PREVIOUS_TRACK,
	BLE_MEDIA_VOLUME_UP,
	BLE_MEDIA_VOLUME_DOWN,
};

enum ble_media_status {
	BLE_MEDIA_STATUS_DISABLED,
	BLE_MEDIA_STATUS_IDLE,
	BLE_MEDIA_STATUS_ADVERTISING,
	BLE_MEDIA_STATUS_CONNECTED,
};

int ble_media_init(void);
int ble_media_send(enum ble_media_command command);
enum ble_media_status ble_media_get_status(void);

#endif /* SMARTWATCH_BLE_MEDIA_H_ */
