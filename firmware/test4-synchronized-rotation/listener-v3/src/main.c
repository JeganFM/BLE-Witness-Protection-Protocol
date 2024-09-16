#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/kernel.h>
#include <string.h>
#include <ctype.h>

#define SYNC_DATA_LEN 5  // Length of the sync data packet (SYNC + counter)
static const uint8_t sync_data[] = { 'S', 'Y', 'N', 'C', 0x00 };  // Sync data to look for
static int current_name_index = 1;  // To track the current device name index
static char device_name[30];  // Buffer for the device name
static int64_t last_sync_time = 0;  // Store the last sync time
static uint8_t last_sync_count = 0;  // Store the last sync count
static const int64_t min_interval_between_syncs_ms = 300000;  // Minimum 60 seconds between SYNC signals
static bool ignore_message_printed = false;  // Flag to suppress repeated prints

// List of observed Iris devices and their RSSI values
#define MAX_OBSERVED_DEVICES 20
static char observed_iris_devices[MAX_OBSERVED_DEVICES][32];
static int observed_iris_rssi[MAX_OBSERVED_DEVICES];  // Array to store the corresponding RSSI values
static int64_t observed_iris_times[MAX_OBSERVED_DEVICES];  // Store the detection time for each device
static int observed_iris_count = 0;  // Number of observed devices

// Function to format time in [HH:MM:SS] format
void format_time(int64_t ms, char *formatted_time, size_t size) {
    int64_t seconds = ms / 1000;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    snprintf(formatted_time, size, "[%02lld:%02lld:%02lld]", hours, minutes, seconds);
}

// Function to normalize the device name by ensuring it follows the pattern Iris<7|9>_<number>
void normalize_device_name(char *device_name) {
    if (strncmp(device_name, "Iris7_", 6) != 0 && strncmp(device_name, "Iris9_", 6) != 0) {
        device_name[0] = '\0';  // If the prefix isn't "Iris7_" or "Iris9_", null-terminate the string
        return;
    }

    for (int i = 6; i < strlen(device_name); i++) {
        if (!isdigit(device_name[i])) {
            device_name[i] = '\0';  // Null-terminate to remove invalid characters
            break;
        }
    }
}

// Function to check if a device has already been observed
bool is_device_already_observed(const char *device_name) {
    for (int i = 0; i < observed_iris_count; i++) {
        if (strcmp(observed_iris_devices[i], device_name) == 0) {
            return true;  // Device already observed
        }
    }
    return false;
}

// Function to update and rotate the device name
void rotate_device_name(void) {
    snprintf(device_name, sizeof(device_name), "Iris6_%d", current_name_index);  // Update the device name

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

    char formatted_time[16];
    format_time(k_uptime_get(), formatted_time, sizeof(formatted_time));

    printk("%s Rotated to device name: %s\n", formatted_time, device_name);
    current_name_index++;  // Increment the name index for the next rotation
    ignore_message_printed = false;  // Reset the suppression flag after successful rotation

    if (observed_iris_count > 0) {
        for (int i = 0; i < observed_iris_count; i++) {
            format_time(observed_iris_times[i], formatted_time, sizeof(formatted_time));
            printk("%s Iris device found: %s, RSSI: %d\n", formatted_time, observed_iris_devices[i], observed_iris_rssi[i]);
        }
    } else {
        printk("No Iris devices found in this rotation.\n");
    }

    observed_iris_count = 0;
}

// Callback for when a sync signal is detected or an "Iris" device is found
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad) {
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        uint8_t ad_type = net_buf_simple_pull_u8(ad);

        // Check if this is the expected manufacturer-specific sync signal
        if (ad_type == BT_DATA_MANUFACTURER_DATA && len >= SYNC_DATA_LEN) {
            if (memcmp(ad->data, sync_data, SYNC_DATA_LEN - 1) == 0) {
                uint8_t sync_count = ad->data[SYNC_DATA_LEN - 1];
                int64_t current_time = k_uptime_get();  // Store the current time as sync time

                // Ensure at least 60 seconds have passed since the last sync and the sync count has changed
                if ((current_time - last_sync_time >= min_interval_between_syncs_ms) || (sync_count != last_sync_count)) {
                    last_sync_time = current_time;
                    last_sync_count = sync_count;

                    char formatted_time[16];
                    format_time(current_time, formatted_time, sizeof(formatted_time));

                    printk("%s SYNC signal detected from %s, RSSI: %d, Sync Count: %d\n",
                           formatted_time, addr_str, rssi, sync_count);

                    rotate_device_name();
                } else if (!ignore_message_printed) {
                    printk("Ignoring duplicate SYNC signal (debounced)\n");
                    ignore_message_printed = true;
                }
            }
        }

        // Check if the advertising data contains a name starting with "Iris"
        if (ad_type == BT_DATA_NAME_COMPLETE) {
            char found_device_name[32];
            strncpy(found_device_name, (char *)ad->data, len);
            found_device_name[len] = '\0';

            normalize_device_name(found_device_name);

            if (strncmp(found_device_name, "Iris7_", 6) == 0 || strncmp(found_device_name, "Iris9_", 6) == 0) {
                if (!is_device_already_observed(found_device_name) && observed_iris_count < MAX_OBSERVED_DEVICES) {
                    strncpy(observed_iris_devices[observed_iris_count], found_device_name, sizeof(observed_iris_devices[0]));
                    observed_iris_rssi[observed_iris_count] = rssi;
                    observed_iris_times[observed_iris_count] = k_uptime_get();
                    observed_iris_count++;
                }
            }
        }

        net_buf_simple_pull(ad, len - 1);
    }
}

// Main function to initialize Bluetooth and start scanning
int main(void) {
    int err;

    printk("Starting Bluetooth SYNC Listener with Name Rotation\n");

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }

    printk("Bluetooth initialized\n");

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

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
