#include "serial_audio.h"
#include "audio_out.h"
#include "json_serial.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include <string.h>

#define RX_BUF_SIZE 128

static const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

static char rx_buf[RX_BUF_SIZE];
static size_t rx_len;

/*
 * Very small JSON/event parser.
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
		audio_out_play_event("eagle_killed");
	} else if (strstr(line, "game_over") != NULL) {
		audio_out_play_event("game_over");
	} else {
		json_emit_status("audio_rx", "unknown audio event");
	}
}

static void uart_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (!uart_irq_rx_ready(dev)) {
			continue;
		}

		uint8_t c;

		while (uart_fifo_read(dev, &c, 1) == 1) {
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
	}
}

void serial_audio_init(void)
{
	if (!device_is_ready(uart_dev)) {
		json_emit_status("serial_audio", "uart not ready");
		return;
	}

	rx_len = 0;

	uart_irq_callback_user_data_set(uart_dev, uart_cb, NULL);
	uart_irq_rx_enable(uart_dev);

	json_emit_status("serial_audio", "ready for audio commands");
}