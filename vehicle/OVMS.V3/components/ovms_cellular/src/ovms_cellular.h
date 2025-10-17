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

#ifndef __OVMS_CELLULAR_H__
#define __OVMS_CELLULAR_H__

#include <map>
#include <vector>
#include <string>
#include <driver/uart.h>
#include "ovms.h"
#include "ovms_utils.h"
#include "pcp.h"
#include "ovms_events.h"
#include "gsmmux.h"
#include "gsmpppos.h"
#include "gsmnmea.h"
#include "ovms_buffer.h"
#include "ovms_command.h"
#include "ovms_mutex.h"
#include "ovms_semaphore.h"

using namespace std;

#define CELLULAR_NETREG_COUNT 3

class modemdriver;  // Forward declaration

class modem : public pcp, public InternalRamAllocated
  {
  public:
    modem(const char* name, uart_port_t uartnum, int baud, int rxpin, int txpin, int pwregpio, int dtregpio);
    ~modem();

  protected:
    volatile TaskHandle_t m_task;
    volatile QueueHandle_t m_queue;
    int m_baud;
    int m_rxpin;
    int m_txpin;
    int m_pwregpio;
    int m_dtregpio;
    float m_good_dbm;
    float m_bad_dbm;
    bool m_good_signal;

  public:
    typedef enum
      {
      None,               // Initialised, and idle
      CheckPowerOff,      // Check modem is powered off, then => PoweredOff
      PoweringOn,         // Power on modem, then => PoweredOn
      Identify,           // Identify the modem model
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
      } modem_state1_t;
    typedef enum
      {
      SETSTATE = UART_EVENT_MAX+1000,
      TICKER1,
      SHUTDOWN
      } event_type_t;
    typedef enum
      {
      Unknown = 0,
      NotRegistered = 1,
      DeniedRegistration = 2,
      Searching = 3,
      Registered = 4,
      RegisteredEmergencyServices = 5,
      RegisteredRoamingSMS = 6,
      RegisteredRoaming = 7,
      RegisteredHomeSMS = 8,
      RegisteredHome = 9
      } network_registration_t;
    typedef enum
      {
      NRT_GSM = 0,
      NRT_GPRS = 1,
      NRT_EPS = 2
      } network_regtype_t;
    typedef enum
      {
      GUM_DEFAULT = 0,          // no user GPS mode overlay, apply defaults
      GUM_STOP = 1,             // user set GPS mode to stopped
      GUM_START = 2             // user set GPS mode to started
      } gps_usermode_t;
    typedef struct
      {
      event_type_t type;
      union
        {
        modem_state1_t newstate; // For SETSTATE
        } data;
      } modem_event_t;
    typedef union
      {
      uart_event_t uart;
      modem_event_t event;
      } modem_or_uart_event_t;

  public:
    OvmsBuffer             m_buffer;

    modem_state1_t         m_state1;
    int                    m_state1_ticker;
    modem_state1_t         m_state1_timeout_goto;
    int                    m_state1_timeout_ticks;
    int                    m_state1_userdata;
    int                    m_line_unfinished;

    std::string            m_line_buffer;
    network_registration_t m_netreg;
    network_registration_t m_netreg_d[CELLULAR_NETREG_COUNT];
    std::string            m_provider;
    int                    m_sq;
    bool                   m_pincode_required;

    uart_port_t            m_uartnum;
    int                    m_err_uart_fifo_ovf;
    int                    m_err_uart_buffer_full;
    int                    m_err_uart_parity;
    int                    m_err_uart_frame;
    int                    m_err_driver_buffer_full;

    modemdriver*           m_driver;
    std::string            m_model;
    int                    m_mux_channels;
    int                    m_mux_channel_CTRL;
    int                    m_mux_channel_NMEA;
    int                    m_mux_channel_DATA;
    int                    m_mux_channel_POLL;
    int                    m_mux_channel_CMD;

    GsmMux*                m_mux;
    GsmPPPOS*              m_ppp;
    GsmNMEA*               m_nmea;

    bool                   m_gps_hot_start;

    bool                   m_gps_enabled;           // = config modem enable.gps
    gps_usermode_t         m_gps_usermode;          // manual GPS control status
    int                    m_gps_parkpause;         // = config modem gps.parkpause (seconds, 0=off)
    int                    m_gps_stopticker;        // Park pause countdown
    int                    m_gps_startticker;       // Re-activation countdown
    int                    m_gps_reactivate;        // = config modem gps.parkreactivate (minutes, 0=off)
    int                    m_gps_reactlock;         // = config modem gps.reactlock (minutes, default 5)
    bool                   m_gps_awake_start;       // start GPS when vehicle awakes
    OvmsMutex              m_gps_mutex;             // lock for start/stop NMEA

    OvmsMutex              m_cmd_mutex;             // lock for the CMD channel
    bool                   m_cmd_running;           // true = collect rx lines in m_cmd_output
    OvmsSemaphore          m_cmd_done;              // signals command termination
    std::string            m_cmd_output;

  public:
    // Modem power control and initialisation
    virtual void SetPowerMode(PowerMode powermode);
    void AutoInit();
    void Restart();
    void PowerOff();
    void PowerCycle();
    void PowerSleep(bool onoff);
    void SupportSummary(OvmsWriter* writer, bool debug=false);

  public:
    // Modem API functionality
    void tx(uint8_t* data, size_t size);
    void tx(const char* data, ssize_t size = -1);
    void muxtx(int channel, uint8_t* data, size_t size);
    void muxtx(int channel, const char* data, ssize_t size = -1);
    bool txcmd(const char* data, ssize_t size = -1);

  public:
    // Modem state diagram implementation
    void SetState1(modem_state1_t newstate);
    modem_state1_t GetState1();
    void State1Leave(modem_state1_t oldstate);
    void State1Enter(modem_state1_t newstate);
    modem_state1_t State1Activity();
    modem_state1_t State1Ticker1();
    bool StandardIncomingHandler(int channel, OvmsBuffer* buf);
    void StandardLineHandler(int channel, OvmsBuffer* buf, std::string line);
    bool IdentifyModel();

  public:
    // High level API functions
    bool ModemIsNetMode();
    void StartTask();
    void StopTask();
    bool StartNMEA();
    void StopNMEA();
    void StartMux();
    void StopMux();
    void StartPPP();
    void StopPPP();
    void SetCellularModemDriver(const char* ModelType);
    void Task();
    void Ticker(std::string event, void* data);
    void EventListener(std::string event, void* data);
    void ConfigChanged(std::string event, void *data);
    void IncomingMuxData(GsmMuxChannel* channel);
    void SendSetState1(modem_state1_t newstate);
    bool IsStarted();
    void SetNetworkRegistration(network_regtype_t regtype, network_registration_t netreg);
    void SetProvider(std::string provider);
    void SetSignalQuality(int newsq);
    void ClearNetMetrics();
    void UpdateNetMetrics();

  public:
    // Development assistance functions
    void DevelopmentHexDump(const char* prefix, const char* data, size_t length, size_t colsize=16);
  };

