#include "audio_out.h"
#include "json_serial.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#define SAMPLE_RATE_HZ 16000
#define TONE_MS 120
#define NUM_SAMPLES ((SAMPLE_RATE_HZ * TONE_MS) / 1000)
#define BLOCK_SIZE (NUM_SAMPLES * sizeof(int16_t))

#define I2S_NODE DT_NODELABEL(i2s0)

static const struct device *i2s_dev = DEVICE_DT_GET_OR_NULL(I2S_NODE);
static int16_t sample_buf[NUM_SAMPLES];

static void build_tone(int freq_hz)
{
	for (int i = 0; i < NUM_SAMPLES; i++) {
		float t = (float)i / (float)SAMPLE_RATE_HZ;
		float s = sinf(2.0f * 3.1415926f * (float)freq_hz * t);
		sample_buf[i] = (int16_t)(s * 8000.0f);
	}
}

void audio_out_init(void)
{
	if (!i2s_dev || !device_is_ready(i2s_dev)) {
		json_emit_status("audio", "i2s device not ready");
		return;
	}

	struct i2s_config config = {
		.word_size = 16,
		.channels = 1,
		.format = I2S_FMT_DATA_FORMAT_I2S,
		.options = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER,
		.frame_clk_freq = SAMPLE_RATE_HZ,
		.mem_slab = NULL,
		.block_size = BLOCK_SIZE,
		.timeout = 1000,
	};

	int err = i2s_configure(i2s_dev, I2S_DIR_TX, &config);

	if (err) {
		json_emit_status("audio", "i2s configure failed");
		return;
	}

	json_emit_status("audio", "m5 core2 i2s audio ready");
}

void audio_out_play_event(const char *event)
{
	if (!i2s_dev || !device_is_ready(i2s_dev)) {
		json_emit_status("audio", "i2s not ready");
		return;
	}

	if (!event) {
		return;
	}

	if (strcmp(event, "eagle_killed") == 0) {
		build_tone(880);
		json_emit_audio_debug("eagle_killed", "tone_880hz");
	} else if (strcmp(event, "game_over") == 0) {
		build_tone(220);
		json_emit_audio_debug("game_over", "tone_220hz");
	} else {
		json_emit_audio_debug(event, "ignored_unknown_audio_event");
		return;
	}

	int err = i2s_write(i2s_dev, sample_buf, BLOCK_SIZE);

	if (err) {
		json_emit_status("audio", "i2s write failed");
		return;
	}

	(void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
	k_msleep(TONE_MS + 20);
	(void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
}