/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "heart_rate.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "display_ui.h"

LOG_MODULE_REGISTER(heart_rate, LOG_LEVEL_INF);

#define MAX30102_NODE DT_NODELABEL(max30102)

#define HR_MEASUREMENT_TIMEOUT_MS 20000
#define HR_MIN_IR_COUNTS 5000
#define HR_MIN_AC_COUNTS 300
#define HR_MIN_BEAT_INTERVAL_MS 300
#define HR_MAX_BEAT_INTERVAL_MS 1500
#define HR_REQUIRED_INTERVALS 4
#define HR_NO_FINGER_SAMPLES 10

static const struct device *const max30102 = DEVICE_DT_GET(MAX30102_NODE);
static const struct sensor_trigger ppg_trigger = {
	.type = SENSOR_TRIG_DATA_READY,
	.chan = SENSOR_CHAN_IR,
};

static struct heart_rate_status latest_status = {
	.state = HEART_RATE_DISABLED,
};
static struct k_spinlock status_lock;

static bool heart_rate_ready;
static bool measurement_active;
static uint32_t sample_count;
static uint32_t samples_with_finger;
static uint32_t no_finger_count;
static uint32_t last_beat_ms;
static uint8_t bpm_interval_count;
static uint16_t averaged_bpm;
static int32_t ir_baseline = -1;
static int32_t previous_filtered;
static int32_t signal_peak;
static int32_t signal_trough;

static void publish_status(enum heart_rate_state state, bool bpm_valid,
			   uint16_t bpm, uint8_t quality)
{
	bool changed;
	k_spinlock_key_t key = k_spin_lock(&status_lock);

	changed = (latest_status.state != state) ||
		  (latest_status.bpm_valid != bpm_valid) ||
		  (latest_status.bpm != bpm) ||
		  (latest_status.quality != quality);

	latest_status.state = state;
	latest_status.bpm_valid = bpm_valid;
	latest_status.bpm = bpm;
	latest_status.quality = quality;

	k_spin_unlock(&status_lock, key);

	if (changed) {
		display_ui_request_update();
	}
}

static void reset_signal_detector(void)
{
	ir_baseline = -1;
	previous_filtered = 0;
	signal_peak = 0;
	signal_trough = 0;
	last_beat_ms = 0U;
	bpm_interval_count = 0U;
	averaged_bpm = 0U;
}

static uint8_t quality_from_signal(int32_t amplitude_counts)
{
	uint32_t quality;

	if (amplitude_counts < 0) {
		amplitude_counts = 0;
	}

	quality = ((uint32_t)bpm_interval_count * 15U) +
		  MIN((uint32_t)amplitude_counts / 80U, 40U);

	return (uint8_t)MIN(quality, 100U);
}

static void disable_sensor_trigger(void)
{
	const int err = sensor_trigger_set(max30102, &ppg_trigger, NULL);

	if (err != 0) {
		LOG_WRN("MAX30102 trigger disable failed (%d)", err);
	}
}

static void finish_measurement(enum heart_rate_state fallback_state)
{
	struct heart_rate_status status;

	if (!measurement_active) {
		return;
	}

	measurement_active = false;
	disable_sensor_trigger();

	heart_rate_get_status(&status);
	if (status.bpm_valid) {
		publish_status(HEART_RATE_READY, true, status.bpm, status.quality);
		LOG_INF("Heart-rate measurement ready: %u BPM quality %u",
			status.bpm, status.quality);
	} else {
		publish_status(fallback_state, false, 0U, 0U);
		LOG_INF("Heart-rate measurement ended without a valid BPM");
	}
}

