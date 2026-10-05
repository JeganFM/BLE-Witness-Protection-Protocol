#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/kernel.h>
#include <ctype.h>

#define SYNC_DATA_LEN 5                // Length of the sync data packet (SYNC + counter)
#define MIN_SYNC_INTERVAL_MS 600000    // Minimum 10 minutes between SYNC signals
#define ROTATION_INTERVAL_MS 300000    // Rotate autonomously every 5 minutes
#define MAX_OBSERVED_DEVICES 20        // Maximum number of observed devices
#define DEBOUNCE_INTERVAL_MS 5000      // Minimum time between rotations in milliseconds

static uint8_t decryption_key = 0x5A;  // Example decryption key
static char device_name[30];           // Buffer for the device name
static char observed_devices[MAX_OBSERVED_DEVICES][32];  // Observed device names
static uint8_t observed_counters[MAX_OBSERVED_DEVICES];  // Observed counters
static int observed_device_rssi[MAX_OBSERVED_DEVICES];   // RSSI values for observed devices
static int64_t observed_device_times[MAX_OBSERVED_DEVICES]; // Detection times for each device
static int observed_device_count = 0;  // Number of observed devices
static int64_t last_sync_time = 0;     // Tracks the last SYNC time
static int64_t last_rotation_time = 0; // Tracks the last rotation time
static int64_t last_autonomous_rotation_time = 0;  // Tracks the last autonomous rotation time
static uint8_t autonomous_counter = 0; // Counter for autonomous rotation
static uint8_t last_processed_counter = 0xFF;  // Tracks the last processed counter
static bool sync_message_logged = false; // Ensures duplicate SYNC logs are minimized

// Custom implementation of atoi
int custom_atoi(const char *str) {
    int result = 0;
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result;
}

// Format time in [HH:MM:SS] format
void format_time(int64_t ms, char *formatted_time, size_t size) {
    int64_t seconds = ms / 1000;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    snprintf(formatted_time, size, "[%02lld:%02lld:%02lld]", hours, minutes, seconds);
}

// Normalize and validate device name for Iris prefixes
void normalize_device_name(char *name) {
    if (strncmp(name, "Iris7_", 6) != 0 &&
        strncmp(name, "Iris6_", 6) != 0 &&
        strncmp(name, "Iris9_", 6) != 0) {
        name[0] = '\0'; // Null-terminate if not a valid Iris name
        return;
    }

    // Validate numeric suffix
    for (int i = 6; i < strlen(name); i++) {
        if (!isdigit(name[i])) {
            name[i] = '\0'; // Null-terminate to remove invalid characters
            break;
        }
    }
}

// Check if a device is already observed
bool is_device_already_observed(const char *device_name) {
    for (int i = 0; i < observed_device_count; i++) {
        if (strcmp(observed_devices[i], device_name) == 0) {
            return true; // Device already observed
        }
    }
    return false;
}

