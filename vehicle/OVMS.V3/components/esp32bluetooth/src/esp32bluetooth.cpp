/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include <string.h>
#include "esp_system.h"
#include "esp32bluetooth.h"
#include "esp_bt.h"
#include "ovms_peripherals.h"
#include "ovms_config.h"

#include "ovms_log.h"
static const char *TAG = "bluetooth";

#define OVMS_BLE_APP_ID             0x42
#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)
#define OVMS_PROFILE_NUM            1
#define OVMS_PROFILE_APP_IDX        0

#define GATTS_SERVICE_UUID_OVMS_METRICS   0x00FF
#define GATTS_CHAR_UUID_OVMS_METRICS      0xFF01
#define GATTS_DESCR_UUID_OVMS_METRICS     0x4242
#define GATTS_NUM_HANDLE_OVMS_METRICS     4

static uint8_t adv_config_done = 0;
static uint8_t ovms_manufacturer[4]={'O', 'V', 'M', 'S'};
static uint8_t ovms_sec_service_uuid[16] = {
  /* LSB <--------------------------------------------------------------------------------> MSB */
  //first uuid, 16bit, [12],[13] is the value
  0x76, 0x5d, 0xf7, 0x40, 0xcc, 0x70, 0x0a, 0xb6, 0xbb, 0x43, 0x08, 0x30, 0x38, 0xcb, 0x2f, 0xe7
};
static esp_ble_adv_data_t ovms_adv_config;
static esp_ble_adv_data_t ovms_scan_rsp_config;
static esp_ble_adv_params_t ovms_adv_params;
static struct gatts_profile_inst ovms_profile_tab[OVMS_PROFILE_NUM];

// Demo property (just for testing)
uint8_t char1_str[] = {'O','V','M','S'};
esp_gatt_char_prop_t a_property = 0;

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40
esp_attr_value_t gatts_demo_char1_val =
{
    .attr_max_len = GATTS_DEMO_CHAR_VAL_LEN_MAX,
    .attr_len     = sizeof(char1_str),
    .attr_value   = char1_str,
};
// End of demo property

