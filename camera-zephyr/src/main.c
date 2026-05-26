/*
 * Oxblood-Prometheus: Colour Detection
 * references:
 * 		zephyr/samples/drivers/video/capture 
 * 		zephyr/drivers/video/ov2640.c
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include "colour_detect.h"
#include "wifi_mqtt.h"

#define STABLE_FRAMES_ON 4 // 4 consecutive frames to confirm a colour
#define STABLE_FRAMES_OFF 8 // 8 consecutive frames to clear a colour detection.

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

//thread for Wifi connection & MQTT event loop 
static void mqtt_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	wifi_mqtt_run();
}
// priority 2 -> runs before main so Wifi connects before camera starts
K_THREAD_DEFINE(mqtt_tid, 8192, mqtt_thread_fn, NULL, NULL, NULL, 2, 0, 0);

static int app_setup_video_format(const struct device *const video_dev, struct video_format *const fmt)
{
	int ret;

	ret = video_get_format(video_dev, fmt);
	if (ret < 0) {
		LOG_ERR("video_get_format failed (%d)", ret);
		return ret;
	}

	fmt->width = CONFIG_VIDEO_FRAME_WIDTH;
	fmt->height = CONFIG_VIDEO_FRAME_HEIGHT;
	fmt->pixelformat = VIDEO_PIX_FMT_RGB565;
	fmt->pitch = fmt->width * 2;
	fmt->size = fmt->pitch * fmt->height;

	ret = video_set_format(video_dev, fmt);
	if (ret < 0) {
		LOG_ERR("video_set_format failed (%d)", ret);
		return ret;
	}

	LOG_INF("Camera : %s  %ux%u RGB565",
		video_dev->name, fmt->width, fmt->height);
	return 0;
}

static int app_setup_video_buffers(const struct device *const video_dev, const struct video_caps *const caps,
	const struct video_format *const fmt)
{
	if (caps->min_vbuf_count > CONFIG_VIDEO_BUFFER_POOL_NUM_MAX ||
	    fmt->size > CONFIG_VIDEO_BUFFER_POOL_SZ_MAX) {
		LOG_ERR("Not enough buffers or pool memory");
		return -EINVAL;
	}

	for (int i = 0; i < CONFIG_VIDEO_BUFFER_POOL_NUM_MAX; i++) {
		struct video_buffer *vbuf =
			video_buffer_aligned_alloc(fmt->size,
						   CONFIG_VIDEO_BUFFER_POOL_ALIGN,
						   K_NO_WAIT);
		if (!vbuf) {
			LOG_ERR("Unable to alloc video buffer %d", i);
			return -ENOMEM;
		}

		vbuf->type = VIDEO_BUF_TYPE_OUTPUT;

		int ret = video_enqueue(video_dev, vbuf);
		if (ret < 0) {
			LOG_ERR("Failed to enqueue video buffer %d (%d)", i, ret);
			return ret;
		}
	}
	return 0;
}

static int app_setup_display(const struct device *const display_dev, const struct video_format *const fmt)
{
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display device not ready");
		return -ENODEV;
	}

	int ret = display_set_pixel_format(display_dev, PIXEL_FORMAT_RGB_565);
	if (ret < 0) {
		LOG_WRN("display_set_pixel_format failed (%d) — continuing", ret);
	}

	display_blanking_off(display_dev);
	LOG_INF("Display: %s  %dx%d",
		display_dev->name, fmt->width, fmt->height);
	return 0;
}

int main(void)
{
	const struct device *const video_dev =
		DEVICE_DT_GET(DT_CHOSEN(zephyr_camera));
	const struct device *const display_dev =
		DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	struct video_format fmt = { .type = VIDEO_BUF_TYPE_OUTPUT };
	struct video_caps  caps = { .type = VIDEO_BUF_TYPE_OUTPUT };
	struct video_buffer *vbuf = NULL;
	int ret;

	/* Wait for Wifi & MQTT before touching camera 
	 * block here until the mqtt thread has fully connected then start
	 * the camera once Wifi is stable. */
	LOG_INF("Waiting for WiFi & MQTT...");
	wifi_mqtt_wait_ready();
	LOG_INF("WiFi ready - starting camera");

	// setup camera 
	if (!device_is_ready(video_dev)) {
		LOG_ERR("Camera device not ready");
		return -ENODEV;
	}

	ret = app_setup_video_format(video_dev, &fmt);
	if (ret < 0) goto err;

	ret = video_get_caps(video_dev, &caps);
	if (ret < 0) {
		LOG_ERR("video_get_caps failed (%d)", ret);
		goto err;
	}

	ret = app_setup_video_buffers(video_dev, &caps, &fmt);
	if (ret < 0) goto err;

	// setup display
	ret = app_setup_display(display_dev, &fmt);
	if (ret < 0) goto err;

	// start filming
	ret = video_stream_start(video_dev, VIDEO_BUF_TYPE_OUTPUT);
	if (ret < 0) {
		LOG_ERR("video_stream_start failed (%d)", ret);
		goto err;
	}

	LOG_INF("Running - hold a coloured card over the lens");

	colour_t reported_colour = COLOUR_NONE;
	colour_t candidate = COLOUR_NONE;
	int stable_count = 0;
	unsigned int frame = 0;

	const struct display_buffer_descriptor disp_desc = {
		.buf_size = fmt.size,
		.width = fmt.width,
		.height = fmt.height,
		.pitch = fmt.width,
	};

	// filming loop
	while (true) {
		ret = video_dequeue(video_dev, &vbuf, K_FOREVER);
		if (ret < 0) {
			LOG_ERR("video_dequeue failed (%d)", ret);
			goto err;
		}

		frame++;

		// display camera image
		display_write(display_dev, 0, 0, &disp_desc, vbuf->buffer);

		//colour detection
		struct colour_result res;

		colour_detect_verbose((const uint8_t *)vbuf->buffer, (uint32_t)fmt.width, (uint32_t)fmt.height, &res);

		// return buffer to camera
		ret = video_enqueue(video_dev, vbuf);
		if (ret < 0) {
			LOG_ERR("video_enqueue failed (%d)", ret);
			goto err;
		}

		// check the number of frames the currrent candiate has been seen for
		if (res.colour == candidate) {
			stable_count++;
		} else {
			candidate = res.colour;
			stable_count = 1;
		}

		int threshold = (candidate == COLOUR_NONE) ? STABLE_FRAMES_OFF : STABLE_FRAMES_ON;

		if (stable_count >= threshold &&
		    candidate != reported_colour) {
			float pct = (res.total > 0) ? (float)res.votes[candidate] / (float)res.total * 100.0f : 0.0f;

			LOG_INF("[%4u] -> %-8s  H:%5.1f S:%.2f V:%.2f  (%2.0f%%)",
				frame, colour_name(candidate),
				(double)res.avg_h,
				(double)res.avg_s,
				(double)res.avg_v,
				(double)pct);

			reported_colour = candidate;
			wifi_mqtt_publish_colour(reported_colour);
		}
	}

err:
	LOG_ERR("Aborting");
	return ret;
}
