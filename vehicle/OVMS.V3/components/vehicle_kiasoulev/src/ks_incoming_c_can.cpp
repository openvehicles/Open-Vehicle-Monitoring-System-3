/*
;    Project:       Open Vehicle Monitor System
;    Date:          29th December 2017
;
;    (C) 2017       Geir Øyvind Vælidalo <geir@validalo.net>
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
#include "vehicle_kiasoulev.h"

static const char *TAG = "v-kiasoulev";

/**
 * Handles incoming CAN-frames on bus 1, the C-bus
 */
void OvmsVehicleKiaSoulEv::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

  switch (p_frame->MsgID)
    {
  	  case 0x018:
  	   {
       // Doors status Byte 0, 4 & 5:
  	   StdMetrics.ms_v_door_chargeport->SetValue((d[0] & 0x01) > 0); 	//Byte 0 - Bit 0
  	   StdMetrics.ms_v_door_fl->SetValue((d[0] & 0x10) > 0);					//Byte 0 - Bit 4
  	   StdMetrics.ms_v_door_fr->SetValue((d[0] & 0x80) > 0);					//Byte 0 - Bit 7
  	   StdMetrics.ms_v_door_rl->SetValue((d[4] & 0x08) > 0);					//Byte 4 - Bit 3
  	   StdMetrics.ms_v_door_rr->SetValue((d[4] & 0x02) > 0); 					//Byte 4 - Bit 1

  	   StdMetrics.ms_v_door_trunk->SetValue((d[5] & 0x80) > 0); 			//Byte 5 - Bit 7

  	   // Light status Byte 2,4 & 5
  	   m_v_env_lowbeam->SetValue( (d[2] & 0x01)>0 );		//Byte 2 - Bit 0 - Low beam
  	   m_v_env_highbeam->SetValue( (d[2] & 0x02)>0 );		//Byte 2 - Bit 1 - High beam
  	   StdMetrics.ms_v_env_headlights->SetValue( (d[4] & 0x01)>0 );		//Byte 4 - Bit 0 - Day lights

  	   //byte 5 - Bit 6 - Left indicator light
  	   //byte 5 - Bit 5 - Right indicator light

  	   // Seat belt status Byte 7
  	   }
     break;

  	  /*case 0x110:
  	   {
  	   StdMetrics.ms_v_env_headlights->SetValue( (d[3] & 0x80)>0 );
  	   }
     break;
*/
  	  case 0x120: //Locks
  	    {
			if( d[3] & 0x20)
				{
				StdMetrics.ms_v_env_locked->SetValue(false); //Todo This only keeps track of the lock signal from keyfob
				}
			else if( d[3] & 0x10 )
				{
				StdMetrics.ms_v_env_locked->SetValue(true); //Todo This only keeps track of the lock signal from keyfob
				}
			if( d[3] & 0x40 && ks_key_fob_open_charge_port )
				{
				ks_openChargePort = true;
				}
  	    }
  	    break;

      case 0x200:
        {
          // Estimated range:
          //TODO   float ekstraRange = d[0]/10.0;  //Ekstra range when heating is off.
          uint16_t estRange = d[2] + ((d[1] & 1)<<9);
          if (estRange > 0){
        	    StdMetrics.ms_v_bat_range_est->SetValue((float)(estRange<<1), Kilometers );
          }
        }
        break;

      case 0x433:
        {
        // Parking brake status
        StdMetrics.ms_v_env_handbrake->SetValue((d[2] & 0x08) > 0); //Why did I have 0x10 here??
        }
        break;

      //case 0x4b0:
      //  {
        // Motor RPM based on wheel rotation
        //int rpm = (d[0]+(d[1]<<8)) * 8.206;
        //StdMetrics.ms_v_mot_rpm->SetValue( rpm );
      //  }
      //  break;

      case 0x4f0:
        {
          // Odometer:
          //if (/*length = 8 &&*/ (d[7] || d[6] || d[5])) { //TODO can_datalength was needed in V2
            StdMetrics.ms_v_pos_odometer->SetValue( (float)((d[7]<<16) + (d[6]<<8) + d[5])/10.0, Kilometers);
          //}
        }
        break;

      case 0x4f2:
      		{
      		// Speed:
      		StdMetrics.ms_v_pos_speed->SetValue( d[1] >> 1, Kph ); // kph

      		// 4f2 is one of the few can-messages that are sent while car is both on and off.
      		// Byte 2 and 7 are some sort of counters which runs while the car is on.
      		if (d[2] > 0 || d[7] > 0)
      			{
      			vehicle_kiasoulev_car_on(true);
      			}
      		else if (d[2] == 0 && d[7] == 0 && StdMetrics.ms_v_pos_speed->AsFloat(Kph) == 0 && StdMetrics.ms_v_env_handbrake->AsBool())
      			{
           // Boths 2 and 7 and speed is 0, so we assumes the car is off.
      	     vehicle_kiasoulev_car_on(false);
      			}
        }
        break;

      case 0x542:
        {
        // SOC:
        if (d[0] > 0)
        	  StdMetrics.ms_v_bat_soc->SetValue( d[0] >> 1 ); //In V2 car_SOC
        }
        break;

      case 0x581:
        {
        // Car is CHARGING:
        StdMetrics.ms_v_bat_power->SetValue((float)(((uint16_t) d[7] << 8) | d[6])/256.0, kW);
        }
        break;

      case 0x594:
        {
          // BMS SOC:
          uint16_t val = (uint16_t) d[5] * 5 + d[6];
          if (val > 0){
            ks_bms_soc = (uint8_t) (val / 10);
          }
        }
        break;

      case 0x653:
        {
        // AMBIENT TEMPERATURE:
        StdMetrics.ms_v_env_temp->SetValue( TO_CELCIUS(d[5]/2.0), Celcius);
        }
        break;

      case 0x654:
        {
        // INSIDE TEMPERATURE:
        m_v_env_inside_temp->SetValue( TO_CELCIUS(d[7]/2.0), Celcius);
        }
        break;

      case 0x779:
      	 {
   		 ESP_LOGD(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      	 }
      	 break;
    }

  	// Check if response is from synchronous can message
	if(p_frame->MsgID==(ks_send_can.id+0x08) && ks_send_can.status==0xff)
		{
		//Store message bytes so that the async method can continue
		ks_send_can.status=3;

		ks_send_can.byte[0]=d[0];
		ks_send_can.byte[1]=d[1];
		ks_send_can.byte[2]=d[2];
		ks_send_can.byte[3]=d[3];
		ks_send_can.byte[4]=d[4];
		ks_send_can.byte[5]=d[5];
		ks_send_can.byte[6]=d[6];
		ks_send_can.byte[7]=d[7];
		}
  }

