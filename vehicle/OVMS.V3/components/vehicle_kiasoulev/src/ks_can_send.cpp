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
	uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
	m_can1->WriteStandard(id, 8, data);
	ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
 }

/**
 * Send a can message to ECU to notify that tester is present
 */
void OvmsVehicleKiaSoulEv::SendTesterPresent(uint16_t id, uint8_t length)
	{
  return SendCanMessage(id, length,VEHICLE_POLL_TYPE_OBDII_TESTER_PRESENT, 0,0,0,0,0,0);
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
bool OvmsVehicleKiaSoulEv::SendCommandInSessionMode(uint16_t id, uint8_t count, uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6 )
	{
	bool result = false;
	SendTesterPresent(id,2);
	vTaskDelay( xDelay );
	if( SetSessionMode(id, EXTENDED_DIAGNOSTIC_SESSION))
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
	return SendCommandInSessionMode(SMART_JUNCTION_BOX, 4,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID, b1, b2, b3, 0,0,0 );
	}

/**
 * Send command to Body Control Module
 * 7a0 04 2F [b1] [b2] [b3]
 */
bool OvmsVehicleKiaSoulEv::Send_BCM_Command( uint8_t b1, uint8_t b2, uint8_t b3)
	{
	return SendCommandInSessionMode(BODY_CONTROL_MODULE, 4,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID, b1, b2, b3, 0,0,0 );
	}

/**
 * Send command to Smart Key Unit
 * 7a5 [b1] 2F [b2] [b3] [b4] [b5] [b6] [b7]
 */
bool OvmsVehicleKiaSoulEv::Send_SMK_Command( uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7)
	{
	return SendCommandInSessionMode(SMART_KEY_UNIT, b1,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID, b2, b3, b4, b5, b6, b7);
	}

/**
 * Send command to ABS & EBP Module
 * 7d5 03 30 [b1] [b2]
 */
bool OvmsVehicleKiaSoulEv::Send_EBP_Command( uint8_t b1, uint8_t b2)
	{
	return SendCommandInSessionMode(ABS_EBP_UNIT, 3,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_LOC_ID, b1, b2, 0,0,0,0 );
	}
