#include "mobile_link.h"
#include "json_serial.h"

#include <zephyr/kernel.h>

#include <math.h>
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
 * Raw-json parser is ready.
 */
void mobile_link_start(void)
{
    json_emit_status("mobile_link",
                     "ready to process raw 13-byte RSSI snapshots");
}

// Porcesses one packed IMU packet from a mobile node
bool mobile_link_process_imu_packet(const char* node_name, 
            const struct imu_packet* pkt) {
    if (!node_name || !pkt) {
        return false;
    }

    if (len != sizeof(struct imu_packet)) {
        json_emit_status("imu_packet_error", "invalid IMU packet length");
        return false; 
    }

    float gyro_y = (float)pkt->gy / 1000000.0f;
    float gyro_z = (float)pkt->gz / 1000000.0f;

    json_emit_imu(node_name, k_uptime_get_32(), gyro_y, gyro_z);

    return true; 
    }