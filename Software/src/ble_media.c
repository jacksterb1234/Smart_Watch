/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_media.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <bluetooth/services/hids.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>

#include "display_ui.h"

LOG_MODULE_REGISTER(ble_media, LOG_LEVEL_INF);

#define BASE_USB_HID_SPEC_VERSION 0x0101
#define REPORT_ID_CONSUMER_CTRL 1
#define REPORT_IDX_CONSUMER_CTRL 0
#define REPORT_SIZE_CONSUMER_CTRL 2

#define HID_USAGE_PLAY_PAUSE 0x00CD
#define HID_USAGE_NEXT_TRACK 0x00B5
#define HID_USAGE_PREVIOUS_TRACK 0x00B6
#define HID_USAGE_VOLUME_UP 0x00E9
#define HID_USAGE_VOLUME_DOWN 0x00EA
#define HID_USAGE_RELEASE 0x0000

BT_HIDS_DEF(hids_obj, REPORT_SIZE_CONSUMER_CTRL);

static bool ble_ready;
static enum ble_media_status ble_status = BLE_MEDIA_STATUS_DISABLED;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static int start_advertising(void)
{
	const int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
					ad, ARRAY_SIZE(ad),
					sd, ARRAY_SIZE(sd));

	if ((err != 0) && (err != -EALREADY)) {
		LOG_ERR("BLE advertising failed to start (%d)", err);
		return err;
	}

	ble_status = BLE_MEDIA_STATUS_ADVERTISING;
	display_ui_request_update();
	LOG_INF("BLE HID advertising as \"%s\"", CONFIG_BT_DEVICE_NAME);
	return 0;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err != 0U) {
		LOG_WRN("BLE connection failed (err %u)", err);
		(void)start_advertising();
		return;
	}

	LOG_INF("BLE connected");
	ble_status = BLE_MEDIA_STATUS_CONNECTED;
	display_ui_request_update();

	err = bt_hids_connected(&hids_obj, conn);
	if (err != 0U) {
		LOG_ERR("HIDS connection notification failed (%u)", err);
	}

	const int security_err = bt_conn_set_security(conn, BT_SECURITY_L2);

	if ((security_err != 0) && (security_err != -EALREADY)) {
		LOG_WRN("BLE security request failed (%d)", security_err);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	const int err = bt_hids_disconnected(&hids_obj, conn);

	if (err != 0) {
		LOG_ERR("HIDS disconnection notification failed (%d)", err);
	}

	LOG_INF("BLE disconnected (reason 0x%02x)", reason);
	(void)start_advertising();
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	if (err == BT_SECURITY_ERR_SUCCESS) {
		LOG_INF("BLE security level %u", level);
	} else {
		LOG_WRN("BLE security failed at level %u (err %u)", level, err);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

static int hids_init(void)
{
	static const uint8_t report_map[] = {
		0x05, 0x0C,                         /* Usage Page (Consumer) */
		0x09, 0x01,                         /* Usage (Consumer Control) */
		0xA1, 0x01,                         /* Collection (Application) */
		0x85, REPORT_ID_CONSUMER_CTRL,      /* Report ID */
		0x15, 0x00,                         /* Logical Minimum (0) */
		0x26, 0xFF, 0x03,                   /* Logical Maximum (0x03ff) */
		0x19, 0x00,                         /* Usage Minimum (0) */
		0x2A, 0xFF, 0x03,                   /* Usage Maximum (0x03ff) */
		0x75, 0x10,                         /* Report Size (16) */
		0x95, 0x01,                         /* Report Count (1) */
		0x81, 0x00,                         /* Input (Data, Array, Absolute) */
		0xC0,                               /* End Collection */
	};

	struct bt_hids_init_param hids_init_param = { 0 };
	struct bt_hids_inp_rep *rep;

	hids_init_param.rep_map.data = report_map;
	hids_init_param.rep_map.size = sizeof(report_map);
	hids_init_param.info.bcd_hid = BASE_USB_HID_SPEC_VERSION;
	hids_init_param.info.b_country_code = 0x00;
	hids_init_param.info.flags = BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE;

	rep = &hids_init_param.inp_rep_group_init.reports[REPORT_IDX_CONSUMER_CTRL];
	rep->id = REPORT_ID_CONSUMER_CTRL;
	rep->size = REPORT_SIZE_CONSUMER_CTRL;
	hids_init_param.inp_rep_group_init.cnt = 1;

	return bt_hids_init(&hids_obj, &hids_init_param);
}

static uint16_t usage_for_command(enum ble_media_command command)
{
	switch (command) {
	case BLE_MEDIA_PLAY_PAUSE:
		return HID_USAGE_PLAY_PAUSE;
	case BLE_MEDIA_NEXT_TRACK:
		return HID_USAGE_NEXT_TRACK;
	case BLE_MEDIA_PREVIOUS_TRACK:
		return HID_USAGE_PREVIOUS_TRACK;
	case BLE_MEDIA_VOLUME_UP:
		return HID_USAGE_VOLUME_UP;
	case BLE_MEDIA_VOLUME_DOWN:
		return HID_USAGE_VOLUME_DOWN;
	default:
		return HID_USAGE_RELEASE;
	}
}

static int send_usage(uint16_t usage)
{
	uint8_t report[REPORT_SIZE_CONSUMER_CTRL];

	sys_put_le16(usage, report);
	return bt_hids_inp_rep_send(&hids_obj, NULL, REPORT_IDX_CONSUMER_CTRL,
				    report, sizeof(report), NULL);
}

int ble_media_send(enum ble_media_command command)
{
	int err;

	if (!ble_ready) {
		return -EAGAIN;
	}

	err = send_usage(usage_for_command(command));
	if (err != 0) {
		LOG_WRN("BLE media press failed (%d)", err);
		return err;
	}

	err = send_usage(HID_USAGE_RELEASE);
	if (err != 0) {
		LOG_WRN("BLE media release failed (%d)", err);
	}

	return err;
}

enum ble_media_status ble_media_get_status(void)
{
	return ble_status;
}

int ble_media_init(void)
{
	int err = hids_init();

	if (err != 0) {
		LOG_ERR("HIDS init failed (%d)", err);
		return err;
	}

	err = bt_enable(NULL);
	if (err != 0) {
		LOG_ERR("Bluetooth init failed (%d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");
	ble_status = BLE_MEDIA_STATUS_IDLE;

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		err = settings_load();
		if (err != 0) {
			LOG_WRN("Settings load failed (%d)", err);
		}
	}

	ble_ready = true;
	return start_advertising();
}
