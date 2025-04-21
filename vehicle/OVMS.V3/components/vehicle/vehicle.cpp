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

#include "ovms_log.h"
static const char *TAG = "vehicle";
// static const char *TAGRX = "vehicle-rx";
static const char *CHECK_SHUTDOWN_TAG = "vehicle-shutdown";

#include <stdio.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ovms_command.h>
#include <ovms_script.h>
#include <ovms_metrics.h>
#include <ovms_notify.h>
#include <metrics_standard.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_webserver.h>
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_peripherals.h>
#include <ovms_boot.h>
#include <string_writer.h>
#include "vehicle.h"

#ifdef bind
#undef bind
#endif
using namespace std::placeholders;

OvmsVehicleFactory MyVehicleFactory __attribute__ ((init_priority (2000)));


OvmsVehicleFactory::OvmsVehicleFactory()
  {
  ESP_LOGI(TAG, "Initialising VEHICLE Factory (2000)");

  m_currentvehicle = NULL;
  m_currentvehicletype.clear();

  MyEvents.RegisterEvent(TAG,"system.shuttingdown",std::bind(&OvmsVehicleFactory::EventSystemShuttingDown, this, _1, _2));

  OvmsCommand* cmd_vehicle = MyCommandApp.RegisterCommand("vehicle","Vehicle framework", vehicle_status, "", 0, 0, false);
  cmd_vehicle->RegisterCommand("module","Set (or clear) vehicle module",vehicle_module,"<type>",0,1,true,vehicle_validate);
  cmd_vehicle->RegisterCommand("list","Show list of available vehicle modules",vehicle_list);
  cmd_vehicle->RegisterCommand("status","Show vehicle module status",vehicle_status);

  MyCommandApp.RegisterCommand("wakeup","Wake up vehicle",vehicle_wakeup);
  MyCommandApp.RegisterCommand("homelink","Activate specified homelink button",vehicle_homelink,"<homelink> [<duration=1000ms>]",1,2);
  OvmsCommand* cmd_climate = MyCommandApp.RegisterCommand("climatecontrol","(De)Activate Climate Control");
  cmd_climate->RegisterCommand("on","Activate Climate Control",vehicle_climatecontrol_on);
  cmd_climate->RegisterCommand("off","Deactivate Climate Control",vehicle_climatecontrol_off);
  MyCommandApp.RegisterCommand("lock","Lock vehicle",vehicle_lock,"<pin>",1,1);
  MyCommandApp.RegisterCommand("unlock","Unlock vehicle",vehicle_unlock,"<pin>",1,1);
  MyCommandApp.RegisterCommand("valet","Activate valet mode",vehicle_valet,"<pin>",1,1);
  MyCommandApp.RegisterCommand("unvalet","Deactivate valet mode",vehicle_unvalet,"<pin>",1,1);
  OvmsCommand *cmd_aux = cmd_vehicle->RegisterCommand("aux", "Aux battery", vehicle_aux);
  cmd_aux->RegisterCommand("status", "Aux Battery Status", vehicle_aux);
  OvmsCommand *cmd_aux_mon = cmd_aux->RegisterCommand("monitor", "Aux Battery Monitor", vehicle_aux_monitor);
  cmd_aux_mon->RegisterCommand("status", "Aux Battery Status", vehicle_aux_monitor);
  cmd_aux_mon->RegisterCommand("enable", "Enable Aux Monitor", vehicle_aux_monitor_enable, "[<low-threshold> [<charging-threshold>]]", 0, 2);
  cmd_aux_mon->RegisterCommand("disable", "Disable Aux Monitor", vehicle_aux_monitor_disable);


  OvmsCommand* cmd_charge = MyCommandApp.RegisterCommand("charge","Charging framework");
  OvmsCommand* cmd_chargemode = cmd_charge->RegisterCommand("mode","Set vehicle charge mode");
  cmd_chargemode->RegisterCommand("standard","Set vehicle standard charge mode",vehicle_charge_mode);
  cmd_chargemode->RegisterCommand("storage","Set vehicle standard charge mode",vehicle_charge_mode);
  cmd_chargemode->RegisterCommand("range","Set vehicle standard charge mode",vehicle_charge_mode);
  cmd_chargemode->RegisterCommand("performance","Set vehicle standard charge mode",vehicle_charge_mode);
  cmd_charge->RegisterCommand("start","Start a vehicle charge",vehicle_charge_start);
  cmd_charge->RegisterCommand("stop","Stop a vehicle charge",vehicle_charge_stop);
  cmd_charge->RegisterCommand("current","Limit charge current",vehicle_charge_current,"<amps>",1,1);
  cmd_charge->RegisterCommand("cooldown","Start a vehicle cooldown",vehicle_charge_cooldown);

  OvmsCommand* cmd_stat = MyCommandApp.RegisterCommand("stat","Show vehicle status",vehicle_stat);
  cmd_stat->RegisterCommand("trip","Show trip status",vehicle_stat_trip);

  OvmsCommand* cmd_bms = MyCommandApp.RegisterCommand("bms","BMS framework", bms_status, "", 0, 0, false);
  cmd_bms->RegisterCommand("status","Show BMS status",bms_status);
  cmd_bms->RegisterCommand("temp","Show BMS temperature status",bms_status);
  cmd_bms->RegisterCommand("volt","Show BMS voltage status",bms_status);
  cmd_bms->RegisterCommand("reset","Reset BMS statistics",bms_reset);
  cmd_bms->RegisterCommand("alerts","Show BMS alerts",bms_alerts);

  OvmsCommand* cmd_obdii = MyCommandApp.RegisterCommand("obdii", "OBDII framework");
  for (int k=1; k <= 4; k++)
    {
    static const char* name[4] = { "can1", "can2", "can3", "can4" };
    OvmsCommand* cmd_canx = cmd_obdii->RegisterCommand(name[k-1], "select bus");

    OvmsCommand* cmd_obdreq = cmd_canx->RegisterCommand(
      "request", "Send OBD2/UDS request, output response");
    cmd_obdreq->RegisterCommand(
      "device", "Send OBD2/ISOTP request to a device", obdii_request,
      "[-e|-E|-v] [-t<timeout_ms>] <txid> <rxid> <request>\n"
      "Give <txid> and <rxid> as hexadecimal CAN IDs,"
      " add -e to use ISO-TP extended addressing (19 bit IDs via standard frames)\n"
      " or -E to use ISO-TP via extended frames (29 bit IDs)\n"
      " or -v to use VW-TP 2.0 (VW/VAG specific transport protocol, txid=200, rxid=ECUID).\n"
      "<request> is the hex string of the request type + arguments,"
      " e.g. '223a4b' = read data from PID 0x3a4b.\n"
      "Default timeout is 3000 ms.",
      3, 5);
    cmd_obdreq->RegisterCommand(
      "broadcast", "Send OBD2/UDS request as broadcast", obdii_request,
      "[-t<timeout_ms>] <request>\n"
      "Sends the request to broadcast ID 7df, listens on IDs 7e8-7ef.\n"
      "Note: only the first response will be shown, enable CAN log to check for more.\n"
      "<request> is the hex string of the request type + arguments,"
      " e.g. '223a4b' = read data from PID 0x3a4b.\n"
      "Default timeout is 3000 ms.",
      1, 2);
    }


#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  DuktapeObjectRegistration* dto = new DuktapeObjectRegistration("OvmsVehicle");
  dto->RegisterDuktapeFunction(DukOvmsVehicleType, 0, "Type");
  dto->RegisterDuktapeFunction(DukOvmsVehicleWakeup, 0, "Wakeup");
  dto->RegisterDuktapeFunction(DukOvmsVehicleHomelink, 2, "Homelink");
  dto->RegisterDuktapeFunction(DukOvmsVehicleClimateControl, 1, "ClimateControl");
  dto->RegisterDuktapeFunction(DukOvmsVehicleLock, 1, "Lock");
  dto->RegisterDuktapeFunction(DukOvmsVehicleUnlock, 1, "Unlock");
  dto->RegisterDuktapeFunction(DukOvmsVehicleValet, 1, "Valet");
  dto->RegisterDuktapeFunction(DukOvmsVehicleUnvalet, 1, "Unvalet");
  dto->RegisterDuktapeFunction(DukOvmsVehicleSetChargeMode, 1, "SetChargeMode");
  dto->RegisterDuktapeFunction(DukOvmsVehicleSetChargeCurrent, 1, "SetChargeCurrent");
  dto->RegisterDuktapeFunction(DukOvmsVehicleSetChargeTimer, 2, "SetChargeTimer");
  dto->RegisterDuktapeFunction(DukOvmsVehicleStopCharge, 0, "StopCharge");
  dto->RegisterDuktapeFunction(DukOvmsVehicleStartCharge, 0, "StartCharge");
  dto->RegisterDuktapeFunction(DukOvmsVehicleStartCooldown, 0, "StartCooldown");
  dto->RegisterDuktapeFunction(DukOvmsVehicleStopCooldown, 0, "StopCooldown");
  dto->RegisterDuktapeFunction(DukOvmsVehicleObdRequest, 1, "ObdRequest");

  DuktapeObjectRegistration* dto_aux= new DuktapeObjectRegistration("OvmsAuxMonitor");;
  dto_aux->RegisterDuktapeFunction(DukOvmsVehicleAuxMonEnable, DUK_VARARGS /*0..2*/, "Enable");
  dto_aux->RegisterDuktapeFunction(DukOvmsVehicleAuxMonDisable, 0, "Disable");
  dto_aux->RegisterDuktapeFunction(DukOvmsVehicleAuxMonStatus, 0, "Status");
  dto->RegisterDuktapeObject(dto_aux, "AuxMon");

  MyDuktape.RegisterDuktapeObject(dto);
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }

OvmsVehicleFactory::~OvmsVehicleFactory()
  {
  MyEvents.DeregisterEvent(TAG);
  MyEvents.DeregisterEvent(CHECK_SHUTDOWN_TAG);
  // Should be shutdown properly
  if (m_currentvehicle)
    delete m_currentvehicle;
  auto it = m_pending_shutdown.begin();
  while (it != m_pending_shutdown.end())
    {
    auto vehicle = (*it);
    it = m_pending_shutdown.erase(it);
    delete vehicle;
    }
  }

void OvmsVehicleFactory::EventSystemShuttingDown(std::string event, void* data)
  {
  MyBoot.ShutdownPending(TAG);
  MyEvents.RegisterEvent(CHECK_SHUTDOWN_TAG,"ticker.1",std::bind(&OvmsVehicleFactory::EventTicker1ShuttingDown, this, _1, _2));
  DoClearVehicle(false, false, false/*dont wait*/);
  }

void OvmsVehicleFactory::EventTicker1ShuttingDown(std::string event, void* data)
  {
  bool iscleared = true;
  auto it = m_pending_shutdown.begin();
  while (it != m_pending_shutdown.end())
    {
    if ((*it)->IsShutdown())
      {
      auto vehicle = (*it);
      it = m_pending_shutdown.erase(it);
      delete vehicle;
      }
    else
      {
      iscleared = false;
      ++it;
      }
    }
  if (iscleared)
    {
    MyEvents.DeregisterEvent(CHECK_SHUTDOWN_TAG);
    if (MyBoot.IsShuttingDown())
      MyBoot.ShutdownReady(TAG);
    }
  }

OvmsVehicle* OvmsVehicleFactory::NewVehicle(const char* VehicleType)
  {
  OvmsVehicleFactory::map_vehicle_t::iterator iter = m_vmap.find(VehicleType);
  if (iter != m_vmap.end())
    {
    return iter->second.construct();
    }
  return NULL;
  }

void OvmsVehicleFactory::ClearVehicle()
  {
  DoClearVehicle(true, true, true);
  }

void OvmsVehicleFactory::DoClearVehicle( bool clearName, bool sendEvent, bool wait)
  {
  if (m_currentvehicle)
    {
    m_currentvehicle->ShuttingDown();
    auto vehicle = m_currentvehicle;
    m_currentvehicle = NULL;

    if (wait)
      {
      int repeat = 20; // 1s
      while (!vehicle->IsShutdown() && repeat--)
        vTaskDelay(pdMS_TO_TICKS(50));
      }

    m_currentvehicletype.clear();
    if (clearName)
      StandardMetrics.ms_v_type->SetValue("");
    if (sendEvent)
      MyEvents.SignalEvent("vehicle.type.cleared", NULL);

    if (vehicle->IsShutdown())
      delete vehicle;
    else
      {
      if (m_pending_shutdown.empty())
        {
        MyEvents.DeregisterEvent(CHECK_SHUTDOWN_TAG);
        MyEvents.RegisterEvent(CHECK_SHUTDOWN_TAG,"ticker.1",std::bind(&OvmsVehicleFactory::EventTicker1ShuttingDown, this, _1, _2));
        }
      m_pending_shutdown.push_back(vehicle);
      }
    }
  }

void OvmsVehicleFactory::SetVehicle(const char* type)
  {
  DoClearVehicle(false, true, true);
  m_currentvehicle = NewVehicle(type);
  if (m_currentvehicle)
    {
    std::string new_type(type);
    m_currentvehicletype = new_type;
    StandardMetrics.ms_v_type->SetValue(m_currentvehicletype);

    m_currentvehicle->StartingUp();

    MyEvents.SignalEvent("vehicle.type.set", (void*)new_type.c_str(), new_type.size()+1);
    }
  else
    {
    StandardMetrics.ms_v_type->SetValue("");
    }
  }

