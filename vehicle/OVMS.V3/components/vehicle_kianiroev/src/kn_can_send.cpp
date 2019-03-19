/*
;    Project:       Open Vehicle Monitor System
;    Date:          21th January 2019
;
;    (C) 2019       Geir Øyvind Vælidalo <geir@validalo.net>
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
#include "vehicle_kianiroev.h"
#include "kia_common.h"

static const char *TAG = "v-kianiroev";

/**
 * Send a can message and wait for the response before continuing.
 * Time out after approx 0.5 second.
 */
bool OvmsVehicleKiaNiroEv::SendCanMessage_sync(uint16_t id, uint8_t count,
		uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
		uint8_t b5, uint8_t b6)
	{
	uint16_t timeout;

	kia_send_can.id = id;
	kia_send_can.status = 0xff;

	uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
	m_can1->WriteStandard(id, 8, data);
	ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

	//Now wait for the response
	timeout = 50; // 50*50 = 2500 ms
	do
		{
		/* Block for 50ms. */
		vTaskDelay( xDelay );
		} while (kia_send_can.status == 0xff && --timeout>0);

	//Did we get the response?
	if(timeout != 0 )
		{
		if( kia_send_can.byte[1]==serviceId+0x40 )
			{
			return true;
			}
		else
			{
			kia_send_can.status = 0x2;
			}
		}
	else
		{
		kia_send_can.status = 0x1; //Timeout
		}
	return false;
 }

void OvmsVehicleKiaNiroEv::SendCanMessage(uint16_t id, uint8_t count,
		uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
		uint8_t b5, uint8_t b6)
	{
	if(!kia_enable_write) return;

	uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
	m_can1->WriteStandard(id, 8, data);
	ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
 }

/**
 * Sends same message three times
 */
void OvmsVehicleKiaNiroEv::SendCanMessageTriple(uint16_t id, uint8_t count,
		uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
		uint8_t b5, uint8_t b6)
	{
	if(!kia_enable_write) return;

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
void OvmsVehicleKiaNiroEv::SetHeadLightDelay(bool on)
	{
//	if(kia_enable_write)
//		SendCanMessageTriple(0x014, 0,0, on ? 0x08:0x04 ,0,0,0,0,0);
	}

/**
 * Set one touch turn signal
 * 0 = off
 * 1 = 3 blink
 * 2 = 5 blink
 * 3 = 7 blink
 *
 */
void OvmsVehicleKiaNiroEv::	SetOneThouchTurnSignal(uint8_t value)
	{
//	if(kia_enable_write)
//		SendCanMessageTriple(0x014, (value+1)<<5 ,0,0,0,0,0,0,0);
	}

/**
 * Set Auto Door unlock setting
 * 1 = Off
 * 2 = Vehicle Off
 * 3 = On shift to P
 * 4 = Driver door unlock
 */
void OvmsVehicleKiaNiroEv::	SetAutoDoorUnlock(uint8_t value)
	{
//	if(kia_enable_write)
//		SendCanMessageTriple(0x014, 0,value,0,0,0,0,0,0);
	}

/**
 * Set Auto Door lock setting
 * 0 = Off
 * 1 = Enable on speed
 * 2 = Enable on Shift
 */
void OvmsVehicleKiaNiroEv::SetAutoDoorLock(uint8_t value)
	{
//	if(kia_enable_write)
//		SendCanMessageTriple(0x014, 0,(value+1)<<5,0,0,0,0,0,0);
	}

/**
 * Send tester present messages for all ECUs
 */
void OvmsVehicleKiaNiroEv::SendTesterPresentMessages()
	{
	if( !kn_shift_bits.Park ){
		StopTesterPresentMessages();
	}
	if( igmp_tester_present_seconds>0 )
		{
		igmp_tester_present_seconds--;
		SendTesterPresent(INTEGRATED_GATEWAY, 2);
		}
	if( bcm_tester_present_seconds>0 )
		{
		bcm_tester_present_seconds--;
		SendTesterPresent(BODY_CONTROL_MODULE, 2);
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
void OvmsVehicleKiaNiroEv::StopTesterPresentMessages()
	{
	igmp_tester_present_seconds = 0;
	bcm_tester_present_seconds = 0;
	smk_tester_present_seconds = 0;
	}


/**
 * Send a can message to ECU to notify that tester is present
 */
void OvmsVehicleKiaNiroEv::SendTesterPresent(uint16_t id, uint8_t length)
	{
	if(kia_enable_write)
		SendCanMessage(id, length,UDS_SID_TESTER_PRESENT, 0,0,0,0,0,0);
	}

/**
 * Send a can message to set ECU in a specific session mode
 */
bool OvmsVehicleKiaNiroEv::SetSessionMode(uint16_t id, uint8_t mode)
	{
	return SendCanMessage_sync(id, 2,VEHICLE_POLL_TYPE_OBDIISESSION, mode,0,0,0,0,0);
	}

/**
 * Put the car in proper session mode and then send the command
 */
bool OvmsVehicleKiaNiroEv::SendCommandInSessionMode(uint16_t id, uint8_t count, uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t mode )
	{
	if(!kia_enable_write) return false;

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
	SetSessionMode(id, UDS_DEFAULT_SESSION);
	return result;
	}

/**
 * Send command to IGMP
 * 770 04 2F [b1] [b2] [b3]
 */
bool OvmsVehicleKiaNiroEv::Send_IGMP_Command( uint8_t b1, uint8_t b2, uint8_t b3)
	{
	return SendCommandInSessionMode(INTEGRATED_GATEWAY, 4,UDS_SID_IOCTRL_BY_ID, b1, b2, b3, 0,0,0, UDS_EXTENDED_DIAGNOSTIC_SESSION );
	}

/**
 * Send command to Body Control Module
 * 7a0 04 2F [b1] [b2] [b3]
 */
bool OvmsVehicleKiaNiroEv::Send_BCM_Command( uint8_t b1, uint8_t b2, uint8_t b3)
	{
	return SendCommandInSessionMode(BODY_CONTROL_MODULE, 4,UDS_SID_IOCTRL_BY_ID, b1, b2, b3, 0,0,0, UDS_EXTENDED_DIAGNOSTIC_SESSION );
	}

/**
 * Send command to Smart Key Unit
 * 7a5 [b1] 2F [b2] [b3] [b4] [b5] [b6] [b7]
 */
bool OvmsVehicleKiaNiroEv::Send_SMK_Command( uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7)
	{
	return SendCommandInSessionMode(SMART_KEY_UNIT, b1,UDS_SID_IOCTRL_BY_ID, b2, b3, b4, b5, b6, b7, UDS_EXTENDED_DIAGNOSTIC_SESSION);
	}

/**
 * Send command to ABS & EBP Module
 * 7d5 03 30 [b1] [b2]
 */
bool OvmsVehicleKiaNiroEv::Send_EBP_Command( uint8_t b1, uint8_t b2, uint8_t mode)
	{
	return SendCommandInSessionMode(ABS_EBP_UNIT, 3,UDS_SID_IOCTRL_BY_LOC_ID, b1, b2, 0,0,0,0, mode );
	}
