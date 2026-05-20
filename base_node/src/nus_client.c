#include "nus_client.h"
#include "mobile_link.h"
#include "json_serial.h"
#include "imu_packet.h"

#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/att.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#define NUM_MOBILE_NODES 2


static const char* const mobile_names[NUM_MOBILE_NODES] = {
    "PROMETHEUS-P1",
    "PROMETHEUS-P2",
};

static const struct bt_le_conn_param fast_conn_params = {
	.interval_min = 16,
	.interval_max = 16,
	.latency = 0,
	.timeout = 400,
};

/*
 * Nordic UART Service UUIDs.

 */
#define BT_UUID_NUS_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x6E400001, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E)

#define BT_UUID_NUS_TX_VAL \
    BT_UUID_128_ENCODE(0x6E400003, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E)

#define BT_UUID_NUS_SERVICE BT_UUID_DECLARE_128(BT_UUID_NUS_SERVICE_VAL)
#define BT_UUID_NUS_TX      BT_UUID_DECLARE_128(BT_UUID_NUS_TX_VAL)

/* 
 * Per-player BLE client state
 */
struct mobile_client {
    const char* name;

    struct bt_conn* conn;
    bool connecting;
    bool subscribed;

    struct bt_uuid_128 uuid;
    const struct bt_uuid* ccc_uuid;

    struct bt_gatt_discover_params discover_params;
    struct bt_gatt_subscribe_params subscribe_params;
    struct bt_gatt_exchange_params mtu_exchange_params;

    uint16_t nus_service_end_handle; 
}; 

static struct mobile_client clients[NUM_MOBILE_NODES]; 

static void start_scan(void);

// Finds which player owns a BLE connection
static struct mobile_client* find_client_by_conn(struct bt_conn* conn) {
    for (int i = 0; i < NUM_MOBILE_NODES; i++) {
        if (clients[i].conn == conn) {
            return &clients[i];
        }
    }

    return NULL; 
}

/* 
 * Returns true if at least one target player is not currently connected
 * or connecting.
 *
 * Used to decide whether scanning still needs to continue. 
 */
static bool any_client_available(void) {
    for (int i = 0; i < NUM_MOBILE_NODES; i++) {
        if (clients[i].conn == NULL && !clients[i].connecting) {
            return true; 
        }
    }
    return false; 
}

/* 
 * Returns true if at least one target player is not currently connected
 * or connecting.
 * 
 * Used to avoid unnecessary scanning once both players are handled
 */
static bool all_clients_connected(void) {
    for (int i = 0; i < NUM_MOBILE_NODES; i++) {
        if (clients[i].conn == NULL && !clients[i].connecting) {
            return false; 
        }
    }

    return true; 
}

/*
 * Helper context used while parsing advertisement data to match the mobile
 * node by advertised name. 
 * 
 * Stores which target player name was matched
 *  0 -> P1
 *  1 -> P2
 *  -1 -> no match
 */
struct name_match_ctx {
    int index;
};

/*
 * Parse advertisement data looking for the device name and compare it to the
 * configured mobile-node name.
 */
static bool ad_parse_name_cb(struct bt_data *data, void *user_data)
{
    struct name_match_ctx *ctx = user_data;

    if (!ctx) {
        return false;
    }

    if (data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED) {
        char name_buf[32];
        size_t copy_len = data->data_len;

        if (copy_len >= sizeof(name_buf)) {
            copy_len = sizeof(name_buf) - 1;
        }

        memcpy(name_buf, data->data, copy_len);
        name_buf[copy_len] = '\0';

        for (int i = 0; i < NUM_MOBILE_NODES; i++) {
            if (strcmp(name_buf, clients[i].name) == 0) {
                ctx->index = i;
                return false;
            }
        }
        
    }

    return true;
}

/*
 * Notification callback.
 *
 * Once subscribed to the mobile node's NUS TX characteristic, all incoming
 * payload bytes arrive here.
 */
static uint8_t notify_func(struct bt_conn *conn,
                           struct bt_gatt_subscribe_params *params,
                           const void *data, uint16_t length)
{
    if (!data) {
        json_emit_status("nus_notify", "unsubscribed");
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }

    struct mobile_client* client = find_client_by_conn(conn);

    if (!client) {
        json_emit_status("nus_notify_error", "unknown connection");
        return BT_GATT_ITER_CONTINUE;
    }

    if (length != sizeof(struct imu_packet)) {
        json_emit_status("imu_packet_error", "unexpected payload length");
        return BT_GATT_ITER_CONTINUE;
    }

    const struct imu_packet* pkt = (const struct imu_packet*)data; 

    if (!mobile_link_process_imu_packet(client->name, pkt, length)) {
        json_emit_status("imu_packet_error", "processing failed");
    }

    return BT_GATT_ITER_CONTINUE;
}

