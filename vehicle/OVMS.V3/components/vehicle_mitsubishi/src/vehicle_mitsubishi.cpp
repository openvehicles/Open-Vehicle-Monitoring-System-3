/*
;    Project:       Open Vehicle Monitor System
;    Date:          2th Sept 2018
;
;    Changes:
;    0.1.0  Initial release: 13-oct-2018 - KommyKT (tested on Peugeot iOn 03-2012 LHD)
;       - web dashboard limit modifications
;       - all 66 temp sensor avg for battery temp
;       - all cell temperature stored
;       - basic charge states
;       - AC charger voltage / current measure
;       - DC
;    0.1.1 11-nov-2018 - KommyKT
;       - BMS Command
;       - trip Command
;       - aux Command
;       - web battery status with volts and temperatures
;    0.1.2 14-nov-2018
;       - new standardised BMS usage
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;    (C) 2018       Tamás Kovács (KommyKT)
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
#include <stdio.h>
#include "vehicle_mitsubishi.h"

#define VERSION "0.1.2"

static const char *TAG = "v-mitsubishi";
typedef enum
  {
  CHARGER_STATUS_CHARGING,
  CHARGER_STATUS_QUICK_CHARGING,
  CHARGER_STATUS_FINISHED,
  CHARGER_STATUS_INTERRUPTED
  } ChargerStatus;

OvmsVehicleMitsubishi::OvmsVehicleMitsubishi()
  {
  ESP_LOGI(TAG, "Start Mitsubishi iMiev, Citroen C-Zero, Peugeot iOn vehicle module");

  StandardMetrics.ms_v_env_parktime->SetValue(0);
  StandardMetrics.ms_v_charge_type->SetValue("None");

  // Require GPS.
  MyEvents.SignalEvent("vehicle.require.gps", NULL);
  MyEvents.SignalEvent("vehicle.require.gpstime", NULL);

  memset(m_vin,0,sizeof(m_vin));

  mi_trip_start_odo = 0;
  WebInit();

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

  //BMS
  BmsSetCellArrangementVoltage(88, 8);
  BmsSetCellArrangementTemperature(66, 6);

  // init commands:
  cmd_xmi = MyCommandApp.RegisterCommand("xmi", "Mitsubishi iMiEV", NULL, "", 0, 0, true);
  cmd_xmi->RegisterCommand("battreset", "Battery monitor", CommandBatteryReset, "", 0, 0, true);
  //cmd_xmi->RegisterCommand("bms","Cell voltages", xmi_bms, 0,0, false);
  cmd_xmi->RegisterCommand("aux", "Aux Battery", xmi_aux, 0, 0, false);
  cmd_xmi->RegisterCommand("trip","Show trip info", xmi_trip, 0,0, false);

  }
OvmsVehicleMitsubishi::~OvmsVehicleMitsubishi()
  {
  ESP_LOGI(TAG, "Stop Mitsubishi  vehicle module");

  // Release GPS.
  MyEvents.SignalEvent("vehicle.require.gps", NULL);
  MyEvents.SignalEvent("vehicle.require.gpstime", NULL);
  }

  /**
   * BatteryReset: reset battery min / max
   */
void OvmsVehicleMitsubishi::BatteryReset()
  {
    ESP_LOGD(TAG, "battmon reset");
    BmsResetCellStats();
  }

/**
 * Charge Status
 */
