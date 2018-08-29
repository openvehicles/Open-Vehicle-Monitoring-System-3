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

#ifndef __ESP32BLUETOOTH_SVC_CONSOLE_H__
#define __ESP32BLUETOOTH_SVC_CONSOLE_H__

#include "esp32bluetooth.h"
#include "esp32bluetooth_gatts.h"

#define GATTS_APP_UUID_OVMS_CONSOLE     0x20
#define GATTS_SERVICE_UUID_OVMS_CONSOLE 0xffe1
#define GATTS_CHAR_UUID_OVMS_CONSOLE    0xffe1
#define GATTS_DESCR_UUID_OVMS_CONSOLE   0xffe2
#define GATTS_NUM_HANDLE_OVMS_CONSOLE   4

class OvmsBluetoothAppConsole : public esp32bluetoothApp
  {
  public:
    OvmsBluetoothAppConsole();
    ~OvmsBluetoothAppConsole();

  public:
    void DataFromConsole(uint8_t* data, size_t len);
    void EventRegistered(esp_ble_gatts_cb_param_t::gatts_reg_evt_param *reg);
    void EventCreate(esp_ble_gatts_cb_param_t::gatts_add_attr_tab_evt_param *attrtab);
    void EventDisconnect(esp_ble_gatts_cb_param_t::gatts_disconnect_evt_param *disconnect);
    void EventAddChar(esp_ble_gatts_cb_param_t::gatts_add_char_evt_param *addchar);
    void EventAddCharDescr(esp_ble_gatts_cb_param_t::gatts_add_char_descr_evt_param *adddescr);
    void EventRead(esp_ble_gatts_cb_param_t::gatts_read_evt_param *read);
    void EventWrite(esp_ble_gatts_cb_param_t::gatts_write_evt_param *write);
    void EventExecWrite(esp_ble_gatts_cb_param_t::gatts_exec_write_evt_param *execwrite);

  private:
    uint16_t m_char_handle;
    esp_bt_uuid_t m_char_uuid;
    esp_gatt_perm_t m_perm;
    esp_gatt_char_prop_t m_property;
    uint16_t m_descr_handle;
    esp_bt_uuid_t m_descr_uuid;
    bool m_notifying;
  };

#endif //#ifndef __ESP32BLUETOOTH_SVC_CONSOLE_H__
