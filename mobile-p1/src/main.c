/*
 * Oxblood-Prometheus: P1 Mobile Node
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>

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


#ifdef CONFIG_LSM6DSL_TRIGGER
static void lsm6dsl_trigger_handler(const struct device *dev,
				    const struct sensor_trigger *trig)
{
	static struct sensor_value gyro_y, gyro_z;

	sensor_sample_fetch_chan(dev, SENSOR_CHAN_GYRO_XYZ);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_Y, &gyro_y);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_Z, &gyro_z);

	// transmit if the base node has subscribed to notifications 
	if (k_sem_take(&notif_sem, K_NO_WAIT) != 0) {
		return;
	}
	k_sem_give(&notif_sem);

	struct imu_packet pkt = {
		.gy = gyro_y.val1 * 1000000 + gyro_y.val2,
		.gz = gyro_z.val1 * 1000000 + gyro_z.val2,
	};

	int err = bt_nus_send(NULL, &pkt, sizeof(pkt));
	if (err < 0 && err != -EAGAIN && err != -ENOTCONN) {
		printk("NUS send error: %d\n", err);
	}
}
#endif

int main(void)
{
	struct sensor_value odr_attr;
	const struct device *const lsm6dsl_dev = DEVICE_DT_GET_ONE(st_lsm6dsl);

	if (!device_is_ready(lsm6dsl_dev)) {
		printk("sensor: device not ready.\n");
		return 0;
	}

	odr_attr.val1 = 104;
	odr_attr.val2 = 0;

	if (sensor_attr_set(lsm6dsl_dev, SENSOR_CHAN_ACCEL_XYZ,
			    SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		printk("Cannot set sampling frequency for accelerometer.\n");
		return 0;
	}

	if (sensor_attr_set(lsm6dsl_dev, SENSOR_CHAN_GYRO_XYZ,
			    SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		printk("Cannot set sampling frequency for gyro.\n");
		return 0;
	}

#ifdef CONFIG_LSM6DSL_TRIGGER
	struct sensor_trigger trig = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ACCEL_XYZ,
	};

	if (sensor_trigger_set(lsm6dsl_dev, &trig, lsm6dsl_trigger_handler) != 0) {
		printk("Could not set sensor trigger\n");
		return 0;
	}
#endif

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
	return 0;
}
