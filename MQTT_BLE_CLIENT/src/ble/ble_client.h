#ifndef BLE_CLIENT_H
#define BLE_CLIENT_H

#include <stddef.h>
#include <stdint.h>

void ble_init(void);
void start_scan(void);

void write_to_ble_0001(uint8_t *data, size_t length);

#endif // BLE_CLIENT_H