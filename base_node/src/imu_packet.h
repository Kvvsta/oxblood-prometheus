#ifndef IMU_PACKET_H
#define IMU_PACKET_H

#include <stdint.h>
#include <zephyr/sys/util.h>

struct imu_packet {
    int32_t gy;
    int32_t gz;
} __packed;

#endif