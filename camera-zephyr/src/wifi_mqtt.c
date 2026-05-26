/*
 * wifi_mqtt.c — Wifi connection & MQTT publisher for Zephyr
 *
 *   Connects to the configured WiFi though WPA2-PSK
 *      then waits for DHCP to assign an address
 *      then establishes an MQTT
 * 
 *   References: 
 *      zephyr/samples/net/mqtt_publisher
 */

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <errno.h>
#include "wifi_mqtt.h"
#include "colour_detect.h"

LOG_MODULE_REGISTER(wifi_mqtt, LOG_LEVEL_INF);

// my wifi hotspot name and password and details
#define WIFI_SSID "Yo"
#define WIFI_PASSWORD "deeznuts"
#define MQTT_BROKER_IP "172.20.10.5"
// broker details
#define MQTT_PORT 1883
#define MQTT_TOPIC "prometheus/gesture"
#define MQTT_CLIENT_ID "prometheus-camera-zephyr"
#define MQTT_BUFFER_SIZE 256
#define CONNECT_TIMEOUT_MS 2000
#define CONNECT_TRIES 10


static struct mqtt_client      client_ctx;
static struct sockaddr_storage broker;
static uint8_t                 rx_buf[MQTT_BUFFER_SIZE];
static uint8_t                 tx_buf[MQTT_BUFFER_SIZE];

static struct zsock_pollfd fds[1];
static int                 nfds;
static bool          mqtt_connected;

static K_SEM_DEFINE(wifi_connected_sem, 0, 1);
static K_SEM_DEFINE(mqtt_ready_sem, 0, 1);
static K_MUTEX_DEFINE(publish_mutex);

// Wifi event callback
static struct net_mgmt_event_callback wifi_mgmt_cb;

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface)
{
    ARG_UNUSED(cb);
    ARG_UNUSED(iface);

    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        const struct wifi_status *status =
            (const struct wifi_status *)cb->info;

        if (status->status == 0) {
            LOG_INF("WiFi associated");
            k_sem_give(&wifi_connected_sem);
        } else {
            LOG_ERR("WiFi connect failed (status %d)", status->status);
        }
    }
}

static void wifi_connect(void)
{
    struct net_if *iface = net_if_get_default();

    net_mgmt_init_event_callback(&wifi_mgmt_cb, wifi_mgmt_event_handler, NET_EVENT_WIFI_CONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_mgmt_cb);

    struct wifi_connect_req_params params = {
        .ssid = (const uint8_t *)WIFI_SSID,
        .ssid_length = strlen(WIFI_SSID),
        .psk = (const uint8_t *)WIFI_PASSWORD,
        .psk_length = strlen(WIFI_PASSWORD),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
    };

    LOG_INF("Requesting WiFi connect: %s", WIFI_SSID);

    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
    if (ret < 0) {
        LOG_ERR("WiFi connect request failed (%d)", ret);
    }
}

static void wait_for_network(void)
{
    wifi_connect();

    // block until Wifi connectyion succeeds
    k_sem_take(&wifi_connected_sem, K_FOREVER);

    // give DHCP time to assign an address.
    LOG_INF("WiFi up — waiting 3 s for DHCP...");
    k_sleep(K_SECONDS(3));
    LOG_INF("Network ready");
}

// mqtt event handler
static void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt)
{
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result != 0) {
            LOG_ERR("MQTT CONNACK error %d", evt->result);
            break;
        }
        mqtt_connected = true;
        LOG_INF("MQTT connected to %s:%d", MQTT_BROKER_IP, MQTT_PORT);
        k_sem_give(&mqtt_ready_sem); // unblock main -> camera can now start 
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_INF("MQTT disconnected (%d)", evt->result);
        mqtt_connected = false;
        nfds = 0;
        break;

    case MQTT_EVT_PUBACK:
    case MQTT_EVT_PUBLISH:
        break;

    default:
        break;
    }
}

// mqtt broker initialisation 
static void broker_init(void)
{
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;

    broker4->sin_family = AF_INET;
    broker4->sin_port = htons(MQTT_PORT);
    net_addr_pton(AF_INET, MQTT_BROKER_IP, &broker4->sin_addr);
}

