#ifndef MOBILE_LINK_H
#define MOBILE_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include "imu_packet.h"

void mobile_link_start(void);
bool mobile_link_process_imu_packet(const char *node_name, const struct imu_packet *pkt,
        size_t len);

#endif /* MOBILE_LINK_H */
