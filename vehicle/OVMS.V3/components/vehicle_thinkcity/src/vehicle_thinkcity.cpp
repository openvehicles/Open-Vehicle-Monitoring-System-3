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

#define TC_CANDATA_TIMEOUT 10

static const OvmsVehicle::poll_pid_t obdii_polls[] =
  {
   // 0x753 03 22 49 65 - charger temp
    { 0x753, 0x75B, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4965, {  0, 30, 300 } }, 
   // 0x753 03 22 49 66 - pcu heat sink temp
    { 0x753, 0x75B, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4966, {  0, 30, 300 } }, 
   // 0x753 03 22 49 67 - motor temp
    { 0x753, 0x75B, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4967, {  0, 30, 300 } }, 
   // 0x753 03 22 49 68 - sli batt temp and volts
    { 0x753, 0x75B, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4968, {  0, 30, 300 } }, 
    { 0, 0, 0x00, 0x00, { 0, 0, 0 } }
  };


OvmsVehicleThinkCity::OvmsVehicleThinkCity()
  {
  ESP_LOGI(TAG, "Start Think City vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  PollSetPidList(m_can1, obdii_polls);
  PollSetState(0);
  }

OvmsVehicleThinkCity::~OvmsVehicleThinkCity()
  {
  }

void OvmsVehicleThinkCity::LogValues()
{
  ESP_LOGI(TAG, "tc_pack_voltage %.1f ", 	tc_pack_voltage	);
  ESP_LOGI(TAG, "tc_pack_current	%.1f ", 	tc_pack_current	);
  
  ESP_LOGI(TAG, "tc_pack_maxchgcurr %.1f ", 	tc_pack_maxchgcurr	);
  ESP_LOGI(TAG, "tc_pack_maxchgvolt %.1f ", 	tc_pack_maxchgvolt	);
  ESP_LOGI(TAG, "tc_pack_failedcells %d ", 	tc_pack_failedcells	);
  ESP_LOGI(TAG, "tc_pack_temp1 %.1f ", 	tc_pack_temp1	);
  ESP_LOGI(TAG, "tc_pack_temp2	%.1f ", 	tc_pack_temp2	);
  ESP_LOGI(TAG, "tc_pack_batteriesavail	%d ", 	tc_pack_batteriesavail	);
  ESP_LOGI(TAG, "tc_pack_rednumbatteries	%d ", 	tc_pack_rednumbatteries	);
  ESP_LOGI(TAG, "tc_pack_mindchgvolt	%.1f ", 	tc_pack_mindchgvolt	);
  ESP_LOGI(TAG, "tc_pack_maxdchgamps	%.1f ", 	tc_pack_maxdchgamps	);
  ESP_LOGI(TAG, "tc_charger_temp	%d ", 	tc_charger_temp	);
  ESP_LOGI(TAG, "tc_slibatt_temp	%d ", 	tc_slibatt_temp	);
  ESP_LOGI(TAG, "tc_charger_pwm	%.1f ", 	tc_charger_pwm	);
  ESP_LOGI(TAG, "tc_sys_voltmaxgen	%.1f ", 	tc_sys_voltmaxgen	);
  ESP_LOGI(TAG, "tc_srs_stat	%d ", 	tc_srs_stat	);
  ESP_LOGI(TAG, "tc_srs_nr_err	%d ", 	tc_srs_nr_err	);
  //ESP_LOGI(TAG, "tc_srs_tm	%d ", 	tc_srs_tm	);
  ESP_LOGI(TAG, "tc_heater_count	%d ", 	tc_heater_count	);
  ESP_LOGI(TAG, "tc_AC_volt	%.1f ", 	tc_AC_volt	);
  ESP_LOGI(TAG, "tc_AC_current	%.1f ", 	tc_AC_current	);
  ESP_LOGI(TAG, "tc_pack_soc	%.1f ", 	tc_pack_soc	);
  ESP_LOGI(TAG, "tc_bit_eoc	%d ", 	tc_bit_eoc	);
  ESP_LOGI(TAG, "tc_bit_socgreater102	%d ", 	tc_bit_socgreater102	);
  ESP_LOGI(TAG, "tc_bit_chrgen	%d ", 	tc_bit_chrgen	);
  ESP_LOGI(TAG, "tc_bit_ocvmeas	%d ", 	tc_bit_ocvmeas	);
  ESP_LOGI(TAG, "tc_bit_mainsacdet	%d ", 	tc_bit_mainsacdet	);
  ESP_LOGI(TAG, "tc_bit_syschgenbl	%d ", 	tc_bit_syschgenbl	);
  ESP_LOGI(TAG, "tc_bit_fastchgenbl	%d ", 	tc_bit_fastchgenbl	);
  ESP_LOGI(TAG, "tc_bit_dischgenbl	%d ", 	tc_bit_dischgenbl	);
  ESP_LOGI(TAG, "tc_bit_isotestinprog	%d ", 	tc_bit_isotestinprog	);
  ESP_LOGI(TAG, "tc_bit_acheatrelaystat	%d ", 	tc_bit_acheatrelaystat	);
  ESP_LOGI(TAG, "tc_bit_acheatswitchstat	%d ", 	tc_bit_acheatswitchstat	);
  ESP_LOGI(TAG, "tc_bit_regenbrkenbl	%d ", 	tc_bit_regenbrkenbl	);
  ESP_LOGI(TAG, "tc_bit_dcdcenbl	%d ", 	tc_bit_dcdcenbl	);
  ESP_LOGI(TAG, "tc_bit_fanactive	%d ", 	tc_bit_fanactive	);
  ESP_LOGI(TAG, "tc_bit_epoemerg	%d ", 	tc_bit_epoemerg	);
  ESP_LOGI(TAG, "tc_bit_crash	%d ", 	tc_bit_crash	);
  ESP_LOGI(TAG, "tc_bit_generalerr	%d ", 	tc_bit_generalerr	);
  ESP_LOGI(TAG, "tc_bit_intisoerr	%d ", 	tc_bit_intisoerr	);
  ESP_LOGI(TAG, "tc_bit_extisoerr	%d ", 	tc_bit_extisoerr	);
  ESP_LOGI(TAG, "tc_bit_thermalisoerr	%d ", 	tc_bit_thermalisoerr	);
  ESP_LOGI(TAG, "tc_bit_isoerr	%d ", 	tc_bit_isoerr	);
  ESP_LOGI(TAG, "tc_bit_manyfailedcells	%d ", 	tc_bit_manyfailedcells	);
  ESP_LOGI(TAG, "tc_bit_chgwaittemp	%d ", 	tc_bit_chgwaittemp	);
  ESP_LOGI(TAG, "tc_bit_reacheocplease	%d ", 	tc_bit_reacheocplease	);
  ESP_LOGI(TAG, "tc_bit_waitoktmpdisch	%d ", 	tc_bit_waitoktmpdisch	);
  ESP_LOGI(TAG, "tc_bit_chgwaitttemp2	%d ", 	tc_bit_chgwaitttemp2	);
  ESP_LOGI(TAG, "tc_bit_chgcurr	%d ", 	tc_bit_chgcurr	);
  ESP_LOGI(TAG, "tc_bit_chgovervolt	%d ", 	tc_bit_chgovervolt	);
  ESP_LOGI(TAG, "tc_bit_chgovercurr	%d ", 	tc_bit_chgovercurr	);
  ESP_LOGI(TAG, "m_candata_timer	%d ", 	m_candata_timer	);
}

void OvmsVehicleThinkCity::IncomingFrameCan1(CAN_frame_t* p_frame)
  {

  if (m_poll_state != 1)
    {
    ESP_LOGI(TAG,"Car has woken (CAN bus activity)");
    StandardMetrics.ms_v_env_awake->SetValue(true);
    PollSetState(1);
    }
  m_candata_timer = TC_CANDATA_TIMEOUT;

  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x263:
      tc_AC_volt =(unsigned int) d[1];
      tc_AC_current = (float)((signed char) d[0]) * 0.2f;

      StandardMetrics.ms_v_charge_current->SetValue(tc_AC_current);
      StandardMetrics.ms_v_charge_voltage->SetValue(tc_AC_volt);
      StandardMetrics.ms_v_env_temp->SetValue((float)((signed char) d[2]) * 0.5f); // PCU abmbient temp
      StandardMetrics.ms_v_pos_speed->SetValue((float)((unsigned char) d[5]) * 0.5f);
      break;

    case 0x301:
      tc_pack_current = (float)((short)(d[0] << 8) | d[1]) / 10.0f;
      tc_pack_voltage = (float)(((unsigned int) d[2] << 8) + d[3]) / 10.0f;
      tc_pack_soc = 100.0f - ((((unsigned int)d[4]<<8) | d[5])/10);
      
      StandardMetrics.ms_v_bat_current->SetValue( tc_pack_current );
      StandardMetrics.ms_v_bat_voltage->SetValue( tc_pack_voltage );
      StandardMetrics.ms_v_bat_power->SetValue(tc_pack_voltage * tc_pack_current / 1000);
      StandardMetrics.ms_v_bat_soc->SetValue(tc_pack_soc);
      StandardMetrics.ms_v_bat_temp->SetValue( (float)((short)(d[6]<<8) | d[7]) / 10.0f );
      StandardMetrics.ms_v_bat_range_ideal->SetValue(111.958773f * tc_pack_soc / 100.0f);
      StandardMetrics.ms_v_bat_range_est->SetValue(93.205678f * tc_pack_soc / 100.0f);
      break;

    case 0x302:
      tc_bit_generalerr = (d[0] & 0x01);
      tc_bit_isoerr = (d[2] & 0x01);
      tc_pack_mindchgvolt = (float)(((unsigned int) d[4] << 8) | d[5]) / 10.0f;
      tc_pack_maxdchgamps = (float)(((unsigned int) d[6] << 8) | d[7]) / 10.0f;
      break;

    case 0x303:
      tc_pack_maxchgvolt = (float)(((unsigned int) d[0] << 8) | d[1]) / 10.0f;
      tc_pack_maxchgcurr = (float)((short)(d[2] << 8) | d[3]) / 10.0f;
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
      tc_sys_voltmaxgen = (float)(((unsigned int) d[0] << 8) | d[1]) / 10.0f;
      tc_bit_eoc = (d[3] & 0x01);
      tc_bit_reacheocplease = (d[3] & 0x02);
      tc_bit_chgwaitttemp2 = (d[3] & 0x04);
      tc_bit_manyfailedcells = (d[3] & 0x08);
      tc_bit_acheatrelaystat = (d[3] & 0x10);
      tc_bit_acheatswitchstat = (d[3] & 0x20);
      tc_pack_temp1 = (float)((short)(d[4] << 8) | d[5]) / 10.0f;
      tc_pack_temp2 = (float)((short)(d[6] << 8) | d[7]) / 10.0f;
      break;

    case 0x305:
      tc_charger_pwm = (float)(((unsigned int) d[0] << 8) | d[1]) / 10.0f;
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

    case 0x511:
      // Enerdel battery pack?
      // TODO: Enable battery pack cells monitoring
      // TODO: Enable min/avg/max cell voltage display
      // TODO: Adjust batt temperature dials in dash to 45C
      break;

    case 0x611:
      // Zebra battery pack?
      // TODO: Enable error message fetching
      // TODO: Adjust batt temperature dials in dash to 300C
      break;

    case 0x75B:
      if (d[3] == 0x65)
      {
        StandardMetrics.ms_v_charge_temp->SetValue((float)((short) (d[4] << 8) | d[5]) / 100.0f);
      }
      else if (d[3] == 0x66)
      {
        StandardMetrics.ms_v_inv_temp->SetValue((float)((short) (d[4] << 8) | d[5]) / 100.0f);
      }
      else if (d[3] == 0x67)
      {
        StandardMetrics.ms_v_mot_temp->SetValue((float)((short) (d[4] << 8) | d[5]) / 100.0f);
      }
      else if (d[3] == 0x68)
      {
        tc_slibatt_temp = (float)((short) (d[4] << 8) | d[5]) / 100.0f;
      }
    
    break;
    
    default:
      break;
    }
  }

