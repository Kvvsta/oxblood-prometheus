#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include "colour_detect.h"

void wifi_mqtt_run(void);
void wifi_mqtt_publish_colour(colour_t c);
void wifi_mqtt_wait_ready(void);

#endif /* WIFI_MQTT_H */
