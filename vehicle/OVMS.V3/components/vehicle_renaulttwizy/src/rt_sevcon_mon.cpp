/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy SEVCON Gen4 access: monitoring
 * 
 * (c) 2018  Michael Balzer <dexter@dexters-web.de>
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

// #include "ovms_log.h"
// static const char *TAG = "v-twizy";

#include <stdio.h>
#include <math.h>
#include <cmath>
#include <string>
#include <iomanip>

#include "crypt_base64.h"
#include "ovms_notify.h"

#include "vehicle_renaulttwizy.h"
#include "rt_sevcon.h"


/**
 * Shell command:
 *  - xrt mon start <filename>
 */
void SevconClient::shell_mon_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  SevconClient* me = GetInstance();
  FILE* fp = (FILE*)me->m_mon_file;

  if (fp) {
    OvmsMutexLock lock(&me->m_mon_mutex);
    me->m_mon_enable = false;
    me->m_mon_file = NULL;
    fclose(fp);
    writer->puts("Recording stopped.");
  }
  
  if (argc == 1) {
    if (MyConfig.ProtectedPath(argv[0])) {
      writer->printf("Error: protected path '%s'!\n", argv[0]);
      return;
    }
    if ((me->m_mon_file = fopen(argv[0], "a")) == NULL) {
      writer->printf("Error: can't open '%s'!\n", argv[0]);
      return;
    } else {
      // write CSV header:
      fputs(
        "timestamp,kph,rpm,throttle,kickdown,brake,"
        "mot_torque_limit,mot_torque_demand,mot_torque,"
        "bat_voltage,bat_current,bat_power,cap_voltage,"
        "mot_voltage,mot_current,mot_power,mot_voltmod,mot_slipfreq,mot_outputfreq\n",
        (FILE*)me->m_mon_file);
      me->m_mon_enable = true;
      writer->printf("Recording started to '%s'.\n", argv[0]);
    }
  } else {
    if (me->m_mon_enable) {
      writer->puts("Monitoring already running.");
    } else {
      me->m_mon_enable = true;
      writer->puts("Monitoring started.");
    }
  }
}


/**
 * Shell command:
 *  - xrt mon stop
 */
void SevconClient::shell_mon_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  SevconClient* me = GetInstance();

  if (!me->m_mon_enable) {
    writer->puts("Monitoring not started.");
    return;
  }

  FILE* fp = (FILE*)me->m_mon_file;
  me->m_mon_enable = false;

  if (fp) {
    OvmsMutexLock lock(&me->m_mon_mutex);
    me->m_mon_file = NULL;
    fclose(fp);
    writer->puts("Recording stopped.");
  }
  
  me->SendMonitoringData();
  writer->puts("Monitoring stopped.");
}


/**
 * Shell command:
 *  - xrt mon reset
 */
void SevconClient::shell_mon_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  SevconClient* me = GetInstance();
  if (me->m_mon_enable) {
    me->SendMonitoringData();
  }
  me->m_mon.reset_speedmaps();
  writer->puts("Reset done.");
}


/**
 * Reset speed maps
 */
void sc_mondata::reset_speedmaps()
{
  for (int i=0; i<SCMON_MAX_KPH; i++) {
    bat_power_drv[i] = 0;
    bat_power_rec[i] = 0;
    mot_torque_drv[i] = 0;
    mot_torque_rec[i] = 0;
  }
  m_bat_power_drv->ClearValue();
  m_bat_power_rec->ClearValue();
  m_mot_torque_drv->ClearValue();
  m_mot_torque_rec->ClearValue();
}


/**
 * SendMonitoringData:
 */
void SevconClient::SendMonitoringData()
{
  std::string msg;
  
  msg = "RT-ENG-BatPwrDrv,0,86400,";
  msg.append(m_mon.m_bat_power_drv->AsString());
  MyNotify.NotifyString("data", "xrt.pwr.mon", msg.c_str());
  
  msg = "RT-ENG-BatPwrRec,0,86400,";
  msg.append(m_mon.m_bat_power_rec->AsString());
  MyNotify.NotifyString("data", "xrt.pwr.mon", msg.c_str());
  
  msg = "RT-ENG-MotTrqDrv,0,86400,";
  msg.append(m_mon.m_mot_torque_drv->AsString());
  MyNotify.NotifyString("data", "xrt.pwr.mon", msg.c_str());
  
  msg = "RT-ENG-MotTrqRec,0,86400,";
  msg.append(m_mon.m_mot_torque_rec->AsString());
  MyNotify.NotifyString("data", "xrt.pwr.mon", msg.c_str());
}


/**
 * InitMonitoringMetrics:
 */
