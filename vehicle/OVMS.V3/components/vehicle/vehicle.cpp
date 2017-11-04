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

#include <stdio.h>
#include <ovms_command.h>
#include <ovms_metrics.h>
#include <metrics_standard.h>
#include "vehicle.h"

OvmsVehicleFactory MyVehicleFactory __attribute__ ((init_priority (2000)));

void vehicle_module(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (argc == 0)
    {
    MyVehicleFactory.ClearVehicle();
    }
  else
    {
    MyVehicleFactory.SetVehicle(argv[0]);
    }
  }

void vehicle_wakeup(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandWakeup())
    {
    case OvmsVehicle::Success:
      writer->puts("Vehicle has been woken");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: vehicle could not be woken");
      break;
    default:
      writer->puts("Error: Vehicle wake functionality not available");
      break;
    }
  }

void vehicle_homelink(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int homelink = atoi(argv[0]);
  if ((homelink<1)||(homelink>3))
    {
    writer->puts("Error: Homelink button should be in range 1..3");
    return;
    }

  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandHomelink((uint8_t) homelink))
    {
    case OvmsVehicle::Success:
      writer->printf("Homelink #%d activated\n",homelink);
      break;
    case OvmsVehicle::Fail:
      writer->printf("Error: Could not activate homelink #%d\n",homelink);
      break;
    default:
      writer->puts("Error: Homelink functionality not available");
      break;
    }
  }

void vehicle_lock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandLock(argv[0]))
    {
    case OvmsVehicle::Success:
      writer->puts("Vehicle locked");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: vehicle could not be locked");
      break;
    default:
      writer->puts("Error: Vehicle lock functionality not available");
      break;
    }
  }

void vehicle_unlock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandUnlock(argv[0]))
    {
    case OvmsVehicle::Success:
      writer->puts("Vehicle unlocked");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: vehicle could not be unlocked");
      break;
    default:
      writer->puts("Error: Vehicle unlock functionality not available");
      break;
    }
  }

void vehicle_valet(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandActivateValet(argv[0]))
    {
    case OvmsVehicle::Success:
      writer->puts("Vehicle valet mode activated");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: vehicle could not activate valet mode");
      break;
    default:
      writer->puts("Error: Vehicle valet functionality not available");
      break;
    }
  }

void vehicle_unvalet(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandDeactivateValet(argv[0]))
    {
    case OvmsVehicle::Success:
      writer->puts("Vehicle valet mode deactivated");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: vehicle could not deactivate valet mode");
      break;
    default:
      writer->puts("Error: Vehicle valet functionality not available");
      break;
    }
  }

void vehicle_charge_mode(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  const char* smode = cmd->GetName();
  OvmsVehicle::vehicle_mode_t mode = OvmsVehicle::Standard;
  if (strcmp(smode,"storage")==0)
    mode = OvmsVehicle::Storage;
  else if (strcmp(smode,"range")==0)
    mode = OvmsVehicle::Range;
  else if (strcmp(smode,"performance")==0)
    mode = OvmsVehicle::Performance;
  else if (strcmp(smode,"standard")==0)
    mode = OvmsVehicle::Standard;
  else
    {
    writer->printf("Error: Unrecognised charge mode (%s) not standard/storage/range/performance\n",smode);
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandSetChargeMode(mode))
    {
    case OvmsVehicle::Success:
      writer->printf("Charge mode '%s' set\n",smode);
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: Could not set charge mode");
      break;
    default:
      writer->puts("Error: Charge mode functionality not available");
      break;
    }
  }

void vehicle_charge_current(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int limit = atoi(argv[0]);

  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandSetChargeCurrent((uint16_t) limit))
    {
    case OvmsVehicle::Success:
      writer->printf("Charge current limit set to %dA\n",limit);
      break;
    case OvmsVehicle::Fail:
      writer->printf("Error: Could not sst charge current limit to %dA\n",limit);
      break;
    default:
      writer->puts("Error: Charge current limit functionality not available");
      break;
    }
  }

