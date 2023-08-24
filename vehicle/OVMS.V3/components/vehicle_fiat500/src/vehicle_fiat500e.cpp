/*
;    Project:       Open Vehicle Monitor System
;    Date:          5th July 2018
;
;    Changes:
;    1.0  Initial release
;
;
;
;    (C) 2021       Guenther Huck
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
;
; Can bus: CAN1 C-CAN(500kbps); CAN2 B-CAN(50kbps)
; 
; MsgId        bus     Msg                  Signal    variabl                               factor/offset  status  
; 0xC10A040    can1   MSG09_BPCM                   ms_v_bat_soc                SOC                          ok
; 0xC10A040    can1   MSG09_BPCM                   ms_v_bat_range_est          EStRANGE                     ok
; 0xC10A040    can1   MSG09_BPCM                   ms_v_bat_energy_used_total  EStRANGE                    test
; 0xA18A000    can1   STATUS_B_CAN  19/1           ms_v_door_fl                DriveDoorSts                 ok
; 0xA18A000    can1   STATUS_B_CAN  53/1           ms_v_env_handbrake          HandBrakeSts                 ok
; 0xA18A000    can1   STATUS_B_CAN  24/8           ms_v_env_temp               ExternalTemp      5  -40  wrong value
; 0xA18A040    can1   STATUS_C_EVCU 28/3           ms_v_charge_state           ChargingSystemSts            test
;                                                  ms_v_charge_inprogress                                   test
;                                                  ms_v_door_chargeport                                     test
; 0xA28A000    can1   VEHICLE_SPEED_ODOMETER 52/20 ms_v_pos_odometer           TotalOdometer             wrong value     
; ***********************************************************************************************
;
;
;
; ***********************************************************************************************
; 0xC41401f    can2   TCU_REQ       0x20           CommandStartCharge          TCU_ChargeNow_Req
; 0xC41401f    can2   TCU_REQ       0x40           CommandStopCharge           TCU_ChargeNow_Req
;
; 0xC41401f    can2   TCU_REQ       0x8            CommandLock                 TCU_DoorLockCmd
; 0xC41401f    can2   TCU_REQ       0x10           CommandUnLock               TCU_DoorLockCmd
;                                                  ms_v_env_locked
; 0xC41401f    can2   TCU_REQ       0x2                                        TCU_HornLights_Req
; 0xC41401f    can2   TCU_REQ       0x0                                        TCU_HornLights_Req
;
; 0xE194031    can2   TCU_REQ       0x10      5/3  CommandActivateValet        TCU_PreCondNow_Req      PreConditionNow         
; 0xE194031    can2   TCU_REQ       0x20      5/3  CommandDeactivateValet      TCU_PreCondNow_Req      StopPreconditionNow
;  
;
*/

#include "ovms_log.h"
static const char *TAG = "v-fiat500e";

#include <stdio.h>
#include "vehicle_fiat500e.h"
#include "metrics_standard.h"


OvmsVehicleFiat500e::OvmsVehicleFiat500e()
  {
  ESP_LOGI(TAG, "Start Fiat 500e vehicle module");
  
  // Init metrics:
  ft_v_acelec_pwr  = MyMetrics.InitFloat("xse.v.b.acelec.pwr", SM_STALE_MID, 0, Watts);
  ft_v_htrelec_pwr  = MyMetrics.InitFloat("xse.v.b.htrelec.pwr", SM_STALE_MID, 0, Watts);

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_50KBPS);  
  }

OvmsVehicleFiat500e::~OvmsVehicleFiat500e()
  {
  ESP_LOGI(TAG, "Stop Fiat 500e vehicle module");
  }
  
void OvmsVehicleFiat500e::Ticker1(uint32_t ticker)
  {
  }

