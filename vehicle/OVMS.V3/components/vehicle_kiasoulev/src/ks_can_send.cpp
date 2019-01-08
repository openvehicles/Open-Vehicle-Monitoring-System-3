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
 * Send a can message and wait for the response before continuing.
 * Time out after approx 0.5 second.
 */
bool OvmsVehicleKiaSoulEv::SendCanMessage_sync(uint16_t id, uint8_t count,
		uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
		uint8_t b5, uint8_t b6)
	{
	uint16_t timeout;

	ks_send_can.id = id;
	ks_send_can.status = 0xff;

	uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
	m_can1->WriteStandard(id, 8, data);
	ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

	//Now wait for the response
	timeout = 50; // 50*50 = 2500 ms
	do
		{
		/* Block for 50ms. */
		vTaskDelay( xDelay );
		} while (ks_send_can.status == 0xff && --timeout>0);

	//Did we get the response?
	if(timeout != 0 )
		{
		if( ks_send_can.byte[1]==serviceId+0x40 )
			{
			return true;
			}
		else
			{
			ks_send_can.status = 0x2;
			}
		}
	else
		{
		ks_send_can.status = 0x1; //Timeout
		}
	return false;
 }

void OvmsVehicleKiaSoulEv::SendCanMessage(uint16_t id, uint8_t count,
		uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
		uint8_t b5, uint8_t b6)
	{
	if(!ks_enable_write) return;

	uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
	m_can1->WriteStandard(id, 8, data);
	ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
 }

/**
 * Sends same message three times
 */
void OvmsVehicleKiaSoulEv::SendCanMessageTriple(uint16_t id, uint8_t count,
		uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
		uint8_t b5, uint8_t b6)
	{
	if(!ks_enable_write) return;

	uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
	m_can1->WriteStandard(id, 8, data);
	vTaskDelay( xDelay );
	m_can1->WriteStandard(id, 8, data);
	vTaskDelay( xDelay );
	m_can1->WriteStandard(id, 8, data);
	ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
 }

/**
 *  Turns on/off head light delay
 */
void OvmsVehicleKiaSoulEv::SetHeadLightDelay(bool on)
	{
	if(ks_enable_write)
		SendCanMessageTriple(0x014, 0,0, on ? 0x08:0x04 ,0,0,0,0,0);
	}

/**
 * Set one touch turn signal
 * 0 = off
 * 1 = 3 blink
 * 2 = 5 blink
 * 3 = 7 blink
 *
 */
void OvmsVehicleKiaSoulEv::	SetOneThouchTurnSignal(uint8_t value)
	{
	if(ks_enable_write)
		SendCanMessageTriple(0x014, (value+1)<<5 ,0,0,0,0,0,0,0);
	}

/**
 * Set Auto Door unlock setting
 * 1 = Off
 * 2 = Vehicle Off
 * 3 = On shift to P
 * 4 = Driver door unlock
 */
void OvmsVehicleKiaSoulEv::	SetAutoDoorUnlock(uint8_t value)
	{
	if(ks_enable_write)
		SendCanMessageTriple(0x014, 0,value,0,0,0,0,0,0);
	}

/**
 * Set Auto Door lock setting
 * 0 = Off
 * 1 = Enable on speed
 * 2 = Enable on Shift
 */
void OvmsVehicleKiaSoulEv::SetAutoDoorLock(uint8_t value)
	{
	if(ks_enable_write)
		SendCanMessageTriple(0x014, 0,(value+1)<<5,0,0,0,0,0,0);
	}

/**
 * Fetches the door lock status from smart junction box
 * -1 : Status unknown
 * 0: Booth doors unlocked
 * 1: Left front door unlocked
 * 2: Right front door unlocked
 * 3: Booth doors locked
 */
int8_t OvmsVehicleKiaSoulEv::GetDoorLockStatus()
	{
	if(!ks_enable_write) return -1;
	int8_t result = -1;
	SendTesterPresent(SMART_JUNCTION_BOX,2);
	vTaskDelay( xDelay );
	char buffer[6];
	ACCRelay(true, itoa(MyConfig.GetParamValueInt("password","pin"), buffer, 10));
	if( SetSessionMode(SMART_JUNCTION_BOX, KS_90_DIAGNOSTIC_SESSION ))
		{
		 SendCanMessage_sync(SMART_JUNCTION_BOX, 0x03, 0x22, 0xbc, 0x04, 0, 0, 0, 0 );
		 if( ks_send_can.byte[0]==0x10 && ks_send_can.byte[1]==0x0b && ks_send_can.byte[2]==0x62  )
			{
			SendCanMessage_sync(SMART_JUNCTION_BOX, 0x30, 0x00, 0x0a, 0, 0, 0, 0, 0 );
			if( ks_send_can.byte[0]==0x21 && ks_send_can.byte[1]==0xc2 )
				{
				result = (ks_send_can.byte[2] & 0xC)>>2;
				}
			}
		}
	SetSessionMode(SMART_JUNCTION_BOX, DEFAULT_SESSION);

	if( result==3)
		{
		StdMetrics.ms_v_env_locked->SetValue(true);
		}
	else if( result<3)
		{
		StdMetrics.ms_v_env_locked->SetValue(false);
		}

	return result;
	}