void vehicle_charger_status(ChargerStatus status)
        {
        switch (status)
          {
          case CHARGER_STATUS_QUICK_CHARGING:

            StandardMetrics.ms_v_pos_speed->SetValue(0);
            StandardMetrics.ms_v_mot_rpm->SetValue(0);
            StandardMetrics.ms_v_door_chargeport->SetValue(true);
            StandardMetrics.ms_v_charge_pilot->SetValue(true);
            StandardMetrics.ms_v_charge_inprogress->SetValue(true);
            StandardMetrics.ms_v_charge_type->SetValue("Chademo");
            StandardMetrics.ms_v_charge_state->SetValue("charging");
            StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
            StandardMetrics.ms_v_charge_mode->SetValue("Quickcharge");
            StandardMetrics.ms_v_env_charging12v->SetValue(true);
            StandardMetrics.ms_v_env_on->SetValue(true);
            StandardMetrics.ms_v_env_awake->SetValue(true);
            StandardMetrics.ms_v_charge_climit->SetValue(125);
            StandardMetrics.ms_v_charge_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsInt());
            StandardMetrics.ms_v_charge_current->SetValue(StandardMetrics.ms_v_bat_current->AsInt());
            break;
          case CHARGER_STATUS_CHARGING:

            StandardMetrics.ms_v_pos_speed->SetValue(0);
            StandardMetrics.ms_v_mot_rpm->SetValue(0);
            StandardMetrics.ms_v_door_chargeport->SetValue(true);
            StandardMetrics.ms_v_charge_pilot->SetValue(true);
            StandardMetrics.ms_v_charge_inprogress->SetValue(true);
            StandardMetrics.ms_v_charge_type->SetValue("Type1");
            StandardMetrics.ms_v_charge_state->SetValue("charging");
            StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
            StandardMetrics.ms_v_charge_mode->SetValue("Standard");
            StandardMetrics.ms_v_env_charging12v->SetValue(true);
            StandardMetrics.ms_v_env_on->SetValue(true);
            StandardMetrics.ms_v_env_awake->SetValue(true);
            StandardMetrics.ms_v_charge_climit->SetValue(16);

            break;
          case CHARGER_STATUS_FINISHED:

            StandardMetrics.ms_v_charge_climit->SetValue(0);
            StandardMetrics.ms_v_charge_current->SetValue(0);
            StandardMetrics.ms_v_charge_voltage->SetValue(0);
            StandardMetrics.ms_v_door_chargeport->SetValue(false);
            StandardMetrics.ms_v_charge_pilot->SetValue(false);
            StandardMetrics.ms_v_charge_inprogress->SetValue(false);
            StandardMetrics.ms_v_charge_type->SetValue("None");
            StandardMetrics.ms_v_charge_state->SetValue("done");
            StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
            StandardMetrics.ms_v_charge_mode->SetValue("Not charging");
            StandardMetrics.ms_v_env_charging12v->SetValue(false);
            StandardMetrics.ms_v_env_on->SetValue(false);
            StandardMetrics.ms_v_env_awake->SetValue(false);
            break;
          case CHARGER_STATUS_INTERRUPTED:

            StandardMetrics.ms_v_charge_climit->SetValue(0);
            StandardMetrics.ms_v_charge_current->SetValue(0);
            StandardMetrics.ms_v_charge_voltage->SetValue(0);
            StandardMetrics.ms_v_door_chargeport->SetValue(false);
            StandardMetrics.ms_v_charge_pilot->SetValue(false);
            StandardMetrics.ms_v_charge_inprogress->SetValue(false);
            StandardMetrics.ms_v_charge_type->SetValue("None");
            StandardMetrics.ms_v_charge_state->SetValue("interrupted");
            StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
            StandardMetrics.ms_v_charge_mode->SetValue("Interrupted");
            StandardMetrics.ms_v_env_on->SetValue(false);
            StandardMetrics.ms_v_env_awake->SetValue(false);
            break;
          }
        if (status != CHARGER_STATUS_CHARGING && status != CHARGER_STATUS_QUICK_CHARGING)
          {
            StandardMetrics.ms_v_charge_current->SetValue(0);
            StandardMetrics.ms_v_charge_voltage->SetValue(0);
          }
        }

