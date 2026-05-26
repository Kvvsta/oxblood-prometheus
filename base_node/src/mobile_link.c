#include "mobile_link.h"
#include "json_serial.h"
#include "ui_display.h"

#include <zephyr/kernel.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static int64_t last_ui_update_ms;

/*
 * Emit a startup status message.
 */
void mobile_link_start(void)
{
    json_emit_status("mobile_link", "ready");
}

/*
 * Process one packed IMU packet from a mobile node and emit JSON to serial.
 *
 * Extracts the player number from the last character of node_name
 * e.g. "PROMETHEUS-P1" -> player 1.
 */
bool mobile_link_process_imu_packet(const char *node_name,
				    const struct imu_packet *pkt, size_t len)
{
    if (!node_name || !pkt) {
        return false;
    }

    if (len != sizeof(struct imu_packet)) {
        json_emit_status("imu_packet_error", "invalid packet length");
        return false;
    }

    float gyro_y = (float)pkt->gy / 1000000.0f;
    float gyro_z = (float)pkt->gz / 1000000.0f;

    int player = -1;

    if (strcmp(node_name, "PROMETHEUS-P1") == 0) {
        player = 1;
    } else if (strcmp(node_name, "PROMETHEUS-P2") == 0) {
        player = 2;
    }

    int64_t now = k_uptime_get();

    if (player > 0 && now - last_ui_update_ms >= 200) {
        ui_display_update_imu(player, gyro_y, gyro_z);
        last_ui_update_ms = now;
    }

    json_emit_imu(node_name, gyro_y, gyro_z);

    return true;
}