void SevconClient::InitMonitoring()
{
  // .i. = Inverter:
  
  m_mon.mot_current         = MyMetrics.InitFloat("xrt.i.cur.act",      SM_STALE_MIN, 0, Amps);
  m_mon.mot_voltage         = MyMetrics.InitFloat("xrt.i.vlt.act",      SM_STALE_MIN, 0, Volts);
  m_mon.mot_power           = MyMetrics.InitFloat("xrt.i.pwr.act",      SM_STALE_MIN, 0, kW);
  m_mon.mot_voltmod         = MyMetrics.InitFloat("xrt.i.vlt.mod",      SM_STALE_MIN, 0, Percentage);

  m_mon.mot_slipfreq        = MyMetrics.InitFloat("xrt.i.frq.slip",     SM_STALE_MIN, 0, Other);
  m_mon.mot_outputfreq      = MyMetrics.InitFloat("xrt.i.frq.output",   SM_STALE_MIN, 0, Other);

  m_mon.mot_torque_demand   = MyMetrics.InitFloat("xrt.i.trq.demand",   SM_STALE_MIN, 0, Other);
  m_mon.mot_torque          = MyMetrics.InitFloat("xrt.i.trq.act",      SM_STALE_MIN, 0, Other);
  m_mon.mot_torque_limit    = MyMetrics.InitFloat("xrt.i.trq.limit",    SM_STALE_MIN, 0, Other);

  m_mon.bat_voltage         = MyMetrics.InitFloat("xrt.i.vlt.bat",      SM_STALE_MIN, 0, Other);
  m_mon.cap_voltage         = MyMetrics.InitFloat("xrt.i.vlt.cap",      SM_STALE_MIN, 0, Amps);
  
  // .s. = Statistics:
  
  m_mon.m_bat_power_drv     = new OvmsMetricVector<float>("xrt.s.b.pwr.drv", SM_STALE_HIGH, kW);
  m_mon.m_bat_power_rec     = new OvmsMetricVector<float>("xrt.s.b.pwr.rec", SM_STALE_HIGH, kW);

  m_mon.m_mot_torque_drv    = new OvmsMetricVector<float>("xrt.s.m.trq.drv", SM_STALE_HIGH, Nm);
  m_mon.m_mot_torque_rec    = new OvmsMetricVector<float>("xrt.s.m.trq.rec", SM_STALE_HIGH, Nm);
}


/**
 * QueryMonitoringData: request monitored SDOs from SEVCON
 *  - called by IncomingFrameCan1 (triggered by PDO 0x629, 100ms interval)
 *  - running in vehicle task context
 * Results are processed by ProcessMonitoringData()
 */
void SevconClient::QueryMonitoringData()
{
  if (!m_mon_enable || m_cfgmode_request || CtrlCfgMode() || !CtrlLoggedIn() || StdMetrics.ms_v_env_gear->AsInt()==0)
    return;
  
  // 4600.0c Actual AC Motor Current [A]
  SendRead(0x4600, 0x0c, &m_mon.mot_current_raw);
  // 4600.0d Actual AC Motor Voltage [1/16 V]
  SendRead(0x4600, 0x0d, &m_mon.mot_voltage_raw);
  // 4600.0b Voltage modulation [100/255 %]
  SendRead(0x4600, 0x0b, &m_mon.mot_voltmod_raw);

  // 4600.01 Slip Frequency [1/256 rad/s]
  SendRead(0x4600, 0x01, &m_mon.mot_slipfreq_raw);
  // 4600.0f Electrical output frequency [1/16 rad/s]
  SendRead(0x4600, 0x0f, &m_mon.mot_outputfreq_raw);

  // 4602.0b Torque demand value (U.T_d) [1/16 Nm]
  SendRead(0x4602, 0x0b, &m_mon.mot_torque_demand_raw);
  // 4602.0c Torque actual value (DWork.Td) [1/16 Nm]
  SendRead(0x4602, 0x0c, &m_mon.mot_torque_raw);
  // 4602.0e Maximum power limit torque (Y.T_power_limit) [1/16 Nm]
  SendRead(0x4602, 0x0e, &m_mon.mot_torque_limit_raw);

  // 4602.11 Battery Voltage [1/16 V]
  SendRead(0x4602, 0x11, &m_mon.bat_voltage_raw);
  // 4602.12 Capacitor Voltage [1/16 V]
  SendRead(0x4602, 0x12, &m_mon.cap_voltage_raw);
}


/**
 * ProcessMonitoringData: process async read results
 *  - called by SevconAsyncTask
 *  - running in m_asynctask context
 */