void OvmsVehicleMitsubishi::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
      case 0x101: //Key status
      {
        if(d[1] == 4)
        {
          vehicle_mitsubishi_car_on(true);
        }else if(d[1] == 0)
        {
          vehicle_mitsubishi_car_on(true);
        }

      break;
      }

      case 0x208: // Brake pedal position
      {
        StandardMetrics.ms_v_env_footbrake->SetValue(((d[2] * 256 + d[3]) - 24576.0) / 640 * 100.0);
      break;
      }

      case 0x210: // Accelerator pedal position
      {
        StandardMetrics.ms_v_env_throttle->SetValue(d[2]*0.4);
      break;
      }

      case 0x231: // Brake pedal pressed?
      {
        if (d[4] == 0) {
          //Brake not pressed
        } else if (d[4] == 2) {
          /* Brake pressed*/
        }
      break;
      }

      case 0x236: // Steeering wheel
      {
        // (((d[0] * 256) +d[1])-4096)/2  //Negative angle - right, positive angle left
        // ((d[2] * 256 + d[3] - 4096) / 2.0)) //Steering wheel movement
      break;
      }

      case 0x286: // Charger/inverter temperature
      {
        StandardMetrics.ms_v_charge_temp->SetValue((float)d[3]-40);
        StandardMetrics.ms_v_inv_temp->SetValue((float)d[3]-40);
      break;
      }

      case 0x298: // Motor temperature and RPM
      {
        StandardMetrics.ms_v_mot_temp->SetValue((int)d[3]-40);
        StandardMetrics.ms_v_mot_rpm->SetValue(((d[6]*256.0)+d[7])-10000);

        if(StandardMetrics.ms_v_mot_rpm->AsInt() < 10 && StandardMetrics.ms_v_mot_rpm->AsInt() > -10)
        { // (-10 < Motor RPM > 10) Set to 0
          StandardMetrics.ms_v_mot_rpm->SetValue(0);
        }
      break;
      }

      case 0x29A: // VIN determination
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

      case 0x325:
      {
        /*d[0]==1?*/
      break;
      }

      case 0x346: // Estimated range , // Handbrake state
      {
        StandardMetrics.ms_v_bat_range_est->SetValue(d[7]);

        if((d[4] & 32) == 0)
        {
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          StandardMetrics.ms_v_env_parktime->SetValue(0);
          break;
        }
        StandardMetrics.ms_v_env_handbrake->SetValue(true);
      break;
      }

      case 0x373: // Main Battery volt and current
      {
        StandardMetrics.ms_v_bat_current->SetValue((((((d[2]*256.0)+d[3]))-32768))/100.0);
        StandardMetrics.ms_v_bat_voltage->SetValue((d[4]*256.0+d[5])/10.0);
        StandardMetrics.ms_v_bat_power->SetValue((StandardMetrics.ms_v_bat_voltage->AsFloat()*StandardMetrics.ms_v_bat_current->AsFloat())/1000.0*-1.0);
      break;
      }

      case 0x374: // Main Battery Soc
      {
        StandardMetrics.ms_v_bat_soc->SetValue(((int)d[1]-10)/2.0);
        StandardMetrics.ms_v_bat_cac->SetValue(d[7]/2.0);
      break;
      }

      case 0x384: //heating current?
      {
        //d[4]/10 //heating current;
      break;
      }

      case 0x389: // Charger voltage and current
      {
        StandardMetrics.ms_v_charge_voltage->SetValue(d[1]*1.0);
        StandardMetrics.ms_v_charge_current->SetValue(d[6]/10.0);
      break;
      }

      case 0x3A4: // Climate console
      {
        /**
         * http://myimiev.com/forum/viewtopic.php?p=31226 PID 3A4 byte 0, bits
         * 0-3: heating level (7 is off, under 7 is cooling, over 7 is heating)
         * byte 0, bit 7: AC on (ventilation dial pressed) byte 0, bit 5: MAX
         * heating (heating dial pressed) byte 0, bit 6: air recirculation
         * (ventilation direction dial pressed)
         *
         * byte 1, bits 0-3: ventilation level (if AUTO is chosen, the
         * automatically calculated level is returned) byte 1, bits 4-7:
         * ventilation direction (1-2 face, 3 legs+face, 4 -5legs, 6
         * legs+windshield 7-9 windshield)
         */

        //heating level
        if (((int)d[0]<<4) == 112)
        {
          //Heater off
        }else if (((int)d[0]<< 4) > 112)
        {
          // Heating
        }
        else if (((int)d[0]<< 4) < 112)
        {
          //Cooling
        }

      if (((int)d[0]>>7) == 1)
      {
       // AC on
      }

      string ventDirection = "-";
      switch ((d[1] & 240 >> 4)) {
        case 1:
        case 2:
        ventDirection = "Face";
        break;
        case 3:
        case 4:
        ventDirection = "Legs + Face";
        break;
        case 5:
        case 6:
        ventDirection = "Legs";
        break;
        case 7:
        case 8:
        ventDirection = "Legs + Windshield";
        break;
        case 9:
        ventDirection = "Windshield";
        break;
      }
      break;
    }
    case 0x408:
    {
      // all == 0
    break;
    }
    case 0x412: // Speed and odometer
    {
      if (d[1]>200)
        StandardMetrics.ms_v_pos_speed->SetValue((int)d[1]-255.0,Kph);
      else
        StandardMetrics.ms_v_pos_speed->SetValue(d[1]);

        StandardMetrics.ms_v_pos_odometer->SetValue(((int)d[2]<<16)+((int)d[3]<<8)+d[4],Kilometers);
    break;
    }

    case 0x418: // Transmissin state determination
      {
        switch (d[0]){
          case 80: //P
          {
            StandardMetrics.ms_v_env_gear->SetValue(-1);
            break;
          }
          case 82: //R
          {
            StandardMetrics.ms_v_env_gear->SetValue(-2);
            StandardMetrics.ms_v_env_parktime->SetValue(0);
            break;
          }
          case 78: //N
          {
            StandardMetrics.ms_v_env_gear->SetValue(0);
            StandardMetrics.ms_v_env_parktime->SetValue(0);
            break;
          }
          case 68: //D
          {
            StandardMetrics.ms_v_env_gear->SetValue(1);
            StandardMetrics.ms_v_env_parktime->SetValue(0);
            break;
          }
          case 131: //B
          {
            StandardMetrics.ms_v_env_gear->SetValue(2);
            StandardMetrics.ms_v_env_parktime->SetValue(0);
            break;
          }
          case 50: //C
          {
            StandardMetrics.ms_v_env_gear->SetValue(3);
            StandardMetrics.ms_v_env_parktime->SetValue(0);
            break;
          }
        }

        break;
      }

    case 0x424: // Lights and doors
      {
        //Windshield wipers	424	1	if bit5 = 1 then on else off
        //Rear window defrost	424	6	if bit5 = 1 then on else off

        if ((d[0]& 4)!=0) //headlight
        { // ON
          StandardMetrics.ms_v_env_headlights->SetValue(true);
        }
        else
        {
          StandardMetrics.ms_v_env_headlights->SetValue(false);
        }

        if ((d[0]& 16)!=0) //Fog lights rear
        {// ON
          m_v_env_rearfog->SetValue(true);
        }
        else
        {
          m_v_env_rearfog->SetValue(false);
        }

        if ((d[0]& 8)!=0) //Fog lights front
        { // ON
          m_v_env_frontfog->SetValue(true);
        }
        else
        {
          m_v_env_frontfog->SetValue(false);
        }

        if ((d[1]& 4)!=0)// Highbeam
        { // ON
          m_v_env_highbeam->SetValue(true);
        }
        else
        {
          m_v_env_highbeam->SetValue(false);
        }

        if ((d[1]& 1)!=0)// Flash lights right
        { // ON
          m_v_env_blinker_right->SetValue(true);
        }
        else
        {
          m_v_env_blinker_right->SetValue(false);
        }

        if ((d[1]& 2)!=0) //Flash lights left
        { // ON
          m_v_env_blinker_left->SetValue(true);
        }
        else
        {
          m_v_env_blinker_left->SetValue(false);
        }

        if ((d[1]& 32)!=0)  //Headlight
        {
		        ESP_LOGI(TAG, "Headlight2 on");
        }
        else
        {

        }

        if ((d[1]& 64)!=0)
        { //Parkinglight
		      ESP_LOGI(TAG, "Parkinglight on");
        }
        else
        {

        }

        if ((d[2]& 1)!=0) //any Door + trunk
        { //OPEN
            //StandardMetrics.ms_v_door_fr->SetValue(true);
        }
        else
        {
              //StandardMetrics.ms_v_door_fr->SetValue(false);
        }


        if ((d[2]& 2)!=0) //LHD Driver Door
        { //OPEN
          StandardMetrics.ms_v_door_fl->SetValue(true);
        }
        else
        {
          StandardMetrics.ms_v_door_fl->SetValue(false);
        }

        if ((d[2]& 128)!=0) //??
        {
        }
        else
        {
        }
        break;
      }

    case 0x6e1: // Battery temperatures and voltages E1
    {
      if ((d[0]>=1)&&(d[0]<=12))
        {
          int idx = ((int)d[0]<<1)-2;
          BmsSetCellTemperature(idx,d[2] - 50);
          BmsSetCellTemperature(idx+1,d[3] - 50);
          BmsSetCellVoltage(idx,(((d[4]*256.0+d[5])/200.0)+2.1));
          BmsSetCellVoltage(idx+1,(((d[6]*256.0+d[7])/200.0)+2.1));
        }
    break;
    }

    case 0x6e2: // Battery temperatures and voltages E2
    {
        if ((d[0]>=1)&&(d[0]<=12))
          {
            int idx = ((int)d[0]<<1)+22;
            if (d[0]==12)
              {
                BmsSetCellTemperature(idx-1,d[1] - 50);
              }
              else
                {
                  BmsSetCellTemperature(idx,d[1] - 50);
                }
            if((d[0]!=6) && (d[0]<6))
              {
                BmsSetCellTemperature(idx+1,d[2] - 50);
              }
              else if ((d[0]!=6) && (d[0]>6) && (d[0]!=12))
                {
                  BmsSetCellTemperature(idx-1,d[2] - 50);
                }
        }
        if ((d[0]>=1)&&(d[0]<=12))
          {
            int idx = ((int)d[0]<<1)+22;
            BmsSetCellVoltage(idx,(((d[4]*256.0+d[5])/200.0)+2.1));
            BmsSetCellVoltage(idx+1,(((d[6]*256.0+d[7])/200.0)+2.1));
          }
    break;
    }

    case 0x6e3: // Battery temperatures and voltages E3
    {
        if ((d[0]>=1)&&(d[0]<=12))
          {
            if((d[0]!=6) && (d[0]<6))
              {
                int idx = ((int)d[0]<<1)+44;
                BmsSetCellTemperature(idx,d[1] - 50);
                BmsSetCellTemperature(idx+1,d[2] - 50);
              }
              else if ((d[0]!=12) && (d[0]>6))
                {
                  int idx = ((int)d[0]<<1)+42;
                  BmsSetCellTemperature(idx,d[1] - 50);
                  BmsSetCellTemperature(idx+1,d[2] - 50);
                }
          }

            if ((d[0]>=1)&&(d[0]<=12))
              {
                if((d[0]!=6) && (d[0]<6))
                  {
                    int idx = ((int)d[0]<<1)+46;
                    BmsSetCellVoltage(idx,(((d[4]*256.0+d[5])/200.0)+2.1));
                    BmsSetCellVoltage(idx+1,(((d[6]*256.0+d[7])/200.0)+2.1));
                  }
                  else if ((d[0]!=12) && (d[0]>6))
                    {
                      int idx = ((int)d[0]<<1)+44;
                      BmsSetCellVoltage(idx,(((d[4]*256.0+d[5])/200.0)+2.1));
                      BmsSetCellVoltage(idx+1,(((d[6]*256.0+d[7])/200.0)+2.1));
                    }
              }
    break;
    }

    case 0x6e4: // Battery voltages E4
    {
        if ((d[0]>=1)&&(d[0]<=12))
            {
              if((d[0]!=6) && (d[0]<6))
                {
                  int idx = ((int)d[0]<<1)+66;
                  BmsSetCellVoltage(idx,(((d[4]*256.0+d[5])/200.0)+2.1));
                  BmsSetCellVoltage(idx+1,(((d[6]*256.0+d[7])/200.0)+2.1));
                }
                else if ((d[0]!=12) && (d[0]>6))
                  {
                    int idx = ((int)d[0]<<1)+64;
                    BmsSetCellVoltage(idx,(((d[4]*256.0+d[5])/200.0)+2.1));
                    BmsSetCellVoltage(idx+1,(((d[6]*256.0+d[7])/200.0)+2.1));
                  }
            }
    break;
    }

  /*  case 0x762:
    {
        if (d[0] == 36)
            {
              StandardMetrics.ms_v_bat_cac->SetValue(((d[3]*256)+d[4])/10.0);
              StandardMetrics.ms_v_bat_soh->SetValue(StandardMetrics.ms_v_bat_cac->AsFloat()/48);
            }
    break;
  }*/

    default:
    break;
    }



  }

