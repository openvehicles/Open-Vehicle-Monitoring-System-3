/*
;    Project:       Open Vehicle Monitor System
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
;    1.0.1
;       - reworked voltage and temp
;       - added support for 80 cell cars
;       - ideal range from settings
;       - disable bms notifications by default
;       - add 80 cell car support
;       - VIN command -> show VIN info and Car manufacturer, and battery cell numbers from VIN
;    1.0.2
;       - trip since last charger: xmi tripch
;       - trip since power off: xmi trip
;       - last charge: xmi charge
;    1.0.3
;       - add support for regen brake light
;    1.0.4
;       - Commands fix and add duration for last trip
;    1.0.5
;       - efficiency flooding bug in metrics trace fix
;       - New charge detection for Standard Charger
;         -Pid 0x286
;       - New charge detection for Quick ChargerStatus
;         -Pid 0x697
;     1.0.6
;       - Get CAC from car (BETA), -> remove user settings
;       - Real/displayed SOC,
;       - Charge/discharge max current
;         - Pid: 0x761 request - 0x762 response BMU ECU
;       - Ideal range calculation 16.1kWh/100km
;
;    (C) 2011         Michael Stegen / Stegen Electronics
;    (C) 2011-2018    Mark Webb-Johnson
;    (C) 2011         Sonny Chen @ EPRO/DX
;    (C) 2018-2020    Tamás Kovács (KommyKT)
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

#define VERSION "1.0.2"

static const char *TAG = "v-mitsubishi";

OvmsVehicleMitsubishi::OvmsVehicleMitsubishi()
{
  ESP_LOGI(TAG, "Start Mitsubishi iMiev, Citroen C-Zero, Peugeot iOn vehicle module");

  StandardMetrics.ms_v_env_parktime->SetValue(0);
  StandardMetrics.ms_v_charge_type->SetValue("None");
  StandardMetrics.ms_v_bat_energy_used->SetValue(0);
  StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
  memset(m_vin, 0, sizeof(m_vin));

  mi_start_time_utc = StandardMetrics.ms_m_timeutc->AsInt();
  StandardMetrics.ms_v_charge_inprogress->SetAutoStale(30);    //Set autostale to 30 second

  ms_v_charge_ac_kwh->SetValue(0);
  ms_v_charge_dc_kwh->SetValue(0);

  ms_v_trip_park_energy_recd->SetValue(0);
  ms_v_trip_park_energy_used->SetValue(0);
  ms_v_trip_park_heating_kwh->SetValue(0);
  ms_v_trip_park_ac_kwh->SetValue(0);

  set_odo = false;

  v_c_efficiency->SetValue(0);

  if(POS_ODO > 0)
  {
    has_odo = true;

    mi_charge_trip_counter.Reset(POS_ODO);

    ms_v_trip_charge_soc_start->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());
    ms_v_trip_charge_soc_stop->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());
  }
  else
  {
    has_odo = false;
  }

  // reset charge counter

  ms_v_trip_charge_energy_recd->SetValue(0);
  ms_v_trip_charge_energy_used->SetValue(0);
  ms_v_trip_charge_heating_kwh->SetValue(0);
  ms_v_trip_charge_ac_kwh->SetValue(0);


  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

  //BMS
  //Disable BMS alerts by default
  MyConfig.GetParamValueBool("vehicle", "bms.alerts.enabled", false);

  //80 or 88 cell car.
  cfg_newcell =  MyConfig.GetParamValueBool("xmi", "newcell", false);

  if (cfg_newcell)
  {
    BmsSetCellArrangementVoltage(80, 8);
    BmsSetCellArrangementTemperature(60, 6);
  }
  else
  {
    BmsSetCellArrangementVoltage(88, 8);
    BmsSetCellArrangementTemperature(66, 6);
  }

  BmsSetCellLimitsVoltage(2.52, 4.9);
  BmsSetCellLimitsTemperature(-30, 60);

  #ifdef CONFIG_OVMS_COMP_WEBSERVER
    MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
    MyWebServer.RegisterPage("/cfg/brakelight", "Brake Light control",OvmsWebServer::HandleCfgBrakelight,PageMenu_Vehicle,PageAuth_Cookie);
    WebInit();
  #endif

  // init commands:
  cmd_xmi = MyCommandApp.RegisterCommand("xmi", "Mitsubishi iMiEV");
  cmd_xmi->RegisterCommand("aux", "Aux Battery", xmi_aux);
  cmd_xmi->RegisterCommand("trip", "Show trip info since last parked", xmi_trip_since_parked);
  cmd_xmi->RegisterCommand("tripch", "Show trip info since last charger", xmi_trip_since_charge);
  cmd_xmi->RegisterCommand("vin", "Show vin info", xmi_vin);
  cmd_xmi->RegisterCommand("charge", "Show last charge info", xmi_charge);

  // init configs:
  MyConfig.RegisterParam( "xmi", "Trio", true, true);
  ConfigChanged(NULL);

}

OvmsVehicleMitsubishi::~OvmsVehicleMitsubishi()
{
  ESP_LOGI(TAG, "Stop Mitsubishi  vehicle module");

  #ifdef CONFIG_OVMS_COMP_WEBSERVER
    MyWebServer.DeregisterPage("/bms/cellmon");
    MyWebServer.DeregisterPage("/cfg/brakelight");
  #endif
  MyCommandApp.UnregisterCommand("xmi");
}

  /**
   * ConfigChanged: reload configuration variables
   */
