/*
;    Project:       Open Vehicle Monitor System
;    Date:          2th Sept 2018
;
;    Changes:
;    1.0.0  Initial release: 05-jan-2019 - KommyKT (tested on Peugeot iOn 03-2012 LHD, 88cell)
;       - web dashboard limit modifications
;       - all 66 temp sensorfor battery temp
;       - all cell temperature stored
;       - AC charger voltage / current measure
;       - DC charger voltage / current measure
;       - BMS Command
;       - trip Command
;       - aux Command
;       - web battery status with volts and temperatures
;       - new standardised BMS usage
;       - new standardised BMS /web ui for BMS
;       - heater flow/return temperature
;       - heater generation change in web settings
;       - soh setting to 'calibrate' ideal range
;       - Energy use, and heater energy use
;       - Count charge energy on AC and DC
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

#define VERSION "0.1.4"

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
  StandardMetrics.ms_v_bat_energy_used->SetValue(0);
  StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
  memset(m_vin,0,sizeof(m_vin));

  mi_trip_start_odo = 0;
  mi_start_cdc = 0;
  mi_start_cc = 0;
  mi_est_range = 0;
  mi_QC = 0;
  mi_QC_counter = 5;
  mi_last_good_SOC = 0;
  mi_last_good_range = 0;
  mi_start_time_utc = StandardMetrics.ms_m_timeutc->AsInt();
  StandardMetrics.ms_v_env_awake->SetAutoStale(1);            //Set autostale to 1 second
  StandardMetrics.ms_v_charge_inprogress->SetAutoStale(30);    //Set autostale to 30 second

  m_v_charge_ac_kwh->SetValue(0);
  m_v_charge_dc_kwh->SetValue(0);

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

  //BMS
  BmsSetCellArrangementVoltage(88, 8);
  BmsSetCellArrangementTemperature(66, 6);

  BmsSetCellLimitsVoltage(2.52,4.9);
  BmsSetCellLimitsTemperature(-40,70);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  WebInit();
#endif

  // init commands:
  cmd_xmi = MyCommandApp.RegisterCommand("xmi", "Mitsubishi iMiEV", NULL, "", 0, 0, true);
  cmd_xmi->RegisterCommand("aux", "Aux Battery", xmi_aux, "", 0, 0, false);
  cmd_xmi->RegisterCommand("trip","Show trip info", xmi_trip, "", 0,0, false);

  // init configs:
  MyConfig.RegisterParam("xmi", "Trio", true, true);
  ConfigChanged(NULL);

  }
OvmsVehicleMitsubishi::~OvmsVehicleMitsubishi()
  {
  ESP_LOGI(TAG, "Stop Mitsubishi  vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  MyWebServer.DeregisterPage("/bms/cellmon");
#endif
  MyCommandApp.UnregisterCommand("xmi");
  }

  /**
   * ConfigChanged: reload configuration variables
   */
void OvmsVehicleMitsubishi::ConfigChanged(OvmsConfigParam* param)
  {
    ESP_LOGD(TAG, "Trio reload configuration");
    cfg_heater_old = MyConfig.GetParamValueBool("xmi", "oldheater", false);
    cfg_soh = MyConfig.GetParamValueInt("xmi", "soh", 100);
  }