static const char *esp_key_type_to_str(esp_ble_key_type_t key_type)
  {
  const char *key_str = NULL;

  switch(key_type)
    {
    case ESP_LE_KEY_NONE:
      key_str = "ESP_LE_KEY_NONE";
      break;
    case ESP_LE_KEY_PENC:
      key_str = "ESP_LE_KEY_PENC";
      break;
    case ESP_LE_KEY_PID:
      key_str = "ESP_LE_KEY_PID";
      break;
    case ESP_LE_KEY_PCSRK:
      key_str = "ESP_LE_KEY_PCSRK";
      break;
    case ESP_LE_KEY_PLK:
      key_str = "ESP_LE_KEY_PLK";
      break;
    case ESP_LE_KEY_LLK:
      key_str = "ESP_LE_KEY_LLK";
      break;
    case ESP_LE_KEY_LENC:
      key_str = "ESP_LE_KEY_LENC";
      break;
    case ESP_LE_KEY_LID:
      key_str = "ESP_LE_KEY_LID";
      break;
    case ESP_LE_KEY_LCSRK:
      key_str = "ESP_LE_KEY_LCSRK";
      break;
    default:
      key_str = "INVALID BLE KEY TYPE";
      break;
    }

  return key_str;
  }

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
  {
  ESP_LOGD(TAG, "GAP_EVT, event %d\n", event);

  switch (event)
    {
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
      adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
      if (adv_config_done == 0)
        {
        esp_ble_gap_start_advertising(&ovms_adv_params);
        }
      break;
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
      adv_config_done &= (~ADV_CONFIG_FLAG);
      if (adv_config_done == 0)
        {
        esp_ble_gap_start_advertising(&ovms_adv_params);
        }
      break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      //advertising start complete event to indicate advertising start successfully or failed
      if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
        ESP_LOGE(TAG, "advertising start failed, error status = %x", param->adv_start_cmpl.status);
        }
      else
        {
        ESP_LOGI(TAG, "advertising start success");
        }
      break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
        ESP_LOGE(TAG, "Advertising stop failed\n");
        }
      else
        {
        ESP_LOGI(TAG, "Stop adv successfully\n");
        }
      break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
      ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
              param->update_conn_params.status,
              param->update_conn_params.min_int,
              param->update_conn_params.max_int,
              param->update_conn_params.conn_int,
              param->update_conn_params.latency,
              param->update_conn_params.timeout);
      break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:                           /* passkey request event */
      ESP_LOGI(TAG, "ESP_GAP_BLE_PASSKEY_REQ_EVT");
      //esp_ble_passkey_reply(heart_rate_profile_tab[HEART_PROFILE_APP_IDX].remote_bda, true, 0x00);
      break;
    case ESP_GAP_BLE_OOB_REQ_EVT:                                /* OOB request event */
      ESP_LOGI(TAG, "ESP_GAP_BLE_OOB_REQ_EVT");
      break;
    case ESP_GAP_BLE_LOCAL_IR_EVT:                               /* BLE local IR event */
      ESP_LOGI(TAG, "ESP_GAP_BLE_LOCAL_IR_EVT");
      break;
    case ESP_GAP_BLE_LOCAL_ER_EVT:                               /* BLE local ER event */
      ESP_LOGI(TAG, "ESP_GAP_BLE_LOCAL_ER_EVT");
      break;
    case ESP_GAP_BLE_NC_REQ_EVT:
      /* The app will receive this evt when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
      show the passkey number to the user to confirm it with the number displayed by peer deivce. */
      ESP_LOGI(TAG, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%d", param->ble_security.key_notif.passkey);
      break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
      /* send the positive(true) security response to the peer device to accept the security request.
      If not accept the security request, should sent the security response with negative(false) accept value*/
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:  ///the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
      ///show the passkey number to the user to input it in the peer deivce.
      ESP_LOGI(TAG, "The passkey Notify number:%d", param->ble_security.key_notif.passkey);
      break;
    case ESP_GAP_BLE_KEY_EVT:
      //shows the ble key info share with peer device to the user.
      ESP_LOGI(TAG, "key type = %s", esp_key_type_to_str(param->ble_security.ble_key.key_type));
      break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      {
      esp_bd_addr_t bd_addr;
      memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
      ESP_LOGI(TAG, "remote BD_ADDR: %08x%04x",\
              (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
              (bd_addr[4] << 8) + bd_addr[5]);
      ESP_LOGI(TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
      ESP_LOGI(TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");
      //show_bonded_devices();
      break;
      }
    case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:
      {
      ESP_LOGD(TAG, "ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT status = %d", param->remove_bond_dev_cmpl.status);
      ESP_LOGI(TAG, "ESP_GAP_BLE_REMOVE_BOND_DEV");
      ESP_LOGI(TAG, "-----ESP_GAP_BLE_REMOVE_BOND_DEV----");
      esp_log_buffer_hex(TAG, (void *)param->remove_bond_dev_cmpl.bd_addr, sizeof(esp_bd_addr_t));
      ESP_LOGI(TAG, "------------------------------------");
      break;
      }
    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:
      {
      if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
        ESP_LOGE(TAG, "config local privacy failed, error status = %x", param->local_privacy_cmpl.status);
        break;
        }

      esp_err_t ret = esp_ble_gap_config_adv_data(&ovms_adv_config);
      if (ret)
        {
        ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        }
      else
        {
        adv_config_done |= ADV_CONFIG_FLAG;
        }

      ret = esp_ble_gap_config_adv_data(&ovms_scan_rsp_config);
      if (ret)
        {
        ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        }
      else
        {
        adv_config_done |= SCAN_RSP_CONFIG_FLAG;
        }
      break;
      }
    default:
      break;
    }
  }

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param)
  {
  ESP_LOGD(TAG, "GATTS_EVT, event %d\n", event);

  /* If event is register event, store the gatts_if for each profile */
  if (event == ESP_GATTS_REG_EVT)
    {
    if (param->reg.status == ESP_GATT_OK)
      {
      ESP_LOGI(TAG,"Reg app succeeded with inteface ID %d", gatts_if);
      ovms_profile_tab[OVMS_PROFILE_APP_IDX].gatts_if = gatts_if;
      }
    else
      {
      ESP_LOGI(TAG, "Reg app failed, app_id %04x, status %d\n",
               param->reg.app_id,
               param->reg.status);
      return;
      }
    }

  do
    {
    int idx;
    for (idx = 0; idx < OVMS_PROFILE_NUM; idx++)
      {
      if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
          gatts_if == ovms_profile_tab[idx].gatts_if)
        {
        if (ovms_profile_tab[idx].gatts_cb)
          {
          ovms_profile_tab[idx].gatts_cb(event, gatts_if, param);
          }
        }
      }
    } while (0);
  }

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
  {
  ESP_LOGD(TAG, "GATTS PROFILE event = %d\n",event);

  switch (event)
    {
    case ESP_GATTS_REG_EVT:
      {
      std::string devname = MyConfig.GetParamValue("vehicle", "id");
      if (devname.empty())
        {
        devname = std::string("OVMS");
        }
      else
        {
        devname.insert(0,"OVMS ");
        }
      esp_ble_gap_set_device_name(devname.c_str());
      esp_ble_gap_config_local_privacy(true);
      ovms_profile_tab[OVMS_PROFILE_APP_IDX].service_id.is_primary = true;
      ovms_profile_tab[OVMS_PROFILE_APP_IDX].service_id.id.inst_id = 0x00;
      ovms_profile_tab[OVMS_PROFILE_APP_IDX].service_id.id.uuid.len = ESP_UUID_LEN_16;
      ovms_profile_tab[OVMS_PROFILE_APP_IDX].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_OVMS_METRICS;
      esp_err_t ret = esp_ble_gap_config_adv_data(&ovms_adv_config);
      if (ret)
        {
        ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        }
      adv_config_done |= ADV_CONFIG_FLAG;
      //config scan response data
      ret = esp_ble_gap_config_adv_data(&ovms_scan_rsp_config);
      if (ret)
        {
        ESP_LOGE(TAG, "config scan response data failed, error code = %x", ret);
        }
      adv_config_done |= SCAN_RSP_CONFIG_FLAG;
      esp_ble_gatts_create_service(gatts_if, &ovms_profile_tab[OVMS_PROFILE_APP_IDX].service_id, GATTS_NUM_HANDLE_OVMS_METRICS);
      }
      break;
    case ESP_GATTS_READ_EVT:
      {
      ESP_LOGI(TAG, "GATT_READ_EVT, conn_id %d, trans_id %d, handle %d\n",
      param->read.conn_id, param->read.trans_id, param->read.handle);
      esp_gatt_rsp_t rsp;
      memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
      rsp.attr_value.handle = param->read.handle;
      rsp.attr_value.len = 4;
      rsp.attr_value.value[0] = 0xde;
      rsp.attr_value.value[1] = 0xed;
      rsp.attr_value.value[2] = 0xbe;
      rsp.attr_value.value[3] = 0xef;
      esp_ble_gatts_send_response(gatts_if,
                                  param->read.conn_id,
                                  param->read.trans_id,
                                  ESP_GATT_OK, &rsp);
      break;
      }
    case ESP_GATTS_WRITE_EVT:
      ESP_LOGI(TAG, "ESP_GATTS_WRITE_EVT, write value:");
      esp_log_buffer_hex(TAG, param->write.value, param->write.len);
      break;
    case ESP_GATTS_EXEC_WRITE_EVT:
      break;
    case ESP_GATTS_MTU_EVT:
      break;
    case ESP_GATTS_CONF_EVT:
      break;
    case ESP_GATTS_UNREG_EVT:
      break;
    case ESP_GATTS_DELETE_EVT:
      break;
    case ESP_GATTS_START_EVT:
      break;
    case ESP_GATTS_STOP_EVT:
      break;
    case ESP_GATTS_CONNECT_EVT:
      {
      esp_ble_conn_update_params_t conn_params;
      memset(&conn_params,0,sizeof(conn_params));
      memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
      /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
      conn_params.latency = 0;
      conn_params.max_int = 0x30;    // max_int = 0x30*1.25ms = 40ms
      conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
      conn_params.timeout = 400;     // timeout = 400*10ms = 4000ms
      ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x",
             param->connect.conn_id,
             param->connect.remote_bda[0],
             param->connect.remote_bda[1],
             param->connect.remote_bda[2],
             param->connect.remote_bda[3],
             param->connect.remote_bda[4],
             param->connect.remote_bda[5]);
      ovms_profile_tab[OVMS_PROFILE_APP_IDX].conn_id = param->connect.conn_id;
      //start sent the update connection parameters to the peer device.
      esp_ble_gap_update_conn_params(&conn_params);
      esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
      break;
      }
    case ESP_GATTS_DISCONNECT_EVT:
      ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT");
      /* start advertising again when missing the connect */
      esp_ble_gap_start_advertising(&ovms_adv_params);
      break;
    case ESP_GATTS_OPEN_EVT:
      break;
    case ESP_GATTS_CANCEL_OPEN_EVT:
      break;
    case ESP_GATTS_CLOSE_EVT:
      break;
    case ESP_GATTS_LISTEN_EVT:
      break;
    case ESP_GATTS_CONGEST_EVT:
      break;
    case ESP_GATTS_CREATE_EVT:
      {
      ESP_LOGI(TAG, "ESP_GATTS_CREATE_EVT, status %d, service_handle %d\n", param->create.status, param->create.service_handle);
      ovms_profile_tab[OVMS_PROFILE_APP_IDX].service_handle = param->create.service_handle;
      ovms_profile_tab[OVMS_PROFILE_APP_IDX].char_uuid.len = ESP_UUID_LEN_16;
      ovms_profile_tab[OVMS_PROFILE_APP_IDX].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_OVMS_METRICS;
      esp_ble_gatts_start_service(ovms_profile_tab[OVMS_PROFILE_APP_IDX].service_handle);

      a_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
      esp_err_t add_char_ret =
      esp_ble_gatts_add_char(ovms_profile_tab[OVMS_PROFILE_APP_IDX].service_handle,
                            &ovms_profile_tab[OVMS_PROFILE_APP_IDX].char_uuid,
                            ESP_GATT_PERM_READ,
                            a_property,
                            &gatts_demo_char1_val,
                            NULL);
      if (add_char_ret)
        {
        ESP_LOGE(TAG, "add char failed, error code =%x",add_char_ret);
        }
      break;
      }
    case ESP_GATTS_ADD_CHAR_EVT:
      {
      uint16_t length = 0;
      const uint8_t *prf_char;

      ESP_LOGI(TAG, "ADD_CHAR_EVT, status %d,  attr_handle %d, service_handle %d\n",
             param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
      ovms_profile_tab[OVMS_PROFILE_APP_IDX].char_handle = param->add_char.attr_handle;
      ovms_profile_tab[OVMS_PROFILE_APP_IDX].descr_uuid.len = ESP_UUID_LEN_16;
      ovms_profile_tab[OVMS_PROFILE_APP_IDX].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
      esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle, &length, &prf_char);
      if (get_attr_ret == ESP_FAIL)
        {
        ESP_LOGE(TAG, "ILLEGAL HANDLE");
        }
      ESP_LOGI(TAG, "the gatts demo char length = %x\n", length);
      for (int i = 0; i < length; i++)
        {
        ESP_LOGI(TAG, "prf_char[%x] = %x\n",i,prf_char[i]);
        }
      esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(
                             ovms_profile_tab[OVMS_PROFILE_APP_IDX].service_handle,
                             &ovms_profile_tab[OVMS_PROFILE_APP_IDX].descr_uuid,
                             ESP_GATT_PERM_READ,
                             NULL,NULL);
      if (add_descr_ret)
        {
        ESP_LOGE(TAG, "add char descr failed, error code = %x", add_descr_ret);
        }
      break;
      }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
      ESP_LOGI(TAG, "ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d\n",
              param->add_char.status, param->add_char.attr_handle,
              param->add_char.service_handle);
      break;
    default:
      break;
    }
  }

