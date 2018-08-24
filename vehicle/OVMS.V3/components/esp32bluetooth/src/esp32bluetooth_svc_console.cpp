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
#include "esp32bluetooth_svc_console.h"
#include "esp32bluetooth_gap.h"
#include "ovms_config.h"
#include "ovms_events.h"

#include "ovms_log.h"
static const char *TAG = "bt-svc-console";

struct gatts_profile_inst ovms_gatts_profile_console;

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

void ovms_ble_gatts_profile_console_event_handler(esp_gatts_cb_event_t event,
                     esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
  {
  switch (event)
    {
    case ESP_GATTS_REG_EVT:
      {
      ovms_gatts_profile_console.service_id.is_primary = true;
      ovms_gatts_profile_console.service_id.id.inst_id = 0x00;
      ovms_gatts_profile_console.service_id.id.uuid.len = ESP_UUID_LEN_16;
      ovms_gatts_profile_console.service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_OVMS_CONSOLE;
      ESP_LOGI(TAG,"ESP_GATTS_REG_EVT Creating service %04x on interface %d",
        ovms_gatts_profile_console.service_id.id.uuid.uuid.uuid16,
        gatts_if);
      esp_ble_gatts_create_service(gatts_if, &ovms_gatts_profile_console.service_id, GATTS_NUM_HANDLE_OVMS_CONSOLE);
      }
      break;
    case ESP_GATTS_READ_EVT:
      {
      ESP_LOGI(TAG, "ESP_GATTS_READ_EVT, conn_id %d, trans_id %d, handle %d\n",
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
      ESP_LOGI(TAG, "ESP_GATTS_WRITE_EVT, write %d bytes, value:",param->write.len);
      esp_log_buffer_hex(TAG, param->write.value, param->write.len);
      break;
    case ESP_GATTS_EXEC_WRITE_EVT:
      ESP_LOGI(TAG,"ESP_GATTS_EXEC_WRITE_EVT");
      break;
    case ESP_GATTS_MTU_EVT:
      ESP_LOGI(TAG,"ESP_GATTS_MTU_EVT");
      break;
    case ESP_GATTS_CONF_EVT:
      ESP_LOGI(TAG,"ESP_GATTS_CONF_EVT");
      break;
    case ESP_GATTS_UNREG_EVT:
      ESP_LOGI(TAG,"ESP_GATTS_UNREG_EVT");
      break;
    case ESP_GATTS_DELETE_EVT:
      ESP_LOGI(TAG,"ESP_GATTS_DELETE_EVT");
      break;
    case ESP_GATTS_START_EVT:
      ESP_LOGI(TAG,"ESP_GATTS_START_EVT");
      break;
    case ESP_GATTS_STOP_EVT:
      ESP_LOGI(TAG,"ESP_GATTS_STOP_EVT");
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
      ovms_gatts_profile_console.conn_id = param->connect.conn_id;
      break;
      }
    case ESP_GATTS_DISCONNECT_EVT:
      {
      ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT");
      /* start advertising again when missing the connect */
      ovms_ble_gap_start_advertising();
      break;
      }
    case ESP_GATTS_OPEN_EVT:
      ESP_LOGI(TAG,"ESP_GATTS_OPEN_EVT");
      break;
    case ESP_GATTS_CANCEL_OPEN_EVT:
      ESP_LOGI(TAG,"ESP_GATTS_CANCEL_OPEN_EVT");
      break;
    case ESP_GATTS_CLOSE_EVT:
      ESP_LOGI(TAG,"ESP_GATTS_CLOSE_EVT");
      break;
    case ESP_GATTS_LISTEN_EVT:
      ESP_LOGI(TAG,"ESP_GATTS_LISTEN_EVT");
      break;
    case ESP_GATTS_CONGEST_EVT:
      ESP_LOGI(TAG,"ESP_GATTS_CONGEST_EVT");
      break;
    case ESP_GATTS_CREATE_EVT:
      {
      ESP_LOGI(TAG, "ESP_GATTS_CREATE_EVT, status %d, service_handle %d\n", param->create.status, param->create.service_handle);
      ovms_gatts_profile_console.service_handle = param->create.service_handle;
      ovms_gatts_profile_console.char_uuid.len = ESP_UUID_LEN_16;
      ovms_gatts_profile_console.char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_OVMS_CONSOLE;

      a_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
      esp_err_t add_char_ret =
      esp_ble_gatts_add_char(ovms_gatts_profile_console.service_handle,
                            &ovms_gatts_profile_console.char_uuid,
                            ESP_GATT_PERM_READ,
                            a_property,
                            &gatts_demo_char1_val,
                            NULL);
      if (add_char_ret)
        {
        ESP_LOGE(TAG, "add char failed, error code =%x",add_char_ret);
        }

      esp_ble_gatts_start_service(ovms_gatts_profile_console.service_handle);
      break;
      }
    case ESP_GATTS_ADD_CHAR_EVT:
      {
      uint16_t length = 0;
      const uint8_t *prf_char;

      ESP_LOGI(TAG, "ADD_CHAR_EVT, status %d,  attr_handle %d, service_handle %d\n",
             param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
      ovms_gatts_profile_console.char_handle = param->add_char.attr_handle;
      ovms_gatts_profile_console.descr_uuid.len = ESP_UUID_LEN_16;
      ovms_gatts_profile_console.descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
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
                             ovms_gatts_profile_console.service_handle,
                             &ovms_gatts_profile_console.descr_uuid,
                             ESP_GATT_PERM_READ,
                             NULL,NULL);
      if (add_descr_ret)
        {
        ESP_LOGE(TAG, "add char descr failed, error code = %x", add_descr_ret);
        }
      break;
      }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
      ESP_LOGI(TAG, "ESP_GATTS_ADD_CHAR_DESCR_EVT, status %d, attr_handle %d, service_handle %d\n",
              param->add_char.status, param->add_char.attr_handle,
              param->add_char.service_handle);
      break;
    default:
      ESP_LOGD(TAG, "GATTS PROFILE event = %d\n",event);
      break;
    }
  }

void ovms_ble_gatts_profile_console_init()
  {
  memset(&ovms_gatts_profile_console,0,sizeof(ovms_gatts_profile_console));
  ovms_gatts_profile_console.gatts_cb = ovms_ble_gatts_profile_console_event_handler;
  ovms_gatts_profile_console.gatts_if = ESP_GATT_IF_NONE;
  }
