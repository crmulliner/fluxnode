/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"

#include "log.h"
#include "ble_server.h"
#include "ble_support.h"
#include "record.h"

//#define BLE_DEBUG_1 1
//#define BLE_DEBUG_2 1

enum
{
   FLUX_IDX_SVC,

   FLUX_IDX_FLUX_DATA_RECV_CHAR,
   FLUX_IDX_FLUX_DATA_RECV_VAL,

   FLUX_IDX_FLUX_DATA_NOTIFY_CHAR,
   FLUX_IDX_FLUX_DATA_NTY_VAL,

   FLUX_IDX_NB,
};

enum
{
   BATT_IDX_SVC,

   BATT_IDX_CHAR,
   BATT_IDX_VAL,

   BATT_IDX_NB,
};

#define FLUX_DATA_MAX_LEN (512)

#define BLE_PROFILE_NUM 2

// flux app
#define FLUX_APP_ID 0
#define GATTS_SERVICE_UUID_FLUX 0xF700
#define ESP_GATT_UUID_FLUX_DATA_RECEIVE 0xF701
#define ESP_GATT_UUID_FLUX_DATA_NOTIFY 0xF702

// batt app
#define BATT_APP_ID 1
#define BATTERY_UUID 0x180F

static ble_server_receive_callback *receive_cb = NULL;
static ble_server_conninfo_callback *conninfo_cb = NULL;
static ble_server_battery_percent *get_battery_percent = NULL;
static struct record_t recv_record;

static uint32_t passkey = 123456;

// Length Type Value
static const uint8_t flux_adv_data[] = {
    // Flags
    0x02, 0x01, 0x06,
    // Complete List of 16-bit Service Class UUIDs
    0x05, 0x03, 0x00, 0xF7, 0x0F, 0x18,
    // appearance
    0x03, 0x19, 0x40, 0x02,
    // Complete Local Name in advertising
    // length 9 (inc type), type 9, ... value
    0x09, 0x09, 'F', 'L', 'U', 'X', 'N', '0', 'D', 'E'};

static char node_name[] = {"FLUXN0DE"};

static bool accept_bonding = false;

static uint16_t flux_mtu_size = 23;
static uint16_t flux_conn_id = 0xffff;
static esp_gatt_if_t flux_gatts_if = 0xff;
static bool flux_is_connected = false;
static esp_bd_addr_t flux_remote_bda = {0x0};
static char bdaddr_str[24];

static uint16_t batt_mtu_size = 23;
static uint16_t batt_conn_id = 0xffff;
static esp_gatt_if_t batt_gatts_if = 0xff;
static bool batt_is_connected = false;
static esp_bd_addr_t batt_remote_bda = {0x0};

static uint16_t flux_handle_table[FLUX_IDX_NB];
static uint16_t batt_handle_table[BATT_IDX_NB];

static esp_ble_adv_params_t flux_adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile_inst
{
   esp_gatts_cb_t gatts_cb;
   uint16_t gatts_if;
   uint16_t app_id;
   uint16_t conn_id;
   uint16_t service_handle;
   esp_gatt_srvc_id_t service_id;
   uint16_t char_handle;
   esp_bt_uuid_t char_uuid;
   esp_gatt_perm_t perm;
   esp_gatt_char_prop_t property;
   uint16_t descr_handle;
   esp_bt_uuid_t descr_uuid;
};

static void flux_gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void batt_gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

static struct gatts_profile_inst flux_profile_tab[BLE_PROFILE_NUM] = {
    [FLUX_APP_ID] = {
        .gatts_cb = flux_gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },
    [BATT_APP_ID] = {
        .gatts_cb = batt_gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },
};

#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t flux_service_uuid = GATTS_SERVICE_UUID_FLUX;

static const uint8_t char_prop_read_notify = /* ESP_GATT_CHAR_PROP_BIT_READ | */ ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE_NR; //| ESP_GATT_CHAR_PROP_BIT_READ;

