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
#include "ovms_boot.h"

const char* SimcomState1Name(simcom::SimcomState1 state)
  {
  switch (state)
    {
    case simcom::None:           return "None";
    case simcom::CheckPowerOff:  return "CheckPowerOff";
    case simcom::PoweringOn:     return "PoweringOn";
    case simcom::PoweredOn:      return "PoweredOn";
    case simcom::MuxStart:       return "MuxStart";
    case simcom::NetWait:        return "NetWait";
    case simcom::NetStart:       return "NetStart";
    case simcom::NetLoss:        return "NetLoss";
    case simcom::NetHold:        return "NetHold";
    case simcom::NetSleep:       return "NetSleep";
    case simcom::NetMode:        return "NetMode";
    case simcom::NetDeepSleep:   return "NetDeepSleep";
    case simcom::PoweringOff:    return "PoweringOff";
    case simcom::PoweredOff:     return "PoweredOff";
    case simcom::PowerOffOn:     return "PowerOffOn";
    default:                     return "Undefined";
    };
  }

const char* SimcomNetRegName(simcom::network_registration_t netreg)
  {
  switch (netreg)
    {
    case simcom::NotRegistered:      return "NotRegistered";
    case simcom::Searching:          return "Searching";
    case simcom::DeniedRegistration: return "DeniedRegistration";
    case simcom::RegisteredHome:     return "RegisteredHome";
    case simcom::RegisteredRoaming:  return "RegisteredRoaming";
    default:                         return "Undefined";
    };
  }

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
              MyCommandApp.HexDump(TAG, "rx", (const char*)data, len);
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
            SetState1(event.simcom.data.newstate);
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
  m_provider = "";
  m_sq = 99; // = unknown
  m_powermode = Off;
  m_line_unfinished = -1;
  m_line_buffer.clear();
  StartTask();

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"ticker.1", std::bind(&simcom::Ticker, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "system.shuttingdown", std::bind(&simcom::EventListener, this, _1, _2));
  }

simcom::~simcom()
  {
  StopTask();
  }

void simcom::AutoInit()
  {
  if (MyConfig.GetParamValueBool("auto", "modem", false))
    SetPowerMode(On);
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
      .use_ref_tick = 0,
      };
    // Set UART parameters
    uart_param_config(m_uartnum, &uart_config);

    // Install UART driver, and get the queue.
    uart_driver_install(m_uartnum, SIMCOM_BUF_SIZE * 2, SIMCOM_BUF_SIZE * 2, 10, &m_queue, 0);

    // Set UART pins
    uart_set_pin(m_uartnum, m_txpin, m_rxpin, 0, 0);

    xTaskCreatePinnedToCore(SIMCOM_task, "OVMS SIMCOM", 4096, (void*)this, 5, &m_task, 1);
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

void simcom::SendSetState1(SimcomState1 newstate)
  {
  SimcomOrUartEvent ev;
  ev.simcom.type = SETSTATE;
  ev.simcom.data.newstate = newstate;
  xQueueSend(m_queue,&ev,0);
  }

bool simcom::IsStarted()
  {
  return (m_task != NULL);
  }

