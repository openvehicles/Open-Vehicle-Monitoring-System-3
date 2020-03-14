/*
;    Project:       Open Vehicle Monitor System
;    Date:          9th March 2020
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
static const char *TAG = "ovms-net";

#include "ovms.h"
#include "ovms_netconns.h"

////////////////////////////////////////////////////////////////////////////////
// OvmsMongooseWrapper
////////////////////////////////////////////////////////////////////////////////

static void OvmsMongooseWrapperCallback(struct mg_connection *nc, int ev, void *ev_data)
  {
  OvmsMongooseWrapper* me = (OvmsMongooseWrapper*)nc->user_data;

  if (me != NULL) me->Mongoose(nc, ev, ev_data);
  }

OvmsMongooseWrapper::OvmsMongooseWrapper()
  {
  }

OvmsMongooseWrapper::~OvmsMongooseWrapper()
  {
  }

void OvmsMongooseWrapper::Mongoose(struct mg_connection *nc, int ev, void *ev_data)
  {
  return;
  }

////////////////////////////////////////////////////////////////////////////////
// OvmsNetTcpClient
////////////////////////////////////////////////////////////////////////////////

OvmsNetTcpClient::OvmsNetTcpClient()
  {
  m_mgconn = NULL;
  m_netstate = NetConnIdle;
  }

OvmsNetTcpClient::~OvmsNetTcpClient()
  {
  if (m_mgconn)
    {
    m_mgconn->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_mgconn = NULL;
    m_netstate = NetConnIdle;
    }
  }

void OvmsNetTcpClient::Mongoose(struct mg_connection *nc, int ev, void *ev_data)
  {
  switch (ev)
    {
    case MG_EV_CONNECT:
      {
      int *success = (int*)ev_data;
      if (*success == 0)
        {
        // Successful connection
        ESP_LOGD(TAG, "OvmsNetTcpClient Connection successful");
        m_netstate = NetConnConnected;
        Connected();
        }
      else
        {
        // Connection failed
        ESP_LOGD(TAG, "OvmsNetTcpClient Connection failed");
        m_netstate = NetConnFailed;
        m_mgconn = NULL;
        ConnectionFailed();
        }
      }
      break;
    case MG_EV_CLOSE:
      ESP_LOGD(TAG, "OvmsNetTcpClient Connection closed");
      m_netstate = NetConnDisconnected;
      m_mgconn = NULL;
      ConnectionClosed();
      break;
    case MG_EV_RECV:
      {
      ESP_LOGD(TAG, "OvmsNetTcpClient Incoming data (%d bytes)", nc->recv_mbuf.len);
      size_t removed = IncomingData(nc->recv_mbuf.buf, nc->recv_mbuf.len);
      if (removed > 0)
        {
        OvmsMutexLock mg(&m_mgconn_mutex);
        mbuf_remove(&nc->recv_mbuf, removed);
        }
      }
      break;
    default:
      break;
    }
  }

bool OvmsNetTcpClient::Connect(std::string dest, struct mg_connect_opts opts)
  {
  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  if (mgr == NULL) return false;

  OvmsMutexLock mg(&m_mgconn_mutex);
  m_dest = dest;
  if ((m_mgconn = mg_connect_opt(mgr, dest.c_str(), OvmsMongooseWrapperCallback, opts)) == NULL)
    {
    return false;
    }
  m_mgconn->user_data = this;
  m_netstate = NetConnConnecting;
  return true;
  }

void OvmsNetTcpClient::Disconnect()
  {
  if (m_mgconn)
    {
    m_mgconn->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_mgconn = NULL;
    m_netstate = NetConnIdle;
    }
  }

bool OvmsNetTcpClient::IsConnected()
  {
  return (m_netstate == NetConnConnected);
  }

size_t OvmsNetTcpClient::SendData(uint8_t *data, size_t length)
  {
  ESP_LOGD(TAG, "OvmsNetTcpClient Send data (%d bytes)", length);
  if (m_mgconn != NULL)
    {
    OvmsMutexLock mg(&m_mgconn_mutex);
    mg_send(m_mgconn, data, length);
    return length;
    }
  else
    return 0;
  }

void OvmsNetTcpClient::Connected()
  {
  }

void OvmsNetTcpClient::ConnectionFailed()
  {
  }

void OvmsNetTcpClient::ConnectionClosed()
  {
  }

size_t OvmsNetTcpClient::IncomingData(void *data, size_t length)
  {
  return length;
  }
