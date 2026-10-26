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
#include "ovms_notify.h"
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

static void pmgmt_powersaving(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* target = (argc > 0) ? argv[0] : nullptr;
  const char* days = (argc > 1) ? argv[1] : nullptr;
  MyPowerMgmt.CommandPowerSaving(verbosity, writer, cmd->GetName(), target, days);
  }

static void pmgmt_powersaving_ctrl(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* action = cmd->GetName();
  if (strcmp(action, "on") == 0)
    {
    MyConfig.SetParamValueBool("power", "powersaving.enabled", true);
    writer->puts("Periodic power saving enabled.");
    }
  else if (strcmp(action, "off") == 0)
    {
    MyConfig.SetParamValueBool("power", "powersaving.enabled", false);
    MyPowerMgmt.EventVehicleOn("", nullptr); // reset active state
    writer->puts("Periodic power saving disabled.");
    }
  }

static void pmgmt_powersaving_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* target = cmd->GetName();
  std::string t(target);
  if (t == "modem" || t == "wifi" || t == "both" || t == "deepsleep")
    {
    if (t == "deepsleep" && argc > 0)
      {
      char* endptr = nullptr;
      long days = strtol(argv[0], &endptr, 10);
      if (endptr == argv[0] || *endptr != '\0' || days < 0)
        {
        writer->puts("Error: Invalid days value. Must be >= 0 (0 = unlimited).");
        return;
        }
      auto lock = MyConfig.Lock();
      MyConfig.SetParamValue("power", "powersaving.target", t);
      MyConfig.SetParamValueInt("power", "powersaving.deepsleep_days", (int)days);
      writer->printf("Periodic power saving target set to: %s (max %ld days)\n", t.c_str(), days);
      }
    else
      {
      MyConfig.SetParamValue("power", "powersaving.target", t);
      writer->printf("Periodic power saving target set to: %s\n", t.c_str());
      }
    }
  else
    {
    writer->puts("Error: Invalid target. Use modem, wifi, both or deepsleep.");
    }
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
  m_powersaving_timer = 0;
  m_modem_wifi_fallback = false;
  m_wifioff_if_disconnected = false;
  m_powersaving_enabled = false;
  m_powersaving_days = POWERMGMT_POWERSAVING_DAYS;
  m_powersaving_deepsleep_days = POWERMGMT_POWERSAVING_DEEPSLEEP_DAYS;
  m_powersaving_target = POWERMGMT_POWERSAVING_TARGET;
  m_powersaving_interval = POWERMGMT_POWERSAVING_INTERVAL;
  m_powersaving_duration = POWERMGMT_POWERSAVING_DURATION;
  m_powersaving_active = false;
  m_powersaving_deepsleep_fallback = false;
  m_powersaving_cycle_timer = 0;
  m_powersaving_awake_phase = false;
  m_charging = false;
  m_modem_off = false;
  m_modem_off_wifi = false;
  m_wifi_off = false;

  MyConfig.RegisterParam("power", "Power management", true, true);
  ConfigChanged("config.mounted", NULL);

  MyEvents.RegisterEvent(TAG, "vehicle.on", std::bind(&powermgmt::EventVehicleOn, this, _1, _2));

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
  OvmsCommand* cmd_powersaving = cmd_pmgmt->RegisterCommand("powersaving", "Configure or toggle periodic power saving mode");
  cmd_powersaving->RegisterCommand("status", "Show periodic power saving status", pmgmt_powersaving, "", 0, 0, false);
  cmd_powersaving->RegisterCommand("on", "Activate periodic power saving immediately", pmgmt_powersaving, "[<modem|wifi|both|deepsleep> [<days>]]", 0, 2, true);
  cmd_powersaving->RegisterCommand("off", "Deactivate periodic power saving immediately", pmgmt_powersaving, "", 0, 0, true);
  OvmsCommand* cmd_powersaving_set = cmd_powersaving->RegisterCommand("set", "Set periodic power saving target");
  cmd_powersaving_set->RegisterCommand("modem", "Set target to cellular modem", pmgmt_powersaving_set, "", 0, 0, true);
  cmd_powersaving_set->RegisterCommand("wifi", "Set target to WiFi", pmgmt_powersaving_set, "", 0, 0, true);
  cmd_powersaving_set->RegisterCommand("both", "Set target to both modem and WiFi", pmgmt_powersaving_set, "", 0, 0, true);
  cmd_powersaving_set->RegisterCommand("deepsleep", "Set target to deep sleep (optional: max days)", pmgmt_powersaving_set, "[<days>]", 0, 1, true);
  OvmsCommand* cmd_powersaving_ctrl = cmd_powersaving->RegisterCommand("ctrl", "Enable or disable periodic power saving mode");
  cmd_powersaving_ctrl->RegisterCommand("on", "Enable periodic power saving", pmgmt_powersaving_ctrl, "", 0, 0, true);
  cmd_powersaving_ctrl->RegisterCommand("off", "Disable periodic power saving", pmgmt_powersaving_ctrl, "", 0, 0, true);

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
    m_wifioff_if_disconnected = MyConfig.GetParamValueBool("power", "wifioff_if_disconnected", false);
    if (MyConfig.ismounted() && !MyConfig.IsDefined("power", "wifioff_if_disconnected"))
      {
      MyConfig.SetParamValueBool("power", "wifioff_if_disconnected", m_wifioff_if_disconnected);
      }
    m_12v_shutdown_delay = MyConfig.GetParamValueInt("power", "12v_shutdown_delay", POWERMGMT_12V_SHUTDOWN_DELAY);
    m_powersaving_enabled = MyConfig.GetParamValueBool("power", "powersaving.enabled", false);
    m_powersaving_days = MyConfig.GetParamValueInt("power", "powersaving.days", POWERMGMT_POWERSAVING_DAYS);
    m_powersaving_deepsleep_days = MyConfig.GetParamValueInt("power", "powersaving.deepsleep_days", POWERMGMT_POWERSAVING_DEEPSLEEP_DAYS);
    m_powersaving_target = MyConfig.GetParamValue("power", "powersaving.target", POWERMGMT_POWERSAVING_TARGET);
    m_powersaving_interval = MyConfig.GetParamValueInt("power", "powersaving.interval", POWERMGMT_POWERSAVING_INTERVAL);
    m_powersaving_duration = MyConfig.GetParamValueInt("power", "powersaving.duration", POWERMGMT_POWERSAVING_DURATION);

    // Restore timers from parktime after reboot if feature enabled
    int park_secs = StandardMetrics.ms_v_env_parktime->AsInt(0);
    if (m_enabled && park_secs > 1)
      m_notcharging_timer = park_secs;
    if (!m_powersaving_enabled)
      {
      m_powersaving_active = false;
      m_powersaving_awake_phase = false;
      m_powersaving_deepsleep_fallback = false;
      }
    else if (park_secs > 1)
      {
      m_powersaving_timer = park_secs;
      if (m_powersaving_target == "deepsleep" && m_powersaving_deepsleep_days > 0 &&
          m_powersaving_timer >= (m_powersaving_days + m_powersaving_deepsleep_days) * 24 * 60 * 60)
        {
        m_powersaving_deepsleep_fallback = true;
        m_powersaving_target = "both";
        }
      else
        {
        m_powersaving_deepsleep_fallback = false;
        }
      if (event == "config.mounted" && m_powersaving_timer >= m_powersaving_days * 24 * 60 * 60)
        {
        m_powersaving_active = true;
        m_powersaving_cycle_timer = 0;
        m_powersaving_awake_phase = false;
        }
      }
    }
  }

void powermgmt::EventVehicleOn(std::string event, void* data)
  {
  if (m_powersaving_active || m_powersaving_timer > 0 || m_powersaving_deepsleep_fallback)
    {
    ESP_LOGI(TAG, "Vehicle ON event received, resetting power saving timer and mode.");
    m_powersaving_active = false;
    m_powersaving_deepsleep_fallback = false;
    m_powersaving_timer = 0;
    m_powersaving_cycle_timer = 0;
    m_powersaving_awake_phase = false;
    m_powersaving_target = MyConfig.GetParamValue("power", "powersaving.target", POWERMGMT_POWERSAVING_TARGET);
    }
  }

void powermgmt::GetEstimatedConsumption(powermgmt_consumption_t& out)
  {
  // Approximate OVMS v3.3 current draw at ~12.6V in mA based on official hardware specs
  // https://docs.openvehicles.com/en/latest/userguide/warnings.html
  out = {};

  out.base_ma = POWERMGMT_EST_BASE; // Base System

  if (MyPeripherals)
    {
#ifdef CONFIG_OVMS_COMP_WIFI
    if (MyPeripherals->m_esp32wifi && MyPeripherals->m_esp32wifi->GetPowerMode() == On)
      {
      esp32wifi_mode_t wmode = MyPeripherals->m_esp32wifi->GetMode();
      if (wmode == ESP32WIFI_MODE_AP || wmode == ESP32WIFI_MODE_APCLIENT)
        out.wifi_ma = POWERMGMT_EST_WIFI_AP; // Wifi AP Mode
      else if (wmode != ESP32WIFI_MODE_OFF)
        out.wifi_ma = POWERMGMT_EST_WIFI_CLIENT;  // Wifi Client
      }
#endif
#ifdef CONFIG_OVMS_COMP_CELLULAR
    if (MyPeripherals->m_cellular_modem)
      {
      PowerMode mpm = MyPeripherals->m_cellular_modem->GetPowerMode();
      if (mpm == On)
        {
        out.modem_ma = POWERMGMT_EST_MODEM; // Modem
        if (MyPeripherals->m_cellular_modem->m_nmea != NULL)
          out.gps_ma = POWERMGMT_EST_GPS; // GPS
        }
      else if (mpm == Sleep)
        {
        out.modem_ma = POWERMGMT_EST_MODEM_SLEEP; // Modem in Sleep mode
        }
      }
#endif
    }

  out.total_ma = out.base_ma + out.wifi_ma + out.modem_ma + out.gps_ma;
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

  const char* wifi_pm = "n/a";
  const char* wifi_state = "disabled";
  const char* modem_pm = "n/a";
  const char* modem_state = "idle";
  const char* gps_pm = "n/a";

  if (MyPeripherals)
    {
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
    }

  writer->printf("12V Battery:       %.2f V (%s)\n", v12, chg12 ? "charging" : "not charging");
  if (StandardMetrics.ms_v_bat_12v_current->IsDefined())
    writer->printf("12V Current (Veh): %.2f A\n", StandardMetrics.ms_v_bat_12v_current->AsFloat());
  writer->printf("Timers:            not charging %u min, WiFi modem-off %u min, 12V alert %u min\n",
    m_notcharging_timer / 60, m_modemoff_wifi_timer / 60, m_12v_alert_timer / 60);
  writer->printf("Flags:             modem off = %s, modem off wifi connected = %s, wifi off = %s, wifi off if disconnected = %s\n\n",
    m_modem_off ? "yes" : "no", m_modem_off_wifi ? "yes" : "no", m_wifi_off ? "yes" : "no", m_wifioff_if_disconnected ? "yes" : "no");

  writer->printf("%-26s %-26s %12s %10s\n", "Consumer", "State", "Est. Current", "Est. Power");
  writer->printf("------------------------------------------------------------------------------\n");
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "Base System", "on",
    cons.base_ma, v12_calc * (cons.base_ma / 1000.0f));
  char wifi_buf[36];
  snprintf(wifi_buf, sizeof(wifi_buf), "%s (%s)", wifi_pm, wifi_state);
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "WiFi", wifi_buf,
    cons.wifi_ma, v12_calc * (cons.wifi_ma / 1000.0f));
  char modem_buf[36];
  snprintf(modem_buf, sizeof(modem_buf), "%s (%s)", modem_pm, modem_state);
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "Modem", modem_buf,
    cons.modem_ma, v12_calc * (cons.modem_ma / 1000.0f));
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "GPS", gps_pm,
    cons.gps_ma, v12_calc * (cons.gps_ma / 1000.0f));
  float amp = cons.total_ma / 1000.0f;
  float ah_day = amp * 24.0f;
  float ah_week = ah_day * 7.0f;
  float ah_month = ah_day * 30.0f;

  writer->printf("------------------------------------------------------------------------------\n");
  writer->printf("%-26s %-26s %9.0f mA %8.2f W\n", "Total Estimated OVMS Draw", "",
    cons.total_ma, est_w);
  writer->printf("Estimated Consumption:    Day: %.2f Ah | Week: %.2f Ah | Month: %.2f Ah\n\n",
    ah_day, ah_week, ah_month);
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
  writer->printf("  WiFi off only when disconnected:    %s\n", m_wifioff_if_disconnected ? "yes" : "no");
