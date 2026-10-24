/*
;    Project:       Open Vehicle Monitor System
;    Module:        Power Management Webserver
;    Date:          3rd September 2019
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2019        Marko Juhanne
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
static const char *TAG = "powermgmt";
#include "ovms_events.h"
#include "ovms_config.h"
#include "powermgmt.h"
#include "ovms_peripherals.h"
#include "ovms_netmanager.h"
#include "ovms_boot.h"
#include "metrics_standard.h"

powermgmt MyPowerMgmt __attribute__ ((init_priority (8500)));

powermgmt::powermgmt()
  {
  ESP_LOGI(TAG, "Initialising POWERMGMT (8500)");

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&powermgmt::Ticker1, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.changed", std::bind(&powermgmt::ConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.mounted", std::bind(&powermgmt::ConfigChanged, this, _1, _2));

  m_enabled = false;
  m_notcharging_timer = 0;
  m_modemoff_wifi_timer = 0;
  m_12v_alert_timer = 0;
  m_modem_wifi_fallback = false;
  m_charging = false;
  m_modem_off = false;
  m_modem_off_wifi = false;
  m_wifi_off = false;

  MyConfig.RegisterParam("power", "Power management", true, true);
  ConfigChanged("config.mounted", NULL);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif
  }

powermgmt::~powermgmt()
  {
 #ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebCleanup();
#endif
 }

void powermgmt::ConfigChanged(std::string event, void* data)
  {
  OvmsConfigParam* param = (OvmsConfigParam*) data;

  // read vehicle framework config:
  if (!param || param->GetName() == "power")
    {
    m_enabled = MyConfig.GetParamValueBool("power", "enabled", false);
    m_modemoff_delay = MyConfig.GetParamValueInt("power", "modemoff_delay", POWERMGMT_MODEMOFF_DELAY);
    m_modemoff_wifi_delay = MyConfig.GetParamValueInt("power", "modemoff_wifi_delay", POWERMGMT_MODEMOFF_WIFI_DELAY);
    m_modem_wifi_fallback = MyConfig.GetParamValueBool("power", "modem_wifi_fallback", false);
    m_wifioff_delay = MyConfig.GetParamValueInt("power", "wifioff_delay", POWERMGMT_WIFIOFF_DELAY);
    m_12v_shutdown_delay = MyConfig.GetParamValueInt("power", "12v_shutdown_delay", POWERMGMT_12V_SHUTDOWN_DELAY);
    }
  }

void powermgmt::GetEstimatedConsumption(powermgmt_consumption_t& out)
  {
  // Approximate OVMS v3 current draw at ~12.6V in mA (no hardware current sensor available)
  out = {};
  out.base_ma = 45.0f; // Base ESP32 + SPIRAM + system

  if (MyPeripherals)
    {
#ifdef CONFIG_OVMS_COMP_ESP32CAN
    if (MyPeripherals->m_esp32can && MyPeripherals->m_esp32can->GetPowerMode() == On)
      out.can1_ma = 8.0f;
#endif
#ifdef CONFIG_OVMS_COMP_MCP2515
    if (MyPeripherals->m_mcp2515_1 && MyPeripherals->m_mcp2515_1->GetPowerMode() == On)
      out.can2_ma = 8.0f;
    if (MyPeripherals->m_mcp2515_2 && MyPeripherals->m_mcp2515_2->GetPowerMode() == On)
      out.can3_ma = 8.0f;
#endif
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
    if (MyPeripherals->m_mcp2515_swcan && MyPeripherals->m_mcp2515_swcan->GetPowerMode() == On)
      out.swcan_ma = 8.0f;
#endif
#ifdef CONFIG_OVMS_COMP_WIFI
    if (MyPeripherals->m_esp32wifi && MyPeripherals->m_esp32wifi->GetPowerMode() == On)
      {
      esp32wifi_mode_t wmode = MyPeripherals->m_esp32wifi->GetMode();
      if (wmode == ESP32WIFI_MODE_AP || wmode == ESP32WIFI_MODE_APCLIENT)
        out.wifi_ma = 45.0f;
      else if (wmode != ESP32WIFI_MODE_OFF)
        out.wifi_ma = 35.0f;
      }
#endif
#ifdef CONFIG_OVMS_COMP_CELLULAR
    if (MyPeripherals->m_cellular_modem)
      {
      PowerMode mpm = MyPeripherals->m_cellular_modem->GetPowerMode();
      if (mpm == On)
        {
        out.modem_ma = MyNetManager.m_connected_modem ? 45.0f : 25.0f;
        if (MyPeripherals->m_cellular_modem->m_nmea != NULL)
          out.gps_ma = 15.0f;
        }
      else if (mpm == Sleep)
        {
        out.modem_ma = 10.0f;
        }
      }
#endif
#ifdef CONFIG_OVMS_COMP_EXT12V
    if (MyPeripherals->m_ext12v && MyPeripherals->m_ext12v->GetPowerMode() == On)
      out.ext12v_ma = 15.0f;
#endif
    }

  out.total_ma = out.base_ma + out.can1_ma + out.can2_ma + out.can3_ma +
                 out.swcan_ma + out.wifi_ma + out.modem_ma + out.gps_ma + out.ext12v_ma;
  }

float powermgmt::GetEstimatedCurrent()
  {
  powermgmt_consumption_t cons;
  GetEstimatedConsumption(cons);
  return cons.total_ma;
  }

void powermgmt::Ticker1(std::string event, void* data)
  {
  if (!m_charging)
    {
    m_notcharging_timer++;
#ifdef CONFIG_OVMS_COMP_WIFI
    if (MyNetManager.m_connected_wifi)
      m_modemoff_wifi_timer++;
    else
      m_modemoff_wifi_timer = 0;
#endif
    }
  else
    {
    m_modemoff_wifi_timer = 0;
    }

  if (StandardMetrics.ms_v_env_charging12v->AsBool())
    {
    if (!m_charging)
      {
      ESP_LOGI(TAG,"Charging 12V battery..");
      m_charging=true;
      m_modemoff_wifi_timer=0;
      // Turn on WiFi and modem if they were previously turned off by us
#ifdef CONFIG_OVMS_COMP_WIFI
      if (m_wifi_off)
        {
        MyPeripherals->m_esp32wifi->SetPowerMode(On);
        m_wifi_off = false;
        }
#endif
#ifdef CONFIG_OVMS_COMP_CELLULAR
      if (m_modem_off)
        {
        MyPeripherals->m_cellular_modem->SetPowerMode(On);
        m_modem_off = false;
        m_modem_off_wifi = false;
        }
#endif
      }
    }
  else
    {
    if (m_charging)
      {
      ESP_LOGI(TAG,"No longer charging 12V battery..");
      m_charging=false;
      m_notcharging_timer=0;
      m_modemoff_wifi_timer=0;
      }
    }

  if (!m_enabled)
    return;

#ifdef CONFIG_OVMS_COMP_WIFI
  if (m_wifioff_delay && m_notcharging_timer > m_wifioff_delay*60*60) // hours to seconds
    {
    if (MyPeripherals->m_esp32wifi->GetPowerMode()==On)
      {
      ESP_LOGW(TAG,"Powering off wifi");
      MyEvents.SignalEvent("powermgmt.wifi.off",NULL);
      vTaskDelay(500 / portTICK_PERIOD_MS); // make sure all notifications all transmitted before powerring down WiFI
      MyPeripherals->m_esp32wifi->SetPowerMode(Off);
      m_wifi_off = true;
      }
    }
#endif

#if defined(CONFIG_OVMS_COMP_CELLULAR) && defined(CONFIG_OVMS_COMP_WIFI)
  if (m_modemoff_wifi_delay && m_modemoff_wifi_timer > m_modemoff_wifi_delay*60) // minutes to seconds
    {
    if (MyPeripherals->m_cellular_modem->GetPowerMode()==On)
      {
      ESP_LOGW(TAG,"Powering off cellular modem (WiFi connected)");
      MyEvents.SignalEvent("powermgmt.modem.off",NULL);
      vTaskDelay(500 / portTICK_PERIOD_MS); // make sure all notifications all transmitted before powerring down modem
      MyPeripherals->m_cellular_modem->SetPowerMode(Off);
      m_modem_off = true;
      m_modem_off_wifi = true;
      }
    }
  else if (m_modem_wifi_fallback && m_modem_off_wifi && !MyNetManager.m_connected_wifi)
    {
    if (!(m_modemoff_delay && m_notcharging_timer > m_modemoff_delay*60*60))
      {
      ESP_LOGI(TAG,"WiFi disconnected, powering on cellular modem (fallback)");
      MyPeripherals->m_cellular_modem->SetPowerMode(On);
      m_modem_off = false;
      m_modem_off_wifi = false;
      }
    }
#endif

#ifdef CONFIG_OVMS_COMP_CELLULAR
  if (m_modemoff_delay && m_notcharging_timer > m_modemoff_delay*60*60) // hours to seconds
    {
    if (MyPeripherals->m_cellular_modem->GetPowerMode()==On)
      {
      ESP_LOGW(TAG,"Powering off cellular modem");
      MyEvents.SignalEvent("powermgmt.modem.off",NULL);
      vTaskDelay(500 / portTICK_PERIOD_MS); // make sure all notifications all transmitted before powerring down modem
      MyPeripherals->m_cellular_modem->SetPowerMode(Off);
      m_modem_off = true;
      }
    m_modem_off_wifi = false;
    }
#endif

  if (StandardMetrics.ms_v_bat_12v_voltage_alert->AsBool())
    {
    m_12v_alert_timer++;
    if (m_12v_shutdown_delay && m_12v_alert_timer == 1)
      {
      ESP_LOGW(TAG, "12V battery alert detected, will shutdown in %d minutes", m_12v_shutdown_delay);
      }
    if (m_12v_shutdown_delay && m_12v_alert_timer > m_12v_shutdown_delay*60) // minutes to seconds
      {
      m_12v_shutdown_delay = 0;
      ESP_LOGE(TAG,"Ongoing 12V battery alert time limit exceeded! Shutting down OVMS..");
      MyEvents.SignalEvent("powermgmt.ovms.shutdown",NULL);
      
      // Override default boot voltage level configuration:
      float dref = MyConfig.GetParamValueFloat("vehicle", "12v.ref", 12.6);
      float vref = MAX(StandardMetrics.ms_v_bat_12v_voltage_ref->AsFloat(), dref);
      float alert_threshold = MyConfig.GetParamValueFloat("vehicle", "12v.alert", 1.6);
      MyBoot.SetMin12VLevel(vref - alert_threshold + 0.5);
      
      // Request deep sleep shutdown:
      unsigned int wakeup_interval = MyConfig.GetParamValueInt("vehicle", "12v.wakeup_interval", 60);
      MyBoot.DeepSleep(wakeup_interval);
      }
    }
  else
    {
    if (m_12v_shutdown_delay && m_12v_alert_timer)
      {
      ESP_LOGI(TAG, "12V battery alert resolved, shutdown cancelled");
      }
    m_12v_alert_timer = 0;
    }
  }