esp32bluetooth::esp32bluetooth(const char* name)
  : pcp(name)
  {
  m_service_running = false;
  m_powermode = Off;
  }

esp32bluetooth::~esp32bluetooth()
  {
  }

void esp32bluetooth::StartService()
  {
  esp_err_t ret;

  if (m_service_running)
    {
    ESP_LOGE(TAG,"Bluetooth service cannot start (already running)");
    return;
    }

  ESP_LOGI(TAG,"Powering bluetooth on...");

  memset(&ovms_adv_config,0,sizeof(ovms_adv_config));
  ovms_adv_config.set_scan_rsp = false;
  ovms_adv_config.include_txpower = true;
  ovms_adv_config.min_interval = 0x100;
  ovms_adv_config.max_interval = 0x100;
  ovms_adv_config.appearance = 0x00;
  ovms_adv_config.manufacturer_len = 0;
  ovms_adv_config.p_manufacturer_data =  NULL;
  ovms_adv_config.service_data_len = 0;
  ovms_adv_config.p_service_data = NULL;
  ovms_adv_config.service_uuid_len = sizeof(ovms_sec_service_uuid);
  ovms_adv_config.p_service_uuid = ovms_sec_service_uuid;
  ovms_adv_config.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

  memset(&ovms_scan_rsp_config,0,sizeof(ovms_scan_rsp_config));
  ovms_scan_rsp_config.set_scan_rsp = true;
  ovms_scan_rsp_config.include_name = true;
  ovms_scan_rsp_config.manufacturer_len = sizeof(ovms_manufacturer);
  ovms_scan_rsp_config.p_manufacturer_data = ovms_manufacturer;

  memset(&ovms_adv_params,0,sizeof(ovms_adv_params));
  ovms_adv_params.adv_int_min        = 0x100;
  ovms_adv_params.adv_int_max        = 0x100;
  ovms_adv_params.adv_type           = ADV_TYPE_IND;
  ovms_adv_params.own_addr_type      = BLE_ADDR_TYPE_RANDOM;
  ovms_adv_params.channel_map        = ADV_CHNL_ALL;
  ovms_adv_params.adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

  memset(&ovms_profile_tab,0,sizeof(ovms_profile_tab));
  /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
  ovms_profile_tab[OVMS_PROFILE_APP_IDX].gatts_cb = gatts_profile_event_handler;
  ovms_profile_tab[OVMS_PROFILE_APP_IDX].gatts_if = ESP_GATT_IF_NONE;

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ret = esp_bt_controller_init(&bt_cfg);
  if (ret)
    {
    ESP_LOGE(TAG, "init controller failed: %s", esp_err_to_name(ret));
    return;
    }

  ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (ret)
    {
    ESP_LOGE(TAG, "enable controller failed: %s", esp_err_to_name(ret));
    return;
    }

  ret = esp_bluedroid_init();
  if (ret)
    {
    ESP_LOGE(TAG, "init bluetooth failed: %s", esp_err_to_name(ret));
    return;
    }

  ret = esp_bluedroid_enable();
  if (ret)
    {
    ESP_LOGE(TAG, "enable bluetooth failed: %s", esp_err_to_name(ret));
    return;
    }

  ret = esp_ble_gatts_register_callback(gatts_event_handler);
  if (ret)
    {
    ESP_LOGE(TAG, "gatts register error, error code = %x", ret);
    return;
    }

  ret = esp_ble_gap_register_callback(gap_event_handler);
  if (ret)
    {
    ESP_LOGE(TAG, "gap register error, error code = %x", ret);
    return;
    }

  ret = esp_ble_gatts_app_register(OVMS_BLE_APP_ID);
  if (ret)
    {
    ESP_LOGE(TAG, "gatts app register error, error code = %x", ret);
    return;
    }

  /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;     //bonding with peer device after authentication
  esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;           //set the IO capability to output but no input
  uint8_t key_size = 16;      //the key size should be 7~16 bytes
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
  /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribut to you,
  and the response key means which key you can distribut to the Master;
  If your BLE device act as a master, the response key means you hope which types of key of the slave should distribut to you,
  and the init key means which key you can distribut to the slave. */
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

  m_service_running = true;
  }

