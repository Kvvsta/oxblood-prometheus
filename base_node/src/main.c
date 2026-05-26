#include "nus_client.h"
#include "json_serial.h"
#include "mobile_link.h"
#include "serial_audio.h"
#include "audio_out.h"
#include "ui_display.h"

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/printk.h>

int main(void) {

    int err;

    k_sleep(K_SECONDS(2));
    
    printk("BASE BOOTED\n");

    json_serial_init();
    ui_display_init();
    mobile_link_start();
    audio_out_init();
    serial_init();

    // Enable bluetooth stack 
    err = bt_enable(NULL);
    if (err) {
        printk("{\"type\":\"status\",\"status\":\"bt_enable_failed\","
		       "\"err\":%d}\n",
		       err);
		return 0;
    }

    json_emit_status("bluetooth", "ready"); // Bluetooth ready status

    // Initialise and start base scanning/conneting to mobile nodes
    nus_client_init();
    (void)nus_client_start();
        
    while (1) {
        // m5core LVGL updated regularly 
        ui_display_tick();
        k_sleep(K_MSEC(10));
    }

    return 0;
}