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
static const char *TAG = "modem";

#include <string.h>
#include <algorithm>
#include <functional>
#include "ovms_modem.h"
#include "ovms_peripherals.h"
#include "metrics_standard.h"
#include "ovms_config.h"
#include "ovms_events.h"
#include "ovms_notify.h"
#include "ovms_boot.h"

////////////////////////////////////////////////////////////////////////////////
// Global convenience variables

modem* MyModem = NULL;

////////////////////////////////////////////////////////////////////////////////
// General utility functions

const char* ModemState1Name(modem::modem_state1_t state)
  {
  switch (state)
    {
    case modem::None:            return "None";
    case modem::CheckPowerOff:   return "CheckPowerOff";
    case modem::PoweringOn:      return "PoweringOn";
    case modem::Identify:        return "Identify";
    case modem::PoweredOn:       return "PoweredOn";
    case modem::MuxStart:        return "MuxStart";
    case modem::NetWait:         return "NetWait";
    case modem::NetStart:        return "NetStart";
    case modem::NetLoss:         return "NetLoss";
    case modem::NetHold:         return "NetHold";
    case modem::NetSleep:        return "NetSleep";
    case modem::NetMode:         return "NetMode";
    case modem::NetDeepSleep:    return "NetDeepSleep";
    case modem::PoweringOff:     return "PoweringOff";
    case modem::PoweredOff:      return "PoweredOff";
    case modem::PowerOffOn:      return "PowerOffOn";
    case modem::Development:     return "Development";
    default:                     return "Undefined";
    };
  }

const char* ModemNetRegName(modem::network_registration_t netreg)
  {
  switch (netreg)
    {
    case modem::NotRegistered:       return "NotRegistered";
    case modem::Searching:           return "Searching";
    case modem::DeniedRegistration:  return "DeniedRegistration";
    case modem::RegisteredHome:      return "RegisteredHome";
    case modem::RegisteredRoaming:   return "RegisteredRoaming";
    default:                         return "Unknown";
    };
  }

////////////////////////////////////////////////////////////////////////////////
// The modem task itself.
// This runs as a freertos task, controlling the modem via a state diagram.

static void MODEM_task(void *pvParameters)
  {
  modem *me = (modem*)pvParameters;
  me->Task();
  }

void modem::Task()
  {
  modem_or_uart_event_t event;
  uint8_t data[128];

  // Init UART:
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
  uart_param_config(m_uartnum, &uart_config);
  uart_set_pin(m_uartnum, m_txpin, m_rxpin, 0, 0);
  uart_driver_install(m_uartnum,
    CONFIG_OVMS_HW_MODEM_UART_SIZE,
    CONFIG_OVMS_HW_MODEM_UART_SIZE,
    CONFIG_OVMS_HW_MODEM_QUEUE_SIZE,
    (QueueHandle_t*)&m_queue,
    ESP_INTR_FLAG_LEVEL2);

  // Queue processing loop:
  while (m_task)
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
              if (buffered_size>sizeof(data)) buffered_size = sizeof(data);
              int len = uart_read_bytes(m_uartnum, (uint8_t*)data, buffered_size, 100 / portTICK_RATE_MS);

              if (!m_buffer.Push(data,len))
                { m_err_driver_buffer_full++; }

              if (m_state1 == Development)
                { DevelopmentHexDump("rx", (const char*)data, len); }

              uart_get_buffered_data_len(m_uartnum, &buffered_size);

              modem_state1_t newstate = State1Activity();
              if ((newstate != m_state1)&&(newstate != None)) SetState1(newstate);
              }
            }
            break;

          case UART_FIFO_OVF:
            uart_flush(m_uartnum);
            m_err_uart_fifo_ovf++;
            ESP_LOGW(TAG, "UART hw fifo overflow");
            break;

          case UART_BUFFER_FULL:
            uart_flush(m_uartnum);
            m_err_uart_buffer_full++;
            ESP_LOGW(TAG, "UART ring buffer full");
            break;

          case UART_BREAK:
            ESP_LOGD(TAG, "UART rx break");
            break;

          case UART_PARITY_ERR:
            ESP_LOGW(TAG, "UART parity check error");
            m_err_uart_parity++;
            break;

          case UART_FRAME_ERR:
            ESP_LOGW(TAG, "UART frame error");
            m_err_uart_frame++;
            break;

          case UART_PATTERN_DET:
            ESP_LOGD(TAG, "UART pattern detected");
            break;

          default:
            ESP_LOGD(TAG, "UART unknown event type: %d", event.uart.type);
            break;
          }
        }
      else
        {
        // Must be a MODEM event
        switch (event.event.type)
          {
          case SETSTATE:
            {
            // Handle queued state change request
            SetState1(event.event.data.newstate);
            }
            break;

          case TICKER1:
            {
            m_state1_ticker++;
            modem_state1_t newstate = State1Ticker1();
            if ((newstate != m_state1)&&(newstate != None))
              {
              // Handle direct state transitions
              ESP_LOGD(TAG, "State transition %s => %s",
                ModemState1Name(m_state1),
                ModemState1Name(newstate));
              SetState1(newstate);
              }
            else
              {
              // Handle auto-state transitions
              if (m_state1_timeout_ticks > 0)
                {
                m_state1_timeout_ticks--;
                if (m_state1_timeout_ticks <= 0)
                  {
                  ESP_LOGD(TAG, "State timeout %s => %s",
                    ModemState1Name(m_state1),
                    ModemState1Name(m_state1_timeout_goto));
                  SetState1(m_state1_timeout_goto);
                  }
                }
              }
            }
            break;

          case SHUTDOWN:
            m_task = 0;
            break;

          default:
            break;
          }
        }
      }
    }

  // Shutdown:
  uart_driver_delete(m_uartnum);
  m_queue = 0;
  vTaskDelete(NULL);
  }