// incoming data receive characteristic, read&write without response
static const uint16_t flux_data_receive_uuid = ESP_GATT_UUID_FLUX_DATA_RECEIVE;
static const uint8_t flux_data_receive_val[20] = {0x41};

// outgoing data notify characteristic, notify&read
static const uint16_t flux_data_notify_uuid = ESP_GATT_UUID_FLUX_DATA_NOTIFY;
static const uint8_t flux_data_notify_val[20] = {0x00};

static const esp_gatts_attr_db_t flux_gatt_db[FLUX_IDX_NB] = {
    [FLUX_IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(flux_service_uuid), sizeof(flux_service_uuid), (uint8_t *)&flux_service_uuid}},

    [FLUX_IDX_FLUX_DATA_RECV_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},

    //ESP_GATT_RSP_BY_APP
    [FLUX_IDX_FLUX_DATA_RECV_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&flux_data_receive_uuid, ESP_GATT_PERM_WRITE | ESP_GATT_PERM_WRITE_ENCRYPTED, FLUX_DATA_MAX_LEN, sizeof(flux_data_receive_val), (uint8_t *)flux_data_receive_val}},

    [FLUX_IDX_FLUX_DATA_NOTIFY_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},

    //ESP_GATT_RSP_BY_APP
    [FLUX_IDX_FLUX_DATA_NTY_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&flux_data_notify_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_READ_ENCRYPTED, FLUX_DATA_MAX_LEN, sizeof(flux_data_notify_val), (uint8_t *)flux_data_notify_val}},
};

static const uint16_t battery_primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t battery_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;

static const uint16_t battery_service_uuid = BATTERY_UUID;
static const uint16_t battery_level_uuid = 0x2A19;
static const uint16_t battery_level_char_prop = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static uint8_t battery_level_val[1] = {00};

static const esp_gatts_attr_db_t batt_gatt_db[BATT_IDX_NB] = {

    [BATT_IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&battery_primary_service_uuid, ESP_GATT_PERM_READ, sizeof(battery_service_uuid), sizeof(battery_service_uuid), (uint8_t *)&battery_service_uuid}},

    [BATT_IDX_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&battery_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&battery_level_char_prop}},

    [BATT_IDX_VAL] = {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&battery_level_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_READ_ENCRYPTED, sizeof(battery_level_val), sizeof(battery_level_val), (uint8_t *)battery_level_val}},

};

static uint8_t flux_find_char_and_desr_index(uint16_t handle)
{
   uint8_t error = 0xff;

   for (int i = 0; i < FLUX_IDX_NB; i++)
   {
      if (handle == flux_handle_table[i])
      {
         return i;
      }
   }

   return error;
}

static uint8_t batt_find_char_and_desr_index(uint16_t handle)
{
   uint8_t error = 0xff;

   for (int i = 0; i < BATT_IDX_NB; i++)
   {
      if (handle == batt_handle_table[i])
      {
         return i;
      }
   }

   return error;
}

char *ble_server_get_client_addr()
{
   return bdaddr_str;
}

static void conninfo_notify(int status)
{
   if (conninfo_cb)
   {
      conninfo_cb(status);
   }
}

void ble_server_set_conninfo_callback(ble_server_conninfo_callback *cb)
{
   conninfo_cb = cb;
}

#ifdef BLE_DEBUG_1
static void ble_support_show_bonded_devices()
{
   int dev_num = esp_ble_get_bond_device_num();

   esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
   esp_ble_get_bond_device_list(&dev_num, dev_list);

   logprintf("Bonded devices list : %d\n", dev_num);
   for (int i = 0; i < dev_num; i++)
   {
      logprintf("%s: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", __func__,
                dev_list[i].bd_addr[0],
                dev_list[i].bd_addr[1],
                dev_list[i].bd_addr[2],
                dev_list[i].bd_addr[3],
                dev_list[i].bd_addr[4],
                dev_list[i].bd_addr[5]);
   }

   free(dev_list);
}
#endif

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
   esp_err_t err;

   switch (event)
   {
   case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
      esp_ble_gap_start_advertising(&flux_adv_params);
      break;
   case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      //advertising start complete event to indicate advertising start successfully or failed
#ifdef BLE_DEBUG_1
      if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS)
      {
         logprintf("Advertising start failed: %s\n", esp_err_to_name(err));
      }
