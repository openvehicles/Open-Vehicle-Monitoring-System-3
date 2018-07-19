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
static const char *TAG = "v-thinkcity";

#include <stdio.h>
#include "vehicle_thinkcity.h"

OvmsVehicleThinkCity::OvmsVehicleThinkCity()
  {
  ESP_LOGI(TAG, "Start Think City vehicle module");

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

  // require GPS:
  MyEvents.SignalEvent("vehicle.require.gps", NULL);
  MyEvents.SignalEvent("vehicle.require.gpstime", NULL);
  }

OvmsVehicleThinkCity::~OvmsVehicleThinkCity()
  {
  ESP_LOGI(TAG, "Stop Think City vehicle module");
  }

void OvmsVehicleThinkCity::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x263:
      tc_AC_volt =(unsigned int) d[1];
      tc_AC_current = ((signed int) d[0]) * 0.2;
      StandardMetrics.ms_v_charge_current->SetValue(tc_AC_current);
      StandardMetrics.ms_v_charge_voltage->SetValue(tc_AC_volt);
      StandardMetrics.ms_v_env_temp->SetValue(((signed char) d[2]) * 0.5); // PCU abmbient temp
      StandardMetrics.ms_v_pos_speed->SetValue(((unsigned char) d[5]) / 2);
      break;

    case 0x301:
      {
      tc_pack_current = (((int) d[0] << 8) + d[1]) / 10;
      tc_pack_voltage = (((unsigned int) d[2] << 8) + d[3]) / 10;
      tc_pack_soc = 100.0 - ((((unsigned int)d[4]<<8) + d[5])/10);
      
      StandardMetrics.ms_v_bat_current->SetValue( tc_pack_current );
      StandardMetrics.ms_v_bat_voltage->SetValue( tc_pack_voltage );
      StandardMetrics.ms_v_bat_soc->SetValue(tc_pack_soc);
      StandardMetrics.ms_v_bat_temp->SetValue( (((signed int)d[6]<<8) + d[7])/10 );
      StandardMetrics.ms_v_bat_range_ideal->SetValue(111.958773 * (float)tc_pack_soc / 100);
      StandardMetrics.ms_v_bat_range_est->SetValue(93.205678 * (float)tc_pack_soc / 100);
      break;
      }

    case 0x302:
      tc_bit_generalerr = (d[0] & 0x01);
      tc_bit_isoerr = (d[2] & 0x01);
      tc_pack_mindchgvolt = (((unsigned int) d[4] << 8) + d[5]) / 10;
      tc_pack_maxdchgamps = (((unsigned int) d[6] << 8) + d[7]) / 10;
      break;

    case 0x303:
      tc_pack_maxchgcurr = (((signed int) d[0] << 8) + d[1]) / 10;
      tc_pack_maxchgvolt = (((unsigned int) d[2] << 8) + d[3]) / 10;
      tc_bit_syschgenbl = (d[4] & 0x01);
      tc_bit_regenbrkenbl = (d[4] & 0x02);
      tc_bit_dischgenbl = (d[4] & 0x04);
      tc_bit_fastchgenbl = (d[4] & 0x08);
      tc_bit_dcdcenbl = (d[4] & 0x10);
      tc_bit_mainsacdet = (d[4] & 0x20);
      tc_pack_batteriesavail = d[5];
      tc_pack_rednumbatteries = (d[6] & 0x01);
      tc_bit_epoemerg = (d[6] & 0x08);
      tc_bit_crash = (d[6] & 0x10);
      tc_bit_fanactive = (d[6] & 0x20);
      tc_bit_socgreater102 = (d[6] & 0x40);
      tc_bit_isotestinprog = (d[6] & 0x80);
      tc_bit_chgwaittemp = (d[7] & 0x01);
      break;

    case 0x304:
      tc_sys_voltmaxgen = (((unsigned int) d[0] << 8) + d[1]) / 10;
      tc_bit_eoc = (d[3] & 0x01);
      tc_bit_reacheocplease = (d[3] & 0x02);
      tc_bit_chgwaitttemp2 = (d[3] & 0x04);
      tc_bit_manyfailedcells = (d[3] & 0x08);
      tc_bit_acheatrelaystat = (d[3] & 0x10);
      tc_bit_acheatswitchstat = (d[3] & 0x20);
      tc_pack_temp1 = (((signed int) d[4] << 8) + d[5]) / 10;
      tc_pack_temp2 = (((signed int) d[6] << 8) + d[7]) / 10;
      break;

    case 0x305:
      tc_charger_pwm = (((unsigned int) d[0] << 8) + d[1]) / 10;
      tc_bit_intisoerr = (d[2] & 0x10);
      tc_bit_extisoerr = (d[2] & 0x20);
      tc_bit_chrgen = (d[3] & 0x01);
      tc_bit_ocvmeas = (d[3] & 0x02);
      tc_bit_chgcurr = (d[3] & 0x04);
      tc_bit_chgovervolt = (d[3] & 0x08);
      tc_bit_chgovercurr = (d[3] & 0x10);
      tc_pack_failedcells = ((unsigned int)d[4]<<8) + d[5];
      tc_bit_waitoktmpdisch = (d[6] & 0x40);
      tc_bit_thermalisoerr = (d[6] & 0x20);
      break;

    case 0x311:
      // Charge limit, controlled by the "power charge button", usually 9 or 15A.
      StandardMetrics.ms_v_charge_climit->SetValue(((unsigned char) d[1]) * 0.2);
      break;
    
    default:
      break;
    }
  }

