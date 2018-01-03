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

#include "ovms_log.h"
static const char *TAG = "v-twizy";

#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"

#include "vehicle_renaulttwizy.h"

using namespace std;


struct sdo_buffer
{
  union
  {
    uint32_t num;
    char txt[128];
  };
  size_t size;
  char type;
  
  void parse(const char* arg)
  {
    size = 0;
    type = '?';
    char* endptr = NULL;
    
    if (arg[0]=='0' && arg[1]=='x') {
      // try hexadecimal:
      num = strtol(arg+2, &endptr, 16);
      if (*endptr == 0) {
        size = 0; // let SDO server figure out type&size
        type = 'H';
      }
    }
    if (type == '?') {
      // try decimal:
      num = strtol(arg, &endptr, 10);
      if (*endptr == 0) {
        size = 0; // let SDO server figure out type&size
        type = 'D';
      }
    }
    if (type == '?') {
      // string:
      strncpy(txt, arg, sizeof(txt)-1);
      txt[sizeof(txt)-1] = 0;
      size = strlen(txt);
      type = 'S';
    }
  }
  
  void print(OvmsWriter* writer)
  {
    // test for printable text:
    bool show_txt = true;
    int i;
    for (i=0; i < size && txt[i]; i++) {
      if (!(isprint(txt[i]) || isspace(txt[i]))) {
        show_txt = false;
        break;
      }
    }
    // terminate string:
    if (i == size)
      txt[i] = 0;
    
    if (show_txt)
      writer->printf("dec=%d hex=0x%0*x str='%s'", num, MIN(size,4)*2, num, txt);
    else
      writer->printf("dec=%d hex=0x%0*x", num, MIN(size,4)*2, num);
  }
};



// Shell command:
//    xrt cfg <pre|op>
void xrt_cfg_mode(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  
  SevconJob sc(twizy);
  bool on = (strcmp(cmd->GetName(), "pre") == 0);
  CANopenResult_t res = sc.CfgMode(on);
  
  if (res != COR_OK)
    writer->printf("CFG mode %s failed: %s\n", cmd->GetName(), sc.GetResultString().c_str());
  else
    writer->printf("CFG mode %s established\n", cmd->GetName());
}


// Shell command:
//    xrt cfg read <index_hex> <subindex_hex>
void xrt_cfg_read(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  
  // parse args:
  uint16_t index = strtol(argv[0], NULL, 16);
  uint8_t subindex = strtol(argv[1], NULL, 16);
  
  // execute:
  SevconJob sc(twizy);
  sdo_buffer readval;
  CANopenResult_t res = sc.Read(index, subindex, (uint8_t*)&readval, sizeof(readval)-1);
  readval.size = sc.m_job.sdo.xfersize;
  
  // output result:
  if (res != COR_OK)
  {
    writer->printf("Read 0x%04x.%02x failed: %s\n", index, subindex, sc.GetResultString().c_str());
  }
  else
  {
    writer->printf("Read 0x%04x.%02x: ", index, subindex);
    readval.print(writer);
    writer->puts("");
  }
}


// Shell command:
//    xrt cfg write[only] <index_hex> <subindex_hex> <value>
// <value>:
// - interpreted as hexadecimal if begins with "0x"
// - interpreted as decimal if only contains digits
// - else interpreted as string
void xrt_cfg_write(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  
  sdo_buffer readval, writeval;
  CANopenResult_t readres, writeres;
  
  // parse args:
  bool writeonly = (strcmp(cmd->GetName(), "writeonly") == 0);
  uint16_t index = strtol(argv[0], NULL, 16);
  uint8_t subindex = strtol(argv[1], NULL, 16);
  
  writeval.parse(argv[2]);
  switch (writeval.type)
  {
    case 'H':
      writer->printf("Write 0x%04x.%02x: hex=0x%08x => ", index, subindex, writeval.num);
      break;
    case 'D':
      writer->printf("Write 0x%04x.%02x: dec=%d => ", index, subindex, writeval.num);
      break;
    default:
      writer->printf("Write 0x%04x.%02x: str='%s' => ", index, subindex, writeval.txt);
      break;
  }
  
  // execute:
  SevconJob sc(twizy);
  if (!writeonly) {
    readres = sc.Read(index, subindex, (uint8_t*)&readval, sizeof(readval)-1);
    readval.size = sc.m_job.sdo.xfersize;
  }
  writeres = sc.Write(index, subindex, (uint8_t*)&writeval, writeval.size);
  
  // output result:
  if (writeres != COR_OK)
    writer->printf("ERROR: %s\n", sc.GetResultString().c_str());
  else if (writeonly)
    writer->puts("OK");
  else if (readres != COR_OK)
    writer->puts("OK\nOld value: unknown, read failed");
  else {
    writer->printf("OK\nOld value: ");
    readval.print(writer);
    writer->puts("");
  }
}



/**
 * SevconClient:
 */

SevconClient::SevconClient(OvmsVehicleRenaultTwizy* twizy)
  : m_sync(twizy->m_can1), m_async(twizy->m_can1)
{
  ESP_LOGI(TAG, "sevcon subsystem init");
  
  m_twizy = twizy;
  m_sevcon_type = 0;
  memset(&m_drivemode, 0, sizeof(m_drivemode));
  memset(&m_profile, 0, sizeof(m_profile));
  
  m_cfgmode_request = false;

  // register shell commands:
  
  OvmsCommand *cmd_cfg = m_twizy->cmd_xrt->RegisterCommand("cfg", "SEVCON tuning", NULL, "", 0, 0, true);
  
  cmd_cfg->RegisterCommand("pre", "Enter configuration mode (pre-operational)", xrt_cfg_mode, "", 0, 0, true);
  cmd_cfg->RegisterCommand("op", "Leave configuration mode (go operational)", xrt_cfg_mode, "", 0, 0, true);
  
  cmd_cfg->RegisterCommand("read", "Read register", xrt_cfg_read, "<index_hex> <subindex_hex>", 2, 2, true);
  cmd_cfg->RegisterCommand("write", "Read & write register", xrt_cfg_write, "<index_hex> <subindex_hex> <value>", 3, 3, true);
  cmd_cfg->RegisterCommand("writeonly", "Write register", xrt_cfg_write, "<index_hex> <subindex_hex> <value>", 3, 3, true);
  
  // TODO:
  
  // xrt cfg …
  //  power, speed, drive, recup, ramps, ramplimits, smooth, tsmap, brakelight
  //  info, load, save, get, set, reset
  //  clearlog, showlog [data]
  
  // lock, unlock, valet, unvalet, homelink
  
  // NICE2HAVE: xrt cfg …
  //  getdcf
  
  
  // fault listener:
  
  m_faultqueue = xQueueCreate(10, sizeof(uint16_t));
  m_lastfault = 0;
  m_buttoncnt = 0;
  
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "canopen.node.emcy", std::bind(&SevconClient::EmcyListener, this, _1, _2));
  
}

SevconClient::~SevconClient()
{
  if (m_faultqueue)
    vQueueDelete(m_faultqueue);
}


SevconJob::SevconJob(SevconClient* client)
{
  m_client = client;
}

SevconJob::SevconJob(OvmsVehicleRenaultTwizy* twizy)
{
  m_client = twizy->GetSevconClient();
}

SevconJob::~SevconJob()
{
}


uint32_t SevconClient::GetDeviceError()
{
  // get SEVCON specific error code from 0x5310.00:
  CANopenJob errjob;
  uint32_t device_error = 0;
  if (m_sync.ReadSDO(errjob, m_nodeid, 0x5310, 0x00, (uint8_t*)&device_error, 4) == COR_OK)
    return 0xde000000 | device_error;
  return CANopen_GeneralError;
}


CANopenResult_t SevconClient::CheckBus()
{
  // check for CAN write access:
  if (!m_twizy->twizy_flags.EnableWrite)
    return COR_ERR_NoCANWrite;
  
  // check component status (currently only SEVCON):
  if ((m_twizy->twizy_status & CAN_STATUS_KEYON) == 0)
    return COR_ERR_DeviceOffline;
  
  return COR_OK;
}