void OvmsVehicleFactory::AutoInit()
  {
  std::string type = MyConfig.GetParamValue("auto", "vehicle.type");
  if (!type.empty())
    SetVehicle(type.c_str());
  }

OvmsVehicle* OvmsVehicleFactory::ActiveVehicle()
  {
  return m_currentvehicle;
  }

const char* OvmsVehicleFactory::ActiveVehicleType()
  {
  return m_currentvehicletype.c_str();
  }

const char* OvmsVehicleFactory::ActiveVehicleName()
  {
  map_vehicle_t::iterator it = m_vmap.find(m_currentvehicletype.c_str());
  if (it != m_vmap.end())
    return it->second.name;
  return "";
  }

const char* OvmsVehicleFactory::ActiveVehicleShortName()
  {
  return m_currentvehicle ? m_currentvehicle->VehicleShortName() : "";
  }

OvmsVehicle::OvmsVehicle()
  {

  m_is_shutdown = false;

  m_can1 = NULL;
  m_can2 = NULL;
  m_can3 = NULL;
  m_can4 = NULL;

  m_last_chargetime = 0;
  m_last_drivetime = 0;
  m_last_gentime = 0;
  m_last_parktime = 0;

  m_drive_startsoc = StdMetrics.ms_v_bat_soc->AsFloat();
  m_drive_startrange = StdMetrics.ms_v_bat_range_est->AsFloat();
  m_drive_startaltitude = StdMetrics.ms_v_pos_altitude->AsFloat();
  time(&m_drive_starttime);
  m_drive_speedcnt = 0;
  m_drive_speedsum = 0;
  m_drive_accelcnt = 0;
  m_drive_accelsum = 0;
  m_drive_decelcnt = 0;
  m_drive_decelsum = 0;

  m_ticker = 0;
  m_12v_ticker = 0;
  m_12v_low_ticker = 0;
  m_12v_shutdown_ticker = 0;
  m_chargestate_ticker = 0;
  m_vehicleon_ticker = 0;
  m_vehicleoff_ticker = 0;
  m_idle_ticker = 0;
  m_autonotifications = true;
  m_ready = false;

#ifdef CONFIG_OVMS_COMP_POLLER
  m_poll_state = 0;
  m_pollsignal = nullptr;
  m_poll_bus_default = nullptr;

  // Poll parameters.
  PollSetThrottling(1);
  // response default timing: 25 milliseconds
  PollSetResponseSeparationTime(25);
  // channel keepalive default: 60 seconds
  PollSetChannelKeepalive(60);
#endif

  m_bms_voltages = NULL;
  m_bms_vmins = NULL;
  m_bms_vmaxs = NULL;
  m_bms_vdevmaxs = NULL;
  m_bms_valerts = NULL;
  m_bms_valerts_new = 0;
  m_bms_vstddev_cnt = 0;
  m_bms_vstddev_avg = 0;
  m_bms_has_voltages = false;

  m_bms_temperatures = NULL;
  m_bms_tmins = NULL;
  m_bms_tmaxs = NULL;
  m_bms_tdevmaxs = NULL;
  m_bms_talerts = NULL;
  m_bms_talerts_new = 0;
  m_bms_has_temperatures = false;

  m_bms_bitset_v.clear();
  m_bms_bitset_t.clear();
  m_bms_bitset_cv = 0;
  m_bms_bitset_ct = 0;
  m_bms_readings_v = 0;
  m_bms_readingspermodule_v = 0;
  m_bms_readings_t = 0;
  m_bms_readingspermodule_t = 0;

  m_bms_limit_tmin = -1000;
  m_bms_limit_tmax = 1000;
  m_bms_limit_vmin = -1000;
  m_bms_limit_vmax = 1000;

  m_bms_defthr_vmaxgrad   = BMS_DEFTHR_VMAXGRAD;
  m_bms_defthr_vmaxsddev  = BMS_DEFTHR_VMAXSDDEV;
  m_bms_defthr_vwarn      = BMS_DEFTHR_VWARN;
  m_bms_defthr_valert     = BMS_DEFTHR_VALERT;
  m_bms_defthr_twarn      = BMS_DEFTHR_TWARN;
  m_bms_defthr_talert     = BMS_DEFTHR_TALERT;

  m_bms_vlog_last = 0;
  m_bms_tlog_last = 0;

  m_minsoc = 0;
  m_minsoc_triggered = 0;

  m_accel_refspeed = 0;
  m_accel_reftime = 0;
  m_accel_smoothing = 2.0;

  m_batpwr_smoothing = 2.0;
  m_batpwr_smoothed = 0;

  m_brakelight_enable = false;
  m_brakelight_on = 1.3;
  m_brakelight_off = 0.7;
  m_brakelight_port = 1;
  m_brakelight_start = 0;
  m_brakelight_basepwr = 0;
  m_brakelight_ignftbrk = false;

  m_tpms_lastcheck = 0;
  m_inv_energyused = 0;
  m_inv_energyrecd = 0;

#ifdef CONFIG_OVMS_COMP_POLLER
  MyPollers.RegisterRunFinished(TAG, std::bind(&OvmsVehicle::PollRunFinishedNotify, this, _1, _2));
  MyPollers.RegisterPollStateTicker(TAG, std::bind(&OvmsVehicle::PollerStateTickerNotify, this, _1, _2));
#endif

  MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&OvmsVehicle::VehicleTicker1, this, _1, _2));

  MyEvents.RegisterEvent(TAG, "config.changed", std::bind(&OvmsVehicle::VehicleConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.mounted", std::bind(&OvmsVehicle::VehicleConfigChanged, this, _1, _2));
  VehicleConfigChanged("config.mounted", NULL);

  MyMetrics.RegisterListener(TAG, "*", std::bind(&OvmsVehicle::MetricModified, this, _1));

#ifdef CONFIG_OVMS_COMP_POLLER

  MyPollers.RegisterFrameRx(TAG, std::bind(&OvmsVehicle::IncomingRxFrame, this, _1));
#else

  m_vqueue = xQueueCreate(CONFIG_OVMS_VEHICLE_CAN_RX_QUEUE_SIZE,sizeof(CAN_frame_t));
  xTaskCreatePinnedToCore(OvmsVehicleTask, "OVMS Vehicle Poll",
      CONFIG_OVMS_VEHICLE_RXTASK_STACK, (void*)this, 10, &m_vtask, CORE(1));
  MyCan.RegisterListener(m_vqueue);
  for (int idx = 0; idx < VEHICLE_MAXBUSSES; ++idx)
    m_autopoweroff[idx] = false;
#endif
  }

OvmsVehicle::~OvmsVehicle()
  {
#ifndef CONFIG_OVMS_COMP_POLLER
  auto vtask = Atomic_GetAndNull(m_vtask);
  if (vtask)
    vTaskDelete(vtask);

  vQueueDelete(m_vqueue);
  m_vqueue = nullptr;
#endif

  if (m_bms_voltages != NULL)
    {
    delete [] m_bms_voltages;
    m_bms_voltages = NULL;
    }
  if (m_bms_vmins != NULL)
    {
    delete [] m_bms_vmins;
    m_bms_vmins = NULL;
    }
  if (m_bms_vmaxs != NULL)
    {
    delete [] m_bms_vmaxs;
    m_bms_vmaxs = NULL;
    }
  if (m_bms_vdevmaxs != NULL)
    {
    delete [] m_bms_vdevmaxs;
    m_bms_vdevmaxs = NULL;
    }
  if (m_bms_valerts != NULL)
    {
    delete [] m_bms_valerts;
    m_bms_valerts = NULL;
    }

  if (m_bms_temperatures != NULL)
    {
    delete [] m_bms_temperatures;
    m_bms_temperatures = NULL;
    }
  if (m_bms_tmins != NULL)
    {
    delete [] m_bms_tmins;
    m_bms_tmins = NULL;
    }
  if (m_bms_tmaxs != NULL)
    {
    delete [] m_bms_tmaxs;
    m_bms_tmaxs = NULL;
    }
  if (m_bms_tdevmaxs != NULL)
    {
    delete [] m_bms_tdevmaxs;
    m_bms_tdevmaxs = NULL;
    }
  if (m_bms_talerts != NULL)
    {
    delete [] m_bms_talerts;
    m_bms_talerts = NULL;
    }
  }

void OvmsVehicle::StartingUp()
  {
  m_ready = true;
#ifdef CONFIG_OVMS_COMP_POLLER
  MyPollers.StartingUp();
#endif
  }

void OvmsVehicle::ShuttingDown()
  {
  if (m_is_shutdown)
    return;
  m_is_shutdown = true;
  m_ready = false;
#ifdef CONFIG_OVMS_COMP_POLLER
  MyPollers.ShuttingDownVehicle();
  MyPollers.DeregisterRunFinished(TAG);
  MyPollers.DeregisterPollStateTicker(TAG);
  MyPollers.DeregisterFrameRx(TAG);

  if (m_pollsignal)
    delete m_pollsignal;
#else
  MyCan.DeregisterListener(m_vqueue);
  CAN_frame_t entry;
  entry.origin = nullptr;
  entry.callback = nullptr;
  entry.MsgID = 0;
  xQueueSendToFront(m_vqueue, &entry, 0);

  if (MyConfig.GetParamValueBool("vehicle", "can.autooff", true))
    {
    if (m_can1 && m_autopoweroff[0]) m_can1->SetPowerMode(Off);
    if (m_can2 && m_autopoweroff[1]) m_can2->SetPowerMode(Off);
    if (m_can3 && m_autopoweroff[2]) m_can3->SetPowerMode(Off);
    if (m_can4 && m_autopoweroff[3]) m_can4->SetPowerMode(Off);
    }

#endif
  MyEvents.DeregisterEvent(TAG);
  MyMetrics.DeregisterListener(TAG);
  }
bool OvmsVehicle::IsShutdown()
  {
  if (!m_is_shutdown)
    return false;
#ifndef CONFIG_OVMS_COMP_POLLER
  if (Atomic_Get(m_vqueue) != nullptr) {
    return false;
  }
#endif
  return true;
  }

const char* OvmsVehicle::VehicleShortName()
  {
  return MyVehicleFactory.ActiveVehicleName();
  }

const char* OvmsVehicle::VehicleType()
  {
  return MyVehicleFactory.ActiveVehicleType();
  }

canbus *OvmsVehicle::GetBus(uint8_t busno)
  {
  switch (busno)
    {
    case 1: return m_can1;
    case 2: return m_can2;
    case 3: return m_can3;
    case 4: return m_can4;
    default: return nullptr;
    }
  }

uint8_t OvmsVehicle::GetBusNo(canbus* bus)
  {
  if (bus == m_can1)
    return 1;
  if(bus == m_can2)
    return 2;
  if (bus == m_can3)
    return 3;
  if (bus == m_can4)
    return 4;
  return 0;
  }
void OvmsVehicle::PollRunFinished(canbus* bus)
  {
  }

#ifdef CONFIG_OVMS_COMP_POLLER
OvmsVehicle::OvmsVehicleSignal::OvmsVehicleSignal( OvmsVehicle *parent)
  {
  m_parent = parent;
  }
// Signals for vehicle
void OvmsVehicle::OvmsVehicleSignal::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
  {
  if (Ready())
    m_parent->IncomingPollReply(job, data, length);
  }

void OvmsVehicle::OvmsVehicleSignal::IncomingPollError(const OvmsPoller::poll_job_t &job, uint16_t code)
  {
  if (Ready())
    m_parent->IncomingPollError(job, code);
  }

void OvmsVehicle::OvmsVehicleSignal::IncomingPollTxCallback(const OvmsPoller::poll_job_t &job, bool success)
  {
  if (Ready())
    m_parent->IncomingPollTxCallback(job, success);
  }

bool OvmsVehicle::OvmsVehicleSignal::Ready() const
  {
  return m_parent->m_ready;
  }
#endif

void OvmsVehicle::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicle::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicle::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  }
void OvmsVehicle::IncomingFrameCan4(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicle::Status(int verbosity, OvmsWriter* writer)
  {
  writer->printf("Vehicle module '%s' (code %s) loaded and running\n", VehicleShortName(), VehicleType());
  }

void OvmsVehicle::RegisterCanBus(int bus, CAN_mode_t mode, CAN_speed_t speed, dbcfile* dbcfile, bool autoPoweroff)
  {
  canbus *can;
#ifdef CONFIG_OVMS_COMP_POLLER
  OvmsPollers::BusPoweroff unload = autoPoweroff ? OvmsPollers::BusPoweroff::Vehicle : OvmsPollers::BusPoweroff::None;
  can = MyPollers.RegisterCanBus(bus, mode, speed, dbcfile, unload);
#else
  {
    std::string busname = string_format("can%d", bus);
    can = (canbus*)MyPcpApp.FindDeviceByName(busname.c_str());
    can->SetPowerMode(On);
    can->Start(mode,speed,dbcfile);
    if (bus <= VEHICLE_MAXBUSSES)
      m_autopoweroff[bus-1] = autoPoweroff;
  }
#endif
  switch (bus)
    {
    case 1: m_can1 = can; break;
    case 2: m_can2 = can; break;
    case 3: m_can3 = can; break;
    case 4: m_can4 = can; break;
    }
  }

bool OvmsVehicle::PinCheck(const char* pin)
  {
  if (!MyConfig.IsDefined("password","pin")) return false;

  std::string vpin = MyConfig.GetParamValue("password","pin");
  return (strcmp(vpin.c_str(),pin)==0);
  }

void OvmsVehicle::PollRunFinishedNotify(canbus* bus, void *data)
  {
  PollRunFinished(bus);
  }
void OvmsVehicle::PollerStateTickerNotify(canbus* bus, void *data)
  {
  PollerStateTicker(bus);
  }

void OvmsVehicle::VehicleTicker1(std::string event, void* data)
  {
  if (!m_ready)
    return;

  m_ticker++;

  // Battery Monitor (evey 2 seconds).
  if (m_aux_enabled && (m_ticker & 1) == 0)
    Check12vState();

  Ticker1(m_ticker);
  if ((m_ticker % 10) == 0)
    {
    Ticker10(m_ticker);
    if ((m_ticker % 60) == 0)
      {
      Ticker60(m_ticker);
      if ((m_ticker % 300) == 0)
        {
        Ticker300(m_ticker);
        if ((m_ticker % 600) == 0)
          {
          Ticker600(m_ticker);
          if ((m_ticker % 3600) == 0)
            Ticker3600(m_ticker);
          }
        }
      }
    }

  if (StandardMetrics.ms_v_env_on->AsBool())
    {
    StandardMetrics.ms_v_env_parktime->SetValue(0);
    m_last_drivetime = StandardMetrics.ms_v_env_drivetime->AsInt() + 1;
    StandardMetrics.ms_v_env_drivetime->SetValue(m_last_drivetime);
    }
  else
    {
    StandardMetrics.ms_v_env_drivetime->SetValue(0);
    m_last_parktime = StandardMetrics.ms_v_env_parktime->AsInt() + 1;
    StandardMetrics.ms_v_env_parktime->SetValue(m_last_parktime);
    }

  if (StandardMetrics.ms_v_charge_inprogress->AsBool())
    {
    m_last_chargetime = StandardMetrics.ms_v_charge_time->AsInt() + 1;
    StandardMetrics.ms_v_charge_time->SetValue(m_last_chargetime);
    }
  else
    {
    if(!StandardMetrics.ms_v_door_chargeport->AsBool())
      {
      StandardMetrics.ms_v_charge_time->SetValue(0);
      }
    }

  if (StandardMetrics.ms_v_gen_inprogress->AsBool())
    {
    m_last_gentime = StandardMetrics.ms_v_gen_time->AsInt() + 1;
    StandardMetrics.ms_v_gen_time->SetValue(m_last_gentime);
    }
  else
    {
    StandardMetrics.ms_v_gen_time->SetValue(0);
    }

  if (m_chargestate_ticker > 0 && --m_chargestate_ticker == 0)
    NotifyChargeState();
  if (m_vehicleon_ticker > 0 && --m_vehicleon_ticker == 0)
    NotifyVehicleOn();
  if (m_vehicleoff_ticker > 0 && --m_vehicleoff_ticker == 0)
    NotifyVehicleOff();

  CalculateEfficiency();

  // 12V battery monitor:
  if (StandardMetrics.ms_v_env_charging12v->AsBool() == true)
    {
    // add two seconds calmdown per second charging, max 15 minutes:
    if (m_12v_ticker < 15*60)
      m_12v_ticker += 2;
    }
  else if (m_12v_ticker > 0)
    {
    --m_12v_ticker;
    if (m_12v_ticker == 0)
      {
      // take 12V reference voltage:
      StandardMetrics.ms_v_bat_12v_voltage_ref->SetValue(StandardMetrics.ms_v_bat_12v_voltage->AsFloat());
      }
    }

  if ((m_ticker % 60) == 0)
    {
    // check 12V voltage:
    float volt = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
    // …against the maximum of default and measured reference voltage, so alerts will also
    //  be triggered if the measured ref follows a degrading battery:
    float dref = MyConfig.GetParamValueFloat("vehicle", "12v.ref", 12.6);
    float vref = std::max(StandardMetrics.ms_v_bat_12v_voltage_ref->AsFloat(), dref);

    // Check for alert level:
    bool alert_on = StandardMetrics.ms_v_bat_12v_voltage_alert->AsBool();
    float alert_threshold = MyConfig.GetParamValueFloat("vehicle", "12v.alert", 1.6);
    if (!alert_on && volt > 0 && vref > 0 && vref-volt > alert_threshold)
      {
      StandardMetrics.ms_v_bat_12v_voltage_alert->SetValue(true);
      MyEvents.SignalEvent("vehicle.alert.12v.on", NULL);
      if (m_autonotifications) Notify12vCritical();
      }
    else if (alert_on && volt > 0 && vref > 0 && vref-volt < alert_threshold*0.6)
      {
      StandardMetrics.ms_v_bat_12v_voltage_alert->SetValue(false);
      MyEvents.SignalEvent("vehicle.alert.12v.off", NULL);
      if (m_autonotifications) Notify12vRecovered();
      }

    // Check for shutdown level:
    if (!m_12v_shutdown_ticker && !MyBoot.IsShuttingDown())
      {
      float shutdown_threshold = MyConfig.GetParamValueFloat("vehicle", "12v.shutdown", 0);
      if (shutdown_threshold > 0 && volt > 0 && volt <= shutdown_threshold)
        {
        ++m_12v_low_ticker;
        if (m_12v_low_ticker == 1)
          {
          MyEvents.SignalEvent("vehicle.alert.12v.low", NULL);
          }
        int shutdown_delay = MyConfig.GetParamValueInt("vehicle", "12v.shutdown_delay", 2);
        if (m_12v_low_ticker > shutdown_delay)
          {
          MyEvents.SignalEvent("vehicle.alert.12v.shutdown", NULL);
          if (m_autonotifications) Notify12vShutdown();
          // shutdown in 10 seconds to allow for scripts & notifications:
          m_12v_shutdown_ticker = 10;
          }
        }
      else
        {
        if (m_12v_low_ticker > 0)
          {
          m_12v_low_ticker = 0;
          MyEvents.SignalEvent("vehicle.alert.12v.operational", NULL);
          }
        }
      }
    }

  if (m_12v_shutdown_ticker > 0 && --m_12v_shutdown_ticker == 0)
    {
    unsigned int wakeup_interval = MyConfig.GetParamValueInt("vehicle", "12v.wakeup_interval", 60);
    MyBoot.DeepSleep(wakeup_interval);
    }

  if ((m_ticker % 10)==0)
    {
    // Check MINSOC
    int soc = (int) ceil(StandardMetrics.ms_v_bat_soc->AsFloat());
    m_minsoc = MyConfig.GetParamValueInt("vehicle", "minsoc", 0);
    if (m_minsoc <= 0)
      {
      m_minsoc_triggered = 0;
      }
    else if (soc >= m_minsoc+2)
      {
      m_minsoc_triggered = m_minsoc;
      }
    if ((m_minsoc_triggered > 0) && (soc <= m_minsoc_triggered))
      {
      // We have reached the minimum SOC level
      if (m_autonotifications) NotifyMinSocCritical();
      if (soc > 1)
        m_minsoc_triggered = soc - 1;
      else
        m_minsoc_triggered = 0;
      }
    }

  // BMS ticker:
  BmsTicker();

  // TPMS alerts:
  if (StdMetrics.ms_v_tpms_alert->LastModified() > m_tpms_lastcheck)
    {
    m_tpms_lastcheck = StdMetrics.ms_v_tpms_alert->LastModified();
    auto tpms_state = StdMetrics.ms_v_tpms_alert->AsVector();
    m_tpms_laststate.resize(tpms_state.size());
    bool notify = false;
    for (int i = 0; i < tpms_state.size(); i++)
      {
      if (tpms_state[i] > m_tpms_laststate[i])
        notify = true;
      m_tpms_laststate[i] = tpms_state[i];
      }
    if (notify)
      {
      MyEvents.SignalEvent("vehicle.alert.tpms", NULL);
      if (m_autonotifications && MyConfig.GetParamValueBool("vehicle", "tpms.alerts.enabled", true))
        NotifyTpmsAlerts();
      }
    }

  // Idle alert:
  if (!StdMetrics.ms_v_env_awake->AsBool() || StdMetrics.ms_v_pos_speed->AsFloat() > 0)
    {
    m_idle_ticker = 15 * 60; // first alert after 15 minutes
    }
  else if (m_idle_ticker > 0 && --m_idle_ticker == 0)
    {
    NotifyVehicleIdling();
    m_idle_ticker = 60 * 60; // successive alerts every 60 minutes
    }
  } // VehicleTicker1()

void OvmsVehicle::Ticker1(uint32_t ticker)
  {
  }

void OvmsVehicle::Ticker10(uint32_t ticker)
  {
  }

void OvmsVehicle::Ticker60(uint32_t ticker)
  {
  }

void OvmsVehicle::Ticker300(uint32_t ticker)
  {
  }

void OvmsVehicle::Ticker600(uint32_t ticker)
  {
  }

void OvmsVehicle::Ticker3600(uint32_t ticker)
  {
  }

void OvmsVehicle::EnableAuxMonitor()
  {
  m_aux_enabled = true;
  }

void OvmsVehicle::EnableAuxMonitor(float low_thresh, float charge_thresh)
  {
  m_aux_battery_mon.set_thresholds(low_thresh, charge_thresh);
  EnableAuxMonitor();
  }

void OvmsVehicle::Check12vState()
  {
  // Update the battery monitor
  if (StdMetrics.ms_v_bat_12v_voltage->IsDefined())
    {
    m_aux_battery_mon.add(StdMetrics.ms_v_bat_12v_voltage->AsFloat());
    if (m_aux_battery_mon.checkStateChange())
      {
      NotifyVehicleAux12vState(m_aux_battery_mon.state(), m_aux_battery_mon);
      }
    }
#ifdef OVMS_DEBUG_BATTERYMON_VERBOSE
    if (m_ticker % 30)
      {
      ESP_LOGV(TAG, "Aux Battery: %s", m_aux_battery_mon.to_string().c_str());
      }
#endif

  }

void OvmsVehicle::NotifyVehicleAux12vState(OvmsBatteryState new_state, const OvmsBatteryMon &monitor)
  {
  if (new_state != OvmsBatteryState::Unknown)
    {
    std::string event("vehicle.aux.12v.");
    event += OvmsBatteryMon::state_code(new_state);
    MyEvents.SignalEvent(event, nullptr);

    NotifiedVehicleAux12vStateChanged(new_state, monitor);
    }
  }

void OvmsVehicle::NotifyChargeStart()
  {
  StringWriter buf(200);
  CommandStat(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","charge.started",buf.c_str());
  }

void OvmsVehicle::NotifyChargeTopOff()
  {
  StringWriter buf(200);
  CommandStat(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","charge.toppingoff",buf.c_str());
  }

void OvmsVehicle::NotifyHeatingStart()
  {
  StringWriter buf(200);
  CommandStat(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","heating.started",buf.c_str());
  }

void OvmsVehicle::NotifyChargeStopped()
  {
  StringWriter buf(200);
  CommandStat(COMMAND_RESULT_NORMAL, &buf);
  if (StdMetrics.ms_v_charge_substate->AsString() == "scheduledstop" ||
      StdMetrics.ms_v_charge_substate->AsString() == "timerwait" ||
      StdMetrics.ms_v_charge_substate->AsString() == "onrequest")
    MyNotify.NotifyString("info","charge.stopped",buf.c_str());
  else
    MyNotify.NotifyString("alert","charge.stopped",buf.c_str());
  }

void OvmsVehicle::NotifyChargeDone()
  {
  StringWriter buf(200);
  CommandStat(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","charge.done",buf.c_str());
  }

void OvmsVehicle::NotifyValetEnabled()
  {
  MyNotify.NotifyString("info", "valet.enabled", "Valet mode enabled");
  }

void OvmsVehicle::NotifyValetDisabled()
  {
  MyNotify.NotifyString("info", "valet.disabled", "Valet mode disabled");
  }

void OvmsVehicle::NotifyValetHood()
  {
  MyNotify.NotifyString("alert", "valet.hood", "Vehicle hood opened while in valet mode");
  }

void OvmsVehicle::NotifyValetTrunk()
  {
  MyNotify.NotifyString("alert", "valet.trunk", "Vehicle trunk opened while in valet mode");
  }

void OvmsVehicle::NotifyAlarmSounding()
  {
  MyNotify.NotifyString("alert", "alarm.sounding", "Vehicle alarm is sounding");
  }

void OvmsVehicle::NotifyAlarmStopped()
  {
  MyNotify.NotifyString("alert", "alarm.stopped", "Vehicle alarm has stopped");
  }

void OvmsVehicle::Notify12vCritical()
  {
  float volt = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
  float dref = MyConfig.GetParamValueFloat("vehicle", "12v.ref", 12.6);
  float vref = std::max(StandardMetrics.ms_v_bat_12v_voltage_ref->AsFloat(), dref);

  MyNotify.NotifyStringf("alert", "batt.12v.alert", "12V Battery critical: %.1fV (ref=%.1fV)", volt, vref);
  }

void OvmsVehicle::Notify12vRecovered()
  {
  float volt = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
  float dref = MyConfig.GetParamValueFloat("vehicle", "12v.ref", 12.6);
  float vref = std::max(StandardMetrics.ms_v_bat_12v_voltage_ref->AsFloat(), dref);

  MyNotify.NotifyStringf("alert", "batt.12v.recovered", "12V Battery restored: %.1fV (ref=%.1fV)", volt, vref);
  }

void OvmsVehicle::Notify12vShutdown()
  {
  float volt = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
  float wakeup = MyBoot.GetMin12VLevel();

  MyNotify.NotifyStringf("alert", "batt.12v.shutdown", "12V Battery shutdown: %.1fV (wakeup at/above %.1fV)", volt, wakeup);
  }

void OvmsVehicle::NotifyMinSocCritical()
  {
  float soc = StandardMetrics.ms_v_bat_soc->AsFloat();

  MyNotify.NotifyStringf("alert", "batt.soc.alert", "Battery SOC critical: %.1f%% (alert<=%d%%)", soc, m_minsoc);
  }

void OvmsVehicle::NotifyVehicleIdling()
  {
  MyNotify.NotifyString("alert", "vehicle.idle", "Vehicle is idling / stopped turned on");
  }

std::vector<std::string> OvmsVehicle::GetTpmsLayout()
  {
  return { "FL", "FR", "RL", "RR" };
  }

void OvmsVehicle::NotifyTpmsAlerts()
  {
  int maxlevel = 0;
  for (int i = 0; i < m_tpms_laststate.size(); i++)
    {
    if (m_tpms_laststate[i] > maxlevel)
      maxlevel = m_tpms_laststate[i];
    }
  if (maxlevel == 0)
    return;

  StringWriter buf(200);
  std::vector<std::string> wheels = GetTpmsLayout();
  const char* alertlevel[] = { "OK", "WARNING", "ALERT" };

  buf.printf("TPMS %s:\n", maxlevel == 1 ? "INFO" : "ALERT");

  for (int i = 0; i < m_tpms_laststate.size(); i++)
    {
    if (m_tpms_laststate[i])
      {
      buf.printf("%s wheel %s:", wheels[i].c_str(), alertlevel[m_tpms_laststate[i]]);
      if (StdMetrics.ms_v_tpms_health->IsDefined())
        buf.printf(" Health=%s", StdMetrics.ms_v_tpms_health->ElemAsUnitString(i, "", Native, 0).c_str());
      if (StdMetrics.ms_v_tpms_pressure->IsDefined())
        buf.printf(" Pressure=%s", StdMetrics.ms_v_tpms_pressure->ElemAsUnitString(i, "", Native, 1).c_str());
      if (StdMetrics.ms_v_tpms_temp->IsDefined())
        buf.printf(" Temp=%sC", StdMetrics.ms_v_tpms_temp->ElemAsString(i, "", Native, 1).c_str());
      buf.append("\n");
      }
    }

  if (maxlevel == 1)
    MyNotify.NotifyString("info", "tpms.warning", buf.c_str());
  else
    MyNotify.NotifyString("alert", "tpms.alert", buf.c_str());
  }

// Default efficiency calculation by speed & power per second, average smoothed over 5 seconds.
// Override if your vehicle provides more detail.
void OvmsVehicle::CalculateEfficiency()
  {
  float consumption = 0;
  if (StdMetrics.ms_v_pos_speed->AsFloat() >= 5)
    consumption = StdMetrics.ms_v_bat_power->AsFloat(0, Watts) / StdMetrics.ms_v_pos_speed->AsFloat();
  StdMetrics.ms_v_bat_consumption->SetValue(
    TRUNCPREC((StdMetrics.ms_v_bat_consumption->AsFloat() * 4 + consumption) / 5, 1));
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandSetChargeMode(vehicle_mode_t mode)
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    std::string mode_code = chargemode_code((int)mode);
    StringWriter dukcmd;
    dukcmd.printf("(!OvmsVehicle.SetChargeMode.prototype)?-1:"
      "OvmsVehicle.SetChargeMode(\"%s\")", mode_code.c_str());
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0) return res ? Success : Fail;
    }
#endif
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandSetChargeCurrent(uint16_t limit)
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    StringWriter dukcmd;
    dukcmd.printf("(!OvmsVehicle.SetChargeCurrent.prototype)?-1:"
      "OvmsVehicle.SetChargeCurrent(%" PRIu16 ")", limit);
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0) return res ? Success : Fail;
    }
#endif
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandStartCharge()
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    StringWriter dukcmd;
    dukcmd.printf("(!OvmsVehicle.StartCharge.prototype)?-1:"
      "OvmsVehicle.StartCharge()");
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0) return res ? Success : Fail;
    }
