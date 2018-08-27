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
#include "esp32bluetooth_gap.h"
#include "ovms_events.h"

#include "ovms_log.h"
static const char *TAG = "bt-gap";

esp32bluetoothGAP MyBluetoothGAP __attribute__ ((init_priority (8011)));

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

static uint8_t ovms_manufacturer[4]={'O', 'V', 'M', 'S'};
static uint8_t ovms_sec_service_uuid[16] = // LSB ... MSB
  { 0x76, 0x5d, 0xf7, 0x40, 0xcc, 0x70, 0x0a, 0xb6, 0xbb, 0x43, 0x08, 0x30, 0x38, 0xcb, 0x2f, 0xe7 };

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

void ble_gap_event_handler(esp_gap_ble_cb_event_t event,
                           esp_ble_gap_cb_param_t *param)
  {
  MyBluetoothGAP.EventHandler(event, param);
  }

esp32bluetoothGAP::esp32bluetoothGAP()
  {
  ESP_LOGI(TAG, "Initialising Bluetooth GAP (8011)");

  m_adv_config_done = 0;

  memset(&m_adv_config,0,sizeof(m_adv_config));
  m_adv_config.set_scan_rsp = false;
  m_adv_config.include_txpower = true;
  m_adv_config.min_interval = 0x100;
  m_adv_config.max_interval = 0x100;
  m_adv_config.appearance = 0x00;
  m_adv_config.manufacturer_len = 0;
  m_adv_config.p_manufacturer_data =  NULL;
  m_adv_config.service_data_len = 0;
  m_adv_config.p_service_data = NULL;
  m_adv_config.service_uuid_len = sizeof(ovms_sec_service_uuid);
  m_adv_config.p_service_uuid = ovms_sec_service_uuid;
  m_adv_config.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

  memset(&m_scan_rsp_config,0,sizeof(m_scan_rsp_config));
  m_scan_rsp_config.set_scan_rsp = true;
  m_scan_rsp_config.include_name = true;
  m_scan_rsp_config.manufacturer_len = sizeof(ovms_manufacturer);
  m_scan_rsp_config.p_manufacturer_data = ovms_manufacturer;

  memset(&m_adv_params,0,sizeof(m_adv_params));
  m_adv_params.adv_int_min        = 0x100;
  m_adv_params.adv_int_max        = 0x100;
  m_adv_params.adv_type           = ADV_TYPE_IND;
  m_adv_params.own_addr_type      = BLE_ADDR_TYPE_RANDOM;
  m_adv_params.channel_map        = ADV_CHNL_ALL;
  m_adv_params.adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  }

esp32bluetoothGAP::~esp32bluetoothGAP()
  {
  }

void esp32bluetoothGAP::StartAdvertising()
  {
  ESP_LOGI(TAG,"Starting advertising");
  esp_ble_gap_start_advertising(&m_adv_params);
  }

void esp32bluetoothGAP::RegisterForEvents()
  {
  esp_err_t ret = esp_ble_gap_register_callback(ble_gap_event_handler);
  if (ret)
    {
    ESP_LOGE(TAG, "gap register error, error code = %x", ret);
    return;
    }
  }

