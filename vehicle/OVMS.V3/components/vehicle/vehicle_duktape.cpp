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
#include "vehicle_common.h"

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleType(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle != NULL)
    {
    duk_push_string(ctx, MyVehicleFactory.m_currentvehicletype.c_str());
    return 1;
    }
  else
    {
    return 0;
    }
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleWakeup(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    switch(MyVehicleFactory.m_currentvehicle->CommandWakeup())
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }

  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleHomelink(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    int homelink = duk_to_int(ctx,0);
    int durationms = duk_to_int(ctx,-1);
    if (durationms==0) durationms = 1000;

    if ((homelink<1)||(homelink>3))
      { duk_push_boolean(ctx, 0); }
    else if (durationms < 100)
      { duk_push_boolean(ctx, 0); }
    else
      {
      switch(MyVehicleFactory.m_currentvehicle->CommandHomelink(homelink-1, durationms))
        {
        case OvmsVehicle::Success:
          duk_push_boolean(ctx, 1);
          break;
        default:
          duk_push_boolean(ctx, 0);
          break;
        }
      }
    }

  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleClimateControl(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    bool on = duk_to_boolean(ctx,0);
    switch(MyVehicleFactory.m_currentvehicle->CommandClimateControl(on))
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }
  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleLock(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    const char* pin = duk_safe_to_string(ctx, 0);
    switch(MyVehicleFactory.m_currentvehicle->CommandLock(pin))
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }
  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleUnlock(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    const char* pin = duk_safe_to_string(ctx, 0);
    switch(MyVehicleFactory.m_currentvehicle->CommandUnlock(pin))
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }
  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleValet(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    const char* pin = duk_safe_to_string(ctx, 0);
    switch(MyVehicleFactory.m_currentvehicle->CommandActivateValet(pin))
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }
  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleUnvalet(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    const char* pin = duk_safe_to_string(ctx, 0);
    switch(MyVehicleFactory.m_currentvehicle->CommandDeactivateValet(pin))
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }
  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleSetChargeMode(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    const char* mode = duk_safe_to_string(ctx, 0);
    OvmsVehicle::vehicle_mode_t tmode = (OvmsVehicle::vehicle_mode_t)chargemode_key(mode);
    switch(MyVehicleFactory.m_currentvehicle->CommandSetChargeMode(tmode))
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }
  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleSetChargeCurrent(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    int limit = duk_to_int(ctx,0);
    switch(MyVehicleFactory.m_currentvehicle->CommandSetChargeCurrent(limit))
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }
  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleSetChargeTimer(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    bool timeron = duk_to_boolean(ctx,0);
    int timerstart = duk_to_int(ctx,-1);
    switch(MyVehicleFactory.m_currentvehicle->CommandSetChargeTimer(timeron, timerstart))
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }
  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleStopCharge(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    switch(MyVehicleFactory.m_currentvehicle->CommandStopCharge())
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }
  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleStartCharge(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    switch(MyVehicleFactory.m_currentvehicle->CommandStartCharge())
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }
  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleStartCooldown(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    switch(MyVehicleFactory.m_currentvehicle->CommandCooldown(true))
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }
  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleStopCooldown(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    switch(MyVehicleFactory.m_currentvehicle->CommandCooldown(false))
      {
      case OvmsVehicle::Success:
        duk_push_boolean(ctx, 1);
        break;
      default:
        duk_push_boolean(ctx, 0);
        break;
      }
    }
  return 1;
  }


/**
 * DukOvmsVehicleObdRequest: OBD/UDS PollSingleRequest wrapper (synchronous).
 * 
 * Javascript API:
 *    var obdResult = OvmsVehicle.ObdRequest({
 *      txid: <int>,
 *      rxid: <int>,
 *      [bus: <string>,]    // default: "can1"
 *      [timeout: <int>,]   // in ms, default: 3000
 *      [protocol: <int>,]  // default: 0 = ISOTP_STD, see vehicle_common.h
 *      [pid: <int>,]       // PID (otherwise embedded in request with type)
 *      [type: <int>,]      // Package type (defaults to READDATA type 0x22 if pid specified)
 *      [request: <string>|<Uint8Array>,]  // hex encoded string or binary byte array
 *    });
 * 
 *    obdResult = {
 *      error: <int>,                     // see PollSingleRequest, 0 = success
 *      errordesc: <string>,              // error description / emtpy on success
 *      response: <Uint8Array>,           // only if error = 0: binary response byte array
 *      response_hex: <string>,           // only if error = 0: hex encoded response string
 *    }
 */
