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
#include "esp32bluetooth_gatts.h"
#include "ovms_config.h"
#include "ovms_events.h"

#include "ovms_log.h"
static const char *TAG = "bt-gatts";

esp32bluetoothGATTS MyBluetoothGATTS __attribute__ ((init_priority (8012)));

////////////////////////////////////////////////////////////////////////
// esp32bluetoothApp

esp32bluetoothApp::esp32bluetoothApp(const char* name)
  {
  m_name = name;
  m_gatts_if = ESP_GATT_IF_NONE;
  m_app_id = 0;
  m_conn_id = 0;
  m_service_handle = 0;
  memset(&m_service_id, 0, sizeof(m_service_id));
  m_char_handle = 0;
  memset(&m_char_uuid, 0, sizeof(m_char_uuid));
  m_perm = 0;
  m_property = 0;
  m_descr_handle = 0;
  memset(&m_descr_uuid, 0, sizeof(m_descr_uuid));
  }

esp32bluetoothApp::~esp32bluetoothApp()
  {
  }

void esp32bluetoothApp::EventRegistered(esp_ble_gatts_cb_param_t::gatts_reg_evt_param *reg)
  {
  }

void esp32bluetoothApp::EventRead(esp_ble_gatts_cb_param_t::gatts_read_evt_param *read)
  {
  }

void esp32bluetoothApp::EventWrite(esp_ble_gatts_cb_param_t::gatts_write_evt_param *write)
  {
  }

void esp32bluetoothApp::EventExecWrite(esp_ble_gatts_cb_param_t::gatts_exec_write_evt_param *execwrite)
  {
  }

void esp32bluetoothApp::EventMTU(esp_ble_gatts_cb_param_t::gatts_mtu_evt_param *mtu)
  {
  }

void esp32bluetoothApp::EventConf(esp_ble_gatts_cb_param_t::gatts_conf_evt_param *conf)
  {
  }

void esp32bluetoothApp::EventUnregistered()
  {
  }

void esp32bluetoothApp::EventDelete(esp_ble_gatts_cb_param_t::gatts_delete_evt_param *del)
  {
  }

void esp32bluetoothApp::EventStart(esp_ble_gatts_cb_param_t::gatts_start_evt_param *start)
  {
  }

void esp32bluetoothApp::EventStop(esp_ble_gatts_cb_param_t::gatts_stop_evt_param *stop)
  {
  }

void esp32bluetoothApp::EventConnect(esp_ble_gatts_cb_param_t::gatts_connect_evt_param *connect)
  {
  }

void esp32bluetoothApp::EventDisconnect(esp_ble_gatts_cb_param_t::gatts_disconnect_evt_param *disconnect)
  {
  }

void esp32bluetoothApp::EventOpen(esp_ble_gatts_cb_param_t::gatts_open_evt_param *open)
  {
  }

void esp32bluetoothApp::EventCancelOpen(esp_ble_gatts_cb_param_t::gatts_cancel_open_evt_param *cancelopen)
  {
  }

void esp32bluetoothApp::EventClose(esp_ble_gatts_cb_param_t::gatts_close_evt_param *close)
  {
  }

void esp32bluetoothApp::EventListen()
  {
  }

void esp32bluetoothApp::EventCongest(esp_ble_gatts_cb_param_t::gatts_congest_evt_param *congest)
  {
  }

void esp32bluetoothApp::EventCreate(esp_ble_gatts_cb_param_t::gatts_add_attr_tab_evt_param *attrtab)
  {
  }

void esp32bluetoothApp::EventAddChar(esp_ble_gatts_cb_param_t::gatts_add_char_evt_param *addchar)
  {
  }

void esp32bluetoothApp::EventAddCharDescr(esp_ble_gatts_cb_param_t::gatts_add_char_descr_evt_param *adddescr)
  {
  }



////////////////////////////////////////////////////////////////////////
// esp32bluetoothGATTS

void ble_gatts_event_handler(esp_gatts_cb_event_t event,
                             esp_gatt_if_t gatts_if,
                             esp_ble_gatts_cb_param_t *param)
  {
  MyBluetoothGATTS.EventHandler(event, gatts_if, param);
  }

