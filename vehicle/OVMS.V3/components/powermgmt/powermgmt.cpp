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

static void pmgmt_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPowerMgmt.CommandStatus(verbosity, writer);
  }

static void pmgmt_config(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPowerMgmt.CommandConfig(verbosity, writer);
  }

static void pmgmt_poweroff(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPowerMgmt.CommandPowerOff(verbosity, writer, cmd->GetName());
  }

static void pmgmt_poweron(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPowerMgmt.CommandPowerOn(verbosity, writer, cmd->GetName());
  }

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

  OvmsCommand* cmd_pmgmt = MyCommandApp.RegisterCommand("pmgmt", "Power management framework", pmgmt_status, "", 0, 0, false);
  cmd_pmgmt->RegisterCommand("status", "Show power status and estimated consumption", pmgmt_status, "", 0, 0, false);
  cmd_pmgmt->RegisterCommand("config", "Show power management configuration", pmgmt_config, "", 0, 0, false);
  OvmsCommand* cmd_poweroff = cmd_pmgmt->RegisterCommand("poweroff", "Power off modem, wifi or both (as if delay reached)");
  cmd_poweroff->RegisterCommand("modem", "Power off cellular modem", pmgmt_poweroff, "", 0, 0, true);
  cmd_poweroff->RegisterCommand("wifi", "Power off WiFi", pmgmt_poweroff, "", 0, 0, true);
  cmd_poweroff->RegisterCommand("both", "Power off both cellular modem and WiFi", pmgmt_poweroff, "", 0, 0, true);
  OvmsCommand* cmd_poweron = cmd_pmgmt->RegisterCommand("poweron", "Power on modem, wifi or both and reset delay timers");
  cmd_poweron->RegisterCommand("modem", "Power on cellular modem and reset delay timers", pmgmt_poweron, "", 0, 0, true);
  cmd_poweron->RegisterCommand("wifi", "Power on WiFi and reset delay timers", pmgmt_poweron, "", 0, 0, true);
  cmd_poweron->RegisterCommand("both", "Power on both cellular modem and WiFi and reset delay timers", pmgmt_poweron, "", 0, 0, true);

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

