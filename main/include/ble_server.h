/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef _BLE_SERVER_H_
#define _BLE_SERVER_H_

#include "esp_gap_ble_api.h"

// return = 0 for success, return != 0 will close connection
typedef int ble_server_receive_callback(const uint8_t *buf, size_t len);

// returns 0-100
typedef int ble_server_battery_percent();

// 1 = connected, 0 = disconnected
typedef void ble_server_conninfo_callback(int);

void ble_server_set_receive_callback(ble_server_receive_callback *func);
void ble_server_set_battery_percent(ble_server_battery_percent *func);

int ble_server_send(const uint8_t *buf, const size_t len);
int ble_server_is_connected();

void ble_server_accept_bonding(int status);
void ble_support_remove_bonded_device(esp_bd_addr_t *bd_addr);
int ble_support_num_bonded_devices();

char* ble_server_get_client_addr();
void ble_server_set_conninfo_callback(ble_server_conninfo_callback *cb);
void ble_server_set_passkey(uint32_t pkey);
void ble_support_get_bonded_devices(esp_ble_bond_dev_t **dev_list, int *num);

void ble_server_stop();
int ble_server_start();

#endif
