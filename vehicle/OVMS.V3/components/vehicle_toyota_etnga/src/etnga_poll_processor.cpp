/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform
   Date:          4th June 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Licensed under the MIT License. See the LICENSE file for details.

  Code adapted from Michael Geddes & Geir Øyvind Vælidalo vehicle modules
*/

#include "ovms_log.h"
static const char *TAG = "v-toyota-etnga";

#include "vehicle_toyota_etnga.h"

void OvmsVehicleToyotaETNGA::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
{
  // Check if this is the first frame of the multi-frame response
  if (m_poll_ml_frame == 0) {
    m_rxbuf.clear();
    m_rxbuf.reserve(length + mlremain);
  }

  // Append the data to the receive buffer
  m_rxbuf.append(reinterpret_cast<char*>(data), length);

  // Check if response is complete
  if (mlremain != 0)
    return;

  // Log the received response
  ESP_LOGV(TAG, "IncomingPollReply: PID %02X: len=%d %s", pid, m_rxbuf.size(), hexencode(m_rxbuf).c_str());

  // Process based on m_poll_moduleid_low
  switch (m_poll_moduleid_low) {
    case HYBRID_BATTERY_SYSTEM_RX:
      // TODO: Add handling for Hybrid Battery System
      break;

    case HYBRID_CONTROL_SYSTEM_RX:
      IncomingHybridControlSystem(pid);
      break;

    default:
      ESP_LOGD(TAG, "Unknown module: %03" PRIx32, m_poll_moduleid_low);
      return;
  }
}

void OvmsVehicleToyotaETNGA::IncomingHybridControlSystem(uint16_t pid)
{
  switch (pid) {
    case PID_BATTERY_VOLTAGE_AND_CURRENT: {
      float bat_voltage = GetBatteryVoltage(m_rxbuf);
      float bat_current = GetBatteryCurrent(m_rxbuf);
      float bat_power = CalculateBatteryPower(bat_voltage, bat_current);

      // Set values for battery voltage, current, and power
      SetBatteryVoltage(bat_voltage);
      SetBatteryCurrent(bat_current);
      SetBatteryPower(bat_power);

      break;
    }
    // Add more cases for other PIDs if needed
    
    default:
      // Handle unsupported PID
      ESP_LOGD(TAG, "Unsupported PID: %04X", pid);
      break;
  }
}

int OvmsVehicleToyotaETNGA::RequestVIN()
{
  ESP_LOGD(TAG, "RequestVIN: Sending Request");

/*
  // TODO: This doesn't work yet as we aren't populating the 'awake'
  if (!StdMetrics.ms_v_env_awake->AsBool()) {
    ESP_LOGD(TAG, "RequestVIN: Not Awake Request not sent");
    return -3;
  }
*/

  std::string response;
  int res = PollSingleRequest(
    m_can2,
    VEHICLE_OBD_BROADCAST_MODULE_TX,
    VEHICLE_OBD_BROADCAST_MODULE_RX,
    VEHICLE_POLL_TYPE_OBDIIVEHICLE,
    0x02,
    response,
    1000
  );

  if (res != POLLSINGLE_OK)
  {
    switch (res)
    {
      case POLLSINGLE_TIMEOUT:
        ESP_LOGE(TAG, "RequestVIN: Request Timeout");
        break;

      case POLLSINGLE_TXFAILURE:
        ESP_LOGE(TAG, "RequestVIN: Request TX Failure");
        break;

      default:
        ESP_LOGE(TAG, "RequestVIN: UDC Error %d", res);
    }

    return res;
  }
  else
  {
    uint32_t byte;
    if (!get_uint_buff_be<1>(response, 0, byte))
    {
      ESP_LOGE(TAG, "RequestVIN: Bad Buffer");
      return POLLSINGLE_TIMEOUT;
    }
    else if (byte != 1)
    {
      ESP_LOGI(TAG, "RequestVIN: Ignore Response (byte = %u)", byte);
      return POLLSINGLE_TIMEOUT;
    }
    else
    {
      std::string vin;
      if (get_buff_string(response, 1, 17, vin))
      {
          ESP_LOGD(TAG, "RequestVIN: Success: '%s'", vin.c_str());
          StandardMetrics.ms_v_vin->SetValue(std::move(vin));
          return POLLSINGLE_OK;
      }
      else
      {
          ESP_LOGE(TAG, "RequestVIN.String: Bad Buffer");
          return POLLSINGLE_TIMEOUT;
      }
    }
  }
}