class OvmsVehicleFiat500eInit
  {
  public: OvmsVehicleFiat500eInit();
  } 
  OvmsVehicleFiat500eInit  __attribute__ ((init_priority (9000)));
  OvmsVehicleFiat500eInit::OvmsVehicleFiat500eInit() 
  {
    ESP_LOGI(TAG, "Registering Vehicle: FIAT 500e (9000)");
    MyVehicleFactory.RegisterVehicle<OvmsVehicleFiat500e>("FT5E","Fiat 500e");  
      //StandardMetrics.ms_v_door_chargeport->SetValue(false); // false (for test only)
      //StandardMetrics.ms_v_env_locked->SetValue(true); // True for Test only
      //StandardMetrics.ms_v_pos_odometer->SetValue(0);
      //StandardMetrics.ms_v_env_temp->SetValue(0);
      //StandardMetrics.ms_v_env_cabintemp->SetValue(0);
      //StandardMetrics.ms_v_bat_soc->SetValue(0);
      //StandardMetrics.ms_v_bat_range_est->SetValue(0);
      //StandardMetrics.ms_v_bat_temp->SetValue(0);
      //StandardMetrics.ms_v_bat_voltage->SetValue(0);
  } 

//*****************************************************************************
//****************** Incoming Frames Can-Bus C (500kbps) **********************
//*****************************************************************************

void OvmsVehicleFiat500e::IncomingFrameCan1(CAN_frame_t* p_frame) {
  
  uint8_t *d = p_frame->data.u8;
  
  switch (p_frame->MsgID) { 
    case 0xC10A040: // MSG31B_EVCU
    {
      StandardMetrics.ms_v_bat_range_est->SetValue(d[0]);
      //Est_Range (estimated range)
      //d[0] 11111111 
      float soc = d[1] >> 1;
      StandardMetrics.ms_v_bat_soc->SetValue(soc);
      //SOC_Display (state of charge)
      //d[0] 11111110
      float pow = ((uint16_t) d[2] << 5) | (d[3] >>3);
      StandardMetrics.ms_v_bat_energy_used->SetValue((pow/100)-24);
      //möglicherweise aktueller ENergieverbrauch in kw?
      //HVTotalEnergy (Energieverbrauch X*0,01-24) in kwh?
      //d[2] 11111111
      //d[3] 11111000
      break;
    }   
    case 0x820A040: //MSG29_EVCU
    {
      StandardMetrics.ms_v_charge_inprogress->SetValue(d[1]&0x10);
      //ms_v_door_chargeport ?
      //J1772_S2_Close //0x0 open //0x1 closed
      //d[1] 00010000
      break;
    }
    case 0x640A046: //MSG06_BPCM
    {
      StandardMetrics.ms_v_charge_inprogress->SetValue(d[5]&0x4);
      //StandardMetrics.ms_v_door_chargeport->SetValue(d[5]&0x4);
      //RdyForChrg (ready for charge)
      //d[5] 00000100
      break;
    }
    case 0x400A042: //EM_02
    {
      //StandardMetrics. ms_v_mot_temp->SetValue(d[0]*0.5-25);
      StandardMetrics. ms_v_mot_temp->SetValue(((float)d[0])-50);
      //TempCurrMot (temperature of e-machine)
      //d[0] 11111111
      StandardMetrics. ms_v_gen_temp->SetValue(((float)d[1])-50);
      //TempCurrRotor (temperature of rotor)
      //d[1] 11111111
       StandardMetrics. ms_v_inv_temp->SetValue(((float)d[2])-50);
         //StandardMetrics. ms_v_inv_temp->SetValue(d[2]*0.5-25);
      //TempCurrPWR (temperature of inverter)
      //d[2] 11111111
      break;
    }
    case 0xC08A040: //MSG30_EVCU
    {
      ft_v_acelec_pwr->SetValue(d[0]*4);
      //ACElecPwr (Aircondition electric power)
      //d[0] 11111111
      ft_v_htrelec_pwr->SetValue(d[4]*4);
      //HtrElecPwr (Heater electric power)
      //d[1] 11111111
      break;
    }
    case 0xC50A049: //MSG36_OBCM
    {
      unsigned int cvolt = ((unsigned int)d[1]<<8)
                          + (unsigned int)d[2];
      StandardMetrics.ms_v_charge_voltage->SetValue((float)cvolt/10);

      unsigned int ccurr = ((unsigned int)d[3]<<8)
                          | (unsigned int)d[4];
      StandardMetrics.ms_v_charge_current->SetValue((float)ccurr/5-50);

      unsigned int bvolt = ((unsigned int)d[5]<<8)
                          | (unsigned int)d[6];
      StandardMetrics.ms_v_bat_voltage->SetValue((float)bvolt/10);
      
      StandardMetrics.ms_v_charge_temp->SetValue(d[0]);
     
      break;
    }
    case 0x840A046: //MSG08_BPCM 
    {
      float btemp = (((d[2] & 0x7f) << 1) | (d[2] & 0x80));
      //StandardMetrics.ms_v_bat_temp->SetValue(btemp*0.5-40);
      StandardMetrics.ms_v_bat_temp->SetValue(btemp-40);
      //StandardMetrics.ms_v_bat_temp->SetValue(((d[2]<<1)|(d[3]&0x80))-40);
      //ModTempAvg (Modultemperatur)
      //d[2] 011111111 
      //d[3]           100000000
      break;
    }
    /*
    case 0xA18A040: // STATUS_C_EVCU
    {
      if (((d[3]&0x70)>>4) == 0x0) {
        //ChargingSystemSts (0x0 Not Charging)
        //d[3] 01110000
        StandardMetrics.ms_v_charge_inprogress->SetValue(false); // False
        StandardMetrics.ms_v_door_chargeport->SetValue(false); // False
        StandardMetrics.ms_v_charge_state->SetValue("topoff");
        break;
      }
      if (((d[3]&0x70)>>4) == 0x1) {
        //ChargingSystemSts (0x1 Charging)
        //d[3] 01110000
	      StandardMetrics.ms_v_charge_inprogress->SetValue(true); // True
        StandardMetrics.ms_v_door_chargeport->SetValue(true); // True
        StandardMetrics.ms_v_charge_state->SetValue("charging");
        break;
      }
      if (((d[3]&0x70)>>4) == 0x2) {
        //ChargingSystemSts (0x2 Charge Interrupted)
        //d[3] 01110000
	      StandardMetrics.ms_v_charge_inprogress->SetValue(true); // False
        StandardMetrics.ms_v_door_chargeport->SetValue(true); // True
        StandardMetrics.ms_v_charge_state->SetValue("stopped");
        break;
      }
      if (((d[3]&0x70)>>4) == 0x3) {  //ChargingSystemSts (0x3 Charge Complete)
        //d[3] 01110000
	      StandardMetrics.ms_v_charge_inprogress->SetValue(false); // False
        StandardMetrics.ms_v_door_chargeport->SetValue(false); // True
        StandardMetrics.ms_v_charge_state->SetValue("done");
        break;
      }
      break;
    }
    default:
    //ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x",
    //p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
    break;
    }
    */
  }
}
//*****************************************************************************
//****************** Incoming Frames Can-Bus B (50kbps) ***********************
//*****************************************************************************

