/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_settings.h"

#include <errno.h>
#include <stddef.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(app_settings, LOG_LEVEL_INF);

#define APP_SETTINGS_SUBTREE "watch"
#define APP_SETTINGS_RECORD_NAME "user"
#define APP_SETTINGS_KEY "watch/user"
#define APP_SETTINGS_VERSION 1U

#define HAPTIC_INTENSITY_DEFAULT 50U
#define HAPTIC_INTENSITY_MIN 10U
#define HAPTIC_INTENSITY_MAX 100U
#define DISPLAY_TIMEOUT_DEFAULT_S 30U
#define DISPLAY_TIMEOUT_MIN_S 5U
#define DISPLAY_TIMEOUT_MAX_S 300U

struct app_settings_record {
	uint8_t version;
	uint8_t haptics_enabled;
	uint8_t haptic_intensity_percent;
	uint8_t reserved;
	uint16_t display_timeout_s;
};

static struct app_settings_record current_settings = {
	.version = APP_SETTINGS_VERSION,
	.haptics_enabled = 1U,
	.haptic_intensity_percent = HAPTIC_INTENSITY_DEFAULT,
	.reserved = 0U,
	.display_timeout_s = DISPLAY_TIMEOUT_DEFAULT_S,
};
static struct k_spinlock settings_lock;

static void sanitize_settings(struct app_settings_record *settings)
{
	settings->version = APP_SETTINGS_VERSION;
	settings->haptics_enabled = settings->haptics_enabled != 0U ? 1U : 0U;
	settings->reserved = 0U;
	settings->haptic_intensity_percent =
		(uint8_t)CLAMP(settings->haptic_intensity_percent,
			       HAPTIC_INTENSITY_MIN, HAPTIC_INTENSITY_MAX);
	settings->display_timeout_s =
		(uint16_t)CLAMP(settings->display_timeout_s,
				DISPLAY_TIMEOUT_MIN_S, DISPLAY_TIMEOUT_MAX_S);
}

static void store_current_settings(const struct app_settings_record *settings)
{
	k_spinlock_key_t key = k_spin_lock(&settings_lock);

	current_settings = *settings;
	k_spin_unlock(&settings_lock, key);
}

static struct app_settings_record load_current_settings(void)
{
	struct app_settings_record settings;
	k_spinlock_key_t key = k_spin_lock(&settings_lock);

	settings = current_settings;
	k_spin_unlock(&settings_lock, key);

	return settings;
}

static int save_current_settings(void)
{
	struct app_settings_record settings = load_current_settings();

	if (!IS_ENABLED(CONFIG_SETTINGS)) {
		return -ENOTSUP;
	}

	return settings_save_one(APP_SETTINGS_KEY, &settings, sizeof(settings));
}

static int app_settings_set(const char *name, size_t len,
			    settings_read_cb read_cb, void *cb_arg)
{
	const char *next;
	struct app_settings_record loaded;
	ssize_t read_len;

	if (!settings_name_steq(name, APP_SETTINGS_RECORD_NAME, &next) || next != NULL) {
		return -ENOENT;
	}

	if (len != sizeof(loaded)) {
		return -EINVAL;
	}

	read_len = read_cb(cb_arg, &loaded, sizeof(loaded));
	if (read_len < 0) {
		return (int)read_len;
	}

	if (read_len != sizeof(loaded)) {
		return -EINVAL;
	}

	if (loaded.version != APP_SETTINGS_VERSION) {
		return -EINVAL;
	}

	sanitize_settings(&loaded);
	store_current_settings(&loaded);
	LOG_INF("User settings restored");
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(app_settings, APP_SETTINGS_SUBTREE, NULL,
			       app_settings_set, NULL, NULL);

int app_settings_init(void)
{
	int err;

	if (!IS_ENABLED(CONFIG_SETTINGS)) {
		LOG_WRN("Settings subsystem is disabled");
		return -ENOTSUP;
	}

	err = settings_load_subtree(APP_SETTINGS_SUBTREE);
	if (err != 0) {
		LOG_WRN("User settings load failed (%d)", err);
		return err;
	}

	LOG_INF("User settings ready");
	return 0;
}

void app_settings_get(struct app_settings_snapshot *settings)
{
	struct app_settings_record current;

	if (settings == NULL) {
		return;
	}

	current = load_current_settings();
	settings->haptics_enabled = current.haptics_enabled != 0U;
	settings->haptic_intensity_percent = current.haptic_intensity_percent;
	settings->display_timeout_s = current.display_timeout_s;
}

bool app_settings_haptics_enabled(void)
{
	return load_current_settings().haptics_enabled != 0U;
}

uint8_t app_settings_haptic_intensity_percent(void)
{
	return load_current_settings().haptic_intensity_percent;
}

int app_settings_set_haptics(bool enabled, uint8_t intensity_percent)
{
	struct app_settings_record settings = load_current_settings();
	int err;

	settings.haptics_enabled = enabled ? 1U : 0U;
	settings.haptic_intensity_percent = intensity_percent;
	sanitize_settings(&settings);
	store_current_settings(&settings);

	err = save_current_settings();
	if (err != 0) {
		LOG_WRN("User settings save failed (%d)", err);
	}

	return err;
}
