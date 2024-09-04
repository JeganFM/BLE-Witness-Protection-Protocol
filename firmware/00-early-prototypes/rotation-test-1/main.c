#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <string.h>

// Define the tests
#define TEST5 1 // Change this as needed for testing different scenarios

// Constants
#define TOTAL_NAMES 255
#define NAME_LEN 30
#define ROTATION_INTERVAL_MS (60 * 1000) // 1 minute
#define ROTATION_INTERVAL_MIN_MS (1 * 60 * 1000) // 1 minute
#define ROTATION_INTERVAL_MAX_MS (5 * 60 * 1000) // 5 minutes
#define SYNC_DATA_LEN 4 // Length of the synchronization data packet
#define RING_BUF_SIZE 10 // Ring buffer size for timestamps
#define CONVERGENCE_ADJUSTMENT_FACTOR 10 // Factor to control convergence rate

// Base device name and sync data
static char device_name[248] = "Iris7";
static uint8_t mfg_data[] = { 0x01, 0x02, 0x03, 0x04 };
static const uint8_t sync_data[] = { 'S', 'Y', 'N', 'C' };

// Array to hold 255 names for rotation
static char device_names[TOTAL_NAMES][NAME_LEN];
static int current_name_index = 0;

// Ring buffer for storing synchronization timestamps
RING_BUF_DECLARE(sync_timestamps, RING_BUF_SIZE);

// Function to populate device names
void populate_device_names(void) {
    for (int i = 0; i < TOTAL_NAMES; i++) {
        snprintf(device_names[i], NAME_LEN, "Iris7_%03d", i + 1);  // Generate names like Iris7_001, Iris7_002, etc.
    }
}

// Function to rotate the device name
void rotate_device_name(int iteration) {
    snprintf(device_name, sizeof(device_name), "Iris7_%d", iteration);

    struct bt_data ad[] = {
        BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data)),
        BT_DATA(BT_DATA_NAME_COMPLETE, device_name, strlen(device_name)),
    };

    int err = bt_le_adv_stop();
    if (err) {
        printk("Failed to stop advertising (err %d)\n", err);
        return;
    }

    printk("Rotating to device name: %s\n", device_name);

    err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    printk("Advertising with new device name started\n");
}

// Callback for scanning
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad) {
    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        uint8_t ad_type = net_buf_simple_pull_u8(ad);

        if (ad_type == BT_DATA_MANUFACTURER_DATA && len >= sizeof(sync_data)) {
            if (memcmp(ad->data, sync_data, sizeof(sync_data)) == 0) {
                int64_t sync_time = k_uptime_get();
                uint8_t sync_count = ad->data[4];

                printk("Synchronization signal detected with sync_count: %d\n", sync_count);

                // Store the sync_time in the ring buffer
                ring_buf_put(&sync_timestamps, (uint8_t *)&sync_time, sizeof(sync_time));

                break;
            }
        }

        net_buf_simple_pull(ad, len - 1);
    }
}

// Observer start function to initiate scanning
int observer_start(void) {
    struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_PASSIVE,
        .options    = BT_LE_SCAN_OPT_NONE,
        .interval   = BT_GAP_SCAN_FAST_INTERVAL,
        .window     = BT_GAP_SCAN_FAST_WINDOW,
    };

    int err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        printk("Starting scanning failed (err %d)\n", err);
        return err;
    }

    printk("Scanning started\n");
    return 0;
}

// Function to calculate average interval excluding outliers
int64_t calculate_average_interval(void) {
    uint32_t timestamp_size = sizeof(int64_t);
    int64_t timestamps[RING_BUF_SIZE];
    int count = ring_buf_get(&sync_timestamps, (uint8_t *)timestamps, RING_BUF_SIZE * timestamp_size) / timestamp_size;

    if (count < 3) {
        // Not enough data to calculate interval
        return ROTATION_INTERVAL_MS;
    }

    // Sort timestamps to find min and max easily
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (timestamps[j] > timestamps[j + 1]) {
                int64_t temp = timestamps[j];
                timestamps[j] = timestamps[j + 1];
                timestamps[j + 1] = temp;
            }
        }
    }

    // Calculate average interval excluding the slowest and fastest intervals
    int64_t total_interval = 0;
    int valid_intervals = 0;

    for (int i = 1; i < count - 1; i++) { // Exclude the first and last (min and max)
        total_interval += timestamps[i] - timestamps[i - 1];
        valid_intervals++;
    }

    return (valid_intervals > 0) ? (total_interval / valid_intervals) : ROTATION_INTERVAL_MS;
}

// Function to adjust sleep interval based on convergence logic (Test 5)
int64_t adjust_sleep_interval(int64_t current_sleep_time) {
    int64_t average_interval = calculate_average_interval();
    int64_t adjustment = (average_interval - current_sleep_time) / CONVERGENCE_ADJUSTMENT_FACTOR;

    return current_sleep_time + adjustment;
}

// Main function
int main(void) {
    int err;

    printk("Starting Bluetooth device name rotation example with synchronization\n");

    // Initialize the Bluetooth Subsystem
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return -1;
    }

    printk("Bluetooth initialized\n");

    // Populate the device names array
    populate_device_names();

    // Start scanning for synchronization signals
    observer_start();

    int64_t current_sleep_time = ROTATION_INTERVAL_MS; // Initial sleep time

    // Run tests based on defined macros
    while (1) {
        rotate_device_name(current_name_index);
        current_name_index = (current_name_index + 1) % TOTAL_NAMES;

#if defined(TEST1)
        // Never rotate
        printk("Never changing identity\n");
        break;

#elif defined(TEST2)
        printk("Waiting for %u ms before changing identity\n", ROTATION_INTERVAL_MS);
        k_msleep(ROTATION_INTERVAL_MS);  // Wait for a fixed interval

#elif defined(TEST3)
        uint32_t sleep_interval = ROTATION_INTERVAL_MIN_MS + (sys_rand32_get() % (ROTATION_INTERVAL_MAX_MS - ROTATION_INTERVAL_MIN_MS));
        printk("Waiting for %u ms before changing identity\n", sleep_interval);
        k_msleep(sleep_interval);  // Wait for a random interval

#elif defined(TEST4)
        // Calculate sleep interval based on synchronization signals
        int64_t sleep_interval = calculate_sync_based_interval();
        printk("Waiting for %lld ms before changing identity (based on sync signals)\n", sleep_interval);
        k_msleep((uint32_t)sleep_interval);

#elif defined(TEST5)
        // Converge identities based on distributed algorithm
        current_sleep_time = adjust_sleep_interval(current_sleep_time);
        printk("Waiting for %lld ms before changing identity (converging)\n", current_sleep_time);
        k_msleep((uint32_t)current_sleep_time);

#else
#  error "Unknown test"
#endif
    }

    // Put CPU to sleep to save power
    k_cpu_idle();

    return 0;  // Return 0 to indicate success
}
