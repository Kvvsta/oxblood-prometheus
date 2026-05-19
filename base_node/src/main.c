#include "nus_client.h"
#include "json_serial.h"
#include "mobile_link.h"

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/printk.h>

int main(void) {

    int err;

    json_serial_init();
    mobile_link_init();
    //audio_out_init();
    //serial_audio_init();

    err = bt_enable(NULL);
    if (err) {
        printk("{\"type\":\"status\",\"status\":\"bt_enable_failed\","
		       "\"err\":%d}\n",
		       err);
		return 0;
    }

    json_emit_status("bluetooth", "ready");

    nus_client_init();

    if (!nus_client_start()) {
        json_emit_status("nus_client", "start failed");
        return 0; 
    }

    while (1) {
        k_sleep(K_SECONDS(1)); 
    }

    return 0;
}