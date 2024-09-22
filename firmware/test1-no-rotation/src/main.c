#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <string.h>

static const char *device_name = "Iris6_Static"; // Fixed name for broadcasting

int main(void) {
    int err;
    
    printk("Starting Test 1: No Rotation (Broadcasting Only)\n");

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }

    printk("Bluetooth initialized\n");

    struct bt_data ad[] = {
        BT_DATA(BT_DATA_NAME_COMPLETE, device_name, strlen(device_name)),
    };

    err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return err;
    }

    printk("Broadcasting with fixed name: %s\n", device_name);

    // The device broadcasts indefinitely without rotating the name
    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