CANopenResult_t SevconClient::Read(CANopenJob& job, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize)
{
  CANopenResult_t res = CheckBus();
  if (res != COR_OK)
    return job.SetResult(res);
  res = m_sync.ReadSDO(job, m_nodeid, index, subindex, buf, bufsize);
  if (res != COR_OK && job.sdo.error == CANopen_GeneralError && index != 0x5310)
    job.sdo.error = GetDeviceError();
  if (res != COR_OK)
    ESP_LOGD(TAG, "Sevcon ReadSDO 0x%04x.%02x failed: %s", index, subindex, GetResultString(job).c_str());
  return res;
}

CANopenResult_t SevconClient::Write(CANopenJob& job, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize)
{
  CANopenResult_t res = CheckBus();
  if (res != COR_OK)
    return job.SetResult(res);
  res = m_sync.WriteSDO(job, m_nodeid, index, subindex, buf, bufsize);
  if (res != COR_OK && job.sdo.error == CANopen_GeneralError && index != 0x5310)
    job.sdo.error = GetDeviceError();
  if (res != COR_OK)
    ESP_LOGD(TAG, "Sevcon WriteSDO 0x%04x.%02x failed: %s", index, subindex, GetResultString(job).c_str());
  return res;
}

CANopenResult_t SevconClient::RequestState(CANopenJob& job, CANopenNMTCommand_t command, bool wait_for_state)
{
  CANopenResult_t res = CheckBus();
  if (res != COR_OK)
    return job.SetResult(res);
  res = m_sync.SendNMT(job, m_nodeid, command, wait_for_state);
  if (res != COR_OK)
    ESP_LOGD(TAG, "Sevcon state request %s failed: %s",
      CANopen::GetCommandName(command).c_str(), GetResultString(job).c_str());
  return res;
}

CANopenResult_t SevconClient::GetHeartbeat(CANopenJob& job, CANopenNMTState_t& var)
{
  CANopenResult_t res = CheckBus();
  if (res != COR_OK)
    return job.SetResult(res);
  res = m_sync.ReceiveHB(job, m_nodeid, &var);
  if (res != COR_OK)
    ESP_LOGD(TAG, "Sevcon get heartbeat failed: %s", GetResultString(job).c_str());
  return res;
}

CANopenResult_t SevconClient::SendWrite(uint16_t index, uint8_t subindex, uint32_t value)
{
  CANopenResult_t res = CheckBus();
  if (res != COR_OK)
    return res;
  return m_async.WriteSDO(m_nodeid, index, subindex, (uint8_t*)&value, 0);
}

CANopenResult_t SevconClient::SendRequestState(CANopenNMTCommand_t command)
{
  CANopenResult_t res = CheckBus();
  if (res != COR_OK)
    return res;
  return m_async.SendNMT(m_nodeid, command);
}


void SevconClient::ProcessAsyncResults()
{
  CANopenJob job;
  while (m_async.ReceiveDone(job, 0) != COR_ERR_QueueEmpty) {
    ESP_LOGD(TAG, "Sevcon async result for %s: %s",
      CANopen::GetJobName(job).c_str(), GetResultString(job).c_str());
  }
}


void SevconClient::SetStatus(bool car_awake)
{
  *StdMetrics.ms_v_env_ctrl_login = (bool) false;
  *StdMetrics.ms_v_env_ctrl_config = (bool) false;
  m_buttoncnt = 0;
}


void SevconClient::Ticker1(uint32_t ticker)
{
  // cleanup CANopen job result queue:
  ProcessAsyncResults();
  
  // Send fault alerts:
  uint16_t faultcode;
  while (xQueueReceive(m_faultqueue, &faultcode, 0) == pdTRUE) {
    SendFaultAlert(faultcode);
  }

  if (!m_twizy->twizy_flags.CarAwake || !m_twizy->twizy_flags.EnableWrite)
    return;
  
  // Login to SEVCON:
  if (!StdMetrics.ms_v_env_ctrl_login->AsBool()) {
    if (Login(true) != COR_OK)
      return;
  }

  // Check for 3 successive button presses in STOP mode => CFG RESET:
  if ((m_buttoncnt >= 5) && (!m_twizy->twizy_flags.DisableReset)) {
    // reset SEVCON profile:
    ESP_LOGW(TAG, "Sevcon: detected D/R button cfg reset request [TODO]");
    
//     memset(&twizy_cfg_profile, 0, sizeof(twizy_cfg_profile));
//     twizy_cfg.unsaved = (twizy_cfg.profile_user > 0);
//     vehicle_twizy_cfg_applyprofile(twizy_cfg.profile_user);
//     twizy_notify(SEND_ResetResult);

    // reset button cnt:
    m_buttoncnt = 0;
  }
  else if (m_buttoncnt > 0) {
    m_buttoncnt--;
  }
  

#ifdef TODO

  // Valet mode: lock speed if valet max odometer reached:
  if ((m_twizy->twizy_flags.ValetMode)
          && (!m_twizy->twizy_flags.CarLocked) && (twizy_odometer > twizy_valet_odo))
  {
    vehicle_twizy_cfg_restrict_cmd(FALSE, CMD_Lock, NULL);
  }

  // Auto drive & recuperation adjustment (if enabled):
  vehicle_twizy_cfg_autopower();

#endif

}



