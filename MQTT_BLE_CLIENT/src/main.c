
#include "ble/ble_client.h"

void main(void) {
    // Initialize BLE
    ble_init();

    // Start BLE scanning
    start_scan();

    // Proceed with MQTT functionality...
}