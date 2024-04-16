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
#include "vehicle_neta_aya.h"
#include "common.h"

static const char *TAG = "v-netaAya";

/**
 * Send a can message and wait for the response before continuing.
 * Time out after approx 0.5 second.
 */
bool OvmsVehicleNetaAya::SendCanMessage_sync(uint16_t id, uint8_t count,
												  uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
												  uint8_t b5, uint8_t b6)
{
	uint16_t timeout;

	message_send_can.id = id;
	message_send_can.status = 0xff;

	uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
	m_can1->WriteStandard(id, 8, data);
	ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

	// Now wait for the response
	timeout = 5; // 50*5 = 250 ms
	do
	{
		/* Block for 50ms. */
		vTaskDelay(xDelay);
	} while (message_send_can.status == 0xff && --timeout > 0);

	// Did we get the response?
	
	if (timeout != 0)
	{
		if (message_send_can.byte[1] == serviceId + 0x40)
		{
			ESP_LOGV(TAG, "RESP: %02x %02x %02x %02x %02x %02x %02x %02x", message_send_can.byte[0], message_send_can.byte[1], message_send_can.byte[2], message_send_can.byte[3], message_send_can.byte[4], message_send_can.byte[5], message_send_can.byte[6], message_send_can.byte[7]);
			return true;
		}
		else
		{
			message_send_can.status = 0x2;
		}
	}
	else
	{
		message_send_can.status = 0x1; // Timeout
	}
	ESP_LOGV(TAG, "RESP: %02x %02x %02x %02x %02x %02x %02x %02x", message_send_can.byte[0], message_send_can.byte[1], message_send_can.byte[2], message_send_can.byte[3], message_send_can.byte[4], message_send_can.byte[5], message_send_can.byte[6], message_send_can.byte[7]);
	return false;
}

void OvmsVehicleNetaAya::SendCanMessage(uint16_t id, uint8_t count,
											 uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
											 uint8_t b5, uint8_t b6)
{
	message_send_can.id = id;
	message_send_can.status = 0xff;
	uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
	m_can1->WriteStandard(id, 8, data);
	ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
}

void OvmsVehicleNetaAya::SendCanMessageSecondary(uint16_t id, uint8_t count,
											 uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
											 uint8_t b5, uint8_t b6)
{	
	uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
	m_can2->WriteStandard(id, 8, data);
	ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
}

/**
 * Sends same message three times
 */
void OvmsVehicleNetaAya::SendCanMessageTriple(uint16_t id, uint8_t count,
												   uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
												   uint8_t b5, uint8_t b6)
{

	uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
	m_can1->WriteStandard(id, 8, data);
	vTaskDelay(xDelay);
	m_can1->WriteStandard(id, 8, data);
	vTaskDelay(xDelay);
	m_can1->WriteStandard(id, 8, data);
	ESP_LOGV(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
}

/**
 * Send tester present messages for all ECUs
 */
void OvmsVehicleNetaAya::SendTesterPresentMessages()
{
	// SendTesterPresent(INTEGRATED_GATEWAY, 2);

}

/**
 * Send a can message to ECU to notify that tester is present
 */
void OvmsVehicleNetaAya::SendTesterPresent(uint16_t id, uint8_t length)
{
	SendCanMessage(id, length, UDS_SID_TESTER_PRESENT, 0, 0, 0, 0, 0, 0);
}

/**
 * Send a can message to set ECU in a specific session mode
 */
bool OvmsVehicleNetaAya::SetSessionMode(uint16_t id, uint8_t mode)
{
	return SendCanMessage_sync(id, 2, VEHICLE_POLL_TYPE_OBDIISESSION, mode, 0, 0, 0, 0, 0);
}

/**
 * Put the car in proper session mode and then send the command
 */
bool OvmsVehicleNetaAya::SendCommandInSessionMode(uint16_t id, uint8_t count, uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t mode)
{
	bool result = false;
	SendTesterPresent(id, 2);
	vTaskDelay(xDelay);
	if (SetSessionMode(id, mode))
	{
		vTaskDelay(xDelay);
		SendTesterPresent(id, 2);
		vTaskDelay(xDelay);
		result = SendCanMessage_sync(id, count, serviceId, b1, b2, b3, b4, b5, b6);
	}
	SetSessionMode(id, UDS_DEFAULT_SESSION);
	return result;
}
