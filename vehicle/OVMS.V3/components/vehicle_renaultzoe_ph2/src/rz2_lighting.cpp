/*
;    Project:       Open Vehicle Monitor System
;    Module:        Renault Zoe Ph2 - Coming Home Lighting
;
;    (C) 2022-2026  Carsten Schmiemann
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software;
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
#include "vehicle_renaultzoe_ph2.h"

void OvmsVehicleRenaultZoePh2::TriggerComingHomeLighting()
{
  if (!m_vcan_enabled)
  {
    ESP_LOGD_RZ2(TAG, "Coming home function skipped - V-CAN not enabled");
    return;
  }

  if (!m_coming_home_enabled)
  {
    ESP_LOGD_RZ2(TAG, "Coming home function skipped - not enabled");
    return;
  }

  // Check if headlights (lowbeam) were on when ignition was turned off
  // We use saved state because headlights turn off when driver door opens
  if (m_last_headlight_state)
  {
    // If the BCM already reports lowbeam ON, don't send the toggle frame again
    if (vcan_light_lowbeam)
    {
      ESP_LOGI(TAG, "Coming home skipped - headlights already on (vcan_light_lowbeam)");
      m_last_headlight_state = false;
      return;
    }

    ESP_LOGI(TAG, "Coming home function activated - headlights were on when ignition turned off, triggering lighting");

    uint8_t lighting_cmd[8] = {0xF4, 0xB7, 0xA8, 0x40, 0x24, 0x84, 0x07, 0x10};
    // Keep at most two attempts to avoid toggling the HFM action back off
    const int max_attempts = 2;
    const TickType_t wait_ms = 300; // allow one V-CAN cycle to reflect state
    bool lights_on = false;
    bool tx_success = false;

    // Send, then stop as soon as BCM reports the lowbeam bit
    for (int i = 0; i < max_attempts && !lights_on; i++)
    {
      // Clear success flag before sending
      m_tx_success_flag.store(false);

      // Send the frame
      m_can1->WriteStandard(HFM_A1_ID, 8, lighting_cmd);

      // Wait for transmission and for BCM to reflect new state
      vTaskDelay(wait_ms / portTICK_PERIOD_MS);

      // Update flags
      tx_success = m_tx_success_flag.load() && m_tx_success_id.load() == HFM_A1_ID;
      lights_on = vcan_light_lowbeam;

      #ifdef RZ2_DEBUG_BUILD
      if (tx_success)
      {
        ESP_LOGI(TAG, "Coming home lighting command transmitted successfully on attempt %d", i + 1);
      }
      else
      {
        ESP_LOGW(TAG, "Coming home lighting command transmission failed (attempt %d/%d)", i + 1, max_attempts);
      }
      #endif
    }

    if (lights_on)
    {
      ESP_LOGI(TAG, "Coming home lighting engaged - headlights on (attempts used: %s)",
               tx_success ? "ok" : "unknown tx state");
      // Reset the saved state after successful transmission
      m_last_headlight_state = false;
    }
    else
    {
      ESP_LOGE(TAG, "Coming home lighting failed - headlights stayed off after %d attempts", max_attempts);
      // Keep the state so we can retry on next lock
    }
  }
  #ifdef RZ2_DEBUG_BUILD
  else
  {
    ESP_LOGD(TAG, "Coming home function skipped - headlights were not on when ignition turned off");
  }
  #endif
}
