#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/kernel.h>

#define SYNC_DATA_LEN 5  // Length of the sync data packet (SYNC + counter)
static const uint8_t sync_data[] = { 'S', 'Y', 'N', 'C', 0x00 };  // Sync data to look for
static int current_name_index = 1;  // To track the current device name index
static char device_name[30];  // Buffer for the device name
static int64_t last_sync_time = 0;  // Store the last sync time
static uint8_t last_sync_count = 0;  // Store the last sync count
static const int64_t debounce_interval_ms = 50000;  // Debounce interval (50 seconds)
static bool ignore_message_printed = false;  // Flag to suppress repeated prints

// Function to format time in [HH:MM:SS] format
void format_time(int64_t ms, char *formatted_time, size_t size) {
    int64_t seconds = ms / 1000;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    snprintf(formatted_time, size, "[%02lld:%02lld:%02lld]", hours, minutes, seconds);
}

// Function to update and rotate the device name
void rotate_device_name(void) {
    snprintf(device_name, sizeof(device_name), "Iris9_%d", current_name_index);  // Update the device name

    // Create advertising data with the updated device name
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

    printk("Rotated to device name: %s\n", device_name);
    current_name_index++;  // Increment the name index for the next rotation
    ignore_message_printed = false;  // Reset the suppression flag after successful rotation
}

// Callback for when a sync signal is detected
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad) {
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        uint8_t ad_type = net_buf_simple_pull_u8(ad);

        // Check if this is the expected manufacturer-specific sync signal
        if (ad_type == BT_DATA_MANUFACTURER_DATA && len >= SYNC_DATA_LEN) {
            if (memcmp(ad->data, sync_data, SYNC_DATA_LEN - 1) == 0) {  // Compare SYNC data
                uint8_t sync_count = ad->data[SYNC_DATA_LEN - 1];
                int64_t current_time = k_uptime_get();  // Store the current time as sync time

                // Check if enough time has passed or the sync count has changed
                if ((current_time - last_sync_time >= debounce_interval_ms) || (sync_count != last_sync_count)) {
                    last_sync_time = current_time;  // Update the last sync time
                    last_sync_count = sync_count;  // Update the last sync count

                    // Format the time and print it
                    char formatted_time[16];
                    format_time(current_time, formatted_time, sizeof(formatted_time));
                    printk("%s SYNC signal detected from %s, RSSI: %d, Sync Count: %d\n",
                           formatted_time, addr_str, rssi, sync_count);

                    // Rotate the device name based on the sync signal
                    rotate_device_name();
                } else if (!ignore_message_printed) {
                    printk("Ignoring duplicate SYNC signal (debounced)\n");
                    ignore_message_printed = true;  // Only print this message once per debounce interval
                }
                break;
            }
        }

        // Skip the remaining data
        net_buf_simple_pull(ad, len - 1);
    }
}

// Main function to initialize Bluetooth and start scanning
int main(void) {
    int err;

    printk("Starting Bluetooth SYNC Listener with Name Rotation\n");

    // Initialize the Bluetooth Subsystem
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }

    printk("Bluetooth initialized\n");

    // Start scanning for nearby devices
    struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_PASSIVE,
        .options    = BT_LE_SCAN_OPT_NONE,
        .interval   = 0x0010,  // Scan interval 10 ms
        .window     = 0x0010,  // Scan window 10 ms
    };

    err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
        return err;
    }

    printk("Scanning started\n");

    // Keep the application running
    while (1) {
        k_sleep(K_SECONDS(1));  // Sleep for 1 second to prevent high CPU usage
    }

    return 0;
}
