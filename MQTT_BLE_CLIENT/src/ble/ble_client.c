
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/scan.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gatt.h>
#include <string.h>
#include "ble_client.h"

static bool ad_data_parse_callback(struct bt_data *data, void *user_data) {
    bool *target_found = (bool *)user_data;

    if (data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED) {
        char name[30];  // Adjust size as needed
        size_t name_len = MIN(data->data_len, sizeof(name) - 1);
        memcpy(name, data->data, name_len);
        name[name_len] = '\0';

        if (strcmp(name, "Christmas display") == 0) {
            printk("Device found: Christmas display\n");
            *target_found = true;  // Update the flag
            return false;          // Stop further parsing
        }
    }
    return true; // Continue parsing
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type, struct net_buf_simple *ad) {
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    printk("Scanning device: %s (RSSI %d)\n", addr_str, rssi);

    bool target_found = false;  // Flag to track if "Christmas display" is found
    bt_data_parse(ad, ad_data_parse_callback, &target_found);

    if (target_found) {
        bt_le_scan_stop();
        printk("Target device found. Stopping scan.\n");
    }
}

void start_scan(void) {
    static struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_ACTIVE,
        .options    = BT_LE_SCAN_OPT_NONE,
        .interval   = BT_GAP_SCAN_FAST_INTERVAL,
        .window     = BT_GAP_SCAN_FAST_WINDOW,
    };

    int err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
    } else {
        printk("Scanning for 'Christmas display' started\n");
    }
}

void ble_init(void) {
    int err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }
    printk("Bluetooth initialized\n");
}
