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
#include <algorithm>
#include <ovms_command.h>
#include <ovms_metrics.h>
#include <ovms_notify.h>
#include <metrics_standard.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_webserver.h>
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <string_writer.h>
#include "vehicle.h"

#undef SQR
#define SQR(n) ((n)*(n))
#undef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))
#undef TRUNCPREC
#define TRUNCPREC(fval,prec) (trunc((fval) * pow(10,(prec))) / pow(10,(prec)))
#undef ROUNDPREC
#define ROUNDPREC(fval,prec) (round((fval) * pow(10,(prec))) / pow(10,(prec)))


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

void vehicle_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Type Name");
  for (OvmsVehicleFactory::map_vehicle_t::iterator k=MyVehicleFactory.m_vmap.begin(); k!=MyVehicleFactory.m_vmap.end(); ++k)
    {
    writer->printf("%-4.4s %s\n",k->first,k->second.name);
    }
  }

void vehicle_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle != NULL)
    {
    MyVehicleFactory.m_currentvehicle->Status(verbosity, writer);
    }
  else
    {
    writer->puts("No vehicle module selected");
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

  int durationms = 1000;
  if (argc>1) durationms= atoi(argv[1]);
  if (durationms < 100)
    {
    writer->puts("Error: Minimum homelink timer duration 100ms");
    return;
    }

  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandHomelink(homelink-1, durationms))
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

void bms_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle != NULL)
    {
    MyVehicleFactory.m_currentvehicle->BmsStatus(verbosity, writer);
    }
  else
    {
    writer->puts("No vehicle module selected");
    }
  }

void bms_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle != NULL)
    {
    MyVehicleFactory.m_currentvehicle->BmsResetCellStats();
    writer->puts("BMS cell statistics have been reset.");
    }
  else
    {
    writer->puts("No vehicle module selected");
    }
  }

void bms_alerts(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle != NULL)
    {
    MyVehicleFactory.m_currentvehicle->FormatBmsAlerts(verbosity, writer, true);
    }
  else
    {
    writer->puts("No vehicle module selected");
    }
  }

OvmsVehicleFactory::OvmsVehicleFactory()
  {
  ESP_LOGI(TAG, "Initialising VEHICLE Factory (2000)");

  m_currentvehicle = NULL;
  m_currentvehicletype.clear();

  OvmsCommand* cmd_vehicle = MyCommandApp.RegisterCommand("vehicle","Vehicle framework",NULL,"",0,0, true);
  cmd_vehicle->RegisterCommand("module","Set (or clear) vehicle module",vehicle_module,"<type>",0,1, true);
  cmd_vehicle->RegisterCommand("list","Show list of available vehicle modules",vehicle_list,"",0,0, true);
  cmd_vehicle->RegisterCommand("status","Show vehicle module status",vehicle_status,"",0,0, true);

  MyCommandApp.RegisterCommand("wakeup","Wake up vehicle",vehicle_wakeup,"",0,0,true);
  MyCommandApp.RegisterCommand("homelink","Activate specified homelink button",vehicle_homelink,"<homelink><durationms>",1,2,true);
  MyCommandApp.RegisterCommand("lock","Lock vehicle",vehicle_lock,"<pin>",1,1,true);
  MyCommandApp.RegisterCommand("unlock","Unlock vehicle",vehicle_unlock,"<pin>",1,1,true);
  MyCommandApp.RegisterCommand("valet","Activate valet mode",vehicle_valet,"<pin>",1,1,true);
  MyCommandApp.RegisterCommand("unvalet","Deactivate valet mode",vehicle_unvalet,"<pin>",1,1,true);

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

  MyCommandApp.RegisterCommand("stat","Show vehicle status",vehicle_stat,"",0,0,true);

  OvmsCommand* cmd_bms = MyCommandApp.RegisterCommand("bms","BMS framework",NULL,"",0,0, true);
  cmd_bms->RegisterCommand("status","Show BMS status",bms_status,"",0,0, true);
  cmd_bms->RegisterCommand("reset","Reset BMS statistics",bms_reset,"",0,0, true);
  cmd_bms->RegisterCommand("alerts","Show BMS alerts",bms_alerts,"",0,0, true);
  }