// mqtt client initialisation from mqtt_publisher sample
static void client_init(struct mqtt_client *client)
{
    mqtt_client_init(client);

    broker_init();

    client->broker           = &broker;
    client->evt_cb           = mqtt_evt_handler;
    client->client_id.utf8   = (uint8_t *)MQTT_CLIENT_ID;
    client->client_id.size   = strlen(MQTT_CLIENT_ID);
    client->password         = NULL;
    client->user_name        = NULL;
    client->protocol_version = MQTT_VERSION_3_1_1;
    client->rx_buf           = rx_buf;
    client->rx_buf_size      = sizeof(rx_buf);
    client->tx_buf           = tx_buf;
    client->tx_buf_size      = sizeof(tx_buf);
    client->transport.type   = MQTT_TRANSPORT_NON_SECURE;
}

//poll helpers from mqtt_publisher sample
static void prepare_fds(struct mqtt_client *client)
{
    fds[0].fd = client->transport.tcp.sock;
    fds[0].events = ZSOCK_POLLIN;
    nfds = 1;
}

static int do_poll(int timeout_ms)
{
    if (nfds <= 0) {
        return 0;
    }
    int ret = zsock_poll(fds, nfds, timeout_ms);
    if (ret < 0) {
        LOG_ERR("poll error: %d", errno);
    }
    return ret;
}

// retry conenction from mqtt_publisher sample 
static int try_to_connect(void)
{
    int rc;

    for (int i = 0; i < CONNECT_TRIES && !mqtt_connected; i++) {
        client_init(&client_ctx);

        rc = mqtt_connect(&client_ctx);
        if (rc != 0) {
            LOG_ERR("mqtt_connect failed (%d), try %d/%d",
                    rc, i + 1, CONNECT_TRIES);
            k_sleep(K_MSEC(500));
            continue;
        }

        prepare_fds(&client_ctx);

        if (do_poll(CONNECT_TIMEOUT_MS)) {
            mqtt_input(&client_ctx);
        }

        if (!mqtt_connected) {
            mqtt_abort(&client_ctx);
        }
    }

    return mqtt_connected ? 0 : -ETIMEDOUT;
}

// block until WIFI & MQTT are ready
void wifi_mqtt_wait_ready(void)
{
    k_sem_take(&mqtt_ready_sem, K_FOREVER);
}

// infinite WIFI & MQTTloop 
void wifi_mqtt_run(void)
{
    wait_for_network();

    while (true) {
        if (!mqtt_connected) {
            LOG_INF("Connecting to MQTT broker...");
            if (try_to_connect() < 0) {
                LOG_ERR("MQTT connect failed — retry in 5 s");
                k_sleep(K_SECONDS(5));
                continue;
            }
        }

        int ret = do_poll(mqtt_keepalive_time_left(&client_ctx));
        if (ret > 0 && (fds[0].revents & ZSOCK_POLLIN)) {
            mqtt_input(&client_ctx);
        }
        mqtt_live(&client_ctx);
    }
}

//publish colour to MQTT 
void wifi_mqtt_publish_colour(colour_t c)
{
    if (!mqtt_connected) {
        return;
    }

    const char *cmd;
    switch (c) {
    case COLOUR_RED: cmd = "START"; break;
    case COLOUR_ORANGE: cmd = "PAUSE"; break;
    case COLOUR_PURPLE: cmd = "RESTART"; break;
    default: return; // COLOUR_NONE -> nothing to send 
    }

    struct mqtt_publish_param p = {
        .message = {
            .topic = {
                .qos = MQTT_QOS_0_AT_MOST_ONCE,
                .topic = {.utf8 = (uint8_t *)MQTT_TOPIC, .size = strlen(MQTT_TOPIC)},
            },
            .payload = {
                .data = (uint8_t *)cmd,
                .len = (uint32_t)strlen(cmd),
            },
        },
        .message_id = 0,
        .dup_flag = 0,
        .retain_flag = 0,
    };

    k_mutex_lock(&publish_mutex, K_FOREVER);
    int ret = mqtt_publish(&client_ctx, &p);
    k_mutex_unlock(&publish_mutex);

    if (ret < 0) {
        LOG_ERR("mqtt_publish failed (%d)", ret);
    } else {
        LOG_INF("Published: %s", cmd);
    }
}