#endif
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandStopCharge()
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    StringWriter dukcmd;
    dukcmd.printf("(!OvmsVehicle.StopCharge.prototype)?-1:"
      "OvmsVehicle.StopCharge()");
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0) return res ? Success : Fail;
    }
#endif
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandSetChargeTimer(bool timeron, uint16_t timerstart)
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    StringWriter dukcmd;
    dukcmd.printf("(!OvmsVehicle.SetChargeTimer.prototype)?-1:"
      "OvmsVehicle.SetChargeTimer(%s,%" PRIu16 ")", timeron ? "true" : "false", timerstart);
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0) return res ? Success : Fail;
    }
#endif
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandCooldown(bool cooldownon)
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    StringWriter dukcmd;
    const char *method = cooldownon ? "StartCooldown" : "StopCooldown";
    dukcmd.printf("(!OvmsVehicle.%s.prototype)?-1:"
      "OvmsVehicle.%s()", method, method);
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0) return res ? Success : Fail;
    }
#endif
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandClimateControl(bool climatecontrolon)
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    StringWriter dukcmd;
    dukcmd.printf("(!OvmsVehicle.ClimateControl.prototype)?-1:"
      "OvmsVehicle.ClimateControl(%s)", climatecontrolon ? "true" : "false");
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0) return res ? Success : Fail;
    }
