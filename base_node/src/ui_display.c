#include "ui_display.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <lvgl.h>
#include <stdio.h>
#include <math.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>

#define LCD_PWR_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec lcd_pwr =
	GPIO_DT_SPEC_GET(LCD_PWR_NODE, gpios);

static lv_obj_t *label_title;
static lv_obj_t *label_p1;
static lv_obj_t *label_p2;
static lv_obj_t *label_audio;
static lv_obj_t *label_p1_score;
static lv_obj_t *label_p2_score;
static lv_obj_t *label_high_score;
static lv_obj_t *label_gesture;

void ui_display_init(void) {

    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display)) {
        printk("DISPLAY NOT READY\n");
        return;
    }

    printk("DISPLAY READY\n");
    display_blanking_off(display);

    lv_obj_t* screen = lv_scr_act();

    // Background 
	lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    // Title label 
	label_title = lv_label_create(screen);
	lv_label_set_text(label_title, "OXBLOOD PROMETHEUS");
	lv_obj_set_style_text_color(label_title, lv_color_white(), LV_PART_MAIN);
	lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 8);

    // Gyroscope labels
	label_p1 = lv_label_create(screen);
	lv_label_set_text(label_p1, "P1 gy: -- gz: --");
	lv_obj_set_style_text_color(label_p1, lv_color_white(), LV_PART_MAIN);
	lv_obj_align(label_p1, LV_ALIGN_TOP_LEFT, 10, 45);

	label_p2 = lv_label_create(screen);
	lv_label_set_text(label_p2, "P2 gy: -- gz: --");
	lv_obj_set_style_text_color(label_p2, lv_color_white(), LV_PART_MAIN);
	lv_obj_align(label_p2, LV_ALIGN_TOP_LEFT, 10, 80);

    // Gesture label
    label_gesture = lv_label_create(screen);
    lv_label_set_text(label_gesture, "Gesture: NONE");
    lv_obj_set_style_text_color(label_gesture, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_gesture, LV_ALIGN_TOP_LEFT, 10, 120);

    // Audio labels
	label_audio = lv_label_create(screen);
	lv_label_set_text(label_audio, "Audio: ready");
	lv_obj_set_style_text_color(label_audio, lv_color_white(), LV_PART_MAIN);
	lv_obj_align(label_audio, LV_ALIGN_TOP_LEFT, 10, 155);

    // Score labels
	label_p1_score = lv_label_create(screen);
    lv_label_set_text(label_p1_score, "P1 Score: 0");
    lv_obj_set_style_text_color(label_p1_score, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_p1_score, LV_ALIGN_TOP_LEFT, 175, 45);

    label_p2_score = lv_label_create(screen);
    lv_label_set_text(label_p2_score, "P2 Score: 0");
    lv_obj_set_style_text_color(label_p2_score, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_p2_score, LV_ALIGN_TOP_LEFT, 175, 80);

    label_high_score = lv_label_create(screen);
    lv_label_set_text(label_high_score, "High Score: 0");
    lv_obj_set_style_text_color(label_high_score, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_high_score, LV_ALIGN_TOP_LEFT, 175, 115);

	lv_timer_handler();
}

void ui_display_update_imu(int player, float gy, float gz) {
	char buf[64];

	snprintf(buf, sizeof(buf), "P%d gy: %5.1f gz: %5.1f", player, 
                (double)gy, (double)gz);

	if (player == 1 && label_p1) {
		lv_label_set_text(label_p1, buf);
	} else if (player == 2 && label_p2) {
		lv_label_set_text(label_p2, buf);
	}
}

void ui_display_update_score(int p1_score, int p2_score, int high_score) {
	char buf[64];

	snprintf(buf, sizeof(buf), "P1 Score: %d", p1_score);

	if (label_p1_score) {
		lv_label_set_text(label_p1_score, buf);
	}

	snprintf(buf, sizeof(buf), "P2 Score: %d", p2_score);

	if (label_p2_score) {
		lv_label_set_text(label_p2_score, buf);
	}

	snprintf(buf, sizeof(buf), "High Score: %d", high_score);

	if (label_high_score) {
		lv_label_set_text(label_high_score, buf);
	}
}

void ui_display_update_audio(const char *event) {
	char buf[64];

	snprintf(buf, sizeof(buf), "Audio: %s", event ? event : "unknown");

	if (label_audio) {
		lv_label_set_text(label_audio, buf);
	}
}

void ui_display_update_gesture(const char *gesture) {
    char buf[64];

    snprintf(buf,
             sizeof(buf),
             "Gesture: %s",
             gesture ? gesture : "NONE");

    if (label_gesture) {
        lv_label_set_text(label_gesture, buf);
    }
}

void ui_display_tick(void) {
	lv_timer_handler();
}