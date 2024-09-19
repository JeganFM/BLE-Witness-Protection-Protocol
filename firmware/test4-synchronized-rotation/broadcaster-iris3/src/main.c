#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/kernel.h>

#define SYNC_INTERVAL_MS 60000  // Broadcast synchronization signal every 60 seconds

static uint8_t sync_data[] = { 'S', 'Y', 'N', 'C', 0x00 };  // Synchronization data packet with counter

void beacon_broadcast_sync_signal(void) {
    int err;

    // Increment the synchronization counter and wrap around at 255
    sync_data[4] = (sync_data[4] + 1) % UINT8_MAX;

    // Stop current advertising session
    err = bt_le_adv_stop();
    if (err) {
        printk("Failed to stop advertising (err %d)\n", err);
        return;
    }

    // Delay to ensure the stack processes the stop operation
    k_msleep(100);

    // Start advertising with synchronization data
    struct bt_data ad[] = {
        BT_DATA(BT_DATA_MANUFACTURER_DATA, sync_data, sizeof(sync_data)),
    };

    err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    printk("Broadcasting synchronization signal with counter: %d\n", sync_data[4]);
}

int main(void) {
    int err;
    printk("Test 4 Broadcaster (Iris3) starting\n");

    // Initialize the Bluetooth Subsystem
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return -1;
    }

    printk("Bluetooth initialized\n");

    while (1) {
        beacon_broadcast_sync_signal();  // Broadcast the synchronization signal
        k_msleep(SYNC_INTERVAL_MS);  // Wait before broadcasting the signal again
    }

    return 0;
}
