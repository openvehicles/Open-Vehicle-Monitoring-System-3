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
*/

#include "esp_log.h"
static const char *TAG = "simcom";

#include <string.h>
#include "simcom.h"
#include "ovms_peripherals.h"
#include "metrics_standard.h"

static void SIMCOM_task(void *pvParameters)
  {
  simcom *me = (simcom*)pvParameters;
  me->Task();
  }

void simcom::Task()
  {
  SimcomOrUartEvent event;

  for(;;)
    {
    if (xQueueReceive(m_queue, (void *)&event, (portTickType)portMAX_DELAY))
      {
      if (event.uart.type <= UART_EVENT_MAX)
        {
        // Must be a UART event
        switch(event.uart.type)
          {
          case UART_DATA:
            {
            size_t buffered_size = event.uart.size;
            while (buffered_size > 0)
              {
              uint8_t data[16];
              if (buffered_size>16) buffered_size = 16;
              int len = uart_read_bytes(m_uartnum, (uint8_t*)data, buffered_size, 100 / portTICK_RATE_MS);
              m_buffer.Push(data,len);
              MyCommandApp.HexDump("SIMCOM rx",(const char*)data,len);
              uart_get_buffered_data_len(m_uartnum, &buffered_size);
              SimcomState1 newstate = State1Activity();
              if ((newstate != m_state1)&&(newstate != None)) SetState1(newstate);
              }
            }
            break;
          case UART_FIFO_OVF:
            uart_flush(m_uartnum);
            break;
          case UART_BUFFER_FULL:
            uart_flush(m_uartnum);
            break;
          case UART_BREAK:
          case UART_PARITY_ERR:
          case UART_FRAME_ERR:
          case UART_PATTERN_DET:
          default:
            break;
          }
        }
      else
        {
        // Must be a SIMCOM event
        switch (event.simcom.type)
          {
          case SETSTATE:
            break;
          default:
            break;
          }
        }
      }
    }
  }

simcom::simcom(const char* name, uart_port_t uartnum, int baud, int rxpin, int txpin, int pwregpio, int dtregpio)
  : pcp(name), m_buffer(SIMCOM_BUF_SIZE), m_mux(this)
  {
  m_task = 0;
  m_uartnum = uartnum;
  m_baud = baud;
  m_pwregpio = pwregpio;
  m_dtregpio = dtregpio;
  m_rxpin = rxpin;
  m_txpin = txpin;
  m_state1 = None;
  m_state1_ticker = 0;
  m_state1_timeout_goto = None;
  m_state1_timeout_ticks = -1;
  StartTask();

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"ticker.1", std::bind(&simcom::Ticker, this, _1, _2));
  }

simcom::~simcom()
  {
  StopTask();
  }

void simcom::StartTask()
  {
  if (!m_task)
    {
    uart_config_t uart_config =
      {
      .baud_rate = m_baud,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 122,
      };
    // Set UART parameters
    uart_param_config(m_uartnum, &uart_config);

    // Install UART driver, and get the queue.
    uart_driver_install(m_uartnum, SIMCOM_BUF_SIZE * 2, SIMCOM_BUF_SIZE * 2, 10, &m_queue, 0);

    // Set UART pins
    uart_set_pin(m_uartnum, m_txpin, m_rxpin, 0, 0);

    xTaskCreatePinnedToCore(SIMCOM_task, "SIMCOMTask", 4096, (void*)this, 5, &m_task, 1);
    }
  }

void simcom::StopTask()
  {
  if (m_task)
    {
    uart_driver_delete(m_uartnum);
    vTaskDelete(m_task);
    m_task = 0;
    }
  }

void simcom::SetPowerMode(PowerMode powermode)
  {
  pcp::SetPowerMode(powermode);
  switch (powermode)
    {
    case On:
      SetState1(PoweringOn);
      break;
    case Sleep:
    case DeepSleep:
    case Off:
      SetState1(PoweringOff);
      break;
    default:
      break;
    }
  }

void simcom::Ticker(std::string event, void* data)
  {
  m_state1_ticker++;
  SimcomState1 newstate = State1Ticker1();
  if ((newstate != m_state1)&&(newstate != None)) SetState1(newstate);

  // Handle auto-state transitions
  if (m_state1_timeout_ticks > 0)
    {
    m_state1_timeout_ticks--;
    if (m_state1_timeout_ticks <= 0)
      {
      ESP_LOGI(TAG, "State timeout, transition to %d",m_state1_timeout_goto);
      SimcomState1 newstate = m_state1_timeout_goto;
      SetState1(newstate);
      }
    }
  }