#endif
  writer->printf("  12V alert shutdown delay:           %u minutes (0 = disabled)\n", m_12v_shutdown_delay);
  writer->printf("  Periodic power saving enabled:      %s\n", m_powersaving_enabled ? "yes" : "no");
  writer->printf("  Periodic power saving days:         %u days\n", m_powersaving_days);
  writer->printf("  Periodic power saving target:       %s\n", m_powersaving_target.c_str());
  writer->printf("  Periodic deep sleep limit:          %u days (0 = unlimited)\n", m_powersaving_deepsleep_days);
  writer->printf("  Periodic power saving interval:     %u hours\n", m_powersaving_interval);
  writer->printf("  Periodic power saving duration:     %u minutes\n", m_powersaving_duration);
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
      vTaskDelay(2000 / portTICK_PERIOD_MS); // make sure all notifications are transmitted before powering down WiFi
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
      vTaskDelay(2000 / portTICK_PERIOD_MS); // make sure all notifications are transmitted before powering down modem
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
  m_powersaving_timer = 0;
  m_powersaving_active = false;
  m_powersaving_deepsleep_fallback = false;
  m_powersaving_awake_phase = false;
  m_powersaving_target = MyConfig.GetParamValue("power", "powersaving.target", POWERMGMT_POWERSAVING_TARGET);

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

void powermgmt::CommandPowerSaving(int verbosity, OvmsWriter* writer, const char* action, const char* target, const char* days_str)
  {
  auto lock = MyConfig.Lock();
  if (strcmp(action, "on") == 0)
    {
    if (target && *target)
      {
      std::string t(target);
      if (t == "modem" || t == "wifi" || t == "both" || t == "deepsleep")
        {
        if (days_str && *days_str)
          {
          if (t != "deepsleep")
            {
            writer->puts("Error: <days> parameter is only valid with target 'deepsleep'.");
            return;
            }
          char* endptr = nullptr;
          long days = strtol(days_str, &endptr, 10);
          if (endptr == days_str || *endptr != '\0' || days < 0)
            {
            writer->puts("Error: Invalid days value. Must be >= 0 (0 = unlimited).");
            return;
            }
          m_powersaving_deepsleep_days = (unsigned int)days;
          MyConfig.SetParamValueInt("power", "powersaving.deepsleep_days", m_powersaving_deepsleep_days);
          }
        m_powersaving_target = t;
        MyConfig.SetParamValue("power", "powersaving.target", m_powersaving_target);
        if (t == "deepsleep" && days_str && *days_str)
          writer->printf("Periodic power saving target set to: %s (max %u days)\n", m_powersaving_target.c_str(), m_powersaving_deepsleep_days);
        else
          writer->printf("Periodic power saving target set to: %s\n", m_powersaving_target.c_str());
        }
      else
        {
        writer->puts("Error: Invalid target. Use modem, wifi, both or deepsleep.");
        return;
        }
      }

    m_powersaving_active = true;
    m_powersaving_deepsleep_fallback = false;
    m_powersaving_timer = m_powersaving_days * 24 * 60 * 60;
    m_powersaving_cycle_timer = 0;
    m_powersaving_awake_phase = false;
    MyConfig.SetParamValueBool("power", "powersaving.enabled", true);
    writer->puts("Periodic power saving mode activated immediately.");
    }
  else if (strcmp(action, "off") == 0)
    {
    m_powersaving_active = false;
    m_powersaving_deepsleep_fallback = false;
    m_powersaving_timer = 0;
    m_powersaving_awake_phase = false;
    m_powersaving_target = MyConfig.GetParamValue("power", "powersaving.target", POWERMGMT_POWERSAVING_TARGET);
    MyConfig.SetParamValueBool("power", "powersaving.enabled", false);
    writer->puts("Periodic power saving mode deactivated.");
    }
  else
    {
    std::string active_target = m_powersaving_deepsleep_fallback ? "both (fallback from deepsleep)" : m_powersaving_target;
    writer->printf("Periodic Power Saving Status:\n");
    writer->printf("  Enabled in config: %s\n", m_powersaving_enabled ? "yes" : "no");
    writer->printf("  Currently active:  %s\n", m_powersaving_active ? "yes" : "no");
    writer->printf("  Target component:  %s\n", active_target.c_str());
    writer->printf("  Days threshold:    %u\n", m_powersaving_days);
    writer->printf("  Deep sleep limit:  %u days (0 = unlimited)\n", m_powersaving_deepsleep_days);
    writer->printf("  Interval:          %u hours\n", m_powersaving_interval);
    writer->printf("  Duration:          %u minutes\n", m_powersaving_duration);
    writer->printf("  Timer progress:    %u / %u seconds\n", m_powersaving_timer, m_powersaving_days * 24 * 60 * 60);
    writer->printf("  Awake phase now:   %s\n", m_powersaving_awake_phase ? "yes" : "no");
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

  // Periodic power saving inactive tracking (independent of 12V charging / trickle charging)
  if (!m_powersaving_active)
    {
    if (m_powersaving_enabled)
      {
      m_powersaving_timer++;
      if (m_powersaving_timer >= m_powersaving_days * 24 * 60 * 60)
        {
        MyNotify.NotifyStringf("info", "powermgmt.powersaving.active",
          "Periodic power saving mode activated after %u days of inactivity (target: %s).",
          m_powersaving_days, m_powersaving_target.c_str());
        ESP_LOGI(TAG, "Power saving inactivity threshold of %u days reached, entering periodic power saving mode.", m_powersaving_days);
        vTaskDelay(2000 / portTICK_PERIOD_MS); // make sure all notifications are transmitted before entering power saving mode
        m_powersaving_active = true;
        m_powersaving_cycle_timer = 0;
        m_powersaving_awake_phase = false;
        if (m_powersaving_target == "deepsleep" && m_powersaving_deepsleep_days > 0 &&
            m_powersaving_timer >= (m_powersaving_days + m_powersaving_deepsleep_days) * 24 * 60 * 60)
          {
          m_powersaving_deepsleep_fallback = true;
          m_powersaving_target = "both";
          }
        }
      }
    }
  else
    {
    m_powersaving_timer++;
    if (m_powersaving_target == "deepsleep" && m_powersaving_deepsleep_days > 0 &&
        m_powersaving_timer >= (m_powersaving_days + m_powersaving_deepsleep_days) * 24 * 60 * 60)
      {
      MyNotify.NotifyStringf("info", "powermgmt.powersaving.fallback",
        "Deep sleep limit of %u days reached, continuing periodic power saving with target 'both' until vehicle ON.",
        m_powersaving_deepsleep_days);
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      ESP_LOGI(TAG, "Deep sleep limit of %u days reached, switching power saving target to 'both' until vehicle ON.",
        m_powersaving_deepsleep_days);
      m_powersaving_deepsleep_fallback = true;
      m_powersaving_target = "both";      
      }

    // Duty-cycle execution
    m_powersaving_cycle_timer++;
    unsigned int interval_secs = (m_powersaving_interval > 0 ? m_powersaving_interval : POWERMGMT_POWERSAVING_INTERVAL) * 60 * 60;
    unsigned int duration_secs = (m_powersaving_duration > 0 ? m_powersaving_duration : POWERMGMT_POWERSAVING_DURATION) * 60;
    if (duration_secs >= interval_secs)
      duration_secs = interval_secs / 2;
    unsigned int pos_in_cycle = m_powersaving_cycle_timer % interval_secs;

    if (pos_in_cycle < duration_secs)
      {
      if (!m_powersaving_awake_phase)
        {
        ESP_LOGI(TAG, "Power saving duty-cycle: waking up target components (%s) for %u minutes",
          m_powersaving_target.c_str(), duration_secs / 60);
        m_powersaving_awake_phase = true;
        if (m_powersaving_target == "modem" || m_powersaving_target == "both" || m_powersaving_target == "deepsleep")
          PowerOnModem();
        if (m_powersaving_target == "wifi" || m_powersaving_target == "both" || m_powersaving_target == "deepsleep")
          PowerOnWifi();
        }
      }
    else
      {
      if (m_powersaving_awake_phase)
        {
        m_powersaving_awake_phase = false;
        if (m_powersaving_target == "deepsleep")
          {
          if (!MyBoot.IsShuttingDown())
            {
            unsigned int sleep_secs = interval_secs - duration_secs;
            int park_secs = StandardMetrics.ms_v_env_parktime->AsInt(0);
            if ((unsigned int)park_secs < m_powersaving_timer)
              park_secs = m_powersaving_timer;
            StandardMetrics.ms_v_env_parktime->SetValue(park_secs + (int)sleep_secs);
            ESP_LOGI(TAG, "Power saving duty-cycle: entering deep sleep for %u seconds", sleep_secs);
            MyBoot.DeepSleep(sleep_secs);
            }
          }
        else
          {
          ESP_LOGI(TAG, "Power saving duty-cycle: putting target components back to sleep (%s)", m_powersaving_target.c_str());
          bool do_modem = (m_powersaving_target == "modem" || m_powersaving_target == "both");
          bool do_wifi = (m_powersaving_target == "wifi" || m_powersaving_target == "both");
          if (do_modem) PowerOffModem(false);
          if (do_wifi) PowerOffWifi();
          }
        }
      }
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

  bool ps_keep_wifi = m_powersaving_active && m_powersaving_awake_phase &&
    (m_powersaving_target == "wifi" || m_powersaving_target == "both" || m_powersaving_target == "deepsleep");
  bool ps_keep_modem = m_powersaving_active && m_powersaving_awake_phase &&
    (m_powersaving_target == "modem" || m_powersaving_target == "both" || m_powersaving_target == "deepsleep");

#ifdef CONFIG_OVMS_COMP_WIFI
  if (!ps_keep_wifi && m_wifioff_delay && m_notcharging_timer > m_wifioff_delay*60*60) // hours to seconds
    {
    if (!m_wifioff_if_disconnected || !MyNetManager.m_connected_wifi)
      {
      PowerOffWifi();
      }
    }
#endif

#if defined(CONFIG_OVMS_COMP_CELLULAR) && defined(CONFIG_OVMS_COMP_WIFI)
  if (!ps_keep_modem && m_modemoff_wifi_delay && m_modemoff_wifi_timer > m_modemoff_wifi_delay*60) // minutes to seconds
    {
    PowerOffModem(true);
    }
#endif

#ifdef CONFIG_OVMS_COMP_CELLULAR
  if (!ps_keep_modem && m_modemoff_delay && m_notcharging_timer > m_modemoff_delay*60*60) // hours to seconds
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
