/*
;    Project:       Open Vehicle Monitor System
;    Date:          5th July 2018
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
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
static const char *TAG = "v-mitsubishi";

#include <stdio.h>
#include "vehicle_mitsubishi.h"

static unsigned int mi_candata_timer;
static unsigned int mi_est_range;
static unsigned int mi_stale_charge;
static unsigned char mi_QC;         // Quick charge status
static unsigned char mi_QC_counter; // Counter to filter false QC messages (0xff)
static signed char mi_batttemps[24]; // First two temps from the 0x6e1 messages (24 of the 64 values)
static unsigned char mi_last_good_SOC;    // The last known good SOC
static unsigned char mi_last_good_range;  // The last known good range

OvmsVehicleMitsubishi::OvmsVehicleMitsubishi()
  {
  ESP_LOGI(TAG, "Start Mitsubishi iMiev vehicle module");

  mi_candata_timer = 0;
  mi_est_range = 0;
  mi_stale_charge = 0;
  mi_QC = 0;
  mi_QC_counter = 5;
  mi_last_good_SOC = 0;
  mi_last_good_range = 0;
  memset(m_vin,0,sizeof(m_vin));
  memset(mi_batttemps,0,sizeof(mi_batttemps));

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

  // require GPS:
  MyEvents.SignalEvent("vehicle.require.gps", NULL);
  MyEvents.SignalEvent("vehicle.require.gpstime", NULL);
  }

OvmsVehicleMitsubishi::~OvmsVehicleMitsubishi()
  {
  ESP_LOGI(TAG, "Stop Mitsubishi iMiev vehicle module");
  }

void OvmsVehicleMitsubishi::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  mi_candata_timer = 60;  // Reset the timer

  switch (p_frame->MsgID)
    {
    case 0x346:
      {
      mi_est_range = d[7];
      if ((mi_QC != 0) && (mi_est_range == 255))
        {
        mi_stale_charge = 30; // Reset stale charging indicator
        }
      break;
      }
    case 0x374:
      {
      StandardMetrics.ms_v_bat_soc->SetValue(((int)d[1]-10)/2);
      break;
      }
    case 0x389:
      {
      StandardMetrics.ms_v_charge_voltage->SetValue(d[1]);
      StandardMetrics.ms_v_charge_current->SetValue(d[6]/10);
      mi_stale_charge = 30; // Reset stale charging indicator
      break;
      }
    case 0x412:  // Speed and odometer
      {
      if (d[1]>200)
        StandardMetrics.ms_v_pos_speed->SetValue((int)d[1]-255,Kph);
      else
        StandardMetrics.ms_v_pos_speed->SetValue(d[1]);
      StandardMetrics.ms_v_pos_odometer->SetValue(
        ((int)d[2]<<16)+((int)d[3]<<8)+d[4],
        Kilometers);
      break;
      }
    case 0x285: // Handbrake
      {
      if (d[6] == 0x0c)
        {
        // Car is in park
        StandardMetrics.ms_v_env_handbrake->SetValue(true);
        }
      else if (d[6] == 0x0e)
        {
        // Car is not in park
        StandardMetrics.ms_v_env_handbrake->SetValue(false);
        }
      }
    case 0x286: // Charger temperature
      {
      StandardMetrics.ms_v_charge_temp->SetValue((int)d[3]-40);
      StandardMetrics.ms_v_inv_temp->SetValue((int)d[3]-40);
      break;
      }
    case 0x298: // Motor temperature
      {
      StandardMetrics.ms_v_mot_temp->SetValue((int)d[3]-40);
      break;
      }
    case 0x29A: // VIN
      {
      if (d[0]==0)
        for (int k=0;k<7;k++) m_vin[k] = d[k+1];
      else if (d[0]==1)
        for (int k=0;k<7;k++) m_vin[k+7] = d[k+1];
      else if (d[0]==2)
        {
        m_vin[14] = d[1];
        m_vin[15] = d[2];
        m_vin[16] = d[3];
        m_vin[17] = 0;
        if ((m_vin[0] != 0)&&(m_vin[7] != 0))
          {
          StandardMetrics.ms_v_vin->SetValue(m_vin);
          }
        }
      break;
      }
    case 0x6e1: // Battery temperatures
      {
      // Calculate average battery pack temperature based on 24 of the 64 temperature values
      // Message 0x6e1 carries two temperatures for each of 12 banks, bank number (1..12) in byte 0,
      // temperatures in bytes 2 and 3, offset by 50C
      if ((d[0]>=1)&&(d[0]<=12))
        {
        int idx = ((int)d[0]<<1)-2;
        mi_batttemps[idx] = (signed char)d[2] - 50;
        mi_batttemps[idx+1] = (signed char)d[3] - 50;
        }
      break;
      }
    default:
      break;
      }
  }

void OvmsVehicleMitsubishi::Ticker1(uint32_t ticker)
  {
  if (mi_stale_charge > 0)
    {
    if (--mi_stale_charge == 0) // Charge stale
      {
      mi_QC = 0;
      StandardMetrics.ms_v_charge_voltage->SetValue(0);
      StandardMetrics.ms_v_charge_current->SetValue(0);
      }
    }

  if (mi_candata_timer > 0)
    {
    if (--mi_candata_timer == 0)
      {
      // Car has gone to sleep
      StandardMetrics.ms_v_env_awake->SetValue(false);
      StandardMetrics.ms_v_env_on->SetValue(false);
      }
    else
      {
      // Car is awake
      StandardMetrics.ms_v_env_awake->SetValue(true);
      StandardMetrics.ms_v_env_on->SetValue(true);
      }
    }

  ////////////////////////////////////////////////////////////////////////
  // Quick/rapid charge detection
  ////////////////////////////////////////////////////////////////////////
  // Range of 255 is returned during rapid/quick charge
  // We can use this to indicate charge rate, but we will need to
  // calculate a new range based on the last seen SOC and estrange
  // A reported estimated range of 255 is a probable signal that we are rapid charging.
  // However, this has been seen to be reported during turn-on initialisation and
  // it may be read in error at times too. So, we only accept it if we have seen it
  // for 5 consecutive 1-second ticks. Then mi_QC_counter == 0 becomes an indicator
  // of the rapid charge state
  if (mi_est_range == 255)
    {
    if (mi_QC_counter > 0)
      {
      if (--mi_QC_counter == 0) mi_QC=1;
      }
    }
  else
    {
    if (mi_QC_counter < 5) mi_QC_counter++;
    if (mi_QC_counter == 5) mi_QC = 0;
    }

  ////////////////////////////////////////////////////////////////////////
  // Range update
  ////////////////////////////////////////////////////////////////////////
  // Range of 255 is returned during rapid/quick charge
  // We can use this to indicate charge rate, but we will need to
  // calculate a new range based on the last seen SOC and estrange
  if (mi_QC != 0) // Quick Charging
    {
    if (StandardMetrics.ms_v_bat_soc->AsInt() <= 10)
      StandardMetrics.ms_v_bat_range_est->SetValue(0);
    else
      {
      // Simple range stimation during quick/rapid charge based on
      // last seen decent values of SOC and estrange and assuming that
      // estrange hits 0 at 10% SOC and is linear from there to 100%

      // If the last known SOC was too low for accurate guesstimating,
      // use fundge-figures giving 72 mile (116 km)range as it is probably best
      // to guess low-ish here (but not silly low)
      if (StandardMetrics.ms_v_bat_soc->AsInt() < 20)
        {
        mi_last_good_SOC = 20;
        mi_last_good_range = 8;
        }
      StandardMetrics.ms_v_bat_range_est->SetValue(
        ((int)mi_last_good_range * (int)(StandardMetrics.ms_v_bat_soc->AsInt()-10))
        /(int)(mi_last_good_SOC - 10));
      }
    }
  else
    {
    // Not quick charging
    if (mi_est_range != 255)
      {
      StandardMetrics.ms_v_bat_range_est->SetValue(mi_est_range);
      if ((StandardMetrics.ms_v_bat_soc->AsInt() >= 20)&&
          (StandardMetrics.ms_v_bat_range_est->AsInt() >= 5))
        {
        // Save last good value
        mi_last_good_SOC = StandardMetrics.ms_v_bat_soc->AsInt();
        mi_last_good_range = StandardMetrics.ms_v_bat_range_est->AsInt();
        }
      }
    }
  // Calculate ideal range
  if (StandardMetrics.ms_v_bat_soc->AsInt() <= 10)
    {
    StandardMetrics.ms_v_bat_range_ideal->SetValue(0);
    }
  else
    {
    // Ideal range 93 miles (150km)
    StandardMetrics.ms_v_bat_range_ideal->SetValue(
      (StandardMetrics.ms_v_bat_soc->AsInt()/100)*150);
    }

  ////////////////////////////////////////////////////////////////////////
  // Charge state determination
  ////////////////////////////////////////////////////////////////////////
  if ((mi_QC != 0) ||
      ((StandardMetrics.ms_v_charge_current->AsInt() != 0)&&
       (StandardMetrics.ms_v_charge_voltage->AsInt() != 0)))
    {
    StandardMetrics.ms_v_env_charging12v->SetValue(true);
    if (! StandardMetrics.ms_v_charge_pilot->AsBool())
      {
      // Charge has started
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_charge_pilot->SetValue(true);
      StandardMetrics.ms_v_door_chargeport->SetValue(true);
      StandardMetrics.ms_v_charge_mode->SetValue("standard");
      StandardMetrics.ms_v_charge_state->SetValue("charging");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      if (mi_QC != 0)
        StandardMetrics.ms_v_charge_climit->SetValue(125);
      else
        StandardMetrics.ms_v_charge_climit->SetValue(16);
      }
    else
      {
      // Charge is ongoing...
      }
    }
  else if ((mi_QC == 0) &&
        ((StandardMetrics.ms_v_charge_current->AsInt() == 0)&&
         (StandardMetrics.ms_v_charge_voltage->AsInt() < 100)))
    {
    // Car is not charging
    if (StandardMetrics.ms_v_charge_pilot->AsBool())
      {
      // Charge has completed / stopped
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_pilot->SetValue(false);
      StandardMetrics.ms_v_door_chargeport->SetValue(false);
      StandardMetrics.ms_v_env_charging12v->SetValue(false);
      if (StandardMetrics.ms_v_bat_soc->AsInt() < 95)
        {
        // Assume charge was interrupted
        StandardMetrics.ms_v_charge_state->SetValue("stopped");
        StandardMetrics.ms_v_charge_substate->SetValue("stopped");
        }
      else
        {
        StandardMetrics.ms_v_charge_state->SetValue("done");
        StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        }
      }
    }
  }

void OvmsVehicleMitsubishi::Ticker10(uint32_t ticker)
  {
  int tbattery = 0;
  for (int k=0;k<24;k++) tbattery += mi_batttemps[k];
  StandardMetrics.ms_v_bat_temp->SetValue(tbattery/24);

  StandardMetrics.ms_v_env_cooling->SetValue(!StandardMetrics.ms_v_bat_temp->IsStale());
  }

class OvmsVehicleMitsubishiInit
  {
  public: OvmsVehicleMitsubishiInit();
} OvmsVehicleMitsubishiInit  __attribute__ ((init_priority (9000)));

OvmsVehicleMitsubishiInit::OvmsVehicleMitsubishiInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: MITSUBISHI IMIEV (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleMitsubishi>("MI","Mitsubishi iMiev");
  }