/**
 * Send tester present messages for all ECUs
 */
void OvmsVehicleKiaSoulEv::SendTesterPresentMessages()
	{
	if( !ks_shift_bits.Park ){
		StopTesterPresentMessages();
	}
	if( sjb_tester_present_seconds>0 )
		{
		sjb_tester_present_seconds--;
		SendTesterPresent(SMART_JUNCTION_BOX, 2);
		}

	if( smk_tester_present_seconds>0 )
		{
		smk_tester_present_seconds--;
		SendTesterPresent(SMART_KEY_UNIT, 2);
		}
	}

/**
 * Stop all tester present massages for all ECUs
 */
void OvmsVehicleKiaSoulEv::StopTesterPresentMessages()
	{
	sjb_tester_present_seconds = 0;
	smk_tester_present_seconds = 0;
	}


/**
 * Send a can message to ECU to notify that tester is present
 */
void OvmsVehicleKiaSoulEv::SendTesterPresent(uint16_t id, uint8_t length)
	{
	if(ks_enable_write)
		SendCanMessage(id, length,VEHICLE_POLL_TYPE_OBDII_TESTER_PRESENT, 0,0,0,0,0,0);
	}

/**
 * Send a can message to set ECU in a specific session mode
 */
bool OvmsVehicleKiaSoulEv::SetSessionMode(uint16_t id, uint8_t mode)
	{
	return SendCanMessage_sync(id, 2,VEHICLE_POLL_TYPE_OBDIISESSION, mode,0,0,0,0,0);
	}

/**
 * Put the car in proper session mode and then send the command
 */
bool OvmsVehicleKiaSoulEv::SendCommandInSessionMode(uint16_t id, uint8_t count, uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t mode )
	{
	if(!ks_enable_write) return false;

	bool result = false;
	SendTesterPresent(id,2);
	vTaskDelay( xDelay );
	if( SetSessionMode(id, mode ))
		{
		vTaskDelay( xDelay );
		SendTesterPresent(id,2);
		vTaskDelay( xDelay );
		result = SendCanMessage_sync(id, count, serviceId, b1, b2, b3, b4, b5, b6 );
		}
	SetSessionMode(id, DEFAULT_SESSION);
	return result;
	}

/**
 * Send command to Smart Junction Box
 * 771 04 2F [b1] [b2] [b3]
 */
bool OvmsVehicleKiaSoulEv::Send_SJB_Command( uint8_t b1, uint8_t b2, uint8_t b3)
	{
	return SendCommandInSessionMode(SMART_JUNCTION_BOX, 4,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID, b1, b2, b3, 0,0,0, EXTENDED_DIAGNOSTIC_SESSION );
	}

/**
 * Send command to Body Control Module
 * 7a0 04 2F [b1] [b2] [b3]
 */
bool OvmsVehicleKiaSoulEv::Send_BCM_Command( uint8_t b1, uint8_t b2, uint8_t b3)
	{
	return SendCommandInSessionMode(BODY_CONTROL_MODULE, 4,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID, b1, b2, b3, 0,0,0, EXTENDED_DIAGNOSTIC_SESSION );
	}

/**
 * Send command to Smart Key Unit
 * 7a5 [b1] 2F [b2] [b3] [b4] [b5] [b6] [b7]
 */
bool OvmsVehicleKiaSoulEv::Send_SMK_Command( uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7)
	{
	return SendCommandInSessionMode(SMART_KEY_UNIT, b1,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID, b2, b3, b4, b5, b6, b7, EXTENDED_DIAGNOSTIC_SESSION);
	}

/**
 * Send command to ABS & EBP Module
 * 7d5 03 30 [b1] [b2]
 */
bool OvmsVehicleKiaSoulEv::Send_EBP_Command( uint8_t b1, uint8_t b2, uint8_t mode)
	{
	return SendCommandInSessionMode(ABS_EBP_UNIT, 3,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_LOC_ID, b1, b2, 0,0,0,0, mode );
	}