#endif
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandWakeup()
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    StringWriter dukcmd;
    dukcmd.printf("(!OvmsVehicle.Wakeup.prototype)?-1:"
      "OvmsVehicle.Wakeup()");
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0) return res ? Success : Fail;
    }
#endif
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandLock(const char* pin)
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    std::string pin_json = json_encode(std::string(pin));
    StringWriter dukcmd;
    dukcmd.printf("(!OvmsVehicle.Lock.prototype)?-1:"
      "OvmsVehicle.Lock(\"%s\")", pin_json.c_str());
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0) return res ? Success : Fail;
    }
#endif
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandUnlock(const char* pin)
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    std::string pin_json = json_encode(std::string(pin));
    StringWriter dukcmd;
    dukcmd.printf("(!OvmsVehicle.Unlock.prototype)?-1:"
      "OvmsVehicle.Unlock(\"%s\")", pin_json.c_str());
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0) return res ? Success : Fail;
    }
#endif
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandActivateValet(const char* pin)
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    std::string pin_json = json_encode(std::string(pin));
    StringWriter dukcmd;
    dukcmd.printf("(!OvmsVehicle.Valet.prototype)?-1:"
      "OvmsVehicle.Valet(\"%s\")", pin_json.c_str());
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0)
      {
        if (res)
          {
          StandardMetrics.ms_v_env_valet->SetValue(true);
          return Success;
          }
        else
          return Fail;
      }
    }
#endif
  if (StandardMetrics.ms_v_env_valet->AsBool())
    return Success;
  if (!PinCheck(pin))
    return Fail;
  StandardMetrics.ms_v_env_valet->SetValue(true);
  return Success;

  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandDeactivateValet(const char* pin)
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    std::string pin_json = json_encode(std::string(pin));
    StringWriter dukcmd;
    dukcmd.printf("(!OvmsVehicle.Unvalet.prototype)?-1:"
      "OvmsVehicle.Unvalet(\"%s\")", pin_json.c_str());
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0)
      {
      if (res)
        StandardMetrics.ms_v_env_valet->SetValue(false);
      return res ? Success : Fail;
      }
    }
#endif
  if (!StandardMetrics.ms_v_env_valet->AsBool())
    return Success;
  if (!PinCheck(pin))
    return Fail;
  StandardMetrics.ms_v_env_valet->SetValue(false);
  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandHomelink(int button, int durationms)
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (MyDuktape.DukTapeAvailable())
    {
    StringWriter dukcmd;
    dukcmd.printf("(!OvmsVehicle.Homelink.prototype)?-1:"
      "OvmsVehicle.Homelink(%d,%d)", button+1, durationms);
    int res = MyDuktape.DuktapeEvalIntResult(dukcmd.c_str());
    if (res >= 0) return res ? Success : Fail;
    }
#endif
  return NotImplemented;
  }

#ifdef CONFIG_OVMS_COMP_TPMS

bool OvmsVehicle::TPMSRead(std::vector<uint32_t> *tpms)
  {
  ESP_LOGE(TAG, "TPMS tyre IDs not implemented in this vehicle");
  return false;
  }

bool OvmsVehicle::TPMSWrite(std::vector<uint32_t> &tpms)
  {
  ESP_LOGE(TAG, "TPMS tyre IDs not implemented in this vehicle");
  return false;
  }

#endif // #ifdef CONFIG_OVMS_COMP_TPMS

/**
 * CommandStat: default implementation of vehicle status output
 */
OvmsVehicle::vehicle_command_t OvmsVehicle::CommandStat(int verbosity, OvmsWriter* writer)
  {

  bool chargeport_open = StdMetrics.ms_v_door_chargeport->AsBool();
  std::string charge_state = StdMetrics.ms_v_charge_state->AsString();
  if (chargeport_open && charge_state != "")
    {
    std::string charge_mode = StdMetrics.ms_v_charge_mode->AsString();
    bool show_details = !(charge_state == "done" || charge_state == "stopped");

    // Translate mode codes:
    if (charge_mode == "standard")
      charge_mode = "Standard";
    else if (charge_mode == "storage")
      charge_mode = "Storage";
    else if (charge_mode == "range")
      charge_mode = "Range";
    else if (charge_mode == "performance")
      charge_mode = "Performance";

    // Translate state codes:
    if (charge_state == "charging")
      charge_state = "Charging";
    else if (charge_state == "topoff")
      charge_state = "Topping off";
    else if (charge_state == "done")
      charge_state = "Charge Done";
    else if (charge_state == "preparing")
      charge_state = "Preparing";
    else if (charge_state == "heating")
      charge_state = "Charging, Heating";
    else if (charge_state == "stopped")
      charge_state = "Charge Stopped";
    else if (charge_state == "timerwait")
      charge_state = "Charge Stopped, Timer On";

    if (charge_mode != "")
      writer->printf("%s - ", charge_mode.c_str());
    writer->printf("%s\n", charge_state.c_str());

    if (show_details)
      {
      // Voltage & current:
      bool show_vc = (StdMetrics.ms_v_charge_voltage->AsFloat() > 0 || StdMetrics.ms_v_charge_current->AsFloat() > 0);
      if (show_vc)
        {
        writer->printf("%s/%s ",
          (char*) StdMetrics.ms_v_charge_voltage->AsUnitString("-", Native, 1).c_str(),
          (char*) StdMetrics.ms_v_charge_current->AsUnitString("-", Native, 1).c_str());
        }

      // Charge speed:
      if (StdMetrics.ms_v_bat_range_speed->IsDefined() && StdMetrics.ms_v_bat_range_speed->AsFloat() != 0)
        {
        writer->printf("%s\n", StdMetrics.ms_v_bat_range_speed->AsUnitString("-", ToUser, 1).c_str());
        }
      else if (show_vc)
        {
        writer->puts("");
        }

      // Estimated time(s) remaining:
      int duration_full = StdMetrics.ms_v_charge_duration_full->AsInt();
      if (duration_full > 0)
        writer->printf("Full: %d:%02dh\n", duration_full / 60, duration_full % 60);

      int duration_soc = StdMetrics.ms_v_charge_duration_soc->AsInt();
      if (duration_soc > 0)
        writer->printf("%s: %d:%02dh\n",
          (char*) StdMetrics.ms_v_charge_limit_soc->AsUnitString("SOC", ToUser, 0).c_str(),
          duration_soc / 60, duration_soc % 60);

      int duration_range = StdMetrics.ms_v_charge_duration_range->AsInt();
      if (duration_range > 0)
        writer->printf("%s: %d:%02dh\n",
          (char*) StdMetrics.ms_v_charge_limit_range->AsUnitString("Range", ToUser, 0).c_str(),
          duration_range / 60, duration_range % 60);
      }

    // Energy sums:
    if (StdMetrics.ms_v_charge_kwh_grid->IsDefined())
      {
      writer->printf("Drawn: %s\n",
        StdMetrics.ms_v_charge_kwh_grid->AsUnitString("-", ToUser, 1).c_str());
      }
    if (StdMetrics.ms_v_charge_kwh->IsDefined())
      {
      writer->printf("Charged: %s\n",
        StdMetrics.ms_v_charge_kwh->AsUnitString("-", ToUser, 1).c_str());
      }
    }
  else
    {
    writer->puts("Not charging");
    }

  writer->printf("SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", ToUser, 1).c_str());

  if (StdMetrics.ms_v_bat_range_ideal->IsDefined())
    {
    const std::string& range_ideal = StdMetrics.ms_v_bat_range_ideal->AsUnitString("-", ToUser, 0);
    writer->printf("Ideal range: %s\n", range_ideal.c_str());
    }

  if (StdMetrics.ms_v_bat_range_est->IsDefined())
    {
    const std::string& range_est = StdMetrics.ms_v_bat_range_est->AsUnitString("-", ToUser, 0);
    writer->printf("Est. range: %s\n", range_est.c_str());
    }

  if (StdMetrics.ms_v_pos_odometer->IsDefined())
    {
    const std::string& odometer = StdMetrics.ms_v_pos_odometer->AsUnitString("-", ToUser, 1);
    writer->printf("ODO: %s\n", odometer.c_str());
    }

  if (StdMetrics.ms_v_bat_cac->IsDefined())
    {
    const std::string& cac = StdMetrics.ms_v_bat_cac->AsUnitString("-", ToUser, 1);
    writer->printf("CAC: %s\n", cac.c_str());
    }

  if (StdMetrics.ms_v_bat_soh->IsDefined())
    {
    const std::string& soh = StdMetrics.ms_v_bat_soh->AsUnitString("-", ToUser, 0);
    writer->printf("SOH: %s\n", soh.c_str());
    }

  return Success;
  }

