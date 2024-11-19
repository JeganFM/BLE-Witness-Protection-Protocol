#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <string.h>
#include <stdio.h>

#define ROTATION_INTERVAL 60000  // Rotate every minute (in milliseconds)
#define IRK_KEY_SIZE 16   // 16-byte key size
#define KEY_ID 1
#define KEY_VERSION_ID 2
#define BROADCAST_NAME "IRIS3"

static struct nvs_fs fs;
static uint8_t current_key[IRK_KEY_SIZE];
static int current_key_version = 0;  // Default to version 0
static bool advertising_active = false;  // Track the state of advertising

// Initialize sync_data with the broadcast name and placeholder for key
static uint8_t sync_data[IRK_KEY_SIZE + sizeof(BROADCAST_NAME)] = BROADCAST_NAME;

// Initialize the NVS file system with optional reset
void init_nvs(bool reset) {
    fs.flash_device = FIXED_PARTITION_DEVICE(storage_partition);
    fs.offset = FIXED_PARTITION_OFFSET(storage_partition);
    
    struct flash_pages_info info;
    int rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
    if (rc) {
        printk("Failed to get page info: %d\n", rc);
        return;
    }
    fs.sector_size = info.size;
    fs.sector_count = 3U;  // Use 3 sectors for NVS storage

    rc = nvs_mount(&fs);
    if (rc) {
        printk("NVS mount failed: %d\n", rc);
        return;
    }

    if (reset) {
        // Clear all entries in NVS
        printk("Attempting to clear NVS storage...\n");
        rc = nvs_clear(&fs);
        if (rc) {
            printk("Failed to clear NVS: %d\n", rc);
        } else {
            printk("NVS storage cleared successfully\n");

            // Remount the NVS after clearing
            rc = nvs_mount(&fs);
            if (rc) {
                printk("NVS remount failed after clearing: %d\n", rc);
            } else {
                printk("NVS remounted successfully after clearing\n");
            }
        }
    }
}

// Function to generate a new random key
void generate_new_key(uint8_t *key, size_t len) {
    for (size_t i = 0; i < len; i++) {
        key[i] = sys_rand32_get() & 0xFF;  // Generate random bytes
    }
}

// Function to store the key in NVS
int store_key(uint8_t *key, size_t len) {
    int rc = nvs_write(&fs, KEY_ID, key, len);
    if (rc < 0) {
        printk("Failed to store key: %d\n", rc);
        return rc;
    }
    return 0;
}

// Function to store the key version in NVS
int store_key_version(int version) {
    int rc = nvs_write(&fs, KEY_VERSION_ID, &version, sizeof(version));
    if (rc < 0) {
        printk("Failed to store key version: %d\n", rc);
        return rc;
    }
    return 0;
}

// Function to load the current key from NVS
int load_key(uint8_t *key, size_t len) {
    int rc = nvs_read(&fs, KEY_ID, key, len);
    if (rc < 0) {
        printk("No key found, generating a new key\n");
        generate_new_key(key, len);
        store_key(key, len);
    }
    return 0;
}

// Function to load the current key version from NVS
int load_key_version(int *version) {
    int rc = nvs_read(&fs, KEY_VERSION_ID, version, sizeof(*version));
    if (rc < 0) {
        printk("No key version found, initializing to version 0\n");
        *version = 0;
        store_key_version(*version);
    }
    return 0;
}

// Function to rotate keys
void rotate_key(void) {
    // Generate a new key
    generate_new_key(current_key, IRK_KEY_SIZE);

    // Increment the key version and save both key and version in NVS
    current_key_version++;
    store_key(current_key, IRK_KEY_SIZE);
    store_key_version(current_key_version);

    // Update sync_data with the new key
    memcpy(&sync_data[sizeof(BROADCAST_NAME)], current_key, IRK_KEY_SIZE);

    printk("Rotated key to version %d\n", current_key_version);
}

void beacon_broadcast_key_signal(void) {
    int err;

    // Stop current advertising session if active
    if (advertising_active) {
        err = bt_le_adv_stop();
        if (err) {
            printk("Failed to stop advertising (err %d)\n", err);
            return;
        }
        advertising_active = false;  // Update state
    }

    // Delay to ensure the stack processes the stop operation
    k_msleep(100);

    // Update sync_data with the current key before broadcasting
    memcpy(&sync_data[sizeof(BROADCAST_NAME)], current_key, IRK_KEY_SIZE);

    // Start advertising with synchronization data
    struct bt_data ad[] = {
        BT_DATA(BT_DATA_MANUFACTURER_DATA, sync_data, sizeof(sync_data)),
    };

    // Print the key included in sync_data for verification
    printk("Broadcasting Iris3 with key: ");
    for (size_t i = sizeof(BROADCAST_NAME); i < sizeof(sync_data); i++) {
        printk("%02x ", sync_data[i]);  // Print each byte as a hex value
    }
    printk("\n");

    // Start advertising
    err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    } else {
        advertising_active = true;  // Update state
        printk("Advertising started successfully.\n");
    }
}

// Main function
int main(void) {
    int err;
    bool reset_nvs = true;  // Set to true for first-time reset
    init_nvs(reset_nvs);

    // Initialize the Bluetooth Subsystem
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return -1;
    }

    printk("Bluetooth initialized\n");

    // Load the key and key version from NVS
    load_key(current_key, IRK_KEY_SIZE);
    load_key_version(&current_key_version);

    while (1) {
        printk("Current key version: %d\n", current_key_version);
        beacon_broadcast_key_signal();  // Broadcast the synchronization signal
        k_msleep(ROTATION_INTERVAL);  // Wait for the rotation interval
        rotate_key();  // Rotate the key
    }

    return 0;  // This line is never reached but ensures correct return type
}
