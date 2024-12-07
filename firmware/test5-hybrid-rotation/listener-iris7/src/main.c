#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/kernel.h>
#include <ctype.h>
#include <string.h>

#define SYNC_DATA_LEN 5                // Length of the sync data packet (SYNC + counter)
#define MIN_SYNC_INTERVAL_MS 300000    // Minimum 5 minutes between SYNC signals
#define ROTATION_INTERVAL_MS 120000    // Rotate autonomously every 2 minutes
#define MAX_OBSERVED_DEVICES 20        // Maximum number of observed devices

static uint8_t decryption_key = 0x5A;  // Example decryption key
static char device_name[30];           // Buffer for the device name
static char observed_devices[MAX_OBSERVED_DEVICES][32];  // Observed device names
static int observed_device_count = 0;  // Number of observed devices
static int64_t last_sync_time = 0;     // Tracks the last SYNC time
static int64_t last_rotation_time = 0; // Tracks the last autonomous rotation time
static uint8_t last_processed_counter = 0xFF;  // Tracks the last processed counter

// Forward declaration for the device_found function
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad);

// Custom implementation of atoi
int custom_atoi(const char *str) {
    int result = 0;
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result;
}

// Rotate the device name
void rotate_device_name(uint8_t counter) {
    snprintf(device_name, sizeof(device_name), "Iris9_%d", counter);

    struct bt_data ad[] = {
        BT_DATA(BT_DATA_NAME_COMPLETE, device_name, strlen(device_name)),
    };

    int err = bt_le_adv_stop();
    if (err) {
        printk("Failed to stop advertising (err %d)\n", err);
        return;
    }

    err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    printk("Rotated to device name: %s (Counter: %d)\n", device_name, counter);

    // Restart scanning
    struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_PASSIVE,
        .options    = BT_LE_SCAN_OPT_NONE,
        .interval   = 0x0010,
        .window     = 0x0010,
    };

    err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        printk("Failed to restart scanning (err %d)\n", err);
    }

    last_rotation_time = k_uptime_get();
}

// Callback for detected devices
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad) {
    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        uint8_t ad_type = net_buf_simple_pull_u8(ad);

        // Handle observed device names
        if (ad_type == BT_DATA_NAME_COMPLETE && len > 6) {
            char found_device_name[32];
            strncpy(found_device_name, (char *)ad->data, len);
            found_device_name[len] = '\0';

            if (strncmp(found_device_name, "Iris", 4) == 0) {
                if (observed_device_count < MAX_OBSERVED_DEVICES) {
                    strncpy(observed_devices[observed_device_count], found_device_name, sizeof(observed_devices[0]));
                    observed_device_count++;
                    printk("Observed device: %s, RSSI: %d\n", found_device_name, rssi);
                }
            }
        }

        // Skip remaining advertising data
        net_buf_simple_pull(ad, len - 1);
    }
}

// Main function
int main(void) {
    int err;

    printk("Starting Iris9 Listener\n");

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }

    printk("Bluetooth initialized\n");

    struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_PASSIVE,
        .options    = BT_LE_SCAN_OPT_NONE,
        .interval   = 0x0010,
        .window     = 0x0010,
    };

    err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
        return err;
    }

    printk("Scanning started\n");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}

