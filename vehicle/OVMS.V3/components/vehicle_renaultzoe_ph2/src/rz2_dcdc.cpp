/*
;    Project:       Open Vehicle Monitor System
;    Module:        Renault Zoe Ph2 - DCDC Control
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

// === INFORMATION ABOUT HV/DCDC KEEPALIVE ===
// The HV system and DC/DC converter can be kept alive by simulating a driver door lock or unlock event, because the Zoe Ph2 enables the HV system when these events occur.
// No actual unlock or relock takes place. We only spoof the BCM state reported to the EVC, which is sufficient to do the job reliably without causing significant side effects.
// This avoids false indications on the dashboard or in OVMS. The only limitation is that the door cannot be unlocked using the exterior door knob (only MY 2019 - Mid 2020); 
// it can only be unlocked with the key fob or through OVMS when keepalive is running.
// There are other ways to keep the HV system running, but they take more time to implement, and this method has been working flawlessly.

#include "ovms_log.h"
#include "ovms_notify.h"
#include "vehicle_renaultzoe_ph2.h"

void OvmsVehicleRenaultZoePh2::SendDCDCMessage()
{
  if (!m_vcan_enabled)
  {
    ESP_LOGW(TAG, "Cannot send DCDC message, V-CAN not enabled");
    return;
  }

  // Check if bus is asleep - if so, wake it up first using existing wakeup command
  bool bus_awake = mt_bus_awake->AsBool();
  if (!bus_awake)
  {
    ESP_LOGI(TAG, "DCDC: CAN bus is sleeping, waking it up first");
    if (CommandWakeup() != Success)
    {
      ESP_LOGW(TAG, "DCDC: Failed to wake up CAN bus, aborting DCDC keepalive");
      return;
    }
  }

  // Send fake door state opposite to actual lock state to keep HEVC and HV system awake
  uint8_t dcdc_cmd[8];
  bool car_is_locked = StandardMetrics.ms_v_env_locked->AsBool();

  if (car_is_locked)
  {
    // Car is locked, send Fake Driver Door Unlock to HEVC
    uint8_t msg_a[8] = {0x30, 0x06, 0x00, 0x03, 0x08, 0x74, 0x10, 0x20};
    memcpy(dcdc_cmd, msg_a, 8);
  }
  else
  {
    // Car is unlocked, send Fake Driver Door Locked to HEVC
    uint8_t msg_b[8] = {0x60, 0x06, 0x0c, 0x03, 0x08, 0xd9, 0xa0, 0x20};
    memcpy(dcdc_cmd, msg_b, 8);
  }

  // Send DCDC message
  esp_err_t result = m_can1->WriteStandard(0x418, 8, dcdc_cmd);

  if (result == ESP_OK || result == ESP_QUEUED)
  {
    ESP_LOGV_RZ2(TAG, "DCDC message sent (result: %d)", result);
    ESP_LOGD_RZ2(TAG, "DCDC: Message sent successfully, mode=%s, runtime=%us, soc=%.1f%%, 12v=%.2fV",
             m_dcdc_manual_enabled ? "manual" : (m_dcdc_after_ignition_active ? "after-ignition" : "auto"),
             (xTaskGetTickCount() * portTICK_PERIOD_MS - m_dcdc_start_time) / 1000,
             StandardMetrics.ms_v_bat_soc->AsFloat(0),
             StandardMetrics.ms_v_bat_12v_voltage->AsFloat(0));
  }
  else
  {
    ESP_LOGD_RZ2(TAG, "DCDC message send failed (result: %d), will retry in 1 second", result);
    ESP_LOGW(TAG, "DCDC: CAN transmission failed, mode=%s, runtime=%us",
             m_dcdc_manual_enabled ? "manual" : (m_dcdc_after_ignition_active ? "after-ignition" : "auto"),
             (xTaskGetTickCount() * portTICK_PERIOD_MS - m_dcdc_start_time) / 1000);
  }
}

bool OvmsVehicleRenaultZoePh2::EnableDCDC(bool manual)
{
  ESP_LOGD_RZ2(TAG, "DCDC: EnableDCDC called, manual=%d, ignition=%d, charging=%d, soc=%.1f%%, 12v=%.2fV",
           manual,
           StandardMetrics.ms_v_env_on->AsBool(),
           StandardMetrics.ms_v_charge_inprogress->AsBool(),
           StandardMetrics.ms_v_bat_soc->AsFloat(0),
           StandardMetrics.ms_v_bat_12v_voltage->AsFloat(0));

  if (manual)
  {
    // Validate current SOC against limit - prevent draining traction battery too low
    float current_soc = StandardMetrics.ms_v_bat_soc->AsFloat(100);
    if (current_soc <= m_dcdc_soc_limit)
    {
      ESP_LOGW(TAG, "Cannot enable manual DCDC: SOC %.0f%% is at or below limit %.0f%%", current_soc, (float)m_dcdc_soc_limit);
      return false;
    }

    m_dcdc_manual_enabled = true;

    char time_limit_buf[32];
    if (m_dcdc_time_limit == 0) {
      snprintf(time_limit_buf, sizeof(time_limit_buf), "infinite");
    } else {
      snprintf(time_limit_buf, sizeof(time_limit_buf), "%d min", m_dcdc_time_limit);
    }
    ESP_LOGI(TAG, "DCDC converter manually enabled (SOC limit: %d%%, Time limit: %s)",
             m_dcdc_soc_limit, time_limit_buf);
  }
  else
  {
    ESP_LOGI(TAG, "DCDC converter auto-enabled (will run for max 30 minutes)");
  }

  m_dcdc_active = true;
  m_dcdc_last_send_time = 0; // Force immediate send
  m_dcdc_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS; // Record start time
  m_dcdc_auto_started = !manual; // Track if this was auto-started
  m_dcdc_current_detected = false; // Reset current detection flag

  ESP_LOGI(TAG, "DCDC: Enabled successfully, mode=%s, after_ignition_active=%d",
           m_dcdc_manual_enabled ? "manual" : "auto",
           m_dcdc_after_ignition_active);

  return true;
}

void OvmsVehicleRenaultZoePh2::DisableDCDC()
{
  uint32_t runtime_seconds = 0;
  if (m_dcdc_start_time > 0)
  {
    runtime_seconds = (xTaskGetTickCount() * portTICK_PERIOD_MS - m_dcdc_start_time) / 1000;
  }

  ESP_LOGI(TAG, "DCDC: Disabling converter keepalive, mode=%s, runtime=%us, soc=%.1f%%, 12v=%.2fV",
           m_dcdc_manual_enabled ? "manual" : (m_dcdc_after_ignition_active ? "after-ignition" : "auto"),
           runtime_seconds,
           StandardMetrics.ms_v_bat_soc->AsFloat(0),
           StandardMetrics.ms_v_bat_12v_voltage->AsFloat(0));

  m_dcdc_manual_enabled = false;
  m_dcdc_active = false;
  m_dcdc_start_time = 0;
  m_dcdc_auto_started = false;
  m_dcdc_current_detected = false;
  m_dcdc_after_ignition_active = false;

  ESP_LOGI(TAG, "DCDC converter disabled");
}

void OvmsVehicleRenaultZoePh2::CheckAutoRecharge()
{
  uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
  bool is_ignition_on = StandardMetrics.ms_v_env_on->AsBool();
  bool is_charging = StandardMetrics.ms_v_charge_inprogress->AsBool();
  bool is_hvac_active = StandardMetrics.ms_v_env_hvac->AsBool();

  // Detect HVAC OFF→ON transition during after-ignition mode (external preconditioning (Easylink or Renault app))
  if (!m_last_hvac_state && is_hvac_active && m_dcdc_after_ignition_active)
  {
    ESP_LOGI(TAG, "DCDC: External preconditioning detected during after-ignition, stopping DCDC keepalive");
    DisableDCDC();
    m_last_hvac_state = is_hvac_active;
    return;
  }

  // Check if DCDC is active and needs to be stopped
  if (m_dcdc_active)
  {
    ESP_LOGD_RZ2(TAG, "DCDC: CheckAutoRecharge - active check, ignition=%d, charging=%d, hvac=%d, runtime=%us",
             is_ignition_on, is_charging, is_hvac_active,
             (currentTime - m_dcdc_start_time) / 1000);

    // Check if charging current has been detected
    float charge_12v_current = StandardMetrics.ms_v_charge_12v_current->AsFloat(0);
    if (charge_12v_current > 0.5)  // Small threshold to account for noise
    {
      m_dcdc_current_detected = true;
    }

    // Check if no current detected after 1 minute
    if (!m_dcdc_current_detected)
    {
      uint32_t elapsed_time = currentTime - m_dcdc_start_time;
      if (elapsed_time >= 60000)  // 60 seconds = 1 minute
      {
        if (m_dcdc_auto_started)
        {
          // For auto mode: disable and block to let car sleep
          ESP_LOGI(TAG, "DCDC: No charging current detected after 1 minute, disabling auto-recharge to let car sleep");
          DisableDCDC();

          // Notify user that auto-recharge was blocked due to no current
          MyNotify.NotifyString("alert", "vehicle.charge.12v.failed",
                               "Auto-recharge blocked: No charging current detected after 1 minute. Will retry after next drive or charge.");

          // Temporarily block auto-recharge to prevent repeated attempts
          m_auto_recharge_blocked = true;
          m_auto_recharge_block_time = currentTime;
          ESP_LOGI(TAG, "DCDC: Auto-recharge temporarily blocked until next drive or charge session");

          return;
        }
        else if (m_dcdc_manual_enabled)
        {
          // For manual mode: warn user but keep running (they may want to investigate)
          ESP_LOGW(TAG, "DCDC: Manual mode - no charging current detected after 1 minute");
          MyNotify.NotifyString("alert", "vehicle.charge.12v.warning",
                               "WARNING: DCDC is active but no charging current detected. Check car state or disable DCDC.");

          // Set flag so we only send notification once
          m_dcdc_current_detected = true;  // Prevent repeated notifications
        }
      }
    }

    // Stop if ignition is ON (safety measure - don't run DCDC keepalive while driving)
    if (is_ignition_on)
    {
      ESP_LOGI(TAG, "DCDC: Ignition ON detected, stopping DCDC converter keepalive");
      DisableDCDC();
      return;
    }

    // Stop if car is charging (safety measure - DCDC interferes with charging)
    if (is_charging)
    {
      ESP_LOGI(TAG, "DCDC: Charging detected, stopping DCDC converter keepalive");
      DisableDCDC();
      return;
    }

    // Stop if preconditioning is active (DCDC will be running anyway)
    if (is_hvac_active && !m_dcdc_after_ignition_active)
    {
      ESP_LOGI(TAG, "DCDC: Preconditioning detected, stopping DCDC converter keepalive");
      DisableDCDC();
      return;
    }

    // For MANUAL mode only: Check SOC limit - stop if traction battery drops too low
    if (m_dcdc_manual_enabled)
    {
      float current_soc = StandardMetrics.ms_v_bat_soc->AsFloat(100);
      if (current_soc <= m_dcdc_soc_limit)
      {
        ESP_LOGI(TAG, "DCDC: SOC dropped to limit %.0f%%, stopping manual DCDC to protect traction battery", (float)m_dcdc_soc_limit);
        DisableDCDC();
        return;
      }

      // For MANUAL mode only: Check time limit (if not 0/infinite)
      if (m_dcdc_time_limit > 0)
      {
        uint32_t elapsed_time = currentTime - m_dcdc_start_time;
        uint32_t time_limit_ms = m_dcdc_time_limit * 60 * 1000;
        if (elapsed_time >= time_limit_ms)
        {
          ESP_LOGI(TAG, "DCDC: Manual mode time limit (%d min) reached, stopping", m_dcdc_time_limit);
          DisableDCDC();
          return;
        }
      }
    }
    else
    {
      // For AUTO mode: Keep existing 30-minute timeout
      uint32_t elapsed_time = currentTime - m_dcdc_start_time;
      if (elapsed_time >= (30 * 60 * 1000))
      {
        ESP_LOGI(TAG, "DCDC: Auto-recharge timeout (30 minutes), stopping");
        DisableDCDC();
        return;
      }
    }
  }

  // Only check for starting auto-recharge if enabled and not manually controlled
  if (!m_auto_12v_recharge_enabled || !m_vcan_enabled || m_dcdc_manual_enabled)
  {
    return;
  }

  // Check if auto-recharge block should be cleared
  if (m_auto_recharge_blocked)
  {
    // Clear block if car has been driven (ignition was ON)
    if (is_ignition_on)
    {
      m_auto_recharge_blocked = false;
      m_auto_recharge_block_time = 0;
      ESP_LOGI(TAG, "DCDC: Auto-recharge block cleared - car was driven");
      MyNotify.NotifyString("info", "vehicle.charge.12v", "Auto-recharge re-enabled after driving");
    }
    // Clear block if car has been charging
    else if (is_charging)
    {
      m_auto_recharge_blocked = false;
      m_auto_recharge_block_time = 0;
      ESP_LOGI(TAG, "DCDC: Auto-recharge block cleared - car was charging");
      MyNotify.NotifyString("info", "vehicle.charge.12v", "Auto-recharge re-enabled after charging");
    }
    // If still blocked, don't attempt auto-recharge
    else
    {
      return;
    }
  }

  float voltage_12v = StandardMetrics.ms_v_bat_12v_voltage->AsFloat(0);

  // Auto-start if ignition OFF, NOT charging, NOT preconditioning, and voltage below threshold
  if (!is_ignition_on && !is_charging && !is_hvac_active && voltage_12v > 0 && voltage_12v < m_auto_12v_threshold && !m_dcdc_active)
  {
    ESP_LOGI(TAG, "DCDC: 12V voltage %.2fV below threshold %.2fV (ignition OFF, not charging, no preconditioning), enabling auto-recharge", voltage_12v, m_auto_12v_threshold);
    EnableDCDC(false);

    // Notify user that auto-recharge has been engaged
    char notify_msg[64];
    snprintf(notify_msg, sizeof(notify_msg), "Auto-recharge engaged: 12V at %.2fV (threshold %.2fV)", voltage_12v, m_auto_12v_threshold);
    MyNotify.NotifyString("info", "vehicle.charge.12v", notify_msg);
  }

  // Update HVAC state for next cycle (for OFF→ON transition detection)
  m_last_hvac_state = is_hvac_active;
}
