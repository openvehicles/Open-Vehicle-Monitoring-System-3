/*
;    Project:       Open Vehicle Monitor System
;    Module:        Vehicle Toyota bZ4X
;    Date:          27th May 2023
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2023       Jerry Kezar <solterra@kezarnet.com>
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
static const char *TAG = "v-toyotabz4x";

#include <stdio.h>
#include "vehicle_toyota_bz4x.h"

// RX buffer access macros: b=byte#
#define RXB_BYTE(b)           m_rxbuf[b]
#define RXB_UINT16(b)         (((uint16_t)RXB_BYTE(b) << 8) | RXB_BYTE(b+1))
#define RXB_UINT24(b)         (((uint32_t)RXB_BYTE(b) << 16) | ((uint32_t)RXB_BYTE(b+1) << 8) \
                               | RXB_BYTE(b+2))
#define RXB_UINT32(b)         (((uint32_t)RXB_BYTE(b) << 24) | ((uint32_t)RXB_BYTE(b+1) << 16) \
                               | ((uint32_t)RXB_BYTE(b+2) << 8) | RXB_BYTE(b+3))
#define RXB_INT8(b)           ((int8_t)RXB_BYTE(b))
#define RXB_INT16(b)          ((int16_t)RXB_UINT16(b))
#define RXB_INT32(b)          ((int32_t)RXB_UINT32(b))

// Pollstate 0 - car is off
// Pollstate 1 - car is on
// Pollstate 2 - car is charging
// Pollstate 3 - ping : car is off, not charging and something triggers a wake

static const OvmsVehicle::poll_pid_t vehicle_bz4x_polls[] = {
  //                                                   Off  On  Chrg Ping
  { 0x7D2, 0x7DA, VEHICLE_POLL_TYPE_READDATA, 0xF186, { 2, 2,   2, 2}, 0, ISOTP_STD },   // State?

//  { 0x745, 0x74d, VEHICLE_POLL_TYPE_READDATA, 0x1739, { 15, 15,   10, 30}, 0, ISOTP_STD },   // SoC - Meter
//  { 0x7d2, 0x7da, VEHICLE_POLL_TYPE_READDATA, 0x1f9a, { 15, 15,   10, 30}, 0, ISOTP_STD },   // Battery Voltage and Current
//  { 0x7d2, 0x7da, VEHICLE_POLL_TYPE_READDATA, 0x1f0d, { 15, 15,   10, 30}, 0, ISOTP_STD },   // Vehicle Speed
//  { 0x7d2, 0x7da, VEHICLE_POLL_TYPE_READDATA, 0x1f46, { 15, 15,   10, 30}, 0, ISOTP_STD },   // Ambient temp  
//  { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0xa6, { 15, 15,   10, 30}, 0, ISOTP_STD },   // Odometer
  POLL_LIST_END
};

/**
 * OvmsVehicleToyotaBz4x constructor
 */
OvmsVehicleToyotaBz4x::OvmsVehicleToyotaBz4x()
  {
  ESP_LOGI(TAG, "Toyota bZ4X vehicle module");

  // Init CAN:
   RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
 
  // Set polling PID list
  PollSetPidList(m_can2,vehicle_bz4x_polls);

  // Set polling state
  PollSetState(0);

  }

/**
 * OvmsVehicleToyotaBz4x destructor
 */
OvmsVehicleToyotaBz4x::~OvmsVehicleToyotaBz4x()
  {
  ESP_LOGI(TAG, "Shutdown Toyota bZ4X vehicle module");
  }

/**
 * IncomingPollReply: framework callback: process response
 */
