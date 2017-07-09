/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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
;
; Portions of this are based on the work of Thomas Barth, and licensed
; under MIT license.
; Copyright (c) 2017, Thomas Barth, barth-dev.de
; https://github.com/ThomasBarth/ESP32-CAN-Driver
*/

#include <string.h>
#include "esp32can.h"
#include "esp32can_regdef.h"

esp32can* MyESP32can = NULL;

static void ESP32CAN_rxframe(esp32can *me)
  {
  CAN_frame_t msg;

  // Record the origin
  memset(&msg,0,sizeof(msg));
  msg.origin = me;

  //get FIR
  msg.FIR.U = MODULE_ESP32CAN->MBX_CTRL.FCTRL.FIR.U;

  //check if this is a standard or extended CAN frame
  if (msg.FIR.B.FF==CAN_frame_std)
    { // Standard frame
    //Get Message ID
    msg.MsgID = ESP32CAN_GET_STD_ID;
    //deep copy data bytes
    for (int k=0 ; k<msg.FIR.B.DLC ; k++)
    	msg.data.u8[k] = MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.STD.data[k];
    }
  else
    { // Extended frame
    //Get Message ID
    msg.MsgID = ESP32CAN_GET_EXT_ID;
    //deep copy data bytes
    for (int k=0 ; k<msg.FIR.B.DLC ; k++)
    	msg.data.u8[k] = MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.EXT.data[k];
    }

  //send frame to main CAN processor task
  xQueueSendFromISR(MyCan.m_rxqueue,&msg,0);

  //Let the hardware know the frame has been read.
  MODULE_ESP32CAN->CMR.B.RRB=1;
  }

static void ESP32CAN_isr(void *pvParameters)
  {
  esp32can *me = (esp32can*)pvParameters;

  // Read interrupt status and clear flags
  ESP32CAN_IRQ_t interrupt = (ESP32CAN_IRQ_t)MODULE_ESP32CAN->IR.U;

  // Handle TX complete interrupt
  if ((interrupt & __CAN_IRQ_TX) != 0)
    {
  	/*handler*/
    }

  // Handle RX frame available interrupt
  if ((interrupt & __CAN_IRQ_RX) != 0)
    ESP32CAN_rxframe(me);

  // Handle error interrupts.
  if ((interrupt & (__CAN_IRQ_ERR						//0x4
                  | __CAN_IRQ_DATA_OVERRUN	//0x8
                  | __CAN_IRQ_WAKEUP				//0x10
                  | __CAN_IRQ_ERR_PASSIVE		//0x20
                  | __CAN_IRQ_ARB_LOST			//0x40
                  | __CAN_IRQ_BUS_ERR				//0x80
                  )) != 0)
    {
    /*handler*/
    }
  }

esp32can::esp32can(std::string name, int txpin, int rxpin)
  : canbus(name)
  {
  m_txpin = (gpio_num_t)txpin;
  m_rxpin = (gpio_num_t)rxpin;
  MyESP32can = this;
  }

esp32can::~esp32can()
  {
  MyESP32can = NULL;
  }

esp_err_t esp32can::Init(CAN_speed_t speed)
  {
  double __tq; // Time quantum

  m_speed = speed;

  // Enable module
  DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_CAN_CLK_EN);
  DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_CAN_RST);

  // Configure TX pin
  gpio_set_direction(MyESP32can->m_txpin,GPIO_MODE_OUTPUT);
  gpio_matrix_out(MyESP32can->m_txpin,CAN_TX_IDX,0,0);
  gpio_pad_select_gpio(MyESP32can->m_txpin);

  // Configure RX pin
  gpio_set_direction(MyESP32can->m_rxpin,GPIO_MODE_INPUT);
  gpio_matrix_in(MyESP32can->m_rxpin,CAN_RX_IDX,0);
  gpio_pad_select_gpio(MyESP32can->m_rxpin);

  // Set to PELICAN mode
  MODULE_ESP32CAN->CDR.B.CAN_M=0x1;

  // Synchronization jump width is the same for all baud rates
  MODULE_ESP32CAN->BTR0.B.SJW=0x1;

  // TSEG2 is the same for all baud rates
  MODULE_ESP32CAN->BTR1.B.TSEG2=0x1;

  // select time quantum and set TSEG1
  switch (MyESP32can->m_speed)
    {
    case CAN_SPEED_1000KBPS:
      MODULE_ESP32CAN->BTR1.B.TSEG1=0x4;
      __tq = 0.125;
      break;
    case CAN_SPEED_800KBPS:
      MODULE_ESP32CAN->BTR1.B.TSEG1=0x6;
      __tq = 0.125;
      break;
    default:
      MODULE_ESP32CAN->BTR1.B.TSEG1=0xc;
      __tq = ((float)1000/MyESP32can->m_speed) / 16;
    }

  // Set baud rate prescaler
  MODULE_ESP32CAN->BTR0.B.BRP=(uint8_t)round((((APB_CLK_FREQ * __tq) / 2) - 1)/1000000)-1;

  /* Set sampling
   * 1 -> triple; the bus is sampled three times; recommended for low/medium speed buses     (class A and B) where filtering spikes on the bus line is beneficial
   * 0 -> single; the bus is sampled once; recommended for high speed buses (SAE class C)*/
  MODULE_ESP32CAN->BTR1.B.SAM=0x1;

  // Enable all interrupts
  MODULE_ESP32CAN->IER.U = 0xff;

  // No acceptance filtering, as we want to fetch all messages
  MODULE_ESP32CAN->MBX_CTRL.ACC.CODE[0] = 0;
  MODULE_ESP32CAN->MBX_CTRL.ACC.CODE[1] = 0;
  MODULE_ESP32CAN->MBX_CTRL.ACC.CODE[2] = 0;
  MODULE_ESP32CAN->MBX_CTRL.ACC.CODE[3] = 0;
  MODULE_ESP32CAN->MBX_CTRL.ACC.MASK[0] = 0xff;
  MODULE_ESP32CAN->MBX_CTRL.ACC.MASK[1] = 0xff;
  MODULE_ESP32CAN->MBX_CTRL.ACC.MASK[2] = 0xff;
  MODULE_ESP32CAN->MBX_CTRL.ACC.MASK[3] = 0xff;

  // Set to normal mode
  MODULE_ESP32CAN->OCR.B.OCMODE=__CAN_OC_NOM;

  // Clear error counters
  MODULE_ESP32CAN->TXERR.U = 0;
  MODULE_ESP32CAN->RXERR.U = 0;
  (void)MODULE_ESP32CAN->ECC;

  // Clear interrupt flags
  (void)MODULE_ESP32CAN->IR.U;

  // Install CAN ISR
  esp_intr_alloc(ETS_CAN_INTR_SOURCE,0,ESP32CAN_isr,this,NULL);

  // Showtime. Release Reset Mode.
  MODULE_ESP32CAN->MOD.B.RM = 0;

  return ESP_OK;
  }

esp_err_t esp32can::Stop()
  {
  // Enter reset mode
  MODULE_ESP32CAN->MOD.B.RM = 1;

  return ESP_OK;
  }

esp_err_t esp32can::Write(const CAN_frame_t* p_frame)
  {
  uint8_t __byte_i; // Byte iterator

  // copy frame information record
  MODULE_ESP32CAN->MBX_CTRL.FCTRL.FIR.U=p_frame->FIR.U;

  if (p_frame->FIR.B.FF==CAN_frame_std)
    { // Standard frame
    // Write message ID
    ESP32CAN_SET_STD_ID(p_frame->MsgID);
    // Copy the frame data to the hardware
    for (__byte_i=0 ; __byte_i<p_frame->FIR.B.DLC ; __byte_i++)
      MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.STD.data[__byte_i]=p_frame->data.u8[__byte_i];
    }
  else
    { // Extended frame
    // Write message ID
    ESP32CAN_SET_EXT_ID(p_frame->MsgID);
    // Copy the frame data to the hardware
    for (__byte_i=0 ; __byte_i<p_frame->FIR.B.DLC ; __byte_i++)
      MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.EXT.data[__byte_i]=p_frame->data.u8[__byte_i];
    }

  // Transmit frame
  MODULE_ESP32CAN->CMR.B.TR=1;

  return ESP_OK;
  }

void esp32can::SetPowerMode(PowerMode powermode)
  {
  switch (powermode)
    {
    case On:
      Init(m_speed);
      break;
    case Sleep:
    case DeepSleep:
    case Off:
      Stop();
      break;
    default:
      break;
    };
  }