#endif
      break;
   case ESP_GAP_BLE_PASSKEY_REQ_EVT:
#ifdef BLE_DEBUG_1
      logprintf("ESP_GAP_BLE_PASSKEY_REQ_EVT\n");
#endif
      // Call the following function to input the passkey which is displayed on the remote device
      //esp_ble_passkey_reply(heart_rate_profile_tab[HEART_PROFILE_APP_IDX].remote_bda, true, 0x00);
      break;
   case ESP_GAP_BLE_OOB_REQ_EVT:
#ifdef BLE_DEBUG_1
      logprintf("ESP_GAP_BLE_OOB_REQ_EVT\n");
#endif
      //uint8_t tk[16] = {1};
      // If you paired with OOB, both devices need to use the same tk
      //esp_ble_oob_req_reply(param->ble_security.ble_req.bd_addr, tk, sizeof(tk));
      break;
   case ESP_GAP_BLE_LOCAL_IR_EVT:
#ifdef BLE_DEBUG_1
      logprintf("ESP_GAP_BLE_LOCAL_IR_EVT\n");
#endif
      break;
   case ESP_GAP_BLE_LOCAL_ER_EVT:
#ifdef BLE_DEBUG_1
      logprintf("ESP_GAP_BLE_LOCAL_ER_EVT\n");
#endif
      break;
   case ESP_GAP_BLE_AUTH_CMPL_EVT:
   {
      //esp_bd_addr_t bd_addr;
      //memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
      logprintf("auth compl evt\n");

      if (!param->ble_security.auth_cmpl.success)
      {
#ifdef BLE_DEBUG_1
         logprintf("fail reason = 0x%x\n", param->ble_security.auth_cmpl.fail_reason);
#endif
      }
      else
      {
#ifdef BLE_DEBUG_1
         logprintf("auth mode = %s\n", esp_auth_req_to_str(param->ble_security.auth_cmpl.auth_mode));
#endif
      }
#ifdef BLE_DEBUG_1
      ble_support_show_bonded_devices();
#endif
   }
   break;
   case ESP_GAP_BLE_KEY_EVT:
#ifdef BLE_DEBUG_2
      //shows the ble key info share with peer device to the user.
      logprintf("key type = %s\n", esp_key_type_to_str(param->ble_security.ble_key.key_type));
#endif
      break;
   case ESP_GAP_BLE_NC_REQ_EVT:
      /* The app will receive this evt when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
        show the passkey number to the user to confirm it with the number displayed by peer device. */

#ifdef BLE_DEBUG_1
      logprintf("%s: accept bonding = %d\n", __func__, accept_bonding);
#endif
      // basically press button to flip some value to this function is called with true (or false to deny)
      esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, accept_bonding);
      if (accept_bonding)
      {
         accept_bonding = false;
      }
#ifdef BLE_DEBUG_1
      logprintf("ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%d\n", param->ble_security.key_notif.passkey);
#endif
      break;
   case ESP_GAP_BLE_SEC_REQ_EVT:
#ifdef BLE_DEBUG_1
      logprintf("ESP_GAP_BLE_SEC_REQ_EVT\n");
      logprintf("%s: accept bonding = %d\n", __func__, accept_bonding);
#endif
      /* send the positive(true) security response to the peer device to accept the security request.
        If not accept the security request, should send the security response with negative(false) accept value*/
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, accept_bonding);
      if (accept_bonding)
      {
         accept_bonding = false;
      }
      break;
   case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
      //the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
      //show the passkey number to the user to input it in the peer device.
#ifdef BLE_DEBUG_1
      logprintf("The passkey Notify number: %06d\n", param->ble_security.key_notif.passkey);