/*
 * GATT discovery callback.
 *
 * Stages:
 *   1. discover NUS primary service
 *   2. discover NUS TX characteristic
 *   3. discover CCC descriptor for the TX characteristic
 *   4. subscribe to notifications
 */
static uint8_t discover_func(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    int err;
    struct mobile_client* client = find_client_by_conn(conn);

    if (!client) {
        return BT_GATT_ITER_STOP;
    }

    if (!attr) {
        json_emit_status("nus_discovery", "complete");
        (void)memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }

    if (!bt_uuid_cmp(client->discover_params.uuid, BT_UUID_NUS_SERVICE)) {
        const struct bt_gatt_service_val *service = attr->user_data;

        client->nus_service_end_handle = service->end_handle;

        memcpy(&client->uuid, BT_UUID_NUS_TX, sizeof(client->uuid));
        client->discover_params.uuid = &client->uuid.uuid;
        client->discover_params.start_handle = attr->handle + 1U;
        client->discover_params.end_handle = service->end_handle;
        client->discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &client->discover_params);
        if (err) {
            json_emit_status("nus_discovery_error", "TX characteristic discovery failed");
        }

        return BT_GATT_ITER_STOP;
    }

    if (!bt_uuid_cmp(client->discover_params.uuid, BT_UUID_NUS_TX)) {
        const struct bt_gatt_chrc *chrc = attr->user_data;

        client->discover_params.uuid = client->ccc_uuid;
        client->discover_params.start_handle = chrc->value_handle + 1U;
        client->discover_params.end_handle = client->nus_service_end_handle;
        client->discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;

        client->subscribe_params.value_handle = chrc->value_handle;

        err = bt_gatt_discover(conn, &client->discover_params);
        if (err) {
            json_emit_status("nus_discovery_error", "CCC discovery failed");
        }

        return BT_GATT_ITER_STOP;
    }

    if (!bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CCC)) {
        client->subscribe_params.notify = notify_func;
        client->subscribe_params.value = BT_GATT_CCC_NOTIFY;
        client->subscribe_params.ccc_handle = attr->handle;

        err = bt_gatt_subscribe(conn, &client->subscribe_params);
        if (err && err != -EALREADY) {
            json_emit_status("nus_subscribe_error", "failed to subscribe");
        } else {
            client->subscribed = true; 
            json_emit_status("nus_subscribe", "notifications enabled");

            if (any_client_available()) {
                start_scan();
            }
        }

        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_STOP;
}

/*
 * MTU exchange callback.
 *
 * This is optional but useful when transporting application data over GATT.
 */
static void mtu_exchange_cb(struct bt_conn *conn, uint8_t err,
                            struct bt_gatt_exchange_params *params)
{
    ARG_UNUSED(params);

    struct mobile_client* client = find_client_by_conn(conn);

    printk("%s: MTU exchange %s (%u)\n",
           client ? client->name : "unknown",
           err == 0U ? "successful" : "failed",
           bt_gatt_get_mtu(conn));
}

/*
 * Request an MTU exchange after connection.
 */
static int mtu_exchange(struct mobile_client *client)
{
    int err;

    if (!client || !client->conn) {
        return -EINVAL;
    }

    //printk("%s: Current MTU = %u\n", __func__, bt_gatt_get_mtu(conn));
    
    
    err = bt_gatt_exchange_mtu(client->conn, &client->mtu_exchange_params);
    if (err) {
        printk("%s: MTU exchange failed (err %d)\n", __func__, err);
    }

    return err;
}

// Starts NUS service discovery for specific connected player
static void begin_discovery(struct mobile_client* client) {
    if (!client || !client->conn) {
        return;
    }

    memcpy(&client->uuid, BT_UUID_NUS_SERVICE, sizeof(client->uuid));
    client->discover_params.uuid = &client->uuid.uuid;
    client->discover_params.func = discover_func;
    client->discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    client->discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    client->discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    int err = bt_gatt_discover(client->conn, &client->discover_params);
    if (err) {
        printk("Discover failed (err %d)\n", err);
    }
}