void simcom::IncomingMuxData(int channel, OvmsBuffer* buf)
  {
  // The MUX has indicated there is data on the specified channel
  switch (channel)
    {
    case 0:
      buf->EmptyAll();
      break;
    case 1:
      buf->EmptyAll();
      break;
    case 2:
      while (buf->HasLine() >= 0)
        {
        StandardLineHandler(buf->ReadLine());
        }
      break;
    case 3:
      while (buf->HasLine() >= 0)
        {
        StandardLineHandler(buf->ReadLine());
        }
      break;
    default:
      break;
    }
  }

void simcom::SetState1(SimcomState1 newstate)
  {
  m_state1_timeout_ticks = -1;
  m_state1_timeout_goto = None;
  if (m_state1 != None) State1Leave(m_state1);
  State1Enter(newstate);
  }

void simcom::State1Leave(SimcomState1 oldstate)
  {
  ESP_LOGI(TAG,"State: Leave %d",oldstate);

  switch (oldstate)
    {
    case CheckPowerOff:
      break;
    case PoweringOn:
      break;
    case PoweredOn:
      break;
    case MuxMode:
      break;
    case PoweringOff:
      break;
    case PoweredOff:
      break;
    default:
      break;
    }
  }

void simcom::State1Enter(SimcomState1 newstate)
  {
  m_state1 = newstate;
  m_state1_ticker = 0;

  switch (m_state1)
    {
    case CheckPowerOff:
      ESP_LOGI(TAG,"State: Enter CheckPowerOff state");
      // We need to power off the modem (or leave it powered off if it already is)
      // Need to init the modem control lines...
      PowerSleep(false);
      MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 0); // Modem EN/PWR line low
      m_state1_timeout_ticks = 15;
      m_state1_timeout_goto = PoweredOff;
      break;
    case PoweringOn:
      ESP_LOGI(TAG,"State: Enter PoweringOn state");
      PowerCycle();
      m_state1_timeout_ticks = 10;
      m_state1_timeout_goto = PoweringOn;
      break;
    case PoweredOn:
      ESP_LOGI(TAG,"State: Enter PoweredOn state");
      pcp::SetPowerMode(On);
      break;
    case MuxMode:
      m_mux.Start();
      break;
    case PoweringOff:
      ESP_LOGI(TAG,"State: Enter PoweringOff state");
      PowerCycle();
      m_state1_timeout_ticks = 10;
      m_state1_timeout_goto = CheckPowerOff;
      break;
    case PoweredOff:
      ESP_LOGI(TAG,"State: Enter PoweredOff state");
      pcp::SetPowerMode(Off);
      break;
    default:
      break;
    }
  }

simcom::SimcomState1 simcom::State1Activity()
  {
  switch (m_state1)
    {
    case None:
      break;
    case CheckPowerOff:
      // We have activity, so modem is powered on
      m_buffer.EmptyAll(); // Drain it
      return PoweringOff;
      break;
    case PoweringOn:
      // We have activity, so modem is powered on
      m_buffer.EmptyAll(); // Drain it
      return PoweredOn;
      break;
    case PoweredOn:
      while (m_buffer.HasLine() >= 0)
        {
        StandardLineHandler(m_buffer.ReadLine());
        if (m_state1_ticker >= 15) return MuxMode;
        }
      break;
    case MuxMode:
      m_mux.Process(&m_buffer);
      break;
    case PoweringOff:
      m_buffer.EmptyAll(); // Drain it
      break;
    case PoweredOff:
      // We shouldn't have any activity while powered off, so try again...
      return PoweringOff;
      break;
    default:
      break;
    }

  return None;
  }

simcom::SimcomState1 simcom::State1Ticker1()
  {
  switch (m_state1)
    {
    case None:
      return CheckPowerOff;
      break;
    case CheckPowerOff:
      if (m_state1_ticker > 10) tx("AT\r\n");
      break;
    case PoweringOn:
      tx("AT\r\n");
      break;
    case PoweredOn:
      switch (m_state1_ticker)
        {
        case 10:
          tx("AT+CPIN?;+CREG=1;+CLIP=1;+CMGF=1;+CNMI=1,2,0,0,0;+CSDH=1;+CMEE=2;+CSQ;+AUTOCSQ=1,1;E0\r\n");
          break;
        case 12:
          tx("AT+CGMR;+ICCID\r\n");
          break;
        case 15:
          tx("AT+CMUXSRVPORT=0,0;+CMUXSRVPORT=1,5;+CMUXSRVPORT=2,1;+CMUXSRVPORT=3,1;+CMUX=0\r\n");
        default:
          break;
        }
      break;
    case MuxMode:
      break;
    case PoweringOff:
      break;
    case PoweredOff:
      break;
    default:
      break;
    }

  return None;
  }

