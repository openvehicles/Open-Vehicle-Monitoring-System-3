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

#ifndef __ESP32BLUETOOTH_GATTS_H__
#define __ESP32BLUETOOTH_GATTS_H__

#include <vector>
#include "esp32bluetooth.h"

class esp32bluetoothApp;

class esp32bluetoothCharacteristic
  {
  public:
    esp32bluetoothCharacteristic(esp32bluetoothApp* app,
      esp_bt_uuid_t* charuuid,
      esp_bt_uuid_t* descruuid);
    ~esp32bluetoothCharacteristic();

  public:
    esp32bluetoothApp* m_app;

    esp_bt_uuid_t m_char_uuid;
    uint16_t m_char_handle;
    esp_gatt_perm_t m_char_perm;
    esp_gatt_char_prop_t m_char_property;

    uint16_t m_descr_handle;
    esp_bt_uuid_t m_descr_uuid;

    bool m_notifying;
    bool m_indicating;
  };

class esp32bluetoothApp
  {
  public:
    esp32bluetoothApp(const char* name);
    ~esp32bluetoothApp();

  public:
    virtual void EventRegistered(esp_ble_gatts_cb_param_t::gatts_reg_evt_param *reg);
    virtual void EventRead(esp_ble_gatts_cb_param_t::gatts_read_evt_param *read);
    virtual void EventWrite(esp_ble_gatts_cb_param_t::gatts_write_evt_param *write);
    virtual void EventExecWrite(esp_ble_gatts_cb_param_t::gatts_exec_write_evt_param *execwrite);
    virtual void EventMTU(esp_ble_gatts_cb_param_t::gatts_mtu_evt_param *mtu);
    virtual void EventConf(esp_ble_gatts_cb_param_t::gatts_conf_evt_param *conf);
    virtual void EventUnregistered();
    virtual void EventDelete(esp_ble_gatts_cb_param_t::gatts_delete_evt_param *del);
    virtual void EventStart(esp_ble_gatts_cb_param_t::gatts_start_evt_param *start);
    virtual void EventStop(esp_ble_gatts_cb_param_t::gatts_stop_evt_param *stop);
    virtual void EventConnect(esp_ble_gatts_cb_param_t::gatts_connect_evt_param *connect);
    virtual void EventDisconnect(esp_ble_gatts_cb_param_t::gatts_disconnect_evt_param *disconnect);
    virtual void EventOpen(esp_ble_gatts_cb_param_t::gatts_open_evt_param *open);
    virtual void EventCancelOpen(esp_ble_gatts_cb_param_t::gatts_cancel_open_evt_param *cancelopen);
    virtual void EventClose(esp_ble_gatts_cb_param_t::gatts_close_evt_param *close);
    virtual void EventListen();
    virtual void EventCongest(esp_ble_gatts_cb_param_t::gatts_congest_evt_param *congest);
    virtual void EventCreate(esp_ble_gatts_cb_param_t::gatts_add_attr_tab_evt_param *attrtab);
    virtual void EventAddChar(esp_ble_gatts_cb_param_t::gatts_add_char_evt_param *addchar);
    virtual void EventAddCharDescr(esp_ble_gatts_cb_param_t::gatts_add_char_descr_evt_param *adddescr);

  public:
    const char* m_name;
    uint16_t m_app_slot;
    esp_gatt_if_t m_gatts_if;
    uint16_t m_app_id;
    uint16_t m_conn_id;
    uint16_t m_service_handle;
    esp_gatt_srvc_id_t m_service_id;
    uint16_t m_mtu;
  };

class esp32bluetoothGATTS
  {
  public:
    esp32bluetoothGATTS();
    ~esp32bluetoothGATTS();

  public:
    void RegisterForEvents();
    void EventHandler(esp_gatts_cb_event_t event,
                      esp_gatt_if_t gatts_if,
                      esp_ble_gatts_cb_param_t *param);
    void RegisterAllApps();
    void UnregisterAllApps();

  public:
    void RegisterApp(esp32bluetoothApp* app);

  private:
    std::vector<esp32bluetoothApp*> m_apps;
  };

extern esp32bluetoothGATTS MyBluetoothGATTS;

#endif //#ifndef __ESP32BLUETOOTH_GAP_H__