void OvmsVehicleMitsubishi::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  StdMetrics.ms_v_env_awake->SetValue(true);  //If can awake modify awake variable, checked with IsStale

  switch (p_frame->MsgID)
    {
      case 0x101: //freq10 //Key status
      {
        if(d[0] == 4)
        {
          vehicle_mitsubishi_car_on(true);
        }else if(d[0] == 0)
        {
          vehicle_mitsubishi_car_on(false);
        }

      break;
      }

      case 0x208://freq50 // Brake pedal position
      {
        StandardMetrics.ms_v_env_footbrake->SetValue(((d[2] * 256 + d[3]) - 24576.0) / 640 * 100.0);
      break;
      }

      case 0x210://freq50 // Accelerator pedal position
      {
        StandardMetrics.ms_v_env_throttle->SetValue(d[2]*0.4);
      break;
      }

      case 0x231://freq50 // Brake pedal pressed?
      {
        if (d[4] == 0) {
          //Brake not pressed
        } else if (d[4] == 2) {
          /* Brake pressed*/
        }
      break;
      }

      case 0x236://freq100 // Steeering wheel
      {
        // (((d[0] * 256.0) +d[1])-4096) / 2.0  //Negative angle - right, positive angle left
        // (((d[2] * 256.0) + d[3]) - 4096) / 2.0)) //Steering wheel movement
      break;
      }

      case 0x286://freq10  // Charger/inverter temperature
      {
        StandardMetrics.ms_v_charge_temp->SetValue((float)d[3]-40);
        StandardMetrics.ms_v_inv_temp->SetValue((float)d[3]-40);
      break;
      }

      case 0x298://freq10 // Motor temperature and RPM
      {
        StandardMetrics.ms_v_mot_temp->SetValue((int)d[3]-40);
        StandardMetrics.ms_v_mot_rpm->SetValue(((d[6]*256.0)+d[7])-10000);

        if(StandardMetrics.ms_v_mot_rpm->AsInt() < 10 && StandardMetrics.ms_v_mot_rpm->AsInt() > -10)
        { // (-10 < Motor RPM > 10) Set to 0
          StandardMetrics.ms_v_mot_rpm->SetValue(0);
        }
      break;
      }

      case 0x29A://freq10 // VIN determination //6FA VIN2
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

      case 0x346://freq50 // Estimated range, Handbrake state
      {
        StandardMetrics.ms_v_bat_range_est->SetValue(d[7]);
        mi_est_range = d[7];
        if((mi_QC != 0) && (mi_est_range == 255))
        {
          StandardMetrics.ms_v_charge_inprogress->SetValue(true);
        }


        if((d[4] & 32) == 0)
        {
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          StandardMetrics.ms_v_env_parktime->SetValue(0);
          break;
        }
        StandardMetrics.ms_v_env_handbrake->SetValue(true);
      break;
      }

      case 0x373://freq100 // Main Battery volt and current
      {  // 1kwh -» 3600000Ws -- freq100 -» 360000000
        StandardMetrics.ms_v_bat_current->SetValue((((((d[2]*256.0)+d[3]))-32768))/100.0,Amps);
        StandardMetrics.ms_v_bat_voltage->SetValue((d[4]*256.0+d[5])/10.0,Volts);
        StandardMetrics.ms_v_bat_power->SetValue((StandardMetrics.ms_v_bat_voltage->AsFloat(0,Volts)*StandardMetrics.ms_v_bat_current->AsFloat(0,Amps))/1000.0*-1.0,kW);
        v_c_power_dc->SetValue(StandardMetrics.ms_v_bat_power->AsFloat()*-1.0,kW);

        // energy usage
        if(StandardMetrics.ms_v_bat_power->AsInt() < 0)
          {
            StandardMetrics.ms_v_bat_energy_recd->SetValue((StandardMetrics.ms_v_bat_energy_recd->AsFloat()+(StandardMetrics.ms_v_bat_power->AsFloat()/-360000.0)));
          }
          else
          {
            StandardMetrics.ms_v_bat_energy_used->SetValue((StandardMetrics.ms_v_bat_energy_used->AsFloat()+(StandardMetrics.ms_v_bat_power->AsFloat()/360000.0)));
          }

        if(StandardMetrics.ms_v_charge_inprogress->AsBool())
          {
            m_v_charge_dc_kwh->SetValue((m_v_charge_dc_kwh->AsFloat()+(StandardMetrics.ms_v_bat_power->AsFloat()/-360000.0)));
          }

        if(StandardMetrics.ms_v_env_gear->AsInt() == -1 && StandardMetrics.ms_v_bat_power->AsInt() < 0 && (mi_QC != 0))
          {
            //set battery voltage/current to charge voltage/current, when car in Park, and charging
            StandardMetrics.ms_v_charge_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsFloat());
            StandardMetrics.ms_v_charge_current->SetValue(StandardMetrics.ms_v_bat_current->AsFloat()*1.0);
          }

      break;
      }

      case 0x374://freq10 // Main Battery Soc
      {
        StandardMetrics.ms_v_bat_soc->SetValue(((int)d[1]-10)/2.0,Percentage);
      break;
      }

      case 0x384://freq10 //heating current
      {
        m_v_env_heating_amp->SetValue(d[4]/10.0);
        m_v_env_heating_watt->SetValue(m_v_env_heating_amp->AsFloat()*StandardMetrics.ms_v_bat_voltage->AsFloat());
        m_v_env_heating_kwh->SetValue(m_v_env_heating_kwh->AsFloat()+(m_v_env_heating_watt->AsFloat()/36000000));

        if (m_v_env_heating_watt->AsFloat() > 0.0){
          StandardMetrics.ms_v_env_heating->SetValue(true);
        }else{
          StandardMetrics.ms_v_env_heating->SetValue(false);
        }
        if (cfg_heater_old)
        {
          m_v_env_heating_temp_return->SetValue(((d[5] - 32) / 1.8d) - 3.0d);
          m_v_env_heating_temp_flow->SetValue(((d[6] - 32) / 1.8d) - 3.0d);
        }
        else
          {
            m_v_env_heating_temp_return->SetValue((d[5] * 0.6) - 40.0d);
            m_v_env_heating_temp_flow->SetValue((d[6] * 0.6) - 40.0d);
          }

        m_v_env_ac_amp->SetValue((d[0]*256.0+d[1])/1000.0);
        m_v_env_ac_watt->SetValue(m_v_env_ac_amp->AsFloat()*StandardMetrics.ms_v_bat_voltage->AsFloat());
        m_v_env_ac_kwh->SetValue(m_v_env_ac_kwh->AsFloat()+(m_v_env_ac_watt->AsFloat()/36000000));

      break;
      }

      case 0x389://freq10 // Charger voltage and current
      {
        if(mi_QC == 0)
        {
          StandardMetrics.ms_v_charge_voltage->SetValue(d[1]*1.0,Volts);
          StandardMetrics.ms_v_charge_current->SetValue(d[6]/10.0,Amps);
        }

        v_c_power_ac->SetValue((StandardMetrics.ms_v_charge_voltage->AsFloat()*StandardMetrics.ms_v_charge_current->AsFloat())/1000, kW);
        if(StdMetrics.ms_v_charge_voltage->AsInt()>0 && StdMetrics.ms_v_charge_current->AsFloat()>0.0){
            StandardMetrics.ms_v_charge_inprogress->SetValue(true);
            StandardMetrics.ms_v_charge_kwh->SetValue(StandardMetrics.ms_v_charge_kwh->AsFloat()+(v_c_power_ac->AsFloat()/36000.0));
            m_v_charge_ac_kwh->SetValue(StandardMetrics.ms_v_charge_kwh->AsFloat());
        }
      break;
      }

      case 0x3A4://freq10// Climate console
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
          //in the middle
        }else if (((int)d[0]<< 4) > 112)
        {
          // Warm
        }
        else if (((int)d[0]<< 4) < 112)
        {
          //Cold
        }

      if (((int)d[0]>>7) == 1)
      {
       // AC on
       StandardMetrics.ms_v_env_hvac->SetValue(true);
     }else{
       StandardMetrics.ms_v_env_hvac->SetValue(false);
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

    case 0x412://freq10 // Speed and odometer
    {
      if (d[1]>200)
        StandardMetrics.ms_v_pos_speed->SetValue((int)d[1]-255.0,Kph);
      else
        StandardMetrics.ms_v_pos_speed->SetValue(d[1]);

        StandardMetrics.ms_v_pos_odometer->SetValue(((int)d[2]<<8)+((int)d[3]<<8)+d[4],Kilometers);
    break;
    }

    case 0x418://freq50 // Transmissin state determination
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

    case 0x424://freq25 // Lights and doors
      {
        //Windshield wipers	424	1	if bit5 = 1 then on else off
        //Rear window defrost	424	6	if bit5 = 1 then on else off

        if ((d[0] & 4)!=0) //headlight
        { // ON
          StandardMetrics.ms_v_env_headlights->SetValue(true);
        }
        else
        {
          StandardMetrics.ms_v_env_headlights->SetValue(false);
        }

        if ((d[0] & 8)!=0) //Fog lights front
        { // ON
          m_v_env_frontfog->SetValue(true);
        }
        else
        {
          m_v_env_frontfog->SetValue(false);
        }

        if ((d[0] & 16)!=0) //Fog lights rear
        {// ON
          m_v_env_rearfog->SetValue(true);
        }
        else
        {
          m_v_env_rearfog->SetValue(false);
        }

        if ((d[1] & 1)!=0)// Blinker right
        { // ON
          m_v_env_blinker_right->SetValue(true);
        }
        else
        {
          m_v_env_blinker_right->SetValue(false);
        }

        if ((d[1] & 2)!=0) //Blinker left
        { // ON
          m_v_env_blinker_left->SetValue(true);
        }
        else
        {
          m_v_env_blinker_left->SetValue(false);
        }

        if ((d[1] & 4)!=0)// Highbeam
        { // ON
          m_v_env_highbeam->SetValue(true);
        }
        else
        {
          m_v_env_highbeam->SetValue(false);
        }

        if ((d[1] & 32)!=0)  //Headlight
        {
		        ESP_LOGI(TAG, "Headlight2 on");
        }
        else
        {

        }

        if ((d[1] & 64)!=0)
        { //Parkinglight
		      ESP_LOGI(TAG, "Parkinglight on");
        }
        else
        {

        }

        if ((d[2] & 1)!=0) //any Door + trunk
        { //OPEN
            //StandardMetrics.ms_v_door_fr->SetValue(true);
        }
        else
        {
              //StandardMetrics.ms_v_door_fr->SetValue(false);
        }


        if ((d[2] & 2)!=0) //LHD Driver Door
        { //OPEN
          StandardMetrics.ms_v_door_fl->SetValue(true);
        }
        else
        {
          StandardMetrics.ms_v_door_fl->SetValue(false);
        }
        if ((d[2] & 3)!=0) //??
        {
        }
        if ((d[2] & 128)!=0) //??
        {
        }
        else
        {
        }
        if ((d[2] & 192)!=0) //??
        {
        }
        break;
      }

    case 0x6e1://freq25 // Battery temperatures and voltages E1
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

    case 0x6e2://freq25 // Battery temperatures and voltages E2
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

    case 0x6e3://freq25 // Battery temperatures and voltages E3
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

    case 0x6e4://freq25 // Battery voltages E4
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

    default:
    break;
    }



  }

