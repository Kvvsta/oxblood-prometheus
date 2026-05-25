#include "serial_audio.h"
#include "audio_out.h"
#include "json_serial.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include <string.h>

#define RX_BUF_SIZE 128
#define SERIAL_AUDIO_STACK_SIZE 2048
#define SERIAL_AUDIO_PRIORITY 7

static const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

static char rx_buf[RX_BUF_SIZE];
static size_t rx_len;


/*
 * JSON/event parser.
 *
 * Expected input from bridge.py:
 *
 *     {"type":"audio","event":"eagle_killed"}
 *
 * This avoids needing a full JSON parser on the base node.
 */
static void process_line(const char *line)
{
	if (!line) {
		return;
	}

	if (strstr(line, "\"type\":\"audio\"") == NULL &&
	    strstr(line, "\"type\": \"audio\"") == NULL) {
		return;
	}

	if (strstr(line, "eagle_killed") != NULL) {
		audio_out_queue_event("eagle_killed");
	} else if (strstr(line, "game_over") != NULL) {
		audio_out_queue_event("game_over");
	} else {
		json_emit_status("audio_rx", "unknown audio event");
	}
}

static void serial_audio_thread(void *a, void *b, void *c) {

    while (1) {
        unsigned char c;

        while (uart_poll_in(uart_dev, &c) == 0) {
            if (c == '\r') {
                continue;
            }

            if (c == '\n') {
                rx_buf[rx_len] = '\0';
                process_line(rx_buf);
                rx_len = 0;
                continue;
            }

            if (rx_len < sizeof(rx_buf) - 1) {
                rx_buf[rx_len++] = (char)c;
            } else {
                rx_len = 0;
                json_emit_status("serial_rx", "line too long");
            }
        }

        k_sleep(K_MSEC(2));
    }
}

K_THREAD_DEFINE(serial_audio_tid,
                SERIAL_AUDIO_STACK_SIZE,
                serial_audio_thread,
                NULL, NULL, NULL,
                SERIAL_AUDIO_PRIORITY,
                0,
                0);

void serial_audio_init(void)
{
	if (!device_is_ready(uart_dev)) {
		json_emit_status("serial_audio", "uart not ready");
		return;
	}

	json_emit_status("serial_audio", "ready for audio commands");
}