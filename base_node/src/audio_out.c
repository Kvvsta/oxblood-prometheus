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
#define NUM_FRAMES ((SAMPLE_RATE_HZ * TONE_MS) / 1000)
#define NUM_SAMPLES (2 * NUM_FRAMES)
#define BLOCK_SIZE (NUM_SAMPLES * sizeof(int16_t))

#define I2S_NODE DT_NODELABEL(i2s0)
#define BLOCK_COUNT 8

#define AUDIO_EVENT_MAX_LEN 24
#define AUDIO_QUEUE_LEN 4
#define AUDIO_STACK_SIZE 2048
#define AUDIO_PRIORITY 5

K_MSGQ_DEFINE(audio_msgq, AUDIO_EVENT_MAX_LEN, AUDIO_QUEUE_LEN, 4);

static void audio_thread(void *a, void *b, void *c);
K_THREAD_DEFINE(audio_tid, AUDIO_STACK_SIZE, audio_thread, NULL, NULL, NULL,
                AUDIO_PRIORITY, 0, 0);

K_MEM_SLAB_DEFINE(i2s_mem_slab, BLOCK_SIZE, BLOCK_COUNT, 4);

static const struct device *i2s_dev = DEVICE_DT_GET_OR_NULL(I2S_NODE);
static int16_t sample_buf[NUM_SAMPLES];

void audio_out_queue_event(const char* event) {
    char msg[AUDIO_EVENT_MAX_LEN] = {0};

    if (!event) {
        return;
    }

    strncpy(msg, event, sizeof(msg) - 1);

    if (k_msgq_put(&audio_msgq, msg, K_NO_WAIT) != 0) {
        json_emit_status("audio", "audio queue full");
    }
}

static void audio_thread(void* a, void* b, void* c) {

    char event[AUDIO_EVENT_MAX_LEN];

    while (1) {
        k_msgq_get(&audio_msgq, event, K_FOREVER);
        audio_out_play_event(event);
    }
}

static void build_tone(int freq_hz)
{
	for (int i = 0; i < NUM_FRAMES; i++) {
		float t = (float)i / (float)SAMPLE_RATE_HZ;
		float s = sinf(2.0f * 3.1415926f * (float)freq_hz * t);
		int16_t sample = (int16_t)(s * 8000.0f);

        sample_buf[2 * i] = sample;       // left
        sample_buf[2 * i + 1] = sample;   // right
	}
}

void audio_out_init(void)
{
	/*if (!i2s_dev || !device_is_ready(i2s_dev)) {
		json_emit_status("audio", "i2s device not ready");
		return;
	}*/

	if (!i2s_dev) {
        json_emit_status("audio", "i2s device pointer is null");
        return;
    }

    if (!device_is_ready(i2s_dev)) {
        json_emit_status("audio", "i2s device exists but is not ready");
        return;
    }

	struct i2s_config config = {
		.word_size = 16,
		.channels = 2,
		.format = I2S_FMT_DATA_FORMAT_I2S,
		.options = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER,
		.frame_clk_freq = SAMPLE_RATE_HZ,
		.mem_slab = &i2s_mem_slab,
		.block_size = BLOCK_SIZE,
		.timeout = 1000,
	};

	int err = i2s_configure(i2s_dev, I2S_DIR_TX, &config);

	if (err) {
		printk("i2s_configure failed err=%d\n", err);
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
		build_tone(4000);
		json_emit_audio_debug("eagle_killed", "tone_880hz");
	} else if (strcmp(event, "game_over") == 0) {
		build_tone(800);
		json_emit_audio_debug("game_over", "tone_220hz");
	} else {
		json_emit_audio_debug(event, "ignored_unknown_audio_event");
		return;
	}

	void* tx_block;
	int err = k_mem_slab_alloc(&i2s_mem_slab, &tx_block, K_MSEC(100));

	if (err) {
		json_emit_status("audio", "i2s slab alloc failed");
		return;
	}

	memcpy(tx_block, sample_buf, BLOCK_SIZE);

	err = i2s_write(i2s_dev, tx_block, BLOCK_SIZE);

	if (err) {
		printk("i2s_write failed err=%d\n", err);
		k_mem_slab_free(&i2s_mem_slab, tx_block);
		json_emit_status("audio", "i2s write failed");
		return;
	}

	(void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
	k_msleep(TONE_MS + 20);
	(void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
}