/*
 * Scan callback used while searching for the mobile node.
 *
 * This process:
 *   - ignores if already connected
 *   - only considers connectable advertisements
 *   - identifies the target device
 *   - stops scanning
 *   - initiates a connection
 */
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    int err;

    if (!any_client_available()) {
        return;
    }

    if (type != BT_GAP_ADV_TYPE_ADV_IND &&
        type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return;
    }

    struct name_match_ctx ctx = {
        .index = -1
    };

    bt_data_parse(ad, ad_parse_name_cb, &ctx);

    if (ctx.index < 0) {
        return;
    }

    struct mobile_client* client = &clients[ctx.index];

    if (client->conn || client->connecting) {
        return;
    }

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    printk("Mobile node found: %s (RSSI %d)\n", client->name, rssi);

    /*if (bt_le_scan_stop()) {
        return;
    }*/
    (void)bt_le_scan_stop(); 

    client->connecting = true; 

    err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
                            BT_LE_CONN_PARAM_DEFAULT, &client->conn);
    if (err) {
        client->connecting = false;
        client->conn = NULL;
        printk("Create conn to %s failed (%d)\n", addr_str, err);
        start_scan();
    } else {
        json_emit_status("nus_connect", "connection initiated");
    }
}

/*
 * Start passive scanning for the mobile node.
 */
static void start_scan(void)
{
    int err;

    if (all_clients_connected()) {
        json_emit_status("nus_scan", "all target mobiles handled");
        return; 
    }

    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
    if (err && err != -EALREADY) {
        printk("Scanning failed to start (err %d)\n", err);
        return;
    }

    json_emit_status("nus_scan", "scanning for PROMETHEUS-P1/P2");
}

/*
 * Connection callback.
 *
 * On successful connection:
 *   - print and emit status
 *   - begin NUS service discovery
 *
 * On failure:
 *   - release connection reference
 *   - restart scanning
 */
static void connected(struct bt_conn *conn, uint8_t err) {
    //char addr[BT_ADDR_LE_STR_LEN];

    //bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    struct mobile_client* client = find_client_by_conn(conn); 

    if (!client) {
        return; 
    }

    client->connecting = false;

    if (err) {
        printk("Failed to connect to %s %u %s\n",
               client->name, err, bt_hci_err_to_str(err));

        bt_conn_unref(client->conn);
        client->conn = NULL;
        client->subscribed = false; 

        start_scan();
        return;
    }

    printk("Connected: %s\n", client->name);
    json_emit_status("nus_connect", "connected to mobile node");

    int parameter_err = bt_conn_le_param_update(conn, &fast_conn_params); 
    if (parameter_err) {
        printk("Connection parameter update failed: %d\n", parameter_err);
    }

    (void)mtu_exchange(client);
    begin_discovery(client); 
}

/*
 * Disconnection callback.
 *
 * When the mobile node disconnects, release the connection reference and
 * restart scanning so the base can reconnect automatically.
 */
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    struct mobile_client* client = find_client_by_conn(conn);

    if (!client) {
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Disconnected: %s, reason 0x%02x %s\n",
           addr, reason, bt_hci_err_to_str(reason));

    bt_conn_unref(client->conn);
    client->conn = NULL;
    client->connecting = false;
    client->subscribed = false;
    client->nus_service_end_handle = 0; 

    memset(&client->discover_params, 0, sizeof(client->discover_params));
    memset(&client->subscribe_params, 0, sizeof(client->subscribe_params));


    json_emit_status("nus_connect", "disconnected from mobile node");
    start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/*
 * Initialise local client state.
 */
void nus_client_init(void)
{

    for (int i = 0; i < NUM_MOBILE_NODES; i++) {
        clients[i].name = mobile_names[i];
        clients[i].conn = NULL;
        clients[i].connecting = false; 
        clients[i].subscribed = false; 
        clients[i].ccc_uuid = BT_UUID_GATT_CCC; 
        clients[i].nus_service_end_handle = 0;

        memset(&clients[i].uuid, 0, sizeof(clients[i].uuid));
        memset(&clients[i].discover_params, 0, sizeof(clients[i].discover_params));
        memset(&clients[i].subscribe_params, 0, sizeof(clients[i].subscribe_params));
        memset(&clients[i].mtu_exchange_params, 0, sizeof(clients[i].mtu_exchange_params));

        clients[i].mtu_exchange_params.func = mtu_exchange_cb;
    }
    
}

/*
 * Start the BLE central/client workflow.
 */
bool nus_client_start(void)
{
    start_scan();
    return true;
}