void powermgmt::CommandStatus(int verbosity, OvmsWriter* writer)
  {
  float v12 = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
  float v12_calc = (v12 > 1.0f) ? v12 : 12.6f;
  bool chg12 = StandardMetrics.ms_v_env_charging12v->AsBool();
  powermgmt_consumption_t cons;
  GetEstimatedConsumption(cons);
  float est_w = v12_calc * (cons.total_ma / 1000.0f);

  const char* can1_pm = "n/a";
  const char* can2_pm = "n/a";
  const char* can3_pm = "n/a";
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  const char* swcan_pm = "n/a";
#endif
  const char* wifi_pm = "n/a";
  const char* wifi_state = "disabled";
  const char* modem_pm = "n/a";
  const char* modem_state = "idle";
  const char* gps_pm = "n/a";
  const char* ext12v_pm = "n/a";

  if (MyPeripherals)
    {
#ifdef CONFIG_OVMS_COMP_ESP32CAN
    if (MyPeripherals->m_esp32can)
      can1_pm = MyPcpApp.FindPowerModeByType(MyPeripherals->m_esp32can->GetPowerMode());
#endif
#ifdef CONFIG_OVMS_COMP_MCP2515
    if (MyPeripherals->m_mcp2515_1)
      can2_pm = MyPcpApp.FindPowerModeByType(MyPeripherals->m_mcp2515_1->GetPowerMode());
    if (MyPeripherals->m_mcp2515_2)
      can3_pm = MyPcpApp.FindPowerModeByType(MyPeripherals->m_mcp2515_2->GetPowerMode());
#endif
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
    if (MyPeripherals->m_mcp2515_swcan)
      swcan_pm = MyPcpApp.FindPowerModeByType(MyPeripherals->m_mcp2515_swcan->GetPowerMode());
#endif
#ifdef CONFIG_OVMS_COMP_WIFI
    if (MyPeripherals->m_esp32wifi)
      {
      wifi_pm = MyPcpApp.FindPowerModeByType(MyPeripherals->m_esp32wifi->GetPowerMode());
      esp32wifi_mode_t wmode = MyPeripherals->m_esp32wifi->GetMode();
      if (wmode == ESP32WIFI_MODE_AP)
        wifi_state = "AP";
      else if (wmode == ESP32WIFI_MODE_APCLIENT)
        wifi_state = MyNetManager.m_connected_wifi ? "AP+Client, connected" : "AP+Client, disconnected";
      else if (wmode == ESP32WIFI_MODE_CLIENT)
        wifi_state = MyNetManager.m_connected_wifi ? "Client, connected" : "Client, disconnected";
      else if (wmode == ESP32WIFI_MODE_SCAN)
        wifi_state = "Scan";
      else
        wifi_state = "off";
      }
#endif
#ifdef CONFIG_OVMS_COMP_CELLULAR
    if (MyPeripherals->m_cellular_modem)
      {
      modem_pm = MyPcpApp.FindPowerModeByType(MyPeripherals->m_cellular_modem->GetPowerMode());
      modem_state = MyNetManager.m_connected_modem ? "PPP connected" : "idle";
      gps_pm = (MyPeripherals->m_cellular_modem->GetPowerMode() == On &&
                MyPeripherals->m_cellular_modem->m_nmea != NULL) ? "on" : "off";
      }
#endif
#ifdef CONFIG_OVMS_COMP_EXT12V
    if (MyPeripherals->m_ext12v)
      ext12v_pm = MyPcpApp.FindPowerModeByType(MyPeripherals->m_ext12v->GetPowerMode());
#endif
    }

  writer->printf("12V Battery:       %.2f V (%s)\n", v12, chg12 ? "charging" : "not charging");
  if (StandardMetrics.ms_v_bat_12v_current->IsDefined())
    writer->printf("12V Current (Veh): %.2f A\n", StandardMetrics.ms_v_bat_12v_current->AsFloat());
  writer->printf("Timers:            not charging %u min, WiFi modem-off %u min, 12V alert %u min\n",
    m_notcharging_timer / 60, m_modemoff_wifi_timer / 60, m_12v_alert_timer / 60);
  writer->printf("Flags:             modem off=%s, modem off wifi connected=%s, wifi off=%s\n\n",
    m_modem_off ? "yes" : "no", m_modem_off_wifi ? "yes" : "no", m_wifi_off ? "yes" : "no");

  writer->printf("%-26s %-26s %12s %10s\n", "Consumer", "State", "Est. Current", "Est. Power");
  writer->printf("------------------------------------------------------------------------------\n");
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "ESP32 + SPIRAM + System", "on",
    cons.base_ma, v12_calc * (cons.base_ma / 1000.0f));
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "CAN1 (esp32can)", can1_pm,
    cons.can1_ma, v12_calc * (cons.can1_ma / 1000.0f));
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "CAN2 (mcp2515_1)", can2_pm,
    cons.can2_ma, v12_calc * (cons.can2_ma / 1000.0f));
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "CAN3 (mcp2515_2)", can3_pm,
    cons.can3_ma, v12_calc * (cons.can3_ma / 1000.0f));
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "SWCAN (mcp2515_swcan)", swcan_pm,
    cons.swcan_ma, v12_calc * (cons.swcan_ma / 1000.0f));
