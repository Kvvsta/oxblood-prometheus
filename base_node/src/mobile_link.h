#ifndef MOBILE_LINK_H
#define MOBILE_LINK_H

#include <stdbool.h>
#include <zephyr/kernel.h>
#include "imu_packet.h"

void mobile_link_init(struct k_msgq *target_q);
void mobile_link_start(void);
bool mobile_link_process_imu_packet(const char *node_name, const struct imu_packet *pkt);

#endif /* MOBILE_LINK_H */