void simcom::UpdateNetMetrics()
  {
  if (StdMetrics.ms_m_net_type->AsString() == "modem")
    {
    StdMetrics.ms_m_net_provider->SetValue(m_provider);
    StdMetrics.ms_m_net_sq->SetValue(m_sq, sq);
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

void simcom::EventListener(std::string event, void* data)
  {
  if (event == "system.shuttingdown")
    {
    if (m_state1 != PoweredOff)
      {
      MyBoot.RestartPending(TAG);
      SetState1(PoweringOff);
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
      while (channel->m_buffer.HasLine() >= 0)
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
        StandardIncomingHandler(channel->m_channel, &channel->m_buffer);
      break;
    case GSM_MUX_CHAN_POLL:
      StandardIncomingHandler(channel->m_channel, &channel->m_buffer);
      break;
    case GSM_MUX_CHAN_CMD:
      StandardIncomingHandler(channel->m_channel, &channel->m_buffer);
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
    case MuxStart:
      break;
    case NetWait:
      break;
    case NetStart:
      break;
    case NetLoss:
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
#ifdef CONFIG_OVMS_COMP_MAX7317
      MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 0); // Modem EN/PWR line low
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317
      m_state1_timeout_ticks = 15;
      m_state1_timeout_goto = PoweredOff;
      break;
    case PoweringOn:
      ESP_LOGI(TAG,"State: Enter PoweringOn state");
      MyEvents.SignalEvent("system.modem.poweringon", NULL);
      PowerCycle();
      m_state1_timeout_ticks = 20;
      m_state1_timeout_goto = PoweringOn;
      break;
    case PoweredOn:
      ESP_LOGI(TAG,"State: Enter PoweredOn state");
      MyEvents.SignalEvent("system.modem.poweredon", NULL);
      m_state1_timeout_ticks = 30;
      m_state1_timeout_goto = PoweringOn;
      break;
    case MuxStart:
      ESP_LOGI(TAG,"State: Enter MuxStart state");
      MyEvents.SignalEvent("system.modem.muxstart", NULL);
      m_state1_timeout_ticks = 120;
      m_state1_timeout_goto = PoweringOn;
      m_mux.Start();
      break;
    case NetWait:
      ESP_LOGI(TAG,"State: Enter NetWait state");
      MyEvents.SignalEvent("system.modem.netwait", NULL);
      m_nmea.Startup();
      break;
    case NetStart:
      ESP_LOGI(TAG,"State: Enter NetStart state");
      MyEvents.SignalEvent("system.modem.netstart", NULL);
      m_state1_timeout_ticks = 30;
      m_state1_timeout_goto = PowerOffOn;
      break;
    case NetLoss:
      ESP_LOGI(TAG,"State: Enter NetLoss state");
      MyEvents.SignalEvent("system.modem.netloss", NULL);
      m_state1_timeout_ticks = 10;
      m_state1_timeout_goto = NetWait;
      m_mux.tx(GSM_MUX_CHAN_POLL, "AT+CGATT=0\r\n");
      m_ppp.Shutdown(true);
      break;
    case NetHold:
      ESP_LOGI(TAG,"State: Enter NetHold state");
      MyEvents.SignalEvent("system.modem.nethold", NULL);
      break;
    case NetSleep:
      ESP_LOGI(TAG,"State: Enter NetSleep state");
      MyEvents.SignalEvent("system.modem.netsleep", NULL);
      m_ppp.Shutdown();
      m_nmea.Shutdown();
      break;
    case NetMode:
      ESP_LOGI(TAG,"State: Enter NetMode state");
      MyEvents.SignalEvent("system.modem.netmode", NULL);
      m_ppp.Initialise();
      m_ppp.Connect();
      break;
    case NetDeepSleep:
      ESP_LOGI(TAG,"State: Enter NetDeepSleep state");
      MyEvents.SignalEvent("system.modem.netdeepsleep", NULL);
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
      MyEvents.SignalEvent("system.modem.poweredoff", NULL);
      if (MyBoot.IsShuttingDown()) MyBoot.RestartReady(TAG);
      m_mux.Stop();
      break;
    case PowerOffOn:
      ESP_LOGI(TAG,"State: Enter PowerOffOn state");
      m_ppp.Shutdown();
      m_nmea.Shutdown();
      m_mux.Stop();
      MyEvents.SignalEvent("system.modem.stop",NULL);
      PowerCycle();
      m_state1_timeout_ticks = 3;
      m_state1_timeout_goto = PoweringOn;
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
      if (StandardIncomingHandler(GSM_MUX_CHAN_CTRL, &m_buffer))
        {
        if (m_state1_ticker >= 20) return MuxStart;
        }
      break;
    case MuxStart:
    case NetWait:
    case NetStart:
    case NetHold:
    case NetSleep:
    case NetMode:
      m_mux.Process(&m_buffer);
      break;
    case NetLoss:
    case NetDeepSleep:
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
  if (m_mux.IsMuxUp())
    {
    if ((m_mux.m_lastgoodrxframe > 0)&&((monotonictime-m_mux.m_lastgoodrxframe)>180))
      {
      // We haven't had a good MUX frame in 3 minutes. Let's assume the MUX has failed
      ESP_LOGW(TAG, "3 minutes since last MUX rx frame - assume MUX has failed");
      m_ppp.Shutdown();
      m_nmea.Shutdown();
      m_mux.Stop();
      MyEvents.SignalEvent("system.modem.stop",NULL);
      PowerCycle();
      return PoweringOn;
      }
    }

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
    case MuxStart:
      if ((m_state1_ticker>5)&&((m_state1_ticker % 30) == 0))
        m_mux.tx(GSM_MUX_CHAN_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n");
      if (m_mux.IsMuxUp())
        return NetWait;
      break;
    case NetWait:
      if (m_powermode == Sleep)
        {
        return NetSleep; // Just hold, without starting the network
        }
      if (m_state1_ticker == 1)
        {
        // Check for exit out of this state...
        std::string p = MyConfig.GetParamValue("modem", "apn");
        if ((!MyConfig.GetParamValueBool("modem", "enable.net", true))||(p.empty()))
          return NetHold; // Just hold, without starting PPP
        }
      else if ((m_state1_ticker > 3)&&((m_netreg==RegisteredHome)||(m_netreg==RegisteredRoaming)))
        return NetStart; // We have GSM, so start the network
      if ((m_state1_ticker>3)&&((m_state1_ticker % 10) == 0))
        m_mux.tx(GSM_MUX_CHAN_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n");
      break;
    case NetStart:
      if (m_powermode == Sleep)
        {
        return NetSleep; // Just hold, without starting the network
        }
      if (m_state1_ticker == 1)
        {
        m_state1_userdata = 1;
        std::string apncmd("AT+CGDCONT=1,\"IP\",\"");
        apncmd.append(MyConfig.GetParamValue("modem", "apn"));
        apncmd.append("\";+CGDATA=\"PPP\",1\r\n");
        m_mux.tx(GSM_MUX_CHAN_DATA,apncmd.c_str());
        }
      if (m_state1_userdata == 2)
        return NetMode; // PPP Connection is ready to be started
      else if (m_state1_userdata == 99)
        return NetLoss;
      else if (m_state1_userdata == 100)
        {
        ESP_LOGW(TAG, "NetStart: unresolvable error, performing modem power cycle");
        return PowerOffOn;
        }
      break;
    case NetLoss:
      break;
    case NetHold:
      if ((m_state1_ticker>5)&&((m_state1_ticker % 30) == 0))
        m_mux.tx(GSM_MUX_CHAN_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n");
      break;
    case NetSleep:
      if (m_powermode == On) return NetWait;
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
        return NetLoss;
        }
      if (m_state1_userdata == 99)
        {
        // We've lost the network connection
        ESP_LOGI(TAG, "Lost network connection (+PPP disconnect in NetMode)");
        return NetLoss;
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

bool simcom::StandardIncomingHandler(int channel, OvmsBuffer* buf)
  {
  bool result = false;

  while(1)
    {
    if (buf->m_userdata != 0)
      {
      // Expecting N bytes of data mode
      if (buf->UsedSpace() < (size_t)buf->m_userdata) return false;
      StandardDataHandler(channel, buf);
      result = true;
      }
    else
      {
      // Normal line mode
      while (buf->HasLine() >= 0)
        {
        StandardLineHandler(channel, buf, buf->ReadLine());
        result = true;
        }
      return result;
      }
    }
  }

void simcom::StandardDataHandler(int channel, OvmsBuffer* buf)
  {
  // We have SMS data ready...
  size_t needed = (size_t)buf->m_userdata;

  char* result = new char[needed+1];
  buf->Pop(needed, (uint8_t*)result);
  result[needed] = 0;
  MyCommandApp.HexDump(TAG, "data", result, needed);

  delete [] result;
  buf->m_userdata = 0;
  }

void simcom::StandardLineHandler(int channel, OvmsBuffer* buf, std::string line)
  {
  if (line.length() == 0)
    return;

  // expecting continuation of previous line?
  if (m_line_unfinished == channel)
    {
    m_line_buffer += line;
    if (m_line_buffer.length() > 1000)
      {
      ESP_LOGE(TAG, "rx line buffer grown too long, discarding");
      m_line_buffer.clear();
      m_line_unfinished = -1;
      return;
      }
    line = m_line_buffer;
    }

  ESP_LOGD(TAG, "rx line ch=%d len=%-4d: %s", channel, line.length(), line.c_str());

  if ((line.compare(0, 8, "CONNECT ") == 0)&&(m_state1 == NetStart)&&(m_state1_userdata == 1))
    {
    ESP_LOGI(TAG, "PPP Connection is ready to start");
    m_state1_userdata = 2;
    }
  else if ((line.compare(0, 5, "ERROR") == 0)&&(m_state1 == NetStart)&&(m_state1_userdata == 1))
    {
    ESP_LOGI(TAG, "PPP Connection init error");
    m_state1_userdata = 100;
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
    m_sq = atoi(line.substr(6).c_str());
    UpdateNetMetrics();
    }
  else if (line.compare(0, 7, "+CREG: ") == 0)
    {
    size_t qp = line.find(',');
    int creg;
    network_registration_t nreg = Unknown;
    if (qp != string::npos)
      creg = atoi(line.substr(qp+1,1).c_str());
    else
      creg = atoi(line.substr(7,1).c_str());
    switch (creg)
      {
      case 0:
      case 4:
        nreg = NotRegistered;
        break;
      case 1:
        nreg = RegisteredHome;
        break;
      case 2:
        nreg = Searching;
        break;
      case 3:
        nreg = DeniedRegistration;
        break;
      case 5:
        nreg = RegisteredRoaming;
        break;
      default:
        break;
      }
    if (m_netreg != nreg)
      {
      m_netreg = nreg;
      ESP_LOGI(TAG, "CREG Network Registration: %s",SimcomNetRegName(m_netreg));
      }
    }
  else if (line.compare(0, 7, "+COPS: ") == 0)
    {
    size_t qp = line.find('"');
    if (qp != string::npos)
      {
      size_t qp2 = line.find('"',qp+1);
      if (qp2 != string::npos)
        {
        m_provider = line.substr(qp+1,qp2-qp-1);
        UpdateNetMetrics();
        }
      }
    }
  // SMS received (URC):
  else if (line.compare(0, 6, "+CMT: ") == 0)
    {
    size_t qp = line.find_last_of(',');
    if (qp != string::npos)
      {
      buf->m_userdata = (void*)atoi(line.substr(qp+1).c_str());
      ESP_LOGI(TAG,"SMS length is %d",(int)buf->m_userdata);
      }
    }
  // MMI/USSD response (URC):
  //  sent on all free channels, so we only process GSM_MUX_CHAN_CMD
  else if (channel == GSM_MUX_CHAN_CMD && line.compare(0, 7, "+CUSD: ") == 0)
    {
    // Format: +CUSD: 0,"…msg…",15
    // The message string may contain CR/LF so can come on multiple lines, with unknown length
    size_t q1 = line.find('"');
    size_t q2 = line.find_last_of('"');
    if (q1 == string::npos || q2 == q1)
      {
      // check again after adding next line:
      if (m_line_unfinished < 0)
        {
        m_line_unfinished = channel;
        m_line_buffer = line;
        m_line_buffer += "\n";
        }
      }
    else
      {
      // complete, process:
      m_line_buffer = line.substr(q1+1, q2-q1-1);
      ESP_LOGI(TAG, "USSD received: %s", m_line_buffer.c_str());
      MyEvents.SignalEvent("system.modem.received.ussd", (void*)m_line_buffer.c_str(),m_line_buffer.size()+1);
      m_line_unfinished = -1;
      m_line_buffer.clear();
      }
    }
  }

void simcom::PowerCycle()
  {
  ESP_LOGI(TAG, "Power Cycle");
#ifdef CONFIG_OVMS_COMP_MAX7317
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 0); // Modem EN/PWR line low
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 1); // Modem EN/PWR line high
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  MyPeripherals->m_max7317->Output(MODEM_EGPIO_PWR, 0); // Modem EN/PWR line low
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317
  }

void simcom::PowerSleep(bool onoff)
  {
#ifdef CONFIG_OVMS_COMP_MAX7317
  if (onoff)
    MyPeripherals->m_max7317->Output(MODEM_EGPIO_DTR, 1);
  else
    MyPeripherals->m_max7317->Output(MODEM_EGPIO_DTR, 0);
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317
  }

void simcom::tx(uint8_t* data, size_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)
  MyCommandApp.HexDump(TAG, "tx", (const char*)data, size);
  uart_write_bytes(m_uartnum, (const char*)data, size);
  }

void simcom::tx(const char* data, ssize_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)
  if (size == -1) size = strlen(data);
  MyCommandApp.HexDump(TAG, "tx", data, size);
  if (size > 0)
    ESP_LOGD(TAG, "tx scmd ch=0 len=%-4d: %s", size, data);
  uart_write_bytes(m_uartnum, data, size);
  }

void simcom::muxtx(int channel, uint8_t* data, size_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)
  MyCommandApp.HexDump(TAG, "mux tx", (const char*)data, size);
  m_mux.tx(channel, data, size);
  }

void simcom::muxtx(int channel, const char* data, ssize_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)
  if (size == -1) size = strlen(data);
  MyCommandApp.HexDump(TAG, "mux tx", data, size);
  if (size > 0 && (channel == GSM_MUX_CHAN_POLL || channel == GSM_MUX_CHAN_CMD))
    ESP_LOGD(TAG, "tx mcmd ch=%d len=%-4d: %s", channel, size, data);
  m_mux.tx(channel, (uint8_t*)data, size);
  }

bool simcom::txcmd(const char* data, ssize_t size)
  {
  if (!m_task) return false; // Quick exit if not task (we are stopped)
  if (size == -1) size = strlen(data);
  if (size <= 0) return false;
  if (m_mux.m_state == GsmMux::DlciClosed)
    {
    ESP_LOGD(TAG, "tx scmd ch=0 len=%-4d: %s", size, data);
    tx((uint8_t*)data, size);
    return true;
    }
  else if (m_mux.IsChannelOpen(GSM_MUX_CHAN_CMD))
    {
    ESP_LOGD(TAG, "tx mcmd ch=%d len=%-4d: %s", GSM_MUX_CHAN_CMD, size, data);
    m_mux.tx(GSM_MUX_CHAN_CMD, (uint8_t*)data, size);
    return true;
    }
  else
    return false;
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

void simcom_cmd(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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
  bool sent = MyPeripherals->m_simcom->txcmd(msg.c_str(),msg.length());
  if (verbosity >= COMMAND_RESULT_MINIMAL)
    {
    if (sent)
      writer->puts("SIMCOM command has been sent.");
    else
      writer->puts("ERROR: SIMCOM command channel not available!");
    }
  }

void simcom_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  bool debug = (strcmp(cmd->GetName(), "debug") == 0);
  
  writer->printf("Network Registration: %s\nProvider: %s\nSignal: %d dBm\n\nState: %s\n",
    SimcomNetRegName(MyPeripherals->m_simcom->m_netreg),
    MyPeripherals->m_simcom->m_provider.c_str(),
    UnitConvert(sq, dbm, MyPeripherals->m_simcom->m_sq),
    SimcomState1Name(MyPeripherals->m_simcom->m_state1));

  if (debug)
    {
    writer->printf("  Ticker: %d\n  User Data: %d\n",
      MyPeripherals->m_simcom->m_state1_ticker,
      MyPeripherals->m_simcom->m_state1_userdata);

    if (MyPeripherals->m_simcom->m_state1_timeout_goto != simcom::None)
      {
      writer->printf("  State Timeout Goto: %s (in %d seconds)\n",
        SimcomState1Name(MyPeripherals->m_simcom->m_state1_timeout_goto),
        MyPeripherals->m_simcom->m_state1_timeout_ticks);
      }

    writer->printf("\n  Mux\n    Status: %s\n",
      MyPeripherals->m_simcom->m_mux.IsMuxUp()?"up":"down");

    writer->printf("    Open Channels: %d\n",
      MyPeripherals->m_simcom->m_mux.m_openchannels);

    writer->printf("    Framing Errors: %d\n",
      MyPeripherals->m_simcom->m_mux.m_framingerrors);

    writer->printf("    Last RX frame: %d sec(s) ago\n",
      (MyPeripherals->m_simcom->m_mux.m_lastgoodrxframe==0)?0:(monotonictime-MyPeripherals->m_simcom->m_mux.m_lastgoodrxframe));

    writer->printf("    RX frames: %d\n",
      MyPeripherals->m_simcom->m_mux.m_rxframecount);

    writer->printf("    TX frames: %d\n",
      MyPeripherals->m_simcom->m_mux.m_txframecount);
    }

  if (MyPeripherals->m_simcom->m_ppp.m_connected)
    {
    writer->printf("\nPPP: Connected on channel: #%d\n",
      MyPeripherals->m_simcom->m_ppp.m_channel);
    }
  else
    {
    writer->puts("\nPPP: Not connected");
    }
  if (MyPeripherals->m_simcom->m_ppp.m_lasterrcode > 0)
    {
    writer->printf("     Last Error: %s\n",
      MyPeripherals->m_simcom->m_ppp.ErrCodeName(MyPeripherals->m_simcom->m_ppp.m_lasterrcode));
    }

  if (MyPeripherals->m_simcom->m_nmea.m_connected)
    {
    writer->printf("\nGPS: Connected on channel: #%d\n",
      MyPeripherals->m_simcom->m_nmea.m_channel);
    }
  else
    {
    writer->puts("\nGPS: Not connected");
    }
  if (debug)
    {
    writer->printf("     Status: %s\n",
      MyConfig.GetParamValueBool("modem", "enable.gps", false) ? "enabled" : "disabled");
    writer->printf("     Time: %s\n",
      MyConfig.GetParamValueBool("modem", "enable.gpstime", false) ? "enabled" : "disabled");
    }

  }

void simcom_setstate(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* statename = cmd->GetName();

  if (!MyPeripherals->m_simcom->IsStarted())
    {
    writer->puts("Error: SIMCOM task is not running");
    return;
    }

  simcom::SimcomState1 newstate = simcom::None;
  if (strcmp(statename,"CheckPowerOff")==0)
    newstate = simcom::CheckPowerOff;
  else if (strcmp(statename,"PoweringOn")==0)
    newstate = simcom::PoweringOn;
  else if (strcmp(statename,"PoweredOn")==0)
    newstate = simcom::PoweredOn;
  else if (strcmp(statename,"MuxStart")==0)
    newstate = simcom::MuxStart;
  else if (strcmp(statename,"NetWait")==0)
    newstate = simcom::NetWait;
  else if (strcmp(statename,"NetStart")==0)
    newstate = simcom::NetStart;
  else if (strcmp(statename,"NetLoss")==0)
    newstate = simcom::NetLoss;
  else if (strcmp(statename,"NetHold")==0)
    newstate = simcom::NetHold;
  else if (strcmp(statename,"NetSleep")==0)
    newstate = simcom::NetSleep;
  else if (strcmp(statename,"NetMode")==0)
    newstate = simcom::NetMode;
  else if (strcmp(statename,"NetDeepSleep")==0)
    newstate = simcom::NetDeepSleep;
  else if (strcmp(statename,"PoweringOff")==0)
    newstate = simcom::PoweringOff;
  else if (strcmp(statename,"PoweredOff")==0)
    newstate = simcom::PoweredOff;
  else if (strcmp(statename,"PowerOffOn")==0)
    newstate = simcom::PowerOffOn;

  if (newstate == simcom::None)
    {
    writer->printf("Error: Unrecognised state %s\n",statename);
    return;
    }

  MyPeripherals->m_simcom->SendSetState1(newstate);
  }

class SimcomInit
  {
  public: SimcomInit();
} SimcomInit  __attribute__ ((init_priority (4600)));

SimcomInit::SimcomInit()
  {
  ESP_LOGI(TAG, "Initialising SIMCOM (4600)");

  OvmsCommand* cmd_simcom = MyCommandApp.RegisterCommand("simcom","SIMCOM framework",simcom_status, "", 0, 0, false);
  cmd_simcom->RegisterCommand("tx","Transmit data on SIMCOM",simcom_tx, "", 1, INT_MAX);
  cmd_simcom->RegisterCommand("muxtx","Transmit data on SIMCOM MUX",simcom_muxtx, "<chan> <data>", 2, INT_MAX);
  OvmsCommand* cmd_status = cmd_simcom->RegisterCommand("status","Show SIMCOM status",simcom_status, "[debug]", 0, 0, false);
  cmd_status->RegisterCommand("debug","Show extended SIMCOM status",simcom_status, "", 0, 0, false);
  cmd_simcom->RegisterCommand("cmd","Send SIMCOM AT command",simcom_cmd, "<command>", 1, INT_MAX);

  OvmsCommand* cmd_setstate = cmd_simcom->RegisterCommand("setstate","SIMCOM state change framework");
  for (int x = simcom::CheckPowerOff; x<=simcom::PowerOffOn; x++)
    {
    cmd_setstate->RegisterCommand(SimcomState1Name((simcom::SimcomState1)x),"Force SIMCOM state change",simcom_setstate);
    }

  MyConfig.RegisterParam("modem", "Modem Configuration", true, true);
  // Our instances:
  //   'gsmlock': GSM network to lock to (at COPS stage)
  //   'apn': GSM APN
  //   'apn.user': GSM username
  //   'apn.password': GMS password
  //   'enable.sms': Is SMS enabled? yes/no (default: yes)
  //   'enable.net': Is NET enabled? yes/no (default: yes)
  //   'enable.gps': Is GPS enabled? yes/no (default: no)
  //   'enable.gpstime': use GPS time as system time? yes/no (default: no)
  }