void OvmsVehicleMitsubishi::ConfigChanged(OvmsConfigParam* param)
{
  cfg_heater_old = MyConfig.GetParamValueBool("xmi", "oldheater", false);
  cfg_newcell =  MyConfig.GetParamValueBool("xmi", "newcell", false);
  if (cfg_newcell){
    BmsSetCellArrangementVoltage(80, 8);
    BmsSetCellArrangementTemperature(60, 6);
  }
  else{
    BmsSetCellArrangementVoltage(88, 8);
    BmsSetCellArrangementTemperature(66, 6);
  }
}

void OvmsVehicleMitsubishi::IncomingFrameCan1(CAN_frame_t* p_frame)
{
  uint8_t *d = p_frame->data.u8;

  switch ( p_frame->MsgID )
    {
      case 0x101: //freq10 //Key status
      {
        if (d[0] == 4)
        {
          vehicle_mitsubishi_car_on(true);
        }else if (d[0] ==0)
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
        StandardMetrics.ms_v_env_throttle->SetValue((d[2] * 100.0) / 255.0);
      break;
      }

      case 0x231://freq50 // Brake pedal pressed?
      {
        if (d[4] ==0) {
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
        StandardMetrics.ms_v_charge_temp->SetValue((float)d[3] - 40);
        StandardMetrics.ms_v_inv_temp->SetValue((float)d[3] - 40);

        //charger detection
        if(d[0] == 14  && d[1] == 116 ) // charger connected
        {
          mi_SC = true;
          if(d[2] == 100) //charging (d[2] <= 99 prepare charge?)
          {
            StandardMetrics.ms_v_charge_type->SetValue("Standard");
            StandardMetrics.ms_v_charge_mode->SetValue("Type1");
            StandardMetrics.ms_v_charge_climit->SetValue("14");
            StandardMetrics.ms_v_charge_state->SetValue("charging");
          }
        }
        else
        {
          mi_SC = false;
        }

      break;
      }

      case 0x298://freq10 // Motor temperature and RPM
      {
        StandardMetrics.ms_v_mot_temp->SetValue((int)d[3] - 40 );
        StandardMetrics.ms_v_mot_rpm->SetValue(((d[6] * 256.0) + d[7]) - 10000);

        if ( StandardMetrics.ms_v_mot_rpm->AsInt() < 10 && StandardMetrics.ms_v_mot_rpm->AsInt() > -10)
        { // (-10 < Motor RPM > 10) Set to 0
          StandardMetrics.ms_v_mot_rpm->SetValue(0);
        }
      break;
      }

      case 0x29A://freq10 // VIN determination //6FA VIN2
      {
        if (d[0]==0)
          for (int k = 0; k < 7; k++) m_vin[k] = d[k+1];
          else if (d[0] == 1)
          for (int k = 0; k < 7; k++) m_vin[k+7] = d[k+1];
          else if ( d[0] == 2 )
          {
            m_vin[14] = d[1];
            m_vin[15] = d[2];
            m_vin[16] = d[3];
            m_vin[17] = 0;
            if ( ( m_vin[0] !=0) && ( m_vin[7] !=0) )
            {
              StandardMetrics.ms_v_vin->SetValue(m_vin);
            }
          }
/*
          if (m_vin[0] == 'V' && m_vin[1] == 'F' && m_vin[7] == 'Y'){
            BmsSetCellArrangementVoltage(80, 8);
            BmsSetCellArrangementTemperature(60, 6);
          }
*/
      break;
      }

      case 0x346://freq50 // Estimated range, Handbrake state
      {
        StandardMetrics.ms_v_bat_range_est->SetValue(d[7]);
        if ((d[4] & 32) == 0)
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
        StandardMetrics.ms_v_bat_current->SetValue((((((d[2] * 256.0) + d[3])) - 32768)) / 100.0, Amps);
        StandardMetrics.ms_v_bat_voltage->SetValue((d[4] * 256.0 + d[5]) / 10.0, Volts);
        StandardMetrics.ms_v_bat_power->SetValue((StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts) * StandardMetrics.ms_v_bat_current->AsFloat(0, Amps)) / 1000.0 * -1.0, kW);
        v_c_power_dc->SetValue( StandardMetrics.ms_v_bat_power->AsFloat() * -1.0, kW );
        if (!StandardMetrics.ms_v_charge_pilot->AsBool())
        {
          if ( StandardMetrics.ms_v_bat_power->AsInt() < 0)
            {
              StandardMetrics.ms_v_bat_energy_recd->SetValue((StandardMetrics.ms_v_bat_energy_recd->AsFloat() + (StandardMetrics.ms_v_bat_power->AsFloat() / -360000.0)));
              ms_v_trip_park_energy_recd->SetValue((ms_v_trip_park_energy_recd->AsFloat() + (StandardMetrics.ms_v_bat_power->AsFloat() / -360000.0)));
              ms_v_trip_charge_energy_recd->SetValue((ms_v_trip_charge_energy_recd->AsFloat() + (StandardMetrics.ms_v_bat_power->AsFloat() / -360000.0)));
            }
            else
            {
              StandardMetrics.ms_v_bat_energy_used->SetValue( ( StandardMetrics.ms_v_bat_energy_used->AsFloat()+(StandardMetrics.ms_v_bat_power->AsFloat() / 360000.0)));
              ms_v_trip_park_energy_used->SetValue((ms_v_trip_park_energy_used->AsFloat() + (StandardMetrics.ms_v_bat_power->AsFloat() / 360000.0)));
              ms_v_trip_charge_energy_used->SetValue((ms_v_trip_charge_energy_used->AsFloat() + (StandardMetrics.ms_v_bat_power->AsFloat() / 360000.0)));
            }
        }
        else
        {
          // energy usage
          if ( StandardMetrics.ms_v_bat_power->AsInt() < 0)
          {
            StandardMetrics.ms_v_bat_energy_recd->SetValue((StandardMetrics.ms_v_bat_energy_recd->AsFloat() + (StandardMetrics.ms_v_bat_power->AsFloat() / -360000.0)));
          }
          else
          {
            StandardMetrics.ms_v_bat_energy_used->SetValue((StandardMetrics.ms_v_bat_energy_used->AsFloat() + (StandardMetrics.ms_v_bat_power->AsFloat() / 360000.0)));
          }
        }


        if (StandardMetrics.ms_v_charge_inprogress->AsBool())
        {
          ms_v_charge_dc_kwh->SetValue((ms_v_charge_dc_kwh->AsFloat() + (StandardMetrics.ms_v_bat_power->AsFloat() / -360000.0)));
        }

        if (StandardMetrics.ms_v_env_gear->AsInt() == -1 && StandardMetrics.ms_v_bat_power->AsInt() < 0 && (mi_QC == true))
        {
          //set battery voltage/current to charge voltage/current, when car in Park, and charging
          StandardMetrics.ms_v_charge_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsFloat());
          StandardMetrics.ms_v_charge_current->SetValue(StandardMetrics.ms_v_bat_current->AsFloat() * 1.0);
        }

        //min power
        if (v_b_power_max->AsFloat() > StandardMetrics.ms_v_bat_power->AsFloat())
          v_b_power_max->SetValue(StandardMetrics.ms_v_bat_power->AsFloat());
        //max power
        if (v_b_power_min->AsFloat() < StandardMetrics.ms_v_bat_power->AsFloat())
          v_b_power_min->SetValue(StandardMetrics.ms_v_bat_power->AsFloat());

      break;
      }

      case 0x374://freq10 // Main Battery Soc
      {
        StandardMetrics.ms_v_bat_soc->SetValue(((int)d[1] - 10 ) / 2.0, Percentage);
      break;
      }

      case 0x384://freq10 //heating current
      {
        ms_v_env_heating_amp->SetValue(d[4] / 10.0);
        ms_v_env_heating_watt->SetValue(ms_v_env_heating_amp->AsFloat() * StandardMetrics.ms_v_bat_voltage->AsFloat());

        ms_v_trip_park_heating_kwh->SetValue(ms_v_trip_park_heating_kwh->AsFloat() + (ms_v_env_heating_watt->AsFloat() / 36000000));
        ms_v_trip_charge_heating_kwh->SetValue(ms_v_trip_charge_heating_kwh->AsFloat() + (ms_v_env_heating_watt->AsFloat() / 36000000));

        if (ms_v_env_heating_watt->AsFloat() > 0.0)
        {
          StandardMetrics.ms_v_env_heating->SetValue(true);
        }
        else
        {
          StandardMetrics.ms_v_env_heating->SetValue(false);
        }

        if ( cfg_heater_old )
        {
          ms_v_env_heating_temp_return->SetValue(((d[5] - 32) / 1.8d) - 3.0d);
          ms_v_env_heating_temp_flow->SetValue(((d[6] - 32) / 1.8d) - 3.0d);
        }
        else
        {
          ms_v_env_heating_temp_return->SetValue((d[5] * 0.6) - 40.0d);
          ms_v_env_heating_temp_flow->SetValue((d[6] * 0.6) - 40.0d);
        }

        ms_v_env_ac_amp->SetValue((d[0] * 256.0 + d[1]) / 1000.0);
        ms_v_env_ac_watt->SetValue(ms_v_env_ac_amp->AsFloat() * StandardMetrics.ms_v_bat_voltage->AsFloat());

        ms_v_trip_park_ac_kwh->SetValue(ms_v_trip_park_ac_kwh->AsFloat() + (ms_v_env_ac_watt->AsFloat() / 36000000));
        ms_v_trip_charge_ac_kwh->SetValue(ms_v_trip_charge_ac_kwh->AsFloat() + (ms_v_env_ac_watt->AsFloat() / 36000000));

      break;
      }

      case 0x389://freq10 // Charger voltage and current
      {
        if (mi_QC == false && mi_SC == true)
        {
          StandardMetrics.ms_v_charge_voltage->SetValue(d[1] * 1.0, Volts);
          StandardMetrics.ms_v_charge_current->SetValue(d[6] / 10.0, Amps);
        }

        v_c_power_ac->SetValue((StandardMetrics.ms_v_charge_voltage->AsFloat() * StandardMetrics.ms_v_charge_current->AsFloat()) / 1000, kW);
        if ( (StdMetrics.ms_v_charge_voltage->AsInt() > 0) && (StdMetrics.ms_v_charge_current->AsFloat() > 0.0) )
        {
          StandardMetrics.ms_v_charge_inprogress->SetValue(true);
          StandardMetrics.ms_v_charge_kwh->SetValue(StandardMetrics.ms_v_charge_kwh->AsFloat() + (v_c_power_ac->AsFloat() / 36000.0));
          ms_v_charge_ac_kwh->SetValue(StandardMetrics.ms_v_charge_kwh->AsFloat());
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
        if (((int)d[0] << 4 ) == 112)
        {
          //in the middle
        }else if (((int)d[0] << 4 ) > 112)
        {
          // Warm
        }
        else if (((int)d[0] << 4) < 112)
        {
          //Cold
        }

      if (((int)d[0] >> 7) == 1)
      {
        // AC on
        StandardMetrics.ms_v_env_hvac->SetValue(true);
      }
      else
      {
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
      if (d[1] > 200)
        StandardMetrics.ms_v_pos_speed->SetValue((int)d[1] - 255.0, Kph);
      else
        StandardMetrics.ms_v_pos_speed->SetValue(d[1]);

        CalculateAcceleration();

        StandardMetrics.ms_v_pos_odometer->SetValue(((int)d[2] << 8 ) + ((int)d[3] << 8) + d[4], Kilometers);

        if(StandardMetrics.ms_v_pos_odometer->AsInt() > 0 && has_odo == false && StandardMetrics.ms_v_bat_soc->AsFloat() > 0)
        {
          has_odo = true;
          if (!set_odo)
          {
            mi_charge_trip_counter.Reset(POS_ODO);
            ms_v_trip_charge_soc_start->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());
            ms_v_trip_charge_soc_stop->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());
            set_odo = true;
          }
        }


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

        if ( (d[0] & 4) != 0) //headlight
        { // ON
          StandardMetrics.ms_v_env_headlights->SetValue(true);
        }
        else
        {
          StandardMetrics.ms_v_env_headlights->SetValue(false);
        }

        if ((d[0] & 8 ) != 0) //Fog lights front
        { // ON
          ms_v_env_frontfog->SetValue(true);
        }
        else
        {
          ms_v_env_frontfog->SetValue(false);
        }

        if ((d[0] & 16) != 0) //Fog lights rear
        {// ON
          ms_v_env_rearfog->SetValue(true);
        }
        else
        {
          ms_v_env_rearfog->SetValue(false);
        }

        if ((d[1] & 1) != 0)// Blinker right
        { // ON
          ms_v_env_blinker_right->SetValue(true);
        }
        else
        {
          ms_v_env_blinker_right->SetValue(false);
        }

        if ((d[1] & 2) != 0) //Blinker left
        { // ON
          ms_v_env_blinker_left->SetValue(true);
        }
        else
        {
          ms_v_env_blinker_left->SetValue(false);
        }

        if ((d[1] & 4) != 0)// Highbeam
        { // ON
          ms_v_env_highbeam->SetValue(true);
        }
        else
        {
          ms_v_env_highbeam->SetValue(false);
        }

        if ((d[1] & 32) != 0 )  //Headlight
        {
		       // ESP_LOGI(TAG, "Headlight2 on");
        }
        else
        {

        }

        if ((d[1] & 64) != 0)
        { //Parkinglight
		      //ESP_LOGI(TAG, "Parkinglight on");
        }
        else
        {

        }

        if ((d[2] & 1) != 0) //any Door + trunk
        { //OPEN
            //StandardMetrics.ms_v_door_fr->SetValue(true);
        }
        else
        {
              //StandardMetrics.ms_v_door_fr->SetValue(false);
        }


        if ((d[2] & 2) != 0) //LHD Driver Door
        { //OPEN
          StandardMetrics.ms_v_door_fl->SetValue(true);
        }
        else
        {
          StandardMetrics.ms_v_door_fl->SetValue(false);
        }
        if ((d[2] & 3) != 0) //??
        {
        }
        if ((d[2] & 128) != 0) //??
        {
        }
        else
        {
        }
        if ((d[2] & 192) != 0) //??
        {
        }
        break;
      }

    case 0x697:
    {
      if(d[0] == 1 )
      {
        mi_QC = true;
        StandardMetrics.ms_v_charge_type->SetValue("Quickcharge");
        StandardMetrics.ms_v_charge_mode->SetValue("CHAdeMO");
        StandardMetrics.ms_v_charge_climit->SetValue(d[2]);
      }
      else
      {
        mi_QC = false;
      }
    break;
    }

    case 0x6e1://freq25 // Battery temperatures and voltages
    case 0x6e2:
    case 0x6e3:
    case 0x6e4:
    {
      //Pid index 0-3
      int pid_index = (p_frame->MsgID) - 1761;
      //cmu index 1-12
      int cmu_id = d[0];
      //
      double temp1 = d[1] - 50.0;
      double temp2 = d[2] - 50.0;
      double temp3 = d[3] - 50.0;

      double voltage1 = (((d[4] * 256.0 + d[5]) / 200.0) + 2.1);
      double voltage2 = (((d[6] * 256.0 + d[7]) / 200.0) + 2.1);

      int voltage_index = ((cmu_id - 1) * 8 + (2 * pid_index));
      int temp_index = ((cmu_id - 1) * 6 + (2 * pid_index));
      if (cmu_id >= 7)
      {
        voltage_index -= 4;
        temp_index -= 3;
      }

      BmsSetCellVoltage(voltage_index, voltage1);
      BmsSetCellVoltage(voltage_index + 1, voltage2);

      if (pid_index == 0)
      {
        BmsSetCellTemperature(temp_index, temp2);
        BmsSetCellTemperature(temp_index + 1, temp3);
      }
      else
      {
        BmsSetCellTemperature(temp_index, temp1);
        if (cmu_id != 6 && cmu_id != 12)
        {
          BmsSetCellTemperature(temp_index + 1, temp2);
        }
      }
      break;
    }

    case 0x762: // AH
    {
      if (d[0] == 16)
      {
        // Request CAC...
        CAN_frame_t frame;
        memset(&frame,0,sizeof(frame));
        frame.origin = m_can1;
        frame.FIR.U = 0;
        frame.FIR.B.DLC = 3;
        frame.FIR.B.FF = CAN_frame_std;
        frame.MsgID = 0x761;
        frame.data.u8[0] = 0x30;
        frame.data.u8[1] = 0x08;
        frame.data.u8[2] = 0x0A;
        m_can1->Write(&frame);

        //real SOC
        OvmsMetricFloat* xmi_bat_soc_real = MyMetrics.InitFloat("xmi.b.soc.real", 10, 0, Percentage);
        xmi_bat_soc_real->SetValue((d[4] / 2.0 - 5));

        // displayed SOC
        OvmsMetricFloat* xmi_bat_soc_display = MyMetrics.InitFloat("xmi.b.soc.display", 10, 0, Percentage);
        xmi_bat_soc_display->SetValue((d[5] / 2.0 - 5));
      }

      if(d[0] == 36)
      {
        // battery "max" capacity
        StandardMetrics.ms_v_bat_cac->SetValue(((d[3] * 256.0 + d[4]) / 10.0));

        // battery remain capacity
        ms_v_bat_cac_rem->SetValue(((d[5] * 256.0 + d[6]) / 10.0));

        //max charging kW
        ms_v_bat_max_input->SetValue(d[7] / 4.0);
      }

      if(d[0] == 37)
      {
        //max output kW
        ms_v_bat_max_output->SetValue(d[1] / 4.0);
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

  if (StandardMetrics.ms_v_charge_inprogress->IsStale())
  { //clear all charge variable if not charging;
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
      // Charge state determination
      ////////////////////////////////////////////////////////////////////////

    if ((mi_QC == true) || (mi_SC == true))
    {
      StandardMetrics.ms_v_env_charging12v->SetValue(true);
      if (! StandardMetrics.ms_v_charge_pilot->AsBool())
      {
        // Charge has started
        ms_v_charge_ac_kwh->SetValue(0);
        ms_v_charge_dc_kwh->SetValue(0);
        StandardMetrics.ms_v_charge_kwh->SetValue(0.0,kWh);
        mi_chargekwh = 0;      // Reset charge kWh
        v_c_time->SetValue(0); //Reser charge timer
        StandardMetrics.ms_v_charge_inprogress->SetValue(true);
        StandardMetrics.ms_v_charge_pilot->SetValue(true);
        StandardMetrics.ms_v_door_chargeport->SetValue(true);
        StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        ms_v_trip_charge_soc_stop->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());
        v_c_soc_start->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());

        BmsResetCellStats();
      }
      else
      {
        StandardMetrics.ms_v_charge_state->SetValue("charging");
        v_c_time->SetValue(StandardMetrics.ms_v_charge_time->AsInt());
        // if charge and DC current is negative cell balancing is active. If charging battery current is negative, discharge is positive
        if ((StandardMetrics.ms_v_bat_current->AsFloat() < 0) && (mi_QC == false) && StandardMetrics.ms_v_bat_soc->AsInt() > 92 && StandardMetrics.ms_v_charge_mode->AsString() != "Balancing")
        {
          StandardMetrics.ms_v_charge_mode->SetValue("Balancing");
          StandardMetrics.ms_v_charge_state->SetValue("topoff");
        }
      }
    }
    else
      if ((StandardMetrics.ms_v_charge_current->AsInt() == 0) && (StandardMetrics.ms_v_charge_voltage->AsInt() < 100))
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
          if (StandardMetrics.ms_v_bat_soc->AsInt() < 92 && mi_QC == false)
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
          StandardMetrics.ms_v_charge_current->SetValue(0.0); //Reset charge current
          StandardMetrics.ms_v_charge_climit->SetValue(0.0); //Reset charge limit
          StandardMetrics.ms_v_charge_mode->SetValue("None"); //Set charge mode to NONE
          StandardMetrics.ms_v_bat_current->SetValue(0.0);  //Set battery current to 0
          ms_v_charge_ac_kwh->SetValue(StandardMetrics.ms_v_charge_kwh->AsFloat()); //save charge kwh to variable
          StandardMetrics.ms_v_charge_kwh->SetValue(0); //reset charge kwh variable

          // Reset trip counter for this charge
          mi_charge_trip_counter.Reset(POS_ODO);
          ms_v_trip_charge_energy_recd->SetValue(0);
          ms_v_trip_charge_energy_used->SetValue(0);
          ms_v_trip_charge_heating_kwh->SetValue(0);
          ms_v_trip_charge_ac_kwh->SetValue(0);
          ms_v_trip_charge_soc_start->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());
          ms_v_trip_charge_soc_stop->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());
          v_c_soc_stop->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());

        }
      }

      //Efficiency calculation AC/DC
      if ((v_c_power_dc->AsInt() <= 0) || (v_c_power_ac->AsInt() <= 0))
      {
        v_c_efficiency->SetValue(0,Percentage);
      }
      else
      {
        v_c_efficiency->SetValue((v_c_power_dc->AsFloat() / v_c_power_ac->AsFloat()) * 100, Percentage);
      }
  } //Car in P "if" close

  // Update trip data
  if (StdMetrics.ms_v_env_on->AsBool())
  {
    //Current Trip
    if (mi_park_trip_counter.Started())
    {
        mi_park_trip_counter.Update(POS_ODO);
        StdMetrics.ms_v_pos_trip->SetValue(mi_park_trip_counter.GetDistance(), Kilometers);
        ms_v_pos_trip_park->SetValue(mi_charge_trip_counter.GetDistance(), Kilometers);
        ms_v_trip_park_soc_stop->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());
    }
    //Since charge
    if (mi_charge_trip_counter.Started())
    {
      mi_charge_trip_counter.Update(POS_ODO);
      ms_v_pos_trip_charge->SetValue( mi_charge_trip_counter.GetDistance() , Kilometers );
      ms_v_trip_charge_soc_stop->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());
    }
  }

  if (StandardMetrics.ms_v_bat_soc->AsFloat() <= 10)
  {
    StandardMetrics.ms_v_bat_range_ideal->SetValue(0);
  }
  else
  {
    int cell = 88;
    if (cfg_newcell)
      cell = 80;

    StdMetrics.ms_v_bat_range_ideal->SetValue(StdMetrics.ms_v_bat_soc->AsFloat() * StdMetrics.ms_v_bat_cac->AsFloat() * 3.7 * cell / 16100.0);
    StdMetrics.ms_v_bat_soh->SetValue((StdMetrics.ms_v_bat_cac->AsFloat() / 48.0) * 100);
  }
}
void OvmsVehicleMitsubishi::Ticker60(uint32_t ticker)
{
  if (StdMetrics.ms_v_env_on->AsBool() || mi_QC == true || mi_SC == true)
  {
    //Send a request to BMS ECU
    CAN_frame_t frame;
    memset(&frame,0,sizeof(frame));
    frame.origin = m_can1;
    frame.FIR.U = 0;
    frame.FIR.B.DLC = 3;
    frame.FIR.B.FF = CAN_frame_std;
    frame.MsgID = 0x761;
    frame.data.u8[0] = 0x02;
    frame.data.u8[1] = 0x21;
    frame.data.u8[2] = 0x01;
    m_can1->Write(&frame);
  }

}
  /**
   * Takes care of setting all the state appropriate when the car is on
   * or off. Centralized so we can more easily make on and off mirror
   * images.
   */
