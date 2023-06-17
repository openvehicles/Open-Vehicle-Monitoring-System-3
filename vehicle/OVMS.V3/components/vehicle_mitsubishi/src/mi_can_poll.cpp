/*
;    Project:       Open Vehicle Monitor System
;    Date:          02th May 2020
;
;    (C) 2023       Tamás Kovács (KommyKT)
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
#include "vehicle_mitsubishi.h"

static const char *TAGPOLL = "v-trio-poll";
#define RXB_BYTE(b)           m_rxbuf[b]
#define RXB_UINT16(b)         (((uint16_t)RXB_BYTE(b) << 8) | RXB_BYTE(b+1))
#define RXB_UINT24b(b)         (((uint32_t)RXB_BYTE(b+2) << 16) | ((uint32_t)RXB_BYTE(b+1) << 8) \
                               | RXB_BYTE(b))  
/**
 * Incoming poll reply messages
 */
void OvmsVehicleMitsubishi::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
{
  //ESP_LOGW(TAGPOLL, "%03" PRIx32 " TYPE:%x PID:%02x Data:%02x %02x %02x %02x %02x %02x %02x %02x LENG:%02x REM:%02x", m_poll_moduleid_low, type, pid, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], length, mlremain);
  // init / fill rx buffer:
    if (m_poll_ml_frame == 0) 
    {
      m_rxbuf.clear();
      m_rxbuf.reserve(length + mlremain);
    }
    
    m_rxbuf.append((char*)data, length);

    if (mlremain)
      return;

    ESP_LOGV(TAGPOLL, "IncomingPollReply: PID %02X: len=%d %s", pid, m_rxbuf.size(), hexencode(m_rxbuf).c_str());
  	//OvmsVehicleMitsubishi* trio = (OvmsVehicleMitsubishi*) MyVehicleFactory.ActiveVehicle();
    switch (m_poll_moduleid_low)
		{
  		// ****** BMU *****
  		case 0x762:
      {
        if(pid == 0x01)
        {
          OvmsMetricFloat* xmi_bat_soc_real = MyMetrics.InitFloat("xmi.b.soc.real", 10, 0, Percentage);
          xmi_bat_soc_real->SetValue((RXB_BYTE(0) * 0.5) - 5);

          // displayed SOC
          OvmsMetricFloat* xmi_bat_soc_display = MyMetrics.InitFloat("xmi.b.soc.display", 10, 0, Percentage);
          xmi_bat_soc_display->SetValue((RXB_BYTE(1) * 0.5) - 5);
              
          // battery "max" capacity
          StandardMetrics.ms_v_bat_cac->SetValue((RXB_UINT16(27) * 0.1f));

          // battery remain capacity
          ms_v_bat_cac_rem->SetValue((RXB_UINT16(29) * 0.1f));

          //max charging kW
          ms_v_bat_max_input->SetValue(RXB_BYTE(31)* 0.25);
                
          //max output kW
          ms_v_bat_max_output->SetValue(RXB_BYTE(32) * 0.25);
        }

        /*
        Battery cell voltage
        Miev: 88cell voltage, 24 bit blocks 16 bit voltage date, 
        */ 
        
        if(pid == 0x02 && !has_broadcast)
        {
          BmsRestartCellVoltages();
          
          for (int i = 0; i < 16; i+=2) {
            BmsSetCellVoltage(i/2, (RXB_UINT16(0+i) * 0.001f));
          }

          int j = 0;
          for (int i = 24; i < 40; i+=2) {
              BmsSetCellVoltage(j+8, (RXB_UINT16(0+i) * 0.001f));
            j++;
          }

          j = 0;
          for (int i = 48; i < 64; i+=2) {
              BmsSetCellVoltage(j+16, (RXB_UINT16(0+i) * 0.001f));
            j++;
          }

          j = 0;
          for (int i = 72; i < 88; i+=2) {
              BmsSetCellVoltage(j+24, (RXB_UINT16(0+i) * 0.001f));
            j++;
          }

          j = 0;
          for (int i = 96; i < 112; i+=2) {
              BmsSetCellVoltage(j+32, (RXB_UINT16(0+i) * 0.001f));
            j++;
          }

          j = 0;
          for (int i = 120; i < 128; i+=2) {
              BmsSetCellVoltage(j+40, (RXB_UINT16(0+i) * 0.001f));
            j++;
          }

          j = 0;
          for (int i = 144; i < 160; i+=2) {
              BmsSetCellVoltage(j+44, (RXB_UINT16(0+i) * 0.001f));
          j++;
          }

          j = 0;
          for (int i = 168; i < 184; i+=2) {
              BmsSetCellVoltage(j+52, (RXB_UINT16(0+i) * 0.001f));
            j++;
          }

          j = 0;
          for (int i = 192; i < 208; i+=2) {
              BmsSetCellVoltage(j+60, (RXB_UINT16(0+i) * 0.001f));
            j++;
          }

          j = 0;
          for (int i = 216; i < 232; i+=2) {
              BmsSetCellVoltage(j+68, (RXB_UINT16(0+i) * 0.001f));
            j++;
          }

          j = 0;
          for (int i = 240; i < 256; i+=2) {
              BmsSetCellVoltage(j+76, (RXB_UINT16(0+i) * 0.001f));
            j++;
          }
          j = 0;
          for (int i = 264; i < 272; i+=2) {
              BmsSetCellVoltage(j+84, (RXB_UINT16(0+i) * 0.001f));
            j++;
          }
        }
         
        /* 
        Battery cell temperature 
        72 bit 66 cell temp after first 33 bit (cell temp) 3 fe bit 
        */

        if(pid == 0x03 && !has_broadcast)
        {
          BmsRestartCellTemperatures();
          for (int i = 0; i < 33; i++) {
            BmsSetCellTemperature(i, (RXB_BYTE(i) - 50.0));
            ESP_LOGV(TAGPOLL, " %i cell temp: %0f",i, RXB_BYTE(i) - 50.0f);
          }
          for (int i = 36; i < 69; i++) {
            BmsSetCellTemperature(33+i-36, (RXB_BYTE(i) - 50.0));
            ESP_LOGV(TAGPOLL, " %i cell temp: %0f",33+i-36, RXB_BYTE(i) - 50.0f);
          }
        }
        break;
      }

      // ****** OBC *****
      case 0x766:
      {
        ESP_LOGV(TAGPOLL, "%03" PRIx32 " TYPE:%x PID:%02x Data:%02x %02x %02x %02x %02x %02x %02x %02x LENG:%02x REM:%02x", m_poll_moduleid_low, type, pid, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], length, mlremain);
        break;
      }

      // ****** HVAC *****
      case 0x772:
      {
        if(pid == 0x13)
        {
          StandardMetrics.ms_v_env_cabintemp->SetValue((RXB_BYTE(2) * 0.25) - 16.0);
        }

        break;
      }

      // ****** METER  *****
      case 0x783:
      {
        if(pid == 0xCE)
        {
          ms_v_trip_A->SetValue(RXB_UINT24b(0) * 0.1, Kilometers);
          ms_v_trip_B->SetValue(RXB_UINT24b(3) * 0.1, Kilometers); 
        }
        break;
      }

     default:
		   ESP_LOGV(TAGPOLL, "Unknown module: %03" PRIx32, m_poll_moduleid_low);
	  }

}
