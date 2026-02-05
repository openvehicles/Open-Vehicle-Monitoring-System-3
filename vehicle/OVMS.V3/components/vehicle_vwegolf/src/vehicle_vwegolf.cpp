/*
;    Project:       Open Vehicle Monitor System
;	 Subproject:    Integrate VW e-Golf
;
;    Changes:
;    Sept 6, 2023: stub made
;
;    (C) 2023  Jared Greco <jaredgreco@gmail.com>
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

// #include "ovms_log.h"
static const char *TAG = "v-vwegolf";

// #include <stdio.h>
#include "vehicle_vwegolf.h"

OvmsVehicleVWeGolf::OvmsVehicleVWeGolf()
{
  ESP_LOGI(TAG, "Start vehicle module: VW e-Golf");

  // init configs:
  MyConfig.RegisterParam("xvg", "VW e-Golf", true, true);
  Testbutton = MyConfig.GetParamValueBool("xvg", "Testbutton");

  // Pinout source:
  // https://i.ytimg.com/vi/YXnpnKqsZME/hq720.jpg?sqp=-oaymwEhCK4FEIIDSFryq4qpAxMIARUAAAAAGAElAADIQj0AgKJD&rs=AOn4CLAKpYqTVTQjVzcyorpJt8FnLgstxw
  // Location of the Gateway:
  // https://forums.ross-tech.com/index.php?threads/13163/
  //
  //RegisterCanBus(1, CAN_MODE_LISTEN, CAN_SPEED_500KBPS); //OBD -> Diagnosis CAN
  //RegisterCanBus(2, CAN_MODE_LISTEN, CAN_SPEED_500KBPS); //FCAN -> Powertrain CAN
  RegisterCanBus(3, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS); //KCAN -> convenience CAN

  WebInit();
}

OvmsVehicleVWeGolf::~OvmsVehicleVWeGolf()
{
  WebDeInit();
  ESP_LOGI(TAG, "Stop vehicle module: VW e-Golf");
}

class OvmsVehicleVWeGolfInit
{
  public: OvmsVehicleVWeGolfInit();
} MyOvmsVehicleVWeGolfInit  __attribute__ ((init_priority (9000)));

void vweg_OfflineCall(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  m_is_control_active = false;
  ESP_LOGI(TAG, "Heartbeat sending should be stopped");
}

void vweg_SpiegelanklappenCall(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  ESP_LOGI(TAG, "Test command Spiegelanklappen call executed");
}

OvmsVehicleVWeGolfInit::OvmsVehicleVWeGolfInit()
{
	ESP_LOGI(TAG, "Registering Vehicle: VW e-Golf (9000)");
	MyVehicleFactory.RegisterVehicle<OvmsVehicleVWeGolf>("VWEG","VW e-Golf");

  OvmsCommand* cmd_vweg = MyCommandApp.RegisterCommand("vweg","VW-eGolf framework");
  cmd_vweg->RegisterCommand("Offline","OVMS please go offline",vweg_OfflineCall);
  cmd_vweg->RegisterCommand("Spiegelanklappen","VW eGolf please Klappe die Spiegel an",vweg_SpiegelanklappenCall);
  ESP_LOGI(TAG, "Commands for testing purposes registerd");

  StandardMetrics.ms_v_bat_current->SetValue(4.2F);
  StandardMetrics.ms_v_bat_voltage->SetValue(442.0F);
  StandardMetrics.ms_v_bat_soc->SetValue(42.0F);
  StandardMetrics.ms_v_pos_speed->SetValue(69.0F);
  StandardMetrics.ms_v_env_temp->SetValue(24.0F);
  StandardMetrics.ms_v_pos_odometer->SetValue(42.0F);
  StandardMetrics.ms_v_bat_range_est->SetValue(469.0F);
  StdMetrics.ms_v_bat_range_ideal->SetValue(469.0F);
  StandardMetrics.ms_v_env_cabintemp->SetValue(69.0F);
  StandardMetrics.ms_v_bat_energy_recd->SetValue(42.0F);
  StandardMetrics.ms_v_bat_energy_used->SetValue(42.0F);
  StdMetrics.ms_v_door_fl->SetValue(0);
  StdMetrics.ms_v_door_fr->SetValue(0);
  StdMetrics.ms_v_door_rl->SetValue(1);
  StdMetrics.ms_v_door_rr->SetValue(1);
  StdMetrics.ms_v_door_trunk->SetValue(0);
  StdMetrics.ms_v_door_hood->SetValue(1);

  m_last_message_received = 255;
}

void OvmsVehicleVWeGolf::IncomingFrameCan1(CAN_frame_t* p_frame)
{
}

void OvmsVehicleVWeGolf::IncomingFrameCan2(CAN_frame_t* p_frame)
{
}

void OvmsVehicleVWeGolf::IncomingFrameCan3(CAN_frame_t* p_frame)
{
  m_last_message_received = 0;
  uint8_t *d = p_frame->data.u8;

  uint8_t tmp_u8 = 0;
  uint16_t tmp_u16 = 0;
  uint32_t tmp_u32 = 0;
  // int8_t tmp_i8 = 0;
  // int16_t tmp_i16 = 0;
  // int32_t tmp_i32 = 0;
  float tmp_f32 = 0.0F;
  
  // //TODO Debug only doesn't work ECU timeout
  // vTaskDelay(pdMS_TO_TICKS(500)); //500ms wait.. hopefully my log isn't get messed up
  // //TODO end doesn't work ECU timeout


  switch (p_frame->MsgID)
    {
    case 0xFD: // Message from ESP speed
      {
        tmp_u16 =
        ((uint16_t) (d[4] & 0xff) >> 0) |
        ((uint16_t) (d[5] & 0xff) << 8) |
        0; // vSpeed Faktor 0,01 Offset 0, Minimum 0, Maximum 655,34 [km/h] Initial 655,34
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_f32 = ((float)tmp_u16)*0.01F;
        StandardMetrics.ms_v_pos_speed->SetValue(tmp_f32); //working
        ESP_LOGV(TAG, "ms_v_pos_speed: %f", tmp_f32);
      break;
      }
    case 0x131: //SOC
      {
        tmp_u8 =
        ((uint8_t) (d[3] & 0xff) << 0) |
        0; // Ladezustand Faktor 0,5 Offset 0, Minimum 0, Maximum 100 [%] Initial 127
        tmp_u8 = (uint8_t) tmp_u8;
        tmp_f32 = ((float)tmp_u8)*0.5F;
        StandardMetrics.ms_v_bat_soc->SetValue(tmp_f32); //working
        ESP_LOGV(TAG, "ms_v_bat_soc: %f", tmp_f32);
      break;
      }
    case 0x191: // Messages from BMS on the drivetrain CAN
      {
        tmp_u16 =
        ((uint16_t) (d[1] & 0xf0) >> 4) |
        ((uint16_t) (d[2] & 0xff) << 4) |
        0; // Ist BMS Strom Faktor 1 Offset -2047, Minimum -2047, Maximum 2046 [A] Initial 2047
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_f32 = ((float)tmp_u16)*1.0F-2047.0F;
        StandardMetrics.ms_v_bat_current->SetValue(tmp_f32); //working
        ESP_LOGV(TAG, "ms_v_bat_current: %f", tmp_f32);
  
        tmp_u16 = 
        ((uint16_t) (d[3] & 0xff) << 0) |
        ((uint16_t) (d[4] & 0xf) << 8) |
        0; // Ist BMS Spannung Faktor 0,25 Offset 0, Minimum 0, Maximum 1023,25 [V] Initial 1023,25
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_f32 = ((float)tmp_u16)*0.25F;
        StandardMetrics.ms_v_bat_voltage->SetValue(tmp_f32); //working
        ESP_LOGV(TAG, "ms_v_bat_voltage: %f", tmp_f32);
  
        //calculate power
        tmp_f32 = -1.0F * (StandardMetrics.ms_v_bat_voltage->AsFloat() * StandardMetrics.ms_v_bat_current->AsFloat())/1000;
        StandardMetrics.ms_v_bat_power->SetValue(tmp_f32); //working negative is charging, positive is driving
        ESP_LOGV(TAG, "ms_v_bat_power: %f", tmp_f32);
  
      break;
      }
    case 0x2AF: // Energy
      {
        tmp_u16 = 
        ((uint16_t) (d[4] & 0xff) << 0) |
        ((uint16_t) (d[5] & 0x7f) << 8) |
        0; // BMS Rekuperation Faktor 10 Offset 0, Minimum 0, Maximum 327670 [Ws] Initial 0
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_f32 = (((float)tmp_u16)*10.0F)/3600000.0F;
        StandardMetrics.ms_v_bat_energy_recd->SetValue(tmp_f32);
        ESP_LOGV(TAG, "ms_v_bat_energy_recd: %f", tmp_f32);
  
        tmp_u16 = 
        ((uint16_t) (d[6] & 0xff) << 0) |
        ((uint16_t) (d[7] & 0x7f) << 8) |
        0; // BMS Verbrauch Faktor 10 Offset 0, Minimum 0, Maximum 327670 [Ws] Initial 0
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_f32 = (((float)tmp_u16)*10.0F)/3600000.0F;
        StandardMetrics.ms_v_bat_energy_used->SetValue(tmp_f32);
        ESP_LOGV(TAG, "ms_v_bat_energy_used: %f", tmp_f32);
      break;
      }
   case 0x3CE: //Rear door driver side
     {
      // StdMetrics.ms_v_door_rl->SetValue((d[0] & 0x1) << 0);
      // ESP_LOGV(TAG, "ms_v_door_rl: %u", (d[0] & 0x1) << 0);
     break;
     }
   case 0x3CF: //Rear door passenger side
     {
      // StdMetrics.ms_v_door_rr->SetValue((d[0] & 0x1) << 0);
      // ESP_LOGV(TAG, "ms_v_door_rr: %u", (d[0] & 0x1) << 0);
     break;
     }
   case 0x3D0: //Front door driver side
     {
      // StdMetrics.ms_v_door_fl->SetValue((d[0] & 0x1) << 0);
      // ESP_LOGV(TAG, "ms_v_door_fl: %u", (d[0] & 0x1) << 0);
     break;
     }
   case 0x3D1: //Front door passenger side
     {
      // StdMetrics.ms_v_door_fr->SetValue((d[0] & 0x1) << 0);
      // ESP_LOGV(TAG, "ms_v_door_fr: %u", (d[0] & 0x1) << 0);
     break;
     }
    // case 0x3D6: //Ladezustand
    //   {
    //     ESP_LOGD(TAG, "chargeState: %x", ((d[6] & 0x30)>>4));
    //     if(((d[6] & 0x30)>>4) == 0x0)
    //     {
    //       StdMetrics.ms_v_charge_state->SetValue("stopped");
    //       ESP_LOGV(TAG, "charging stopped");
    //     }
    //     if(((d[6] & 0x30)>>4) == 0x1)
    //     {
    //       StdMetrics.ms_v_charge_state->SetValue("charging");
    //       ESP_LOGV(TAG, "charging");
    //     }
    //     if(((d[6] & 0x30)>>4) == 0x2)
    //     {
    //       StdMetrics.ms_v_charge_state->SetValue("init");
    //       ESP_LOGV(TAG, "charging init");
    //     }
    //     if(((d[6] & 0x30)>>4) == 0x3)
    //     {
    //       StdMetrics.ms_v_charge_state->SetValue("error");
    //       ESP_LOGV(TAG, "charging error");
    //     }
        
    //   break;
    //   }
    case 0x3DC: //Energieinhalt HV-Batterie
      {
        //0x3dc => Gateway_73: P, R, N, D
        tmp_u8 = 
        ((uint8_t) (d[5] & 0xf) << 0) |
        0; // outerTemp Faktor 1 Offset 0, Minimum 0, Maximum 15 [] Initial 1
        tmp_u8 = (uint8_t) tmp_u8; //0x0 zwischenstellung, 0x1 Init, 0x5 P, 0x6 R, 0x7 N, 0x8 D, 0x9 S, 0xA Efficency, 0xD Tipp in S, 0xE Tipp in E, 0xF error
        switch(tmp_u8)
        {
          case 0x5: // P
          StandardMetrics.ms_v_env_drivemode->SetValue(0);
          StdMetrics.ms_v_env_gear->SetValue(0);
            break;
          case 0x6: // R
            StandardMetrics.ms_v_env_drivemode->SetValue(1);
            StdMetrics.ms_v_env_gear->SetValue(-1);
            break;
          case 0x7: // N
          StandardMetrics.ms_v_env_drivemode->SetValue(1);
          StdMetrics.ms_v_env_gear->SetValue(0);
            break;
          case 0x8: // D
          StandardMetrics.ms_v_env_drivemode->SetValue(1);
          StdMetrics.ms_v_env_gear->SetValue(1);
            break;
          case 0x9: // S
            StandardMetrics.ms_v_env_drivemode->SetValue(2);
            StdMetrics.ms_v_env_gear->SetValue(2);
            break;
          default:
          break;
        }
        static uint8_t cnt2 = 0;
        cnt2++;
        if(cnt2 == 15)
        {
          cnt2=0;
          ESP_LOGV(TAG, "Drivemode: %u", tmp_u8);//Gangwahl Not working right now
        }
        
      break;
      }
   case 0x486: //Longitude/Latitude
     {
      tmp_u32 = 
      ((uint32_t) (d[0] & 0xff) << 0) |
      ((uint32_t) (d[1] & 0xff) << 8) |
      ((uint32_t) (d[2] & 0xff) << 16) |
      ((uint32_t) (d[3] & 0x7) << 24) |
      0; // LatDegree Faktor 1e-006 Offset 0, Minimum 0, Maximum 90 [°] Initial 134.217726
      tmp_u32 = (uint32_t) tmp_u32;
      tmp_f32 = ((float)tmp_u32)*0.000001F;
      StandardMetrics.ms_v_pos_latitude->SetValue(tmp_f32); //working
      ESP_LOGV(TAG, "ms_v_pos_latitude: %f /r/n", tmp_f32);
  
      tmp_u32 = 
      ((uint32_t) (d[3] & 0xf8) >> 3) |
      ((uint32_t) (d[4] & 0xff) << 5) |
      ((uint32_t) (d[5] & 0xff) << 13) |
      ((uint32_t) (d[6] & 0x7f) << 21) |
      0; //LongDegree Faktor 1e-006 Offset 0, Minimum 0, Maximum 180 [°] Initial 268.435454
      tmp_u32 = (uint32_t) tmp_u32;
      tmp_f32 = ((float)tmp_u32)*0.000001F;
      StandardMetrics.ms_v_pos_longitude->SetValue(tmp_f32); //working
      ESP_LOGV(TAG, "ms_v_pos_longitude: %f /r/n", tmp_f32);
  
      if(StandardMetrics.ms_v_pos_latitude->AsFloat() < 91.0F && StandardMetrics.ms_v_pos_longitude->AsFloat() < 181.0F)
      {
        StandardMetrics.ms_v_pos_gpslock->SetValue(true);
        ESP_LOGV(TAG, "GPS position fix"); //working
      }
      else
      {
        StandardMetrics.ms_v_pos_gpslock->SetValue(false);
        ESP_LOGV(TAG, "no GPS position fix"); //working
      }
     break;
     }
     case 0x583: //central locking
      {
    //0x583 => ZV_02 alle hinteren Türen in einem mit Tankklappe, Heckdekel aber keine Fahrertüren ms_v_env_locked 
        StdMetrics.ms_v_env_locked->SetValue((d[2] & 0x2) >> 1);  //working //verriegelt extern ist                 // Vehicle locked
        //not working StdMetrics.ms_v_env_locked->SetValue((d[2] & 0x10) >> 4);   //gesafet extern ist                 // Vehicle locked
        StdMetrics.ms_v_door_fl->SetValue((d[3] & 0x1) << 0); //working
        StdMetrics.ms_v_door_fr->SetValue((d[3] & 0x2) >> 1); //working
        StdMetrics.ms_v_door_rl->SetValue((d[3] & 0x4) >> 2); //working
        StdMetrics.ms_v_door_rr->SetValue((d[3] & 0x8) >> 3); //working
        StdMetrics.ms_v_door_trunk->SetValue((d[3] & 0x10) >> 4); //working
        //not working StdMetrics.ms_v_door_chargeport->SetValue((d[7] & 0x1) << 0);
  
        static uint8_t cnt = 0;
        cnt++;
        if(cnt == 15)
        {
          cnt=0;
          ESP_LOGV(TAG, "ms_v_env_locked verriegelt: %u", (d[2] & 0x2) >> 1); //working //verriegelt extern ist
          //not working ESP_LOGV(TAG, "ms_v_env_locked gesafet: %u", (d[2] & 0x10) >> 14);//gesafet extern ist
          ESP_LOGV(TAG, "ms_v_door_fl: %u", (d[3] & 0x1) << 0);
          ESP_LOGV(TAG, "ms_v_door_fr: %u", (d[3] & 0x2) >> 1);
          ESP_LOGV(TAG, "ms_v_door_rl: %u", (d[3] & 0x4) >> 2);
          ESP_LOGV(TAG, "ms_v_door_rr: %u", (d[3] & 0x8) >> 3);
          ESP_LOGV(TAG, "ms_v_door_trunk: %u", (d[3] & 0x10) >> 4);
          ESP_LOGI(TAG, "ms_v_door_chargeport: %u", (d[7] & 0x1) << 0);
        }
      break;
      }
    case 0x594: // HV charge management
      {
        //0x594 => AC/DC charging, is climate timer active or not, plug connected (secured or not), programmed cabin temp, charging active, time until HV battery full in 5min steps
        tmp_u16 = 
        ((uint16_t) (d[1] & 0xf0) >> 4) |
        ((uint16_t) (d[2] & 0x1f) << 4) |
        0; // Faktor 5 Offset 0, Minimum 0, Maximum 2545 [5min] Initial 2550
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_u16 = (((int16_t)tmp_u16)*5);    
        StdMetrics.ms_v_charge_duration_full->SetValue(tmp_u16, Minutes); //working          // Estimated time remaing for full charge [min]

        tmp_u8 = 
        ((uint8_t) (d[2] & 0x60) >> 5) |
        0; // Faktor 1 Offset 0, Minimum 0, Maximum 3 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8;
        if(tmp_u8 == 0x1)
        {
          StdMetrics.ms_v_charge_timermode->SetValue(true);              // True if timer enabled
        }
        else
        {
          StdMetrics.ms_v_charge_timermode->SetValue(false);              // false if timer disabled
        }

        tmp_u8 = 
        ((uint8_t) (d[3] & 0x62) >> 5) |
        0; // Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8;
        if(tmp_u8 == 0x1)
        {
          StdMetrics.ms_v_charge_inprogress->SetValue(true);             // True = currently charging
        }
        else
        {
          StdMetrics.ms_v_charge_inprogress->SetValue(false);              // false = currently not charging
        }
  
        tmp_u16 = 
        ((uint16_t) (d[3] & 0xc0) >> 6) |
        ((uint16_t) (d[4] & 0x7f) << 2) |
        0; // Faktor 50 Offset 0, Minimum 0, Maximum 25450 [W] Initial 25500
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_u16 = (((int16_t)tmp_u16)*50);  //maximum charging power


        tmp_u8 = 
        ((uint8_t) (d[4] & 0x80) >> 7) |
        0; // Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8;
        if(tmp_u8 == 0x1)
        {
          //parking climate control timer set
        }
        else
        {
          //parking climate control timer not set
        }

        tmp_u8 = 
        ((uint8_t) (d[5] & 0x1) << 0) |
        0; // Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8;
        if(tmp_u8 == 0x1)
        {
          //error charging plug
        }
        else
        {
          //no error charging plug
        }

        tmp_u8 = 
        ((uint8_t) (d[5] & 0x2) >> 1) |
        0; // Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8;
        if(tmp_u8 == 0x1)
        {
          //error charging plug lock
        }
        else
        {
          //no error charging plug lock
        }

        tmp_u8 = 
        ((uint8_t) (d[5] & 0xc) >> 2) |
        0; // Faktor 1 Offset 0, Minimum 0, Maximum 3 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8;
        switch(tmp_u8)
        {
          case 0x0:
          {
            //no charging active
            StdMetrics.ms_v_charge_type->SetValue("undefined"); //working                   // undefined, type1, type2, chademo, roadster, teslaus, supercharger, ccs
            break;
          }
          case 0x1:
          {
            //AC charging
            StdMetrics.ms_v_charge_type->SetValue("type2"); //working                   // undefined, type1, type2, chademo, roadster, teslaus, supercharger, ccs
            break;
          }
          case 0x2:
          {
            //DC charging
            StdMetrics.ms_v_charge_type->SetValue("ccs");                   // undefined, type1, type2, chademo, roadster, teslaus, supercharger, ccs
            break;
          }
          case 0x3:
          {
            //Battery conditioning
            break;
          }
        }

        tmp_u8 = 
        ((uint8_t) (d[5] & 0x10) >> 4) |
        0; // Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8;
        if(tmp_u8 == 0x1)
        {
          // vehicle connected to power grid
        }
        else
        {
          // vehicle not connected to power grid
        }

        tmp_u8 = 
        ((uint8_t) (d[5] & 0x60) >> 5) |
        0; // Faktor 1 Offset 0, Minimum 0, Maximum 3 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8;
        switch(tmp_u8)
        {
          case 0x0:
          {
            //no HV request
            break;
          }
          case 0x1:
          {
            //charging request
            break;
          }
          case 0x2:
          {
            //conditioning request
            break;
          }
          case 0x3:
          {
            //climatisation request
            break;
          }
        }
        
        // tmp_u8 = 
        // ((uint8_t) (d[5] & 0x80) >> 7) |
        // 0; // Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
        // tmp_u8 = (uint8_t) tmp_u8;
        // if(tmp_u8 == 0x1)
        // {
        //   // no conditioning cabin request
        // }
        // else
        // {
        //   //conditioning cabin request
        // }
        

        tmp_u8 = 
        ((uint8_t) (d[6] & 0xc) >> 2) |
        0; // Faktor 1 Offset 0, Minimum 0, Maximum 3 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8;
        switch(tmp_u8)
        {
          case 0x0:
          {
            //no conditioning request
            break;
          }
          case 0x1:
          {
            //instant conditioning request
            break;
          }
          case 0x2:
          {
            //timed conditioning request
            break;
          }
          case 0x3:
          {
            //error conditioning request
            break;
          }
        }

        tmp_u8 = 
        ((uint8_t) (d[6] & 0x30) >> 4) |
        0; // Faktor 1 Offset 0, Minimum 0, Maximum 3 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8;
        switch(tmp_u8)
        {
          case 0x0:
          {
            //plug status init
            break;
          }
          case 0x1:
          {
            //no plug detected
            break;
          }
          case 0x2:
          {
            //plug detected but not locked
            break;
          }
          case 0x3:
          {
            //eplug detected and locked
            break;
          }
        }
    // StdMetrics.ms_v_charge_state;                  // charging, topoff, done, prepare, timerwait, heating, stopped
    // StdMetrics.ms_v_charge_substate;               // scheduledstop, scheduledstart, onrequest, timerwait, powerwait, stopped, interrupted
    
        

        tmp_u8 = 
        ((uint8_t) (d[7] & 0x1F) << 0) |
        0; // Faktor 0.5 Offset 15.5, Minimum 15.5, Maximum 29.5 [°C] Initial 30.5
        tmp_u8 = (uint8_t) tmp_u8;
        tmp_f32 = ((float)tmp_u8)*0.5F+15.5F;
        StandardMetrics.ms_v_env_cabinsetpoint->SetValue(tmp_f32); //working            // Cabin setpoint temperature [°C]

        tmp_u8 = 
        ((uint8_t) (d[7] & 0x20) >> 5) |
        0; /// Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8; //0x0 no rear window heating 0x1 rear window heating
        // if(tmp_u8 == 0x1)
        // {
        //   //
        // }
        // else
        // {
        //   //
        // }


        tmp_u8 = 
        ((uint8_t) (d[7] & 0x40) >> 6) |
        0; /// Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8; //0x0 no hv bat conditioning 0x1 hv bat conditioning
        // if(tmp_u8 == 0x1)
        // {
        //   //
        // }
        // else
        // {
        //   //
        // }

        tmp_u8 = 
        ((uint8_t) (d[7] & 0x80) >> 7) |
        0; /// Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8; //0x0 no front window heating 0x1 front window heating
        // if(tmp_u8 == 0x1)
        // {
        //   //
        // }
        // else
        // {
        //   //
        // }


        static uint8_t cnt5 = 0;
        cnt5++;
        if(cnt5 == 15)
        {
          cnt5=0;
          ESP_LOGV(TAG, "ms_v_charge_duration_full: %u", StdMetrics.ms_v_charge_duration_full->AsInt()); //working
          ESP_LOGI(TAG, "ms_v_charge_timermode: %u", StdMetrics.ms_v_charge_timermode->AsBool());
          ESP_LOGV(TAG, "ms_v_charge_inprogress: %u", StdMetrics.ms_v_charge_inprogress->AsBool()); //working
          ESP_LOGV(TAG, "ms_v_env_cabinsetpoint: %f", StdMetrics.ms_v_env_cabinsetpoint->AsFloat()); //working
          if (StdMetrics.ms_v_charge_type->AsString() == "ccs")
          {
            ESP_LOGI(TAG, "ms_v_charge_type: ccs");
          }
          if (StdMetrics.ms_v_charge_type->AsString() == "type2") //working
          {
            ESP_LOGV(TAG, "ms_v_charge_type: type2");
          }
          if (StdMetrics.ms_v_charge_type->AsString() == "undefined") //working
          {
            ESP_LOGV(TAG, "ms_v_charge_type: undefined");
          }
          

        
        }
  



    // StdMetrics.ms_v_env_hvac;                      // Climate control system state
    // StdMetrics.;             // Cabin setpoint temperature [°C]
    // StdMetrics.
    // StdMetrics.
  
      break;
      }
    case 0x59E: //BMS_06 Battery Temperature
      {
        tmp_u8 = 
        ((uint8_t) (d[2] & 0xff) << 0) |
        0; // BatteryTemp Faktor 0.5 Offset -40, Minimum -40, Maximum 86.5 [°C] Initial 87
        tmp_u8 = (uint8_t) tmp_u8;
        tmp_f32 = ((float)tmp_u8)*0.5F-40.0F;
        StandardMetrics.ms_v_bat_temp->SetValue(tmp_f32); //working

        static uint8_t cnt59 = 0;
        cnt59++;
        if(cnt59 == 10)
        {
          cnt59=0;
          ESP_LOGV(TAG, "0x59E: ms_v_bat_temp: %f", tmp_f32); //working
        }
      break;
      }
    case 0x5CA: //BMS_07 Battery Energieinhalt HV-Batterie
      {
        tmp_u16 =
        ((uint16_t) (d[1] & 0xf0) >> 4) |
        ((uint16_t) (d[2] & 0x7f) << 4) |
        0; // energieinhalt Faktor 50 Offset 0, Minimum 0, Maximum 102250 [Wh] Initial 102300
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_f32 = ((float)tmp_u16)*50.0F;
        StandardMetrics.ms_v_bat_capacity->SetValue(tmp_f32); //working

        static uint8_t cnt590 = 0;
        cnt590++;
        if(cnt590 == 10)
        {
          cnt590=0;
          ESP_LOGV(TAG, "0x5CA: ms_v_bat_capacity: %f", tmp_f32); //working
        }

      break;
      }
    // case 0x5CA: //Energieinhalt HV-Batterie
    //   {
    //   break;
    //   }
    case 0x5EA: //Temperatur, Stati der Standklimatisierung
      {
        tmp_u16 =
        ((uint16_t) (d[6] & 0xfc) >> 2) |
        ((uint16_t) (d[7] & 0xf) << 6) |
        0; // cabinTempClimateController Faktor 0.1 Offset -40, Minimum -40, Maximum 62 [°C] Initial 62.2
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_f32 = ((float)tmp_u16)*0.1F-40.0F;

        static uint8_t cnt592 = 0;
        cnt592++;
        if(cnt592 == 10)
        {
          ESP_LOGI(TAG, "0x5EA: cabinTempClimateController: %f", tmp_f32);
        } 

        tmp_u8 =
        ((uint8_t) (d[3] & 0xc0) >> 6) |
        ((uint8_t) (d[4] & 0x1) << 2) |
        0; // StandklimaRemoteModus Faktor 1 Offset 0, Minimum 0, Maximum 7 [] Initial
        tmp_u8 = (uint8_t) tmp_u8;

        if(cnt592 == 10)
        {
          ESP_LOGI(TAG, "0x5EA: StandklimaRemoteModus: %u", tmp_u8);
        } 

        tmp_u8 =
        ((uint8_t) (d[3] & 0x38) >> 6) |
        0; // StandklimaStatus_02 Faktor 1 Offset 0, Minimum 0, Maximum 7 [] Initial
        tmp_u8 = (uint8_t) tmp_u8;

        if(cnt592 == 10)
        {
          ESP_LOGI(TAG, "0x5EA: StandklimaStatus_02: %u", tmp_u8);
        } 

        tmp_u8 =
        ((uint8_t) (d[0] & 0xe) >> 1) |
        0; // StandklimaStatus_03 Faktor 1 Offset 0, Minimum 0, Maximum 7 [] Initial
        tmp_u8 = (uint8_t) tmp_u8;

        if(cnt592 == 10)
        {
          cnt592=0;
          ESP_LOGI(TAG, "0x5EA: StandklimaStatus_03: %u", tmp_u8);
        } 

      break;
      }
    case 0x5F5: //Reichweite
      {
        tmp_u16 =
        ((uint16_t) (d[3] & 0xe0) >> 5) |
        ((uint16_t) (d[4] & 0xff) << 3) |
        0; // Reichweite Faktor 1 Offset 0, Minimum 0, Maximum 2044 [km] Initial 0
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_f32 = ((float)tmp_u16)*1.0F;
        StandardMetrics.ms_v_bat_range_est->SetValue(tmp_f32); //working
        ESP_LOGV(TAG, "ms_v_bat_range_est: %f", tmp_f32);
  
        tmp_u16 =
        ((uint16_t) (d[0] & 0xff) << 0) |
        ((uint16_t) (d[1] & 0x7) << 8) |
        0; // Reichweite Faktor 1 Offset 0, Minimum 0, Maximum 2044 [km] Initial 0
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_f32 = ((float)tmp_u16)*1.0F;
        StdMetrics.ms_v_bat_range_ideal->SetValue(tmp_f32); //working
        ESP_LOGV(TAG, "ms_v_bat_range_ideal: %f", tmp_f32);
      break;
      }
    case 0x65A: //BCM_01
      {
        tmp_u8 = 
        ((uint8_t) (d[3] & 0x80) >> 7) |
        0; /// Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8; //MHSchalter

        static uint8_t cnt3 = 0;
        cnt3++;
        if(cnt3 == 10)
        {
          ESP_LOGI(TAG, "0x65A: MHSchalter: %u", tmp_u8);
        }
        tmp_u8 = 
        ((uint8_t) (d[4] & 0x1) << 0) |
        0; /// Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
        tmp_u8 = (uint8_t) tmp_u8; //MHWIVSchalter
        if(cnt3 == 10)
        {
          cnt3=0;
          ESP_LOGI(TAG, "0x65A: MHWIVSchalter: %u", tmp_u8);
        }
        StdMetrics.ms_v_door_hood->SetValue(tmp_u8);
      break;
      }
    case 0x66E: //InnenTemp
      {
        tmp_u8 = 
        ((uint8_t) (d[4] & 0xff) << 0) |
        0; // outerTemp Faktor 0.5 Offset -50, Minimum -50, Maximum 75 [°C] Initial 77
        tmp_u8 = (uint8_t) tmp_u8;
        tmp_f32 = ((float)tmp_u8)*0.5F-50.0F;
        StandardMetrics.ms_v_env_cabintemp->SetValue(tmp_f32); // only initial temp shown

        static uint8_t cnt3 = 0;
        cnt3++;
        if(cnt3 == 10)
        {
          cnt3=0;
          ESP_LOGI(TAG, "0x66E: ms_v_env_cabintemp: %f", tmp_f32);
        }
      break;
      }
    case 0x6B0: //TempLuft FSTempSensor
      {
        tmp_u8 =
        ((uint8_t) (d[4] & 0xff) << 0) |
        0; // FS_TempSensor Faktor 0.5 Offset -40, Minimum -39.5, Maximum 87 [°C] Initial -40
        tmp_u8 = (uint8_t) tmp_u8;
        tmp_f32 = ((float)tmp_u8)*0.5F-40.0F;

        static uint8_t cnt40 = 0;
        cnt40++;
        if(cnt40 == 10)
        {
          cnt40=0;
          ESP_LOGI(TAG, "0x6B0: FS Temperatur: %f", tmp_f32);
        }
      break;
      }
    case 0x6B5: //TempLuft TempSensor
      {
        tmp_u16 =
        ((uint16_t) (d[6] & 0xff) << 0) |
        ((uint16_t) (d[7] & 0x7) << 8) |
        0; // TempSensor Faktor 0.1 Offset -40, Minimum -40, Maximum 90 [°C] Initial 164.6
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_f32 = ((float)tmp_u16)*0.1F-40.0F;
        static uint8_t cnt4 = 0;
        cnt4++;
        if(cnt4 == 10)
        {
          ESP_LOGI(TAG, "0x6B5: FSATempSensor: %f", tmp_f32);
        }


        tmp_u16 =
        ((uint16_t) (d[2] & 0xff) << 0) |
        ((uint16_t) (d[3] & 0x3) << 8) |
        0; // TempLuft Faktor 0.1 Offset -40, Minimum -40, Maximum 61 [°C] Initial 62.2
        tmp_u16 = (uint16_t) tmp_u16;
        tmp_f32 = ((float)tmp_u16)*0.1F-40.0F;
        if(cnt4 == 10)
        {
          cnt4=0;
          ESP_LOGI(TAG, "0x6B5: FSATempLuft: %f", tmp_f32); //only initial temp shown
        }
      break;
      }
    case 0x6B7: //AussenTemp gefiltert Kilometerstand
      {
        tmp_u32 = 
        ((uint32_t) (d[0] & 0xff) << 0) |
        ((uint32_t) (d[1] & 0xff) << 8) |
        ((uint32_t) (d[2] & 0xf) << 16) |
        0; // odometer Faktor 1 Offset 0, Minimum 0, Maximum 1045873 [km] Initial 1045874
        tmp_u32 = (uint32_t) tmp_u32;
        // tmp_f32 = ((float)tmp_u32)*1.0F;
        StandardMetrics.ms_v_pos_odometer->SetValue(tmp_u32); //working
        ESP_LOGV(TAG, "ms_v_pos_odometer: %u", tmp_u32);
  
        tmp_u32 = 
        ((uint32_t) (d[2] & 0xf0) << 4) |
        ((uint32_t) (d[3] & 0xff) << 4) |
        ((uint32_t) (d[4] & 0x1f) << 12) |
        0; // odometer Faktor 1 Offset 0, Minimum 0, Maximum 1045873 [km] Initial 1045874
        tmp_u32 = (uint32_t) tmp_u32;
        // tmp_f32 = ((float)tmp_u32)*1.0F;
        StandardMetrics.ms_v_env_parktime->SetValue(tmp_u32);
        ESP_LOGV(TAG, "ms_v_env_parktime: %u", tmp_u32); //working
      
        tmp_u8 = 
        ((uint8_t) (d[7] & 0xff) << 0) |
        0; // outerTemp Faktor 0.5 Offset -50, Minimum -50, Maximum 75 [°C] Initial 77
        tmp_u8 = (uint8_t) tmp_u8;
        tmp_f32 = ((float)tmp_u8)*0.5F-50.0F;
        StandardMetrics.ms_v_env_temp->SetValue(tmp_f32); //working
        ESP_LOGV(TAG, "ms_v_env_temp: %f", tmp_f32);
      break;
      }
    case 0x3C0: //clamp status received
     {
      tmp_u8 = 
      ((uint8_t) (d[2] & 0x1) << 0) |
      0; // KL_S Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
      tmp_u8 = (uint8_t) tmp_u8;

      tmp_u8 = 
      ((uint8_t) (d[2] & 0x2) >> 1) |
      0; // KL_15 Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
      tmp_u8 = (uint8_t) tmp_u8;

      tmp_u8 = 
      ((uint8_t) (d[2] & 0x4) >> 2) |
      0; // KL_X Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
      tmp_u8 = (uint8_t) tmp_u8;

      tmp_u8 = 
      ((uint8_t) (d[2] & 0x8) >> 3) |
      0; // KL_50 Startanforderung Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
      tmp_u8 = (uint8_t) tmp_u8;

      tmp_u8 = 
      ((uint8_t) (d[2] & 0x10) >> 4) |
      0; // Remotestart Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
      tmp_u8 = (uint8_t) tmp_u8;

      tmp_u8 = 
      ((uint8_t) (d[2] & 0x20) >> 5) |
      0; // KL_Infotainment Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
      tmp_u8 = (uint8_t) tmp_u8;

      tmp_u8 = 
      ((uint8_t) (d[2] & 0x40) >> 6) |
      0; // Remotestart_KL15_Anf Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
      tmp_u8 = (uint8_t) tmp_u8;

      tmp_u8 = 
      ((uint8_t) (d[2] & 0x80) >> 7) |
      0; // Remotestart_Motor_Start Faktor 1 Offset 0, Minimum 0, Maximum 1 [] Initial 0
      tmp_u8 = (uint8_t) tmp_u8;

     break;
     }
    case 0x6B4: //VIN
     {
     break;
     }
    default:
    {
      //only for debug log ALL Incoming frames As i didn't know what frames are coming in after wakeup had to log all
      //only all unknown
      //ESP_LOGI(TAG, "T26: timestamp: %.24i 3R29 %12x %02x %02x %02x %02x %02x %02x %02x %02x", StandardMetrics.ms_m_monotonic->AsInt(), p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
    }
  }
}
  
    // ms_v_env_awake;                     // Vehicle is fully awake (switched on by the user)
    // ms_v_env_on;                        // Vehicle is in "ignition" state (drivable)
  

  //0x12dd546f BCM_04... ansteuerung LED Ladeanzeige mit Dimmung und Farbe?







  //0x5B0 => TimeDate


void OvmsVehicleVWeGolf::Ticker1(uint32_t ticker)
{
  //10 seconds after last received message we assume that the car is sleeping
  m_is_car_online = m_last_message_received < 10;

  m_last_message_received = std::min(m_last_message_received + 1, 254);
  ESP_LOGV(TAG, "m_last_message_received: %u", m_last_message_received);

  if(m_is_control_active && m_is_car_online_b) //after wakeup the other ECUs waiting for the car to be online before we send some Heartbeat messages otherwise we have some serious txerrors just before the car ist active
  {
    SendOcuHeartbeat();//working
    ESP_LOGV(TAG, "Heartbeat sending triggered");
  }
}

void OvmsVehicleVWeGolf::Ticker10(uint32_t ticker)
{

//working
  m_temperature_web = MyConfig.GetParamValueInt("xvg", "cc_temp", 21);
  m_is_charging_on_battery = (MyConfig.GetParamValueBool("xvg", "cc_onbat", false) ? 1 : 0);
  m_mirror_fold_in_requested = (MyConfig.GetParamValueBool("xvg", "control_mirror", false) ? 1 : 0);
  m_web_horn_requested = (MyConfig.GetParamValueBool("xvg", "control_horn", false) ? 1 : 0);
  m_web_indicators_requested = (MyConfig.GetParamValueBool("xvg", "control_indicator", false) ? 1 : 0);
  m_web_panic_mode_requested = (MyConfig.GetParamValueBool("xvg", "control_panicMode", false) ? 1 : 0);
  m_web_unlock_requested = (MyConfig.GetParamValueBool("xvg", "control_unlock", false) ? 1 : 0);
  m_web_lock_requested = (MyConfig.GetParamValueBool("xvg", "control_lock", false) ? 1 : 0);


  ESP_LOGV(TAG, "Trigger10 cc_temp: %u °C, cc_onbat: %u, control_mirror %u, control_horn: %u, control_indicator: %u, control_panicMode: %u, control_unlock %u, control_lock %u", m_temperature_web, m_is_charging_on_battery, m_mirror_fold_in_requested, m_web_horn_requested, m_web_indicators_requested, m_web_panic_mode_requested, m_web_unlock_requested, m_web_lock_requested);

  MyConfig.SetParamValueBool("xvg", "control_mirror", false);
  MyConfig.SetParamValueBool("xvg", "control_horn", false);
  MyConfig.SetParamValueBool("xvg", "control_indicator", false);
  MyConfig.SetParamValueBool("xvg", "control_panicMode", false);
  MyConfig.SetParamValueBool("xvg", "control_unlokc", false);
  MyConfig.SetParamValueBool("xvg", "control_lock", false);
//working

}

void OvmsVehicleVWeGolf::Ticker60(uint32_t ticker)
{
}

  
OvmsVehicle::vehicle_command_t OvmsVehicleVWeGolf::CommandWakeup()
{
  ESP_LOGV(TAG, "Wakeup triggered");

//Info: eGolf300 after sending only the heartbeat message ID:0x5A7 or the first message here ID:0x17330301 one ECU with ID:0x5F5 is answering on the bus perhaps ID:0x66E and ID:0x6B5 too
//Info: perhaps this could be used for another method to wakeup the car comf CAN perhaps changing the settings is possible without weaking up everything

  if(!m_is_car_online_b)
  {
    ESP_LOGI(TAG, "Car is sleeping we are trying to wake it up");
    // Wake up the Bus //CLI: can can3 tx extended 0x17330301 0x40 0x00 0x01 0x1F 0x00 0x00 0x00 0x00
    canbus *comfBus;
    comfBus = m_can3;
    uint8_t length = 8;
    uint8_t data[length];
    data[0] = 0x40;
    data[1] = 0x00;
    data[2] = 0x01;
    data[3] = 0x1F;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    comfBus->WriteExtended(0x17330301, length, data);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGV(TAG, "First message send ID: data 0->7");
    ESP_LOGV(TAG, "First message send ID:0x17330301 data %02x %02x %02x %02x %02x %02x %02x %02x", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

    vTaskDelay(pdMS_TO_TICKS(100));
    length = 8;
    data[0] = 0x67; //source node identifier identity of the transmitter of the message
    data[1] = 0x10; //could be anything
    data[2] = 0x41; //0-5 => State,  6 => eCall Car Wakeup
    data[3] = 0x84; //0-8 => eCall Wakeup
    data[4] = 0x14;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    comfBus->WriteExtended(0x1B000067, length, data);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGV(TAG, "second message send ID: data 0->7");
    ESP_LOGV(TAG, "second message send ID:0x1B000067 data %02x %02x %02x %02x %02x %02x %02x %02x", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

    m_is_control_active = true;
  } else {
    ESP_LOGI(TAG, "Wakeup not necessary car was online before");
  }
  return Success;
}


void OvmsVehicleVWeGolf::SendOcuHeartbeat()
{
  uint8_t tmp_u8 = 0;
  uint16_t tmp_u16 = 0;
  // uint32_t tmp_u32 = 0;
  // float tmp_f32 = 0.0F;

  canbus *comfBus;
  comfBus = m_can3;
  uint8_t length = 8;
  uint8_t data[length];
  length = 8;
  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;


  //Spiegelanklappen
  if(m_mirror_fold_in_requested)
  {
    tmp_u8 = 1;
    data[5] = (((uint8_t) tmp_u8) << 7) & 0x80;
    ESP_LOGI(TAG, "Spiegelanklappen");
  }

  //Hupen
  if(m_web_horn_requested)
  {
    tmp_u8 = 1;
    data[6] = (((uint8_t) tmp_u8) >> 0) & 0x1;
    ESP_LOGI(TAG, "Hupen");
  }

  //Door Lock //TODO there must be some vehicle specific identification send together with this signal so not working OOB
  if(m_web_lock_requested >= 1)
  {
    tmp_u8 = 1;
    data[6] = (((uint8_t) tmp_u8) << 1) & 0x2;
    ESP_LOGI(TAG, "DoorLock");
  }

  //Door Unlock //TODO there must be some vehicle specific identification send together with this signal so not working OOB
  if(m_web_unlock_requested)
  {
    tmp_u8 = 1;
    data[6] = (((uint8_t) tmp_u8) << 2) & 0x4;
    ESP_LOGI(TAG, "DoorUnlock");
  }

  //Warnblinken
  if(m_web_indicators_requested)
  {
    tmp_u8 = 1;
    data[6] = (((uint8_t) tmp_u8) << 3) & 0x8;
    ESP_LOGI(TAG, "Warnblinken");
  }

  //Panicalarm
  if(m_web_panic_mode_requested)
  {
    tmp_u8 = 1;
    data[6] = (((uint8_t) tmp_u8) << 4) & 0x10;
    ESP_LOGI(TAG, "PanicAlarm!");
  }

  //signature
  // It appears like signature is always zero, so we never go in this if block
  // If we somehow do go in, it's always a constant value
  static volatile uint16_t signature_u16 = 0;
  if(signature_u16 > 2047)
  {
    tmp_u16 = 2047;
    data[6] = (((uint16_t) tmp_u16) << 5) & 0xe0;
    data[7] = (((uint16_t) tmp_u16) >> 3) & 0xff;
    signature_u16 = 0;
    ESP_LOGI(TAG, "signature_u16 %u", signature_u16);
  }



  length = 8;
  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00; //TODO after debugging let the if statements decide which data should be here
  data[6] = 0x00; //TODO after debugging let the if statements decide which data should be here
  data[7] = 0x00;
  comfBus->WriteStandard(0x5A7, length, data);
  vTaskDelay(pdMS_TO_TICKS(50));
  ESP_LOGV(TAG, "Heartbeat send ID: data 0->7");
  ESP_LOGI(TAG, "FHeartbeat send ID:0x5A7 data %02x %02x %02x %02x %02x %02x %02x %02x", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
}