void OvmsVehicleMitsubishi::vehicle_mitsubishi_car_on(bool isOn)
{
  StdMetrics.ms_v_env_awake->SetValue(isOn);
    if (isOn && !StdMetrics.ms_v_env_on->AsBool())
    {
  	   // Car is ON
       StdMetrics.ms_v_env_on->SetValue(isOn);
       //Reset trip variables so that they are updated as soon as they are available
       //mi_trip_start_odo = 0;
       StdMetrics.ms_v_env_charging12v->SetValue(true);
       //Reset energy calculation
       StdMetrics.ms_v_bat_energy_recd->SetValue(0);
       StdMetrics.ms_v_bat_energy_used->SetValue(0);
       ms_v_trip_park_energy_used->SetValue(0);
       ms_v_trip_park_energy_recd->SetValue(0);
       ms_v_trip_park_heating_kwh->SetValue(0);
       ms_v_trip_park_ac_kwh->SetValue(0);
       ms_v_trip_park_time_start->SetValue(StdMetrics.ms_m_timeutc->AsInt());
       if(has_odo == true && StandardMetrics.ms_v_bat_soc->AsFloat() > 0.0)
         {
           mi_park_trip_counter.Reset(POS_ODO);
           ms_v_trip_park_soc_start->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());
         }

       BmsResetCellStats();
    }
    else if ( !isOn && StdMetrics.ms_v_env_on->AsBool() )
    {
      // Car is OFF
      StdMetrics.ms_v_env_on->SetValue( isOn );
      StdMetrics.ms_v_pos_speed->SetValue(0);
      StdMetrics.ms_v_pos_trip->SetValue(mi_park_trip_counter.GetDistance());
      ms_v_pos_trip_park->SetValue(mi_park_trip_counter.GetDistance());
      //StdMetrics.ms_v_pos_trip->SetValue( POS_ODO- mi_trip_start_odo );
      StdMetrics.ms_v_env_charging12v->SetValue(false);
      mi_park_trip_counter.Update(POS_ODO);
      ms_v_trip_park_soc_stop->SetValue(StandardMetrics.ms_v_bat_soc->AsFloat());
      ms_v_trip_park_time_stop->SetValue(StdMetrics.ms_m_timeutc->AsInt());
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
          (char*) StdMetrics.ms_v_charge_limit_soc->AsUnitString("SOC", Native, 0).c_str(), duration_soc);

      int duration_range = StdMetrics.ms_v_charge_duration_range->AsInt();
      if (duration_range > 0)
        writer->printf("%s: %d mins\n",
          (char*) StdMetrics.ms_v_charge_limit_range->AsUnitString("Range", rangeUnit, 0).c_str(), duration_range);
    }
  }
  else
  {
    writer->puts("Not charging");
  }

  writer->printf("SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());

  const char* range_ideal = StdMetrics.ms_v_bat_range_ideal->AsUnitString("-", rangeUnit, 0).c_str();
  if (*range_ideal != '-')
    writer->printf("Ideal range: %s\n", range_ideal );

  const char* range_est = StdMetrics.ms_v_bat_range_est->AsUnitString("-", rangeUnit, 0).c_str();
  if (*range_est != '-')
    writer->printf( "Est. range: %s\n", range_est );

  const char* odometer = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();
  if (*odometer != '-')
    writer->printf( "ODO: %s\n", odometer );

  const char* cac = StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str();
  if ( *cac != '-')
    writer->printf( "CAC: %s\n", cac );

  const char* soh = StdMetrics.ms_v_bat_soh->AsUnitString("-", Native, 0).c_str();
  if (*soh != '-')
    writer->printf("SOH: %s\n", soh);

  const char* charge_kWh_ac = ms_v_charge_ac_kwh->AsUnitString("-", Native, 3).c_str();
  if (*charge_kWh_ac != '-')
    writer->printf( "AC charge: %s kWh\n", charge_kWh_ac );

  const char* charge_kWh_dc = ms_v_charge_dc_kwh->AsUnitString("-", Native, 3).c_str();
  if (*charge_kWh_dc != '-')
    writer->printf( "DC charge: %s kWh\n", charge_kWh_dc );

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

  /*******************************************************************************/
MI_Trip_Counter::MI_Trip_Counter()
{
  odo_start=0;
  odo=0;
}

MI_Trip_Counter::~MI_Trip_Counter()
{
}

  /*
   * Resets the trip counter.
   *
   * odo - The current ODO
   */
void MI_Trip_Counter::Reset(float odo)
{
  odo_start = 0;
  Update(odo);
  if ((odo_start == 0) && (odo != 0))
    odo_start = odo;		// ODO at Start of trip
}

  /*
   * Update the trip counter with current ODO
   *
   * odo - The current ODO
   */
void MI_Trip_Counter::Update(float current_odo)
{
  odo = current_odo;
}

  /*
   * Returnstrueif the trip counter has been initialized properly.
   */
bool MI_Trip_Counter::Started()
{
  return odo_start != 0;
}

float MI_Trip_Counter::GetDistance()
{
  return odo - odo_start;
}
