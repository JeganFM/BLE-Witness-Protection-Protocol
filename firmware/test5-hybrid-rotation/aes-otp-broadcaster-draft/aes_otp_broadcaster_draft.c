#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/kernel.h>
#include <zephyr/random/random.h> // For generating random OTP key
#include <mbedtls/aes.h>  // Include mbedTLS AES library

#define SYNC_INTERVAL_MS 60000    	// Broadcast synchronization signal every 60 seconds
#define AES_KEY_SIZE 32           	// AES-256 bit key size
#define AES_BLOCK_SIZE 16         	// AES block size
#define OTP_KEY_SIZE 16           	// OTP key size, matching sync_data length
#define MAX_MESSAGE_SIZE 254        // Maximum length of the message (e.g., "SYNC")
#define SYNC_DATA_SIZE (MAX_MESSAGE_SIZE + 2) // 1 byte for length, max message size, 1 byte for counter

// Define a 256-bit pre-shared key (PSK) for AES encryption
static const uint8_t pre_shared_key[AES_KEY_SIZE] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

// Define a fixed 16-byte Initialization Vector (IV) for AES
static uint8_t iv[AES_BLOCK_SIZE] = {
    0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00
};

static uint8_t sync_data[SYNC_DATA_SIZE]; // Allocate the sync_data array

void build_sync_data(const char *message, uint8_t counter) {
    size_t message_length = strlen(message); // Get the length of the message

    // Ensure the message fits in the sync_data buffer
    if (message_length > MAX_MESSAGE_SIZE) {
        // Handle error: message too long
        return;
    }

    // Calculate total size of sync_data
    size_t total_size = message_length + 2; // message_length + 1 (length byte) + 1 (counter byte)

    // Set the first byte as the total size of the sync_data
    sync_data[0] = (uint8_t)total_size; // Total size including the length byte and the counter byte

    // Copy the message to sync_data
    memcpy(&sync_data[1], message, message_length); // Copy the message to sync_data

    // Set the counter as the last byte
    sync_data[1 + message_length] = counter; // Set the counter as the last byte

}

// Function to generate a random OTP key
void generate_random_otp_key(uint8_t *otp_key, size_t key_size) {
    for (size_t i = 0; i < key_size; i++) {
        otp_key[i] = (uint8_t)sys_rand32_get();  // Fill the key with random bytes
    }
}

// Function to XOR-encrypt the message using OTP
void otp_encrypt(uint8_t *data, const uint8_t *otp_key, size_t data_len) {
    for (size_t i = 0; i < data_len; i++) {
        data[i] ^= otp_key[i];
    }
}

// Function to encrypt the OTP key using AES-256
void encrypt_otp_key_aes(uint8_t *otp_key, uint8_t *encrypted_key) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    // Set up AES context with the 256-bit pre-shared key
    mbedtls_aes_setkey_enc(&aes, pre_shared_key, AES_KEY_SIZE * 8);

    // Encrypt the OTP key using CBC mode
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, OTP_KEY_SIZE, iv, otp_key, encrypted_key);
    if (ret != 0) {
        printk("AES encryption of OTP key failed (ret %d)\n", ret);
    }

    mbedtls_aes_free(&aes);
}

void beacon_broadcast_sync_signal(void) {
    int err;

    // Increment the synchronization counter and wrap around at 255
    sync_data[sizeof(sync_data) - 1] = (sync_data[sizeof(sync_data) - 1] + 1) % UINT8_MAX;

    // Step 1: Generate a random OTP key for `sync_data`
    uint8_t otp_key[OTP_KEY_SIZE];
    generate_random_otp_key(otp_key, OTP_KEY_SIZE);

    // Step 2: Encrypt `sync_data` using the OTP key
    uint8_t encrypted_sync_data[sizeof(sync_data)];
    memcpy(encrypted_sync_data, sync_data, sizeof(sync_data));
    otp_encrypt(encrypted_sync_data, otp_key, sizeof(encrypted_sync_data));

    // Step 3: Encrypt the OTP key with AES-256 using the pre-shared key
    uint8_t encrypted_otp_key[OTP_KEY_SIZE];
    encrypt_otp_key_aes(otp_key, encrypted_otp_key);

    // Stop current advertising session
    err = bt_le_adv_stop();
    if (err) {
        printk("Failed to stop advertising (err %d)\n", err);
        return;
    }

    // Delay to ensure the stack processes the stop operation
    k_msleep(100);

    // Send both encrypted message and encrypted OTP key in advertising data
    struct bt_data ad[] = {
        BT_DATA(BT_DATA_MANUFACTURER_DATA, encrypted_sync_data, sizeof(encrypted_sync_data)),
        BT_DATA(BT_DATA_MANUFACTURER_DATA, encrypted_otp_key, sizeof(encrypted_otp_key))
    };

    err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    printk("Broadcasting OTP-encrypted sync data with AES-256 encrypted OTP key, counter: %d\n", sync_data[4]);
}

int main(void) {
	
	int err;
	uint8_t counter = 0x00; // Counter Value
	
    printk("Test 4 Broadcaster (Iris3) starting\n");

    // Initialize the Bluetooth Subsystem
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return -1;
    }

    printk("Bluetooth initialized\n");

    while (1) {
		build_sync_data("SYNC", counter);
        beacon_broadcast_sync_signal();  // Broadcast the synchronization signal
        k_msleep(SYNC_INTERVAL_MS);  // Wait before broadcasting the signal again
    }

    return 0;
}
