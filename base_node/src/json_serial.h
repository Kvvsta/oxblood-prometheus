#ifndef JSON_SERIAL_H
#define JSON_SERIAL_H

#include <stdint.h>

void json_serial_init(void);
void json_emit_imu(const char* node_name, float gyro_y, float gyro_z);
void json_emit_status(const char *status, const char *detail);
void json_emit_audio_debug(const char *event, const char *action);

#endif /* JSON_SERIAL_H */
