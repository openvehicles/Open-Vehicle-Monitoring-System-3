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

#ifndef __SIMCOM_H__
#define __SIMCOM_H__

#include "gsmpppos.h"
#include "gsmnmea.h"
#include "ovms.h"
#include "pcp.h"
#include "ovms_events.h"
#include "gsmmux.h"
#include "ovms_buffer.h"
#include "ovms_command.h"

#define SIMCOM_BUF_SIZE 1024

class simcom : public pcp, public InternalRamAllocated
  {
  public:
    simcom(const char* name, uart_port_t uartnum, int baud, int rxpin, int txpin, int pwregpio, int dtregpio);
    ~simcom();

  public:
    virtual void SetPowerMode(PowerMode powermode);
    void AutoInit();
    void Restart();
    void SupportSummary(OvmsWriter* writer);

  public:
    void tx(uint8_t* data, size_t size);
    void tx(const char* data, ssize_t size = -1);
    void muxtx(int channel, uint8_t* data, size_t size);
    void muxtx(int channel, const char* data, ssize_t size = -1);
    bool txcmd(const char* data, ssize_t size = -1);

  protected:
    TaskHandle_t m_task;
    volatile QueueHandle_t m_queue;
    uart_port_t m_uartnum;
    int m_baud;
    int m_rxpin;
    int m_txpin;
    int m_pwregpio;
    int m_dtregpio;

  public:
    enum SimcomState1
      {
      None,               // Initialised, and idle
      CheckPowerOff,      // Check modem is powered off, then => PoweredOff
      PoweringOn,         // Power on modem, then => PoweredOn
      PoweredOn,          // Check modem activity, then => MuxStart
      MuxStart,           // Start mux, then => NetWait
      NetWait,            // Wait for cellular service, then => NetStart
      NetStart,           // Start network, either =>NetHold, or =>...
      NetLoss,            // Handle loss of network connectivity
      NetHold,            // MUX established, but no data
      NetSleep,           // PowerMode=Sleep: MUX established, but no data
      NetMode,            // MUX and data established, start PPP
      NetDeepSleep,       // PowerMode=DeepSleep: power save
      PoweringOff,        // Power off modem, then => CheckPowerOff
      PoweredOff,         // Maintain a powered off state
      PowerOffOn,         // Power off modem, then => PoweringOn
      Development         // Let developer handle modem themselves
      };
    typedef enum
      {
      SETSTATE = UART_EVENT_MAX+1000,
      SHUTDOWN
      } event_type_t;
    typedef enum
      {
      NotRegistered = 0,
      Searching = 2,
      DeniedRegistration = 3,
      RegisteredHome = 1,
      RegisteredRoaming = 5,
      Unknown = 99
      } network_registration_t;
    typedef struct
      {
      event_type_t type;
      union
        {
        SimcomState1 newstate; // For SETSTATE
        } data;
      } simcom_event_t;
    typedef union
      {
      uart_event_t uart;
      simcom_event_t simcom;
      } SimcomOrUartEvent;

  public:
    OvmsBuffer   m_buffer;
    SimcomState1 m_state1;
    int          m_state1_ticker;
    SimcomState1 m_state1_timeout_goto;
    int          m_state1_timeout_ticks;
    int          m_state1_userdata;
    network_registration_t m_netreg;
    std::string  m_provider;
    int          m_sq;
    GsmMux       m_mux;
    GsmPPPOS     m_ppp;
    GsmNMEA      m_nmea;
    int          m_line_unfinished;
    std::string  m_line_buffer;
    bool         m_pincode_required;
    int          m_err_fifo_ovf;
    int          m_err_buffer_full;

  protected:
    void SetState1(SimcomState1 newstate);
    void State1Leave(SimcomState1 oldstate);
    void State1Enter(SimcomState1 newstate);
    SimcomState1 State1Activity();
    SimcomState1 State1Ticker1();
    bool StandardIncomingHandler(int channel, OvmsBuffer* buf);
    void StandardDataHandler(int channel, OvmsBuffer* buf);
    void StandardLineHandler(int channel, OvmsBuffer* buf, std::string line);
    void PowerCycle();
    void PowerSleep(bool onoff);

  public:
    void StartTask();
    void StopTask();
    void Task();
    void Ticker(std::string event, void* data);
    void EventListener(std::string event, void* data);
    void IncomingMuxData(GsmMuxChannel* channel);
    void SendSetState1(SimcomState1 newstate);
    bool IsStarted();
    void UpdateNetMetrics();
  };

#endif //#ifndef __SIMCOM_H__