void OvmsVehicleMitsubishi::Ticker1(uint32_t ticker)
  {
    if (StandardMetrics.ms_v_env_gear->AsInt() == -1)
    { //Charge state if transmission in P
      if((StandardMetrics.ms_v_charge_voltage->AsInt() > 90) && (StandardMetrics.ms_v_charge_voltage->AsInt() < 254) && (StandardMetrics.ms_v_charge_current->AsInt() < 16) && (StandardMetrics.ms_v_charge_current->AsInt() > 0) )
      {
        vehicle_charger_status(CHARGER_STATUS_CHARGING);
      }
      else if((StandardMetrics.ms_v_bat_soc->AsInt() > 91) && (StandardMetrics.ms_v_charge_voltage->AsInt() > 90) && (StandardMetrics.ms_v_charge_current->AsInt() < 1) )
      {
        vehicle_charger_status(CHARGER_STATUS_FINISHED);
      }
      else
      {
        vehicle_charger_status(CHARGER_STATUS_INTERRUPTED);
      }
      if ((StandardMetrics.ms_v_bat_current->AsInt() > 0) && (StandardMetrics.ms_v_charge_voltage->AsInt() > 260))
      {
        vehicle_charger_status(CHARGER_STATUS_QUICK_CHARGING);
      }
    }


    if (StandardMetrics.ms_v_bat_soc->AsInt() <= 10)
        {
          StandardMetrics.ms_v_bat_range_ideal->SetValue(0);
        }
      else
        {
        // Ideal range 150km with 100% capacity
        int SOH = StandardMetrics.ms_v_bat_soh->AsFloat();
        if (SOH == 0){
          SOH = 100; //ideal range as 100% else with SOH
        }
          StandardMetrics.ms_v_bat_range_ideal->SetValue((
          (StandardMetrics.ms_v_bat_soc->AsFloat()-10)*1.664)*SOH/100.0);
        }

        // Update trip data
        if (StdMetrics.ms_v_env_on->AsBool() && mi_trip_start_odo!=0)
        {
          StdMetrics.ms_v_pos_trip->SetValue( POS_ODO - mi_trip_start_odo , Kilometers);

        }

  }