void simcom::StandardLineHandler(std::string line)
  {
  MyCommandApp.HexDump("SIMCOM line",line.c_str(),line.length());

  if (line.compare(0, 8, "+ICCID: ") == 0)
    {
    StandardMetrics.ms_m_net_mdm_iccid->SetValue(line.substr(8));
    }
  else if (line.compare(0, 7, "+CGMR: ") == 0)
    {
    StandardMetrics.ms_m_net_mdm_model->SetValue(line.substr(7));
    }
  else if (line.compare(0, 6, "+CSQ: ") == 0)
    {
    int csq = atoi(line.substr(6).c_str());
    int dbm = 0;
    if (csq <= 31) dbm = -113 + (csq*2);
    StandardMetrics.ms_m_net_sq->SetValue(dbm);
    }
  else if (line.compare(0, 7, "+COPS: ") == 0)
    {
    size_t qp = line.find('"');
    if (qp != string::npos)
      {
      size_t qp2 = line.find('"',qp+1);
      if (qp2 != string::npos)
        {
        StandardMetrics.ms_m_net_provider->SetValue(line.substr(qp+1,qp2-qp-1));
        }
      }
    }
  }

void simcom::PowerCycle()
  {
  ESP_LOGI(TAG, "Power Cycle");
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 0); // Modem EN/PWR line low
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 1); // Modem EN/PWR line high
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 0); // Modem EN/PWR line low
  }

void simcom::PowerSleep(bool onoff)
  {
  if (onoff)
    MyPeripherals->m_max7317->Output(MODEM_EGPIO_DTR, 1);
  else
    MyPeripherals->m_max7317->Output(MODEM_EGPIO_DTR, 0);
  }

void simcom::tx(uint8_t* data, size_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)
  MyCommandApp.HexDump("SIMCOM tx",(const char*)data,size);
  uart_write_bytes(m_uartnum, (const char*)data, size);
  }

void simcom::tx(const char* data, ssize_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)
  if (size == -1) size = strlen(data);
  MyCommandApp.HexDump("SIMCOM tx",data,size);
  uart_write_bytes(m_uartnum, data, size);
  }

void simcom::muxtx(int channel, uint8_t* data, size_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)
  MyCommandApp.HexDump("SIMCOM mux tx",(const char*)data,size);
  m_mux.tx(channel, data, size);
  }

void simcom::muxtx(int channel, const char* data, ssize_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)
  if (size == -1) size = strlen(data);
  MyCommandApp.HexDump("SIMCOM mux tx",data,size);
  m_mux.tx(channel, (uint8_t*)data, size);
  }

void simcom_tx(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  for (int k=0; k<argc; k++)
    {
    if (k>0)
      {
      MyPeripherals->m_simcom->tx(" ",1);
      }
    MyPeripherals->m_simcom->tx(argv[k],strlen(argv[k]));
    }
  MyPeripherals->m_simcom->tx("\r\n",2);
  }

void simcom_muxtx(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int channel = atoi(argv[0]);

  for (int k=1; k<argc; k++)
    {
    if (k>1)
      {
      MyPeripherals->m_simcom->muxtx(channel, " ",1);
      }
    MyPeripherals->m_simcom->muxtx(channel,argv[k],strlen(argv[k]));
    }
  MyPeripherals->m_simcom->muxtx(channel,"\r\n",2);
  }

class SimcomInit
  {
  public: SimcomInit();
} SimcomInit  __attribute__ ((init_priority (4600)));

SimcomInit::SimcomInit()
  {
  ESP_LOGI(TAG, "Initialising SIMCOM (4600)");

  OvmsCommand* cmd_simcom = MyCommandApp.RegisterCommand("simcom","SIMCOM framework",NULL, "", 1);
  cmd_simcom->RegisterCommand("tx","Transmit data on SIMCOM",simcom_tx, "", 1);
  cmd_simcom->RegisterCommand("muxtx","Transmit data on SIMCOM MUX",simcom_muxtx, "<chan> <data>", 2);
  }