/**
 * CommandStatTrip: default implementation of vehicle trip status report
 */
OvmsVehicle::vehicle_command_t OvmsVehicle::CommandStatTrip(int verbosity, OvmsWriter* writer)
  {
  metric_unit_t rangeUnit = OvmsMetricGetUserUnit(GrpDistance, Kilometers);
  metric_unit_t speedUnit = OvmsMetricGetUserUnit(GrpSpeed, Kph);
  metric_unit_t accelUnit = OvmsMetricGetUserUnit(GrpAccel, KphPS);
  metric_unit_t consumUnit = OvmsMetricGetUserUnit(GrpConsumption, WattHoursPK);
  metric_unit_t energyUnit = kWh;
  metric_unit_t altitudeUnit = OvmsMetricGetUserUnit(GrpDistanceShort, Meters);
  const char* rangeUnitLabel = OvmsMetricUnitLabel(rangeUnit);
  const char* speedUnitLabel = OvmsMetricUnitLabel(speedUnit);
  const char* accelUnitLabel = OvmsMetricUnitLabel(accelUnit);
  const char* consumUnitLabel = OvmsMetricUnitLabel(consumUnit);
  const char* energyUnitLabel = OvmsMetricUnitLabel(energyUnit);
  const char* altitudeUnitLabel = OvmsMetricUnitLabel(altitudeUnit);

  float trip_length = StdMetrics.ms_v_pos_trip->AsFloat(0);

  float speed_avg = (m_drive_speedcnt > 0)
    ? UnitConvert(Kph, speedUnit, (float)(m_drive_speedsum / m_drive_speedcnt))
    : 0;
  float accel_avg = (m_drive_accelcnt > 0)
    ? UnitConvert(MetersPSS, accelUnit, (float)(m_drive_accelsum / m_drive_accelcnt))
    : 0;
  float decel_avg = (m_drive_decelcnt > 0)
    ? UnitConvert(MetersPSS, accelUnit, (float)(m_drive_decelsum / m_drive_decelcnt))
    : 0;
  float energy_recup_perc = (m_inv_energyused > 0) ? m_inv_energyrecd / m_inv_energyused * 100 : 0;

  float energy_used = StdMetrics.ms_v_bat_energy_used->AsFloat();
  float energy_recd = StdMetrics.ms_v_bat_energy_recd->AsFloat();
  float energy_recd_perc = (energy_used > 0) ? energy_recd / energy_used * 100 : 0;
  float wh_per_km = (trip_length > 0) ? (energy_used - energy_recd) * 1000 / trip_length : 0;

  float soc = StdMetrics.ms_v_bat_soc->AsFloat();
  float soc_diff = soc - m_drive_startsoc;
  float range = StdMetrics.ms_v_bat_range_est->AsFloat();
  float range_diff = range - m_drive_startrange;
  float alt = StdMetrics.ms_v_pos_altitude->AsFloat();
  float alt_diff = UnitConvert(Meters, altitudeUnit, alt - m_drive_startaltitude);
  uint32_t duration = time(NULL) - m_drive_starttime;
  uint8_t duration_h = duration / 3600;
  uint8_t duration_min = duration % 3600 / 60;
  uint8_t duration_sec = duration % 3600 % 60;

  std::ostringstream buf;
  buf
    << "Trip "
    << std::fixed
    << std::setprecision(1)
    << UnitConvert(Kilometers, rangeUnit, trip_length) << rangeUnitLabel
    << std::setprecision(0)
    << " ("
    ;
  if (duration_h > 0) 
    {
    buf
      << +duration_h
      << ":"
      << std::setw(2) << std::setfill('0')
      ;
    }
  buf
    << +duration_min
    << ":"
    << std::setw(2) << std::setfill('0') << +duration_sec
    << std::setfill(' ')
    << ") "
    << " Avg "
    << speed_avg << speedUnitLabel
    << " Alt "
    << ((alt_diff >= 0) ? "+" : "")
    << alt_diff << altitudeUnitLabel
    ;
  if (wh_per_km != 0)
    {
    buf << "\nEnergy ";
    if (consumUnit == KPkWh || consumUnit == MPkWh) 
    {
      buf << std::setprecision(2);
    } 
    else if (consumUnit == kWhP100K) 
    {
      buf << std::setprecision(1);
    }
    else
    {
      buf << std::setprecision(0);
    }
    buf
      << UnitConvert(WattHoursPK, consumUnit, wh_per_km) << consumUnitLabel
      << ", "
      << std::setprecision(0)
      << energy_recd_perc << "% recd"
      ;
    }
  buf
    << std::setprecision(1)
    << "\nSOC "
    << ((soc_diff >= 0) ? "+" : "")
    << soc_diff << "%"
    << " = "
    << soc << "%"
    << "\nRange "
    << ((range_diff >= 0) ? "+" : "")
    << range_diff << rangeUnitLabel
    << " = "
    << range << rangeUnitLabel
    ;
  if (accel_avg > 0)
    {
    buf
      << "\nAccel +"
      << accel_avg
      << " / "
      << decel_avg << accelUnitLabel
      ;
    }
  if (m_inv_energyused > 0)
    {
    buf
      << "\nMotor +"
      << std::setprecision(3)
      << m_inv_energyused
      << " / -"
      << m_inv_energyrecd << energyUnitLabel
      << std::setprecision(0)
      << " (" << energy_recup_perc << "% recd)"
      ;
    }

  writer->puts(buf.str().c_str());

  return Success;
  }

void OvmsVehicle::VehicleConfigChanged(std::string event, void* data)
  {
  OvmsConfigParam* param = (OvmsConfigParam*) data;

  // read vehicle framework config:
  if (!param || param->GetName() == "vehicle")
    {
    // acceleration calculation:
    m_accel_smoothing = MyConfig.GetParamValueFloat("vehicle", "accel.smoothing", 2.0);

    // brakelight battery power smoothing:
    m_batpwr_smoothing = MyConfig.GetParamValueFloat("vehicle", "batpwr.smoothing", 2.0);

    // brakelight control:
    if (m_brakelight_enable)
      {
      SetBrakelight(0);
      StdMetrics.ms_v_env_regenbrake->SetValue(false);
      }
    m_brakelight_enable = MyConfig.GetParamValueBool("vehicle", "brakelight.enable", false);
    m_brakelight_on = MyConfig.GetParamValueFloat("vehicle", "brakelight.on", 1.3);
    m_brakelight_off = MyConfig.GetParamValueFloat("vehicle", "brakelight.off", 0.7);
    m_brakelight_port = MyConfig.GetParamValueInt("vehicle", "brakelight.port", 1);
    m_brakelight_basepwr = MyConfig.GetParamValueFloat("vehicle", "brakelight.basepwr", 0);
    m_brakelight_ignftbrk = MyConfig.GetParamValueBool("vehicle", "brakelight.ignftbrk", false);
    m_brakelight_start = 0;
    }

  // read vehicle specific config:
  ConfigChanged(param);
  }

void OvmsVehicle::ConfigChanged(OvmsConfigParam* param)
  {
  }

