#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/scan.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gatt.h>
#include <string.h>
#include "ble_client.h"

// Forward declaration of connection callbacks
static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);
void start_service_discovery(struct bt_conn *conn);

// Define connection callbacks
static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

static struct bt_conn *default_conn;

// Discovery parameters
static struct bt_gatt_discover_params discover_params;

// Called when device connects
static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        printk("Connection failed (err %u)\n", err);
        return;
    }

    printk("Connected\n");

    // Start GATT service discovery
    start_service_discovery(conn);
}

// Called when device disconnects
static void disconnected(struct bt_conn *conn, uint8_t reason) {
    printk("Disconnected (reason %u)\n", reason);
    if (default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
}

static uint16_t service_start_handle, service_end_handle;

// Callback function for characteristic discovery
static uint8_t discover_characteristics(struct bt_conn *conn,
                                        const struct bt_gatt_attr *attr,
                                        struct bt_gatt_discover_params *params) {
    if (!attr) {
        printk("Characteristic discovery complete\n");
        return BT_GATT_ITER_STOP;
    }

    if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC && attr->user_data) {
        const struct bt_gatt_chrc *chrc = attr->user_data;

        char uuid_str[37];
        bt_uuid_to_str(chrc->uuid, uuid_str, sizeof(uuid_str));
        printk("Characteristic UUID: %s\n", uuid_str);
        printk("  Properties: 0x%02x\n", chrc->properties);
        printk("  Value handle: 0x%04x\n", chrc->value_handle);
    }

    return BT_GATT_ITER_CONTINUE;
}

// Function to start characteristic discovery for a given service
void start_characteristic_discovery(struct bt_conn *conn, uint16_t start_handle, uint16_t end_handle) {
    discover_params.uuid = NULL; // Discover all characteristics
    discover_params.start_handle = start_handle;
    discover_params.end_handle = end_handle;
    discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    discover_params.func = discover_characteristics;

    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        printk("Failed to start characteristic discovery (err %d)\n", err);
    } else {
        printk("Characteristic discovery started\n");
    }
}

// Callback function for service discovery
static uint8_t discover_services(struct bt_conn *conn,
                                  const struct bt_gatt_attr *attr,
                                  struct bt_gatt_discover_params *params) {
    if (!attr) {
        printk("Service discovery complete\n");
        return BT_GATT_ITER_STOP;
    }

    if (params->type == BT_GATT_DISCOVER_PRIMARY && attr->user_data) {
        const struct bt_gatt_service_val *svc = attr->user_data;

        char uuid_str[37];
        bt_uuid_to_str(svc->uuid, uuid_str, sizeof(uuid_str));
        printk("Primary service UUID: %s\n", uuid_str);
        printk("Service handles: start 0x%04x, end 0x%04x\n", attr->handle, svc->end_handle);

        // Save service handles for characteristic discovery
        service_start_handle = attr->handle;
        service_end_handle = svc->end_handle;

        // Start discovering characteristics for this service
        start_characteristic_discovery(conn, service_start_handle, service_end_handle);
    }

    return BT_GATT_ITER_CONTINUE;
}

// Function to start service discovery
void start_service_discovery(struct bt_conn *conn) {
    static struct bt_uuid_16 service_uuid = BT_UUID_INIT_16(0x1234);
    discover_params.uuid = &service_uuid.uuid;
    discover_params.start_handle = 0x0001;
    discover_params.end_handle = 0xffff;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;
    discover_params.func = discover_services;

    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        printk("Failed to start service discovery (err %d)\n", err);
    } else {
        printk("Service discovery started\n");
    }
}

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
        printk("Target device found: %s\n", addr_str);

        // Stop scanning
        int err = bt_le_scan_stop();
        if (err) {
            printk("Failed to stop scanning (err %d)\n", err);
            return;
        }

        // Attempt to connect
        struct bt_conn_le_create_param create_param = BT_CONN_LE_CREATE_PARAM_INIT(
            BT_CONN_LE_OPT_NONE,
            BT_GAP_SCAN_FAST_INTERVAL,
            BT_GAP_SCAN_FAST_WINDOW);

        struct bt_le_conn_param *conn_param = BT_LE_CONN_PARAM_DEFAULT;

        err = bt_conn_le_create(addr, &create_param, conn_param, &default_conn);
        if (err) {
            printk("Create connection failed (err %d)\n", err);
            return;
        }

        printk("Connection initiated\n");
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

    // Register connection callbacks
    bt_conn_cb_register(&conn_callbacks);
}