esp32bluetoothGATTS::esp32bluetoothGATTS()
  {
  ESP_LOGI(TAG, "Initialising Bluetooth GATTS (8012)");
  }

esp32bluetoothGATTS::~esp32bluetoothGATTS()
  {
  }

void esp32bluetoothGATTS::RegisterForEvents()
  {
  esp_err_t ret = esp_ble_gatts_register_callback(ble_gatts_event_handler);
  if (ret)
    {
    ESP_LOGE(TAG, "gatts register error, error code = %x", ret);
    return;
    }
  }

void esp32bluetoothGATTS::EventHandler(esp_gatts_cb_event_t event,
                                       esp_gatt_if_t gatts_if,
                                       esp_ble_gatts_cb_param_t *param)
  {
  for (esp32bluetoothApp* app : m_apps)
    {
    if (event == ESP_GATTS_REG_EVT)
      {
      // Store the gatts_if for the registered app
      if (param->reg.app_id == app->m_app_id)
        {
        if (param->reg.status == ESP_GATT_OK)
          {
          ESP_LOGI(TAG,"ESP_GATTS_REG_EVT register app %s (0x%04x) ok with interface ID %d",
                   app->m_name, param->reg.app_id, gatts_if);
          app->m_gatts_if = gatts_if;
          }
        else
          {
          ESP_LOGE(TAG, "ESP_GATTS_REG_EVT register app %s (0x%04x) failed with status %d",
                   app->m_name,
                   param->reg.app_id,
                   param->reg.status);
          return;
          }
        }
      }
    if ((gatts_if == ESP_GATT_IF_NONE) ||
        (gatts_if == app->m_gatts_if))
      {
      switch (event)
        {
        case ESP_GATTS_REG_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_REG_EVT/%s",app->m_name);
          app->EventRegistered(&param->reg);
          break;
        case ESP_GATTS_READ_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_READ_EVT/%s conn_id %d, trans_id %d, handle %d",
            app->m_name,
            param->read.conn_id, param->read.trans_id, param->read.handle);
          app->EventRead(&param->read);
          break;
        case ESP_GATTS_WRITE_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_WRITE_EVT/%s write %d bytes, value:",
            app->m_name, param->write.len);
          esp_log_buffer_hex(TAG, param->write.value, param->write.len);
          app->EventWrite(&param->write);
          break;
        case ESP_GATTS_EXEC_WRITE_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_EXEC_WRITE_EVT/%s",app->m_name);
          app->EventExecWrite(&param->exec_write);
          break;
        case ESP_GATTS_MTU_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT/%s",app->m_name);
          app->EventMTU(&param->mtu);
          break;
        case ESP_GATTS_CONF_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT/%s",app->m_name);
          app->EventConf(&param->conf);
          break;
        case ESP_GATTS_UNREG_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_UNREG_EVT/%s",app->m_name);
          app->EventUnregistered();
          break;
        case ESP_GATTS_DELETE_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_DELETE_EVT/%s",app->m_name);
          app->EventDelete(&param->del);
          break;
        case ESP_GATTS_START_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_START_EVT/%s",app->m_name);
          app->EventStart(&param->start);
          break;
        case ESP_GATTS_STOP_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_STOP_EVT/%s",app->m_name);
          app->EventStop(&param->stop);
          break;
        case ESP_GATTS_CONNECT_EVT:
          {
          ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT/%s conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x",
            app->m_name,
            param->connect.conn_id,
            param->connect.remote_bda[0],
            param->connect.remote_bda[1],
            param->connect.remote_bda[2],
            param->connect.remote_bda[3],
            param->connect.remote_bda[4],
            param->connect.remote_bda[5]);

          app->EventConnect(&param->connect);

          char signal[32];
          sprintf(signal,"bt.connect.%02x:%02x:%02x:%02x:%02x:%02x",
            param->connect.remote_bda[0],
            param->connect.remote_bda[1],
            param->connect.remote_bda[2],
            param->connect.remote_bda[3],
            param->connect.remote_bda[4],
            param->connect.remote_bda[5]);
          MyEvents.SignalEvent(signal, NULL);

          app->m_conn_id = param->connect.conn_id;

          if (app->m_app_slot == 0)
            {
            esp_ble_conn_update_params_t conn_params;
            memset(&conn_params,0,sizeof(conn_params));
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));

            /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x30;    // max_int = 0x30*1.25ms = 40ms
            conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
            conn_params.timeout = 400;     // timeout = 400*10ms = 4000ms

            //start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
            esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
            }
          break;
          }
        case ESP_GATTS_DISCONNECT_EVT:
          {
          ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT/%s conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x",
            app->m_name,
            param->disconnect.conn_id,
            param->disconnect.remote_bda[0],
            param->disconnect.remote_bda[1],
            param->disconnect.remote_bda[2],
            param->disconnect.remote_bda[3],
            param->disconnect.remote_bda[4],
            param->disconnect.remote_bda[5]);

          app->EventDisconnect(&param->disconnect);

          char signal[32];
          sprintf(signal,"bt.disconnect.%02x:%02x:%02x:%02x:%02x:%02x",
            param->disconnect.remote_bda[0],
            param->disconnect.remote_bda[1],
            param->disconnect.remote_bda[2],
            param->disconnect.remote_bda[3],
            param->disconnect.remote_bda[4],
            param->disconnect.remote_bda[5]);
          MyEvents.SignalEvent(signal, NULL);

          if (app->m_app_slot == 0)
            {
            MyBluetoothGAP.StartAdvertising();
            }
          break;
          }
        case ESP_GATTS_OPEN_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_OPEN_EVT/%s",app->m_name);
          app->EventOpen(&param->open);
          break;
        case ESP_GATTS_CANCEL_OPEN_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_CANCEL_OPEN_EVT/%s",app->m_name);
          app->EventCancelOpen(&param->cancel_open);
          break;
        case ESP_GATTS_CLOSE_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_CLOSE_EVT/%s",app->m_name);
          app->EventClose(&param->close);
          break;
        case ESP_GATTS_LISTEN_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_LISTEN_EVT/%s",app->m_name);
          app->EventListen();
          break;
        case ESP_GATTS_CONGEST_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_CONGEST_EVT/%s",app->m_name);
          app->EventCongest(&param->congest);
          break;
        case ESP_GATTS_CREATE_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_CREATE_EVT/%s status %d, service_handle %d",
            app->m_name,
            param->create.status, param->create.service_handle);
          app->m_service_handle = param->create.service_handle;
          app->EventCreate(&param->add_attr_tab);
          break;
        case ESP_GATTS_ADD_CHAR_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_ADD_CHAR_EVT/%s status %d,  attr_handle %d, service_handle %d",
            app->m_name,
            param->add_char.status,
            param->add_char.attr_handle,
            param->add_char.service_handle);
          app->m_char_handle = param->add_char.attr_handle;
          app->EventAddChar(&param->add_char);
          break;
        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
          ESP_LOGI(TAG, "ESP_GATTS_ADD_CHAR_DESCR_EVT/%s status %d, attr_handle %d, service_handle %d",
            app->m_name,
            param->add_char.status, param->add_char.attr_handle,
            param->add_char.service_handle);
          app->EventAddCharDescr(&param->add_char_descr);
          break;
        default:
          ESP_LOGD(TAG, "GATTS PROFILE event = %d\n",event);
          break;
        }
      }
    }
  }

void esp32bluetoothGATTS::RegisterAllApps()
  {
  for (esp32bluetoothApp* app : m_apps)
    {
    esp_err_t ret = esp_ble_gatts_app_register(app->m_app_id);
    if (ret)
      {
      ESP_LOGE(TAG, "App %s register error, error code = %x",
        app->m_name, ret);
      }
    else
      {
      ESP_LOGI(TAG,"App %s registered successfully",app->m_name);
      }
    }
  }

void esp32bluetoothGATTS::UnregisterAllApps()
  {
  for (esp32bluetoothApp* app : m_apps)
    {
    esp_err_t ret = esp_ble_gatts_app_unregister(app->m_gatts_if);
    if (ret)
      {
      ESP_LOGE(TAG, "App %s unregister error, error code = %x",
        app->m_name, ret);
      }
    else
      {
      ESP_LOGI(TAG,"App %s unregistered successfully",app->m_name);
      }
    }
  }

void esp32bluetoothGATTS::RegisterApp(esp32bluetoothApp* app)
  {
  m_apps.push_back(app);
  app->m_app_slot = m_apps.size()-1;
  }
