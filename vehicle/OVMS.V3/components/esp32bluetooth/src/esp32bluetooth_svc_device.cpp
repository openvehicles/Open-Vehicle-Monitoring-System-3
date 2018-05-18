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
#include "esp32bluetooth_svc_device.h"
#include "esp32bluetooth_gap.h"
#include "ovms_config.h"
#include "ovms_events.h"

#include "ovms_log.h"
static const char *TAG = "bt-svc-device";

struct gatts_profile_inst ovms_gatts_profile_device;

// Demo property (just for testing)
static uint8_t char1_str[] = {'O','V','M','S'};
static esp_gatt_char_prop_t a_property = 0;

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40
static esp_attr_value_t gatts_demo_char1_val =
{
    .attr_max_len = GATTS_DEMO_CHAR_VAL_LEN_MAX,
    .attr_len     = sizeof(char1_str),
    .attr_value   = char1_str,
};
// End of demo property

void ovms_ble_gatts_profile_device_event_handler(esp_gatts_cb_event_t event,
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
      ovms_ble_gap_config_advertising();

      ovms_gatts_profile_device.service_id.is_primary = true;
      ovms_gatts_profile_device.service_id.id.inst_id = 0x00;
      ovms_gatts_profile_device.service_id.id.uuid.len = ESP_UUID_LEN_16;
      ovms_gatts_profile_device.service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_OVMS_DEVICE;

      ESP_LOGI(TAG,"Creating service on interface %d",gatts_if);
      esp_ble_gatts_create_service(gatts_if, &ovms_gatts_profile_device.service_id, GATTS_NUM_HANDLE_OVMS_DEVICE);
      }
      break;
    case ESP_GATTS_READ_EVT:
      {
      ESP_LOGI(TAG, "GATT_READ_EVT, conn_id %d, trans_id %d, handle %d\n",
      param->read.conn_id, param->read.trans_id, param->read.handle);
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
      ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x",
             param->connect.conn_id,
             param->connect.remote_bda[0],
             param->connect.remote_bda[1],
             param->connect.remote_bda[2],
             param->connect.remote_bda[3],
             param->connect.remote_bda[4],
             param->connect.remote_bda[5]);

      char signal[32];
      sprintf(signal,"bt.connect.%02x:%02x:%02x:%02x:%02x:%02x",
        param->connect.remote_bda[0],
        param->connect.remote_bda[1],
        param->connect.remote_bda[2],
        param->connect.remote_bda[3],
        param->connect.remote_bda[4],
        param->connect.remote_bda[5]);
      MyEvents.SignalEvent(signal, NULL);

      esp_ble_conn_update_params_t conn_params;
      memset(&conn_params,0,sizeof(conn_params));
      memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
      /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
      conn_params.latency = 0;
      conn_params.max_int = 0x30;    // max_int = 0x30*1.25ms = 40ms
      conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
      conn_params.timeout = 400;     // timeout = 400*10ms = 4000ms
      ovms_gatts_profile_device.conn_id = param->connect.conn_id;
      //start sent the update connection parameters to the peer device.
      esp_ble_gap_update_conn_params(&conn_params);
      esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
      break;
      }
    case ESP_GATTS_DISCONNECT_EVT:
      {
      ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x",
             param->disconnect.conn_id,
             param->disconnect.remote_bda[0],
             param->disconnect.remote_bda[1],
             param->disconnect.remote_bda[2],
             param->disconnect.remote_bda[3],
             param->disconnect.remote_bda[4],
             param->disconnect.remote_bda[5]);

      char signal[32];
      sprintf(signal,"bt.disconnect.%02x:%02x:%02x:%02x:%02x:%02x",
        param->disconnect.remote_bda[0],
        param->disconnect.remote_bda[1],
        param->disconnect.remote_bda[2],
        param->disconnect.remote_bda[3],
        param->disconnect.remote_bda[4],
        param->disconnect.remote_bda[5]);
      MyEvents.SignalEvent(signal, NULL);

      /* start advertising again when missing the connect */
      ovms_ble_gap_start_advertising();
      break;
      }
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
      ovms_gatts_profile_device.service_handle = param->create.service_handle;
      ovms_gatts_profile_device.char_uuid.len = ESP_UUID_LEN_16;
      ovms_gatts_profile_device.char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_OVMS_DEVICE;
      esp_ble_gatts_start_service(ovms_gatts_profile_device.service_handle);

      a_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
      esp_err_t add_char_ret =
      esp_ble_gatts_add_char(ovms_gatts_profile_device.service_handle,
                            &ovms_gatts_profile_device.char_uuid,
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
      ovms_gatts_profile_device.char_handle = param->add_char.attr_handle;
      ovms_gatts_profile_device.descr_uuid.len = ESP_UUID_LEN_16;
      ovms_gatts_profile_device.descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
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
                             ovms_gatts_profile_device.service_handle,
                             &ovms_gatts_profile_device.descr_uuid,
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

void ovms_ble_gatts_profile_device_init()
  {
  memset(&ovms_gatts_profile_device,0,sizeof(ovms_gatts_profile_device));
  ovms_gatts_profile_device.gatts_cb = ovms_ble_gatts_profile_device_event_handler;
  ovms_gatts_profile_device.gatts_if = ESP_GATT_IF_NONE;
  }