void vehicle_charge_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandStartCharge())
    {
    case OvmsVehicle::Success:
      writer->puts("Charge has been started");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: Could not start charge");
      break;
    default:
      writer->puts("Error: Charge start functionality not available");
      break;
    }
  }

void vehicle_charge_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandStopCharge())
    {
    case OvmsVehicle::Success:
      writer->puts("Charge has been stopped");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: Could not stop charge");
      break;
    default:
      writer->puts("Error: Charge stop functionality not available");
      break;
    }
  }

void vehicle_charge_cooldown(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandCooldown(true))
    {
    case OvmsVehicle::Success:
      writer->puts("Cooldown has been started");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: Could not start cooldown");
      break;
    default:
      writer->puts("Error: Cooldown functionality not available");
      break;
    }
  }

void vehicle_stat(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  if (MyVehicleFactory.m_currentvehicle->CommandStat(verbosity, writer) == OvmsVehicle::NotImplemented)
    {
    writer->puts("Error: Functionality not available");
    }
  }

OvmsVehicleFactory::OvmsVehicleFactory()
  {
  ESP_LOGI(TAG, "Initialising VEHICLE Factory (2000)");

  m_currentvehicle = NULL;

  OvmsCommand* cmd_vehicle = MyCommandApp.RegisterCommand("vehicle","Vehicle framework",NULL,"",0,0);
  cmd_vehicle->RegisterCommand("module","Set (or clear) vehicle module",vehicle_module,"<type>",0,1);

  MyCommandApp.RegisterCommand("wakeup","Wake up vehicle",vehicle_wakeup,"",0,0,true);
  MyCommandApp.RegisterCommand("homelink","Activate specified homelink button",vehicle_homelink,"<homelink>",1,1,true);
  MyCommandApp.RegisterCommand("lock","Lock vehicle",vehicle_lock,"<pin>",1,1,true);
  MyCommandApp.RegisterCommand("unlock","Unlock vehicle",vehicle_unlock,"<pin>",1,1,true);
  MyCommandApp.RegisterCommand("valet","Activate valet mode",vehicle_valet,"<pin>",1,1,true);
  MyCommandApp.RegisterCommand("unvalet","Deactivate valuet mode",vehicle_unvalet,"<pin>",1,1,true);
  OvmsCommand* cmd_charge = MyCommandApp.RegisterCommand("charge","Charging framework",NULL,"",0,0,true);
  OvmsCommand* cmd_chargemode = cmd_charge->RegisterCommand("mode","Set vehicle charge mode",NULL,"",0,0,true);
  cmd_chargemode->RegisterCommand("standard","Set vehicle standard charge mode",vehicle_charge_mode,"",0,0,true);
  cmd_chargemode->RegisterCommand("storage","Set vehicle standard charge mode",vehicle_charge_mode,"",0,0,true);
  cmd_chargemode->RegisterCommand("range","Set vehicle standard charge mode",vehicle_charge_mode,"",0,0,true);
  cmd_chargemode->RegisterCommand("performance","Set vehicle standard charge mode",vehicle_charge_mode,"",0,0,true);
  cmd_charge->RegisterCommand("start","Start a vehicle charge",vehicle_charge_start,"",0,0,true);
  cmd_charge->RegisterCommand("stop","Stop a vehicle charge",vehicle_charge_stop,"",0,0,true);
  cmd_charge->RegisterCommand("current","Limit charge current",vehicle_charge_current,"<amps>",1,1,true);
  cmd_charge->RegisterCommand("cooldown","Start a vehicle cooldown",vehicle_charge_cooldown,"",0,0,true);
  MyCommandApp.RegisterCommand("stat","Show vehicle status",vehicle_stat,"",0,0,false);
  }

OvmsVehicleFactory::~OvmsVehicleFactory()
  {
  if (m_currentvehicle)
    {
    delete m_currentvehicle;
    m_currentvehicle = NULL;
    }
  }

OvmsVehicle* OvmsVehicleFactory::NewVehicle(std::string VehicleType)
  {
  OvmsVehicleFactory::map_type::iterator iter = m_map.find(VehicleType);
  if (iter != m_map.end())
    {
    return iter->second();
    }
  return NULL;
  }

