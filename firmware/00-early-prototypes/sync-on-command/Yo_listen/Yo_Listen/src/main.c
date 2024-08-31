#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/kernel.h>

#define ROTATION_INTERVAL_MS (60 * 1000)  // Rotation interval in milliseconds (1 minute)
#define TOTAL_NAMES 255  // Total number of names for rotation
#define SYNC_DATA_LEN 4  // Length of the synchronization data packet
#define NAME_LEN 30

static char device_names[TOTAL_NAMES][NAME_LEN];  // Array to hold 255 names
static uint8_t sync_data[SYNC_DATA_LEN] = { 'S', 'Y', 'N', 'C' };  // Synchronization data packet

// Populate the array of device names (could be dynamically loaded or predefined)
void populate_device_names(void) {
    for (int i = 0; i < TOTAL_NAMES; i++) {
        snprintf(device_names[i], NAME_LEN, "Device_%03d", i + 1);  // Generate names like Device_001, Device_002, etc.
    }
}

// Get the current name index based on elapsed time
static int get_current_name_index(uint32_t elapsed_time_ms) {
    return (elapsed_time_ms / ROTATION_INTERVAL_MS) % TOTAL_NAMES;
}

// Callback to process received data
static bool data_cb(struct bt_data *data, void *user_data) {
    if (data->type == BT_DATA_MANUFACTURER_DATA && data->data_len == SYNC_DATA_LEN) {
        if (memcmp(data->data, sync_data, SYNC_DATA_LEN) == 0) {
            // Synchronization packet received
            uint32_t *sync_time = (uint32_t *)user_data;
            *sync_time = (uint32_t)k_uptime_get();  // Get the current system uptime
            return false;  // Stop further processing
        }
    }
    return true;  // Continue processing
}

// Handler for when a device is found during scanning
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad) {
    uint32_t sync_time = 0;
    char addr_str[BT_ADDR_LE_STR_LEN];

    // Convert the Bluetooth address to a string
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    bt_data_parse(ad, data_cb, &sync_time);

    if (sync_time > 0) {
        printk("Synchronization signal received from %s, syncing timers\n", addr_str);
        printk("System time synchronized to: %u ms\n", sync_time);

        // Rotate the device name upon synchronization signal
        rotate_identity_and_name();
    }
}

// Function to rotate identity and change the device name
void rotate_identity_and_name(void) {
    int err;

    // Stop current advertising session
    err = bt_le_adv_stop();
    if (err) {
        printk("Failed to stop advertising (err %d)\n", err);
        return;
    }

    // Delay to ensure the stack processes the stop operation
    k_msleep(100);

    // Get the current name index based on elapsed time
    uint32_t elapsed_time_ms = k_uptime_get();
    int current_name_index = get_current_name_index(elapsed_time_ms);

    // Update device name
    bt_set_name(device_names[current_name_index]);
    printk("Rotating to new device name: %s\n", device_names[current_name_index]);

    // Restart advertising with the new device name
    struct bt_data ad[] = {
        BT_DATA(BT_DATA_NAME_COMPLETE, device_names[current_name_index], strlen(device_names[current_name_index])),
    };

    err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start with new name (err %d)\n", err);
        return;
    }

    printk("Advertising with new name started\n");
}

int main(void) {
    int err;
    printk("Listener device starting\n");

    // Initialize the Bluetooth Subsystem
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return -1;  // Return non-zero value on failure
    }

    printk("Bluetooth initialized\n");

    // Populate the device names array
    populate_device_names();

    // Start scanning for synchronization signal
    struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_PASSIVE,
        .options    = 0,
        .interval   = 0x0060,  // Adjusted interval for more frequent scans
        .window     = 0x0030,  // Adjusted window to match or be less than interval
    };

    err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        printk("Start scanning failed (err %d)\n", err);
        return err;
    }

    printk("Started scanning...\n");

    // Main loop to keep the device running
    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;  // Return 0 to indicate success
}