#endif
      break;
   default:
#ifdef BLE_DEBUG_1
      logprintf("default: GAP_EVT, event %d\n", event);
#endif
      break;
   }
}

static int send_function(unsigned char *buf, const size_t len, void *data)
{
   return esp_ble_gatts_send_indicate(flux_gatts_if, flux_conn_id, flux_handle_table[FLUX_IDX_FLUX_DATA_NTY_VAL], len, buf, false);
}

int ble_server_send(const uint8_t *buf, const size_t len)
{
   if (!flux_is_connected) {
      return 0;
   }

   struct record_t r;
   r.lenbytes = 2;
   r.recordSize = flux_mtu_size - 5;
   r.send = (send_func *)send_function;
   r.send_data = NULL;

   return record_send(&r, buf, len);
}

int ble_server_is_connected()
{
   return flux_is_connected;
}

int ble_support_num_bonded_devices()
{
   return esp_ble_get_bond_device_num();
}

void ble_support_get_bonded_devices(esp_ble_bond_dev_t **dev_list, int *num)
{
   int num_dev = esp_ble_get_bond_device_num();
   *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * num_dev);
   esp_ble_get_bond_device_list(&num_dev, *dev_list);
   *num = num_dev;
}

#if 0
void ble_support_remove_all_bonded_devices()
{
   int dev_num = esp_ble_get_bond_device_num();

   esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
   esp_ble_get_bond_device_list(&dev_num, dev_list);
   for (int i = 0; i < dev_num; i++)
   {
#ifdef BLE_DEBUG_1
      logprintf("%s: removing: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", __func__,
                dev_list[i].bd_addr[0],
                dev_list[i].bd_addr[1],
                dev_list[i].bd_addr[2],
                dev_list[i].bd_addr[3],
                dev_list[i].bd_addr[4],
                dev_list[i].bd_addr[5]);
#endif
      esp_ble_remove_bond_device(dev_list[i].bd_addr);
   }

   free(dev_list);
}
#endif

void ble_support_remove_bonded_device(esp_bd_addr_t *bd_addr)
{
   esp_ble_remove_bond_device(bd_addr);
}

void ble_server_accept_bonding(int status)
{
#ifdef BLE_DEBUG_1
   logprintf("%s: accept bonding ON\n", __func__);
#endif
   accept_bonding = status ? true : false;
}

