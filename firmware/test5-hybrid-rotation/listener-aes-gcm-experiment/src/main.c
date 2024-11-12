#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/random/random.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <zephyr/sys/timeutil.h>  // For time utilities

#include <tinycrypt/aes.h>
#include <tinycrypt/gcm.h>
#include <tinycrypt/constants.h>

#define ENCRYPTION_KEY_SIZE 32              // AES-256 key size (256-bit key)
#define KEY_VERSION_SETTING "secure/key_version"
#define KEY_STORAGE_SETTING "secure/encryption_key"

static uint8_t current_key[ENCRYPTION_KEY_SIZE]; // AES-256 key
static uint8_t iv[12];  // Initialization vector for AES-GCM, 12 bytes for nonce

// Function to load the current key from persistent storage
int load_key(uint8_t *key, size_t len, const char *key_storage) {
    int err = flash_area_read(&flash_area, key_storage, key, len);
    if (err) {
        printk("Failed to load encryption key\n");
        return -1;
    }
    return 0;
}

// Function to decrypt the sync data using AES-GCM mode (TinyCrypt)
int decrypt_sync_data(uint8_t *encrypted_data, size_t data_len, uint8_t *decrypted_data, uint8_t *tag) {
    struct TC_AES_GCM_ctx gcm_ctx;
    uint8_t aad[16] = {0};  // Example AAD (could be empty or customized)

    // Initialize AES-GCM context
    TC_AES_GCM_init(&gcm_ctx, current_key, ENCRYPTION_KEY_SIZE);

    // Perform AES-GCM decryption
    if (TC_AES_GCM_decrypt(&gcm_ctx, encrypted_data, data_len - 16, iv, sizeof(iv), aad, sizeof(aad), decrypted_data, tag) != TC_CRYPTO_SUCCESS) {
        printk("AES-GCM decryption failed\n");
        return -1;
    }

    return 0;
}

// Function to log the current timestamp in human-readable format
void log_timestamp(void) {
    uint32_t current_time_ms = k_uptime_get(); // Get time in milliseconds since boot
    uint32_t seconds = current_time_ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;

    // Time format: Days:Hours:Minutes:Seconds
    printk("Received at Time - %lu days, %02lu:%02lu:%02lu\n", days, hours % 24, minutes % 60, seconds % 60);
}

// Bluetooth Advertisement Data Handler
static void received_sync_data(struct bt_data *ad) {
    uint8_t encrypted_sync_data[21];  // Assuming encrypted data + tag (data size + 16 bytes for the tag)
    uint8_t decrypted_sync_data[5];  // Size of decrypted data (5 bytes expected)
    uint8_t tag[16];  // AES-GCM authentication tag

    // Assuming the received data is the sync data + tag
    if (ad->type == BT_DATA_MANUFACTURER_DATA) {
        memcpy(encrypted_sync_data, ad->data, ad->data_len);

        // Separate the encrypted data from the tag
        memcpy(tag, encrypted_sync_data + sizeof(encrypted_sync_data) - 16, 16);
        memcpy(encrypted_sync_data, encrypted_sync_data, sizeof(encrypted_sync_data) - 16);

        // Decrypt the data with AES-GCM
        if (decrypt_sync_data(encrypted_sync_data, sizeof(encrypted_sync_data), decrypted_sync_data, tag) != 0) {
            printk("Failed to decrypt sync data\n");
            return;
        }

        // Print the decrypted data
        printk("Decrypted sync data: %s\n", decrypted_sync_data);

        // Log the timestamp of the received data
        log_timestamp();
    }
}

// Bluetooth advertisement callback function
static void advertisement_received(const struct bt_data *ad, uint8_t count) {
    for (int i = 0; i < count; i++) {
        received_sync_data(&ad[i]);
    }
}

void receiver_init(void) {
    int err;

    printk("Initializing Receiver\n");

    // Initialize random number generator
    sys_rand32_init();

    // Load the current encryption key
    if (load_key(current_key, ENCRYPTION_KEY_SIZE, KEY_STORAGE_SETTING) != 0) {
        printk("Failed to load encryption key\n");
        return;
    }

    // Start Bluetooth scanning
    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, advertisement_received);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
    }
}

int main(void) {
    // Initialize the receiver
    receiver_init();

    while (1) {
        k_sleep(K_FOREVER);  // Wait indefinitely as we are passively listening for incoming advertisements
    }

    return 0;  // Return 0 to indicate successful execution
}