void OvmsVehicleFactory::ClearVehicle()
  {
  if (m_currentvehicle)
    {
    delete m_currentvehicle;
    m_currentvehicle = NULL;
    StandardMetrics.ms_v_type->SetValue("");
    }
  }

void OvmsVehicleFactory::SetVehicle(std::string type)
  {
  if (m_currentvehicle)
    {
    delete m_currentvehicle;
    m_currentvehicle = NULL;
    }
  m_currentvehicle = NewVehicle(type);
  StandardMetrics.ms_v_type->SetValue(type.c_str());
  }

static void OvmsVehicleRxTask(void *pvParameters)
  {
  OvmsVehicle *me = (OvmsVehicle*)pvParameters;
  me->RxTask();
  }

OvmsVehicle::OvmsVehicle()
  {
  m_can1 = NULL;
  m_can2 = NULL;
  m_can3 = NULL;
  m_ticker = 0;
  m_registeredlistener = false;

  m_poll_state = 0;
  m_poll_bus = NULL;
  m_poll_plist = NULL;
  m_poll_plcur = NULL;
  m_poll_ticker = 0;
  m_poll_moduleid_sent = 0;
  m_poll_moduleid_low = 0;
  m_poll_moduleid_high = 0;
  m_poll_type = 0;
  m_poll_pid = 0;
  m_poll_ml_remain = 0;
  m_poll_ml_offset = 0;
  m_poll_ml_frame = 0;

  m_rxqueue = xQueueCreate(20,sizeof(CAN_frame_t));
  xTaskCreatePinnedToCore(OvmsVehicleRxTask, "Vrx Task", 4096, (void*)this, 5, &m_rxtask, 1);
  
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&OvmsVehicle::VehicleTicker1, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.changed", std::bind(&OvmsVehicle::VehicleConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.mounted", std::bind(&OvmsVehicle::VehicleConfigChanged, this, _1, _2));
  }

OvmsVehicle::~OvmsVehicle()
  {
  if (m_can1) m_can1->SetPowerMode(Off);
  if (m_can2) m_can2->SetPowerMode(Off);
  if (m_can3) m_can3->SetPowerMode(Off);

  if (m_registeredlistener)
    {
    MyCan.DeregisterListener(m_rxqueue);
    m_registeredlistener = false;
    }

  vQueueDelete(m_rxqueue);
  vTaskDelete(m_rxtask);

  MyEvents.DeregisterEvent(TAG);
  }

const std::string OvmsVehicle::VehicleName()
  {
  return std::string("unknown");
  }