static void flux_gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
   esp_ble_gatts_cb_param_t *p_data = (esp_ble_gatts_cb_param_t *)param;
   uint8_t res = 0xff;

   //logprintf("%s: event = %x\n", __func__, event);

   switch (event)
   {
   case ESP_GATTS_REG_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s REG\n", __func__);
#endif
      esp_ble_gap_set_device_name(node_name);
      esp_ble_gap_config_adv_data_raw((uint8_t *)flux_adv_data, sizeof(flux_adv_data));
      esp_ble_gatts_create_attr_tab(flux_gatt_db, gatts_if, FLUX_IDX_NB, 0);
      break;
   case ESP_GATTS_READ_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: read event\n", __func__);
#endif
      break;
   case ESP_GATTS_WRITE_EVT:
   {
      res = flux_find_char_and_desr_index(p_data->write.handle);
      if (p_data->write.is_prep == false)
      {
#ifdef BLE_DEBUG_1
         logprintf("%s: ESP_GATTS_WRITE_EVT : handle = %d\n", __func__, res);
#endif

         // incoming
         if (res == FLUX_IDX_FLUX_DATA_RECV_VAL)
         {
#ifdef BLE_DEBUG_2
            logprintf("Full Write len=%d '%*s'\n", p_data->write.len, p_data->write.len, p_data->write.value);
#endif
            record_add(&recv_record, p_data->write.value, p_data->write.len);
            if (record_complete(&recv_record))
            {
               size_t recv_len = record_len(&recv_record);
               unsigned char *recv_buf = record_get(&recv_record);
               // send received data via callback
               //int res =
               receive_cb(recv_buf, recv_len);
               // res != 0 -> disconnect
            }
         }
      }
      else if ((p_data->write.is_prep == true) && (res == FLUX_IDX_FLUX_DATA_RECV_VAL))
      {
#ifdef BLE_DEBUG_1
         logprintf("%s: Prep Write '%*s'\n", __func__, p_data->write.len, p_data->write.value);
#endif
      }
      break;
   }
   case ESP_GATTS_EXEC_WRITE_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: Write Exec\n", __func__);
#endif
      break;
   case ESP_GATTS_MTU_EVT:
      // MTU adjustment command
      flux_mtu_size = p_data->mtu.mtu;
#ifdef BLE_DEBUG_1
      logprintf("%s: MTU %d\n", __func__, flux_mtu_size);
#endif
      break;
   case ESP_GATTS_CONF_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: conf\n", __func__);
#endif
      break;
   case ESP_GATTS_UNREG_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: unreg\n", __func__);
#endif
      break;
   case ESP_GATTS_DELETE_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: delete\n", __func__);
#endif
      break;
   case ESP_GATTS_START_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: start\n", __func__);
#endif
      break;
   case ESP_GATTS_STOP_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: stop\n", __func__);
#endif
      break;
   case ESP_GATTS_CONNECT_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: connect\n", __func__);
#endif
      // we are conected
      flux_conn_id = p_data->connect.conn_id;
      flux_gatts_if = gatts_if;
      flux_is_connected = true;
      // save remote addr
      memcpy(&flux_remote_bda, &p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
      sprintf(bdaddr_str, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
              flux_remote_bda[0],
              flux_remote_bda[1],
              flux_remote_bda[2],
              flux_remote_bda[3],
              flux_remote_bda[4],
              flux_remote_bda[5]);
      conninfo_notify(1);
      break;
   case ESP_GATTS_DISCONNECT_EVT:
// disconnected
#ifdef BLE_DEBUG_1
      logprintf("%s: disconnect\n", __func__);
#endif
      flux_is_connected = false;
      conninfo_notify(0);
      esp_ble_gap_start_advertising(&flux_adv_params);
      break;
   case ESP_GATTS_OPEN_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: open\n", __func__);
#endif
      break;
   case ESP_GATTS_CANCEL_OPEN_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: cancel open\n", __func__);
#endif
      break;
   case ESP_GATTS_CLOSE_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: close\n", __func__);
#endif
      break;
   case ESP_GATTS_LISTEN_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: listen\n", __func__);
#endif
      break;
   case ESP_GATTS_CONGEST_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: congest\n", __func__);
#endif
      break;
   case ESP_GATTS_CREAT_ATTR_TAB_EVT:
   {
#ifdef BLE_DEBUG_1
      logprintf("%s: The number handle =%x\n", __func__, param->add_attr_tab.num_handle);
#endif
      if (param->add_attr_tab.status != ESP_GATT_OK)
      {
#ifdef BLE_DEBUG_1
         logprintf("%s: Create attribute table failed, error code=0x%x", __func__, param->add_attr_tab.status);
#endif
      }
      else if (param->add_attr_tab.num_handle != FLUX_IDX_NB)
      {
#ifdef BLE_DEBUG_1
         logprintf("%s: Create attribute table abnormally, num_handle (%d) doesn't equal to HRS_IDX_NB(%d)", __func__, param->add_attr_tab.num_handle, FLUX_IDX_NB);
#endif
      }
      else
      {
         memcpy(flux_handle_table, param->add_attr_tab.handles, sizeof(flux_handle_table));
         esp_ble_gatts_start_service(flux_handle_table[FLUX_IDX_SVC]);
      }
      break;
   }
   default:
#ifdef BLE_DEBUG_1
      logprintf("%s: default event = %x\n", __func__, event);
#endif
      break;
   }
}

