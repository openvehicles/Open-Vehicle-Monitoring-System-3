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
  m_12v_alert_timer = 0;
  m_charging = false;
  m_modem_off = false;
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
    m_wifioff_delay = MyConfig.GetParamValueInt("power", "wifioff_delay", POWERMGMT_WIFIOFF_DELAY);
    m_12v_shutdown_delay = MyConfig.GetParamValueInt("power", "12v_shutdown_delay", POWERMGMT_12V_SHUTDOWN_DELAY);
    }
  }

void powermgmt::Ticker1(std::string event, void* data)
  {
  if (!m_charging)
    m_notcharging_timer++;

  if (StandardMetrics.ms_v_env_charging12v->AsBool())
    {
    if (!m_charging)
      {
      ESP_LOGI(TAG,"Charging 12V battery..");
      m_charging=true;
      // Turn on WiFi and modem if they were previously turned off by us
#ifdef CONFIG_OVMS_COMP_WIFI
      if (m_wifi_off)
        {
        MyPeripherals->m_esp32wifi->SetPowerMode(On);
        m_wifi_off = false;
        }
#endif
#ifdef CONFIG_OVMS_COMP_MODEM
      if (m_modem_off)
        {
        MyPeripherals->m_modem->SetPowerMode(On);
        m_modem_off = false;
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

#ifdef CONFIG_OVMS_COMP_MODEM
  if (m_modemoff_delay && m_notcharging_timer > m_modemoff_delay*60*60) // hours to seconds
    {
    if (MyPeripherals->m_modem->GetPowerMode()==On)
      {
      ESP_LOGW(TAG,"Powering off modem");
      MyEvents.SignalEvent("powermgmt.modem.off",NULL);
      vTaskDelay(500 / portTICK_PERIOD_MS); // make sure all notifications all transmitted before powerring down modem
      MyPeripherals->m_modem->SetPowerMode(Off);
      m_modem_off = true;
      }
    }
#endif

  if (StandardMetrics.ms_v_bat_12v_voltage_alert->AsBool())
    {
    m_12v_alert_timer++;
    if (m_12v_alert_timer > m_12v_shutdown_delay*60) // minutes to seconds
      {
      ESP_LOGE(TAG,"Ongoing 12V battery alert time limit exceeded! Shutting down OVMS..");
      MyEvents.SignalEvent("powermgmt.ovms.shutdown",NULL);
      vTaskDelay(500 / portTICK_PERIOD_MS); // make sure all notifications all transmitted before powerring down OVMS
#ifdef CONFIG_OVMS_COMP_WIFI
      MyPeripherals->m_esp32wifi->SetPowerMode(Off);
#endif
#ifdef CONFIG_OVMS_COMP_MODEM
      MyPeripherals->m_modem->SetPowerMode(Off);
#endif
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
      MyPeripherals->m_mcp2515_swcan->SetPowerMode(Off);
#endif
#ifdef CONFIG_OVMS_COMP_ESP32CAN
      MyPeripherals->m_esp32can->SetPowerMode(Off);
#endif
#ifdef CONFIG_OVMS_COMP_EXT12V
      MyPeripherals->m_ext12v->SetPowerMode(Off);
#endif
      MyPeripherals->m_esp32->SetPowerMode(DeepSleep);
      }
    }
  else
    {
    m_12v_alert_timer = 0;
    }
  }