void OvmsVehicleMitsubishi::Ticker1(uint32_t ticker)
{
  // battery temp from battery pack avg
  StdMetrics.ms_v_bat_temp->SetValue(StdMetrics.ms_v_bat_pack_tavg->AsFloat());

  if(StandardMetrics.ms_v_charge_inprogress->IsStale())
  { //clear all charge variable if not charging
    mi_QC = 0;
    StandardMetrics.ms_v_charge_voltage->SetValue(0.0);
    StandardMetrics.ms_v_charge_current->SetValue(0.0);
    StandardMetrics.ms_v_bat_current->SetValue(0);
    StandardMetrics.ms_v_bat_power->SetValue(0);
    v_c_power_ac->SetValue(0);
    v_c_power_dc->SetValue(0);
    v_c_efficiency->SetValue(0);
  }

  //Check only if 'transmission in park
  if (StandardMetrics.ms_v_env_gear->AsInt() == -1)
  {
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

////////////////////////////////////////////////////////////////////////
// Charge state determination
////////////////////////////////////////////////////////////////////////

  if ((mi_QC != 0) || ((StandardMetrics.ms_v_charge_current->AsInt() != 0) && (StandardMetrics.ms_v_charge_voltage->AsInt() != 0)))
  {
    StandardMetrics.ms_v_env_charging12v->SetValue(true);
    if (! StandardMetrics.ms_v_charge_pilot->AsBool())
    {
      // Charge has started
      m_v_charge_ac_kwh->SetValue(0);
      m_v_charge_dc_kwh->SetValue(0);
      StandardMetrics.ms_v_charge_kwh->SetValue(0.0,kWh);
      mi_chargekwh = 0;      // Reset charge kWh
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_charge_pilot->SetValue(true);
      StandardMetrics.ms_v_door_chargeport->SetValue(true);
    //  StandardMetrics.ms_v_charge_state->SetValue("charging");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      if (mi_QC != 0)
        {
          StandardMetrics.ms_v_charge_mode->SetValue("Quickcharge");
          StandardMetrics.ms_v_charge_type->SetValue("ChaDeMo");
          StandardMetrics.ms_v_charge_climit->SetValue(125);
        }
      else
        {
          StandardMetrics.ms_v_charge_mode->SetValue("Standard");
          StandardMetrics.ms_v_charge_type->SetValue("Type1");
          StandardMetrics.ms_v_charge_climit->SetValue(16);
        }
    }
    else
        {
          StandardMetrics.ms_v_charge_state->SetValue("charging");

          // if charge and DC current is negative cell balancing is active. If charging battery current is negative, discharge is positive
          if((StandardMetrics.ms_v_bat_current->AsFloat() < 0) && (mi_QC == 0) && StandardMetrics.ms_v_bat_soc->AsInt() > 92)
          {
            StandardMetrics.ms_v_charge_mode->SetValue("Balancing");
            StandardMetrics.ms_v_charge_state->SetValue("topoff");
          }

        }
    }else if ((StandardMetrics.ms_v_charge_current->AsInt() == 0) && (StandardMetrics.ms_v_charge_voltage->AsInt() < 100))
          {
          // Car is not charging

          if (StandardMetrics.ms_v_charge_pilot->AsBool())
            {
            // Charge has stopped
              StandardMetrics.ms_v_charge_inprogress->SetValue(false);
              StandardMetrics.ms_v_charge_pilot->SetValue(false);
              StandardMetrics.ms_v_door_chargeport->SetValue(false);
              StandardMetrics.ms_v_env_charging12v->SetValue(false);
              StandardMetrics.ms_v_charge_type->SetValue("None");
              if (StandardMetrics.ms_v_bat_soc->AsInt() < 92 && mi_QC == 0)
                {
                  // Assume charge was interrupted
                  StandardMetrics.ms_v_charge_state->SetValue("stopped");
                  StandardMetrics.ms_v_charge_substate->SetValue("interrupted");
                }
                else
                  {
                    // Charge done
                    StandardMetrics.ms_v_charge_state->SetValue("done");
                    StandardMetrics.ms_v_charge_substate->SetValue("scheduledstop");
                  }
              v_c_power_ac->SetValue(0.0);  // Reset charge power meter
              v_c_power_dc->SetValue(0.0);  // Reset charge power meter
              m_v_charge_ac_kwh->SetValue(StandardMetrics.ms_v_charge_kwh->AsFloat()); //save charge kwh to variable
              StandardMetrics.ms_v_charge_kwh->SetValue(0); //reset charge kwh variable
              StandardMetrics.ms_v_charge_current->SetValue(0.0); //Reset charge current
              StandardMetrics.ms_v_charge_climit->SetValue(0.0); //Reset charge limit
              StandardMetrics.ms_v_charge_mode->SetValue("None"); //Set charge mode to NONE
              StandardMetrics.ms_v_bat_current->SetValue(0.0);  //Set battery current to 0
            }
          }

    //Efficiency calculation AC/DC
    if((v_c_power_dc->AsInt() <= 0) || (v_c_power_ac->AsInt() == 0) )
      {
        v_c_efficiency->SetValue(0,Percentage);
      }
      else
        {
          v_c_efficiency->SetValue((v_c_power_dc->AsFloat()/v_c_power_ac->AsFloat())*100,Percentage);
        }
  } //Car in P "if" close

    //Set soh value from settings
    StandardMetrics.ms_v_bat_soh->SetValue(cfg_soh);

    //  Trip consumption
    if(StandardMetrics.ms_v_env_gear->AsInt() != -1)
        {
      // Trip started. Let's save current state as soon as they are available
        if(mi_trip_start_odo==0 && POS_ODO!=0)
          mi_trip_start_odo = POS_ODO;	// ODO at Start of trip
        }

    // Update trip data
    if (StdMetrics.ms_v_env_on->AsBool() && mi_trip_start_odo!=0)
      {
        StdMetrics.ms_v_pos_trip->SetValue( POS_ODO - mi_trip_start_odo , Kilometers);
      }
      else
        {
          StandardMetrics.ms_v_bat_consumption->SetValue(0);
        }

    if (StandardMetrics.ms_v_bat_soc->AsFloat() <= 10)
        {
          StandardMetrics.ms_v_bat_range_ideal->SetValue(0);
        }
      else
        {
        // Ideal range
        int SOH = StandardMetrics.ms_v_bat_soh->AsFloat();
        if (SOH == 0){
          SOH = 100; //ideal range as 100% else with SOH
        }
          StandardMetrics.ms_v_bat_range_ideal->SetValue((
          (StandardMetrics.ms_v_bat_soc->AsFloat()-10)*1.4444)*SOH/100.0); //150km 1.664; 125km 1.3889; 130km 1.4444; 135km 1.5
        }

        StandardMetrics.ms_v_bat_cac->SetValue(48.0*(StandardMetrics.ms_v_bat_soh->AsFloat()/100));

    // Update trip data
    if (StdMetrics.ms_v_env_on->AsBool() && mi_trip_start_odo!=0)
      {
        StdMetrics.ms_v_pos_trip->SetValue( POS_ODO - mi_trip_start_odo , Kilometers);
      }
      else
        {
          StandardMetrics.ms_v_bat_consumption->SetValue(0);
        }

    if( StdMetrics.ms_v_pos_trip->AsFloat(Kilometers)>0 )
      m_v_trip_consumption1->SetValue( ((StdMetrics.ms_v_bat_energy_used->AsFloat(kWh)-StdMetrics.ms_v_bat_energy_recd->AsFloat(kWh)) * 100) / StdMetrics.ms_v_pos_trip->AsFloat(Kilometers) );
    if( StdMetrics.ms_v_bat_energy_used->AsFloat(kWh)>0 )
    	m_v_trip_consumption2->SetValue( StdMetrics.ms_v_pos_trip->AsFloat(Kilometers) / (StdMetrics.ms_v_bat_energy_used->AsFloat(kWh)-StdMetrics.ms_v_bat_energy_recd->AsFloat(kWh)));

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
  		//Reset trip variables so that they are updated as soon as they are available
  		mi_trip_start_odo = 0;
  		StdMetrics.ms_v_env_charging12v->SetValue( false );
      //Reset energy calculation
      StdMetrics.ms_v_bat_energy_recd->SetValue(0);
      StdMetrics.ms_v_bat_energy_used->SetValue(0);
      m_v_env_heating_kwh->SetValue(0);

      }
    else if(!isOn && StdMetrics.ms_v_env_on->AsBool())
      {
      // Car is OFF
    		StdMetrics.ms_v_env_on->SetValue( isOn );
    		StdMetrics.ms_v_pos_speed->SetValue( 0 );
    	  StdMetrics.ms_v_pos_trip->SetValue( POS_ODO- mi_trip_start_odo );
    		StdMetrics.ms_v_env_charging12v->SetValue( false );
      }
    }

OvmsVehicle::vehicle_command_t OvmsVehicleMitsubishi::CommandStat(int verbosity, OvmsWriter* writer)
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

      const char* charge_kWh_ac = m_v_charge_ac_kwh->AsUnitString("-", Native, 3).c_str();
      if (*charge_kWh_ac != '-')
        writer->printf("AC charge: %s kWh\n", charge_kWh_ac);

      const char* charge_kWh_dc = m_v_charge_dc_kwh->AsUnitString("-", Native, 3).c_str();
      if (*charge_kWh_dc != '-')
        writer->printf("DC charge: %s kWh\n", charge_kWh_dc);

      return Success;
    }

class OvmsVehicleMitsubishiInit
  {
    public: OvmsVehicleMitsubishiInit();
} OvmsVehicleMitsubishiInit  __attribute__ ((init_priority (9000)));

OvmsVehicleMitsubishiInit::OvmsVehicleMitsubishiInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Mitsubishi iMiEV, Citroen C-Zero, Peugeot iOn (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleMitsubishi>("MI","Trio");
  }
