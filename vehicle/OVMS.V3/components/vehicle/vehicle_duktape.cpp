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
 * DukOvmsVehicleObdRequest: OBD/UDS PollSingleRequest wrapper (synchronous)
 * 
 * Javascript API:
 *    var obdResult = OvmsVehicle.ObdRequest({
 *      txid: <int>,
 *      rxid: <int>,
 *      request: <string>|<Uint8Array>,   // hex encoded string or binary byte array
 *      [bus: <string>,]                  // default: "can1"
 *      [timeout: <int>,]                 // in ms, default: 3000
 *      [protocol: <int>,]                // default: 0 = ISOTP_STD, see vehicle.h
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
  std::string bus = "can1";
  uint32_t txid = 0, rxid = 0;
  std::string request, response;
  int timeout = 3000;
  uint8_t protocol = ISOTP_STD;

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
  else
    {
    // Read arguments:
    if (duk_get_prop_string(ctx, 0, "bus"))
      bus = duk_to_string(ctx, -1);
    duk_pop(ctx);
    if (duk_get_prop_string(ctx, 0, "timeout"))
      timeout = duk_to_int(ctx, -1);
    duk_pop(ctx);
    if (duk_get_prop_string(ctx, 0, "protocol"))
      protocol = duk_to_int(ctx, -1);
    duk_pop(ctx);
    if (duk_get_prop_string(ctx, 0, "txid"))
      txid = duk_to_int(ctx, -1);
    else
      error = -1002;
    duk_pop(ctx);
    if (duk_get_prop_string(ctx, 0, "rxid"))
      rxid = duk_to_int(ctx, -1);
    else
      error = -1002;
    duk_pop(ctx);
    if (duk_get_prop_string(ctx, 0, "request"))
      {
      if (duk_is_buffer_data(ctx, -1))
        {
        size_t size;
        void *data = duk_get_buffer_data(ctx, -1, &size);
        request.resize(size, '\0');
        memcpy(&request[0], data, size);
        }
      else
        {
        request = hexdecode(duk_to_string(ctx, -1));
        }
      }
    duk_pop(ctx);

    // Validate arguments:
    canbus* device = (canbus*)MyPcpApp.FindDeviceByName(bus.c_str());
    if (error != 0)
      {
      errordesc = "Missing mandatory argument";
      }
    else if (device == NULL || (txid <= 0 || (txid != 0x7df && rxid <= 0) ||
        request.size() == 0) || timeout <= 0)
      {
      error = -1003;
      errordesc = "Invalid argument";
      }

    // Execute request:
    if (error == 0)
      {
      error = MyVehicleFactory.m_currentvehicle->PollSingleRequest(
        device, txid, rxid, request, response, timeout, protocol);

      if (error == POLLSINGLE_TXFAILURE)
        errordesc = "Transmission failure (CAN bus error)";
      else if (error < 0)
        errordesc = "Timeout waiting for poller/response";
      else if (error)
        {
        errordesc = "Request failed with response error code " + int_to_hex((uint8_t)error);
        const char* errname = MyVehicleFactory.m_currentvehicle->PollResultCodeName(error);
        if (errname)
          {
          errordesc += ' ';
          errordesc += errname;
          }
        }
      }
    }

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

#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