class modemdriver : public InternalRamAllocated
  {
  public:
    modemdriver();
    virtual ~modemdriver();

  public:
    virtual const char* GetModel();
    virtual const char* GetName();
    virtual void Restart();
    virtual void PowerOff();
    virtual void PowerCycle();
    virtual void PowerSleep(bool onoff);

    virtual int GetMuxChannels();
    virtual int GetMuxChannelCTRL();
    virtual int GetMuxChannelNMEA();
    virtual int GetMuxChannelDATA();
    virtual int GetMuxChannelPOLL();
    virtual int GetMuxChannelCMD();
    virtual void StartupNMEA();
    virtual void ShutdownNMEA();
    virtual void StatusPoller();

    virtual bool State1Leave(modem::modem_state1_t oldstate);
    virtual bool State1Enter(modem::modem_state1_t newstate);
    virtual modem::modem_state1_t State1Activity(modem::modem_state1_t curstate);
    virtual modem::modem_state1_t State1Ticker1(modem::modem_state1_t curstate);
    virtual std::string GetNetTypes();
    virtual bool SetNetworkType(std::string);

  protected:
    unsigned int m_pwridx;
    time_t m_t_pwrcycle;
    modem* m_modem;
    int m_statuspoller_step;
    std::string net_type;
  };

template<typename Type> modemdriver* CreateCellularModemDriver()
  {
  return new Type;
  }

class OvmsCellularModemFactory
  {
  public:
    OvmsCellularModemFactory();
    ~OvmsCellularModemFactory();

  public:
    typedef modemdriver* (*FactoryFuncPtr)();
    typedef struct
      {
      FactoryFuncPtr construct;
      const char* name;
      } modemdriver_t;
    typedef map<const char*, modemdriver_t, CmpStrOp> map_modemdriver_t;

    map_modemdriver_t m_drivermap;

  public:
    template<typename Type>
    short RegisterCellularModemDriver(const char* ModelType, const char* ModelName = "")
      {
      FactoryFuncPtr function = &CreateCellularModemDriver<Type>;
        m_drivermap.insert(std::make_pair(ModelType, (modemdriver_t){ function, ModelName }));
      return 0;
      };
    modemdriver* NewCellularModemDriver(const char* ModelType);
  };

extern OvmsCellularModemFactory MyCellularModemFactory;

#endif //#ifndef __OVMS_CELLULAR_H__
