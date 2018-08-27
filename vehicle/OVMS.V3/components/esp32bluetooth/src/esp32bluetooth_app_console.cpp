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
#include "esp32bluetooth_app_console.h"

#include "ovms_log.h"
static const char *TAG = "bt-app-console";

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

OvmsBluetoothAppConsole MyBluetoothAppConsole __attribute__ ((init_priority (8016)));

OvmsBluetoothAppConsole::OvmsBluetoothAppConsole()
  : esp32bluetoothApp("console")
  {
  ESP_LOGI(TAG, "Initialising Bluetooth CONSOLE App (8016)");
  m_app_id = GATTS_APP_UUID_OVMS_CONSOLE;
  MyBluetoothGATTS.RegisterApp(this);
  }

OvmsBluetoothAppConsole::~OvmsBluetoothAppConsole()
  {
  }

void OvmsBluetoothAppConsole::EventRegistered(esp_ble_gatts_cb_param_t::gatts_reg_evt_param *reg)
  {
  m_service_id.is_primary = true;
  m_service_id.id.inst_id = 0x00;
  m_service_id.id.uuid.len = ESP_UUID_LEN_16;
  m_service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_OVMS_CONSOLE;
  ESP_LOGI(TAG,"ESP_GATTS_REG_EVT Creating service %04x on interface %d",
    m_service_id.id.uuid.uuid.uuid16,
    m_gatts_if);
  esp_ble_gatts_create_service(m_gatts_if, &m_service_id, GATTS_NUM_HANDLE_OVMS_CONSOLE);
  }

void OvmsBluetoothAppConsole::EventRead(esp_ble_gatts_cb_param_t::gatts_read_evt_param *read)
  {
  esp_gatt_rsp_t rsp;
  memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
  rsp.attr_value.handle = read->handle;
  rsp.attr_value.len = 4;
  rsp.attr_value.value[0] = 0xde;
  rsp.attr_value.value[1] = 0xed;
  rsp.attr_value.value[2] = 0xbe;
  rsp.attr_value.value[3] = 0xef;
  esp_ble_gatts_send_response(m_gatts_if,
                              read->conn_id,
                              read->trans_id,
                              ESP_GATT_OK, &rsp);
  }

void OvmsBluetoothAppConsole::EventCreate(esp_ble_gatts_cb_param_t::gatts_add_attr_tab_evt_param *attrtab)
  {
  m_char_uuid.len = ESP_UUID_LEN_16;
  m_char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_OVMS_CONSOLE;
  a_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
  esp_err_t add_char_ret =
    esp_ble_gatts_add_char(m_service_handle,
                          &m_char_uuid,
                          ESP_GATT_PERM_READ_ENC_MITM,
                          a_property,
                          &gatts_demo_char1_val,
                          NULL);
  if (add_char_ret)
    {
    ESP_LOGE(TAG, "add char failed, error code =%x",add_char_ret);
    }

  esp_ble_gatts_start_service(m_service_handle);
  }

void OvmsBluetoothAppConsole::EventAddChar(esp_ble_gatts_cb_param_t::gatts_add_char_evt_param *addchar)
  {
  uint16_t length = 0;
  const uint8_t *prf_char;

  m_descr_uuid.len = ESP_UUID_LEN_16;
  m_descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
  esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(addchar->attr_handle, &length, &prf_char);
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
                         m_service_handle,
                         &m_descr_uuid,
                         ESP_GATT_PERM_READ,
                         NULL,NULL);
  if (add_descr_ret)
    {
    ESP_LOGE(TAG, "add char descr failed, error code = %x", add_descr_ret);
    }
  }