static void batt_gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
   esp_ble_gatts_cb_param_t *p_data = (esp_ble_gatts_cb_param_t *)param;
   uint8_t res = 0xff;

   switch (event)
   {
   case ESP_GATTS_REG_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s REG\n", __func__);
#endif
      esp_ble_gap_set_device_name(node_name);
      esp_ble_gap_config_adv_data_raw((uint8_t *)flux_adv_data, sizeof(flux_adv_data));
      esp_ble_gatts_create_attr_tab(batt_gatt_db, gatts_if, BATT_IDX_NB, 0);
      break;
   case ESP_GATTS_READ_EVT:
      res = batt_find_char_and_desr_index(p_data->read.handle);
#ifdef BLE_DEBUG_1
      logprintf("%s: read event = %d\n", __func__, res);
#endif

      if (res == BATT_IDX_VAL)
      {
         esp_gatt_rsp_t rsp;
         memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
         rsp.attr_value.handle = p_data->read.handle;
         rsp.attr_value.len = 1;
         if (get_battery_percent)
         {
            rsp.attr_value.value[0] = get_battery_percent();
         }
         else
         {
            rsp.attr_value.value[0] = 99;
         }
#ifdef BLE_DEBUG_1
         logprintf("%s: read battery val %d\n", __func__, rsp.attr_value.value[0]);
#endif
         esp_ble_gatts_send_response(gatts_if, p_data->read.conn_id, p_data->read.trans_id,
                                     ESP_GATT_OK, &rsp);
      }
      // we can set it and not have to response to reads
      //esp_ble_gatts_set_attr_value(batt_handle_table[BATT_IDX_VAL], 1, &battery_level_val[0]);
      break;
   case ESP_GATTS_WRITE_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: write\n", __func__);
#endif
      break;
   case ESP_GATTS_EXEC_WRITE_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: exec write\n", __func__);
#endif
      break;
   case ESP_GATTS_MTU_EVT:
      // MTU adjustment command
      batt_mtu_size = p_data->mtu.mtu;
#ifdef BLE_DEBUG_1
      logprintf("%s: MTU %d\n", __func__, batt_mtu_size);
#endif
      break;
   case ESP_GATTS_CONF_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: conf\n", __func__);
#endif
      break;
   case ESP_GATTS_UNREG_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: unreg\n", __func__);
#endif
      break;
   case ESP_GATTS_DELETE_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: delete\n", __func__);
#endif
      break;
   case ESP_GATTS_START_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: start\n", __func__);
#endif
      break;
   case ESP_GATTS_STOP_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: stop\n", __func__);
#endif
      break;
   case ESP_GATTS_CONNECT_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: connect\n", __func__);
#endif
      batt_conn_id = p_data->connect.conn_id;
      batt_gatts_if = gatts_if;
      batt_is_connected = true;
      memcpy(&batt_remote_bda, &p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
      break;
   case ESP_GATTS_DISCONNECT_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: disconnect\n", __func__);
#endif
      batt_is_connected = false;
      esp_ble_gap_start_advertising(&flux_adv_params);
      break;
   case ESP_GATTS_OPEN_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: open\n", __func__);
#endif
      break;
   case ESP_GATTS_CANCEL_OPEN_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: cancel open\n", __func__);
#endif
      break;
   case ESP_GATTS_CLOSE_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: close\n", __func__);
#endif
      break;
   case ESP_GATTS_LISTEN_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: listen\n", __func__);
#endif
      break;
   case ESP_GATTS_CONGEST_EVT:
#ifdef BLE_DEBUG_1
      logprintf("%s: congest\n", __func__);
#endif
      break;
   case ESP_GATTS_CREAT_ATTR_TAB_EVT:
   {
#ifdef BLE_DEBUG_1
      logprintf("%s: The number handle =%x\n", __func__, param->add_attr_tab.num_handle);
#endif
      if (param->add_attr_tab.status != ESP_GATT_OK)
      {
#ifdef BLE_DEBUG_1
         logprintf("%s: Create attribute table failed, error code=0x%x", __func__, param->add_attr_tab.status);
#endif
      }
      else if (param->add_attr_tab.num_handle != BATT_IDX_NB)
      {
#ifdef BLE_DEBUG_1
         logprintf("%s: Create attribute table abnormally, num_handle (%d) doesn't equal to HRS_IDX_NB(%d)",
                   __func__, param->add_attr_tab.num_handle, BATT_IDX_NB);
#endif
      }
      else
      {
         memcpy(batt_handle_table, param->add_attr_tab.handles, sizeof(batt_handle_table));
         esp_ble_gatts_start_service(batt_handle_table[BATT_IDX_SVC]);
      }
      break;
   }
   default:
#ifdef BLE_DEBUG_1
      logprintf("%s: default event = %x\n", __func__, event);
#endif
      break;
   }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
#ifdef BLE_DEBUG_2
   logprintf("EVT %d, gatts if %d\n", event, gatts_if);
#endif

   if (event == ESP_GATTS_REG_EVT)
   {
      if (param->reg.status == ESP_GATT_OK)
      {
         flux_profile_tab[param->reg.app_id].gatts_if = gatts_if;
      }
      else
      {
#ifdef BLE_DEBUG_1
         logprintf("Reg app failed, app_id %04x, status %d\n", param->reg.app_id, param->reg.status);
#endif
         return;
      }
   }

   do
   {
      int idx;
      for (idx = 0; idx < BLE_PROFILE_NUM; idx++)
      {
         if (gatts_if == ESP_GATT_IF_NONE || gatts_if == flux_profile_tab[idx].gatts_if)
         {
            if (flux_profile_tab[idx].gatts_cb)
            {
               // call callback to handle event
               flux_profile_tab[idx].gatts_cb(event, gatts_if, param);
            }
         }
      }
   } while (0);
}