#endif
  char wifi_buf[36];
  snprintf(wifi_buf, sizeof(wifi_buf), "%s (%s)", wifi_pm, wifi_state);
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "WiFi", wifi_buf,
    cons.wifi_ma, v12_calc * (cons.wifi_ma / 1000.0f));
  char modem_buf[36];
  snprintf(modem_buf, sizeof(modem_buf), "%s (%s)", modem_pm, modem_state);
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "Cellular Modem", modem_buf,
    cons.modem_ma, v12_calc * (cons.modem_ma / 1000.0f));
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "GPS (NMEA)", gps_pm,
    cons.gps_ma, v12_calc * (cons.gps_ma / 1000.0f));
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "Ext 12V", ext12v_pm,
    cons.ext12v_ma, v12_calc * (cons.ext12v_ma / 1000.0f));
  writer->printf("------------------------------------------------------------------------------\n");
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n\n", "Total Estimated OVMS Draw", "",
    cons.total_ma, est_w);
  writer->puts("Note: Consumption values are rough estimates based on active peripheral states.");
  }

void powermgmt::CommandConfig(int verbosity, OvmsWriter* writer)
  {
  writer->printf("Power Management Configuration:\n");
  writer->printf("  Enabled:                            %s\n", m_enabled ? "yes" : "no");
#ifdef CONFIG_OVMS_COMP_CELLULAR
  writer->printf("  Modem off delay:                    %u hours (0 = disabled)\n", m_modemoff_delay);
#ifdef CONFIG_OVMS_COMP_WIFI
  writer->printf("  Modem off delay (WiFi connected):   %u minutes (0 = disabled)\n", m_modemoff_wifi_delay);
  writer->printf("  Modem on when WiFi disconnects:     %s\n", m_modem_wifi_fallback ? "yes" : "no");
#endif
#endif
#ifdef CONFIG_OVMS_COMP_WIFI
  writer->printf("  WiFi off delay:                     %u hours (0 = disabled)\n", m_wifioff_delay);
#endif
  writer->printf("  12V alert shutdown delay:           %u minutes (0 = disabled)\n", m_12v_shutdown_delay);
  }

void powermgmt::PowerOffWifi()
  {
#ifdef CONFIG_OVMS_COMP_WIFI
  if (MyPeripherals && MyPeripherals->m_esp32wifi)
    {
    if (MyPeripherals->m_esp32wifi->GetPowerMode() == On)
      {
      ESP_LOGW(TAG, "Powering off wifi");
      MyEvents.SignalEvent("powermgmt.wifi.off", NULL);
      vTaskDelay(1500 / portTICK_PERIOD_MS); // make sure all notifications are transmitted before powering down WiFi
      MyPeripherals->m_esp32wifi->SetPowerMode(Off);
      }
    m_wifi_off = true;
    }
#endif
  }

void powermgmt::PowerOffModem(bool wifi_triggered)
  {
#ifdef CONFIG_OVMS_COMP_CELLULAR
  if (MyPeripherals && MyPeripherals->m_cellular_modem)
    {
    if (MyPeripherals->m_cellular_modem->GetPowerMode() == On)
      {
      ESP_LOGW(TAG, "Powering off cellular modem%s", wifi_triggered ? " (WiFi connected)" : "");
      MyEvents.SignalEvent("powermgmt.modem.off", NULL);
      vTaskDelay(1500 / portTICK_PERIOD_MS); // make sure all notifications are transmitted before powering down modem
      MyPeripherals->m_cellular_modem->SetPowerMode(Off);
      }
    m_modem_off = true;
    m_modem_off_wifi = wifi_triggered;
    }
#endif
  }

void powermgmt::PowerOnWifi()
  {
#ifdef CONFIG_OVMS_COMP_WIFI
  if (MyPeripherals && MyPeripherals->m_esp32wifi)
    {
    ESP_LOGI(TAG, "Powering on wifi");
    MyPeripherals->m_esp32wifi->SetPowerMode(On);
    if (MyPeripherals->m_esp32wifi->GetMode() == ESP32WIFI_MODE_OFF)
      MyPeripherals->m_esp32wifi->AutoInit();
    m_wifi_off = false;
    }
#endif
  }