CANopenResult_t SevconClient::Login(bool on)
{
  ESP_LOGD(TAG, "Sevcon login request: %d", on);
  SevconJob sc(this);

  // get SEVCON type (Twizy 80/45):
  if (sc.Read(0x1018, 0x02, m_sevcon_type) != COR_OK)
    return COR_ERR_UnknownDevice;

  if (m_sevcon_type == SC_Gen4_4845)
    m_drivemode.type = 0; // Twizy80
  else if (m_sevcon_type == SC_Gen4_4827)
    m_drivemode.type = 1; // Twizy45
  else
    return COR_ERR_UnknownDevice;
  
  // check login level:
  uint32_t level;
  if (sc.Read(0x5000, 0x01, level) != COR_OK)
    return COR_ERR_LoginFailed;

  if (on && level != 4) {
    // login:
    sc.Write(0x5000, 0x03, 0);
    sc.Write(0x5000, 0x02, 0x4bdf);

    // check new level:
    sc.Read(0x5000, 0x01, level);
    if (level != 4)
      return COR_ERR_LoginFailed;
  }

  else if (!on && level != 0) {
    // logout:
    sc.Write(0x5000, 0x03, 0);
    sc.Write(0x5000, 0x02, 0);

    // check new level:
    sc.Read(0x5000, 0x01, level);
    if (level != 0)
      return COR_ERR_LoginFailed;
  }

  ESP_LOGD(TAG, "Sevcon login status: %d", on);
  *StdMetrics.ms_v_env_ctrl_login = (bool) on;
  return COR_OK;
}


CANopenResult_t SevconClient::CfgMode(CANopenJob& job, bool on)
{
  // Note: as waiting for the next heartbeat would take ~ 500 ms, so
  //  we read the state change result from 0x5110.00
  
  ESP_LOGD(TAG, "Sevcon cfgmode request: %d", on);
  uint32_t state = 0;
  m_cfgmode_request = on;
  
  if (!on)
  {
    // request operational state:
    if (RequestState(job, CONC_Start, false) != COR_OK)
      return COR_ERR_StateChangeFailed;
    
    // give controller some time…
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // check state:
    Read(job, 0x5110, 0x00, state);
    if (state != CONS_Operational)
      return COR_ERR_StateChangeFailed;
  }
  else
  {
    // Triple (redundancy) safety check:
    //   only allow preop mode attempt if...
    //    a) Twizy is not moving
    //    b) and in "N" mode
    //    c) and not in "GO" mode
    if ((m_twizy->twizy_speed != 0) || (m_twizy->twizy_status & (CAN_STATUS_MODE | CAN_STATUS_GO)))
      return COR_ERR_StateChangeFailed;

    // request pre-operational state:
    if (RequestState(job, CONC_PreOp, false) != COR_OK)
      return COR_ERR_StateChangeFailed;
    
    // give controller some time…
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // check state:
    Read(job, 0x5110, 0x00, state);
    if (state != CONS_PreOperational)
    {
      // clear preop request:
      CANopenJob job2;
      RequestState(job2, CONC_Start, false);
      return COR_ERR_StateChangeFailed;
    }
  }
  
  ESP_LOGD(TAG, "Sevcon cfgmode status: %d", on);
  *StdMetrics.ms_v_env_ctrl_config = (bool) on;
  return COR_OK;
}


