/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy SEVCON Gen4 access
 * 
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __rt_sevcon_h__
#define __rt_sevcon_h__

#include "canopen.h"

using namespace std;

#define SC_Gen4_4845          0x0712302d    // Twizy80
#define SC_Gen4_4827          0x0712301b    // Twizy45


// 
// SEVCON drive profile state:
// 

typedef union __attribute__ ((__packed__))
{
  uint8_t drivemode;
  struct {
    unsigned type:1;                  // CFG: 0=Twizy80, 1=Twizy45
    unsigned profile_user:2;          // CFG: user selected profile: 0=Default, 1..3=Custom
    unsigned profile_cfgmode:2;       // CFG: profile, cfgmode params were last loaded from
    unsigned unsaved:1;               // CFG: RAM profile changed & not yet saved to EEPROM
    unsigned keystate:1;              // CFG: key state change detection
    unsigned applied:1;               // CFG: applyprofile success flag
  };
} cfg_drivemode;


// 
// SEVCON macro configuration profile:
// 

struct __attribute__ ((__packed__)) cfg_profile
{
  uint8_t       checksum;
  uint8_t       speed, warn;
  uint8_t       torque, power_low, power_high;
  uint8_t       drive, neutral, brake;
  struct tsmap {
    uint8_t       spd1, spd2, spd3, spd4;
    uint8_t       prc1, prc2, prc3, prc4;
  }             tsmap[3]; // 0=D 1=N 2=B
  uint8_t       ramp_start, ramp_accel, ramp_decel, ramp_neutral, ramp_brake;
  uint8_t       smooth;
  uint8_t       brakelight_on, brakelight_off;
  uint8_t       ramplimit_accel, ramplimit_decel;
  uint8_t       autorecup_minprc;
  uint8_t       autorecup_ref;
  uint8_t       autodrive_minprc;
  uint8_t       autodrive_ref;
  uint8_t       current;
};

// Macros for converting profile values:
// shift values by 1 (-1.. => 0..) to map param range to uint8_t
#define cfgparam(NAME)  (((int)(m_profile.NAME))-1)
#define cfgvalue(VAL)   ((uint8_t)((VAL)+1))


class OvmsVehicleRenaultTwizy;
class SevconJob;

class SevconClient
{
  friend class OvmsVehicleRenaultTwizy;
  public:
    SevconClient(OvmsVehicleRenaultTwizy* twizy);
    ~SevconClient();
  
  public:
    // Utils:
    uint32_t GetDeviceError();
    static const string GetDeviceErrorName(uint32_t device_error);
    static const string GetResultString(CANopenResult_t res, uint32_t detail);
    static const string GetResultString(CANopenJob& job);
    CANopenResult_t CheckBus();
    void SendFaultAlert(uint16_t faultcode);
  
  public:
    // Framework interface:
    void EmcyListener(string event, void* data);
    void SetStatus(bool car_awake);
    void Ticker1(uint32_t ticker);
  
  public:
    // Synchronous access:
    CANopenResult_t Read(CANopenJob& job, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize);
    CANopenResult_t Read(CANopenJob& job, uint16_t index, uint8_t subindex, uint32_t& var) {
      return Read(job, index, subindex, (uint8_t*)&var, 4);
    }
    CANopenResult_t Write(CANopenJob& job, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize);
    CANopenResult_t Write(CANopenJob& job, uint16_t index, uint8_t subindex, uint32_t value) {
      return Write(job, index, subindex, (uint8_t*)&value, 0);
    }
    CANopenResult_t RequestState(CANopenJob& job, CANopenNMTCommand_t command, bool wait_for_state);
    CANopenResult_t GetHeartbeat(CANopenJob& job, CANopenNMTState_t& var);
  
  public:
    // Asynchronous access:
    CANopenResult_t SendWrite(uint16_t index, uint8_t subindex, uint32_t value);
    CANopenResult_t SendRequestState(CANopenNMTCommand_t command);
    void ProcessAsyncResults();
  
  public:
    // State management:
    CANopenResult_t Login(bool on);
    CANopenResult_t CfgMode(CANopenJob& job, bool on);
  
  private:
    OvmsVehicleRenaultTwizy*  m_twizy;
    const int                 m_nodeid = 1;
    CANopenClient             m_sync;
    CANopenAsyncClient        m_async;
    
    uint32_t                  m_sevcon_type;
    cfg_drivemode             m_drivemode;
    cfg_profile               m_profile;
    bool                      m_cfgmode_request;
    
    QueueHandle_t             m_faultqueue;
    uint16_t                  m_lastfault;
    uint8_t                   m_buttoncnt;
  
};

class SevconJob
{
  public:
    SevconJob(SevconClient* client);
    SevconJob(OvmsVehicleRenaultTwizy* twizy);
    ~SevconJob();
  
  public:
    // Utils:
    const string GetResultString() {
      return SevconClient::GetResultString(m_job);
    }

  public:
    // Synchronous access:
    CANopenResult_t Read(uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize) {
      m_job.Init();
      return m_client->Read(m_job, index, subindex, buf, bufsize);
    }
    CANopenResult_t Read(uint16_t index, uint8_t subindex, uint32_t& var) {
      m_job.Init();
      return m_client->Read(m_job, index, subindex, var);
    }
    CANopenResult_t Write(uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize) {
      m_job.Init();
      return m_client->Write(m_job, index, subindex, buf, bufsize);
    }
    CANopenResult_t Write(uint16_t index, uint8_t subindex, uint32_t value) {
      m_job.Init();
      return m_client->Write(m_job, index, subindex, value);
    }
    CANopenResult_t RequestState(CANopenNMTCommand_t command, bool wait_for_state) {
      m_job.Init();
      return m_client->RequestState(m_job, command, wait_for_state);
    }
    CANopenResult_t GetHeartbeat(CANopenNMTState_t& var) {
      m_job.Init();
      return m_client->GetHeartbeat(m_job, var);
    }
    CANopenResult_t CfgMode(bool on) {
      m_job.Init();
      return m_client->CfgMode(m_job, on);
    }
  
  public:
    SevconClient*             m_client;
    CANopenJob                m_job;

};


#endif // __rt_sevcon_h__