void powermgmt::PowerOnModem()
  {
#ifdef CONFIG_OVMS_COMP_CELLULAR
  if (MyPeripherals && MyPeripherals->m_cellular_modem)
    {
    ESP_LOGI(TAG, "Powering on cellular modem");
    MyPeripherals->m_cellular_modem->SetPowerMode(On);
    m_modem_off = false;
    m_modem_off_wifi = false;
    }
#endif
  }

void powermgmt::CommandPowerOff(int verbosity, OvmsWriter* writer, const char* target)
  {
  bool do_modem = (strcmp(target, "modem") == 0 || strcmp(target, "both") == 0);
  bool do_wifi = (strcmp(target, "wifi") == 0 || strcmp(target, "both") == 0);

  if (do_modem)
    {
#ifdef CONFIG_OVMS_COMP_CELLULAR
    bool wifi_trig = false;
#ifdef CONFIG_OVMS_COMP_WIFI
    if (!do_wifi && MyNetManager.m_connected_wifi)
      wifi_trig = true;
#endif
    PowerOffModem(wifi_trig);
    writer->puts("Cellular modem powered off by powermgmt.");
#else
    writer->puts("Error: Cellular support not enabled.");
#endif
    }

  if (do_wifi)
    {
#ifdef CONFIG_OVMS_COMP_WIFI
    PowerOffWifi();
    writer->puts("WiFi powered off by powermgmt.");
#else
    writer->puts("Error: WiFi support not enabled.");
#endif
    }
  }

void powermgmt::CommandPowerOn(int verbosity, OvmsWriter* writer, const char* target)
  {
  bool do_modem = (strcmp(target, "modem") == 0 || strcmp(target, "both") == 0);
  bool do_wifi = (strcmp(target, "wifi") == 0 || strcmp(target, "both") == 0);

  m_notcharging_timer = 0;
  m_modemoff_wifi_timer = 0;

  if (do_modem)
    {
#ifdef CONFIG_OVMS_COMP_CELLULAR
    PowerOnModem();
    writer->puts("Cellular modem powered on and delay timers reset.");
#else
    writer->puts("Error: Cellular support not enabled.");
#endif
    }

  if (do_wifi)
    {
#ifdef CONFIG_OVMS_COMP_WIFI
    PowerOnWifi();
    writer->puts("WiFi powered on and delay timers reset.");
#else
    writer->puts("Error: WiFi support not enabled.");
#endif
    }
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
        PowerOnWifi();
        }
#endif
#ifdef CONFIG_OVMS_COMP_CELLULAR
      if (m_modem_off)
        {
        PowerOnModem();
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

#if defined(CONFIG_OVMS_COMP_CELLULAR) && defined(CONFIG_OVMS_COMP_WIFI)
  if (m_modem_wifi_fallback && m_modem_off_wifi && !MyNetManager.m_connected_wifi)
    {
    if (!m_enabled || !(m_modemoff_delay && m_notcharging_timer > m_modemoff_delay*60*60))
      {
      ESP_LOGI(TAG,"WiFi disconnected, powering on cellular modem (fallback)");
      PowerOnModem();
      }
    }
#endif

  if (!m_enabled)
    return;

#ifdef CONFIG_OVMS_COMP_WIFI
  if (m_wifioff_delay && m_notcharging_timer > m_wifioff_delay*60*60) // hours to seconds
    {
    PowerOffWifi();
    }
#endif

#if defined(CONFIG_OVMS_COMP_CELLULAR) && defined(CONFIG_OVMS_COMP_WIFI)
  if (m_modemoff_wifi_delay && m_modemoff_wifi_timer > m_modemoff_wifi_delay*60) // minutes to seconds
    {
    PowerOffModem(true);
    }
#endif

#ifdef CONFIG_OVMS_COMP_CELLULAR
  if (m_modemoff_delay && m_notcharging_timer > m_modemoff_delay*60*60) // hours to seconds
    {
    PowerOffModem(false);
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
