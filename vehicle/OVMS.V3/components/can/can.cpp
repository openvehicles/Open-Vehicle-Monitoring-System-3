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

#include "can.h"
#include <algorithm>
#include <ctype.h>

can MyCan;

static void CAN_rxtask(void *pvParameters)
  {
  can *me = (can*)pvParameters;
  CAN_frame_t frame;

  while(1)
    {
    if (xQueueReceive(me->m_rxqueue,&frame, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      me->IncomingFrame(&frame);
      }
    }
  }

can::can()
  {
  m_rxqueue = xQueueCreate(20,sizeof(CAN_frame_t));
  xTaskCreatePinnedToCore(CAN_rxtask, "CanRxTask", 4096, (void*)this, 5, &m_rxtask, 1);
  }

can::~can()
  {
  }

void can::IncomingFrame(CAN_frame_t* p_frame)
  {
//  printf("CAN rx origin %s id %03x len %d %02x %02x %02x %02x %02x %02x %02x %02x\n",
//  p_frame->origin->GetName().c_str(),
//  p_frame->MsgID, p_frame->FIR.B.DLC,
//  p_frame->data.u8[0],p_frame->data.u8[1],p_frame->data.u8[2],p_frame->data.u8[3],
//  p_frame->data.u8[4],p_frame->data.u8[5],p_frame->data.u8[6],p_frame->data.u8[7]);
  printf("CAN rx origin %s id %03x (len:%d)",
    p_frame->origin->GetName().c_str(),p_frame->MsgID,p_frame->FIR.B.DLC);
  for (int k=0;k<8;k++)
    {
    if (k<p_frame->FIR.B.DLC)
      printf(" %02x",p_frame->data.u8[k]);
    else
      printf("   ");
    }
  for (int k=0;k<p_frame->FIR.B.DLC;k++)
    {
    if (isprint(p_frame->data.u8[k]))
      printf("%c",p_frame->data.u8[k]);
    else
      printf(".");
    }
  puts("");
  for (auto n : m_listeners)
    {
    xQueueSend(n,p_frame,0);
    }
  }

void can::RegisterListener(QueueHandle_t *queue)
  {
  m_listeners.push_back(queue);
  }

void can::DeregisterListener(QueueHandle_t *queue)
  {
  auto it = std::find(m_listeners.begin(), m_listeners.end(), queue);
  if (it != m_listeners.end())
    {
    m_listeners.erase(it);
    }
  }

canbus::canbus(std::string name)
  : pcp(name)
  {
  }

canbus::~canbus()
  {
  }

esp_err_t canbus::Init(CAN_speed_t speed)
  {
  return ESP_FAIL; // Not implemented by base implementation
  }

esp_err_t canbus::Stop()
  {
  return ESP_FAIL; // Not implemented by base implementation
  }

esp_err_t canbus::Write(const CAN_frame_t* p_frame)
  {
  return ESP_FAIL; // Not implemented by base implementation
  }