void OvmsVehicleMitsubishi::Ticker10(uint32_t ticker)
  {

  }

  /**
   * Takes care of setting all the state appropriate when the car is on
   * or off. Centralized so we can more easily make on and off mirror
   * images.
   */
void OvmsVehicleMitsubishi::vehicle_mitsubishi_car_on(bool isOn)
    {

    if (isOn && !StdMetrics.ms_v_env_on->AsBool())
      {
  		// Car is ON
  		StdMetrics.ms_v_env_on->SetValue(isOn);
  		StdMetrics.ms_v_env_awake->SetValue(isOn);

  		//Reset trip variables so that they are updated as soon as they are available
  		mi_trip_start_odo = 0;
  		StdMetrics.ms_v_env_charging12v->SetValue( false );

      }
    else if(!isOn && StdMetrics.ms_v_env_on->AsBool())
      {
      // Car is OFF
    		StdMetrics.ms_v_env_on->SetValue( isOn );
    		StdMetrics.ms_v_env_awake->SetValue( isOn );
    		StdMetrics.ms_v_pos_speed->SetValue( 0 );
    	  StdMetrics.ms_v_pos_trip->SetValue( POS_ODO- mi_trip_start_odo );
    		StdMetrics.ms_v_env_charging12v->SetValue( false );
      }

    //Make sure we update the different start values as soon as we have them available
    if(isOn)
    		{
  		// Trip started. Let's save current state as soon as they are available
    		if(mi_trip_start_odo==0 && POS_ODO!=0)
    			mi_trip_start_odo = POS_ODO;	// ODO at Start of trip
    		}
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandSetChargeMode(vehicle_mode_t mode)
    {
    return NotImplemented;
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandSetChargeCurrent(uint16_t limit)
    {
    StandardMetrics.ms_v_charge_climit->SetValue(limit);

    return Success;
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandStartCharge()
    {
    StandardMetrics.ms_v_pos_speed->SetValue(0);
    StandardMetrics.ms_v_mot_rpm->SetValue(0);
    StandardMetrics.ms_v_env_awake->SetValue(false);
    StandardMetrics.ms_v_env_on->SetValue(false);
    StandardMetrics.ms_v_charge_inprogress->SetValue(true);
    StandardMetrics.ms_v_door_chargeport->SetValue(true);
    StandardMetrics.ms_v_charge_state->SetValue("charging");
    StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
    StandardMetrics.ms_v_charge_pilot->SetValue(true);
    StandardMetrics.ms_v_charge_voltage->SetValue(220);
    StandardMetrics.ms_v_charge_current->SetValue(16);

    return Success;
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandStopCharge()
    {
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_door_chargeport->SetValue(false);
    StandardMetrics.ms_v_charge_state->SetValue("done");
    StandardMetrics.ms_v_charge_substate->SetValue("stopped");
    StandardMetrics.ms_v_charge_pilot->SetValue(false);
    StandardMetrics.ms_v_charge_voltage->SetValue(0);
    StandardMetrics.ms_v_charge_current->SetValue(0);

    return Success;
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandSetChargeTimer(bool timeron, uint16_t timerstart)
    {
    return NotImplemented;
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandCooldown(bool cooldownon)
    {
    return NotImplemented;
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandWakeup()
    {
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_door_chargeport->SetValue(false);
    StandardMetrics.ms_v_charge_state->SetValue("done");
    StandardMetrics.ms_v_charge_substate->SetValue("stopped");
    StandardMetrics.ms_v_charge_pilot->SetValue(false);
    StandardMetrics.ms_v_charge_voltage->SetValue(0);
    StandardMetrics.ms_v_charge_current->SetValue(0);
    StandardMetrics.ms_v_env_on->SetValue(true);

    return Success;
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandLock(const char* pin)
    {
    StandardMetrics.ms_v_env_locked->SetValue(true);

    return Success;
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandUnlock(const char* pin)
    {
    StandardMetrics.ms_v_env_locked->SetValue(false);

    return Success;
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandActivateValet(const char* pin)
    {
    StandardMetrics.ms_v_env_valet->SetValue(true);

    return Success;
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandDeactivateValet(const char* pin)
    {
    StandardMetrics.ms_v_env_valet->SetValue(false);

    return Success;
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandHomelink(int button, int durationms)
    {
    return NotImplemented;
    }

void OvmsVehicleMitsubishi::Notify12vCritical()
      {
      }
void OvmsVehicleMitsubishi::Notify12vRecovered()
      {
      }

class OvmsVehicleMitsubishiInit
  {
    public: OvmsVehicleMitsubishiInit();
} OvmsVehicleMitsubishiInit  __attribute__ ((init_priority (9000)));

OvmsVehicleMitsubishiInit::OvmsVehicleMitsubishiInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Mitsubishi iMiEV, Citroen C-Zero, Peugeot iOn (9000)");
//Mitsubishi iMiEV, Citroen C-Zero, Peugeot iOn
  MyVehicleFactory.RegisterVehicle<OvmsVehicleMitsubishi>("MI","Trio");
  }