OvmsVehicleFactory::~OvmsVehicleFactory()
  {
  if (m_currentvehicle)
    {
    delete m_currentvehicle;
    m_currentvehicle = NULL;
    m_currentvehicletype.clear();
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
  if (m_currentvehicle)
    {
    delete m_currentvehicle;
    m_currentvehicle = NULL;
    m_currentvehicletype.clear();
    StandardMetrics.ms_v_type->SetValue("");
    MyEvents.SignalEvent("vehicle.type.cleared", NULL);
    }
  }

void OvmsVehicleFactory::SetVehicle(const char* type)
  {
  if (m_currentvehicle)
    {
    delete m_currentvehicle;
    m_currentvehicle = NULL;
    m_currentvehicletype.clear();
    }
  m_currentvehicle = NewVehicle(type);
  m_currentvehicletype = std::string(type);
  StandardMetrics.ms_v_type->SetValue(m_currentvehicle ? type : "");
  MyEvents.SignalEvent("vehicle.type.set", (void*)type, strlen(type)+1);
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
  m_12v_ticker = 0;
  m_chargestate_ticker = 0;
  m_registeredlistener = false;
  m_autonotifications = true;

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

  m_bms_voltages = NULL;
  m_bms_vmins = NULL;
  m_bms_vmaxs = NULL;
  m_bms_vdevmaxs = NULL;
  m_bms_valerts = NULL;
  m_bms_valerts_new = 0;
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

  m_rxqueue = xQueueCreate(CONFIG_OVMS_VEHICLE_CAN_RX_QUEUE_SIZE,sizeof(CAN_frame_t));
  xTaskCreatePinnedToCore(OvmsVehicleRxTask, "OVMS Vehicle",
    CONFIG_OVMS_VEHICLE_RXTASK_STACK, (void*)this, 10, &m_rxtask, 1);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&OvmsVehicle::VehicleTicker1, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.changed", std::bind(&OvmsVehicle::VehicleConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.mounted", std::bind(&OvmsVehicle::VehicleConfigChanged, this, _1, _2));

  MyMetrics.RegisterListener(TAG, "*", std::bind(&OvmsVehicle::MetricModified, this, _1));
  }

OvmsVehicle::~OvmsVehicle()
  {
  if (m_can1) m_can1->SetPowerMode(Off);
  if (m_can2) m_can2->SetPowerMode(Off);
  if (m_can3) m_can3->SetPowerMode(Off);

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

  if (m_registeredlistener)
    {
    MyCan.DeregisterListener(m_rxqueue);
    m_registeredlistener = false;
    }

  vQueueDelete(m_rxqueue);
  vTaskDelete(m_rxtask);

  MyEvents.DeregisterEvent(TAG);
  MyMetrics.DeregisterListener(TAG);
  }

