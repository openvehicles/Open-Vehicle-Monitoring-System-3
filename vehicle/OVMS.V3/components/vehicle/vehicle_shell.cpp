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
// static const char *TAG = "vehicle";

#include <stdio.h>
#include <algorithm>
#include <ovms_command.h>
#include <ovms_script.h>
#include <ovms_metrics.h>
#include <ovms_notify.h>
#include <metrics_standard.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_webserver.h>
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_peripherals.h>
#include <string_writer.h>
#include "vehicle.h"


int OvmsVehicleFactory::vehicle_validate(OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv, bool complete)
  {
  if (argc == 1)
    return MyVehicleFactory.m_vmap.Validate(writer, argc, argv[0], complete);
  return -1;
  }

void OvmsVehicleFactory::vehicle_module(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (argc == 0)
    {
    MyVehicleFactory.ClearVehicle();
    }
  else
    {
    MyVehicleFactory.SetVehicle(argv[0]);
    }
  // output new status:
  if (MyVehicleFactory.m_currentvehicle != NULL)
    {
    MyVehicleFactory.m_currentvehicle->Status(verbosity, writer);
    }
  else
    {
    writer->puts("No vehicle module selected");
    }
  }

void OvmsVehicleFactory::vehicle_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Type            Name");
  for (OvmsVehicleFactory::map_vehicle_t::iterator k=MyVehicleFactory.m_vmap.begin(); k!=MyVehicleFactory.m_vmap.end(); ++k)
    {
    writer->printf("%-15.15s %s\n",k->first,k->second.name);
    }
  }

void OvmsVehicleFactory::vehicle_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::vehicle_wakeup(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::vehicle_homelink(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::vehicle_climatecontrol(int verbosity, OvmsWriter* writer, bool on)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  switch(MyVehicleFactory.m_currentvehicle->CommandClimateControl(on))
    {
    case OvmsVehicle::Success:
      writer->puts("Climate Control ");
      writer->puts(on ? "on" : "off");
      break;
    case OvmsVehicle::Fail:
      writer->puts("Error: Climate Control failed");
      break;
    default:
      writer->puts("Error: Climate Control functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_climatecontrol_on(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  vehicle_climatecontrol(verbosity, writer, true);
  }

void OvmsVehicleFactory::vehicle_climatecontrol_off(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  vehicle_climatecontrol(verbosity, writer, false);
  }

void OvmsVehicleFactory::vehicle_lock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::vehicle_unlock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::vehicle_valet(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::vehicle_unvalet(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::vehicle_charge_mode(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::vehicle_charge_current(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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
      writer->printf("Error: Could not set charge current limit to %dA\n",limit);
      break;
    default:
      writer->puts("Error: Charge current limit functionality not available");
      break;
    }
  }

void OvmsVehicleFactory::vehicle_charge_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::vehicle_charge_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::vehicle_charge_cooldown(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::vehicle_stat(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::bms_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::bms_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

void OvmsVehicleFactory::bms_alerts(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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


void OvmsVehicleFactory::obdii_request(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyVehicleFactory.m_currentvehicle)
    {
    writer->puts("ERROR: no vehicle module selected\nHint: use module 'NONE' (empty vehicle)");
    return;
    }

  const char* busname = cmd->GetParent()->GetParent()->GetName();
  canbus* bus = (canbus*) MyPcpApp.FindDeviceByName(busname);
  if (!bus)
    {
    writer->printf("ERROR: CAN bus %s not available\n", busname);
    return;
    }
  else if (bus->m_mode != CAN_MODE_ACTIVE)
    {
    writer->printf("ERROR: CAN bus %s not in active mode\n", busname);
    return;
    }

  uint32_t txid = 0, rxid = 0;
  uint8_t protocol = ISOTP_STD;
  int timeout_ms = 3000;
  std::string request;
  std::string response;

  // parse args:
  const char* target = cmd->GetName();
  if (strcmp(target, "device") == 0)
    {
    // device: [-e] [-t<timeout_ms>] txid rxid request
    int argpos = 0;
    for (int i = 0; i < argc; i++)
      {
      if (argv[i][0] == '-')
        {
        switch (argv[i][1])
          {
          case 'e':
            protocol = ISOTP_EXTADR;
            break;
          case 't':
            timeout_ms = atoi(argv[i]+2);
            break;
          default:
            writer->printf("ERROR: unknown option '%s'\n", argv[i]);
            return;
          }
        }
      else
        {
        switch (++argpos)
          {
          case 1:
            txid = strtol(argv[i], NULL, 16);
            break;
          case 2:
            rxid = strtol(argv[i], NULL, 16);
            break;
          case 3:
            request = hexdecode(argv[i]);
            break;
          default:
            writer->puts("ERROR: too many args");
            return;
          }
        }
      }
    if (argpos < 3)
      {
      writer->puts("ERROR: too few args, need: txid rxid request");
      return;
      }
    }
  else
    {
    // broadcast: [-t<timeout_ms>] request
    int argpos = 0;
    for (int i = 0; i < argc; i++)
      {
      if (argv[i][0] == '-')
        {
        switch (argv[i][1])
          {
          case 't':
            timeout_ms = atoi(argv[i]+2);
            break;
          default:
            writer->printf("ERROR: unknown option '%s'\n", argv[i]);
            return;
          }
        }
      else
        {
        switch (++argpos)
          {
          case 1:
            request = hexdecode(argv[i]);
            break;
          default:
            writer->puts("ERROR: too many args");
            return;
          }
        }
      }
    if (argpos < 1)
      {
      writer->puts("ERROR: too few args, need: request");
      return;
      }
    txid = 0x7df;
    rxid = 0;
    }

  // validate request:
  if (timeout_ms <= 0)
    {
    writer->puts("ERROR: invalid timeout (must be > 0)");
    return;
    }
  if (request.size() == 0)
    {
    writer->puts("ERROR: no request");
    return;
    }
  else
    {
    uint8_t type = request.at(0);
    if ((POLL_TYPE_HAS_16BIT_PID(type) && request.size() < 3) ||
        (POLL_TYPE_HAS_8BIT_PID(type) && request.size() < 2))
      {
      writer->printf("ERROR: request too short for type %02X\n", type);
      return;
      }
    }

  // execute request:
  int err = MyVehicleFactory.m_currentvehicle->PollSingleRequest(bus, txid, rxid,
    request, response, timeout_ms, protocol);

  writer->printf("%x[%x] %s: ", txid, rxid, hexencode(request).c_str());
  if (err == POLLSINGLE_TXFAILURE)
    {
    writer->puts("ERROR: transmission failure (CAN bus error)");
    return;
    }
  else if (err < 0)
    {
    writer->puts("ERROR: timeout waiting for poller/response");
    return;
    }
  else if (err)
    {
    writer->printf("ERROR: request failed with response error code %02X\n", err);
    return;
    }

  // output response as hex dump:
  writer->puts("Response:");
  char *buf = NULL;
  size_t rlen = response.size(), offset = 0;
  do
    {
    rlen = FormatHexDump(&buf, response.data() + offset, rlen, 16);
    offset += 16;
    writer->puts(buf ? buf : "-");
    }
  while (rlen);

  if (buf)
    free(buf);
  }