void OvmsVehicleToyotaBz4x::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
{
/**
*  IncomingPollReply: poll response handler
*  This is called by PollerReceive() on each valid response frame for the current request.
*  Be aware responses may consist of multiple frames, detectable e.g. by mlremain > 0.
*  A typical pattern is to collect frames in a buffer until mlremain == 0.
*/

  // init / fill rx buffer:
  if (m_poll_ml_frame == 0) {
    m_rxbuf.clear();
    m_rxbuf.reserve(length + mlremain);
  }
  m_rxbuf.append((char*)data, length);
  if (mlremain)
    return;

  // response complete:
  ESP_LOGV(TAG, "IncomingPollReply: PID %02X: len=%d %s", pid, m_rxbuf.size(), hexencode(m_rxbuf).c_str());

  // This will be a nested switch; first for the ECU responding and second for the PID.
  // This is in case two ECUs have different values within the same PID
  switch(pid) {
    case 0xa6: {
      float odo = RXB_UINT32(0) / 10.0f;
      StdMetrics.ms_v_pos_odometer->SetValue(odo);
      break;
    }

    case 0x1739: {
      float bat_soc = RXB_BYTE(0);
      StdMetrics.ms_v_bat_soc->SetValue(bat_soc);
      break;
    }

    case 0x1f0d: {
      float speed = RXB_BYTE(0);
      StdMetrics.ms_v_pos_speed->SetValue(speed);
      break;
    }

    case 0x1f46: {
      float temp = RXB_BYTE(0) - 40.0f;
      StdMetrics.ms_v_env_temp->SetValue(temp);
      break;
    }

    case 0x1f9a: {
      float bat_voltage = RXB_INT16(2) / 64.0f;
      float bat_current = RXB_UINT16(4) / 10.0f;
      float bat_power = bat_voltage * bat_current / 1000;
      StdMetrics.ms_v_bat_voltage->SetValue(bat_voltage);
      StdMetrics.ms_v_bat_current->SetValue(bat_current);
      StdMetrics.ms_v_bat_power->SetValue(bat_power);
      break;
    }

    default: {
      ESP_LOGW(
        TAG,
        "Unknown poll reply:%03" PRIx32 " TYPE:%" PRIx16 " PID:%02" PRIx16 " Length:%" PRIx8 " Data:%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
        m_poll_moduleid_low,
        type,
        pid,
        length,
        data[0], data[1], data[2], data[3]
        );
      break;
    }
  }
}

void OvmsVehicleToyotaBz4x::SendCanMessage(uint16_t id, uint8_t count,
  uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
  uint8_t b5, uint8_t b6)
{
  uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
  m_can2->WriteStandard(id, 8, data);
  ESP_LOGV(TAG, "Send Can Msg: %03x n=%02x svc=%02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
}

void OvmsVehicleToyotaBz4x::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
    uint8_t *d = p_frame->data.u8;

    ESP_LOGV(TAG,"CAN1 message received: %08" PRIx32 ": [%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "]",
             p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );

  }

void OvmsVehicleToyotaBz4x::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
    uint8_t *d = p_frame->data.u8;


    // Process the incoming message
    switch (p_frame->MsgID) {

    case 0x174d: {
      // Message from the Plug-in Control ECU
      break;
    }

    case 0x174f: {
      // Message from the EV Battery ECU
      break;
    }

    case 0x17da: {
      // Message from the EV Battery ECU
      break;
    }

    case 0x45a: {
      // I'm not sure what this message is...
      break;
    }

    case 0x4e0: {
      // I'm not sure what this message is...
      break;
    }

    case 0x17ea: {
      break;
    }

    default:
      // Unknown frame. Log for evaluation
      ESP_LOGV(TAG,"CAN2 message received: %08" PRIx32 ": [%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "]",
        p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );
        break;
    }
  }

void OvmsVehicleToyotaBz4x::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
    uint8_t *d = p_frame->data.u8;

    ESP_LOGV(TAG,"CAN3 message received: %08" PRIx32 ": [%02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "]",
             p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );

  }

class OvmsVehicleToyotaBz4xInit
  {
  public: OvmsVehicleToyotaBz4xInit();
} OvmsVehicleToyotaBz4xInit  __attribute__ ((init_priority (9000)));

OvmsVehicleToyotaBz4xInit::OvmsVehicleToyotaBz4xInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Toyota bZ4X (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleToyotaBz4x>("TOYBZ4X","Toyota bZ4X");
  }

