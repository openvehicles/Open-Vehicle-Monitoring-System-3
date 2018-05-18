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
#include "esp32bluetooth_gatts.h"
#include "esp32bluetooth_svc_device.h"
#include "esp32bluetooth_svc_metrics.h"

#include "ovms_log.h"
static const char *TAG = "bt-gatts";

void ovms_ble_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                  esp_ble_gatts_cb_param_t *param)
  {
  ESP_LOGD(TAG, "GATTS_EVT, event %d (interface %d)\n",
    event, gatts_if);

  /* If event is register event, store the gatts_if for each profile */
  if (event == ESP_GATTS_REG_EVT)
    {
    if (param->reg.status == ESP_GATT_OK)
      {
      if (param->reg.app_id == GATTS_APP_UUID_OVMS_DEVICE)
        {
        ESP_LOGI(TAG,"Reg DEVICE app %04x succeeded with inteface ID %d",
          param->reg.app_id, gatts_if);
        ovms_gatts_profile_device.gatts_if = gatts_if;
        }
      else if (param->reg.app_id == GATTS_APP_UUID_OVMS_METRICS)
        {
        ESP_LOGI(TAG,"Reg METRICS app %04x succeeded with inteface ID %d",
          param->reg.app_id, gatts_if);
        ovms_gatts_profile_metrics.gatts_if = gatts_if;
        }
      }
    else
      {
      ESP_LOGI(TAG, "Reg app failed, app_id %04x, status %d\n",
               param->reg.app_id,
               param->reg.status);
      return;
      }
    }

  if ((gatts_if == ESP_GATT_IF_NONE) ||
      (gatts_if == ovms_gatts_profile_device.gatts_if))
    ovms_gatts_profile_device.gatts_cb(event, gatts_if, param);

  if ((gatts_if == ESP_GATT_IF_NONE) ||
      (gatts_if == ovms_gatts_profile_metrics.gatts_if))
    ovms_gatts_profile_metrics.gatts_cb(event, gatts_if, param);
  }

uint16_t ovms_ble_gatts_if()
  {
  return ovms_gatts_profile_device.gatts_if;
  }

void ovms_ble_gatts_register()
  {
  esp_err_t ret;

  ret = esp_ble_gatts_app_register(GATTS_APP_UUID_OVMS_DEVICE);
  if (ret)
    {
    ESP_LOGE(TAG, "gatts app register error, error code = %x", ret);
    return;
    }

  ret = esp_ble_gatts_app_register(GATTS_APP_UUID_OVMS_METRICS);
  if (ret)
    {
    ESP_LOGE(TAG, "gatts app register error, error code = %x", ret);
    return;
    }
  }

void ovms_ble_gatts_init()
  {
  ovms_ble_gatts_profile_device_init();
  ovms_ble_gatts_profile_metrics_init();
  }
