#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/util.h>  // For ARRAY_SIZE macro
#include <zephyr/kernel.h>     // For k_uptime_get

#define NAME_LEN 30
#define MAX_DEVICES 100  // Maximum number of unique devices to track

static bt_addr_le_t known_devices[MAX_DEVICES];
static size_t known_device_count = 0;

// Temporarily logging all devices without filtering for troubleshooting
static bool is_new_device(const bt_addr_le_t *addr) {
    return true; // Log all devices, bypassing the check for duplicates
}

// Optionally add the device to the known list if you need to keep track
static void add_new_device(const bt_addr_le_t *addr) {
    if (known_device_count < MAX_DEVICES) {
        bt_addr_le_copy(&known_devices[known_device_count++], addr);
    }
}

static void print_timestamp(void) {
    uint64_t uptime_ms = k_uptime_get();
    uint32_t seconds = uptime_ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;

    seconds = seconds % 60;
    minutes = minutes % 60;

    printk("[%02u:%02u:%02u] ", hours, minutes, seconds);
}

#if !defined(CONFIG_BT_EXT_ADV)
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad) {
    char addr_str[BT_ADDR_LE_STR_LEN];
    char name[NAME_LEN] = {0}; // Initialize name buffer

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    bt_data_parse(ad, data_cb, name); // Extract name from advertisement

    print_timestamp();
    printk("Device found: %s (RSSI %d), type %u, AD data len %u, Name: %s\n",
           addr_str, rssi, type, ad->len, name);

    // Optionally add the device to the known list
    if (is_new_device(addr)) {
        add_new_device(addr);
    }
}
#endif /* CONFIG_BT_EXT_ADV */

#if defined(CONFIG_BT_EXT_ADV)
static bool data_cb(struct bt_data *data, void *user_data) {
    char *name = user_data;
    uint8_t len;

    switch (data->type) {
        case BT_DATA_NAME_SHORTENED:
        case BT_DATA_NAME_COMPLETE:
            len = MIN(data->data_len, NAME_LEN - 1);
            (void)memcpy(name, data->data, len);
            name[len] = '\0';
            return false;
        default:
            return true;
    }
}

static const char *phy2str(uint8_t phy) {
    switch (phy) {
        case BT_GAP_LE_PHY_NONE: return "No packets";
        case BT_GAP_LE_PHY_1M: return "LE 1M";
        case BT_GAP_LE_PHY_2M: return "LE 2M";
        case BT_GAP_LE_PHY_CODED: return "LE Coded";
        default: return "Unknown";
    }
}

static void scan_recv(const struct bt_le_scan_recv_info *info,
                      struct net_buf_simple *buf) {
    char le_addr[BT_ADDR_LE_STR_LEN];
    char name[NAME_LEN] = {0}; // Initialize name buffer
    uint8_t data_status;
    uint16_t data_len;

    data_len = buf->len;
    bt_data_parse(buf, data_cb, name);

    data_status = BT_HCI_LE_ADV_EVT_TYPE_DATA_STATUS(info->adv_props);

    bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));
    print_timestamp();
    printk("[DEVICE]: %s (%s), AD evt type %u, Tx Pwr: %i, RSSI %i "
           "Data status: %u, AD data len: %u Name: %s "
           "C:%u S:%u D:%u SR:%u E:%u Pri PHY: %s, Sec PHY: %s, "
           "Interval: 0x%04x (%u ms), SID: %u\n",
           le_addr, info->addr->type == BT_ADDR_LE_PUBLIC ? "public" : "random",
           info->adv_type, info->tx_power, info->rssi,
           data_status, data_len, name,
           (info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) != 0,
           (info->adv_props & BT_GAP_ADV_PROP_SCANNABLE) != 0,
           (info->adv_props & BT_GAP_ADV_PROP_DIRECTED) != 0,
           (info->adv_props & BT_GAP_ADV_PROP_SCAN_RESPONSE) != 0,
           (info->adv_props & BT_GAP_ADV_PROP_EXT_ADV) != 0,
           phy2str(info->primary_phy), phy2str(info->secondary_phy),
           info->interval, info->interval * 5 / 4, info->sid);

    // Optionally add the device to the known list
    if (is_new_device(info->addr)) {
        add_new_device(info->addr);
    }
}

static struct bt_le_scan_cb scan_callbacks = {
    .recv = scan_recv,
};
#endif /* CONFIG_BT_EXT_ADV */

int observer_start(void) {
    struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_PASSIVE,
        .options    = 0,  // Disable duplicate filtering to log every advertisement
        .interval   = 0x0060,  // Adjusted interval for more frequent scans
        .window     = 0x0030,  // Adjusted window to match or be less than interval
    };
    int err;

#if defined(CONFIG_BT_EXT_ADV)
    bt_le_scan_cb_register(&scan_callbacks);
    printk("Registered scan callbacks\n");
#endif /* CONFIG_BT_EXT_ADV */

    err = bt_le_scan_start(&scan_param, 
#if !defined(CONFIG_BT_EXT_ADV)
                           device_found
#else
                           NULL
#endif
                           );
    if (err) {
        printk("Start scanning failed (err %d)\n", err);
        return err;
    }
    printk("Started scanning...\n");

    return 0;
}