void esp32bluetooth::StopService()
  {
  esp_err_t ret;

  if (!m_service_running)
    {
    ESP_LOGE(TAG,"Bluetooth service cannot stop (not running)");
    return;
    }

  ESP_LOGI(TAG,"Powering bluetooth off...");

  ret = esp_ble_gatts_app_unregister(ovms_profile_tab[OVMS_PROFILE_APP_IDX].gatts_if);
  if (ret)
    {
    ESP_LOGE(TAG, "gatts app unregister error, error code = %x", ret);
    return;
    }

  ret = esp_bluedroid_disable();
  if (ret)
    {
    ESP_LOGE(TAG, "disable bluetooth failed: %s", esp_err_to_name(ret));
    return;
    }

  ret = esp_bluedroid_deinit();
  if (ret)
    {
    ESP_LOGE(TAG, "deinit bluetooth failed: %s", esp_err_to_name(ret));
    return;
    }

  ret = esp_bt_controller_disable();
  if (ret)
    {
    ESP_LOGE(TAG, "disable controller failed: %s", esp_err_to_name(ret));
    return;
    }

  ret = esp_bt_controller_deinit();
  if (ret)
    {
    ESP_LOGE(TAG, "deinit controller failed: %s", esp_err_to_name(ret));
    return;
    }

  m_service_running = false;
  }

