#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <string.h>

#define MIN_ROTATION_INTERVAL_MS 60000 // Minimum 1 minute
#define MAX_ROTATION_INTERVAL_MS 300000 // Maximum 5 minutes
static int current_name_index = 1;

// Function to format time in [HH:MM:SS] format
void format_time(int64_t ms, char *formatted_time, size_t size) {
    int64_t seconds = ms / 1000;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    snprintf(formatted_time, size, "[%02lld:%02lld:%02lld]", hours, minutes, seconds);
}

// Function to rotate the device name and print the timestamp
void rotate_device_name(void) {
    char device_name[30];
    snprintf(device_name, sizeof(device_name), "Iris9_%d", current_name_index);

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
    current_name_index++;
}

int main(void) {
    int err;

    printk("Starting Test 3: Random Interval Name Rotation with Timestamps\n");

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }

    printk("Bluetooth initialized\n");

    srand(k_uptime_get_32()); // Seed the random number generator

    rotate_device_name(); // Start advertising with the initial name

    while (1) {
        int random_interval = MIN_ROTATION_INTERVAL_MS + rand() % (MAX_ROTATION_INTERVAL_MS - MIN_ROTATION_INTERVAL_MS);
        k_sleep(K_MSEC(random_interval));
        rotate_device_name(); // Rotate name at random intervals
    }

    return 0;
}