// Rotate the device name
void rotate_device_name(uint8_t counter, const char *rotation_type) {
    snprintf(device_name, sizeof(device_name), "Iris9_%d", counter);

    struct bt_data ad[] = {
        BT_DATA(BT_DATA_NAME_COMPLETE, device_name, strlen(device_name)),
    };

    int err = bt_le_adv_stop();
    if (err && err != -EINVAL) {  // Ignore "No valid legacy adv" errors
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
    printk("%s Rotated to device name: %s (Counter: %d, %s)\n", formatted_time, device_name, counter, rotation_type);

    // Log observed devices
    if (observed_device_count > 0) {
        printk("Summary of observations from the previous rotation:\n");

        // Track unique counters for grouping
        bool reported_counters[256] = { false };  // Support counters in range 0-255

        for (int i = 0; i < observed_device_count; i++) {
            uint8_t counter = observed_counters[i];

            // Group devices by counter values
            if (!reported_counters[counter]) {
                printk("Devices with Counter %d:\n", counter);
                reported_counters[counter] = true;

                for (int j = 0; j < observed_device_count; j++) {
                    if (observed_counters[j] == counter) {
                        format_time(observed_device_times[j], formatted_time, sizeof(formatted_time));
                        printk("  Device: %s, RSSI: %d, Observed at: %s\n",
                            observed_devices[j], observed_device_rssi[j], formatted_time);
                    }
                }
            }
        }

        observed_device_count = 0;  // Clear the list after logging
    }

    last_rotation_time = k_uptime_get();
    autonomous_counter = counter;  // Update autonomous counter
    sync_message_logged = false;   // Reset SYNC log suppression
}

// Callback for detected devices
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad) {
    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        uint8_t ad_type = net_buf_simple_pull_u8(ad);

        // Handle SYNC signal
        if (ad_type == BT_DATA_MANUFACTURER_DATA && len >= SYNC_DATA_LEN &&
            memcmp(ad->data, "SYNC", 4) == 0) {
            int64_t current_time = k_uptime_get();
            uint8_t encrypted_counter = ad->data[SYNC_DATA_LEN - 1];
            uint8_t decrypted_counter = encrypted_counter ^ decryption_key;

            if (current_time - last_sync_time < MIN_SYNC_INTERVAL_MS &&
                decrypted_counter == last_processed_counter) {
                if (!sync_message_logged) {
                    char formatted_time[16];
                    format_time(last_sync_time + MIN_SYNC_INTERVAL_MS - current_time, formatted_time, sizeof(formatted_time));
                    printk("Ignoring duplicate or early SYNC signal. Next eligible SYNC in: %s\n", formatted_time);
                    sync_message_logged = true;
                }
                return;  // Avoid processing or logging the duplicate SYNC signal
            }

            char addr_str[BT_ADDR_LE_STR_LEN];
            bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
            printk("SYNC signal received from %s, RSSI: %d\nEncrypted: 0x%02X, Decrypted Counter: %d\n",
                   addr_str, rssi, encrypted_counter, decrypted_counter);

            last_sync_time = current_time;
            last_processed_counter = decrypted_counter;
            rotate_device_name(decrypted_counter, "SYNC Rotation");
            return;
        }

        // Handle observed devices
        if (ad_type == BT_DATA_NAME_COMPLETE) {
            char found_device_name[32];
            strncpy(found_device_name, (char *)ad->data, len);
            found_device_name[len] = '\0';

            normalize_device_name(found_device_name);

            if (found_device_name[0] != '\0' &&
                !is_device_already_observed(found_device_name) &&
                observed_device_count < MAX_OBSERVED_DEVICES) {

                strncpy(observed_devices[observed_device_count], found_device_name, sizeof(observed_devices[0]));
                observed_counters[observed_device_count] = custom_atoi(found_device_name + 6);
                observed_device_rssi[observed_device_count] = rssi;
                observed_device_times[observed_device_count] = k_uptime_get();
                observed_device_count++;

                // Rotate on a new peer sighting, debounced
                int64_t current_time = k_uptime_get();
                if (current_time - last_rotation_time >= DEBOUNCE_INTERVAL_MS) {
                    uint8_t next_counter = (autonomous_counter + 1) % 256;
                    rotate_device_name(next_counter, "Peer Rotation");
                }
            }
        }

        net_buf_simple_pull(ad, len - 1);
    }
}

// Main function
int main(void) {
    int err;

    printk("Starting Listener with Enhanced Observations and Rotation Logic\n");

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
        int64_t current_time = k_uptime_get();

        // Check for autonomous rotation
        if (current_time - last_autonomous_rotation_time >= ROTATION_INTERVAL_MS) {
            uint8_t next_counter = (autonomous_counter + 1) % 256;
            rotate_device_name(next_counter, "Automated Rotation");
            last_autonomous_rotation_time = current_time;
        }

        k_sleep(K_SECONDS(1));  // Sleep to reduce CPU usage
    }

    return 0;
}