void esp32bluetoothGAP::EventHandler(esp_gap_ble_cb_event_t event,
                                     esp_ble_gap_cb_param_t *param)
  {
  switch (event)
    {
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
      ESP_LOGI(TAG,"ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT");
      m_adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
      if (m_adv_config_done == 0)
        {
        StartAdvertising();
        }
      break;
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
      ESP_LOGI(TAG,"ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT");
      m_adv_config_done &= (~ADV_CONFIG_FLAG);
      if (m_adv_config_done == 0)
        {
        StartAdvertising();
        }
      break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      //advertising start complete event to indicate advertising start successfully or failed
      if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
        ESP_LOGE(TAG, "ESP_GAP_BLE_ADV_START_COMPLETE_EVT advertising start failed, error status = %x", param->adv_start_cmpl.status);
        }
      else
        {
        ESP_LOGI(TAG, "ESP_GAP_BLE_ADV_START_COMPLETE_EVT advertising start success");
        }
      break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
        ESP_LOGE(TAG, "ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT Advertising stop failed\n");
        }
      else
        {
        ESP_LOGI(TAG, "ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT Stop adv successfully\n");
        }
      break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
      ESP_LOGI(TAG, "ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
              param->update_conn_params.status,
              param->update_conn_params.min_int,
              param->update_conn_params.max_int,
              param->update_conn_params.conn_int,
              param->update_conn_params.latency,
              param->update_conn_params.timeout);
      break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:                           /* passkey request event */
      ESP_LOGI(TAG, "ESP_GAP_BLE_PASSKEY_REQ_EVT");
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
      ESP_LOGI(TAG, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number: %d", param->ble_security.key_notif.passkey);
      break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
      /* send the positive(true) security response to the peer device to accept the security request.
      If not accept the security request, should sent the security response with negative(false) accept value*/
      ESP_LOGI(TAG,"ESP_GAP_BLE_SEC_REQ_EVT");
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:  ///the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
      ///show the passkey number to the user to input it in the peer deivce.
      ESP_LOGI(TAG, "ESP_GAP_BLE_PASSKEY_NOTIF_EVT passkey: %d", param->ble_security.key_notif.passkey);
      break;
    case ESP_GAP_BLE_KEY_EVT:
      //shows the ble key info share with peer device to the user.
      ESP_LOGI(TAG, "ESP_GAP_BLE_KEY_EVT key type = %s", esp_key_type_to_str(param->ble_security.ble_key.key_type));
      break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      {
      esp_bd_addr_t bd_addr;
      memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
      ESP_LOGI(TAG, "ESP_GAP_BLE_AUTH_CMPL_EVT remote BD_ADDR: %02x:%02x:%02x:%02x:%02x:%02x",
              bd_addr[0], bd_addr[1], bd_addr[2],
              bd_addr[3], bd_addr[4], bd_addr[5]);
      ESP_LOGI(TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
      ESP_LOGI(TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");
      if (param->ble_security.auth_cmpl.success)
        {
        char signal[32];
        sprintf(signal,"bt.auth.%02x:%02x:%02x:%02x:%02x:%02x",
          bd_addr[0],
          bd_addr[1],
          bd_addr[2],
          bd_addr[3],
          bd_addr[4],
          bd_addr[5]);
        MyEvents.SignalEvent(signal, NULL);
        }
      break;
      }
    case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:
      {
      ESP_LOGI(TAG, "ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT status = %d", param->remove_bond_dev_cmpl.status);
      esp_log_buffer_hex(TAG, (void *)param->remove_bond_dev_cmpl.bd_addr, sizeof(esp_bd_addr_t));
      break;
      }
    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:
      {
      if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
        ESP_LOGE(TAG, "ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT config local privacy failed, error status = %x", param->local_privacy_cmpl.status);
        break;
        }

      esp_err_t ret = esp_ble_gap_config_adv_data(&m_adv_config);
      if (ret)
        {
        ESP_LOGE(TAG, "ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT config adv data failed, error code = %x", ret);
        }
      else
        {
        m_adv_config_done |= ADV_CONFIG_FLAG;
        }

      ret = esp_ble_gap_config_adv_data(&m_scan_rsp_config);
      if (ret)
        {
        ESP_LOGE(TAG, "ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT config adv data failed, error code = %x", ret);
        }
      else
        {
        ESP_LOGI(TAG, "ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT config done");
        m_adv_config_done |= SCAN_RSP_CONFIG_FLAG;
        }
        break;
      }
    default:
      ESP_LOGD(TAG, "GAP_EVT, event %d\n", event);
      break;
    }
  }
