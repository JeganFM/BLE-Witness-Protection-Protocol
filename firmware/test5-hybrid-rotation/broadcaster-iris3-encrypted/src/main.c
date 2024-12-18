#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/kernel.h>

#define SYNC_INTERVAL_MS 600000 // Broadcast synchronization signal every 5 minutes

static uint8_t sync_data[] = { 'S', 'Y', 'N', 'C', 0x00 }; // SYNC packet with counter
static uint8_t encryption_key = 0x5A; // Example encryption key (IRK)

// Encrypt the synchronization message
uint8_t encrypt_message(uint8_t counter, uint8_t key) {
    return counter ^ key; // Simple XOR encryption
}

// Decrypt the synchronization message (for debugging)
uint8_t decrypt_message(uint8_t encrypted, uint8_t key) {
    return encrypted ^ key; // XOR decryption (symmetric to encryption)
}

// Format the timestamp
void format_time(int64_t ms, char *formatted_time, size_t size) {
    int64_t seconds = ms / 1000;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    snprintf(formatted_time, size, "[%02lld:%02lld:%02lld]", hours, minutes, seconds);
}

// Broadcast the synchronization signal
void beacon_broadcast_sync_signal(void) {
    int err;

    // Increment the synchronization counter and wrap around at 255
    uint8_t counter = (sync_data[4] + 1) % UINT8_MAX;
    sync_data[4] = encrypt_message(counter, encryption_key);

    // Generate a timestamp
    char formatted_time[16];
    format_time(k_uptime_get(), formatted_time, sizeof(formatted_time));

    // Log the encrypted and decrypted messages with timestamp
    printk("%s Counter before encryption: %d\n", formatted_time, counter);
    printk("%s Encrypted message: 0x%02X\n", formatted_time, sync_data[4]);
    printk("%s Decrypted message: %d\n", formatted_time, decrypt_message(sync_data[4], encryption_key));

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

    printk("%s Broadcasting synchronization signal with encrypted counter: 0x%02X\n", formatted_time, sync_data[4]);
}

int main(void) {
    int err;

    printk("Iris_3 Broadcaster starting\n");

    // Initialize the Bluetooth Subsystem
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return -1;
    }

    printk("Bluetooth initialized\n");

    while (1) {
        // Broadcast the synchronization signal
        beacon_broadcast_sync_signal();

        // Wait 10 minutes before the next broadcast
        k_msleep(SYNC_INTERVAL_MS);
    }

    return 0;
}
