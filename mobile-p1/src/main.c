/*
 * Oxblood-Prometheus: P1 Mobile Node
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/bluetooth/conn.h>

#define POLL_INTERVAL_MS 20  

/*
 * Packed IMU payload sent over BLE NUS.
 * Values are in micro-rad/s (val1 * 1000000 + val2).
 * Base node divides by 1000000 to recover rad/s.
 */
struct imu_packet {
	int32_t gy;
	int32_t gz;
} __packed;

static K_SEM_DEFINE(notif_sem, 0, 1);

static void notif_enabled(bool enabled, void *ctx)
{
	ARG_UNUSED(ctx);
	printk("NUS notifications %s\n", enabled ? "enabled" : "disabled");
	if (enabled) {
		k_sem_give(&notif_sem);
	} else {
		k_sem_reset(&notif_sem);
	}
}

static void received(struct bt_conn *conn, const void *data,
		     uint16_t len, void *ctx)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(data);
	ARG_UNUSED(len);
	ARG_UNUSED(ctx);
}

static struct bt_nus_cb nus_cb = {
	.notif_enabled = notif_enabled,
	.received = received,
};

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_NAME_COMPLETE, "PROMETHEUS-P1", sizeof("PROMETHEUS-P1") - 1),
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	ARG_UNUSED(conn);

	if (err) {
		printk("Connection failed: %u\n", err);
		return;
	}

	printk("Connected to base\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);

	printk("Disconnected from base: %u\n", reason);

	/* Notifications are no longer valid after disconnect. */
	k_sem_reset(&notif_sem);

	/* Restart advertising so the base can reconnect after reboot/reset. */
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
				  ad, ARRAY_SIZE(ad),
				  NULL, 0);

	if (err && err != -EALREADY) {
		printk("Advertising restart failed: %d\n", err);
	} else {
		printk("Advertising restarted\n");
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

int main(void)
{
	const struct device *const lsm6dsl_dev = DEVICE_DT_GET_ONE(st_lsm6dsl);

	if (!device_is_ready(lsm6dsl_dev)) {
		printk("sensor: device not ready.\n");
		return 0;
	}
	
	struct sensor_value odr = { .val1 = 104, .val2 = 0 };

	if (sensor_attr_set(lsm6dsl_dev, SENSOR_CHAN_GYRO_XYZ,
			    SENSOR_ATTR_SAMPLING_FREQUENCY, &odr) < 0) {
		printk("Cannot set gyro sampling frequency.\n");
		return 0;
	}

	int err = bt_nus_cb_register(&nus_cb, NULL);
	if (err) {
		printk("NUS cb register failed: %d\n", err);
		return 0;
	}

	err = bt_enable(NULL);
	if (err) {
		printk("BT enable failed: %d\n", err);
		return 0;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
			      NULL, 0);
	if (err) {
		printk("Advertising failed: %d\n", err);
		return 0;
	}

	printk("P1 ready: Advertising as PROMETHEUS-P1\n");

	while (1) {
		k_msleep(POLL_INTERVAL_MS);

		/* Only send while the base node has notifications enabled. */
		if (k_sem_take(&notif_sem, K_NO_WAIT) != 0) {
			continue;
		}
		k_sem_give(&notif_sem);

		struct sensor_value gyro_y, gyro_z;

		sensor_sample_fetch(lsm6dsl_dev);
		sensor_channel_get(lsm6dsl_dev, SENSOR_CHAN_GYRO_Y, &gyro_y);
		sensor_channel_get(lsm6dsl_dev, SENSOR_CHAN_GYRO_Z, &gyro_z);

		struct imu_packet pkt = {
			.gy = gyro_y.val1 * 1000000 + gyro_y.val2,
			.gz = gyro_z.val1 * 1000000 + gyro_z.val2,
		};

		err = bt_nus_send(NULL, (const uint8_t *)&pkt, sizeof(pkt));
		if (err == -ENOTCONN) {
			printk("NUS disconnected, restarting advertising\n");

			k_sem_reset(&notif_sem); // Reset notification semaphore

			int adv_err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
							ad, ARRAY_SIZE(ad),
							NULL, 0);

			if (adv_err && adv_err != -EALREADY) {
				printk("Advertising restart failed: %d\n", adv_err);
			}
		} else if (err < 0 && err != -EAGAIN) {
			printk("NUS send error: %d\n", err);
		}
	}

	return 0;
}