void OvmsVehicleThinkCity::Ticker1(uint32_t ticker)
  {

    if (m_candata_timer > 0)
    {
    if (--m_candata_timer == 0)
      {
      // Car has gone to sleep
      ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
      StandardMetrics.ms_v_env_on->SetValue(false);
      StandardMetrics.ms_v_env_awake->SetValue(false);
      PollSetState(0);
      }
    }

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

      if (tc_AC_volt > 100.0f)  // Is AC line voltage > 100 ?
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
    if (tc_AC_volt < 100.0f)  // AC line voltage < 100
    {
      // charging connector unplugged
      StandardMetrics.ms_v_charge_pilot->SetValue(false);
    }

    // Charging interrupted (assumed)
    if ((tc_bit_chrgen == 1) 
      && (tc_bit_eoc != 1) 
      && (tc_bit_ocvmeas != 2) 
      && (tc_pack_current == 0.0f) 
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


void OvmsVehicleThinkCity::Ticker10(uint32_t ticker)
  {
    //LogValues();
  }


void OvmsVehicleThinkCity::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
  {
    ESP_LOGI(TAG,"Poll replay arrived");
     
    // TODO: These are temperatures so maybe the uint8_t is not proper temperatures can be negative too
    uint8_t value = *data;
    switch (pid)
      {
      case 0x4965:  
        StandardMetrics.ms_v_charge_temp->SetValue((float)((short)value) / 100.0f);
        break;
      case 0x4966:  
        StandardMetrics.ms_v_inv_temp->SetValue((float)((short)value) / 100.0f);
        break;
      case 0x4967:  
        StandardMetrics.ms_v_mot_temp->SetValue((float)((short)value) / 100.0f);
        break;
      case 0x4968:
        tc_slibatt_temp =(float)((short)value) / 100.0f;
        break;
      default:
        break;
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