void OvmsVehicleThinkCity::Ticker1(uint32_t ticker)
  {
  }

void OvmsVehicleThinkCity::Ticker10(uint32_t ticker)
  {
    // Check for charging state

    // TODO: Remove - reference to the old v2 hardware
    // car_door1 Bits used: 0-7
    // Function net_prep_stat in net_msg.c uses car_doors1bits.ChargePort to determine SMS-printout
    //

    // 0x04 Chargeport open, bit 2 set
    // 0x08 Pilot signal on, bit 3 set
    // 0x10 Vehicle charging, bit 4 set
    // 0x0C Chargeport open and pilot signal, bits 2,3 set
    // 0x1C Bits 2,3,4 set

    // Charge enable:
    if (tc_bit_chrgen == 1) // Is charge_enable_bit (bit 0) set (turns to 0 during 0CV-measuring at 80% SOC)?
    {
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_charge_state->SetValue("charging");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      StandardMetrics.ms_v_env_charging12v->SetValue(true);
    }

    // Open Circuit Measurement at 80% SOC
    // Detection of charging connector at 80% SOC
    if (tc_bit_ocvmeas == 2)  // Is ocv_meas_in_progress (bit 1) set?
    {
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_charge_state->SetValue("topoff");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
    }

    // Alert when End of Charge occurs
    if ((tc_bit_eoc == 1) && (tc_bit_chrgen == 1)) // Is EOC_bit (bit 0) set?
    {
      StandardMetrics.ms_v_charge_state->SetValue("done");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      //net_req_notification(NET_NOTIFY_CHARGE);
    }

    // Detection of charging connector at End of Charge
    if (tc_bit_eoc == 1) // Is EOC_bit (bit 0) set?
    {
      StandardMetrics.ms_v_charge_state->SetValue("done");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");

      if (tc_AC_volt > 100)  // Is AC line voltage > 100 ?
      {
        // Charge connector connected
        StandardMetrics.ms_v_charge_pilot->SetValue(true);
        StandardMetrics.ms_v_env_charging12v->SetValue(true);
      }
      else
      {
        // Charge connector disconnected
        StandardMetrics.ms_v_charge_pilot->SetValue(false);
        StandardMetrics.ms_v_env_charging12v->SetValue(false);
      }
    }


    // Charging unplugged or power disconnected
    if (tc_AC_volt < 100.0)  // AC line voltage < 100
    {
      // charging connector unplugged
      StandardMetrics.ms_v_charge_pilot->SetValue(false);
    }

    // Charging interrupted (assumed)
    if ((tc_bit_chrgen == 1) 
      && (tc_bit_eoc != 1) 
      && (tc_bit_ocvmeas !=2) 
      && (tc_pack_current == 0) 
      && (tc_pack_soc < 99.0f))
    {
      StandardMetrics.ms_v_charge_state->SetValue("stopped");
      StandardMetrics.ms_v_charge_substate->SetValue("interrupted");
      //net_req_notification(NET_NOTIFY_CHARGE);
    }

    // Detection of 12V charge off
    if ((tc_bit_chrgen == 0) && (tc_bit_dischgenbl == 0)) // Is charge_enable_bit (bit 0) set (turns to 0 during 0CV-measuring at 80% SOC)?
    {
        StandardMetrics.ms_v_env_on->SetValue(false);
        StandardMetrics.ms_v_env_charging12v->SetValue(false);
    }

    // Parking timer
    if (tc_bit_dischgenbl > 0) //Discharge allowed, car is on
    {
      StandardMetrics.ms_v_env_on->SetValue(true);
      StandardMetrics.ms_v_env_charging12v->SetValue(true);

      // TODO: Test that is not needed (handled automatically) and remove
      /*
      if (StandardMetrics.ms_v_env_parktime->getAsInt() != 0)
      {
        // No longer parking
        StandardMetrics.ms_v_env_parktime->SetValue(0);
        //net_req_notification(NET_NOTIFY_ENV);
      }
      */
    }

    if (tc_bit_dischgenbl == 0) //Discharge not allowed, car is off
    {
      StandardMetrics.ms_v_env_on->SetValue(false);
      StandardMetrics.ms_v_env_charging12v->SetValue(false);


      // TODO: Test that is not needed (handled automatically) and remove
      /*
      if (StandardMetrics.ms_v_env_parktime == 0)
      {
        // Record it as 1 second ago, so non zero report
        StandardMetrics.ms_v_env_parktime->SetValue( StandardMetrics.ms_m_timeutc-1); 
        //net_req_notification(NET_NOTIFY_ENV);
      }
      */
    }
  }

OvmsVehicle::vehicle_command_t OvmsVehicleThinkCity::CommandLock(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleThinkCity::CommandUnlock(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleThinkCity::CommandActivateValet(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleThinkCity::CommandDeactivateValet(const char* pin)
  {
  return NotImplemented;
  }

class OvmsVehicleThinkCityInit
  {
  public: OvmsVehicleThinkCityInit();
} OvmsVehicleThinkCityInit  __attribute__ ((init_priority (9000)));

OvmsVehicleThinkCityInit::OvmsVehicleThinkCityInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: THINK CITY (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleThinkCity>("TGTC","Think City");
  }