bool esp32bluetooth::IsServiceRunning()
  {
  return m_service_running;
  }

void esp32bluetooth::SetPowerMode(PowerMode powermode)
  {
  m_powermode = powermode;
  switch (powermode)
    {
    case On:
      if (!m_service_running) StartService();
      break;
    case Sleep:
    case DeepSleep:
    case Off:
      if (m_service_running) StopService();
      break;
    default:
      break;
    };
  }

void bluetooth_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  esp32bluetooth *me = MyPeripherals->m_esp32bluetooth;

  if (!me->IsServiceRunning())
    {
    writer->puts("Bluetooth service is not running");
    return;
    }

  int nclients = esp_ble_get_bond_device_num();
  esp_ble_bond_dev_t *clist = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * nclients);
  esp_ble_get_bond_device_list(&nclients, clist);
  writer->printf("Bluetooth service is running (%d client(s) registered)\n",nclients);
  for (int k = 0; k < nclients; k++)
    {
    writer->printf("  %02x:%02x:%02x:%02x:%02x:%02x\n",
      clist[k].bd_addr[0],
      clist[k].bd_addr[1],
      clist[k].bd_addr[2],
      clist[k].bd_addr[3],
      clist[k].bd_addr[4],
      clist[k].bd_addr[5]);
    }

  free(clist);
  }

