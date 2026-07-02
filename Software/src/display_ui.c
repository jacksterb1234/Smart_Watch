/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "display_ui.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "activity.h"
#include "backlight.h"
#include "ble_media.h"

LOG_MODULE_REGISTER(display_ui, LOG_LEVEL_INF);

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)

#define UI_WIDTH 240
#define UI_HEIGHT 240
#define UI_CENTER_X 119
#define UI_CENTER_Y 119
#define UI_RADIUS 118
#define UI_RING_RADIUS 116
#define UI_FONT_WIDTH 5
#define UI_FONT_HEIGHT 7

#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

static const struct device *const display = DEVICE_DT_GET(DISPLAY_NODE);
static uint16_t framebuffer[UI_WIDTH * UI_HEIGHT];
static bool display_ui_ready;

static void glyph_rows(char c, uint8_t rows[UI_FONT_HEIGHT])
{
	static const uint8_t blank[UI_FONT_HEIGHT] = { 0, 0, 0, 0, 0, 0, 0 };
	static const uint8_t unknown[UI_FONT_HEIGHT] = { 0x1f, 0x11, 0x15, 0x15, 0x11, 0x1f, 0 };
	const uint8_t *glyph = unknown;

	switch (c) {
	case ' ':
		glyph = blank;
		break;
	case '-': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0, 0, 0, 0x1f, 0, 0, 0 };
		glyph = g;
		break;
	}
	case '%': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13 };
		glyph = g;
		break;
	}
	case '0': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e };
		glyph = g;
		break;
	}
	case '1': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e };
		glyph = g;
		break;
	}
	case '2': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f };
		glyph = g;
		break;
	}
	case '3': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e };
		glyph = g;
		break;
	}
	case '4': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 };
		glyph = g;
		break;
	}
	case '5': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e };
		glyph = g;
		break;
	}
	case '6': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e };
		glyph = g;
		break;
	}
	case '7': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 };
		glyph = g;
		break;
	}
	case '8': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e };
		glyph = g;
		break;
	}
	case '9': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c };
		glyph = g;
		break;
	}
	case 'A': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 };
		glyph = g;
		break;
	}
	case 'B': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e };
		glyph = g;
		break;
	}
	case 'C': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e };
		glyph = g;
		break;
	}
	case 'D': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e };
		glyph = g;
		break;
	}
	case 'E': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f };
		glyph = g;
		break;
	}
	case 'F': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 };
		glyph = g;
		break;
	}
	case 'G': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0e };
		glyph = g;
		break;
	}
	case 'H': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 };
		glyph = g;
		break;
	}
	case 'I': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e };
		glyph = g;
		break;
	}
	case 'K': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 };
		glyph = g;
		break;
	}
	case 'L': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f };
		glyph = g;
		break;
	}
	case 'M': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 };
		glyph = g;
		break;
	}
	case 'N': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 };
		glyph = g;
		break;
	}
	case 'O': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e };
		glyph = g;
		break;
	}
	case 'P': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 };
		glyph = g;
		break;
	}
	case 'R': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 };
		glyph = g;
		break;
	}
	case 'S': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e };
		glyph = g;
		break;
	}
	case 'T': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 };
		glyph = g;
		break;
	}
	case 'V': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x11, 0x11, 0x11, 0x11, 0x0a, 0x0a, 0x04 };
		glyph = g;
		break;
	}
	case 'W': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a };
		glyph = g;
		break;
	}
	case 'Y': {
		static const uint8_t g[UI_FONT_HEIGHT] = { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 };
		glyph = g;
		break;
	}
	default:
		break;
	}

	for (size_t i = 0; i < UI_FONT_HEIGHT; i++) {
		rows[i] = glyph[i];
	}
}

static void draw_pixel(int16_t x, int16_t y, uint16_t color)
{
	if ((x < 0) || (x >= UI_WIDTH) || (y < 0) || (y >= UI_HEIGHT)) {
		return;
	}

	framebuffer[(y * UI_WIDTH) + x] = color;
}

static void fill_rect(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color)
{
	for (int16_t row = 0; row < height; row++) {
		for (int16_t col = 0; col < width; col++) {
			draw_pixel(x + col, y + row, color);
		}
	}
}

static void draw_char(int16_t x, int16_t y, char c, uint8_t scale, uint16_t color)
{
	uint8_t rows[UI_FONT_HEIGHT];

	glyph_rows(c, rows);

	for (uint8_t row = 0; row < UI_FONT_HEIGHT; row++) {
		for (uint8_t col = 0; col < UI_FONT_WIDTH; col++) {
			if ((rows[row] & BIT(UI_FONT_WIDTH - 1 - col)) == 0U) {
				continue;
			}

			fill_rect(x + (col * scale), y + (row * scale), scale, scale, color);
		}
	}
}

static void draw_text(int16_t x, int16_t y, const char *text, uint8_t scale, uint16_t color)
{
	while (*text != '\0') {
		draw_char(x, y, *text, scale, color);
		x += (UI_FONT_WIDTH + 1) * scale;
		text++;
	}
}

