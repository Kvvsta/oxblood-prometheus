#ifndef IMU_PACKET_H
#define IMU_PACKET_H

#include <stdint.h>
#include <zephyr/sys/util.h>

struct imu_packet {
    uint32_t gy; 
    uint32_t gz;
} __packed; 

#endif