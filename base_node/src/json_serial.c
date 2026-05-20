#include "json_serial.h"
#include "mobile_link.h"

#include <zephyr/sys/printk.h>
#include <string.h>

/*
 * Initialise JSON serial output.
 *
 * Emits a startup message confirming the serial JSON interface is ready.
 */
void json_serial_init(void)
{
    printk("{\"status\":\"json_serial_ready\"}\n");
}

/*
 * Emit one IMU record to PC.
 *
 * Output format matches what liver_backend.py and game.js expect:
 *   {"player":1,"gy":1.234567,"gz":-0.678901}
 */
void json_emit_imu(const char* node_name, float gyro_y, float gyro_z)
{
    int player = -1; 
    
    if (strcmp(node_name, "PROMETHEUS-P1") == 0) {
        player = 1;

    } else if (strcmp(node_name, "PROMETHEUS-P2") == 0){
        player = 2; 
    } else {
        json_emit_status("imu_packet_error", "unknown player node");
        return;
    }

    printk("{\"player\":%d,\"gy\":%.4f,\"gz\":%.4f}\n",
           player,
           (double)gyro_y,
           (double)gyro_z);
}

/*
 * Emit a simple status/debug message.
 */
void json_emit_status(const char *status, const char *detail)
{
    printk("{\"type\":\"status\",\"status\":\"%s\",\"detail\":\"%s\"}\n",
           status ? status : "unknown",
           detail ? detail : "");
}

/*
 * Emit an audio debug message.
 */
void json_emit_audio_debug(const char *event, const char *action)
{
    printk("{\"type\":\"audio_debug\",\"event\":\"%s\",\"action\":\"%s\"}\n",
           event ? event : "unknown",
           action ? action : "unknown");
}
