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
#include "rt_sevcon_mon.h"

using namespace std;

#define SC_Gen4_4845          0x0712302d    // Twizy80
#define SC_Gen4_4827          0x0712301b    // Twizy45


// 
// SEVCON drive profile state:
// 

typedef union __attribute__ ((__packed__))
{
  uint32_t u32;
  struct
  {
    unsigned profile_user:7;          // CFG: user selected profile key: 0=Default, 1..99=Custom
    unsigned :1;                      // padding/reserved
    
    unsigned profile_cfgmode:7;       // CFG: profile key, base params were last loaded from
    unsigned :1;                      // padding/reserved
    
    unsigned v3tag:1;                 // OVMS v3 drivemode tag (for clients)
    unsigned unsaved:1;               // CFG: RAM profile changed & not yet saved to EEPROM
    unsigned applied:1;               // CFG: applyprofile success flag
    unsigned keystate:1;              // CFG: profile button push state
    unsigned type:4;                  // CFG: 0=Twizy80, 1=Twizy45, 2=SC80GB45 (SEVCON_T80+Gearbox_T45)
                                      //      0/1   = auto detect
                                      //      2     = config set xrt type SC80GB45
                                      //      3…15  = reserved
    
    unsigned :8;                      // padding/reserved
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


// 
// SevconClient: main class
// 

class OvmsVehicleRenaultTwizy;
class SevconJob;

class SevconClient : public InternalRamAllocated
{
  friend class OvmsVehicleRenaultTwizy;
  public:
    SevconClient(OvmsVehicleRenaultTwizy* twizy);
    ~SevconClient();
    static SevconClient* GetInstance(OvmsWriter* writer=NULL);
  
  public:
    // Utils:
    uint32_t GetDeviceError();
    static const string GetDeviceErrorName(uint32_t device_error);
    static const string GetResultString(CANopenResult_t res, uint32_t detail=0);
    static const string GetResultString(CANopenJob& job);
    CANopenResult_t CheckBus();
    void SendFaultAlert(uint16_t faultcode);
  
  public:
    // Framework interface:
    void EmcyListener(string event, void* data);
    void UnmountListener(string event, void* data);
    void SetStatus(bool car_awake);
    void Ticker1(uint32_t ticker);
  
  public:
    // Synchronous access:
    CANopenResult_t Read(CANopenJob& job, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize);
    CANopenResult_t Read(CANopenJob& job, uint16_t index, uint8_t subindex, uint32_t& var) {
      return Read(job, index, subindex, (uint8_t*)&var, 4);
    }
    CANopenResult_t Read(CANopenJob& job, uint16_t index, uint8_t subindex, int32_t& var) {
      return Read(job, index, subindex, (uint8_t*)&var, 4);
    }
    CANopenResult_t Read(CANopenJob& job, uint16_t index, uint8_t subindex, uint16_t& var) {
      return Read(job, index, subindex, (uint8_t*)&var, 2);
    }
    CANopenResult_t Read(CANopenJob& job, uint16_t index, uint8_t subindex, int16_t& var) {
      return Read(job, index, subindex, (uint8_t*)&var, 2);
    }
    CANopenResult_t Write(CANopenJob& job, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize);
    CANopenResult_t Write(CANopenJob& job, uint16_t index, uint8_t subindex, uint32_t value) {
      return Write(job, index, subindex, (uint8_t*)&value, 0);
    }
    CANopenResult_t RequestState(CANopenJob& job, CANopenNMTCommand_t command, bool wait_for_state);
    CANopenResult_t GetHeartbeat(CANopenJob& job, CANopenNMTState_t& var);
  
  public:
    // Asynchronous access:
    CANopenResult_t SendRead(uint16_t index, uint8_t subindex, uint32_t* value);
    CANopenResult_t SendRead(uint16_t index, uint8_t subindex, int32_t* value);
    CANopenResult_t SendRead(uint16_t index, uint8_t subindex, uint16_t* value);
    CANopenResult_t SendRead(uint16_t index, uint8_t subindex, int16_t* value);
    CANopenResult_t SendWrite(uint16_t index, uint8_t subindex, uint32_t* value);
    CANopenResult_t SendRequestState(CANopenNMTCommand_t command);
    static void SevconAsyncTaskEntry(void *pvParameters);
    void SevconAsyncTask();
  
  public:
    // State management:
    CANopenResult_t Login(bool on);
    CANopenResult_t CfgMode(CANopenJob& job, bool on);
  
  public:
    // Tuning:
    CANopenResult_t CfgMakePowermap();
    CANopenResult_t CfgReadMaxPower();
    CANopenResult_t CfgSpeed(int max_kph, int warn_kph);
    CANopenResult_t CfgPower(int trq_prc, int pwr_lo_prc, int pwr_hi_prc, int curr_prc);
    CANopenResult_t CfgTSMap(char map,
      int8_t t1_prc, int8_t t2_prc, int8_t t3_prc, int8_t t4_prc,
      int t1_spd, int t2_spd, int t3_spd, int t4_spd);
    CANopenResult_t CfgDrive(int max_prc, int autodrive_ref, int autodrive_minprc, bool async=false);
    CANopenResult_t CfgRecup(int neutral_prc, int brake_prc, int autorecup_ref, int autorecup_minprc);
    void CfgAutoPower();
    CANopenResult_t CfgRamps(int start_prm, int accel_prc, int decel_prc, int neutral_prc, int brake_prc);
    CANopenResult_t CfgRampLimits(int accel_prc, int decel_prc);
    CANopenResult_t CfgSmoothing(int prc);
    CANopenResult_t CfgBrakelight(int on_lev, int off_lev);

    uint8_t CalcProfileChecksum(cfg_profile& profile);
    bool GetParamProfile(uint8_t key, cfg_profile& profile);
    bool SetParamProfile(uint8_t key, cfg_profile& profile);
    CANopenResult_t CfgApplyProfile(uint8_t key);
    CANopenResult_t CfgSwitchProfile(uint8_t key);
    string FmtSwitchProfileResult(CANopenResult_t res);
    
    static int PrintProfile(int capacity, OvmsWriter* writer, cfg_profile& profile);
    
    void Kickdown(bool on);
    static void KickdownTimer(TimerHandle_t xTimer);

  public:
    // diagnostics / statistics:
    void AddFaultInfo(ostringstream& buf, uint16_t faultcode);
    CANopenResult_t QueryLogs(int verbosity, OvmsWriter* writer, int which, int start, int* totalcnt, int* sendcnt);
    CANopenResult_t ResetLogs(int which, int* retcnt);

  public:
    // Monitoring:
    void InitMonitoring();
    void QueryMonitoringData();
    void ProcessMonitoringData(CANopenJob &job);
    void SendMonitoringData();
  
  public:
    // Shell commands:
    static void shell_cfg_mode(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_read(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_write(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    
    static void shell_cfg_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_get(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_info(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_save(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_load(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    
    static void shell_cfg_drive(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_recup(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_ramps(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_ramplimits(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_smooth(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    
    static void shell_cfg_power(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_speed(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_tsmap(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_brakelight(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    
    static void shell_cfg_querylogs(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_cfg_clearlogs(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    
    static void shell_mon_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_mon_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_mon_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
  
  private:
    OvmsVehicleRenaultTwizy*  m_twizy;
    const int                 m_nodeid = 1;
    CANopenClient             m_sync;
    CANopenAsyncClient        m_async;
    TaskHandle_t              m_asynctask = 0;
    sc_mondata                m_mon = {};
    bool                      m_mon_enable = false;
    volatile FILE*            m_mon_file = NULL;
    OvmsMutex                 m_mon_mutex;
    
    uint32_t                  m_sevcon_type;
    cfg_drivemode             m_drivemode;
    cfg_profile               m_profile;
    bool                      m_cfgmode_request;
    
    QueueHandle_t             m_faultqueue;
    uint16_t                  m_lastfault;
    uint8_t                   m_buttoncnt;
    
    uint32_t                  twizy_max_rpm = 0;                  // CFG: max speed (RPM: 0..11000)
    uint32_t                  twizy_max_trq = 0;                  // CFG: max torque (mNm: 0..70125)
    uint32_t                  twizy_max_pwr_lo = 0;               // CFG: max power low speed (W: 0..17000)
    uint32_t                  twizy_max_pwr_hi = 0;               // CFG: max power high speed (W: 0..17000)
    uint32_t                  twizy_max_curr = 0;                 // CFG: max current level (%: 10..123)

    int                       twizy_autorecup_checkpoint = 0;     // change detection for autorecup function
    uint16_t                  twizy_autorecup_level = 1000;       // autorecup: current recup level (per mille)
    
    int                       twizy_autodrive_checkpoint = 0;     // change detection for autopower function
    uint16_t                  twizy_autodrive_level = 1000;       // autopower: current drive level (per mille)
    
    uint8_t                   twizy_lock_speed = 0;               // if Lock mode: fix speed to this (kph)
    uint32_t                  twizy_valet_odo = 0;                // if Valet mode: reduce speed if odometer > this
    
    uint32_t                  m_drive_level = 1000;               // current drive level [per mille]
    TimerHandle_t             m_kickdown_timer;
  
};

#define CtrlLoggedIn()        (StdMetrics.ms_v_env_ctrl_login->AsBool())
#define CtrlCfgMode()         (StdMetrics.ms_v_env_ctrl_config->AsBool())
#define CarLocked()           (StdMetrics.ms_v_env_locked->AsBool())
#define ValetMode()           (StdMetrics.ms_v_env_valet->AsBool())

#define SetCtrlLoggedIn(b)    (StdMetrics.ms_v_env_ctrl_login->SetValue(b))
#define SetCtrlCfgMode(b)     (StdMetrics.ms_v_env_ctrl_config->SetValue(b))
#define SetCarLocked(b)       (StdMetrics.ms_v_env_locked->SetValue(b))
#define SetValetMode(b)       (StdMetrics.ms_v_env_valet->SetValue(b))


// 
// SevconJob: SevconClient attached CANopenJob
// 

class SevconJob : public InternalRamAllocated
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
    CANopenResult_t Read(uint16_t index, uint8_t subindex, int32_t& var) {
      m_job.Init();
      return m_client->Read(m_job, index, subindex, var);
    }
    CANopenResult_t Read(uint16_t index, uint8_t subindex, uint16_t& var) {
      m_job.Init();
      return m_client->Read(m_job, index, subindex, var);
    }
    CANopenResult_t Read(uint16_t index, uint8_t subindex, int16_t& var) {
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
    CANopenResult_t Login(bool on) {
      return m_client->Login(on);
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