static void process_ir_sample(uint32_t ir_counts)
{
	const uint32_t now_ms = k_uptime_get_32();
	int32_t filtered;
	int32_t amplitude;
	int32_t threshold;

	sample_count++;

	if (ir_counts < HR_MIN_IR_COUNTS) {
		no_finger_count++;
		samples_with_finger = 0U;
		reset_signal_detector();

		if (no_finger_count >= HR_NO_FINGER_SAMPLES) {
			publish_status(HEART_RATE_NO_FINGER, false, 0U, 0U);
		}
		return;
	}

	no_finger_count = 0U;
	samples_with_finger++;

	if (ir_baseline < 0) {
		ir_baseline = (int32_t)ir_counts;
		publish_status(HEART_RATE_SEARCHING, false, 0U, 0U);
		return;
	}

	filtered = (int32_t)ir_counts - ir_baseline;
	ir_baseline = ((ir_baseline * 31) + (int32_t)ir_counts) / 32;

	if (filtered > signal_peak) {
		signal_peak = filtered;
	}

	if (filtered < signal_trough) {
		signal_trough = filtered;
	}

	amplitude = signal_peak - signal_trough;
	threshold = amplitude / 3;
	if (threshold < HR_MIN_AC_COUNTS) {
		threshold = HR_MIN_AC_COUNTS;
	}

	if ((samples_with_finger >= HR_NO_FINGER_SAMPLES) &&
	    (latest_status.state == HEART_RATE_SEARCHING)) {
		publish_status(HEART_RATE_MEASURING, false, 0U,
			       quality_from_signal(amplitude));
	}

	if ((previous_filtered <= threshold) && (filtered > threshold)) {
		const uint32_t interval_ms = now_ms - last_beat_ms;

		if ((last_beat_ms != 0U) && (interval_ms >= HR_MIN_BEAT_INTERVAL_MS) &&
		    (interval_ms <= HR_MAX_BEAT_INTERVAL_MS)) {
			const uint16_t instant_bpm = (uint16_t)(60000U / interval_ms);
			const uint8_t quality = quality_from_signal(amplitude);

			if (bpm_interval_count == 0U) {
				averaged_bpm = instant_bpm;
			} else {
				averaged_bpm = (uint16_t)(((uint32_t)averaged_bpm * 3U +
							   instant_bpm) / 4U);
			}

			if (bpm_interval_count < UINT8_MAX) {
				bpm_interval_count++;
			}

			publish_status(bpm_interval_count >= 2U ? HEART_RATE_READY :
				       HEART_RATE_MEASURING,
				       bpm_interval_count >= 2U, averaged_bpm, quality);

			if (bpm_interval_count >= HR_REQUIRED_INTERVALS) {
				finish_measurement(HEART_RATE_READY);
			}
		}

		if ((last_beat_ms == 0U) || (interval_ms >= HR_MIN_BEAT_INTERVAL_MS)) {
			last_beat_ms = now_ms;
		}
	}

	previous_filtered = filtered;
	signal_peak = (signal_peak * 31) / 32;
	signal_trough = (signal_trough * 31) / 32;
}

static void heart_rate_sample_work_handler(struct k_work *work)
{
	struct sensor_value ir;
	int err;

	ARG_UNUSED(work);

	if (!measurement_active) {
		return;
	}

	err = sensor_sample_fetch(max30102);
	if (err != 0) {
		LOG_WRN("MAX30102 sample fetch failed (%d)", err);
		finish_measurement(HEART_RATE_SENSOR_ERROR);
		return;
	}

	err = sensor_channel_get(max30102, SENSOR_CHAN_IR, &ir);
	if (err != 0) {
		LOG_WRN("MAX30102 IR read failed (%d)", err);
		finish_measurement(HEART_RATE_SENSOR_ERROR);
		return;
	}

	process_ir_sample((uint32_t)MAX(ir.val1, 0));
}

K_WORK_DEFINE(heart_rate_sample_work, heart_rate_sample_work_handler);

static void heart_rate_timeout_work_handler(struct k_work *work)
{
	enum heart_rate_state fallback = HEART_RATE_POOR_SIGNAL;

	ARG_UNUSED(work);

	if (sample_count == 0U) {
		fallback = HEART_RATE_SENSOR_ERROR;
	} else if (samples_with_finger == 0U) {
		fallback = HEART_RATE_NO_FINGER;
	}

	finish_measurement(fallback);
}

K_WORK_DELAYABLE_DEFINE(heart_rate_timeout_work, heart_rate_timeout_work_handler);

static void heart_rate_trigger_handler(const struct device *dev,
				       const struct sensor_trigger *trigger)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(trigger);

	if (measurement_active) {
		(void)k_work_submit(&heart_rate_sample_work);
	}
}

int heart_rate_init(void)
{
	if (!device_is_ready(max30102)) {
		publish_status(HEART_RATE_DISABLED, false, 0U, 0U);
		LOG_ERR("MAX30102 is not ready for heart-rate sampling");
		return -ENODEV;
	}

	heart_rate_ready = true;
	publish_status(HEART_RATE_IDLE, false, 0U, 0U);
	LOG_INF("Heart-rate service ready");
	return 0;
}

int heart_rate_start_measurement(void)
{
	int err;

	if (!heart_rate_ready) {
		return -ENODEV;
	}

	measurement_active = false;
	disable_sensor_trigger();
	(void)k_work_cancel_delayable(&heart_rate_timeout_work);

	sample_count = 0U;
	samples_with_finger = 0U;
	no_finger_count = 0U;
	reset_signal_detector();
	publish_status(HEART_RATE_SEARCHING, false, 0U, 0U);

	err = sensor_trigger_set(max30102, &ppg_trigger, heart_rate_trigger_handler);
	if (err != 0) {
		publish_status(HEART_RATE_SENSOR_ERROR, false, 0U, 0U);
		LOG_ERR("MAX30102 trigger enable failed (%d)", err);
		return err;
	}

	measurement_active = true;
	(void)k_work_schedule(&heart_rate_timeout_work, K_MSEC(HR_MEASUREMENT_TIMEOUT_MS));
	(void)k_work_submit(&heart_rate_sample_work);
	LOG_INF("Heart-rate measurement started");
	return 0;
}

void heart_rate_get_status(struct heart_rate_status *status)
{
	k_spinlock_key_t key;

	if (status == NULL) {
		return;
	}

	key = k_spin_lock(&status_lock);
	*status = latest_status;
	k_spin_unlock(&status_lock, key);
}