////////////////////////////////////////////////////////////////////////////////
// The modem
// This class implement an OVMS PCP for modem control

modem::modem(const char* name, uart_port_t uartnum, int baud, int rxpin, int txpin, int pwregpio, int dtregpio)
  : pcp(name), m_buffer(CONFIG_OVMS_HW_MODEM_BUFFER_SIZE)
  {
  MyModem = this;
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
  m_line_unfinished = -1;
  m_line_buffer.clear();
  m_netreg = Unknown;
  m_provider = "";
  m_sq = 99; // Unknown
  m_powermode = Off;
  m_pincode_required = false;
  m_err_uart_fifo_ovf = 0;
  m_err_uart_buffer_full = 0;
  m_err_uart_parity = 0;
  m_err_uart_frame = 0;
  m_err_driver_buffer_full = 0;
  m_nmea = NULL;
  m_mux = NULL;
  m_ppp = NULL;
  m_driver = NULL;

  ClearNetMetrics();
  StartTask();

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"ticker.1", std::bind(&modem::Ticker, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "system.shuttingdown", std::bind(&modem::EventListener, this, _1, _2));
  }

modem::~modem()
  {
  StopTask();
  if (m_driver != NULL)
    {
    delete m_driver;
    m_driver = NULL;
    }
  MyModem = NULL;
  }

////////////////////////////////////////////////////////////////////////////////
// Modem power control and initialisation

void modem::SetPowerMode(PowerMode powermode)
  {
  PowerMode original = m_powermode;
  pcp::SetPowerMode(powermode);

  switch (powermode)
    {
    case On:
    case Sleep:
    case DeepSleep:
      if ((original!=On)&&(original!=Sleep)&&(original!=DeepSleep))
        SendSetState1(PoweringOn); // We are not in the task, so queue the state change
      break;

    case Off:
      SendSetState1(PoweringOff); // We are not in the task, so queue the state change
      break;

    case Devel:
      SendSetState1(Development); // We are not in the task, so queue the state change
      break;

    default:
      break;
    }
  }

void modem::AutoInit()
  {
  if (MyConfig.GetParamValueBool("auto", "modem", false))
    SetPowerMode(On);
  }

void modem::Restart()
  {
  if (m_driver == NULL)
    {
    ESP_LOGW(TAG, "Cannot restart, as modem driver is not loaded");
    }
  else
    { m_driver->Restart(); }
  }

void modem::PowerOff()
  {
  if (m_driver == NULL)
    {
    ESP_LOGW(TAG, "Cannot power off, as modem driver is not loaded");
    }
  else
    { m_driver->PowerOff(); }
  }

void modem::PowerCycle()
  {
  if (m_driver == NULL)
    {
    ESP_LOGW(TAG, "Cannot power cycle, as modem driver is not loaded");
    }
  else
    { m_driver->PowerCycle(); }
  }

void modem::PowerSleep(bool onoff)
  {
  if (m_driver == NULL)
    {
    ESP_LOGW(TAG, "Cannot power sleep, as modem driver is not loaded");
    }
  else
    { m_driver->PowerSleep(onoff); }
  }

