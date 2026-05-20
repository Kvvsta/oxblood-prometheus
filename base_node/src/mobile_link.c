#include "mobile_link.h"
#include "json_serial.h"

#include <zephyr/kernel.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static struct k_msgq *app_q;

/*
 * Initialise the mobile-link layer with the message queue used by the
 * processing thread.
 */
void mobile_link_init(struct k_msgq *target_q)
{
    app_q = target_q;
}

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
				    const struct imu_packet *pkt)
{
    if (!node_name || !pkt) {
        return false;
    }

    if (len != sizeof(struct imu_packet)) {
        json_emit_status("imu_packet_error", "invalid packet length");
        return false;
    }

    // Derive player number from last character of name ("PROMETHEUS-P1" -> 1)
    /*int player = node_name[strlen(node_name) - 1] - '0';
    if (player < 1 || player > 9) {
        json_emit_status("imu_packet_error", "invalid player index in node name");
        return false;
    }*/

    float gyro_y = (float)pkt->gy / 1000000.0f;
    float gyro_z = (float)pkt->gz / 1000000.0f;

    json_emit_imu(node_name, gyro_y, gyro_z);

    return true;
}
