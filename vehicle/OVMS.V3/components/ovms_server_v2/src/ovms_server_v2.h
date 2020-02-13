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

#ifndef __OVMS_SERVER_V2_H__
#define __OVMS_SERVER_V2_H__

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <sys/time.h>
#include "ovms_server.h"
#include "ovms_netmanager.h"
#include "ovms_buffer.h"
#include "crypt_rc4.h"
#include "ovms_metrics.h"
#include "ovms_notify.h"
#include "ovms_mutex.h"

#define OVMS_PROTOCOL_V2_TOKENSIZE 22

class OvmsServerV2 : public OvmsServer
  {
  public:
    OvmsServerV2(const char* name);
    ~OvmsServerV2();

  public:
    virtual void SetPowerMode(PowerMode powermode);
    void Connect();
    void SendLogin(struct mg_connection *nc);
    void Disconnect();
    void Reconnect(int connretry);
    size_t IncomingData(uint8_t* data, size_t len);

  protected:
    void ProcessServerMsg();
    void ProcessCommand(const char* payload);
    void Transmit(const std::string& message);
    void Transmit(const char* message);

  protected:
    void TransmitMsgStat(bool always = false);
    void TransmitMsgGPS(bool always = false);
    void TransmitMsgTPMS(bool always = false);
    void TransmitMsgFirmware(bool always = false);
    void TransmitMsgEnvironment(bool always = false);
    void TransmitMsgCapabilities(bool always = false);
    void TransmitMsgGroup(bool always = false);

  protected:
    void TransmitNotifyInfo();
    void TransmitNotifyError();
    void TransmitNotifyAlert();
    void TransmitNotifyData();
    void HandleNotifyDataAck(uint32_t ack);

  public:
    void MetricModified(OvmsMetric* metric);
    bool NotificationFilter(OvmsNotifyType* type, const char* subtype);
    bool IncomingNotification(OvmsNotifyType* type, OvmsNotifyEntry* entry);
    void EventListener(std::string event, void* data);
    void ConfigChanged(OvmsConfigParam* param);
    void NetUp(std::string event, void* data);
    void NetDown(std::string event, void* data);
    void NetReconfigured(std::string event, void* data);
    void NetmanInit(std::string event, void* data);
    void NetmanStop(std::string event, void* data);
    void Ticker1(std::string event, void* data);

  public:
    enum State
      {
      Undefined = 0,
      WaitNetwork,
      ConnectWait,
      Connecting,
      Authenticating,
      Connected,
      Disconnected,
      WaitReconnect
      };
    State m_state;
    std::string m_status;
    void SetStatus(const char* status, bool fault=false, State newstate=Undefined);

  public:
    struct mg_connection *m_mgconn;
    OvmsMutex m_mgconn_mutex;
    int m_connretry;
    bool m_loggedin;

  protected:
    metric_unit_t m_units_distance;
    OvmsBuffer* m_buffer;
    std::string m_vehicleid;
    std::string m_server;
    std::string m_password;
    std::string m_port;
    bool m_tls;

    std::string m_token;
    RC4_CTX1 m_crypto_rx1;
    RC4_CTX2 m_crypto_rx2;
    RC4_CTX1 m_crypto_tx1;
    RC4_CTX2 m_crypto_tx2;

    bool m_now_stat;
    bool m_now_gps;
    bool m_now_tpms;
    bool m_now_firmware;
    bool m_now_environment;
    bool m_now_capabilities;
    bool m_now_group;

    int m_streaming;
    int m_updatetime_idle;
    int m_updatetime_connected;

    uint32_t m_lastrx_time = 0;
    int m_lasttx = 0;
    int m_lasttx_stream = 0;
    int m_peers = 0;

    bool m_pending_notify_info;
    bool m_pending_notify_error;
    bool m_pending_notify_alert;
    bool m_pending_notify_data;
    uint32_t m_pending_notify_data_last;
    int m_pending_notify_data_retransmit;
  };

class OvmsServerV2Init
  {
  public:
    OvmsServerV2Init();
    void AutoInit();
  };

extern OvmsServerV2Init MyOvmsServerV2Init;
extern OvmsServerV2 *MyOvmsServerV2;

#endif //#ifndef __OVMS_SERVER_V2_H__
