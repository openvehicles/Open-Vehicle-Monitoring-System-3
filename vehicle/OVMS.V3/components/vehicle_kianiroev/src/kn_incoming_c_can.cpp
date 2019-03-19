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

//static const char *TAG = "v-kianiroev";

/**
 * Handles incoming CAN-frames on bus 1, the C-bus
 */
void OvmsVehicleKiaNiroEv::IncomingFrameCan1(CAN_frame_t* p_frame)
	{

	uint8_t *d = p_frame->data.u8;
	//ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x",
	//		p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

	// Check if response is from synchronous can message
	if (kia_send_can.status == 0xff && p_frame->MsgID == (kia_send_can.id + 0x08))
		{
		//Store message bytes so that the async method can continue
		kia_send_can.status = 3;

		kia_send_can.byte[0] = d[0];
		kia_send_can.byte[1] = d[1];
		kia_send_can.byte[2] = d[2];
		kia_send_can.byte[3] = d[3];
		kia_send_can.byte[4] = d[4];
		kia_send_can.byte[5] = d[5];
		kia_send_can.byte[6] = d[6];
		kia_send_can.byte[7] = d[7];
		}
	}