duk_ret_t OvmsVehicleFactory::DukOvmsVehicleObdRequest(duk_context *ctx)
  {
  DukContext dc(ctx);
  int error = 0;
  std::string errordesc;
  std::string response;
  if (!MyVehicleFactory.m_currentvehicle)
    {
    error = -1000;
    errordesc = "No vehicle module selected";
    }
  else if (!duk_is_object(ctx, 0))
    {
    error = -1001;
    errordesc = "No request object given";
    }
#ifndef CONFIG_OVMS_COMP_POLLER
  else
    {
    error = -1;
    errordesc = "Polling not implemented";
    }
#else
  else
    {
    uint32_t txid, rxid;
    uint8_t protocol;
    std::string request;
    int timeout;
    uint16_t pid;
    uint16_t type;
    canbus* device;

    // Retrieve parameters
    OvmsPollers::Duk_GetRequestFromObject(ctx, 0, "can1", true, device,
        txid, rxid, protocol, type, pid, request, timeout, error, errordesc);

    // Execute request:
    if (error == 0)
      {
      error = MyVehicleFactory.m_currentvehicle->PollSingleRequest(
        device, txid, rxid, type, pid, request, response, timeout, protocol);

      if (error == POLLSINGLE_TXFAILURE)
        errordesc = "Transmission failure (CAN bus error)";
      else if (error < 0)
        errordesc = "Timeout waiting for poller/response";
      else if (error)
        {
        errordesc = "Request failed with response error code " + int_to_hex((uint8_t)error);
        const char* errname = OvmsPoller::PollResultCodeName(error);
        if (errname)
          {
          errordesc += ' ';
          errordesc += errname;
          }
        }
      }
    }
#endif

  // Push result object:
  duk_idx_t obj_idx = dc.PushObject();
  dc.Push(error);
  dc.PutProp(obj_idx, "error");
  dc.Push(errordesc);
  dc.PutProp(obj_idx, "errordesc");
  if (!error)
    {
    dc.PushBinary(response);
    dc.PutProp(obj_idx, "response");
    dc.Push(hexencode(response));
    dc.PutProp(obj_idx, "response_hex");
    }

  return 1;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleAuxMonEnable(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle==nullptr)
    {
    duk_push_boolean(ctx, 0);
    }
  else
    {
    duk_idx_t numArgs = duk_get_top(ctx);
    if (numArgs == 0)
      MyVehicleFactory.m_currentvehicle->EnableAuxMonitor();
    else
      {
      duk_double_t low_thresh, charge_thresh = 0;
      low_thresh = duk_to_number(ctx, 0);
      if (numArgs > 1)
        charge_thresh = duk_to_number(ctx, 1);
      MyVehicleFactory.m_currentvehicle->EnableAuxMonitor(low_thresh, charge_thresh);
      }
    duk_push_boolean(ctx, 1);
    }
  return 1;
  }
duk_ret_t OvmsVehicleFactory::DukOvmsVehicleAuxMonDisable(duk_context *ctx)
  {
  if (MyVehicleFactory.m_currentvehicle!=nullptr)
    {
    MyVehicleFactory.m_currentvehicle->DisableAuxMonitor();
    }
  return 0;
  }

duk_ret_t OvmsVehicleFactory::DukOvmsVehicleAuxMonStatus(duk_context *ctx)
  {
  OvmsVehicle *mycar = MyVehicleFactory.ActiveVehicle();
  if (mycar == nullptr)
    {
    duk_push_null(ctx);
    return 1;
    }
  DukContext dc(ctx);
  // Push result object:
  duk_idx_t obj_idx = dc.PushObject();
  dc.Push(mycar->m_aux_enabled);
  dc.PutProp(obj_idx, "enabled");
  dc.Push(mycar->m_aux_battery_mon.low_threshold());
  dc.PutProp(obj_idx, "low_threshold");
  dc.Push(mycar->m_aux_battery_mon.charge_threshold());
  dc.PutProp(obj_idx, "charge_threshold");
  dc.Push(mycar->m_aux_battery_mon.average_lastf());
  dc.PutProp(obj_idx, "short_avg");
  dc.Push(mycar->m_aux_battery_mon.average_long());
  dc.PutProp(obj_idx, "long_avg");
  dc.Push(OvmsBatteryMon::state_code(mycar->m_aux_battery_mon.state()));
  dc.PutProp(obj_idx, "state");
  return 1;
  }
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