void OvmsVehicle::MetricModified(OvmsMetric* metric)
  {
  if (metric == StandardMetrics.ms_v_env_on)
    {
    if (StandardMetrics.ms_v_env_on->AsBool())
      {
      m_drive_startsoc = StdMetrics.ms_v_bat_soc->AsFloat();
      m_drive_startrange = StdMetrics.ms_v_bat_range_est->AsFloat();
      m_drive_startaltitude = StdMetrics.ms_v_pos_altitude->AsFloat();
      time(&m_drive_starttime);
      m_drive_speedcnt = 0;
      m_drive_speedsum = 0;
      m_drive_accelcnt = 0;
      m_drive_accelsum = 0;
      m_drive_decelcnt = 0;
      m_drive_decelsum = 0;
      m_inv_reftime = esp_log_timestamp();
      m_inv_refpower = 0;
      m_inv_energyused = 0;
      m_inv_energyrecd = 0;
      MyEvents.SignalEvent("vehicle.on",NULL);
      if (m_autonotifications)
        {
        m_vehicleon_ticker = GetNotifyVehicleStateDelay("on");
        if (m_vehicleon_ticker == 0)
          NotifyVehicleOn();
        }
      }
    else
      {
      if (m_brakelight_enable && m_brakelight_start)
        {
        SetBrakelight(0);
        m_brakelight_start = 0;
        StdMetrics.ms_v_env_regenbrake->SetValue(false);
        }
      MyEvents.SignalEvent("vehicle.off",NULL);
      if (m_autonotifications)
        {
        m_vehicleoff_ticker = GetNotifyVehicleStateDelay("off");
        if (m_vehicleoff_ticker == 0)
          NotifyVehicleOff();
        }
      }
    }
  else if (metric == StandardMetrics.ms_v_env_awake)
    {
    if (StandardMetrics.ms_v_env_awake->AsBool())
      {
      MyEvents.SignalEvent("vehicle.awake",NULL);
      NotifiedVehicleAwake();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.asleep",NULL);
      NotifiedVehicleAsleep();
      }
    }
  else if (metric == StandardMetrics.ms_v_charge_inprogress)
    {
    if (StandardMetrics.ms_v_charge_inprogress->AsBool())
      {
      MyEvents.SignalEvent("vehicle.charge.start",NULL);
      NotifiedVehicleChargeStart();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.charge.stop",NULL);
      NotifiedVehicleChargeStop();
      }
    }
  else if (metric == StandardMetrics.ms_v_door_chargeport)
    {
    if (StandardMetrics.ms_v_door_chargeport->AsBool())
      {
      MyEvents.SignalEvent("vehicle.charge.prepare",NULL);
      NotifiedVehicleChargePrepare();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.charge.finish",NULL);
      NotifiedVehicleChargeFinish();
      }
    }
  else if (metric == StandardMetrics.ms_v_charge_pilot)
    {
    if (StandardMetrics.ms_v_charge_pilot->AsBool())
      {
      MyEvents.SignalEvent("vehicle.charge.pilot.on",NULL);
      NotifiedVehicleChargePilotOn();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.charge.pilot.off",NULL);
      NotifiedVehicleChargePilotOff();
      }
    }
  else if (metric == StandardMetrics.ms_v_charge_timermode)
    {
    if (StandardMetrics.ms_v_charge_timermode->AsBool())
      {
      MyEvents.SignalEvent("vehicle.charge.timermode.on",NULL);
      NotifiedVehicleChargeTimermodeOn();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.charge.timermode.off",NULL);
      NotifiedVehicleChargeTimermodeOff();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_aux12v)
    {
    if (StandardMetrics.ms_v_env_aux12v->AsBool())
      {
      MyEvents.SignalEvent("vehicle.aux.12v.on", NULL);
      NotifiedVehicleAux12vOn();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.aux.12v.off", NULL);
      NotifiedVehicleAux12vOff();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_charging12v)
    {
    if (StandardMetrics.ms_v_env_charging12v->AsBool())
      {
      if (m_12v_ticker < 30)
        m_12v_ticker = 30; // min calmdown time
      MyEvents.SignalEvent("vehicle.charge.12v.start",NULL);
      NotifiedVehicleCharge12vStart();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.charge.12v.stop",NULL);
      NotifiedVehicleCharge12vStop();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_locked)
    {
    if (StandardMetrics.ms_v_env_locked->AsBool())
      {
      MyEvents.SignalEvent("vehicle.locked",NULL);
      NotifiedVehicleLocked();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.unlocked",NULL);
      NotifiedVehicleUnlocked();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_valet)
    {
    if (StandardMetrics.ms_v_env_valet->AsBool())
      {
      MyEvents.SignalEvent("vehicle.valet.on",NULL);
      if (m_autonotifications) NotifyValetEnabled();
      NotifiedVehicleValetOn();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.valet.off",NULL);
      if (m_autonotifications) NotifyValetDisabled();
      NotifiedVehicleValetOff();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_headlights)
    {
    if (StandardMetrics.ms_v_env_headlights->AsBool())
      {
      MyEvents.SignalEvent("vehicle.headlights.on",NULL);
      NotifiedVehicleHeadlightsOn();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.headlights.off",NULL);
      NotifiedVehicleHeadlightsOff();
      }
    }
  else if (metric == StandardMetrics.ms_v_door_hood)
    {
    if (StandardMetrics.ms_v_door_hood->AsBool() &&
        StandardMetrics.ms_v_env_valet->AsBool())
      {
      if (m_autonotifications) NotifyValetHood();
      }
    }
  else if (metric == StandardMetrics.ms_v_door_trunk)
    {
    if (StandardMetrics.ms_v_door_trunk->AsBool() &&
        StandardMetrics.ms_v_env_valet->AsBool())
      {
      if (m_autonotifications) NotifyValetTrunk();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_alarm)
    {
    if (StandardMetrics.ms_v_env_alarm->AsBool())
      {
      MyEvents.SignalEvent("vehicle.alarm.on",NULL);
      if (m_autonotifications) NotifyAlarmSounding();
      NotifiedVehicleAlarmOn();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.alarm.off",NULL);
      if (m_autonotifications) NotifyAlarmStopped();
      NotifiedVehicleAlarmOff();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_gear)
    {
    int gear = StandardMetrics.ms_v_env_gear->AsInt();
    if (gear < 0)
      MyEvents.SignalEvent("vehicle.gear.reverse", NULL);
    else if (gear > 0)
      MyEvents.SignalEvent("vehicle.gear.forward", NULL);
    else
      MyEvents.SignalEvent("vehicle.gear.neutral", NULL);
    NotifiedVehicleGear(gear);
    }
  else if (metric == StandardMetrics.ms_v_env_drivemode)
    {
    std::string event = "vehicle.drivemode." + StandardMetrics.ms_v_env_drivemode->AsString();
    MyEvents.SignalEvent(event, NULL);
    NotifiedVehicleDrivemode(StandardMetrics.ms_v_env_drivemode->AsInt());
    }
  else if (metric == StandardMetrics.ms_v_charge_mode)
    {
    std::string m = metric->AsString();
    const char* mc = m.c_str();
    MyEvents.SignalEvent("vehicle.charge.mode",(void*)mc, strlen(mc)+1);
    NotifiedVehicleChargeMode(mc);
    }
  else if (metric == StandardMetrics.ms_v_charge_state)
    {
    std::string m = metric->AsString();
    const char* mc = m.c_str();
    MyEvents.SignalEvent("vehicle.charge.state",(void*)mc, strlen(mc)+1);
    if (m == "done")
      {
      StandardMetrics.ms_v_charge_duration_full->SetValue(0);
      StandardMetrics.ms_v_charge_duration_range->SetValue(0);
      StandardMetrics.ms_v_charge_duration_soc->SetValue(0);
      }
    if (m_autonotifications)
      {
      m_chargestate_ticker = GetNotifyChargeStateDelay(mc);
      if (m_chargestate_ticker == 0)
        NotifyChargeState();
      }
    }
  else if (metric == StandardMetrics.ms_v_charge_type)
    {
    std::string m = metric->AsString();
    MyEvents.SignalEvent("vehicle.charge.type", (void*)m.c_str(), m.size()+1);
    NotifiedVehicleChargeType(m);
    }
  else if (metric == StandardMetrics.ms_v_gen_state)
    {
    std::string state = metric->AsString();
    MyEvents.SignalEvent("vehicle.gen.state", (void*)state.c_str(), state.size()+1);
    if (m_autonotifications)
      NotifyGenState();
    }
  else if (metric == StandardMetrics.ms_v_gen_type)
    {
    std::string m = metric->AsString();
    MyEvents.SignalEvent("vehicle.gen.type", (void*)m.c_str(), m.size()+1);
    NotifiedVehicleGenType(m);
    }
  else if (metric == StandardMetrics.ms_v_pos_speed)
    {
    // Collect data for trip speed average:
    const float min_speed = 5.0;          // slow speed exclusion [kph]
    float speed = StandardMetrics.ms_v_pos_speed->AsFloat();
    if (speed > min_speed)
      {
      m_drive_speedcnt++;
      m_drive_speedsum += speed;
      }
    }
  else if (metric == StdMetrics.ms_v_inv_power)
    // collect data for trip pos. and neg. (recuperated) motor energies
    {
    uint32_t now = esp_log_timestamp();
    if (now > m_inv_reftime)
      {
      float invpower = StdMetrics.ms_v_inv_power->AsFloat();
      float invenergy = (m_inv_refpower + invpower) * (now - m_inv_reftime) / (2*1000*3600); // average of last 2 values in kWh
      if ( invenergy < 0 )
        {
        m_inv_energyrecd -= invenergy; // sum should be positive
        }
      else
        {
        m_inv_energyused += invenergy;
        }
      m_inv_refpower = invpower;
      m_inv_reftime = now;
      }
    }
  else if (metric == StandardMetrics.ms_v_pos_acceleration)
    {
    if (m_brakelight_enable)
      CheckBrakelight();

    // Collect data for trip acceleration/deceleration average:
    const float min_accel = 2.5 / 3.6;    // cruising range exclusion [m/s²]
    float accel = StandardMetrics.ms_v_pos_acceleration->AsFloat();
    if (accel > min_accel)
      {
      m_drive_accelcnt++;
      m_drive_accelsum += accel;
      }
    else if (accel < -min_accel)
      {
      m_drive_decelcnt++;
      m_drive_decelsum += accel;
      }
    }
  else if (metric == StandardMetrics.ms_v_bat_power)
    {
    if (m_batpwr_smoothing > 0)
      m_batpwr_smoothed = (m_batpwr_smoothed + metric->AsFloat() * m_batpwr_smoothing) / (m_batpwr_smoothing + 1);
    else
      m_batpwr_smoothed = metric->AsFloat();
    }
  else if (metric == StdMetrics.ms_v_bat_current || metric == StdMetrics.ms_v_bat_cac ||
      metric == StdMetrics.ms_v_bat_range_full)
    {
    CalculateRangeSpeed();
    }
  else if (metric == StdMetrics.ms_m_obd2ecu_on)
    {
    if ( StdMetrics.ms_m_obd2ecu_on->AsBool() )
      {
      NotifiedOBD2ECUStart();
      }
    else
      {
      NotifiedOBD2ECUStop();
      }
    }
  }

/**
 * CalculateAcceleration: derive acceleration / deceleration level from speed change
 * Note:
 *  IF you want to let the framework calculate acceleration, call this after your regular
 *  update to StdMetrics.ms_v_pos_speed. This is optional, you can set ms_v_pos_acceleration
 *  yourself if your vehicle provides this metric.
 */
void OvmsVehicle::CalculateAcceleration()
  {
  uint32_t now = esp_log_timestamp();
  if (now > m_accel_reftime)
    {
    float speed = ABS(StdMetrics.ms_v_pos_speed->AsFloat(0, MetersPS));
    float accel = (speed - m_accel_refspeed) / (now - m_accel_reftime) * 1000;
    // smooth out road bumps & gear box backlash:
    if (m_accel_smoothing > 0)
      accel = (accel + StdMetrics.ms_v_pos_acceleration->AsFloat() * m_accel_smoothing) / (m_accel_smoothing + 1);
    StdMetrics.ms_v_pos_acceleration->SetValue(TRUNCPREC(accel, 3));
    m_accel_refspeed = speed;
    m_accel_reftime = now;
    }
  }

/**
 * CheckBrakelight: check for regenerative braking, control brakelight accordingly
 * Notes:
 *  a) This depends on a regular and frequent speed update with <= 100 ms period. If the vehicle
 *     delivers speed values at too large intervals, the trigger will still work but come
 *     too late (reducing/deactivating acceleration smoothing may help).
 *     If the vehicle raw speed is already smoothed, reducing acceleration smoothing will
 *     provide a faster trigger. Same applies for the battery power level.
 *  b) The battery power regen threshold is defined at -[brakelight.basepwr] for activation
 *     and +[brakelight.basepwr] for deactivation. The config default is 0 as that works
 *     on vehicles without the battery power metric.
 *  c) To reduce flicker the brake light has a minimum hold time of currently fixed 500 ms.
 *  d) Normal operation is "regen light XOR foot brake light", set [brakelight.ignftbrk]
 *     to true to disable this.
 * Override to customize.
 */
void OvmsVehicle::CheckBrakelight()
  {
  uint32_t now = esp_log_timestamp();
  float speed = ABS(StdMetrics.ms_v_pos_speed->AsFloat(0, Kph)) * 1000 / 3600;
  float accel = StdMetrics.ms_v_pos_acceleration->AsFloat();
  bool car_on = StdMetrics.ms_v_env_on->AsBool();
  bool footbrake = StdMetrics.ms_v_env_footbrake->AsFloat() > 0;
  const uint32_t holdtime = 500;

  // activate brake light?
  if (car_on && accel < -m_brakelight_on && speed >= 1 && m_batpwr_smoothed <= -m_brakelight_basepwr
    && (m_brakelight_ignftbrk || !footbrake))
    {
    if (!m_brakelight_start)
      {
      if (SetBrakelight(1))
        {
        ESP_LOGD(TAG, "brakelight on at speed=%.2f m/s, accel=%.2f m/s^2", speed, accel);
        m_brakelight_start = now;
        StdMetrics.ms_v_env_regenbrake->SetValue(true);
        }
      else
        ESP_LOGW(TAG, "can't activate brakelight");
      }
    else
      m_brakelight_start = now;
    }
  // deactivate brake light?
  else if (!car_on || accel >= -m_brakelight_off || speed < 1 || m_batpwr_smoothed > m_brakelight_basepwr
    || (!m_brakelight_ignftbrk && footbrake))
    {
    if (m_brakelight_start && now >= m_brakelight_start + holdtime)
      {
      if (SetBrakelight(0))
        {
        ESP_LOGD(TAG, "brakelight off at speed=%.2f m/s, accel=%.2f m/s^2", speed, accel);
        m_brakelight_start = 0;
        StdMetrics.ms_v_env_regenbrake->SetValue(false);
        }
      else
        ESP_LOGW(TAG, "can't deactivate brakelight");
      }
    }
  }

/**
 * SetBrakelight: hardware brake light control method
 * Override for custom control, e.g. CAN.
 */
bool OvmsVehicle::SetBrakelight(int on)
  {
#ifdef CONFIG_OVMS_COMP_MAX7317
  // port 2 = SN65 for esp32can
  if (m_brakelight_port == 1 || (m_brakelight_port >= 3 && m_brakelight_port <= 9))
    {
    MyPeripherals->m_max7317->Output(m_brakelight_port, on);
    return true;
    }
  else
    {
    ESP_LOGE(TAG, "SetBrakelight: invalid port configuration (valid: 1, 3..9)");
    return false;
    }
#else // CONFIG_OVMS_COMP_MAX7317
  ESP_LOGE(TAG, "SetBrakelight: OVMS_COMP_MAX7317 missing");
  return false;
#endif // CONFIG_OVMS_COMP_MAX7317
  }

/**
 * CalculateRangeSpeed: derive momentary charge gain/loss speed (range charged/discharged per hour)
 */
void OvmsVehicle::CalculateRangeSpeed()
  {
  float
    bat_current = StdMetrics.ms_v_bat_current->AsFloat(),
    bat_capacity = StdMetrics.ms_v_bat_cac->AsFloat(),
    range_full = StdMetrics.ms_v_bat_range_full->AsFloat();

  if (bat_capacity > 0 && range_full > 0)
    {
    *StdMetrics.ms_v_bat_range_speed = TRUNCPREC(-bat_current / bat_capacity * range_full, 1);
    }
  }

void OvmsVehicle::NotifyChargeState()
  {
  std::string m = StandardMetrics.ms_v_charge_state->AsString();
  if (m == "done")
    NotifyChargeDone();
  else if (m == "stopped" || m == "timerwait")
    NotifyChargeStopped();
  else if (m == "charging")
    NotifyChargeStart();
  else if (m == "topoff")
    NotifyChargeTopOff();
  else if (m == "heating")
    NotifyHeatingStart();

  if (m != "")
    NotifyGridLog();

  NotifiedVehicleChargeState(m.c_str());
  }

void OvmsVehicle::NotifyGenState()
  {
  std::string m = StandardMetrics.ms_v_gen_state->AsString();
  // Generator states TBD

  if (m != "")
    NotifyGridLog();

  NotifiedVehicleGenState(m);
  }

void OvmsVehicle::NotifyGridLog()
  {
  // Send grid (charge/generator) session log
  //  History type "*-LOG-Grid"
  //  Notification type "data", subtype "log.grid"

  int storetime_days = MyConfig.GetParamValueInt("notify", "log.grid.storetime", 0);
  if (storetime_days <= 0)
    return;

  std::ostringstream buf;
  buf
    << "*-LOG-Grid,1," << storetime_days * 86400        // V1, increment on additions

    << std::noboolalpha
    << "," << (StdMetrics.ms_v_pos_gpslock->AsBool() ? 1 : 0)
    << std::fixed
    << std::setprecision(6)
    << "," << StdMetrics.ms_v_pos_latitude->AsFloat()
    << "," << StdMetrics.ms_v_pos_longitude->AsFloat()
    << std::setprecision(1)
    << "," << StdMetrics.ms_v_pos_altitude->AsFloat()
    << "," << mp_encode(StdMetrics.ms_v_pos_location->AsString())

    << "," << mp_encode(StdMetrics.ms_v_charge_type->AsString())
    << "," << mp_encode(StdMetrics.ms_v_charge_state->AsString())
    << "," << mp_encode(StdMetrics.ms_v_charge_substate->AsString())
    << "," << mp_encode(StdMetrics.ms_v_charge_mode->AsString())
    << "," << StdMetrics.ms_v_charge_climit->AsFloat()
    << "," << StdMetrics.ms_v_charge_limit_range->AsFloat()
    << "," << StdMetrics.ms_v_charge_limit_soc->AsFloat()

    << "," << mp_encode(StdMetrics.ms_v_gen_type->AsString())
    << "," << mp_encode(StdMetrics.ms_v_gen_state->AsString())
    << "," << mp_encode(StdMetrics.ms_v_gen_substate->AsString())
    << "," << mp_encode(StdMetrics.ms_v_gen_mode->AsString())
    << "," << StdMetrics.ms_v_gen_climit->AsFloat()
    << "," << StdMetrics.ms_v_gen_limit_range->AsFloat()
    << "," << StdMetrics.ms_v_gen_limit_soc->AsFloat()

    << std::setprecision(3)

    << "," << m_last_chargetime
    << "," << StdMetrics.ms_v_charge_kwh->AsFloat()
    << "," << StdMetrics.ms_v_charge_kwh_grid->AsFloat()
    << "," << StdMetrics.ms_v_charge_kwh_grid_total->AsFloat()

    << "," << m_last_gentime
    << "," << StdMetrics.ms_v_gen_kwh->AsFloat()
    << "," << StdMetrics.ms_v_gen_kwh_grid->AsFloat()
    << "," << StdMetrics.ms_v_gen_kwh_grid_total->AsFloat()

    << std::setprecision(1)

    << "," << StdMetrics.ms_v_bat_soc->AsFloat()
    << "," << StdMetrics.ms_v_bat_range_est->AsFloat()
    << "," << StdMetrics.ms_v_bat_range_ideal->AsFloat()
    << "," << StdMetrics.ms_v_bat_range_full->AsFloat()

    << "," << StdMetrics.ms_v_bat_voltage->AsFloat()
    << "," << StdMetrics.ms_v_bat_temp->AsFloat()

    << "," << StdMetrics.ms_v_charge_temp->AsFloat()
    << "," << StdMetrics.ms_v_charge_12v_temp->AsFloat()
    << "," << StdMetrics.ms_v_env_temp->AsFloat()
    << "," << StdMetrics.ms_v_env_cabintemp->AsFloat()

    << std::setprecision(3)

    << "," << StdMetrics.ms_v_bat_soh->AsFloat()
    << "," << mp_encode(StdMetrics.ms_v_bat_health->AsString())
    << "," << StdMetrics.ms_v_bat_cac->AsFloat()

    << "," << StdMetrics.ms_v_bat_energy_used_total->AsFloat()
    << "," << StdMetrics.ms_v_bat_energy_recd_total->AsFloat()
    << "," << StdMetrics.ms_v_bat_coulomb_used_total->AsFloat()
    << "," << StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat()
    ;

  MyNotify.NotifyString("data", "log.grid", buf.str().c_str());
  }

void OvmsVehicle::NotifyVehicleOn()
  {
  NotifyTripLog();
  NotifiedVehicleOn();
  }

void OvmsVehicle::NotifyVehicleOff()
  {
  float trip = StdMetrics.ms_v_pos_trip->AsFloat();
  if (trip >= MyConfig.GetParamValueFloat("notify", "log.trip.minlength", 0.2))
    NotifyTripLog();
  if (trip >= MyConfig.GetParamValueFloat("notify", "report.trip.minlength", 0.2))
    NotifyTripReport();
  NotifiedVehicleOff();
  }

void OvmsVehicle::NotifyTripLog()
  {
  // Send trip log
  //  History type "*-LOG-Trip"
  //  Notification type "data", subtype "log.trip"

  int storetime_days = MyConfig.GetParamValueInt("notify", "log.trip.storetime", 0);
  if (storetime_days <= 0)
    return;

  // Get min/max TPMS values:
  const auto t_temp = StdMetrics.ms_v_tpms_temp->AsVector();
  const auto t_prss = StdMetrics.ms_v_tpms_pressure->AsVector();
  const auto t_hlth = StdMetrics.ms_v_tpms_health->AsVector();
  const auto t_temp_minmax = std::minmax_element(t_temp.begin(), t_temp.end());
  const auto t_prss_minmax = std::minmax_element(t_prss.begin(), t_prss.end());
  const auto t_hlth_minmax = std::minmax_element(t_hlth.begin(), t_hlth.end());

  std::ostringstream buf;
  buf
    << "*-LOG-Trip,1," << storetime_days * 86400        // V1, increment on additions

    << std::noboolalpha
    << "," << (StdMetrics.ms_v_pos_gpslock->AsBool() ? 1 : 0)
    << std::fixed
    << std::setprecision(8)
    << "," << StdMetrics.ms_v_pos_latitude->AsFloat()
    << "," << StdMetrics.ms_v_pos_longitude->AsFloat()
    << std::setprecision(1)
    << "," << StdMetrics.ms_v_pos_altitude->AsFloat()
    << "," << mp_encode(StdMetrics.ms_v_pos_location->AsString())

    << "," << StdMetrics.ms_v_pos_odometer->AsFloat()

    << "," << StdMetrics.ms_v_pos_trip->AsFloat()
    << "," << m_last_drivetime
    << "," << StdMetrics.ms_v_env_drivemode->AsInt()

    << "," << StdMetrics.ms_v_bat_soc->AsFloat()
    << "," << StdMetrics.ms_v_bat_range_est->AsFloat()
    << "," << StdMetrics.ms_v_bat_range_ideal->AsFloat()
    << "," << StdMetrics.ms_v_bat_range_full->AsFloat()

    << std::setprecision(3)

    << "," << StdMetrics.ms_v_bat_energy_used->AsFloat()
    << "," << StdMetrics.ms_v_bat_energy_recd->AsFloat()
    << "," << StdMetrics.ms_v_bat_coulomb_used->AsFloat()
    << "," << StdMetrics.ms_v_bat_coulomb_recd->AsFloat()

    << "," << StdMetrics.ms_v_bat_soh->AsFloat()
    << "," << mp_encode(StdMetrics.ms_v_bat_health->AsString())
    << "," << StdMetrics.ms_v_bat_cac->AsFloat()

    << "," << StdMetrics.ms_v_bat_energy_used_total->AsFloat()
    << "," << StdMetrics.ms_v_bat_energy_recd_total->AsFloat()
    << "," << StdMetrics.ms_v_bat_coulomb_used_total->AsFloat()
    << "," << StdMetrics.ms_v_bat_coulomb_recd_total->AsFloat()

    << std::setprecision(1)

    << "," << StdMetrics.ms_v_env_temp->AsFloat()
    << "," << StdMetrics.ms_v_env_cabintemp->AsFloat()
    << "," << StdMetrics.ms_v_bat_temp->AsFloat()
    << "," << StdMetrics.ms_v_inv_temp->AsFloat()
    << "," << StdMetrics.ms_v_mot_temp->AsFloat()
    << "," << StdMetrics.ms_v_charge_12v_temp->AsFloat()
    ;

  if (t_temp.empty())
    buf << ",,";
  else
    buf << "," << *std::get<0>(t_temp_minmax) << "," << *std::get<1>(t_temp_minmax);
  if (t_prss.empty())
    buf << ",,";
  else
    buf << "," << *std::get<0>(t_prss_minmax) << "," << *std::get<1>(t_prss_minmax);
  if (t_hlth.empty())
    buf << ",,";
  else
    buf << "," << *std::get<0>(t_hlth_minmax) << "," << *std::get<1>(t_hlth_minmax);

  MyNotify.NotifyString("data", "log.trip", buf.str().c_str());
  }

void OvmsVehicle::NotifyTripReport()
  {
  // Send trip report notification
  //  Notification type "info", subtype "drive.trip.report"
  bool send_report = MyConfig.GetParamValueBool("notify", "report.trip.enable", false);
  if (send_report)
    {
    StringWriter buf(200);
    CommandStatTrip(COMMAND_RESULT_NORMAL, &buf);
    MyNotify.NotifyString("info", "drive.trip.report", buf.c_str());
    }
  }

OvmsVehicle::vehicle_mode_t OvmsVehicle::VehicleModeKey(const std::string code)
  {
  vehicle_mode_t key;
  if      (code == "standard")      key = Standard;
  else if (code == "storage")       key = Storage;
  else if (code == "range")         key = Range;
  else if (code == "performance")   key = Performance;
  else key = Standard;
  return key;
  }


/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsVehicle::SetFeature(int key, const char *value)
  {
  switch (key)
    {
    case 8:
      MyConfig.SetParamValue("vehicle", "stream", value);
      return true;
    case 9:
      MyConfig.SetParamValue("vehicle", "minsoc", value);
      return true;
    case 14:
      MyConfig.SetParamValue("vehicle", "carbits", value);
      return true;
    case 15:
      MyConfig.SetParamValue("vehicle", "canwrite", value);
      return true;
    default:
      return false;
    }
  }

/**
 * GetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
const std::string OvmsVehicle::GetFeature(int key)
  {
  switch (key)
    {
    case 8:
      return MyConfig.GetParamValue("vehicle", "stream", "0");
    case 9:
      return MyConfig.GetParamValue("vehicle", "minsoc", "0");
    case 14:
      return MyConfig.GetParamValue("vehicle", "carbits", "0");
    case 15:
      return MyConfig.GetParamValue("vehicle", "canwrite", "0");
    default:
      return "0";
    }
  }

/**
 * ProcessMsgCommand: V2 compatibility protocol message command processing
 *  result: optional payload or message to return to the caller with the command response
 */
OvmsVehicle::vehicle_command_t OvmsVehicle::ProcessMsgCommand(std::string &result, int command, const char* args)
  {
  return NotImplemented;
  }


/**
 * PollerStateTicker: check for state changes (stub, override with vehicle implementation)
 *  This is called by VehicleTicker1() just before the next PollerSend().
 *  Implement your poller state transition logic in this method, so the changes
 *  will get applied immediately.
 */
void OvmsVehicle::PollerStateTicker(canbus* bus)
  {
  }

// Signal poller
void OvmsVehicle::PausePolling()
  {
#ifdef CONFIG_OVMS_COMP_POLLER
  MyPollers.PausePolling();
#endif
  }
void OvmsVehicle::ResumePolling()
  {
#ifdef CONFIG_OVMS_COMP_POLLER
  MyPollers.ResumePolling();
#endif
  }

#ifdef CONFIG_OVMS_COMP_POLLER
OvmsPoller::VehicleSignal *OvmsVehicle::GetPollerSignal()
  {
  if (!m_pollsignal)
    m_pollsignal = new OvmsVehicle::OvmsVehicleSignal(this);
  return m_pollsignal;
  }
void OvmsVehicle::PollSetPidList(canbus* bus, const OvmsPoller::poll_pid_t* plist)
  {
  m_poll_bus_default = bus;
  MyPollers.PollSetPidList(bus, plist, GetPollerSignal());
  }
#endif

/**
 * PollSetState: set the polling state
 *  Call this to change the polling state and restart the current polling list.
 *  This won't do anything if the state is already active. The state is changed without
 *  waiting for pending responses to finish (except PollSingleRequests).
 *
 *  @param state
 *    The polling state to activate (0 … VEHICLE_POLL_NSTATES)
 */
void OvmsVehicle::PollSetState(uint8_t state, canbus* bus)
  {
#ifdef CONFIG_OVMS_COMP_POLLER
  if (!bus)
    m_poll_state = state;
  MyPollers.PollSetState(state, bus);
#endif
  }

#ifdef CONFIG_OVMS_COMP_POLLER
int OvmsVehicle::PollSingleRequest(canbus* bus, uint32_t txid, uint32_t rxid,
                std::string request, std::string& response,
                int timeout_ms, uint8_t protocol)
  {

  if (!m_ready)
    return POLLSINGLE_TXFAILURE;
  auto poller = MyPollers.GetPoller(bus, true);
  if (!poller)
    return POLLSINGLE_TXFAILURE;
  return poller->PollSingleRequest(txid, rxid, request, response, timeout_ms, protocol);
  }

int OvmsVehicle::PollSingleRequest(canbus* bus, uint32_t txid, uint32_t rxid,
                uint8_t polltype, uint16_t pid, std::string& response,
                int timeout_ms, uint8_t protocol)
  {
  if (!m_ready)
    return POLLSINGLE_TXFAILURE;
  auto poller = MyPollers.GetPoller(bus, true);
  if (!poller)
    return POLLSINGLE_TXFAILURE;
  return poller->PollSingleRequest(txid, rxid, polltype, pid, response, timeout_ms, protocol);
  }

/** Set the 'tick' interval for the poller.
 * @param tick_time_ms The interval in ms between poll 'ticks'
 * @param secondary_ticks The number of ticks making up a primary tick (0/1 means no secondary ticks)
 * Only Primary ticks will allow starting the poll-queue again once it has reached the end.
 * Either secondary or primary ticks allow fetching of the next entry in the queue.
 */
void OvmsVehicle::PollSetTicker(uint16_t tick_time_ms, uint8_t secondary_ticks)
  {
  MyPollers.PollSetTicker(tick_time_ms, secondary_ticks);
  }

void OvmsVehicle::PollSetResponseSeparationTime(uint8_t septime)
  {
  MyPollers.PollSetResponseSeparationTime(septime);
  }
void OvmsVehicle::PollSetChannelKeepalive(uint16_t keepalive_seconds)
  {
  MyPollers.PollSetChannelKeepalive(keepalive_seconds);
  }
void OvmsVehicle::PollSetTimeBetweenSuccess(uint16_t time_between_ms)
  {
  MyPollers.PollSetTimeBetweenSuccess(time_between_ms);
  }

/**
 * IncomingPollReply: poll response handler (stub, override with vehicle implementation)
 *  This is called by PollerReceive() on each valid response frame for the current request.
 *  Be aware responses may consist of multiple frames, detectable e.g. by mlremain > 0.
 *  A typical pattern is to collect frames in a buffer until mlremain == 0.
 *
 *  @param job
 *    Status of the current Poll job
 *  @param data
 *    Payload
 *  @param length
 *    Payload size
 */
void OvmsVehicle::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
  {
  }

/**
 * IncomingPollError: Calls Vehicle poll response error handler
 *  This is called by PollerReceive() on reception of an OBD/UDS Negative Response Code (NRC),
 *  except if the code is requestCorrectlyReceived-ResponsePending (0x78), which is handled
 *  by the poller. See ISO 14229 Annex A.1 for the list of NRC codes.
 *
 *  @param job
 *    Status of the current Poll job
 *  @param code
 *    NRC detail code
 */
void OvmsVehicle::IncomingPollError(const OvmsPoller::poll_job_t &job, uint16_t code)
  {
  }

/**
 * IncomingPollTxCallback: poller TX callback (stub, override with vehicle implementation)
 *  This is called by PollerTxCallback() on TX success/failure for a poller request.
 *  You can use this to detect CAN bus issues, e.g. if the car switches off the OBD port.
 *
 *  ATT: this is executed in the main CAN task context. Keep it simple.
 *    Complex processing here will affect overall CAN performance.
 *
 *  @param job
 *    Status of the current Poll job
 *  @param success
 *    Frame transmission success
 */
void OvmsVehicle::IncomingPollTxCallback(const OvmsPoller::poll_job_t &job, bool success)
  {
  }

#endif

#ifdef CONFIG_OVMS_COMP_POLLER
void OvmsVehicle::IncomingRxFrame(const CAN_frame_t &frame)
  {
  SendIncomingFrame(&frame);
  }
#else
void OvmsVehicle::OvmsVehicleTask(void *pvParameters)
  {
  OvmsVehicle *me = (OvmsVehicle*)pvParameters;
  me->VehicleTask();
  }

void OvmsVehicle::VehicleTask()
  {

  CAN_frame_t entry;
  while (!m_is_shutdown)
    {
    if (xQueueReceive(m_vqueue, &entry, (portTickType)portMAX_DELAY)!=pdTRUE)
      continue;
    if (entry.origin != nullptr )
      SendIncomingFrame(&entry);
    }
  auto vtask = Atomic_GetAndNull(m_vtask);
  if (vtask)
    vTaskDelete(vtask);
  vTaskSuspend(nullptr);
  }
#endif

void OvmsVehicle::SendIncomingFrame(const CAN_frame_t *frame)
  {
  if (!m_ready)
    return;

  auto bus = frame->origin;

  // Pass frame to standard handlers:
  CAN_frame_t tmp_frame = *frame;
  if (m_can1 == bus) IncomingFrameCan1(&tmp_frame);
  else if (m_can2 == bus) IncomingFrameCan2(&tmp_frame);
  else if (m_can3 == bus) IncomingFrameCan3(&tmp_frame);
  else if (m_can4 == bus) IncomingFrameCan4(&tmp_frame);
  }

#ifdef CONFIG_OVMS_COMP_POLLER
void OvmsVehicle::PollRequest(canbus* bus, const std::string &name, const std::shared_ptr<OvmsPoller::PollSeriesEntry> &series)
  {
  auto poller = MyPollers.GetPoller(bus, true);
  poller->PollRequest(name, series);
  }

void OvmsVehicle::RemovePollRequest(canbus* bus, const std::string &name)
  {
  auto poller = MyPollers.GetPoller(bus, false);
  if (poller)
    poller->RemovePollRequest(name);
  }
#endif

#ifdef CONFIG_OVMS_COMP_POLLER
/** Does the specified bus have a non-empty PollList ?
  * @param bus Canbus to check or null for any bus
  */
bool OvmsVehicle::HasPollList(canbus* bus)
  {
  return MyPollers.HasPollList(bus);
  }
#endif

#ifdef CONFIG_OVMS_COMP_WEBSERVER
/**
 * GetDashboardConfig: template / default configuration
 *  (override with vehicle specific configuration)
 *  see https://api.highcharts.com/highcharts/yAxis for details on options
 */
void OvmsVehicle::GetDashboardConfig(DashboardConfig& cfg)
  {
  // Speed:
  dash_gauge_t speed_dash(NULL,Kph);
  speed_dash.SetMinMax(0, 200, 5);
  speed_dash.AddBand("green", 0, 120);
  speed_dash.AddBand("yellow", 120, 160);
  speed_dash.AddBand("red", 160, 200);

  // Voltage:
  dash_gauge_t voltage_dash(NULL,Volts);
  voltage_dash.SetMinMax(310, 410);
  voltage_dash.AddBand("red", 310, 325);
  voltage_dash.AddBand("yellow", 325, 340);
  voltage_dash.AddBand("green", 340, 410);

  // SOC:
  dash_gauge_t soc_dash("SOC ",Percentage);
  soc_dash.SetMinMax(0, 100);
  soc_dash.AddBand("red", 0, 12.5);
  soc_dash.AddBand("yellow", 12.5, 25);
  soc_dash.AddBand("green", 25, 100);

  // Efficiency:
  dash_gauge_t eff_dash(NULL,WattHoursPK);
  eff_dash.SetMinMax(0, 400);
  eff_dash.AddBand("green", 0, 200);
  eff_dash.AddBand("yellow", 200, 300);
  eff_dash.AddBand("red", 300, 400);

  // Power:
  dash_gauge_t power_dash(NULL,kW);
  power_dash.SetMinMax(-50, 200);
  power_dash.AddBand("violet", -50, 0);
  power_dash.AddBand("green", 0, 100);
  power_dash.AddBand("yellow", 100, 150);
  power_dash.AddBand("red", 150, 200);

  // Charger temperature:
  dash_gauge_t charget_dash("CHG ",Celcius);
  charget_dash.SetMinMax(20, 80);
  charget_dash.SetTick(20);
  charget_dash.AddBand("normal", 20, 65);
  charget_dash.AddBand("red", 65, 80);

  // Battery temperature:
  dash_gauge_t batteryt_dash("BAT ",Celcius);
  batteryt_dash.SetMinMax(-15, 65);
  batteryt_dash.SetTick(25);
  batteryt_dash.AddBand("red", -15, 0);
  batteryt_dash.AddBand("normal", 0, 50);
  batteryt_dash.AddBand("red", 50, 65);

  // Inverter temperature:
  dash_gauge_t invertert_dash("PEM ",Celcius);
  invertert_dash.SetMinMax(20, 80);
  invertert_dash.SetTick(20);
  invertert_dash.AddBand("normal", 20, 70);
  invertert_dash.AddBand("red", 70, 80);

  // Motor temperature:
  dash_gauge_t motort_dash("MOT ",Celcius);
  motort_dash.SetMinMax(50, 125);
  motort_dash.SetTick(25);
  motort_dash.AddBand("normal", 50, 110);
  motort_dash.AddBand("red", 110, 125);

  std::ostringstream str;
  str << "yAxis: ["
      << speed_dash << "," // Speed
      << voltage_dash << "," // Voltage
      << soc_dash << "," // SOC
      << eff_dash << "," // Efficiency
      << power_dash << "," // Power
      << charget_dash << "," // Charger temperature
      << batteryt_dash << "," // Battery temperature
      << invertert_dash << "," // Inverter temperature
      << motort_dash // Motor temperature
      << "]";
  cfg.gaugeset1 = str.str();
  }
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER

OvmsBatteryMon::OvmsBatteryMon()
  :
  m_dirty(false),
  m_to_notify(false),
  m_lastState(OvmsBatteryState::Unknown),
  m_diff_last(0),
  m_low_threshold(def_low_threshold),
  m_charge_threshold(def_charge_threshold)
  {
  }

void OvmsBatteryMon::add(float voltage)
  {
  long newVoltage = lroundf(voltage * entry_mult);
  int32_t iVoltage = static_cast<int32_t>(newVoltage);

  m_short_avg.add(iVoltage);
  m_long_avg.add(iVoltage);
#ifdef OVMS_DEBUG_BATTERYMON
  ESP_LOGD(OvmsHyundaiIoniqEv::TAG,
      "Aux Battery %d short=%d long=%d", iVoltage,
      m_short_avg.get(), m_long_avg.get());
#endif
  m_dirty = true;
  }

bool OvmsBatteryMon::checkStateChange()
  {
  bool res = false;
  if (m_dirty)
    {
    // Causes calculation and clearing dirty.
    state();
    }

  if (m_to_notify)
    {
    m_to_notify = false;
    res = true;
    }
  return res;
  }

OvmsBatteryState OvmsBatteryMon::state() const
  {
  if (m_dirty)
    {
    auto newState = calc_state(m_diff_last);
    m_dirty = false;
    if (newState != m_lastState)
      {
      m_to_notify = true;
      m_lastState = newState;
      }
    }
  return m_lastState;
  }

OvmsBatteryState OvmsBatteryMon::calc_state(int32_t &diff_last) const
  {
  if (m_long_avg.isEmpty())
    return OvmsBatteryState::Unknown;

  int32_t average_long, average_short;
  average_long = m_long_avg.get();
  average_short = m_short_avg.get();
  int32_t diff = (average_short - average_long);
  diff_last = diff;

  if (average_long < m_low_threshold)
    return OvmsBatteryState::Low;

  if (average_long > m_charge_threshold)
    {
    if (diff < chdip_threshold)
      return OvmsBatteryState::ChargingDip;
    if (diff > chblip_threshold)
      return OvmsBatteryState::ChargingBlip;
    return OvmsBatteryState::Charging;
    }

  if (diff > blip_threshold)
    return OvmsBatteryState::Blip;
  else if (diff < dip_threshold)
    return OvmsBatteryState::Dip;

  return OvmsBatteryState::Normal;
  }

float OvmsBatteryMon::average_lastf() const
  {
  return (float)(m_short_avg.get()) / entry_mult;
  }

float OvmsBatteryMon::average_long() const
  {
  return (float)(m_long_avg.get()) / entry_mult;
  }

float OvmsBatteryMon::diff_lastf() const
  {
  return (float)m_diff_last / entry_mult;
  }

float OvmsBatteryMon::low_threshold() const
  {
  return (float)m_low_threshold / entry_mult;
  }
float OvmsBatteryMon::charge_threshold() const
  {
  return (float)m_charge_threshold / entry_mult;
  }

const uint8_t OvmsBatteryMon::long_count;
const uint8_t OvmsBatteryMon::short_count;

std::string OvmsBatteryMon::to_string() const
  {
  std::stringstream ret;

  // Update state.
  auto cur_state = state();

  int32_t average_long, average_short;
  if (!m_long_avg.isEmpty())
    {
    average_long = m_long_avg.get();
    average_short = m_short_avg.get();
    float avg_long = (float(average_long) / entry_mult);
    float avg_short = (float(average_short) / entry_mult);
    float low_thresh = (float(m_low_threshold) / entry_mult);
    float charge_thresh = (float(m_charge_threshold) / entry_mult);

    ret.setf( std::ios::fixed, std::ios::floatfield);
    ret.precision(2);
    ret.width(0);
    ret << " low thresh=" << low_thresh << endl
        << " charge thresh=" << charge_thresh << endl;
    ret << " " << int(long_count) << "s avg=" << avg_long << "v" << endl
        << " " << int(short_count) << "s avg=" << avg_short << "v"  << endl
        << " diff=" << float(avg_short - avg_long) << "v" << endl;
    }
  ret.width(0);

  ret << " state=" << state_code(cur_state) << endl;

  return ret.str();
  }

const char *OvmsBatteryMon::state_code(OvmsBatteryState state)
  {
  switch (state)
    {
    case OvmsBatteryState::Unknown:      return "unknown";
    case OvmsBatteryState::Normal:       return "normal";
    case OvmsBatteryState::Charging:     return "charging";
    case OvmsBatteryState::ChargingDip:  return "charging.dip";
    case OvmsBatteryState::ChargingBlip: return "charging.blip";
    case OvmsBatteryState::Blip:         return "blip";
    case OvmsBatteryState::Dip:          return "dip";
    case OvmsBatteryState::Low:          return "low";
    }
  return "unknown";
  }
