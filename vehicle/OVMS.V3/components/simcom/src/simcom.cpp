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

#include "ovms_log.h"
static const char *TAG = "simcom";

#include <string.h>
#include "simcom.h"
#include "ovms_peripherals.h"
#include "metrics_standard.h"
#include "ovms_config.h"
#include "ovms_events.h"

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
  : pcp(name), m_buffer(SIMCOM_BUF_SIZE), m_mux(this), m_ppp(&m_mux,GSM_MUX_CHAN_DATA), m_nmea(&m_mux, GSM_MUX_CHAN_NMEA)
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
  m_state1_userdata = 0;
  m_netreg = NotRegistered;
  m_powermode = Off;
  StartTask();

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"ticker.1", std::bind(&simcom::Ticker, this, _1, _2));
  }

simcom::~simcom()
  {
  StopTask();
  }

const char* simcom::State1Name(SimcomState1 state)
  {
  switch (state)
    {
    case None:           return "None";
    case CheckPowerOff:  return "CheckPowerOff";
    case PoweringOn:     return "PoweringOn";
    case PoweredOn:      return "PoweredOff";
    case MuxMode:        return "MuxMode";
    case NetStart:       return "NetStart";
    case NetHold:        return "NetHold";
    case NetSleep:       return "NetSleep";
    case NetMode:        return "NetMode";
    case NetDeepSleep:   return "NetDeepSleep";
    case PoweringOff:    return "PoweringOff";
    case PoweredOff:     return "PoweredOff";
    default:             return "Undefined";
    };
  }

