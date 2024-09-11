#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/kernel.h>

#define TOTAL_NAMES 255
#define NAME_LEN 30
#define SYNC_DATA_LEN 5  // Sync packet length

static uint8_t mfg_data[] = { 0x01, 0x02, 0x03, 0x04 };
static char device_names[TOTAL_NAMES][NAME_LEN];
static int current_name_index = 0;
static const uint8_t sync_data[] = { 'S', 'Y', 'N', 'C' };

// Populate device names
void populate_device_names(void) {
    for (int i = 0; i < TOTAL_NAMES; i++) {
        snprintf(device_names[i], NAME_LEN, "Iris9_%03d", i + 1);  // Generate names like Iris6_001, Iris6_002, etc.
    }
}

// Format time as [HH:MM:SS]
void format_time(int64_t ms, char *formatted_time, size_t size) {
    int64_t seconds = ms / 1000;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    snprintf(formatted_time, size, "[%02lld:%02lld:%02lld]", hours, minutes, seconds);
}

// Rotate device name upon receiving sync signal
void rotate_device_name(void) {
    char *device_name = device_names[current_name_index];
    struct bt_data ad[] = {
        BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data)),
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
    printk("%s Test 4: Rotating to device name: %s\n", formatted_time, device_name);

    current_name_index = (current_name_index + 1) % TOTAL_NAMES;
}

// Callback when sync signal is detected
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad) {
    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        uint8_t ad_type = net_buf_simple_pull_u8(ad);

        if (ad_type == BT_DATA_MANUFACTURER_DATA && len >= SYNC_DATA_LEN) {
            if (memcmp(ad->data, sync_data, SYNC_DATA_LEN - 1) == 0) {
                // Extract sync count from the last byte
                uint8_t sync_count = ad->data[SYNC_DATA_LEN - 1];
                
                printk("Sync signal detected with counter: %d, RSSI: %d\n", sync_count, rssi);
                rotate_device_name();  // Rotate only on receiving sync signal
                break;
            }
        }
        net_buf_simple_pull(ad, len - 1);
    }
}

// Start scanning for sync signals
int observer_start(void) {
    struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_PASSIVE,
        .options    = BT_LE_SCAN_OPT_NONE,
        .interval   = 0x0010,
        .window     = 0x0010,
    };

    int err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        printk("Starting scanning failed (err %d)\n", err);
        return err;
    }

    printk("Scanning started\n");
    return 0;
}

int main(void) {
    int err;
    printk("Starting Test 4: Sync-based device name rotation\n");

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return -1;
    }

    printk("Bluetooth initialized\n");
    populate_device_names();
    observer_start();

    while (1) {
        k_sleep(K_FOREVER);  // Wait indefinitely for sync signals
    }

    return 0;
}