void modem::SupportSummary(OvmsWriter* writer, bool debug /*=FALSE*/)
  {
  writer->puts("MODEM Status");

  if (m_pincode_required)
    {
    writer->printf("  PIN: SIM card PIN code required.\n");
    if (MyConfig.GetParamValueBool("modem","wrongpincode"))
      writer->printf("       Error: Wrong PIN (%s) entered\n", MyConfig.GetParamValue("modem","pincode").c_str());
    }

  writer->printf("  Model: %s\n",m_model.c_str());

  if (m_powermode != Off)
    {
    writer->printf("  Network Registration: %s\n    Provider: %s\n    Signal: %d dBm\n",
      ModemNetRegName(m_netreg),
      m_provider.c_str(),
      UnitConvert(sq, dbm, m_sq));
    }

  writer->printf("  State: %s\n", ModemState1Name(m_state1));
  if (debug)
    {
    writer->printf("    Ticker: %d\n    User Data: %d\n",
      m_state1_ticker,
      m_state1_userdata);
    writer->printf(
      "  UART:\n"
      "    FIFO overflows: %d\n"
      "    Buffer overflows: %d\n"
      "    Parity errors: %d\n"
      "    Frame errors: %d\n"
      "    Driver Buffer overflows: %d\n"
      , m_err_uart_fifo_ovf
      , m_err_uart_buffer_full
      , m_err_uart_parity
      , m_err_uart_frame
      , m_err_driver_buffer_full);

    if (m_state1_timeout_goto != None)
      {
      writer->printf("    State Timeout Goto: %s (in %d seconds)\n",
        ModemState1Name(m_state1_timeout_goto),
        m_state1_timeout_ticks);
      }
    }

  if (m_mux)
    {
    writer->printf("  Mux: Status %s\n", m_mux->IsMuxUp()?"up":"down");
    if (debug)
      {
      writer->printf("    Open Channels: %d\n", m_mux->m_openchannels);
      writer->printf("    Framing Errors: %d\n", m_mux->m_framingerrors);
      writer->printf("    RX frames: %d\n", m_mux->m_rxframecount);
      writer->printf("    TX frames: %d\n", m_mux->m_txframecount);
      writer->printf("    Last RX frame: %d sec(s) ago\n", m_mux->GoodFrameAge());
      }
    }
  else
    {
    writer->puts("  Mux: Not running");
    }

  if (m_ppp == NULL)
    {
    writer->puts("  PPP: Not running");
    }
  else if (m_ppp->m_connected)
    {
    writer->printf("  PPP: Connected on channel: #%d\n", m_ppp->m_channel);
    }
  else
    {
    writer->puts("  PPP: Not connected");
    }
  if ((m_ppp != NULL)&&(m_ppp->m_lasterrcode > 0))
    {
    writer->printf("     Last Error: %s\n", m_ppp->ErrCodeName(m_ppp->m_lasterrcode));
    }

  if (m_nmea==NULL)
    {
    writer->puts("  GPS: Not running");
    }
  else
    {
    if (m_nmea->m_connected)
      {
      writer->printf("  GPS: Connected on channel: #%d\n", m_nmea->m_channel_nmea);
      }
    else
      {
      writer->puts("  GPS: Not connected");
      }
    if (debug)
      {
      writer->printf("     Status: %s\n",
        MyConfig.GetParamValueBool("modem", "enable.gps", false) ? "enabled" : "disabled");
      writer->printf("     Time: %s\n",
        MyConfig.GetParamValueBool("modem", "enable.gpstime", false) ? "enabled" : "disabled");
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
// Modem state diagram implementation

void modem::SetState1(modem_state1_t newstate)
  {
  m_state1_timeout_ticks = -1;
  m_state1_timeout_goto = None;
  if (m_state1 != None) State1Leave(m_state1);
  State1Enter(newstate);
  }

modem::modem_state1_t modem::GetState1()
  {
  return m_state1;
  }

void modem::State1Leave(modem_state1_t oldstate)
  {
  if (m_driver)
    {
    // See if the driver want's to override our default behaviour
    if (m_driver->State1Leave(oldstate)) return;
    }

  switch (oldstate)
    {
    case CheckPowerOff:
      break;
    case PoweringOn:
      break;
    case Identify:
      break;
    case PoweredOn:
      break;
    case NetMode:
      break;
    case NetDeepSleep:
      break;
    case PoweringOff:
      break;
    case PoweredOff:
      break;
    case Development:
      break;
    default:
      break;
    }
  }

void modem::State1Enter(modem_state1_t newstate)
  {
  m_state1 = newstate;
  m_state1_ticker = 0;
  m_state1_userdata = 0;

  if (m_driver == NULL)
    {
    // Register a default modem driver
    std::string driver = MyConfig.GetParamValue("modem", "driver","auto");
    SetModemDriver(driver.c_str());
    }

  ESP_LOGI(TAG, "State: Enter %s state",ModemState1Name(newstate));

  // See if the driver want's to override our default behaviour
  if (m_driver->State1Enter(newstate)) return;

  switch (m_state1)
    {
    case CheckPowerOff:
      ClearNetMetrics();
      PowerOff();
      m_state1_timeout_ticks = 15;
      m_state1_timeout_goto = PoweredOff;
      break;

    case PoweringOn:
      if ( (strcmp(m_driver->GetModel(),"auto") != 0) &&
           (MyConfig.GetParamValue("modem", "driver","auto").compare("auto")==0) )
        {
        SetModemDriver("auto");
        }
      ClearNetMetrics();
      MyEvents.SignalEvent("system.modem.poweringon", NULL);
      PowerCycle();
      m_state1_timeout_ticks = 20;
      m_state1_timeout_goto = PoweringOn;
      break;

    case Identify:
      m_state1_timeout_ticks = 60;
      m_state1_timeout_goto = PowerOffOn;
      break;

    case PoweredOn:
      ClearNetMetrics();
      MyEvents.SignalEvent("system.modem.poweredon", NULL);
      m_state1_timeout_ticks = 30;
      m_state1_timeout_goto = PoweringOn;
      break;

    case MuxStart:
      MyEvents.SignalEvent("system.modem.muxstart", NULL);
      m_state1_timeout_ticks = 120;
      m_state1_timeout_goto = PoweringOn;
      StartMux();
      break;

    case NetWait:
      MyEvents.SignalEvent("system.modem.netwait", NULL);
      StartNMEA();
      break;

    case NetStart:
      MyEvents.SignalEvent("system.modem.netstart", NULL);
      m_state1_timeout_ticks = 30;
      m_state1_timeout_goto = PowerOffOn;
      break;

    case NetLoss:
      MyEvents.SignalEvent("system.modem.netloss", NULL);
      m_state1_timeout_ticks = 10;
      m_state1_timeout_goto = NetWait;
      if (m_mux != NULL)
        { m_mux->tx(m_mux_channel_POLL, "AT+CGATT=0\r\n"); }
      StopPPP();
      break;

    case NetHold:
      MyEvents.SignalEvent("system.modem.nethold", NULL);
      break;

    case NetSleep:
      MyEvents.SignalEvent("system.modem.netsleep", NULL);
      StopPPP();
      StopNMEA();
      break;

    case NetMode:
      MyEvents.SignalEvent("system.modem.netmode", NULL);
      StartPPP();
      break;

    case NetDeepSleep:
      ClearNetMetrics();
      MyEvents.SignalEvent("system.modem.netdeepsleep", NULL);
      StopPPP();
      StopNMEA();
      break;

    case PoweringOff:
      ClearNetMetrics();
      StopPPP();
      StopNMEA();
      MyEvents.SignalEvent("system.modem.stop",NULL);
      PowerCycle();
      m_state1_timeout_ticks = 20;
      m_state1_timeout_goto = CheckPowerOff;
      break;

    case PoweredOff:
      ClearNetMetrics();
      MyEvents.SignalEvent("system.modem.poweredoff", NULL);
      if (MyBoot.IsShuttingDown()) MyBoot.RestartReady(TAG);
      StopMux();
      if (m_driver)
        {
        delete m_driver;
        m_driver = NULL;
        m_model.clear();
        }
      break;

    case PowerOffOn:
      ClearNetMetrics();
      StopPPP();
      StopNMEA();
      StopMux();
      MyEvents.SignalEvent("system.modem.stop",NULL);
      PowerCycle();
      m_state1_timeout_ticks = 3;
      m_state1_timeout_goto = PoweringOn;
      break;

    case Development:
      break;

    default:
      break;
    }
  }

modem::modem_state1_t modem::State1Activity()
  {
  if (m_driver)
    {
    // See if the driver want's to override our default behaviour
    modem::modem_state1_t driverstate = m_driver->State1Activity(m_state1);
    if (driverstate == modem::None) return m_state1;
    if (driverstate != m_state1) return driverstate;
    }

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
      if (MyConfig.GetParamValue("modem", "driver","auto").compare("auto")==0)
        return Identify;
      else
        return PoweredOn;
      break;

    case Identify:
      if (IdentifyModel()) return PoweredOn;
      break;

    case PoweredOn:
      if (StandardIncomingHandler(m_mux_channel_CTRL, &m_buffer))
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
      if (m_mux != NULL)
        { m_mux->Process(&m_buffer); }
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

    case Development:
      if ((m_mux != NULL)&&(m_mux->IsMuxUp()))
        {
        m_mux->Process(&m_buffer);
        }
      else
        {
        size_t needed = m_buffer.UsedSpace();
        char* result = new char[needed+1];
        m_buffer.Pop(needed, (uint8_t*)result);
        result[needed] = 0;
        MyCommandApp.HexDump(TAG, "rx", result, needed);
        delete [] result;
        }
      break;

    default:
      break;
    }

  return None;
  }

modem::modem_state1_t modem::State1Ticker1()
  {
  if ((m_mux != NULL)&&(m_mux->GoodFrameAge() > 180))
    {
    // Mux is up, but we haven't had a good MUX frame in 3 minutes.
    // Let's assume the MUX has failed
    ESP_LOGW(TAG, "3 minutes since last MUX rx frame - assume MUX has failed");
    StopPPP();
    StopNMEA();
    StopMux();
    MyEvents.SignalEvent("system.modem.stop",NULL);
    PowerCycle();
    return PoweringOn;
    }

  if (m_driver)
    {
    // See if the driver want's to override our default behaviour
    modem::modem_state1_t driverstate = m_driver->State1Ticker1(m_state1);
    if (driverstate == modem::None) return m_state1;
    if (driverstate != m_state1) return driverstate;
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

    case Identify:
      tx("AT+CGMM\r\n");
      break;

    case PoweredOn:
      if (m_powermode == DeepSleep)
        {
        return NetDeepSleep; // Just hold, without starting the network
        }
      switch (m_state1_ticker)
        {
        case 10:
          tx("AT+CPIN?;+CREG=1;+CTZU=1;+CTZR=1;+CLIP=1;+CMGF=1;+CNMI=1,2,0,0,0;+CSDH=1;+CMEE=2;+CSQ;+AUTOCSQ=1,1;E0;S0=0\r\n");
          break;
        case 12:
          tx("AT+CGMR;+ICCID\r\n");
          break;
        case 20:
          tx("AT+CMUX=0\r\n");
          break;
        default:
          break;
        }
      break;

    case MuxStart:
      if (m_mux != NULL)
        {
        if ((m_state1_ticker>5)&&((m_state1_ticker % 30) == 0))
          { m_mux->tx(m_mux_channel_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n"); }
        if (m_mux->IsMuxUp())
          return NetWait;
        }
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
      if ((m_mux != NULL)&&(m_state1_ticker>3)&&((m_state1_ticker % 10) == 0))
        { m_mux->tx(m_mux_channel_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n"); }
      break;

    case NetStart:
      if (m_powermode == Sleep)
        {
        return NetSleep; // Just hold, without starting the network
        }
      if (m_state1_ticker == 1)
        {
        m_state1_userdata = 1;
        if (m_mux != NULL)
          {
          std::string apncmd("AT+CGDCONT=1,\"IP\",\"");
          apncmd.append(MyConfig.GetParamValue("modem", "apn"));
          apncmd.append("\";+CGDATA=\"PPP\",1\r\n");
          m_mux->tx(m_mux_channel_DATA,apncmd.c_str());
          }
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
      if ((m_mux != NULL)&&(m_state1_ticker>5)&&((m_state1_ticker % 30) == 0))
        { m_mux->tx(m_mux_channel_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n"); }
      break;

    case NetSleep:
      if (m_powermode == On) return NetWait;
      if (m_powermode != Sleep) return PoweringOn;
      if ((m_mux != NULL)&&(m_state1_ticker>5)&&((m_state1_ticker % 30) == 0))
        { m_mux->tx(m_mux_channel_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n"); }
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
        ESP_LOGW(TAG, "Lost network connection (NetworkRegistration in NetMode)");
        return NetLoss;
        }
      if (m_state1_userdata == 99)
        {
        // We've lost the network connection
        ESP_LOGW(TAG, "Lost network connection (+PPP disconnect in NetMode)");
        return NetLoss;
        }
      if ((m_mux != NULL)&&(m_state1_ticker>5)&&((m_state1_ticker % 30) == 0))
        { m_mux->tx(m_mux_channel_POLL, "AT+CREG?;+CCLK?;+CSQ;+COPS?\r\n"); }
      break;

    case NetDeepSleep:
      if (m_powermode != DeepSleep) return PoweringOn;
      break;

    case PoweringOff:
      break;

    case PoweredOff:
      break;

    case Development:
      break;

    default:
      break;
    }

  return None;
  }

bool modem::StandardIncomingHandler(int channel, OvmsBuffer* buf)
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

void modem::StandardDataHandler(int channel, OvmsBuffer* buf)
  {
  // We have SMS data ready...
  size_t needed = (size_t)buf->m_userdata;

  char* result = new char[needed+1];
  buf->Pop(needed, (uint8_t*)result);
  result[needed] = 0;

  // This may be a big performance hit, so disable for the moment
  // MyCommandApp.HexDump(TAG, "data", result, needed);

  delete [] result;
  buf->m_userdata = 0;
  }

void modem::StandardLineHandler(int channel, OvmsBuffer* buf, std::string line)
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
    SetSignalQuality(atoi(line.substr(6).c_str()));
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
    SetNetworkRegistration(nreg);
    }
  else if (line.compare(0, 7, "+COPS: ") == 0)
    {
    size_t qp = line.find('"');
    if (qp != string::npos)
      {
      size_t qp2 = line.find('"',qp+1);
      if (qp2 != string::npos)
        {
        SetProvider(line.substr(qp+1,qp2-qp-1));
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
  // SIM card PIN code required:
  else if (line.compare(0, 14, "+CPIN: SIM PIN") == 0)
    {
    ESP_LOGI(TAG,"SIM card PIN code required");
    m_pincode_required=true;
    std::string pincode = MyConfig.GetParamValue("modem", "pincode");
    if (!MyConfig.GetParamValueBool("modem","wrongpincode"))
      {
      if (pincode != "")
        {
        ESP_LOGI(TAG,"Using PIN code \"%s\"",pincode.c_str());
        std::string at = "AT+CPIN=\"";
        at.append(pincode);
        at.append("\"\r\n");
        tx(at.c_str());
        }
      else
        {
        ESP_LOGE(TAG,"SIM card PIN code not set!");
        MyEvents.SignalEvent("system.modem.pincode_not_set", NULL);
        MyNotify.NotifyString("alert", "modem.no_pincode", "No PIN code for SIM card configured!");
        }
      } else
      {
      ESP_LOGW(TAG,"Wrong PIN code (%s) previously entered.. Will not re-enter until changed!", pincode.c_str());
      MyNotify.NotifyStringf("alert", "modem.wrongpincode", "Wrong pin code (%s) previously entered! Did not re-enter it..",pincode.c_str());
      }
    }
  else if (line.compare(0, 30, "+CME ERROR: incorrect password") == 0)
    {
    ESP_LOGE(TAG,"Wrong PIN code entered!");
    MyEvents.SignalEvent("system.modem.wrongpingcode", NULL);
    MyNotify.NotifyStringf("alert", "modem.wrongpincode", "Wrong pin code (%s) entered!", MyConfig.GetParamValue("modem", "pincode"));
    MyConfig.SetParamValueBool("modem","wrongpincode",true);
    }

  // MMI/USSD response (URC):
  //  sent on all free channels, so we only process m_mux_channel_CMD
  else if (channel == m_mux_channel_CMD && line.compare(0, 7, "+CUSD: ") == 0)
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

bool modem::IdentifyModel()
  {
  while (m_buffer.HasLine() >= 0)
    {
    std::string line = m_buffer.ReadLine();

    for (OvmsModemFactory::map_modemdriver_t::iterator k=MyModemFactory.m_drivermap.begin();
         k!=MyModemFactory.m_drivermap.end();
         ++k)
      {
      if (line.find(k->first) != string::npos)
        {
        ESP_LOGI(TAG, "Identified Modem: %s/%s", k->first,k->second.name );
        SetModemDriver(k->first);
        return true;
        }
      }
    }
  return false;
  }

////////////////////////////////////////////////////////////////////////////////
// Modem API functionality

void modem::tx(uint8_t* data, size_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)

  if (m_state1 == Development)
    { DevelopmentHexDump("tx", (const char*)data, size); }

  uart_write_bytes(m_uartnum, (const char*)data, size);
  }

void modem::tx(const char* data, ssize_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)
  if (size == -1) size = strlen(data);

  if (m_state1 == Development)
    { DevelopmentHexDump("tx", data, size); }

  if (size > 0)
    ESP_LOGD(TAG, "tx scmd ch=0 len=%-4d: %s", size, data);

  uart_write_bytes(m_uartnum, data, size);
  }

void modem::muxtx(int channel, uint8_t* data, size_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)

  if (m_state1 == Development)
    { DevelopmentHexDump("mux-tx", (const char*)data, size); }

  if (m_mux != NULL)
    { m_mux->tx(channel, data, size); }
  else
    { ESP_LOGE(TAG, "Attempt to transmit on a mux not running"); }
  }

void modem::muxtx(int channel, const char* data, ssize_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)
  if (size == -1) size = strlen(data);

  if (m_state1 == Development)
    { DevelopmentHexDump("mux-tx", data, size); }

  if (size > 0 && (channel == m_mux_channel_POLL || channel == m_mux_channel_CMD))
    ESP_LOGD(TAG, "tx mcmd ch=%d len=%-4d: %s", channel, size, data);

  if (m_mux != NULL)
    { m_mux->tx(channel, (uint8_t*)data, size); }
  else
    { ESP_LOGE(TAG, "Attempt to transmit on a mux not running"); }
  }

bool modem::txcmd(const char* data, ssize_t size)
  {
  if (!m_task) return false; // Quick exit if not task (we are stopped)
  if (size == -1) size = strlen(data);
  if (size <= 0) return false;

  if ((m_mux == NULL)||(m_mux->m_state == GsmMux::DlciClosed))
    {
    if (m_state1 == Development)
      { DevelopmentHexDump("tx-cmd", (const char*)data, size); }

    ESP_LOGD(TAG, "tx scmd ch=0 len=%-4d: %s", size, data);

    tx((uint8_t*)data, size);
    return true;
    }
  else if ((m_mux != NULL)&&(m_mux->IsChannelOpen(m_mux_channel_CMD)))
    {
    if (m_state1 == Development)
      { DevelopmentHexDump("mux-tx-cmd", (const char*)data, size); }

    ESP_LOGD(TAG, "tx mcmd ch=%d len=%-4d: %s", m_mux_channel_CMD, size, data);

    m_mux->tx(m_mux_channel_CMD, (uint8_t*)data, size);
    return true;
    }
  else
    return false;
  }

////////////////////////////////////////////////////////////////////////////////
// High level API functions

void modem::StartTask()
  {
  if (!m_task)
    {
    ESP_LOGV(TAG, "Starting modem task");
    xTaskCreatePinnedToCore(MODEM_task, "OVMS MODEM", CONFIG_OVMS_HW_MODEM_STACK_SIZE, (void*)this, 20, &m_task, CORE(0));
    }
  }

void modem::StopTask()
  {
  if (m_task)
    {
    ESP_LOGV(TAG, "Stopping modem task (and waiting for completion)");
    modem_or_uart_event_t ev;
    ev.event.type = SHUTDOWN;
    xQueueSend(m_queue, &ev, portMAX_DELAY);
    while (m_queue)
      vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGV(TAG, "Modem task has stopped");
    }
  }

void modem::StartNMEA()
  {
  if ( (m_nmea == NULL) &&
       (MyConfig.GetParamValueBool("modem", "enable.gps", false)) )
    {
    ESP_LOGV(TAG, "Starting NMEA");
    m_nmea = new GsmNMEA(m_mux, m_mux_channel_NMEA, m_mux_channel_CMD);
    m_nmea->Startup();
    }
  }

void modem::StopNMEA()
  {
  if (m_nmea != NULL)
    {
    ESP_LOGV(TAG, "Stopping NMEA");
    m_nmea->Shutdown();
    delete m_nmea;
    m_nmea = NULL;
    }
  }

void modem::StartMux()
  {
  if (m_mux == NULL)
    {
    ESP_LOGV(TAG, "Starting MUX");
    m_mux = new GsmMux(this, m_mux_channels);
    m_mux->Startup();
    }
  }

void modem::StopMux()
  {
  if (m_mux != NULL)
    {
    ESP_LOGV(TAG, "Stopping MUX");
    m_mux->Shutdown();
    delete m_mux;
    m_mux = NULL;
    }
  }

void modem::StartPPP()
  {
  if (m_ppp == NULL)
    {
    ESP_LOGV(TAG, "Starting PPP");
    m_ppp = new GsmPPPOS(m_mux, m_mux_channel_DATA);
    m_ppp->Initialise();
    m_ppp->Startup();
    }
  }

void modem::StopPPP()
  {
  if (m_ppp != NULL)
    {
    ESP_LOGV(TAG, "Stopping PPP");
    m_ppp->Shutdown(true);
    delete m_ppp;
    m_ppp = NULL;
    }
  }

void modem::SetModemDriver(const char* ModelType)
  {
  if (m_driver)
    {
    ESP_LOGD(TAG, "Remove old '%s' modem driver", m_driver->GetModel());
    delete m_driver;
    m_driver = NULL;
    }

  ESP_LOGI(TAG, "Set modem driver to '%s'", ModelType);
  m_driver = MyModemFactory.NewModemDriver(ModelType);
  m_model = std::string(ModelType);
  m_mux_channels = m_driver->GetMuxChannels();
  m_mux_channel_CTRL = m_driver->GetMuxChannelCTRL();
  m_mux_channel_NMEA = m_driver->GetMuxChannelNMEA();
  m_mux_channel_DATA = m_driver->GetMuxChannelDATA();
  m_mux_channel_POLL = m_driver->GetMuxChannelPOLL();
  m_mux_channel_CMD = m_driver->GetMuxChannelCMD();
  }

void modem::Ticker(std::string event, void* data)
  {
  modem_or_uart_event_t ev;

  ev.event.type = TICKER1;

  xQueueSend(m_queue,&ev,0);
  }

void modem::EventListener(std::string event, void* data)
  {
  if (event == "system.shuttingdown")
    {
    if (m_state1 != PoweredOff)
      {
      MyBoot.RestartPending(TAG);
      SendSetState1(PoweringOff); // We are not in the task, so queue the state change
      }
    }
  }

void modem::IncomingMuxData(GsmMuxChannel* channel)
  {
  // The MUX has indicated there is data on the specified channel

  // This maybe a big performance hit, so disable for the moment
  // ESP_LOGD(TAG, "IncomingMuxData(CHAN=%d, buffer used=%d)",channel->m_channel,channel->m_buffer.UsedSpace());

  if (channel->m_channel == m_mux_channel_CTRL)
    {
    channel->m_buffer.EmptyAll();
    }
  else if (channel->m_channel == m_mux_channel_NMEA)
    {
    while (channel->m_buffer.HasLine() >= 0)
      {
      if (m_nmea != NULL) m_nmea->IncomingLine(channel->m_buffer.ReadLine());
      }
    }
  else if (channel->m_channel == m_mux_channel_DATA)
    {
    if (m_state1 == NetMode)
      {
      uint8_t buf[32];
      size_t n;
      while ((m_ppp != NULL)&&(n = channel->m_buffer.Pop(sizeof(buf),buf)) > 0)
        {
        m_ppp->IncomingData(buf,n);
        }
      }
    else
      {
      StandardIncomingHandler(channel->m_channel, &channel->m_buffer);
      }
    }
  else if (channel->m_channel == m_mux_channel_POLL)
    {
    StandardIncomingHandler(channel->m_channel, &channel->m_buffer);
    }
  else if (channel->m_channel == m_mux_channel_CMD)
    {
    StandardIncomingHandler(channel->m_channel, &channel->m_buffer);
    }
  else
    {
    ESP_LOGW(TAG, "Unrecognised mux channel #%d", channel->m_channel);
    }
  }

void modem::SendSetState1(modem_state1_t newstate)
  {
  modem_or_uart_event_t ev;

  ev.event.type = SETSTATE;
  ev.event.data.newstate = newstate;

  xQueueSend(m_queue,&ev,0);
  }

bool modem::IsStarted()
  {
  return (m_task != NULL);
  }

void modem::SetNetworkRegistration(network_registration_t netreg)
  {
  if (netreg != m_netreg)
    {
    m_netreg = netreg;
    const char *v = ModemNetRegName(m_netreg);
    ESP_LOGI(TAG, "Network Registration status: %s", v);
    StdMetrics.ms_m_net_mdm_netreg->SetValue(v);
    }
  }

void modem::SetProvider(std::string provider)
  {
  // Trim provider, if necessary
  provider.erase(provider.begin(),
                 std::find_if(provider.begin(), provider.end(),
                 std::not1(std::ptr_fun<int, int>(std::isspace))));

  if (m_provider.compare(provider) != 0)
    {
    m_provider = provider;
    ESP_LOGI(TAG, "Network Provider is: %s",m_provider.c_str());
    StdMetrics.ms_m_net_mdm_network->SetValue(m_provider);
    if (StdMetrics.ms_m_net_type->AsString() == "modem")
      {
      StdMetrics.ms_m_net_provider->SetValue(m_provider);
      }
    }
  }

void modem::SetSignalQuality(int newsq)
  {
  if (m_sq != newsq)
    {
    m_sq = newsq;
    ESP_LOGI(TAG, "Signal Quality is: %d (%d dBm)", m_sq, UnitConvert(sq, dbm, m_sq));
    StdMetrics.ms_m_net_mdm_sq->SetValue(m_sq, sq);
    if (StdMetrics.ms_m_net_type->AsString() == "modem")
      {
      StdMetrics.ms_m_net_sq->SetValue(m_sq, sq);
      }
    }
  }

void modem::ClearNetMetrics()
  {
  m_netreg = Unknown;
  StdMetrics.ms_m_net_mdm_netreg->Clear();

  m_provider = "";
  StdMetrics.ms_m_net_mdm_network->Clear();

  m_sq = 99;
  StdMetrics.ms_m_net_mdm_sq->Clear();

  if (StdMetrics.ms_m_net_type->AsString() == "modem")
    {
    StdMetrics.ms_m_net_provider->Clear();
    StdMetrics.ms_m_net_sq->Clear();
    }
  }

void modem::UpdateNetMetrics()
  {
  if (StdMetrics.ms_m_net_type->AsString() == "modem")
    {
    StdMetrics.ms_m_net_provider->SetValue(m_provider);
    StdMetrics.ms_m_net_sq->SetValue(m_sq, sq);
    }
  }

////////////////////////////////////////////////////////////////////////////////
// Command interfaces

void cellular_tx(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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
  MyModem->tx(msg.c_str(),msg.length());
  }

void cellular_muxtx(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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
  MyModem->muxtx(channel,msg.c_str(),msg.length());
  }

void cellular_cmd(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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
  bool sent = MyModem->txcmd(msg.c_str(),msg.length());
  if (verbosity >= COMMAND_RESULT_MINIMAL)
    {
    if (sent)
      writer->puts("MODEM command has been sent.");
    else
      writer->puts("ERROR: MODEM command channel not available!");
    }
  }

void cellular_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyModem->SupportSummary(writer, (strcmp(cmd->GetName(), "debug") == 0));
  }

void modem_setstate(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* statename = cmd->GetName();

  if (!MyModem->IsStarted())
    {
    writer->puts("Error: MODEM task is not running");
    return;
    }

  for (int newstate = (int)modem::None;
        (newstate <= (int)modem::Development);
        newstate++)
    {
    if (strcmp(statename,ModemState1Name((modem::modem_state1_t)newstate)) == 0)
      {
      writer->printf("Set modem to state: %s",statename);
      MyModem->SendSetState1((modem::modem_state1_t)newstate);
      return;
      }
    }

  writer->printf("Error: Unrecognised state %s\n",statename);
  }

void cellular_drivers(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Type       Name");
  for (OvmsModemFactory::map_modemdriver_t::iterator k=MyModemFactory.m_drivermap.begin();
       k!=MyModemFactory.m_drivermap.end();
       ++k)
    {
    writer->printf("%-10.10s %s\n",k->first,k->second.name);
    }
  }

////////////////////////////////////////////////////////////////////////////////
// Development assistance functions

void modem::DevelopmentHexDump(const char* prefix, const char* data, size_t length, size_t colsize /*=16*/)
  {
  char* buffer = NULL;
  int rlength = (int)length;

  while (rlength>0)
    {
    rlength = FormatHexDump(&buffer, data, rlength, colsize);
    data += colsize;
    ESP_LOGI(TAG, "%s: %s", prefix, buffer);
    }

  if (buffer) free(buffer);
  }

////////////////////////////////////////////////////////////////////////////////
// Modem Initialisation and Registrations

class ModemInit
  {
  public: ModemInit();
} MyModemInit  __attribute__ ((init_priority (4600)));

ModemInit::ModemInit()
  {
  ESP_LOGI(TAG, "Initialising MODEM (4600)");

  OvmsCommand* cmd_cellular = MyCommandApp.RegisterCommand("cellular","CELLULAR MODEM framework",cellular_status, "", 0, 0, false);
  cmd_cellular->RegisterCommand("tx","Transmit data on CELLULAR MODEM",cellular_tx, "", 1, INT_MAX);
  cmd_cellular->RegisterCommand("muxtx","Transmit data on CELLULAR MODEM MUX",cellular_muxtx, "<chan> <data>", 2, INT_MAX);
  cmd_cellular->RegisterCommand("cmd","Send CELLULAR MODEM AT command",cellular_cmd, "<command>", 1, INT_MAX);
  cmd_cellular->RegisterCommand("drivers","Show supported CELLULAR MODEM drivers",cellular_drivers, "", 0, 0);
  OvmsCommand* cmd_status = cmd_cellular->RegisterCommand("status","Show CELLULAR MODEM status",cellular_status, "[debug]", 0, 0, false);
  cmd_status->RegisterCommand("debug","Show extended CELLULAR MODEM status",cellular_status, "", 0, 0, false);

  OvmsCommand* cmd_setstate = cmd_cellular->RegisterCommand("setstate","CELLULAR MODEM state change framework");
  for (int x = modem::CheckPowerOff; x<=modem::PowerOffOn; x++)
    {
    cmd_setstate->RegisterCommand(ModemState1Name((modem::modem_state1_t)x),"Force CELLULAR MODEM state change",modem_setstate);
    }

  MyConfig.RegisterParam("modem", "Modem Configuration", true, true);
  // Our instances:
  //   'driver': Driver to use (default: auto)
  //   'gsmlock': GSM network to lock to (at COPS stage)
  //   'apn': GSM APN
  //   'apn.user': GSM username
  //   'apn.password': GMS password
  //   'enable.sms': Is SMS enabled? yes/no (default: yes)
  //   'enable.net': Is NET enabled? yes/no (default: yes)
  //   'enable.gps': Is GPS enabled? yes/no (default: no)
  //   'enable.gpstime': use GPS time as system time? yes/no (default: no)
  }
