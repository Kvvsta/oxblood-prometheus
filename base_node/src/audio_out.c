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
#define CHANNELS 2
#define BLOCK_FRAMES 256
#define BLOCK_SIZE (BLOCK_FRAMES * CHANNELS * sizeof(int16_t))
#define BLOCK_COUNT 4
#define TOTAL_FRAMES ((SAMPLE_RATE_HZ * TONE_MS) / 1000)
#define I2S_NODE DT_NODELABEL(i2s0)
#define AUDIO_EVENT_MAX_LEN 24
#define AUDIO_QUEUE_LEN 4
#define AUDIO_STACK_SIZE 2048
#define AUDIO_PRIORITY 10

K_MSGQ_DEFINE(audio_msgq, AUDIO_EVENT_MAX_LEN, AUDIO_QUEUE_LEN, 4);

static void audio_thread(void *a, void *b, void *c);
K_THREAD_DEFINE(audio_tid, AUDIO_STACK_SIZE, audio_thread, NULL, NULL, NULL,
                AUDIO_PRIORITY, 0, 0);

//K_MEM_SLAB_DEFINE(i2s_mem_slab, BLOCK_SIZE, BLOCK_COUNT, 4);
K_MEM_SLAB_DEFINE_IN_SECT_STATIC(i2s_mem_slab, __nocache, BLOCK_SIZE, BLOCK_COUNT, 4);

static const struct device *i2s_dev = DEVICE_DT_GET_OR_NULL(I2S_NODE);
//static int16_t sample_buf[NUM_SAMPLES];

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

static void fill_tone_block(int16_t *buf, int freq_hz, int start_frame, int frames)
{
	for (int i = 0; i < frames; i++) {
		float t = (float)(start_frame + i) / (float)SAMPLE_RATE_HZ;
		float s = sinf(2.0f * 3.1415926f * (float)freq_hz * t);
		int16_t sample = (int16_t)(s * 8000.0f);

		buf[2 * i] = sample;
		buf[2 * i + 1] = sample;
	}

	for (int i = frames; i < BLOCK_FRAMES; i++) {
		buf[2 * i] = 0;
		buf[2 * i + 1] = 0;
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

	int freq_hz;

	if (strcmp(event, "eagle_killed") == 0) {
		freq_hz = 4000;
		json_emit_audio_debug("eagle_killed", "tone_4000hz");
	} else if (strcmp(event, "game_over") == 0) {
		freq_hz = 800;
		json_emit_audio_debug("game_over", "tone_800hz");
	} else {
		json_emit_audio_debug(event, "ignored_unknown_audio_event");
		return;
	}

	int frame = 0;
	bool started = false;

	while (frame < TOTAL_FRAMES) {
		void *tx_block;
		int frames_this_block = TOTAL_FRAMES - frame;

		if (frames_this_block > BLOCK_FRAMES) {
			frames_this_block = BLOCK_FRAMES;
		}

		int err = k_mem_slab_alloc(&i2s_mem_slab, &tx_block, K_MSEC(100));
		if (err) {
			json_emit_status("audio", "i2s slab alloc failed");
			break;
		}

		fill_tone_block((int16_t *)tx_block, freq_hz, frame, frames_this_block);

		err = i2s_write(i2s_dev, tx_block, BLOCK_SIZE);
		if (err) {
			k_mem_slab_free(&i2s_mem_slab, tx_block);
			json_emit_status("audio", "i2s write failed");
			break;
		}

		if (!started) {
			err = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
			if (err) {
				json_emit_status("audio", "i2s start failed");
				break;
			}
			started = true;
		}

		frame += frames_this_block;
	}

	if (started) {
		(void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
	}
}