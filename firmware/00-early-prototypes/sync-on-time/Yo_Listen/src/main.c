#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>

// Synchronization packet details
#define SYNC_DATA_LEN 5  // The length of the sync data packet (SYNC + counter)
static const uint8_t sync_data[] = { 'S', 'Y', 'N', 'C', 0x00 };  // Sync data to look for

static int64_t last_sync_time = 0;  // Used to store the last synchronization time

// Function to format and print time in [HH:MM:SS] format
void format_time(int64_t ms, char *formatted_time, size_t size) {
    int64_t seconds = ms / 1000;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;

    snprintf(formatted_time, size, "[%02lld:%02lld:%02lld]", hours, minutes, seconds);
}

// Callback for when a device is found during scanning
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        uint8_t ad_type = net_buf_simple_pull_u8(ad);

        // Check if this is the expected manufacturer-specific sync signal
        if (ad_type == BT_DATA_MANUFACTURER_DATA && len >= SYNC_DATA_LEN) {
            if (memcmp(ad->data, sync_data, SYNC_DATA_LEN - 1) == 0) {  // Compare SYNC data
                // Extract sync count from the last byte
                uint8_t sync_count = ad->data[SYNC_DATA_LEN - 1];

                // Synchronize the system clock (simulation by storing uptime)
                last_sync_time = k_uptime_get();  // Store the current time as sync time

                // Format the time and print it
                char formatted_time[16];
                format_time(last_sync_time, formatted_time, sizeof(formatted_time));
                printk("%s SYNC signal detected from %s, RSSI: %d, Sync Count: %d\n",
                       formatted_time, addr_str, rssi, sync_count);

                // Sync the system time (simulation)
                printk("System time synchronized to: %s\n", formatted_time);
                break;
            }
        }

        // Skip the remaining data
        net_buf_simple_pull(ad, len - 1);
    }
}

// Main function to initialize Bluetooth and start scanning
int main(void)
{
    int err;

    printk("Starting Bluetooth SYNC Listener\n");

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
