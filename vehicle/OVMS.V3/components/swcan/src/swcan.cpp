/*
;    Project:       Open Vehicle Monitor System
;    Date:          27th March 2019
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
static const char *TAG = "swcan";

#include "swcan.h"
#include "ovms_peripherals.h"
#include "ovms_config.h"

#define MCP2515_MODE_SWITCH // is the mode0/mode1 switching done indirectly by MCP2515 RXnBF pins or directly by MAX7317 ?

#define LED_BLINK_TIME 40 // ms
#define ERROR_LED_BLINK_COUNT 25 // 10s


swcan::swcan(const char* name, spi* spibus, spi_nodma_host_device_t host, int clockspeed, int cspin, int intpin, bool hw_cs /*=true*/)
  : mcp2515(name,spibus,host,clockspeed,cspin,intpin,hw_cs)
  {
  m_status_led = new swcan_led("status led", MAX7317_SWCAN_STATUS_LED, 1); // default on
  m_tx_led = new swcan_led("tx led", MAX7317_SWCAN_TX_LED);
  m_rx_led = new swcan_led("rx led", MAX7317_SWCAN_RX_LED);


  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"system.start", std::bind(&swcan::systemUp, this, _1, _2));  
  }

swcan::~swcan()
  {
  }

void swcan::systemUp(std::string event, void* data)
  {
  // flash all LEDS three times and then leave status led on
  m_status_led->Blink(200,3);
  vTaskDelay(50 / portTICK_PERIOD_MS);  
  m_tx_led->Blink(200,3);
  vTaskDelay(50 / portTICK_PERIOD_MS);  
  m_rx_led->Blink(200,3);
  }

bool swcan::RxCallback(CAN_frame_t* frame)
  {
  bool res = mcp2515::RxCallback(frame);
  if (res)
    {
    // frame was received -> blink led
    m_rx_led->Blink(LED_BLINK_TIME);
    }
    return res;
  }


void swcan::TxCallback(CAN_frame_t* frame, bool success)
  {
  canbus::TxCallback(frame,success);

  m_tx_led->Blink(LED_BLINK_TIME);
  if (!success)
    {
    m_status_led->Blink(LED_BLINK_TIME, ERROR_LED_BLINK_COUNT);
    }
  }


esp_err_t swcan::Start(CAN_mode_t mode, CAN_speed_t speed)
  {
  esp_err_t ret = mcp2515::Start(mode,speed);
  SetTransceiverMode(tmode_normal);
  return ret;
  }

esp_err_t swcan::Stop()
  {
  esp_err_t ret = mcp2515::Stop();
  SetTransceiverMode(tmode_sleep);
  return ret;
  }

void swcan::DoSetTransceiverMode(bool mode0, bool mode1)
  {
#ifdef MCP2515_MODE_SWITCH
    ESP_LOGI(TAG, "Setting SWCAN mode0(%d) mode1(%d) via MCP2515", mode0, mode1);
    WriteReg(REG_BFPCTRL, 0b00001100 | (mode0 << 5) | (mode1 << 4));    
#else
#ifdef CONFIG_OVMS_COMP_MAX7317
    ESP_LOGI(TAG, "Setting SWCAN mode0(%d) mode1(%d) via MAX7317", mode0, mode1);
      MyPeripherals->m_max7317->Output(MAX7317_SWCAN_MODE0, mode0);
      MyPeripherals->m_max7317->Output(MAX7317_SWCAN_MODE1, mode1);
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317
#endif // #ifdef MCP2515_MODE_SWITCH
  }

void swcan::SetTransceiverMode(TransceiverMode tmode)
  {
  switch (tmode)
    {
    case tmode_normal:  // normal mode
      ESP_LOGI(TAG, "Setting SWCAN transceiver into normal mode");
      DoSetTransceiverMode(1,1);
      if (m_tmode==tmode_sleep)
        vTaskDelay(30 / portTICK_PERIOD_MS);  // extra 20ms transition time from sleep to normal mode (in addition to 30ms below)
      break;
    case tmode_sleep:
      ESP_LOGI(TAG, "Setting SWCAN transceiver into sleep mode");
      DoSetTransceiverMode(0,0);
      break;
    case tmode_highvoltagewakeup:
      ESP_LOGI(TAG, "Setting SWCAN transceiver into high voltage wakeup mode");
      DoSetTransceiverMode(0,1);
      break;
    case tmode_highspeed:
      ESP_LOGI(TAG, "Setting SWCAN transceiver into high speed mode");
      DoSetTransceiverMode(1,0);
      break;
    default:
      break;
    }
  m_tmode = tmode;
  vTaskDelay(30 / portTICK_PERIOD_MS);  // atleast 30ms transition time between modes
  }


void swcan::SetPowerMode(PowerMode powermode)
  {
  ESP_LOGI(TAG, "Setting SWCAN powermode to %s", powermode==On ? "on" : "off");
  mcp2515::SetPowerMode(powermode);
  switch (powermode)
    {
    case On:  // default to normal mode
      SetTransceiverMode(tmode_normal);
      break;
    case Off:
    case Sleep:
    case DeepSleep:
      SetTransceiverMode(tmode_sleep);
      break;
    default:
      break;
    }
  }


/*
void swcan::AutoInit()
  {
  if (MyConfig.GetParamValueBool("auto", "swcan", false))
    SetPowerMode(On);
  }
*/