void SevconClient::ProcessMonitoringData(CANopenJob &job)
{
  if (job.type != COJT_ReadSDO || job.result != COR_OK)
    return;

  uint32_t addr = (uint32_t)job.sdo.index << 8 | job.sdo.subindex;
  bool complete = false;
  switch (addr) {
    // 4600.0c Actual AC Motor Current [A]
    case 0x46000c:
      m_mon.mot_current->SetValue(m_mon.mot_current_raw);
      break;
    // 4600.0d Actual AC Motor Voltage [1/16 V]
    case 0x46000d:
      m_mon.mot_voltage->SetValue(m_mon.mot_voltage_raw / 16.0f);
      m_mon.mot_power->SetValue(m_mon.mot_voltage_raw / 16.0f * m_mon.mot_current->AsFloat() / 1000.0f);
      break;
    // 4600.0b Voltage modulation [100/255 %]
    case 0x46000b:
      m_mon.mot_voltmod->SetValue(m_mon.mot_voltmod_raw * 100.0f / 255.0f);
      break;

    // 4600.01 Slip Frequency [1/256 rad/s]
    case 0x460001:
      m_mon.mot_slipfreq->SetValue(m_mon.mot_slipfreq_raw / 256.0f);
      break;
    // 4600.0f Electrical output frequency [1/16 rad/s]
    case 0x46000f:
      m_mon.mot_outputfreq->SetValue(m_mon.mot_outputfreq_raw / 16.0f);
      break;

    // 4602.0b Torque demand value (U.T_d) [1/16 Nm]
    case 0x46020b:
      m_mon.mot_torque_demand->SetValue(m_mon.mot_torque_demand_raw / 16.0f);
      break;
    // 4602.0c Torque actual value (DWork.Td) [1/16 Nm]
    case 0x46020c:
      m_mon.mot_torque->SetValue(m_mon.mot_torque_raw / 16.0f);
      break;
    // 4602.0e Maximum power limit torque (Y.T_power_limit) [1/16 Nm]
    case 0x46020e:
      m_mon.mot_torque_limit->SetValue(m_mon.mot_torque_limit_raw / 16.0f);
      break;

    // 4602.11 Battery Voltage [1/16 V]
    case 0x460211:
      m_mon.bat_voltage->SetValue(m_mon.bat_voltage_raw / 16.0f);
      break;
    // 4602.12 Capacitor Voltage [1/16 V]
    case 0x460212:
      m_mon.cap_voltage->SetValue(m_mon.cap_voltage_raw / 16.0f);
      complete = true; // last in series
      break;

    default:
      break;
  }

  if (complete) {
    
    // update speed maps:
    
    float spd = StdMetrics.ms_v_pos_speed->AsFloat();
    float batpwr = m_mon.bat_voltage->AsFloat() * StdMetrics.ms_v_bat_current->AsFloat();
    float mottrq = m_mon.mot_torque->AsFloat();
    
    int si = (int) spd;
    if (si < SCMON_MAX_KPH) {
      if (batpwr > 0) {
        // drive:
        if (m_mon.bat_power_drv[si] < (int16_t)batpwr) {
          m_mon.bat_power_drv[si] = (int16_t)batpwr;
          m_mon.m_bat_power_drv->SetElemValue(si, ((int16_t)batpwr) / 1000.0f);
        }
        if (m_mon.mot_torque_drv[si] < mottrq) {
          m_mon.mot_torque_drv[si] = mottrq;
          m_mon.m_mot_torque_drv->SetElemValue(si, mottrq);
        }
      } else {
        // recup:
        if (m_mon.bat_power_rec[si] < -(int16_t)batpwr) {
          m_mon.bat_power_rec[si] = -(int16_t)batpwr;
          m_mon.m_bat_power_rec->SetElemValue(si, (-(int16_t)batpwr) / 1000.0f);
        }
        if (m_mon.mot_torque_rec[si] < -mottrq) {
          m_mon.mot_torque_rec[si] = -mottrq;
          m_mon.m_mot_torque_rec->SetElemValue(si, -mottrq);
        }
      }
    }
    
    // write log:
    if (m_mon_file) {
      OvmsMutexLock lock(&m_mon_mutex);
      if (m_mon_file) {
        fprintf((FILE*)m_mon_file,
          // timestamp,kph,rpm,throttle,kickdown,brake,
          "%u,%.1f,%d,%.0f,%u,%.0f,"
          // mot_torque_limit,mot_torque_demand,mot_torque,
          "%.1f,%.1f,%.1f,"
          // bat_voltage,bat_current,bat_power,cap_voltage,
          "%.1f,%.2f,%.1f,%.1f,"
          // mot_voltage,mot_current,mot_power,mot_voltmod,mot_slipfreq,mot_outputfreq
          "%.1f,%.0f,%.1f,%.1f,%.1f,%.1f\n",
          
          esp_log_timestamp(),
          spd,
          StdMetrics.ms_v_mot_rpm->AsInt(),
          StdMetrics.ms_v_env_throttle->AsFloat(),
          m_twizy->twizy_kickdown_hold,
          StdMetrics.ms_v_env_footbrake->AsFloat(),
          
          m_mon.mot_torque_limit->AsFloat(),
          m_mon.mot_torque_demand->AsFloat(),
          mottrq,
          
          m_mon.bat_voltage->AsFloat(),
          StdMetrics.ms_v_bat_current->AsFloat(),
          batpwr,
          m_mon.cap_voltage->AsFloat(),
          
          m_mon.mot_voltage->AsFloat(),
          m_mon.mot_current->AsFloat(),
          m_mon.mot_power->AsFloat(),
          m_mon.mot_voltmod->AsFloat(),
          m_mon.mot_slipfreq->AsFloat(),
          m_mon.mot_outputfreq->AsFloat());
      }
    }
  }
}