const char* OvmsVehicle::VehicleShortName()
  {
  return MyVehicleFactory.ActiveVehicleName();
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

void OvmsVehicle::Status(int verbosity, OvmsWriter* writer)
  {
  writer->puts("Vehicle module loaded and running");
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

bool OvmsVehicle::PinCheck(char* pin)
  {
  if (!MyConfig.IsDefined("password","pin")) return false;

  std::string vpin = MyConfig.GetParamValue("password","pin");
  return (strcmp(vpin.c_str(),pin)==0);
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

  if (StandardMetrics.ms_v_env_on->AsBool())
    {
    StandardMetrics.ms_v_env_parktime->SetValue(0);
    StandardMetrics.ms_v_env_drivetime->SetValue(StandardMetrics.ms_v_env_drivetime->AsInt() + 1);
    }
  else
    {
    StandardMetrics.ms_v_env_drivetime->SetValue(0);
    StandardMetrics.ms_v_env_parktime->SetValue(StandardMetrics.ms_v_env_parktime->AsInt() + 1);
    }

  if (StandardMetrics.ms_v_charge_inprogress->AsBool())
    StandardMetrics.ms_v_charge_time->SetValue(StandardMetrics.ms_v_charge_time->AsInt() + 1);
  else
    StandardMetrics.ms_v_charge_time->SetValue(0);

  if (m_chargestate_ticker > 0 && --m_chargestate_ticker == 0)
    NotifyChargeState();

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
  else if ((m_ticker % 60) == 0)
    {
    // check 12V voltage:
    float volt = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
    float vref = StandardMetrics.ms_v_bat_12v_voltage_ref->AsFloat();
    bool alert_on = StandardMetrics.ms_v_bat_12v_voltage_alert->AsBool();
    float alert_threshold = MyConfig.GetParamValueFloat("vehicle", "12v.alert", 1.6);
    if (vref > 0 && volt > 0 && vref - volt > alert_threshold && !alert_on)
      {
      StandardMetrics.ms_v_bat_12v_voltage_alert->SetValue(true);
      MyEvents.SignalEvent("vehicle.alert.12v.on", NULL);
      if (m_autonotifications) Notify12vCritical();
      }
    else if (vref - volt < alert_threshold * 0.6 && alert_on)
      {
      StandardMetrics.ms_v_bat_12v_voltage_alert->SetValue(false);
      MyEvents.SignalEvent("vehicle.alert.12v.off", NULL);
      if (m_autonotifications) Notify12vRecovered();
      }
    } // 12V battery monitor

  // BMS alerts:
  if (m_bms_valerts_new || m_bms_talerts_new)
    {
    if (m_autonotifications) NotifyBmsAlerts();
    m_bms_valerts_new = 0;
    m_bms_talerts_new = 0;
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

void OvmsVehicle::NotifyChargeStart()
  {
  StringWriter buf(200);
  CommandStat(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","charge.started",buf.c_str());
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
  if (StdMetrics.ms_v_charge_substate->AsString() == "scheduledstop")
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
  float vref = StandardMetrics.ms_v_bat_12v_voltage_ref->AsFloat();

  MyNotify.NotifyStringf("alert", "batt.12v.alert", "12V Battery critical: %.1fV (ref=%.1fV)", volt, vref);
  }

void OvmsVehicle::Notify12vRecovered()
  {
  float volt = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
  float vref = StandardMetrics.ms_v_bat_12v_voltage_ref->AsFloat();

  MyNotify.NotifyStringf("alert", "batt.12v.recovered", "12V Battery restored: %.1fV (ref=%.1fV)", volt, vref);
  }

void OvmsVehicle::NotifyBmsAlerts()
  {
  StringWriter buf(200);
  if (FormatBmsAlerts(COMMAND_RESULT_SMS, &buf, false))
    MyNotify.NotifyString("alert", "batt.bms.alert", buf.c_str());
  }

// Default efficiency calculation by speed & power per second, average smoothed over 5 seconds.
// Override if your vehicle provides more detail.
void OvmsVehicle::CalculateEfficiency()
  {
  float consumption = 0;
  if (StdMetrics.ms_v_pos_speed->AsFloat() >= 5)
    consumption = StdMetrics.ms_v_bat_power->AsFloat(0, Watts) / StdMetrics.ms_v_pos_speed->AsFloat();
  StdMetrics.ms_v_bat_consumption->SetValue((StdMetrics.ms_v_bat_consumption->AsFloat() * 4 + consumption) / 5);
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

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandClimateControl(bool climatecontrolon)
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

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandHomelink(int button, int durationms)
  {
  return NotImplemented;
  }

/**
 * CommandStat: default implementation of vehicle status output
 */
OvmsVehicle::vehicle_command_t OvmsVehicle::CommandStat(int verbosity, OvmsWriter* writer)
  {
  metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

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
      if (duration_full > 0)
        writer->printf("Full: %d mins\n", duration_full);

      int duration_soc = StdMetrics.ms_v_charge_duration_soc->AsInt();
      if (duration_soc > 0)
        writer->printf("%s: %d mins\n",
          (char*) StdMetrics.ms_v_charge_limit_soc->AsUnitString("SOC", Native, 0).c_str(),
          duration_soc);

      int duration_range = StdMetrics.ms_v_charge_duration_range->AsInt();
      if (duration_range > 0)
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

void OvmsVehicle::MetricModified(OvmsMetric* metric)
  {
  if (metric == StandardMetrics.ms_v_env_on)
    {
    if (StandardMetrics.ms_v_env_on->AsBool())
      {
      MyEvents.SignalEvent("vehicle.on",NULL);
      NotifiedVehicleOn();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.off",NULL);
      NotifiedVehicleOff();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_awake)
    {
    if (StandardMetrics.ms_v_env_awake->AsBool())
      {
      NotifiedVehicleAwake();
      MyEvents.SignalEvent("vehicle.awake",NULL);
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
  else if (metric == StandardMetrics.ms_v_charge_mode)
    {
    const char* m = metric->AsString().c_str();
    MyEvents.SignalEvent("vehicle.charge.mode",(void*)m, strlen(m)+1);
    NotifiedVehicleChargeMode(m);
    }
  else if (metric == StandardMetrics.ms_v_charge_state)
    {
    const char* m = metric->AsString().c_str();
    MyEvents.SignalEvent("vehicle.charge.state",(void*)m, strlen(m)+1);
    if (strcmp(m,"done")==0)
      {
      StandardMetrics.ms_v_charge_duration_full->SetValue(0);
      StandardMetrics.ms_v_charge_duration_range->SetValue(0);
      StandardMetrics.ms_v_charge_duration_soc->SetValue(0);
      }
    if (m_autonotifications)
      {
      m_chargestate_ticker = GetNotifyChargeStateDelay(m);
      if (m_chargestate_ticker == 0)
        NotifyChargeState();
      }
    NotifiedVehicleChargeState(m);
    }
  }

void OvmsVehicle::NotifyChargeState()
  {
  const char* m = StandardMetrics.ms_v_charge_state->AsString().c_str();
  if (strcmp(m,"done")==0)
    NotifyChargeDone();
  else if (strcmp(m,"stopped")==0)
    NotifyChargeStopped();
  else if (strcmp(m,"charging")==0)
    NotifyChargeStart();
  else if (strcmp(m,"topoff")==0)
    NotifyChargeStart();
  else if (strcmp(m,"heating")==0)
    NotifyHeatingStart();
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
        // First frame is 4 bytes header (2 ISO-TP, 2 OBDII), 4 bytes data:
        // [first=1,lenH] [lenL] [type+40] [pid] [data0] [data1] [data2] [data3]
        // Note that the value of 'len' includes the OBDII type and pid bytes,
        // but we don't count these in the data we pass to IncomingPollReply.
        //
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

        // prepare frame processing, first frame contains first 4 bytes:
        m_poll_ml_remain = (((uint16_t)(frame->data.u8[0]&0x0f))<<8) + frame->data.u8[1] - 2 - 4;
        m_poll_ml_offset = 4;
        m_poll_ml_frame = 0;

        // ESP_LOGI(TAG, "Poll ML first frame (frame=%d, remain=%d)",m_poll_ml_frame,m_poll_ml_remain);
        IncomingPollReply(m_poll_bus, m_poll_type, m_poll_pid, &frame->data.u8[4], 4, m_poll_ml_remain);
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

// BMS helpers

void OvmsVehicle::BmsSetCellArrangementVoltage(int readings, int readingspermodule)
  {
  if (m_bms_voltages != NULL) delete m_bms_voltages;
  m_bms_voltages = new float[readings];
  if (m_bms_vmins != NULL) delete m_bms_vmins;
  m_bms_vmins = new float[readings];
  if (m_bms_vmaxs != NULL) delete m_bms_vmaxs;
  m_bms_vmaxs = new float[readings];
  if (m_bms_vdevmaxs != NULL) delete m_bms_vdevmaxs;
  m_bms_vdevmaxs = new float[readings];
  if (m_bms_valerts != NULL) delete m_bms_valerts;
  m_bms_valerts = new short[readings];

  m_bms_bitset_v.clear();
  m_bms_bitset_v.reserve(readings);

  m_bms_readings_v = readings;
  m_bms_readingspermodule_v = readingspermodule;

  BmsResetCellVoltages();
  }

void OvmsVehicle::BmsSetCellArrangementTemperature(int readings, int readingspermodule)
  {
  if (m_bms_temperatures != NULL) delete m_bms_temperatures;
  m_bms_temperatures = new float[readings];
  if (m_bms_tmins != NULL) delete m_bms_tmins;
  m_bms_tmins = new float[readings];
  if (m_bms_tmaxs != NULL) delete m_bms_tmaxs;
  m_bms_tmaxs = new float[readings];
  if (m_bms_tdevmaxs != NULL) delete m_bms_tdevmaxs;
  m_bms_tdevmaxs = new float[readings];
  if (m_bms_talerts != NULL) delete m_bms_talerts;
  m_bms_talerts = new short[readings];
  m_bms_talerts_new = 0;

  m_bms_bitset_t.clear();
  m_bms_bitset_t.reserve(readings);

  m_bms_readings_t = readings;
  m_bms_readingspermodule_t = readingspermodule;

  BmsResetCellTemperatures();
  }

void OvmsVehicle::BmsSetCellLimitsVoltage(float min, float max)
  {
  m_bms_limit_vmin = min;
  m_bms_limit_vmax = max;
  }

void OvmsVehicle::BmsSetCellLimitsTemperature(float min, float max)
  {
  m_bms_limit_tmin = min;
  m_bms_limit_tmax = max;
  }

void OvmsVehicle::BmsSetCellVoltage(int index, float value)
  {
  // ESP_LOGI(TAG,"BmsSetCellVoltage(%d,%f) c=%d", index, value, m_bms_bitset_cv);
  if ((index<0)||(index>=m_bms_readings_v)) return;
  if ((value<m_bms_limit_vmin)||(value>m_bms_limit_vmax)) return;
  m_bms_voltages[index] = value;

  if (! m_bms_has_voltages)
    {
    m_bms_vmins[index] = value;
    m_bms_vmaxs[index] = value;
    }
  else if (m_bms_vmins[index] > value)
    m_bms_vmins[index] = value;
  else if (m_bms_vmaxs[index] < value)
    m_bms_vmaxs[index] = value;

  if (m_bms_bitset_v[index] == false) m_bms_bitset_cv++;
  if (m_bms_bitset_cv == m_bms_readings_v)
    {
    // get min, max, avg & standard deviation:
    double sum=0, sqrsum=0, stddev=0;
    float min=0, max=0, avg;
    for (int i=0; i<m_bms_readings_v; i++)
      {
      sum += m_bms_voltages[i];
      sqrsum += SQR(m_bms_voltages[i]);
      if (min==0 || m_bms_voltages[i]<min)
        min = m_bms_voltages[i];
      if (max==0 || m_bms_voltages[i]>max)
        max = m_bms_voltages[i];
      }
    avg = ROUNDPREC(sum / m_bms_readings_v, 5);
    stddev = ROUNDPREC(sqrt((sqrsum / m_bms_readings_v) - SQR(avg)), 5);
    // check cell deviations:
    float dev;
    float thr_warn  = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.warn", stddev * 1.2);
    float thr_alert = MyConfig.GetParamValueFloat("vehicle", "bms.dev.voltage.alert", stddev * 2.0);
    for (int i=0; i<m_bms_readings_v; i++)
      {
      dev = ROUNDPREC(m_bms_voltages[i] - avg, 5);
      if (ABS(dev) > ABS(m_bms_vdevmaxs[i]))
        m_bms_vdevmaxs[i] = dev;
      if (ABS(dev) >= thr_alert && m_bms_valerts[i] != 2)
        {
        m_bms_valerts[i] = 2;
        m_bms_valerts_new++; // trigger notification
        }
      else if (ABS(dev) >= thr_warn)
        m_bms_valerts[i] = 1;
      }
    // publish to metrics:
    StandardMetrics.ms_v_bat_pack_vmin->SetValue(min);
    StandardMetrics.ms_v_bat_pack_vmax->SetValue(max);
    StandardMetrics.ms_v_bat_pack_vavg->SetValue(avg);
    StandardMetrics.ms_v_bat_pack_vstddev->SetValue(stddev);
    if (stddev > StandardMetrics.ms_v_bat_pack_vstddev_max->AsFloat())
      StandardMetrics.ms_v_bat_pack_vstddev_max->SetValue(stddev);
    StandardMetrics.ms_v_bat_cell_voltage->SetElemValues(0, m_bms_readings_v, m_bms_voltages);
    StandardMetrics.ms_v_bat_cell_vmin->SetElemValues(0, m_bms_readings_v, m_bms_vmins);
    StandardMetrics.ms_v_bat_cell_vmax->SetElemValues(0, m_bms_readings_v, m_bms_vmaxs);
    StandardMetrics.ms_v_bat_cell_vdevmax->SetElemValues(0, m_bms_readings_v, m_bms_vdevmaxs);
    StandardMetrics.ms_v_bat_cell_valert->SetElemValues(0, m_bms_readings_v, m_bms_valerts);
    // complete:
    m_bms_has_voltages = true;
    m_bms_bitset_v.clear();
    m_bms_bitset_v.resize(m_bms_readings_v);
    m_bms_bitset_cv = 0;
    }
  else
    {
    m_bms_bitset_v[index] = true;
    }
  }

void OvmsVehicle::BmsSetCellTemperature(int index, float value)
  {
  // ESP_LOGI(TAG,"BmsSetCellTemperature(%d,%f) c=%d", index, value, m_bms_bitset_ct);
  if ((index<0)||(index>=m_bms_readings_t)) return;
  if ((value<m_bms_limit_tmin)||(value>m_bms_limit_tmax)) return;
  m_bms_temperatures[index] = value;

  if (! m_bms_has_temperatures)
    {
    m_bms_tmins[index] = value;
    m_bms_tmaxs[index] = value;
    }
  else if (m_bms_tmins[index] > value)
    m_bms_tmins[index] = value;
  else if (m_bms_tmaxs[index] < value)
    m_bms_tmaxs[index] = value;

  if (m_bms_bitset_t[index] == false) m_bms_bitset_ct++;
  if (m_bms_bitset_ct == m_bms_readings_t)
    {
    // get min, max, avg & standard deviation:
    double sum=0, sqrsum=0, stddev=0;
    float min=0, max=0, avg;
    for (int i=0; i<m_bms_readings_t; i++)
      {
      sum += m_bms_temperatures[i];
      sqrsum += SQR(m_bms_temperatures[i]);
      if (min==0 || m_bms_temperatures[i]<min)
        min = m_bms_temperatures[i];
      if (max==0 || m_bms_temperatures[i]>max)
        max = m_bms_temperatures[i];
      }
    avg = ROUNDPREC(sum / m_bms_readings_t, 2);
    stddev = ROUNDPREC(sqrt((sqrsum / m_bms_readings_t) - SQR(avg)), 2);
    // check cell deviations:
    float dev;
    float thr_warn  = MyConfig.GetParamValueFloat("vehicle", "bms.dev.temp.warn", stddev * 1.2);
    float thr_alert = MyConfig.GetParamValueFloat("vehicle", "bms.dev.temp.alert", stddev * 2.0);
    for (int i=0; i<m_bms_readings_t; i++)
      {
      dev = ROUNDPREC(m_bms_temperatures[i] - avg, 2);
      if (ABS(dev) > ABS(m_bms_tdevmaxs[i]))
        m_bms_tdevmaxs[i] = dev;
      if (ABS(dev) >= thr_alert && m_bms_talerts[i] != 2)
        {
        m_bms_talerts[i] = 2;
        m_bms_talerts_new++; // trigger notification
        }
      else if (ABS(dev) >= thr_warn)
        m_bms_talerts[i] = 1;
      }
    // publish to metrics:
    StandardMetrics.ms_v_bat_pack_tmin->SetValue(min);
    StandardMetrics.ms_v_bat_pack_tmax->SetValue(max);
    StandardMetrics.ms_v_bat_pack_tavg->SetValue(avg);
    StandardMetrics.ms_v_bat_pack_tstddev->SetValue(stddev);
    if (stddev > StandardMetrics.ms_v_bat_pack_tstddev_max->AsFloat())
      StandardMetrics.ms_v_bat_pack_tstddev_max->SetValue(stddev);
    StandardMetrics.ms_v_bat_cell_temp->SetElemValues(0, m_bms_readings_t, m_bms_temperatures);
    StandardMetrics.ms_v_bat_cell_tmin->SetElemValues(0, m_bms_readings_t, m_bms_tmins);
    StandardMetrics.ms_v_bat_cell_tmax->SetElemValues(0, m_bms_readings_t, m_bms_tmaxs);
    StandardMetrics.ms_v_bat_cell_tdevmax->SetElemValues(0, m_bms_readings_t, m_bms_tdevmaxs);
    StandardMetrics.ms_v_bat_cell_talert->SetElemValues(0, m_bms_readings_t, m_bms_talerts);
    // complete:
    m_bms_has_temperatures = true;
    m_bms_bitset_t.clear();
    m_bms_bitset_t.resize(m_bms_readings_t);
    m_bms_bitset_ct = 0;
    }
  else
    {
    m_bms_bitset_t[index] = true;
    }
  }

void OvmsVehicle::BmsRestartCellVoltages()
  {
  m_bms_bitset_v.clear();
  m_bms_bitset_v.resize(m_bms_readings_v);
  m_bms_bitset_cv = 0;
  }

void OvmsVehicle::BmsRestartCellTemperatures()
  {
  m_bms_bitset_t.clear();
  m_bms_bitset_v.resize(m_bms_readings_t);
  m_bms_bitset_ct = 0;
  }

void OvmsVehicle::BmsResetCellVoltages()
  {
  if (m_bms_readings_v > 0)
    {
    m_bms_bitset_v.clear();
    m_bms_bitset_v.resize(m_bms_readings_v);
    m_bms_bitset_cv = 0;
    m_bms_has_voltages = false;
    for (int k=0; k<m_bms_readings_v; k++)
      {
      m_bms_vmins[k] = 0;
      m_bms_vmaxs[k] = 0;
      m_bms_vdevmaxs[k] = 0;
      m_bms_valerts[k] = 0;
      }
    m_bms_valerts_new = 0;
    StandardMetrics.ms_v_bat_cell_vmin->ClearValue();
    StandardMetrics.ms_v_bat_cell_vmax->ClearValue();
    StandardMetrics.ms_v_bat_cell_vdevmax->ClearValue();
    StandardMetrics.ms_v_bat_cell_valert->ClearValue();
    }
  }

void OvmsVehicle::BmsResetCellTemperatures()
  {
  if (m_bms_readings_t > 0)
    {
    m_bms_bitset_t.clear();
    m_bms_bitset_t.resize(m_bms_readings_t);
    m_bms_bitset_ct = 0;
    m_bms_has_temperatures = false;
    for (int k=0; k<m_bms_readings_t; k++)
      {
      m_bms_tmins[k] = 0;
      m_bms_tmaxs[k] = 0;
      m_bms_tdevmaxs[k] = 0;
      m_bms_talerts[k] = 0;
      }
    m_bms_talerts_new = 0;
    StandardMetrics.ms_v_bat_cell_tmin->ClearValue();
    StandardMetrics.ms_v_bat_cell_tmax->ClearValue();
    StandardMetrics.ms_v_bat_cell_tdevmax->ClearValue();
    StandardMetrics.ms_v_bat_cell_talert->ClearValue();
    }
  }

void OvmsVehicle::BmsResetCellStats()
  {
  BmsResetCellVoltages();
  BmsResetCellTemperatures();
  }

void OvmsVehicle::BmsStatus(int verbosity, OvmsWriter* writer)
  {
  int c;

  if ((! m_bms_has_voltages)||(! m_bms_has_temperatures))
    {
    writer->puts("No BMS status data available");
    return;
    }

  writer->puts("Vehicle BMS Status\n");

  int kv = 0;
  int kt = 0;
  for (int module = 0; module < (m_bms_readings_v/m_bms_readingspermodule_v); module++)
    {
    writer->printf("    +");
    for (c=0;c<m_bms_readingspermodule_v;c++) { writer->printf("-------"); }
    writer->printf("-+");
    for (c=0;c<m_bms_readingspermodule_t;c++) { writer->printf("-------"); }
    writer->puts("-+");
    writer->printf("%3d |",module+1);
    for (c=0; c<m_bms_readingspermodule_v; c++)
      {
      writer->printf(" %5.3fV",m_bms_voltages[kv++]);
      }
    writer->printf(" |");
    for (c=0; c<m_bms_readingspermodule_t; c++)
      {
      writer->printf(" %5.1fC",m_bms_temperatures[kt++]);
      }
    writer->puts(" |");
    }

  writer->printf("    +");
  for (c=0;c<m_bms_readingspermodule_v;c++) { writer->printf("-------"); }
  writer->printf("-+");
  for (c=0;c<m_bms_readingspermodule_t;c++) { writer->printf("-------"); }
  writer->puts("-+");
  }

bool OvmsVehicle::FormatBmsAlerts(int verbosity, OvmsWriter* writer, bool show_warnings)
  {
  // Voltages:
  writer->printf("Voltage: SD=%dmV", (int)(StdMetrics.ms_v_bat_pack_vstddev_max->AsFloat() * 1000));
  bool has_valerts = false;
  for (int i=0; i<m_bms_readings_v; i++)
    {
    int sts = StdMetrics.ms_v_bat_cell_valert->GetElemValue(i);
    if (sts == 0) continue;
    if (sts == 1 && !show_warnings) continue;
    int dev = StdMetrics.ms_v_bat_cell_vdevmax->GetElemValue(i) * 1000;
    writer->printf(" %c%d:%+dmV", (sts==1) ? '?' : '!', i+1, dev);
    has_valerts = true;
    }
  writer->printf("%s\n", has_valerts ? "" : " OK");

  // Temperatures:
  // (Note: '°' is not SMS safe, so we only output 'C')
  writer->printf("Temperature: SD=%.1fC", StdMetrics.ms_v_bat_pack_tstddev_max->AsFloat());
  bool has_talerts = false;
  for (int i=0; i<m_bms_readings_v; i++)
    {
    int sts = StdMetrics.ms_v_bat_cell_talert->GetElemValue(i);
    if (sts == 0) continue;
    if (sts == 1 && !show_warnings) continue;
    float dev = StdMetrics.ms_v_bat_cell_tdevmax->GetElemValue(i);
    writer->printf(" %c%d:%+.1fC", (sts==1) ? '?' : '!', i+1, dev);
    has_talerts = true;
    }
  writer->printf("%s\n", has_talerts ? "" : " OK");

  return has_valerts || has_talerts;
  }

#ifdef CONFIG_OVMS_COMP_WEBSERVER
/**
 * GetDashboardConfig: template / default configuration
 *  (override with vehicle specific configuration)
 *  see https://api.highcharts.com/highcharts/yAxis for details on options
 */
void OvmsVehicle::GetDashboardConfig(DashboardConfig& cfg)
  {
  cfg.gaugeset1 =
    "yAxis: [{"
      // Speed:
      "min: 0, max: 200,"
      "plotBands: ["
        "{ from: 0, to: 120, className: 'green-band' },"
        "{ from: 120, to: 160, className: 'yellow-band' },"
        "{ from: 160, to: 200, className: 'red-band' }]"
    "},{"
      // Voltage:
      "min: 310, max: 410,"
      "plotBands: ["
        "{ from: 310, to: 325, className: 'red-band' },"
        "{ from: 325, to: 340, className: 'yellow-band' },"
        "{ from: 340, to: 410, className: 'green-band' }]"
    "},{"
      // SOC:
      "min: 0, max: 100,"
      "plotBands: ["
        "{ from: 0, to: 12.5, className: 'red-band' },"
        "{ from: 12.5, to: 25, className: 'yellow-band' },"
        "{ from: 25, to: 100, className: 'green-band' }]"
    "},{"
      // Efficiency:
      "min: 0, max: 400,"
      "plotBands: ["
        "{ from: 0, to: 200, className: 'green-band' },"
        "{ from: 200, to: 300, className: 'yellow-band' },"
        "{ from: 300, to: 400, className: 'red-band' }]"
    "},{"
      // Power:
      "min: -50, max: 200,"
      "plotBands: ["
        "{ from: -50, to: 0, className: 'violet-band' },"
        "{ from: 0, to: 100, className: 'green-band' },"
        "{ from: 100, to: 150, className: 'yellow-band' },"
        "{ from: 150, to: 200, className: 'red-band' }]"
    "},{"
      // Charger temperature:
      "min: 20, max: 80, tickInterval: 20,"
      "plotBands: ["
        "{ from: 20, to: 65, className: 'normal-band border' },"
        "{ from: 65, to: 80, className: 'red-band border' }]"
    "},{"
      // Battery temperature:
      "min: -15, max: 65, tickInterval: 25,"
      "plotBands: ["
        "{ from: -15, to: 0, className: 'red-band border' },"
        "{ from: 0, to: 50, className: 'normal-band border' },"
        "{ from: 50, to: 65, className: 'red-band border' }]"
    "},{"
      // Inverter temperature:
      "min: 20, max: 80, tickInterval: 20,"
      "plotBands: ["
        "{ from: 20, to: 70, className: 'normal-band border' },"
        "{ from: 70, to: 80, className: 'red-band border' }]"
    "},{"
      // Motor temperature:
      "min: 50, max: 125, tickInterval: 25,"
      "plotBands: ["
        "{ from: 50, to: 110, className: 'normal-band border' },"
        "{ from: 110, to: 125, className: 'red-band border' }]"
    "}]";
  }
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
