/*
;    Project:       Open Vehicle Monitor System
;    Date:          16th August 2019
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2019       Marko Juhanne
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
static const char *TAG = "v-voltampera";

#include <stdio.h>
#include "ovms_config.h"
#include "vehicle_voltampera.h"
#include "canutils.h"

#define VA_PREHEAT_STOPPED 		0
#define VA_PREHEAT_STARTING 	1
#define VA_PREHEAT_SWITCH_ON 	2 // Used only for overriding BCM
#define VA_PREHEAT_STARTED 		3
#define VA_PREHEAT_STOPPING 	4
#define VA_PREHEAT_ERROR			5
#define VA_PREHEAT_RELINQUISH	6 // Used only when preheating is activated via key fob while BCM overriding is on-going

const char* const va_preheat_status_strings[] = {
	"Stopped",
	"Starting",
	"Switch on",
	"Started",
	"Stopping",
	"Error" };

#define VA_PREHEAT_TIMEOUT 					15 //seconds
#define VA_PREHEAT_MAX_TIME_DEFAULT 20 // minutes
#define VA_PREHEAT_MAX_RETRIES 			3
#define VA_PREHEAT_LIGHTS_ACTIVE (va_light_t)(Park) 
#define VA_PREHEAT_LIGHTS_STARTSTOP (va_light_t)(Driver_front_signal | Passenger_front_signal | Left_rear_signal | Right_rear_signal )
#define VA_PREHEAT_LIGHTS_ERROR VA_PREHEAT_LIGHTS_STARTSTOP

#define BCM_OVERRIDE_IGNORE_PERIOD 		3000 // milliseconds
#define BCM_OVERRIDE_RELAUNCH_PERIOD 10000 // milliseconds


const char * OvmsVehicleVoltAmpera::PreheatStatus()
	{
	return va_preheat_status_strings[ mt_preheat_status->AsInt() ];
	}

void OvmsVehicleVoltAmpera::ClimateControlInit()
 	{
	// New VA specific metrics
	mt_coolant_temp         = MyMetrics.InitInt("xva.v.e.coolant_temp", SM_STALE_MIN, 0);
	mt_coolant_heater_pwr   = MyMetrics.InitFloat("xva.v.e.coolant_heater_pwr", SM_STALE_MIN, 0);
	mt_preheat_status       = MyMetrics.InitInt("xva.v.ac.preheat", SM_STALE_MIN, VA_PREHEAT_STOPPED);
	mt_preheat_timer        = MyMetrics.InitInt("xva.v.ac.preheat_timer", SM_STALE_MIN, 0);
	mt_ac_active            = MyMetrics.InitBool("xva.v.ac.active", SM_STALE_MIN, 0);
	mt_ac_front_blower_fan_speed = MyMetrics.InitInt("xva.v.ac.front_blower_fan_speed", SM_STALE_MIN, 0);

	m_preheat_commander = Disabled;
	}

void OvmsVehicleVoltAmpera::ClimateControlPrintStatus(int verbosity, OvmsWriter* writer)
	{
  writer->printf("Preheat status: %s - Cabin temperature %2.1f\n", PreheatStatus(), StandardMetrics.ms_v_env_cabintemp->AsFloat());
	}

void OvmsVehicleVoltAmpera::ClimateControlIncomingSWCAN(CAN_frame_t* p_frame)
	{
	uint8_t *d = p_frame->data.u8;
	bool unexpected_data = false;

	// Process the incoming message
	switch (p_frame->MsgID)
		{

    // Front seat heat cool control
    case 0x107220A9:
      {
      if ( (d[0]==0) && (d[1]==0) )
        ESP_LOGD(TAG,"Driver and passenger seat heater off");
      else
        ESP_LOGD(TAG,"Seat heaters %02x / %02x", d[2],d[3]);
      break;
      }

		// Fob status
		case 0x10718040: 
		  {
		  uint16_t fob_func = ((GET16(p_frame,0))>>1) & 0x3f;
		  bool fob_low_batt = d[1] & 0x1;
		  uint8_t fob_number = ((GET16(p_frame,0))>>7) & 0x3;
		  ESP_LOGD(TAG,"Fob key press. Fob #%d func #%d low_batt %d", fob_number, fob_func, fob_low_batt);
		  switch (fob_func)
		    {
		    case 1:
		      {
		      ESP_LOGI(TAG,"Fob key press: LOCK");
		      break;
		      }
		    case 3:
		      {
		      ESP_LOGI(TAG,"Fob key press: UNLOCK");
		      if (m_preheat_commander==OVMS)
		        {
		        // Turn off preheating
		      	ESP_LOGI(TAG,"<--- Turn off preheating due to fob unlock cmd ---->");
		        PreheatModeChange(VA_PREHEAT_STOPPING);
		        }
		      break;
		      }
		    case 8:
		      {
		      ESP_LOGI(TAG,"Fob key press: OPEN CHARGER PORT");
		      break;
		      }
		    case 11:
		      {
		      ESP_LOGI(TAG,"Fob key press: PREHEAT");
		      if (m_preheat_commander==OVMS)
		        {
		      	ESP_LOGI(TAG,"<--- Relinquish the control of the preheating to BCM ---->");
		        PreheatModeChange(VA_PREHEAT_RELINQUISH);
		        }
		      break;
		      }
		    case 12:
		      {
		      // This is sent when long pressing the warming button did not cause BCM to start preheating
		      ESP_LOGI(TAG,"Fob key press: WARM BTN LONG PRESS");
		      break;
		      }
		    default:
		      {
		      ESP_LOGW(TAG,"Fob key press: unknown %d",fob_func);
		      }
		    }
		  break;
		  }

    // Coolant Heater Status
    case 0x1047809D: 
      {
      mt_coolant_heater_pwr->SetValue((float)d[1]*0.02);
      break;
      }

    // Climate control general status
    case 0x10734099: 
      {
      uint8_t mode = (d[0] >> 4) & 3;
      // this flag is active when AC is turned on from dashboard, or when preheat is initiated via fob or via Onstar (but NOT when overriding BCM)
      if (mode==1)
        {
        mt_ac_active->SetValue(false);
        }
      else if (mode==2)
        {
        mt_ac_active->SetValue(true);
        }
      else
        unexpected_data = true;

      // Incorrect DBC entry: This is not absolute target temperature but some kind of differential temp. Needs more investigating..
      //mt_ac_target_temp->SetValue( (GET16(p_frame, 2) & 0x3ff) / 10 - 10);
      break;
      }

    // Climate Control Basic Status
    case 0x10814099: 
      {
      // Blower fan speed is the only reliable indicator (that I know of) of whether preheat is switched on or not, since
      // the Climate_control_general_status:ac_active flag is not set if we override BCM...
      bool ac_status = mt_ac_front_blower_fan_speed->AsInt() > 0;
      mt_ac_front_blower_fan_speed->SetValue( (float)d[1]*0.39 );

      AirConStatusUpdated(ac_status);

      // this flag is active only when turning AC on manually on the dashboard, but NOT when preheating
      //mt_ac_active->SetValue( (d[0]>>5)&1 );
      break;
    }

    // Alarm_2_Request_LS (provides us with estimated cabin temperature)
    case 0x10440099: 
      {
      // This is estimated roof surface temperature
      ESP_LOGI(TAG,"Cabin temp: %f",(float)d[5]/2 - 40);
      StandardMetrics.ms_v_env_cabintemp->SetValue( (float)d[5]/2 - 40 ); 

      // There is also estimated bulk internal air temperature, but for comfort I think we prefer the surface temperature
      //StandardMetrics.ms_v_env_cabintemp->SetValue( (float)d[4]/2 - 40 ); 

      // this flag is active only when turning AC on manually on the dashboard, but NOT when preheating
      //mt_ac_active->SetValue( (d[0]>>5)&1 );
      break;
    }

		// Remote start status (Preheating)
    case 0x10390040: 
      {
      if (d[0]==0)
        // Remote start off
        {
        if  ((mt_preheat_status->AsInt()==VA_PREHEAT_STARTING) || (mt_preheat_status->AsInt()==VA_PREHEAT_STARTED) )
          {
          if (m_preheat_commander==OVMS)
            {
            ESP_LOGW(TAG,"BCM attempted to turn off preheating. Overriding by sending new start message...");
            PreheatModeChange(VA_PREHEAT_SWITCH_ON);
            } 
          else 
          	{
          	if ( (m_preheat_commander==Onstar) && (mt_preheat_status->AsInt()==VA_PREHEAT_STARTING) )
	          	{
	            ESP_LOGW(TAG,"BCM sending preheating off msg while trying to start? Ignoring for now..");
	          	}
	          else
	            {
	            ESP_LOGW(TAG,"BCM switching preheat off (after maximum time-on?)");
	            PreheatModeChange(VA_PREHEAT_STOPPING);
			        }
            }
          }
        }
      else if ( (d[0]>>1) & 1)  
        // Remote start request
        {
        // Preheat switched on most likely via key fob
        ESP_LOGI(TAG,"Preheating initiated by BCM (via key fob?)");
        m_preheat_commander = Fob;
        PreheatModeChange(VA_PREHEAT_STARTING);
        }
      break;
      }

		 default:
		 	break;			
		}

	if (unexpected_data)
	ESP_LOGE(TAG,"IncomingFrameSWCan: Unexpected data! Id %08x: %02x %02x %02x %02x %02x %02x %02x %02x", 
	  p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );
	}


// Take care of handling state changes and event signaling. 
void OvmsVehicleVoltAmpera::PreheatModeChange( uint8_t preheat_status )
	{
	CAN_frame_t txframe;
	m_preheat_modechange_timer = 0;

  ESP_LOGW(TAG,"---- PreheatModeChange: %s -> %s ----", 
  	PreheatStatus(), va_preheat_status_strings[preheat_status]);

	switch (preheat_status)
    {
    case VA_PREHEAT_RELINQUISH:	// used only when preheating is started via key fob while BCM overriding is on-going
    	{
	      CommandLights(VA_PREHEAT_LIGHTS_ACTIVE,false);
      	m_preheat_commander=Fob;
      } // intentional fall-thru
	  case VA_PREHEAT_STARTING:
	    {
	    // Reset timer only if it's actually starting the first time (not cycled between started -> starting -> started)
	    if (mt_preheat_status->AsInt() == VA_PREHEAT_STOPPED)
	    	mt_preheat_timer->SetValue(0);

	    mt_preheat_status->SetValue(VA_PREHEAT_STARTING);
	    MyEvents.SignalEvent("vehicle.preheat.starting",NULL);
	  	} // ... intentional fall-thru
	  case VA_PREHEAT_SWITCH_ON:	// used only for overriding BCM
	  	{
	    if (m_preheat_commander == Onstar)
	    	{
	  		ESP_LOGI(TAG,"Turning preheating on via Onstar emulation");
	  		SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x80,0x01,0xff)
	  		vTaskDelay(360 / portTICK_PERIOD_MS);  
	  		SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x00,0x00,0xff)
	    	}
	    else if (m_preheat_commander == OVMS)
	    	{
	      ESP_LOGI(TAG,"Turn on preheat via overriding BCM");
	      //m_preheat_BCM_overridden = true;
	      //SEND_EXT_FRAME(p_swcan, txframe,  0x10390040, 1, 0x02)
	      //vTaskDelay(720 / portTICK_PERIOD_MS);
	      SEND_EXT_FRAME(p_swcan, txframe,  0x10390040, 1, 0x03)
	    	}
	    break;
	    }
	  case VA_PREHEAT_STARTED:
	    {
	    ESP_LOGI(TAG,"PreheatModeChange: Preheat started");
	    mt_preheat_status->SetValue(VA_PREHEAT_STARTED);
	    StandardMetrics.ms_v_env_hvac->SetValue(true);
	    MyEvents.SignalEvent("vehicle.preheat.started",NULL);
	    if (m_preheat_commander == OVMS)
	      {
	      // When overriding BCM, controlling lights is needed since BCM does not do it for us
	      CommandLights(VA_PREHEAT_LIGHTS_ACTIVE,true);
	      }
	    break;
	    }
	  case VA_PREHEAT_STOPPING:
	    {
	    MyEvents.SignalEvent("vehicle.preheat.stopping",NULL);
	    mt_preheat_status->SetValue(VA_PREHEAT_STOPPING);
	    if (m_preheat_commander == Onstar)
	    	{
	    	ESP_LOGI(TAG,"Turning preheating off via Onstar emulation");
	    	SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x40,0x01,0xff)
	    	vTaskDelay(360 / portTICK_PERIOD_MS);  
	    	SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x00,0x00,0xff)
	    	}
	    else if (m_preheat_commander == OVMS)
	    	{
	    	ESP_LOGI(TAG,"Turn off preheat via overriding BCM");
	    	SEND_EXT_FRAME(p_swcan, txframe,  0x10390040, 1, 0x00)
	    	}
	    break;
	    }
	  case VA_PREHEAT_STOPPED:
	    {
	    ESP_LOGI(TAG,"PreheatModeChange: Preheat stopped");
	    mt_preheat_status->SetValue(VA_PREHEAT_STOPPED);
	    StandardMetrics.ms_v_env_hvac->SetValue(false);
	    MyEvents.SignalEvent("vehicle.preheat.stopped",NULL);
	    mt_preheat_timer->SetValue(0);
	    if (m_preheat_commander == OVMS)
	      {
	      // When overriding BCM, controlling lights is needed since BCM does not do it for us
	      CommandLights(VA_PREHEAT_LIGHTS_ACTIVE,false);
	      }
	    m_preheat_commander = Disabled;
	    break;
	    }
	  case VA_PREHEAT_ERROR:
	    {
	    MyEvents.SignalEvent("vehicle.preheat.error", NULL);
	    FlashLights(VA_PREHEAT_LIGHTS_ERROR,300,5);
	    PreheatModeChange(VA_PREHEAT_STOPPED);
	    break;
	    }
    }
  }


// AC status changed
void OvmsVehicleVoltAmpera::AirConStatusUpdated( bool ac_enabled )
  {
  ESP_LOGD(TAG,"AirConStatusUpdated: ac_enabled %d, preheat_status: %s", ac_enabled, PreheatStatus());

  if (m_preheat_commander == Disabled)
  	return;

  if (ac_enabled)
    {
    if (mt_preheat_status->AsInt() == VA_PREHEAT_STARTING)
      {
      PreheatModeChange(VA_PREHEAT_STARTED);
      }
    } 
  else
    {
    if (mt_preheat_status->AsInt() == VA_PREHEAT_STOPPING)
      {
      PreheatModeChange(VA_PREHEAT_STOPPED);
      }
    else if (mt_preheat_status->AsInt() == VA_PREHEAT_STARTED)
      {
      if ( (m_preheat_commander == OVMS) && (m_preheat_modechange_timer < BCM_OVERRIDE_RELAUNCH_PERIOD))
        {
        ESP_LOGW(TAG,"AirConStatusUpdated: BCM managed to turn AC off despite our counter-attack. Relaunching..");
        PreheatModeChange(VA_PREHEAT_STARTING);
        }
      else
        {
        ESP_LOGW(TAG,"AirConStatusUpdated: Preheat stopped prematurily!");
        PreheatModeChange(VA_PREHEAT_ERROR);
        }
      }
    }
  }


void OvmsVehicleVoltAmpera::PreheatWatchdog()
  {
  if (mt_preheat_status->AsInt() == VA_PREHEAT_STARTED)
    {
    mt_preheat_timer->SetValue(mt_preheat_timer->AsInt()+1);
    }
  m_preheat_modechange_timer++;

  switch (mt_preheat_status->AsInt())
    {
    case VA_PREHEAT_STARTING:
      {
      if (m_preheat_modechange_timer > VA_PREHEAT_TIMEOUT)
        {
        // For some reason preheating did not (re)start.. 
        m_preheat_retry_counter++;
        if (m_preheat_retry_counter < VA_PREHEAT_MAX_RETRIES)
        	{
	        ESP_LOGW(TAG,"Preheat did not start! Retrying..");
          MyEvents.SignalEvent("vehicle.preheat.start_retry", NULL);
			    CommandWakeup();
			    vTaskDelay(720 / portTICK_PERIOD_MS);
    			PreheatModeChange(VA_PREHEAT_STARTING);
        	}
        else
        	{
	        ESP_LOGE(TAG,"Preheat did not start!");
	        PreheatModeChange(VA_PREHEAT_ERROR);
        	}
        }
      break;
      }
    case VA_PREHEAT_STARTED:
      {
      if (m_preheat_commander==OVMS)
        {
        unsigned int max_time = MyConfig.GetParamValueInt("xva", "preheat.max_time", VA_PREHEAT_MAX_TIME_DEFAULT);
        if (mt_preheat_timer->AsInt() > 60 * max_time)
          {
          ESP_LOGW(TAG,"Maximum preheat time reached. Stopping..");
          PreheatModeChange(VA_PREHEAT_STOPPING);
          }
        }
      break;
      }
    case VA_PREHEAT_STOPPING:
      {
      if (m_preheat_modechange_timer > VA_PREHEAT_TIMEOUT)
        {
        if (mt_ac_front_blower_fan_speed->IsStale())
          {
          // AC actually did stop, but we did not receive the last fan_speed=0 message. Assume AC is off
          ESP_LOGW(TAG,"AC status went stale... Assume AC is off");
          PreheatModeChange(VA_PREHEAT_STOPPED);
          }
        else
          {
          if (m_preheat_commander==OVMS)
						{
						ESP_LOGE(TAG,"Preheat did not stop! Retrying..");
	          MyEvents.SignalEvent("vehicle.preheat.error", NULL);
	          PreheatModeChange(VA_PREHEAT_STOPPING);
	          }
	        }
        }
      break;
      }
    default:
      break;
    }
  }


OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandClimateControl(bool enable)
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  ESP_LOGW(TAG,"<------ REMOTE CLIMATE CONTROL: %d (Preheat Status: %s) ------>", enable, PreheatStatus() );

  if ( ((mt_preheat_status->AsInt()!=VA_PREHEAT_STARTED) && (enable)) || 
       ((mt_preheat_status->AsInt()!=VA_PREHEAT_STOPPED) && (!enable) )  )
    {
    if (enable)
	    {
	    CommandWakeup();
	    vTaskDelay(720 / portTICK_PERIOD_MS);
	    }
    if (MyConfig.GetParamValueBool("xva", "preheat.override_bcm", false))
    	{
    	FlashLights(VA_PREHEAT_LIGHTS_STARTSTOP);
    	m_preheat_commander = OVMS;
    	}
    else
    	m_preheat_commander = Onstar;
    m_preheat_retry_counter = 0;

    PreheatModeChange(enable ? VA_PREHEAT_STARTING : VA_PREHEAT_STOPPING );
    } 
  else
    {
    ESP_LOGE(TAG,"Preheat already %s!", enable ? "active" : "inactive");
    return Fail;
    }
  return Success;
#else
  return NotImplemented;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }
