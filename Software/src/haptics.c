/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "haptics.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(haptics, LOG_LEVEL_INF);

#define HAPTIC_EN_NODE DT_NODELABEL(haptic_en)
#define HAPTIC_PWM_NODE DT_NODELABEL(haptic_pwm)

static const struct gpio_dt_spec haptic_en = GPIO_DT_SPEC_GET(HAPTIC_EN_NODE, gpios);
static const struct pwm_dt_spec haptic_pwm = PWM_DT_SPEC_GET(HAPTIC_PWM_NODE);
static bool haptics_ready;

static int haptics_apply(uint32_t pulse_ns, bool enable)
{
	int err = pwm_set_dt(&haptic_pwm, haptic_pwm.period, pulse_ns);

	if (err != 0) {
		LOG_ERR("Haptic PWM update failed (%d)", err);
		return err;
	}

	err = gpio_pin_set_dt(&haptic_en, enable ? 1 : 0);
	if (err != 0) {
		LOG_ERR("Haptic enable update failed (%d)", err);
	}

	return err;
}

static void haptic_stop_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	(void)haptics_apply(0U, false);
}

K_WORK_DELAYABLE_DEFINE(haptic_stop_work, haptic_stop_work_handler);

int haptics_init(void)
{
	int err;

	if (!gpio_is_ready_dt(&haptic_en)) {
		LOG_ERR("Haptic enable GPIO is not ready");
		return -ENODEV;
	}

	if (!pwm_is_ready_dt(&haptic_pwm)) {
		LOG_ERR("Haptic PWM device is not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&haptic_en, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_ERR("Haptic enable configure failed (%d)", err);
		return err;
	}

	err = haptics_apply(0U, false);
	if (err != 0) {
		return err;
	}

	haptics_ready = true;
	LOG_INF("Haptics ready: period %u ns", haptic_pwm.period);
	return 0;
}

int haptics_pulse(uint16_t duration_ms, uint8_t intensity_percent)
{
	uint32_t pulse_ns;
	int err;

	if (!haptics_ready) {
		return -ENODEV;
	}

	if ((duration_ms == 0U) || (intensity_percent == 0U)) {
		return haptics_stop();
	}

	if (intensity_percent > 100U) {
		intensity_percent = 100U;
	}

	pulse_ns = (uint32_t)(((uint64_t)haptic_pwm.period * intensity_percent) / 100U);

	err = haptics_apply(pulse_ns, true);
	if (err != 0) {
		return err;
	}

	err = k_work_reschedule(&haptic_stop_work, K_MSEC(duration_ms));
	if (err < 0) {
		(void)haptics_apply(0U, false);
		LOG_ERR("Haptic stop scheduling failed (%d)", err);
		return err;
	}

	return 0;
}

int haptics_stop(void)
{
	if (!haptics_ready) {
		return -ENODEV;
	}

	(void)k_work_cancel_delayable(&haptic_stop_work);
	return haptics_apply(0U, false);
}
