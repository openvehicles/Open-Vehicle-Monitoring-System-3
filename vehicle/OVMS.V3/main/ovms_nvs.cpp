/*
;    Project:       Open Vehicle Monitor System
;    Date:          12th November 2022
;
;    Changes:
;    1.0  Initial release - based on public domain code from Espressif (examples)
;
;    (C) 2022       Ludovic LANGE
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

#include "ovms_log.h"
static const char *TAG = "nvs";

#include "ovms_nvs.h"
#include "nvs.h"
#include "nvs_flash.h"

OvmsNonVolatileStorage MyNonVolatileStorage __attribute__ ((init_priority (10000)));

OvmsNonVolatileStorage::OvmsNonVolatileStorage() : m_restart_counter(0){
  ESP_LOGI(TAG, "Initialising NVS (10000)");
}

OvmsNonVolatileStorage::~OvmsNonVolatileStorage() {
  esp_err_t err = nvs_flash_deinit();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) de-initialising NVS subsystem!", esp_err_to_name(err));
  }
}

void OvmsNonVolatileStorage::Init() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) initialising NVS subsystem!", esp_err_to_name(err));
  }
}

void OvmsNonVolatileStorage::Start() {
  esp_err_t err = ESP_OK;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    nvs_handle_t handle;
#else
    nvs_handle handle;
#endif

  ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) default partition and 'ovms' namespace");
  err = nvs_open("ovms", NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
  } else {
    err = nvs_get_u64(handle, "restart_counter", &m_restart_counter);
    switch (err) {
      case ESP_OK:
        ESP_LOGI(TAG, "Current restart counter: %llu", m_restart_counter);
        break;
      case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGW(TAG, "The restart counter is not initialized yet");
        break;
      default :
        ESP_LOGE(TAG, "Error (%s) while reading restart counter in NVS!", esp_err_to_name(err));
    }

    // Write
    ESP_LOGD(TAG, "Updating restart counter in NVS");
    m_restart_counter++;
    // To ease the display on 8 decimal digits, we wrap after 99.999.999 restarts.
    // If we restart every minute, we'll reach this time in 190 years.
    if (m_restart_counter > 99999999) {
      m_restart_counter = 0;
    }
    err = nvs_set_u64(handle, "restart_counter", m_restart_counter);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error (%s) while updating restart counter in NVS!", esp_err_to_name(err));
    }

    // Commit written value.
    // After setting any values, nvs_commit() must be called to ensure changes are written
    // to flash storage. Implementations may write to storage at other times,
    // but this is not guaranteed.
    err = nvs_commit(handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error (%s) while committing changes in NVS!", esp_err_to_name(err));
    }
    nvs_close(handle);
  }
}

uint64_t OvmsNonVolatileStorage::GetRestartCount() {
  return m_restart_counter;
}