const char* simcom::NetRegName(network_registration_t netreg)
  {
  switch (netreg)
    {
    case NotRegistered:      return "NotRegistered";
    case Searching:          return "Searching";
    case DeniedRegistration: return "DeniedRegistration";
    case RegisteredHome:     return "RegisteredHome";
    case RegisteredRoaming:  return "RegisteredRoaming";
    default:                 return "Undefined";
    };
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
  PowerMode original = m_powermode;
  pcp::SetPowerMode(powermode);
  switch (powermode)
    {
    case On:
    case Sleep:
    case DeepSleep:
      if ((original!=On)&&(original!=Sleep)&&(original!=DeepSleep))
        SetState1(PoweringOn);
      break;
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

void simcom::IncomingMuxData(GsmMuxChannel* channel)
  {
  // The MUX has indicated there is data on the specified channel
  // ESP_LOGI(TAG, "IncomingMuxData(CHAN=%d, buffer used=%d)",channel->m_channel,channel->m_buffer.UsedSpace());
  switch (channel->m_channel)
    {
    case GSM_MUX_CHAN_CTRL:
      channel->m_buffer.EmptyAll();
      break;
    case GSM_MUX_CHAN_NMEA:
      if (channel->m_buffer.HasLine() >= 0)
        m_nmea.IncomingLine(channel->m_buffer.ReadLine());
      break;
    case GSM_MUX_CHAN_DATA:
      if (m_state1 == NetMode)
        {
        uint8_t buf[32];
        size_t n;
        while ((n = channel->m_buffer.Pop(sizeof(buf),buf)) > 0)
          {
          m_ppp.IncomingData(buf,n);
          }
        }
      else
        StandardIncomingHandler(&channel->m_buffer);
      break;
    case GSM_MUX_CHAN_POLL:
      StandardIncomingHandler(&channel->m_buffer);
      break;
    case GSM_MUX_CHAN_CMD:
      StandardIncomingHandler(&channel->m_buffer);
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
    case NetStart:
      break;
    case NetHold:
      break;
    case NetSleep:
      break;
    case NetMode:
      break;
    case NetDeepSleep:
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
  m_state1_userdata = 0;

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
      m_state1_timeout_ticks = 30;
      m_state1_timeout_goto = PoweringOn;
      break;
    case MuxMode:
      ESP_LOGI(TAG,"State: Enter MuxMode state");
      m_mux.Start();
      break;
    case NetStart:
      ESP_LOGI(TAG,"State: Enter NetStart state");
      m_nmea.Startup();
      break;
    case NetHold:
      ESP_LOGI(TAG,"State: Enter NetHold state");
      break;
    case NetSleep:
      ESP_LOGI(TAG,"State: Enter NetSleep state");
      m_ppp.Shutdown();
      m_nmea.Shutdown();
      break;
    case NetMode:
      ESP_LOGI(TAG,"State: Enter NetMode state");
      m_ppp.Initialise();
      m_ppp.Connect();
      break;
    case NetDeepSleep:
      ESP_LOGI(TAG,"State: Enter NetDeepSleep state");
      m_ppp.Shutdown();
      m_nmea.Shutdown();
      break;
    case PoweringOff:
      ESP_LOGI(TAG,"State: Enter PoweringOff state");
      m_ppp.Shutdown();
      m_nmea.Shutdown();
      MyEvents.SignalEvent("system.modem.stop",NULL);
      PowerCycle();
      m_state1_timeout_ticks = 10;
      m_state1_timeout_goto = CheckPowerOff;
      break;
    case PoweredOff:
      ESP_LOGI(TAG,"State: Enter PoweredOff state");
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
      if (StandardIncomingHandler(&m_buffer))
        {
        if (m_state1_ticker >= 20) return MuxMode;
        }
      break;
    case MuxMode:
      m_mux.Process(&m_buffer);
      break;
    case NetStart:
      m_mux.Process(&m_buffer);
      break;
    case NetHold:
      m_mux.Process(&m_buffer);
      break;
    case NetSleep:
      m_mux.Process(&m_buffer);
      break;
    case NetMode:
      m_mux.Process(&m_buffer);
      break;
    case NetDeepSleep:
      m_buffer.EmptyAll(); // Drain it
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
      if (m_powermode == DeepSleep)
        {
        return NetDeepSleep; // Just hold, without starting the network
        }
      switch (m_state1_ticker)
        {
        case 10:
          tx("AT+CPIN?;+CREG=1;+CTZU=1;+CTZR=1;+CLIP=1;+CMGF=1;+CNMI=1,2,0,0,0;+CSDH=1;+CMEE=2;+CSQ;+AUTOCSQ=1,1;E0\r\n");
          break;
        case 12:
          tx("AT+CGMR;+ICCID\r\n");
          break;
        case 15:
          tx("AT+COPS?");
          break;
        case 16:
          tx("AT+CMUXSRVPORT=3,1\r\n");
          break;
        case 17:
          tx("AT+CMUXSRVPORT=2,1\r\n");
          break;
        case 18:
          tx("AT+CMUXSRVPORT=1,1\r\n");
          break;
        case 19:
          tx("AT+CMUXSRVPORT=0,5\r\n");
          break;
        case 20:
          tx("AT+CMUX=0\r\n");
          break;
        default:
          break;
        }
      break;
    case MuxMode:
      if ((m_state1_ticker>5)&&((m_state1_ticker % 30) == 0))
        m_mux.tx(GSM_MUX_CHAN_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n");
      if (m_mux.m_openchannels == GSM_MUX_CHANNELS)
        return NetStart;
      break;
    case NetStart:
      if (m_powermode == Sleep)
        {
        return NetSleep; // Just hold, without starting the network
        }
      if (m_state1_ticker == 1)
        {
        // Check for exit out of this state...
        if (MyConfig.GetParamValueBool("modem", "enable.net", true))
          {
          std::string p = MyConfig.GetParamValue("modem", "apn");
          if (!p.empty())
            {
            // Ready to start a PPP
            return None; // Stay in this state...
            }
          }
        return NetHold; // Just hold, without starting the network
        }
      else if ((m_state1_ticker > 5)&&((m_netreg==RegisteredHome)||(m_netreg==RegisteredRoaming)))
        {
        // OK. We have a network registration, and are ready to start
        if (m_state1_userdata == 0)
          {
          m_state1_userdata = 1;
          m_state1_ticker = 5;
          std::string apncmd("AT+CGDCONT=1,\"IP\",\"");
          apncmd.append(MyConfig.GetParamValue("modem", "apn"));
          apncmd.append("\";+CGDATA=\"PPP\",1\r\n");
          m_mux.tx(GSM_MUX_CHAN_DATA,apncmd.c_str());
          }
        else if ((m_state1_userdata == 1)&&(m_state1_ticker > 30))
          {
          ESP_LOGI(TAG,"No valid response to AT+CGDCONT, try again...");
          m_state1_userdata = 0;
          m_state1_ticker = 0;
          }
        else if (m_state1_userdata == 2)
          return NetMode;
        else if (m_state1_userdata == 99)
          {
          m_state1_ticker = 0;
          m_state1_userdata = 0;
          }
        }
      if ((m_state1_ticker>10)&&((m_state1_ticker % 30) == 0))
        m_mux.tx(GSM_MUX_CHAN_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n");
      break;
    case NetHold:
      if ((m_state1_ticker>5)&&((m_state1_ticker % 30) == 0))
        m_mux.tx(GSM_MUX_CHAN_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n");
      break;
    case NetSleep:
      if (m_powermode == On) return NetStart;
      if (m_powermode != Sleep) return PoweringOn;
      if ((m_state1_ticker>5)&&((m_state1_ticker % 30) == 0))
        m_mux.tx(GSM_MUX_CHAN_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n");
      break;
    case NetMode:
      if (m_powermode == Sleep)
        {
        // Need to shutdown ppp, and get back to NetSleep mode
        return NetSleep;
        }
      if ((m_netreg!=RegisteredHome)&&(m_netreg!=RegisteredRoaming))
        {
        // We've lost the network connection
        ESP_LOGI(TAG, "Lost network connection (NetworkRegistration in NetMode)");
        m_mux.tx(GSM_MUX_CHAN_POLL, "AT+CGATT=0\r\n");
        m_ppp.Shutdown(true);
        return NetStart;
        }
      if (m_state1_userdata == 99)
        {
        // We've lost the network connection
        ESP_LOGI(TAG, "Lost network connection (+PPP disconnect in NetMode)");
        m_mux.tx(GSM_MUX_CHAN_POLL, "AT+CGATT=0\r\n");
        m_ppp.Shutdown(true);
        return NetStart;
        }
      if ((m_state1_ticker>5)&&((m_state1_ticker % 30) == 0))
        m_mux.tx(GSM_MUX_CHAN_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n");
      break;
    case NetDeepSleep:
      if (m_powermode != DeepSleep) return PoweringOn;
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

bool simcom::StandardIncomingHandler(OvmsBuffer* buf)
  {
  bool result = false;

  while(1)
    {
    if (buf->m_userdata != 0)
      {
      // Expecting N bytes of data mode
      if (buf->UsedSpace() < (size_t)buf->m_userdata) return false;
      StandardDataHandler(buf);
      result = true;
      }
    else
      {
      // Normal line mode
      if (buf->HasLine() >= 0)
        {
        StandardLineHandler(buf, buf->ReadLine());
        result = true;
        }
      else
        return result;
      }
    }
  }

void simcom::StandardDataHandler(OvmsBuffer* buf)
  {
  // We have SMS data ready...
  size_t needed = (size_t)buf->m_userdata;

  char* result = new char[needed+1];
  buf->Pop(needed, (uint8_t*)result);
  result[needed] = 0;
  MyCommandApp.HexDump("SIMCOM data",result,needed);

  delete [] result;
  buf->m_userdata = 0;
  }

void simcom::StandardLineHandler(OvmsBuffer* buf, std::string line)
  {
  MyCommandApp.HexDump("SIMCOM line",line.c_str(),line.length());

  if ((line.compare(0, 8, "CONNECT ") == 0)&&(m_state1 == NetStart)&&(m_state1_userdata == 1))
    {
    ESP_LOGI(TAG, "PPP Connection is ready to start");
    m_state1_userdata = 2;
    }
  else if ((line.compare(0, 19, "+PPPD: DISCONNECTED") == 0)&&((m_state1 == NetStart)||(m_state1 == NetMode)))
    {
    ESP_LOGI(TAG, "PPP Connection disconnected");
    m_state1_userdata = 99;
    }
  else if (line.compare(0, 8, "+ICCID: ") == 0)
    {
    StandardMetrics.ms_m_net_mdm_iccid->SetValue(line.substr(8));
    }
  else if (line.compare(0, 7, "+CGMR: ") == 0)
    {
    StandardMetrics.ms_m_net_mdm_model->SetValue(line.substr(7));
    }
  else if (line.compare(0, 6, "+CSQ: ") == 0)
    {
    int val = atoi(line.substr(6).c_str());
    StandardMetrics.ms_m_net_sq->SetValue(val,sq);
    }
  else if (line.compare(0, 7, "+CREG: ") == 0)
    {
    size_t qp = line.find(',');
    int creg;
    if (qp != string::npos)
      creg = atoi(line.substr(qp+1,1).c_str());
    else
      creg = atoi(line.substr(7,1).c_str());
    switch (creg)
      {
      case 0:
      case 4:
        m_netreg = NotRegistered;
        break;
      case 1:
        m_netreg = RegisteredHome;
        break;
      case 2:
        m_netreg = Searching;
        break;
      case 3:
        m_netreg = DeniedRegistration;
        break;
      case 5:
        m_netreg = RegisteredRoaming;
        break;
      default:
        break;
      }
    ESP_LOGI(TAG, "CREG Network Registration: %s",NetRegName(m_netreg));
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
  else if (line.compare(0, 6, "+CMT: ") == 0)
    {
    size_t qp = line.find_last_of(',');
    if (qp != string::npos)
      {
      buf->m_userdata = (void*)atoi(line.substr(qp+1).c_str());
      ESP_LOGI(TAG,"SMS length is %d",(int)buf->m_userdata);
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
  std::string msg;
  for (int k=0; k<argc; k++)
    {
    if (k>0)
      {
      msg.append(" ");
      }
    msg.append(argv[k]);
    }
  msg.append("\r\n");
  MyPeripherals->m_simcom->tx(msg.c_str(),msg.length());
  }

void simcom_muxtx(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int channel = atoi(argv[0]);

  std::string msg;
  for (int k=1; k<argc; k++)
    {
    if (k>1)
      {
      msg.append(" ");
      }
    msg.append(argv[k]);
    }
  msg.append("\r\n");
  MyPeripherals->m_simcom->muxtx(channel,msg.c_str(),msg.length());
  }

void simcom_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->printf("SIMCOM\n  Network Registration: %s\n  State: %s\n  Ticker: %d\n  User Data: %d\n",
    MyPeripherals->m_simcom->NetRegName(MyPeripherals->m_simcom->m_netreg),
    MyPeripherals->m_simcom->State1Name(MyPeripherals->m_simcom->m_state1),
    MyPeripherals->m_simcom->m_state1_ticker,
    MyPeripherals->m_simcom->m_state1_userdata);

  if (MyPeripherals->m_simcom->m_state1_timeout_goto != simcom::None)
    {
    writer->printf("  State Timeout Goto: %s (in %d seconds)\n",
      MyPeripherals->m_simcom->State1Name(MyPeripherals->m_simcom->m_state1_timeout_goto),
      MyPeripherals->m_simcom->m_state1_timeout_ticks);
    }

  writer->printf("  Mux Open Channels: %d\n",
    MyPeripherals->m_simcom->m_mux.m_openchannels);

  if (MyPeripherals->m_simcom->m_ppp.m_connected)
    {
    writer->printf("  PPP Connected on channel: #%d\n",
      MyPeripherals->m_simcom->m_ppp.m_channel);
    }
  else
    {
    writer->puts("  PPP Not Connected");
    }

  writer->printf("  PPP Last Error: %s\n",
    MyPeripherals->m_simcom->m_ppp.ErrCodeName(MyPeripherals->m_simcom->m_ppp.m_lasterrcode));
  
  if (MyPeripherals->m_simcom->m_nmea.m_connected)
    {
    writer->printf("  NMEA (GPS/GLONASS) Connected on channel: #%d\n",
      MyPeripherals->m_simcom->m_nmea.m_channel);
    }
  else
    {
    writer->puts("  NMEA (GPS/GLONASS) Not Connected");
    }
  
  writer->printf("  GPS time: %s\n",
    MyPeripherals->m_simcom->m_nmea.m_gpstime_enabled ? "enabled" : "disabled");
  
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
  cmd_simcom->RegisterCommand("status","Show SIMCOM status",simcom_status, "", 0);

  MyConfig.RegisterParam("modem", "Modem Configuration", true, true);
  // Our instances:
  //   'gsmlock': GSM network to lock to (at COPS stage)
  //   'apn': GSM APN
  //   'apn.user': GSM username
  //   'apn.password': GMS password
  //   'enable.sms': Is SMS enabled? yes/no (default: yes)
  //   'enable.net': Is NET enabled? yes/no (default: yes)
  //   'enable.gps': Is GPS enabled? yes/no (default: yes)
  //   'enable.gpstime': use GPS time as system time? yes/no (default: yes)
  }
