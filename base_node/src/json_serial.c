#include "json_serial.h"
#include "mobile_link.h"

#include <zephyr/sys/printk.h>

/*
 * Initialise JSON serial output.
 *
 * This emits a startup message confirming the serial JSON
 * interface is ready.
 */
void json_serial_init(void)
{
    printk("{\"status\":\"json_serial_ready\"}\n");
}

/*
 * Emit one IMU record to PC
 *
 */
void json_emit_imu(const char* node_name,
            uint32_t ts_ms,
            float gyro_y,
            float_gyro_z)
{

    printk("{\"type\":\"imu\",\"node\":\"%s\",\"ts_ms\":%u"
           "\"gyro_y\":%.6f,\"gyro_z\":%.6f}\n",
           node_name ? node_name : "unknown",
           ts_ms,
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
 * Audio debug output
 */
void json_emit_audio_debug(const char *event, const char *action)
{
    printk("{\"type\":\"audio_debug\",\"event\":\"%s\",\"action\":\"%s\"}\n",
           event ? event : "unknown",
           action ? action : "unknown");
}