void ble_server_set_receive_callback(ble_server_receive_callback *func)
{
   receive_cb = func;
}

void ble_server_set_battery_percent(ble_server_battery_percent *func)
{
   get_battery_percent = func;
}

void ble_server_set_passkey(uint32_t pkey)
{
   passkey = pkey;
}

int ble_server_start()
{
   esp_err_t ret;
   esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

   recv_record.lenbytes = 2;
   recv_record.recordSize = flux_mtu_size - 5;
   recv_record.len = 0;
   recv_record.buf = NULL;
   recv_record.cur = 0;

   ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

   ret = esp_bt_controller_init(&bt_cfg);
   if (ret)
   {
#ifdef BLE_DEBUG_1
      logprintf("%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
#endif
      return 0;
   }

   ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
   if (ret)
   {
#ifdef BLE_DEBUG_1
      logprintf("%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
#endif
      return 0;
   }

#ifdef BLE_DEBUG_1
   logprintf("%s init bluetooth\n", __func__);
#endif
   ret = esp_bluedroid_init();
   if (ret)
   {
#ifdef BLE_DEBUG_1
      logprintf("%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
#endif
      return 0;
   }
   ret = esp_bluedroid_enable();
   if (ret)
   {
#ifdef BLE_DEBUG_1
      logprintf("%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
#endif
      return 0;
   }

   esp_ble_gatts_register_callback(gatts_event_handler);
   esp_ble_gap_register_callback(gap_event_handler);
   esp_ble_gatts_app_register(FLUX_APP_ID);
   esp_ble_gatts_app_register(BATT_APP_ID);

   // --- security ---
   // bonding with peer device after authentication
   esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
   // set to CAP_OUT to enable PIN entry on remote device
   // ESP_IO_CAP_IO == just need to confirm PIN
   esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;

   uint8_t key_size = 16;
   uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
   uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

   // ENABLE == don't got back to `just works`
   uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_ENABLE;

   uint8_t oob_support = ESP_BLE_OOB_DISABLE;
   esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
   esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
   esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
   esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
   esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));
   esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));

   /* If your BLE device acts as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the master;
    If your BLE device acts as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
   esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
   esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

   return 1;
}

void ble_server_stop()
{
   esp_bt_controller_disable();
   esp_bt_controller_deinit();
}
