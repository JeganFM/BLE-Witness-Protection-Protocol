/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>

static uint8_t mfg_data[] = { 0xff, 0xff, 0x00 };
static const char *device_name = "My_Broadcaster_3";

void get_device_mac(bt_addr_le_t *addr)
{
    size_t count = 1;
    bt_id_get(addr, &count);
}

int main(void)
{
    int err;
    bt_addr_le_t addr;
    char addr_str[BT_ADDR_LE_STR_LEN];

    printk("Starting Broadcaster\n");

    /* Initialize the Bluetooth Subsystem */
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");

    /* Get and print the device MAC address */
    get_device_mac(&addr);
    bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));
    printk("Device Address: %s\n", addr_str);

    /* Set the device name */
    bt_set_name(device_name);

    /* Advertising data */
    struct bt_data ad[] = {
        BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data)),
        BT_DATA(BT_DATA_NAME_COMPLETE, device_name, strlen(device_name)),
        BT_DATA(BT_DATA_LE_BT_DEVICE_ADDRESS, addr.a.val, sizeof(addr.a.val)),
    };

    do {
        k_msleep(1000);

        printk("Sending advertising data: 0x%02X\n", mfg_data[2]);

        /* Start advertising */
        err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
        if (err) {
            printk("Advertising failed to start (err %d)\n", err);
            return 0;
        }

        k_msleep(1000);

        err = bt_le_adv_stop();
        if (err) {
            printk("Advertising failed to stop (err %d)\n", err);
            return 0;
        }

        mfg_data[2]++;

    } while (1);
    return 0;
}
