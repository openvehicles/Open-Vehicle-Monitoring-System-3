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
static const char *TAG = "v-dbc";

#include "vehicle_dbc.h"
#include "dbc_app.h"

OvmsVehicleDBC::OvmsVehicleDBC()
  {
  }

OvmsVehicleDBC::~OvmsVehicleDBC()
  {
  if (m_can1) m_can1->DetachDBC();
  if (m_can2) m_can2->DetachDBC();
  if (m_can3) m_can3->DetachDBC();
  }

bool OvmsVehicleDBC::RegisterCanBusDBC(int bus, CAN_mode_t mode, const char* name, const char* dbc)
  {
  dbcfile *ndbc = MyDBC.LoadString(name, dbc);
  if (ndbc == NULL) return false;

  OvmsVehicle::RegisterCanBus(bus, mode, (CAN_speed_t)(ndbc->m_bittiming.GetBaudRate()/1000),ndbc);
  return true;
  }

bool OvmsVehicleDBC::RegisterCanBusDBCLoaded(int bus, CAN_mode_t mode, const char* dbcloaded)
  {
  dbcfile* ndbc = MyDBC.Find(dbcloaded);
  if (ndbc==NULL) return false;
  OvmsVehicle::RegisterCanBus(bus, mode, (CAN_speed_t)(ndbc->m_bittiming.GetBaudRate()/1000),ndbc);
  return true;
  }

void OvmsVehicleDBC::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  // This should be called from the IncomingFrameCan1 handler of the derived vehicle class
  IncomingFrame(m_can1, p_frame);
  }

void OvmsVehicleDBC::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  // This should be called from the IncomingFrameCan1 handler of the derived vehicle class
  IncomingFrame(m_can2, p_frame);
  }

void OvmsVehicleDBC::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  // This should be called from the IncomingFrameCan1 handler of the derived vehicle class
  IncomingFrame(m_can3, p_frame);
  }

void OvmsVehicleDBC::IncomingFrame(canbus* bus, CAN_frame_t* frame)
  {
  dbcfile* dbc = bus->GetDBC();
  if (dbc==NULL) return;

  dbcMessage* msg = dbc->m_messages.FindMessage(frame->FIR.B.FF, frame->MsgID);
  if (msg)
    {
    dbcSignal* mux = msg->GetMultiplexorSignal();
    uint32_t muxval;
    if (mux)
      {
      dbcNumber r = mux->Decode(frame);
      muxval = r.GetSignedInteger();
      }
    for (dbcSignal* sig : msg->m_signals)
      {
      OvmsMetric* m = sig->GetMetric();
      if (m)
        {
        if ((mux==NULL)||(sig->GetMultiplexSwitchvalue() == muxval))
          {
          dbcNumber r = sig->Decode(frame);
          m->SetValue(r);
          }
        }
      }
    }
  }

OvmsVehiclePureDBC::OvmsVehiclePureDBC()
  {
  ESP_LOGI(TAG, "Pure DBC vehicle module");

  std::string dbctype;

  dbctype = MyConfig.GetParamValue("vehicle", "dbc.can1");
  if (dbctype.length()>0)
    {
    ESP_LOGI(TAG,"Registering can bus #1 as DBC %s",dbctype.c_str());
    RegisterCanBusDBCLoaded(1, CAN_MODE_LISTEN, dbctype.c_str());
    }

  dbctype = MyConfig.GetParamValue("vehicle", "dbc.can2");
  if (dbctype.length()>0)
    {
    ESP_LOGI(TAG,"Registering can bus #2 as DBC %s",dbctype.c_str());
    RegisterCanBusDBCLoaded(2, CAN_MODE_LISTEN, dbctype.c_str());
    }

  dbctype = MyConfig.GetParamValue("vehicle", "dbc.can3");
  if (dbctype.length()>0)
    {
    ESP_LOGI(TAG,"Registering can bus #3 as DBC %s",dbctype.c_str());
    RegisterCanBusDBCLoaded(3, CAN_MODE_LISTEN, dbctype.c_str());
    }
  }

OvmsVehiclePureDBC::~OvmsVehiclePureDBC()
  {
  ESP_LOGI(TAG, "Shutdown Pure DBC vehicle module");
  }

void OvmsVehiclePureDBC::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  OvmsVehicleDBC::IncomingFrameCan1(p_frame);
  }

void OvmsVehiclePureDBC::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  OvmsVehicleDBC::IncomingFrameCan2(p_frame);
  }

void OvmsVehiclePureDBC::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  OvmsVehicleDBC::IncomingFrameCan3(p_frame);
  }

class OvmsVehiclePureDBCInit
  {
  public: OvmsVehiclePureDBCInit();
  } MyOvmsVehiclePureDBCInit  __attribute__ ((init_priority (9000)));

OvmsVehiclePureDBCInit::OvmsVehiclePureDBCInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: DBC based vehicle (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehiclePureDBC>("DBC","DBC based vehicle");
  }