void OvmsVehicleFiat500e::IncomingFrameCan2(CAN_frame_t* p_frame) {
  
  uint8_t *d = p_frame->data.u8;
  
  switch (p_frame->MsgID) {
    /*
    case 0xC414003: // IPC_DISPLAY
    {
      float soc = ((uint8_t)d[0]>>1);
      //SOC_Display (state of charge)
      //d[0] 11111110
      
      float estrange = ((uint8_t)d[1]>>1);
      //Est_Range (estimated range)
      //d[1] 11111110 
      
      float pow = ((uint16_t) d[6] << 4) | (d[3]&0xF0);
      //möglicherweise aktueller ENergieverbrauch in kw?
      //HVTotalEnergy (Energieverbrauch X*0,01-24) in kwh?
      //d[4] 00001111
      //d[3] 11100000

      StandardMetrics.ms_v_bat_soc->SetValue(soc); 
      StandardMetrics.ms_v_bat_range_est->SetValue(estrange);
      StandardMetrics.ms_v_bat_energy_used->SetValue((pow/100)-24); 
      break;
    }
    */
    case 0x6214000: // STATUS_BCM
    {
      StandardMetrics.ms_v_env_handbrake->SetValue(d[0]&0x20);    
      //HandBrakeSts (0x0 off, 0x1 on)
      StandardMetrics.ms_v_door_fl->SetValue(d[1]&0x4);
      //DriverDoorSts (0x0 closed, 0x1 open)
      StandardMetrics.ms_v_door_fr->SetValue(d[1]&0x8);
      //PsngrDoorSts (0x0 closed, 0x1 open)
      StandardMetrics.ms_v_door_trunk->SetValue(d[1]&0x40);
      //RHatchSts (0x0 closed, 0x1 open)
      //*********** additional parameter *************
      //StandardMetrics.ms_v_?????->SetValue(d[2]&0x4);
      //WarningKeyInSts (0x0 not active, 0x1 active- This signal activates a 
      //buzzer when the driver door is open and the key is in the starting system)
      //***********************************************
      //StandardMetrics.ms_v_?????->SetValue(d[2]&0x8);
      //RechargeSts (0x0 charging, 0x1 not charging)
      //***********************************************
      //StandardMetrics.ms_v_?????->SetValue(d[0]&0x8);
      //InternalLightSts (0x0 not active, 0x1 active)
      //***********************************************
      //StandardMetrics.ms_v_?????->SetValue(d[0]&0x64);
      //BrakeFluidLevelSts (0x0 ok, 0x1 low level)
      break;
    }
    case 0x631400A: // STATUS_ECC2
    {
      unsigned int evapset = ((d[1]& 0x3f) << 2)
                            +((d[2]& 0xf8) >> 4);
      //00111111 11110000
      StandardMetrics.ms_v_env_cabinsetpoint->SetValue(((float)evapset));
      //StandardMetrics.ms_v_env_cabintemp->SetValue(d[3]*0.5-40);
      //StandardMetrics.ms_v_env_cabintemp->SetValue(((float)(d[3])-51));
      //Cabin Temp Set/Evap Temp Target
      //***********
      switch (d[1]&0xC0) {
        case 0x0: {
        // PreCondCabinSts  (0x0 off, 0x1 on, 0x2 Set point reached)
        //d[1] 11000000
        StandardMetrics.ms_v_env_valet->SetValue(false);    
        break;
        } 
        case 0x40: {
        // PreCondCabinSts  (0x0 off, 0x1 on, 0x2 Set point reached)
        //d[1] 11000000
        StandardMetrics.ms_v_env_valet->SetValue(true); 
        break;
        }
        case 0xC0: {
        // PreCondCabinSts  (0x0 off, 0x1 on, 0x2 Set point reached)
        //d[1] 11000000
	      StandardMetrics.ms_v_env_valet->SetValue(true);
        break;
        }
      default:
        StandardMetrics.ms_v_env_valet->SetValue(false);
      }
      break;
    }
    case 0xA194040: // STATUS_B_EVCU
    {
      if ((d[3]&0x30) == 0) {
        //ChargingSystemSts (0x0 Not Charging)
        //d[3] 01110000
        //StandardMetrics.ms_v_charge_inprogress->SetValue(false); // False
        StandardMetrics.ms_v_door_chargeport->SetValue(false); // False
        StandardMetrics.ms_v_charge_state->SetValue("topoff");
        break;
      }
      if ((d[3]&0x30) == 0x1) {
        //ChargingSystemSts (0x1 Charging)
        //d[3] 01110000
	      //StandardMetrics.ms_v_charge_inprogress->SetValue(true); // True
        StandardMetrics.ms_v_door_chargeport->SetValue(true); // True
        StandardMetrics.ms_v_charge_state->SetValue("charging");
        break;
      }
      if ((d[3]&0x30) == 0x2) {
        //ChargingSystemSts (0x2 Charge Interrupted)
        //d[3] 01110000
	      //StandardMetrics.ms_v_charge_inprogress->SetValue(true); // False
        StandardMetrics.ms_v_door_chargeport->SetValue(true); // True
        StandardMetrics.ms_v_charge_state->SetValue("stopped");
        break;
      }
      if ((d[3]&0x30) == 0x3) {
        //ChargingSystemSts (0x3 Charge Complete)
        //d[3] 01110000
	      //StandardMetrics.ms_v_charge_inprogress->SetValue(false); // False
        StandardMetrics.ms_v_door_chargeport->SetValue(false); // True
        StandardMetrics.ms_v_charge_state->SetValue("done");
        break;
      }
      break;
    }
    case 0x6414000: // STATUS_BCM4
    {
      StandardMetrics.ms_v_env_locked->SetValue(d[3]&0x40);
      //DriverDoorLogicSts (0x0 unlocked, 0x1 locked)
      break;
    }
    case 0x63D4000: // ENVIRONMENTAL_CONDITIONS
    {
      if (d[0] != 0) {
      StandardMetrics.ms_v_env_temp->SetValue((d[0]*0.5)-40);
      //ExternalTemperature (Aussentemperatur)
      //d[0] 11111111
      break;
      }

      float vbat = (d[1]&0x7F);
      StandardMetrics.ms_v_bat_voltage->SetValue(vbat*0.16);
      //BatteryVoltageLevel (X+0.16 in Volt)
      //d[1] 01111111 
      break;
    }
  /*
    case 0x6814000: // ENVIRONMENTAL_CONDITIONS
     {
     //Motortemperatur ? CoreTemp
      //StandardMetrics.ms_v_bat_temp->SetValue(d[6]*0.1);
      //Sensor
      //StandardMetrics.ms_v_bat_temp->SetValue(d[5]-40);
      //break;
  */
    case 0xC414000: // HUMIDITY_000
    {
    //HumSenAirTemp / BEV NEW. Humidity element temperature
    float itemp = ((uint16_t) d[3] << 1) | (d[4]&0x80 >> 7 );
    StandardMetrics.ms_v_env_cabintemp->SetValue(itemp*0.5-40);
    //01111111 10000000
    break;
    }
     case 0xC014003: //TRIP_A_B
    {
     	 float vodo = ((d[1] & 0x0f) << 16) | (d[2] << 8) | d[3];
       StandardMetrics.ms_v_pos_odometer->SetValue(vodo);
      //TotalOdometer (km)
      // 00001111 11111111 11111111
      break;
    }
/* 
    case 0x631400A: //STATUS_ECC2
    {
   	 if ((d[1]&0xC0) == 0x0) {
	      StandardMetrics.ms_v_env_valet->SetValue(false);
        //ChargingSystemSts (0x0 Off)
        //d[1] 11000000
        break;
        }
      if ((d[1]&0xC0) == 0x1) {
	      StandardMetrics.ms_v_env_valet->SetValue(true);
        //ChargingSystemSts (0x1 currently preconditioning)
        //d[1] 11000000
        break;
        }
      if ((d[1]&0xC0) == 0x2) {
	      StandardMetrics.ms_v_env_valet->SetValue(false);
        //ChargingSystemSts (0x2 Set point reached/Maintaining)
        //d[1] 11000000
        break;
      }
      break;
    }
    default:
    //ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x",
    //p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
  break;
    }  
  */
  }  
}