void OvmsVehicle::RxTask()
  {
  CAN_frame_t frame;

  while(1)
    {
    if (xQueueReceive(m_rxqueue, &frame, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      if ((frame.origin == m_poll_bus)&&(m_poll_plist))
        {
        // This is intended for our poller
        // ESP_LOGI(TAG, "Poller Rx candidate ID=%03x (expecting %03x-%03x)",frame.MsgID,m_poll_moduleid_low,m_poll_moduleid_high);
        if ((frame.MsgID >= m_poll_moduleid_low)&&(frame.MsgID <= m_poll_moduleid_high))
          {
          PollerReceive(&frame);
          }
        }
      if (m_can1 == frame.origin) IncomingFrameCan1(&frame);
      else if (m_can2 == frame.origin) IncomingFrameCan2(&frame);
      else if (m_can3 == frame.origin) IncomingFrameCan3(&frame);
      }
    }
  }

void OvmsVehicle::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicle::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicle::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicle::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
  {
  }

void OvmsVehicle::RegisterCanBus(int bus, CAN_mode_t mode, CAN_speed_t speed)
  {
  switch (bus)
    {
    case 1:
      m_can1 = (canbus*)MyPcpApp.FindDeviceByName("can1");
      m_can1->SetPowerMode(On);
      m_can1->Start(mode,speed);
      break;
    case 2:
      m_can2 = (canbus*)MyPcpApp.FindDeviceByName("can2");
      m_can2->SetPowerMode(On);
      m_can2->Start(mode,speed);
      break;
    case 3:
      m_can3 = (canbus*)MyPcpApp.FindDeviceByName("can3");
      m_can3->SetPowerMode(On);
      m_can3->Start(mode,speed);
      break;
    default:
      break;
    }

  if (!m_registeredlistener)
    {
    m_registeredlistener = true;
    MyCan.RegisterListener(m_rxqueue);
    }
  }

void OvmsVehicle::VehicleTicker1(std::string event, void* data)
  {
  m_ticker++;

  if (m_poll_plist)
    {
    PollerSend();
    }

  Ticker1(m_ticker);
  if ((m_ticker % 10) == 0) Ticker10(m_ticker);
  if ((m_ticker % 60) == 0) Ticker60(m_ticker);
  if ((m_ticker % 300) == 0) Ticker300(m_ticker);
  if ((m_ticker % 600) == 0) Ticker600(m_ticker);
  if ((m_ticker % 3600) == 0) Ticker3600(m_ticker);
  }

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

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandSetChargeMode(vehicle_mode_t mode)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandSetChargeCurrent(uint16_t limit)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandStartCharge()
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandStopCharge()
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandSetChargeTimer(bool timeron, uint16_t timerstart)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandCooldown(bool cooldownon)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandWakeup()
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandLock(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandUnlock(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandActivateValet(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandDeactivateValet(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandHomelink(uint8_t button)
  {
  return NotImplemented;
  }

/**
 * CommandStat: default implementation of vehicle status output
 */
OvmsVehicle::vehicle_command_t OvmsVehicle::CommandStat(int verbosity, OvmsWriter* writer)
  {
  metric_unit_t rangeUnit = Native; // TODO: use user config if set
  
  bool chargeport_open = StdMetrics.ms_v_door_chargeport->AsBool();
  if (chargeport_open)
    {
    std::string charge_mode = StdMetrics.ms_v_charge_mode->AsString();
    std::string charge_state = StdMetrics.ms_v_charge_state->AsString();
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
    
    writer->printf("%s - %s\n", charge_mode.c_str(), charge_state.c_str());
    
    if (show_details)
      {
      writer->printf("%s/%s\n",
        (char*) StdMetrics.ms_v_charge_voltage->AsUnitString("-", Native, 1).c_str(),
        (char*) StdMetrics.ms_v_charge_current->AsUnitString("-", Native, 1).c_str());
      
      int duration_full = StdMetrics.ms_v_charge_duration_full->AsInt();
      if (duration_full)
        writer->printf("Full: %d mins\n", duration_full);
      
      int duration_soc = StdMetrics.ms_v_charge_duration_soc->AsInt();
      if (duration_soc)
        writer->printf("%s: %d mins\n",
          (char*) StdMetrics.ms_v_charge_limit_soc->AsUnitString("SOC", Native, 0).c_str(),
          duration_soc);
      
      int duration_range = StdMetrics.ms_v_charge_duration_range->AsInt();
      if (duration_full)
        writer->printf("%s: %d mins\n",
          (char*) StdMetrics.ms_v_charge_limit_range->AsUnitString("Range", rangeUnit, 0).c_str(),
          duration_range);
      }
    }
  else
    {
    writer->puts("Not charging");
    }
  
  writer->printf("SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());
  
  const char* range_ideal = StdMetrics.ms_v_bat_range_ideal->AsUnitString("-", rangeUnit, 0).c_str();
  if (*range_ideal != '-')
    writer->printf("Ideal range: %s\n", range_ideal);
  
  const char* range_est = StdMetrics.ms_v_bat_range_est->AsUnitString("-", rangeUnit, 0).c_str();
  if (*range_est != '-')
    writer->printf("Est. range: %s\n", range_est);

  const char* odometer = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();
  if (*odometer != '-')
    writer->printf("ODO: %s\n", odometer);
  
  const char* cac = StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str();
  if (*cac != '-')
    writer->printf("CAC: %s\n", cac);
  
  const char* soh = StdMetrics.ms_v_bat_soh->AsUnitString("-", Native, 0).c_str();
  if (*soh != '-')
    writer->printf("SOH: %s\n", soh);
  
  return Success;
  }

void OvmsVehicle::VehicleConfigChanged(std::string event, void* param)
  {
  ConfigChanged((OvmsConfigParam*) param);
  }

void OvmsVehicle::ConfigChanged(OvmsConfigParam* param)
  {
  }

void OvmsVehicle::PollSetPidList(canbus* bus, const poll_pid_t* plist)
  {
  m_poll_bus = bus;
  m_poll_plist = plist;
  }

void OvmsVehicle::PollSetState(uint8_t state)
  {
  if ((state < VEHICLE_POLL_NSTATES)&&(state != m_poll_state))
    {
    m_poll_state = state;
    m_poll_ticker = 0;
    m_poll_plcur = NULL;
    }
  }

void OvmsVehicle::PollerSend()
  {
  if (m_poll_plcur == NULL) m_poll_plcur = m_poll_plist;

  while (m_poll_plcur->txmoduleid != 0)
    {
    if ((m_poll_plcur->polltime[m_poll_state] > 0)&&
        ((m_poll_ticker % m_poll_plcur->polltime[m_poll_state] ) == 0))
      {
      // We need to poll this one...
      m_poll_type = m_poll_plcur->type;
      m_poll_pid = m_poll_plcur->pid;
      if (m_poll_plcur->rxmoduleid != 0)
        {
        // send to <moduleid>, listen to response from <rmoduleid>:
        m_poll_moduleid_sent = m_poll_plcur->txmoduleid;
        m_poll_moduleid_low = m_poll_plcur->rxmoduleid;
        m_poll_moduleid_high = m_poll_plcur->rxmoduleid;
        }
      else
        {
        // broadcast: send to 0x7df, listen to all responses:
        m_poll_moduleid_sent = 0x7df;
        m_poll_moduleid_low = 0x7e8;
        m_poll_moduleid_high = 0x7ef;
        }

      // ESP_LOGI(TAG, "Polling for %d/%02x (expecting %03x/%03x-%03x)",
      //   m_poll_type,m_poll_pid,m_poll_moduleid_sent,m_poll_moduleid_low,m_poll_moduleid_high);
      CAN_frame_t txframe;
      memset(&txframe,0,sizeof(txframe));
      txframe.origin = m_poll_bus;
      txframe.MsgID = m_poll_moduleid_sent;
      txframe.FIR.B.FF = CAN_frame_std;
      txframe.FIR.B.DLC = 8;
      switch (m_poll_plcur->type)
        {
        case VEHICLE_POLL_TYPE_OBDIICURRENT:
        case VEHICLE_POLL_TYPE_OBDIIFREEZE:
        case VEHICLE_POLL_TYPE_OBDIISESSION:
          // 8 bit PID request for single frame response:
          txframe.data.u8[0] = 0x02;
          txframe.data.u8[1] = m_poll_type;
          txframe.data.u8[2] = m_poll_pid;
          break;
        case VEHICLE_POLL_TYPE_OBDIIVEHICLE:
        case VEHICLE_POLL_TYPE_OBDIIGROUP:
          // 8 bit PID request for multi frame response:
          m_poll_ml_remain = 0;
          txframe.data.u8[0] = 0x02;
          txframe.data.u8[1] = m_poll_type;
          txframe.data.u8[2] = m_poll_pid;
          break;
        case VEHICLE_POLL_TYPE_OBDIIEXTENDED:
          // 16 bit PID request:
          txframe.data.u8[0] = 0x03;
          txframe.data.u8[1] = VEHICLE_POLL_TYPE_OBDIIEXTENDED;    // Get extended PID
          txframe.data.u8[2] = m_poll_pid >> 8;
          txframe.data.u8[3] = m_poll_pid & 0xff;
          break;
        }
      m_poll_bus->Write(&txframe);
      m_poll_plcur++;
      return;
      }
    m_poll_plcur++;
    }

  m_poll_plcur = m_poll_plist;
  m_poll_ticker++;
  if (m_poll_ticker > 3600) m_poll_ticker -= 3600;
  }

void OvmsVehicle::PollerReceive(CAN_frame_t* frame)
  {
  // ESP_LOGI(TAG, "Receive Poll Response for %d/%02x",m_poll_type,m_poll_pid);
  switch (m_poll_type)
    {
    case VEHICLE_POLL_TYPE_OBDIICURRENT:
    case VEHICLE_POLL_TYPE_OBDIIFREEZE:
    case VEHICLE_POLL_TYPE_OBDIISESSION:
      // 8 bit PID single frame response:
      if ((frame->data.u8[1] == 0x40+m_poll_type)&&
          (frame->data.u8[2] == m_poll_pid))
        {
        IncomingPollReply(m_poll_bus, m_poll_type, m_poll_pid, &frame->data.u8[3], 5, 0);
        return;
        }
      break;
    case VEHICLE_POLL_TYPE_OBDIIVEHICLE:
    case VEHICLE_POLL_TYPE_OBDIIGROUP:
      // 8 bit PID multiple frame response:
      if (((frame->data.u8[0]>>4) == 0x1)&&
          (frame->data.u8[2] == 0x40+m_poll_type)&&
          (frame->data.u8[3] == m_poll_pid))
        {
        // First frame; send flow control frame:
        CAN_frame_t txframe;
        memset(&txframe,0,sizeof(txframe));
        txframe.origin = m_poll_bus;
        txframe.FIR.B.FF = CAN_frame_std;
        txframe.FIR.B.DLC = 8;

        if (m_poll_moduleid_sent == 0x7df)
          {
          // broadcast request: derive module ID from response ID:
          // (Note: this only works for the SAE standard ID scheme)
          txframe.MsgID = frame->MsgID - 8;
          }
        else
          {
          // use known module ID:
          txframe.MsgID = m_poll_moduleid_sent;
          }

        txframe.data.u8[0] = 0x30; // flow control frame type
        txframe.data.u8[1] = 0x00; // request all frames available
        txframe.data.u8[2] = 0x19; // with 25ms send interval
        m_poll_bus->Write(&txframe);

        // prepare frame processing, first frame contains first 3 bytes:
        m_poll_ml_remain = (((uint16_t)(frame->data.u8[0]&0x0f))<<8) + frame->data.u8[1] - 3;
        m_poll_ml_offset = 3;
        m_poll_ml_frame = 0;
        // ESP_LOGI(TAG, "Poll ML first frame (frame=%d, remain=%d)",m_poll_ml_frame,m_poll_ml_remain);
        IncomingPollReply(m_poll_bus, m_poll_type, m_poll_pid, &frame->data.u8[5], 3, m_poll_ml_remain);
        return;
        }
      else if (((frame->data.u8[0]>>4)==0x2)&&(m_poll_ml_remain>0))
        {
        // Consecutive frame (1 control + 7 data bytes)
        uint16_t len;
        if (m_poll_ml_remain>7)
          {
          m_poll_ml_remain -= 7;
          m_poll_ml_offset += 7;
          len = 7;
          }
        else
          {
          len = m_poll_ml_remain;
          m_poll_ml_offset += m_poll_ml_remain;
          m_poll_ml_remain = 0;
          }
        m_poll_ml_frame++;
        // ESP_LOGI(TAG, "Poll ML subsequent frame (frame=%d, remain=%d)",m_poll_ml_frame,m_poll_ml_remain);
        IncomingPollReply(m_poll_bus, m_poll_type, m_poll_pid, &frame->data.u8[1], len, m_poll_ml_remain);
        return;
        }
      break;
    case VEHICLE_POLL_TYPE_OBDIIEXTENDED:
      // 16 bit PID response:
      if ((frame->data.u8[1] == 0x62)&&
          ((frame->data.u8[3]+(((uint16_t) frame->data.u8[2]) << 8)) == m_poll_pid))
        {
        IncomingPollReply(m_poll_bus, m_poll_type, m_poll_pid, &frame->data.u8[4], 4, 0);
        return;
        }
      break;
    }
  }