void bluetooth_clear(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  esp32bluetooth *me = MyPeripherals->m_esp32bluetooth;

  if (!me->IsServiceRunning())
    {
    writer->puts("Error: Bluetooth service is not running");
    return;
    }

  int nclients = esp_ble_get_bond_device_num();
  esp_ble_bond_dev_t *clist = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * nclients);
  esp_ble_get_bond_device_list(&nclients, clist);
  for (int k = 0; k < nclients; k++)
    {
    char bta[19];
    sprintf(bta,"%02x:%02x:%02x:%02x:%02x:%02x",
      clist[k].bd_addr[0],
      clist[k].bd_addr[1],
      clist[k].bd_addr[2],
      clist[k].bd_addr[3],
      clist[k].bd_addr[4],
      clist[k].bd_addr[5]);
    if ((argc==0)||(strcasecmp(bta,argv[0])==0))
      {
      writer->printf("Removing Registration: %s\n",bta);
      esp_ble_remove_bond_device(clist[k].bd_addr);
      }
    }

  free(clist);
  }

class esp32bluetoothInit
  {
  public: esp32bluetoothInit();
  } esp32bluetoothInit  __attribute__ ((init_priority (8010)));

esp32bluetoothInit::esp32bluetoothInit()
  {
  ESP_LOGI(TAG, "Initialising ESP32 BLUETOOTH (8010)");

  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

  OvmsCommand* cmd_bt = MyCommandApp.RegisterCommand("bt","BLUETOOTH framework", bluetooth_status, "", 0, 1, true);
  cmd_bt->RegisterCommand("status","Show bluetooth status",bluetooth_status, "", 0, 0, true);
  cmd_bt->RegisterCommand("clear","Clear bluetooth registrations",bluetooth_clear, "(<mac>)", 0, 1, true);
  }
