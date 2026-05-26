#include "serial_audio.h"
#include "audio_out.h"
#include "json_serial.h"
#include "ui_display.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#define RX_BUF_SIZE 128
#define LINE_BUF_SIZE 128
#define SERIAL_AUDIO_STACK_SIZE 2048
#define SERIAL_AUDIO_PRIORITY 7

static const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
static char rx_buf[RX_BUF_SIZE];
static char line_buf[LINE_BUF_SIZE];
static size_t rx_len;
static K_SEM_DEFINE(line_ready_sem, 0, 1);

void serial_processing_thread(void *a, void *b, void *c);
K_THREAD_DEFINE(serial_audio_tid,
                SERIAL_AUDIO_STACK_SIZE,
                serial_processing_thread,
                NULL, NULL, NULL,
                SERIAL_AUDIO_PRIORITY,
                0,
                0);
				

/*
 * Parse JSON packets from the PC backend.
 *
 * Supported packets:
 *     {"type":"score","p1":1,"p2":0,"high":4}
 *     {"type":"gesture","gesture":"START"}
 *     {"type":"audio","event":"eagle_killed"}
 */
static void process_line(const char *line) {
	if (!line) {
		return;
	}

	// Process score json packets
	if (strstr(line, "\"type\":\"score\"") != NULL ||
    strstr(line, "\"type\": \"score\"") != NULL) {

		int p1 = 0;
		int p2 = 0;
		int high = 0;

		char* p;

		p = strstr(line, "\"p1\":");
		if (p) {
			p1 = atoi(p + 5);
		}

		p = strstr(line, "\"p2\":");
		if (p) {
			p2 = atoi(p + 5);
		}

		p = strstr(line, "\"high\":");
		if (p) {
			high = atoi(p + 7);
		}

		ui_display_update_score(p1, p2, high);
		return;
	}

	// Process winner json packets
	if (strstr(line, "\"type\":\"winner\"") != NULL ||
    	strstr(line, "\"type\": \"winner\"") != NULL) {

		if (strstr(line, "P1")) {
			ui_display_update_winner("P1");
		} else if (strstr(line, "P2")) {
			ui_display_update_winner("P2");
		} else {
			ui_display_update_winner("NONE");
		}

    	return;
	}

	// Process gesture packets
	if (strstr(line, "\"type\":\"gesture\"") != NULL ||
		strstr(line, "\"type\": \"gesture\"") != NULL) {

		if (strstr(line, "START")) {
			ui_display_update_gesture("START");
		} else if (strstr(line, "PAUSE")) {
			ui_display_update_gesture("PAUSE");
		} else if (strstr(line, "RESTART")) {
			ui_display_update_gesture("RESTART");
		} else {
			ui_display_update_gesture("NONE");
		}

		return;
	}

	// Process audio json packets
	if (strstr(line, "\"type\":\"audio\"") == NULL &&
	    strstr(line, "\"type\": \"audio\"") == NULL) {
		return;
	}

	// Process event json packets 
	if (strstr(line, "eagle_killed") != NULL) {
		ui_display_update_audio("eagle_killed");
		audio_out_queue_event("eagle_killed");
	} else if (strstr(line, "game_over") != NULL) {
		ui_display_update_audio("game_over");
		audio_out_queue_event("game_over");
	} else if (strstr(line, "ready") != NULL) {
    	ui_display_update_audio("ready"); 
	} else {
		json_emit_status("audio_rx", "unknown audio event");
	}
}

/*
 * UART ISR callback.
 *
 * Reads bytes from UART FIFO and constructs newline-terminated messages.
 */
static void uart_cb(const struct device *dev, void *user_data) {
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (!uart_irq_rx_ready(dev)) {
			continue;
		}

		uint8_t ch;

		while (uart_fifo_read(dev, &ch, 1) == 1) {
			if (ch == '\r') {
				continue;
			}

			if (ch == '\n') {
				rx_buf[rx_len] = '\0';
				memcpy(line_buf, rx_buf, rx_len + 1);
                rx_len = 0;
                k_sem_give(&line_ready_sem);
                continue;
			}

			if (rx_len < sizeof(rx_buf) - 1) {
				rx_buf[rx_len++] = (char)ch;
			} else {
				rx_len = 0;
			}
		}
	}
}

/*
 * Initialise UART receive interrupt handling.
 */
void serial_init(void)
{
	if (!device_is_ready(uart_dev)) {
		json_emit_status("serial", "uart not ready");
		return;
	}

	rx_len = 0;

	uart_irq_callback_user_data_set(uart_dev, uart_cb, NULL);
	uart_irq_rx_enable(uart_dev);

	json_emit_status("serial", "ready for audio commands");
}

/*
 * Thread that processes complete UART lines queued by uart_cb().
 */
void serial_processing_thread(void *a, void *b, void *c) {
    while (1) {
		// Takes semaphore for line ready; then processes line
        k_sem_take(&line_ready_sem, K_FOREVER);
        process_line(line_buf);
    }
}