static size_t append_u32(char *dst, size_t dst_len, size_t pos, uint32_t value)
{
	char reverse[10];
	size_t count = 0U;

	do {
		reverse[count++] = (char)('0' + (value % 10U));
		value /= 10U;
	} while ((value != 0U) && (count < sizeof(reverse)));

	while ((count > 0U) && (pos + 1U < dst_len)) {
		dst[pos++] = reverse[--count];
	}

	if (dst_len > 0U) {
		dst[pos] = '\0';
	}

	return pos;
}

static const char *ble_status_text(void)
{
	switch (ble_media_get_status()) {
	case BLE_MEDIA_STATUS_ADVERTISING:
		return "ADVERTISING";
	case BLE_MEDIA_STATUS_CONNECTED:
		return "CONNECTED";
	case BLE_MEDIA_STATUS_IDLE:
		return "READY";
	case BLE_MEDIA_STATUS_DISABLED:
	default:
		return "DISABLED";
	}
}

static void clear_watch_face(void)
{
	const uint16_t black = RGB565(0, 0, 0);
	const uint16_t bg = RGB565(8, 12, 14);
	const uint16_t ring = RGB565(48, 180, 168);

	for (int16_t y = 0; y < UI_HEIGHT; y++) {
		for (int16_t x = 0; x < UI_WIDTH; x++) {
			const int16_t dx = x - UI_CENTER_X;
			const int16_t dy = y - UI_CENTER_Y;
			const int32_t dist_sq = ((int32_t)dx * dx) + ((int32_t)dy * dy);

			if (dist_sq > (UI_RADIUS * UI_RADIUS)) {
				draw_pixel(x, y, black);
			} else if (dist_sq > (UI_RING_RADIUS * UI_RING_RADIUS)) {
				draw_pixel(x, y, ring);
			} else {
				draw_pixel(x, y, bg);
			}
		}
	}
}

static int render_ui(void)
{
	const uint16_t text = RGB565(238, 244, 241);
	const uint16_t muted = RGB565(128, 150, 148);
	const uint16_t panel = RGB565(18, 28, 31);
	const uint16_t accent = RGB565(42, 210, 180);
	char steps_text[11];
	struct display_buffer_descriptor desc = {
		.buf_size = sizeof(framebuffer),
		.width = UI_WIDTH,
		.height = UI_HEIGHT,
		.pitch = UI_WIDTH,
		.frame_incomplete = false,
	};
	int err;

	clear_watch_face();

	draw_text(48, 28, "SMART WATCH", 2, muted);
	draw_text(32, 58, "BLE", 2, muted);
	draw_text(78, 58, ble_status_text(), 2, text);

	fill_rect(32, 88, 176, 56, panel);
	draw_text(52, 98, "STEPS", 2, muted);
	(void)append_u32(steps_text, sizeof(steps_text), 0U, activity_get_steps());
	draw_text(84, 118, steps_text, 3, accent);

	draw_text(42, 164, "HR -- BPM", 2, text);
	draw_text(42, 190, "BAT --%", 2, text);

	err = display_write(display, 0, 0, &desc, framebuffer);
	if (err != 0) {
		LOG_ERR("Display write failed (%d)", err);
		return err;
	}

	err = display_blanking_off(display);
	if ((err != 0) && (err != -ENOSYS)) {
		LOG_ERR("Display blanking off failed (%d)", err);
		return err;
	}

	return 0;
}

static void display_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!display_ui_ready) {
		return;
	}

	(void)render_ui();
}

K_WORK_DEFINE(display_work, display_work_handler);

int display_ui_init(void)
{
	struct display_capabilities caps;
	int err;

	if (!device_is_ready(display)) {
		LOG_ERR("Display device is not ready");
		return -ENODEV;
	}

	display_get_capabilities(display, &caps);
	if ((caps.x_resolution < UI_WIDTH) || (caps.y_resolution < UI_HEIGHT)) {
		LOG_ERR("Display resolution %ux%u is too small", caps.x_resolution,
			caps.y_resolution);
		return -EINVAL;
	}

	if ((caps.current_pixel_format != PIXEL_FORMAT_RGB_565) &&
	    ((caps.supported_pixel_formats & PIXEL_FORMAT_RGB_565) != 0U)) {
		err = display_set_pixel_format(display, PIXEL_FORMAT_RGB_565);
		if (err != 0) {
			LOG_ERR("Display RGB565 format set failed (%d)", err);
			return err;
		}
	}

	display_ui_ready = true;
	err = render_ui();
	if (err != 0) {
		display_ui_ready = false;
		return err;
	}

	err = backlight_set(true);
	if (err != 0) {
		LOG_WRN("Backlight enable failed (%d)", err);
	}

	LOG_INF("Display UI ready");
	return 0;
}

void display_ui_request_update(void)
{
	if (!display_ui_ready) {
		return;
	}

	(void)k_work_submit(&display_work);
}