//*****************************************************************************
//************************ Commands (send on Can-bus) *************************
//*****************************************************************************

OvmsVehicle::vehicle_command_t OvmsVehicleFiat500e::CommandWakeup() {
  CAN_frame_t frame = {};
  frame.FIR.B.FF = CAN_frame_ext; 
  frame.MsgID = 0xE094000; // NWM_BCM
  frame.FIR.B.DLC = 6;
  frame.data.u8[0] = 0x0;
  frame.data.u8[1] = 0x1;  // SystemCommand // 0x1 wakeup / 0x2 stay active
  frame.data.u8[2] = 0x0;
  frame.data.u8[3] = 0x0;
  frame.data.u8[4] = 0x0;
  frame.data.u8[5] = 0x0;
  m_can2->Write(&frame);
  
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleFiat500e::CommandStartCharge() {
  CAN_frame_t frame = {};
  frame.FIR.B.FF = CAN_frame_ext;
  frame.MsgID = 0xC41401f;  // TCU_REQ
  frame.FIR.B.DLC = 1;
  frame.data.u8[0] = 0x20;  // TCU_ChargeNow_Req // start charge now
  m_can2->Write(&frame);
  
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleFiat500e::CommandStopCharge() {
  CAN_frame_t frame = {};
  frame.FIR.B.FF = CAN_frame_ext;
  frame.MsgID = 0xC41401f;  // TCU_REQ
  frame.FIR.B.DLC = 1;
  frame.data.u8[0] = 0x40;  // TCU_ChargeNow_Req // stop charge now
  m_can2->Write(&frame);
  
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleFiat500e::CommandLock(const char* pin) {
  CAN_frame_t frame = {};
  frame.FIR.B.FF = CAN_frame_ext;
  frame.MsgID = 0xC41401f;  // TCU_REQ
  frame.FIR.B.DLC = 1;
  frame.data.u8[0] = 0x8;  // TCU_DoorLockCmd // lock
  m_can2->Write(&frame);
  
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  //StandardMetrics.ms_v_env_locked->SetValue(true);
  return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleFiat500e::CommandUnlock(const char* pin) {
  CAN_frame_t frame = {};
  frame.FIR.B.FF = CAN_frame_ext;
  frame.MsgID = 0xC41401f;  // TCU_REQ 
  frame.FIR.B.DLC = 1;
  frame.data.u8[0] = 0x10;  // TCU_DoorLockCmd // unlock
  m_can2->Write(&frame);
  
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  //StandardMetrics.ms_v_env_locked->SetValue(false);
  return Success; 
}

OvmsVehicle::vehicle_command_t OvmsVehicleFiat500e::CommandActivateValet(const char* pin) {
  CAN_frame_t frame_wu = {};
  frame_wu.FIR.B.FF = CAN_frame_ext; 
  frame_wu.MsgID = 0xE094000; // NWM_BCM
  frame_wu.FIR.B.DLC = 6;
  frame_wu.data.u8[0] = 0x0;
  frame_wu.data.u8[1] = 0x1;  // SystemCommand // 0x1 wakeup / 0x2 stay active
  frame_wu.data.u8[2] = 0x0;
  frame_wu.data.u8[3] = 0x0;
  frame_wu.data.u8[4] = 0x0;
  frame_wu.data.u8[5] = 0x0;
    
  CAN_frame_t frame = {};
  frame.FIR.B.FF = CAN_frame_ext; 
  frame.MsgID = 0xE194031; // TCU_PRECOND_NOW
  frame.FIR.B.DLC = 2;
  frame.data.u8[0] = 0x20;  // TCU_PreCondNow_Req // PreCondition Start
  frame.data.u8[1] = 0x64;  
    
  m_can2->Write(&frame_wu);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  /*
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);*/
  return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleFiat500e::CommandDeactivateValet(const char* pin) {
  CAN_frame_t frame = {};
  frame.FIR.B.FF = CAN_frame_ext; 
  frame.MsgID = 0xE194031; // TCU_PRECOND_NOW
  frame.FIR.B.DLC = 2;
  frame.data.u8[0] = 0x40;  // TCU_PreCondNow_Req // PreCondition Stop
  frame.data.u8[1] = 0x64;  
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  StandardMetrics.ms_v_env_valet->SetValue(false);
  
  /*vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);*/
  return Success;
}

/*
//Schalte precondition ein / aus

OvmsVehicle::vehicle_command_t OvmsVehicleFiat500e::CommandHomelink(int button, int durationms) {
  if (button == 0) {  
  CAN_frame_t frame = {};
  frame.FIR.B.FF = CAN_frame_ext; 
  frame.MsgID = 0xE194031; // TCU_PRECOND_NOW
  frame.FIR.B.DLC = 2;
  frame.data.u8[0] = 0x20;  // TCU_PreCondNow_Req // PreCondition Start
  frame.data.u8[1] = 0x64;  
  m_can2->Write(&frame);
  
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  return Success;
  }
  
  if (button == 1) {
  CAN_frame_t frame = {};
  frame.FIR.B.FF = CAN_frame_ext; 
  frame.MsgID = 0xE194031; // TCU_PRECOND_NOW
  frame.FIR.B.DLC = 2;
  frame.data.u8[0] = 0x40;  // TCU_PreCondNow_Req // PreCondition Stop
  frame.data.u8[1] = 0x64;  
  m_can2->Write(&frame);
  
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  StandardMetrics.ms_v_env_valet->SetValue(false);
  return Success;
  }

  if (button == 2) {
    CAN_frame_t frame = {};
    frame.FIR.B.FF = CAN_frame_ext;
    frame.MsgID = 0x18DA42f1;  // DIAGNOSTIC_REQUEST_BPCM (ODO)
    frame.FIR.B.DLC = 8;
    frame.data.u8[0] = 0x03;
    frame.data.u8[1] = 0x22;
    frame.data.u8[2] = 0x20;
    frame.data.u8[3] = 0x01;
    frame.data.u8[4] = 0x00;
    frame.data.u8[5] = 0x00;
    frame.data.u8[6] = 0x00;
    frame.data.u8[7] = 0x00;
    m_can1->Write(&frame);
  
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can1->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can1->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
    return Success;
  }
return NotImplemented;
}
*/
//Schalte Alarmton ein / aus
OvmsVehicle::vehicle_command_t OvmsVehicleFiat500e::CommandHomelink(int button, int durationms)
{
  //Test of Homelink Alarm!!!
  if (button == 0) {
    CAN_frame_t frame = {};
    frame.FIR.B.FF = CAN_frame_ext;
    frame.MsgID = 0xC41401f;  // TCU_REQ
    frame.FIR.B.DLC = 1;
    frame.data.u8[0] = 0x2;  // HornLightsReq // Enable Light and Horn Flashing
    m_can2->Write(&frame);
  
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
    return Success; 
  }
  
  if (button == 1) {
    CAN_frame_t frame = {};
    frame.FIR.B.FF = CAN_frame_ext;
    frame.MsgID = 0xC41401f;  // TCU_REQ
    frame.FIR.B.DLC = 1;
    frame.data.u8[0] = 0x0;  // HornLightsReq // Enable Light and Horn Flashing
    m_can2->Write(&frame);
  
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  m_can2->Write(&frame);
  vTaskDelay(50 / portTICK_PERIOD_MS);
    return Success;
  }
  if (button == 2) {
    CAN_frame_t frame = {};
    frame.FIR.B.FF = CAN_frame_ext;
    frame.MsgID = 0x18DA42f1;  // DIAGNOSTIC_REQUEST_BPCM (ODO)
    frame.FIR.B.DLC = 8;
    frame.data.u8[0] = 0x03;
    frame.data.u8[1] = 0x22;
    frame.data.u8[2] = 0x20;
    frame.data.u8[3] = 0x01;
    frame.data.u8[4] = 0x00;
    frame.data.u8[5] = 0x00;
    frame.data.u8[6] = 0x00;
    frame.data.u8[7] = 0x00;
    
    CAN_frame_t frame_1 = {};
    frame_1.FIR.B.FF = CAN_frame_ext;
    frame_1.MsgID = 0x18DA47f1;  // DIAGNOSTIC_REQUEST_BPCM (ODO)
    frame_1.FIR.B.DLC = 8;
    frame_1.data.u8[0] = 0x03;
    frame_1.data.u8[1] = 0x22;
    frame_1.data.u8[2] = 0x20;
    frame_1.data.u8[3] = 0x01;
    frame_1.data.u8[4] = 0x00;
    frame_1.data.u8[5] = 0x00;
    frame_1.data.u8[6] = 0x00;
    frame_1.data.u8[7] = 0x00;
    m_can1->Write(&frame);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    m_can1->Write(&frame_1);
  
  //vTaskDelay(50 / portTICK_PERIOD_MS);
  //m_can1->Write(&frame);
  //vTaskDelay(50 / portTICK_PERIOD_MS);
  //m_can1->Write(&frame);
  return Success;
  }
  return NotImplemented;
}

