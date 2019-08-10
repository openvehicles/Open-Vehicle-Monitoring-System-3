/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy: low level CAN communication
 * 
 * (c) 2019  Michael Balzer <dexter@dexters-web.de>
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
#include <iomanip>

#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_utils.h"

#include "vehicle_renaulttwizy.h"
#include "rt_obd2.h"

using namespace std;

#define CLUSTER_TXID              0x743
#define CLUSTER_RXID              0x763
#define CLUSTER_PID_VIN           0x81
#define CLUSTER_PID_DTC           0x13

const OvmsVehicle::poll_pid_t twizy_poll_default[] = {
  // Note: poller ticker cycles at 3600 seconds = max period
  // { txid, rxid, type, pid, { period_off, period_drive, period_charge } }
  { CLUSTER_TXID, CLUSTER_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, CLUSTER_PID_VIN, { 0, 3600, 3600 } },
  { CLUSTER_TXID, CLUSTER_RXID, VEHICLE_POLL_TYPE_OBDIIGROUP, CLUSTER_PID_DTC, { 0, 10, 60 } },
  { 0, 0, 0, 0, { 0, 0, 0 } }
};


void OvmsVehicleRenaultTwizy::ObdInit()
{
  // init poller:
  PollSetPidList(m_can1, twizy_poll_default);
  PollSetState(0);
  
  // init commands:
  OvmsCommand* cmd;
  cmd = cmd_xrt->RegisterCommand("dtc", "Show DTC report / clear DTC", shell_obd_showdtc);
  cmd->RegisterCommand("show", "Show DTC report", shell_obd_showdtc);
  cmd->RegisterCommand("clear", "Clear stored DTC in car", shell_obd_cleardtc,
    "Att: this clears the car DTC store if the car is turned on.\n"
    "Invoke with car turned off to just clear the OVMS DTC alert buffer.");
  cmd->RegisterCommand("reset", "Reset OVMS DTC statistics", shell_obd_resetdtc,
    "Resets internal DTC buffer and presence counters.\n"
    "No change is done on the car side.");

#if 0
  // Test:
  const uint8_t test[] = {
  0x04, 0x01, 0x26, 0x0A, 0x00, 0xC5, 0x06, 0xFF, 0x00, 0x5E, 0xFF,
  0x01, 0x25, 0x2A, 0x4A, 0x00, 0xC5, 0x06, 0xFF, 0x00, 0x5E, 0xFF,
  0x03, 0x3B, 0x29, 0x0B, 0x00, 0xC5, 0x53, 0xFF, 0x00, 0x30, 0x22,
  0x04, 0x28, 0x25, 0x4B, 0x00, 0xC5, 0x53, 0xFF, 0x00, 0x30, 0x22,
  0x04, 0x15, 0x2A, 0x4B, 0x00, 0xC7, 0x2B, 0xFF, 0x00, 0x2D, 0x1B,
  0x03, 0x3A, 0x29, 0x0B, 0x00, 0xCC, 0x1E, 0xFF, 0x00, 0x27, 0x39,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  memcpy(&twizy_cluster_dtc_new, test, sizeof twizy_cluster_dtc_new);
  twizy_cluster_dtc_updated = true;
  twizy_cluster_dtc_inhibit_alert = false;
#endif
}


void OvmsVehicleRenaultTwizy::ObdTicker1()
{
  int new_state;
  
  if (twizy_flags.Charging)
    new_state = 2;
  else if (twizy_flags.CarAwake)
    new_state = 1;
  else
    new_state = 0;
  
  // reset DTC presence count on drive/charge start:
  if (cfg_dtc_autoreset && new_state && new_state != m_poll_state)
    fill_n(twizy_cluster_dtc_present, DTC_COUNT, 0);
  
  PollSetState(new_state);
}


void OvmsVehicleRenaultTwizy::IncomingPollReply(
  canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
{
  static string rxbuf;

  // init / fill rx buffer:
  if (m_poll_ml_frame == 0) {
    rxbuf.clear();
    rxbuf.reserve(length + mlremain);
  }
  rxbuf.append((char*)data, length);
  if (mlremain)
    return;
  
  // complete:
  switch (pid) {

    // VIN:
    case CLUSTER_PID_VIN:
      // payload = 17 chars VIN + 2 bytes checksum
      ESP_LOGD(TAG, "OBD2: got VIN='%s'", rxbuf.substr(0,17).c_str());
      *StdMetrics.ms_v_vin = rxbuf.substr(0,17);
      break;

    // DTC:
    case CLUSTER_PID_DTC:
      if (rxbuf.size() != sizeof twizy_cluster_dtc_new)
        ESP_LOGW(TAG, "OBD2: DTC size mismatch: expected %d, got %d bytes", sizeof twizy_cluster_dtc_new, rxbuf.size());
      memcpy(&twizy_cluster_dtc_new, rxbuf.data(), MIN(rxbuf.size(), sizeof twizy_cluster_dtc_new));
      twizy_cluster_dtc_updated = true;
      // alert processing done by ticker
      break;

    // Unknown: output
    default: {
      char *buf = NULL;
      size_t rlen = rxbuf.size(), offset = 0;
      while (rlen) {
        rlen = FormatHexDump(&buf, rxbuf.data() + offset, rlen, 16);
        offset += 16;
        ESP_LOGW(TAG, "OBD2: unhandled reply: %s", buf);
      }
      if (buf)
        free(buf);
      break;
    }
  }
}


void OvmsVehicleRenaultTwizy::ResetDTCStats()
{
  twizy_cluster_dtc_updated = false;
  twizy_cluster_dtc = {};
  twizy_cluster_dtc_new = {};
  fill_n(twizy_cluster_dtc_present, DTC_COUNT, 0);
  twizy_cluster_dtc_inhibit_alert = false; // show all DTC on next query
}


void OvmsVehicleRenaultTwizy::FormatDTC(StringWriter& buf, int i, cluster_dtc& e)
{
  if (!e.EcuName && !e.NumDTC) {
    buf.printf("#%02d: -clear-", i+1);
  } else {
    buf.printf("#%02d: %d/%d rev%d",
               i+1, e.EcuName, e.NumDTC, e.Revision);
    if (twizy_cluster_dtc_present[i]) {
      buf.printf(" PRESENT(%dx)",
                 twizy_cluster_dtc_present[i]);
    }
    buf.printf("%s%s%s%s%s%s%s%s%s\n",
               e.G1 ? " G1" : "",
               e.G2 ? " G2" : "",
               e.ServKey ? " SERV" : "",
               e.Customer ? " INT" : "",
               e.Memorize ? " MEM" : "",
               e.Bt ? " Bt" : "",
               e.Ef ? " Ef" : "",
               e.Dc ? " Dc" : "",
               e.DNS ? " DNS" : "");
    buf.printf("   @ %dkm %dkph SOC=%d%% BV=%dV TC=%d IC=%d",
               e.getOdometer(), e.Speed, e.BL, e.getBV(),
               e.TimeCounter, e.IgnitionCycle);
  }
}


void OvmsVehicleRenaultTwizy::SendDTC(int i, cluster_dtc& e)
{
  // H type "RT-OBD-ClusterDTC",rec_nr
  //   ,EcuName,NumDTC,Revision,FailPresentCnt
  //   ,G1,G2,ServKey,Customer,Memorize,Bt,Ef,Dc,DNS
  //   ,Odometer,Speed,SOC,BattV,TimeCounter,IgnitionCycle
  ostringstream buf;
  buf
    << "RT-OBD-ClusterDTC," << i+1
    << "," << 86400*7
    << "," << (int) e.EcuName
    << "," << (int) e.NumDTC
    << "," << (int) e.Revision
    << "," << (int) twizy_cluster_dtc_present[i]
    << "," << (int) e.G1
    << "," << (int) e.G2
    << "," << (int) e.ServKey
    << "," << (int) e.Customer
    << "," << (int) e.Memorize
    << "," << (int) e.Bt
    << "," << (int) e.Ef
    << "," << (int) e.Dc
    << "," << (int) e.DNS
    << "," << (int) e.getOdometer()
    << "," << (int) e.Speed
    << "," << (int) e.BL
    << "," << (int) e.getBV()
    << "," << (int) e.TimeCounter
    << "," << (int) e.IgnitionCycle
    ;
  MyNotify.NotifyString("data", "xrt.obd.cluster.dtc", buf.str().c_str());
}


void OvmsVehicleRenaultTwizy::ObdTicker10()
{
  // Process DTC update:
  if (twizy_cluster_dtc_updated)
  {
    int cnt_stored = 0, cnt_present = 0;
    StringWriter dtcbuf, alertbuf;
    bool new_dtc, new_present;
    
    for (int i=0; i < DTC_COUNT; i++)
    {
      cluster_dtc& n = twizy_cluster_dtc_new.entry[i];
      cluster_dtc& e = twizy_cluster_dtc.entry[i];
      
      if (!n.EcuName && !n.NumDTC)
      {
        // clear slot:
        twizy_cluster_dtc_present[i] = 0;
      }
      else
      {
        cnt_stored++;
        
        // check for new DTC:
        new_dtc = (e.EcuName != n.EcuName || e.NumDTC != n.NumDTC);
        if (new_dtc) {
          // clear slot:
          twizy_cluster_dtc_present[i] = 0;
        }
        
        // check for new presence:
        new_present = (n.FailPresent && !twizy_cluster_dtc_present[i]);
        
        // count presence:
        if (n.FailPresent)
          twizy_cluster_dtc_present[i]++;
        if (twizy_cluster_dtc_present[i])
          cnt_present++;
        
        // present or new/changed DTC?
        if (n.FailPresent || n != e)
        {
          // …log:
          dtcbuf.clear();
          dtcbuf.append(new_dtc ? "NEW " : "UPD ");
          FormatDTC(dtcbuf, i, n);
          ESP_LOGW(TAG, "OBD2 Cluster DTC %s", dtcbuf.c_str());

          // …add to alert:
          if (!twizy_cluster_dtc_inhibit_alert || new_present) {
            alertbuf.append(dtcbuf);
            alertbuf.append("\n");
          }
          
          // …and send history record:
          SendDTC(i, n);
        }
      }
      
      // store DTC:
      e = n;
    }
    
    if (!alertbuf.empty())
    {
      MyNotify.NotifyStringf("alert", "vehicle.status.dtc", "DTC update (%d stored, %d present):\n%s",
                             cnt_stored, cnt_present, alertbuf.c_str());
    }
    
    twizy_cluster_dtc_updated = false;
    twizy_cluster_dtc_inhibit_alert = false;
  }
}


void OvmsVehicleRenaultTwizy::shell_obd_showdtc(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = GetInstance(writer);
  if (!twizy)
    return;

  int cnt_stored = 0, cnt_present = 0;
  StringWriter buf;

  for (int i=0; i < DTC_COUNT; i++)
  {
    cluster_dtc& e = twizy->twizy_cluster_dtc.entry[i];
    if (!e.EcuName && !e.NumDTC)
      continue;
    cnt_stored++;
    if (twizy->twizy_cluster_dtc_present[i])
      cnt_present++;
    twizy->FormatDTC(buf, i, e);
    buf.append("\n");
  }

  writer->printf("DTC report: %d stored, %d present\n%s",
                 cnt_stored, cnt_present, buf.c_str());
}


void OvmsVehicleRenaultTwizy::shell_obd_cleardtc(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = GetInstance(writer);
  if (!twizy)
    return;

  twizy->ResetDTCStats();

  if (!twizy->twizy_flags.CarAwake) {
    writer->puts("NOTE: Car is offline, only OVMS DTC buffer has been cleared.");
    return;
  }

  CAN_frame_t txframe = {};
  txframe.FIR.B.FF = CAN_frame_std;
  txframe.FIR.B.DLC = 8;
  txframe.MsgID = CLUSTER_TXID;

  // StartDiagnosticSession.ExtendedDiagnostic:
  //  - Request 10C0
  //  - Response 50C0 (unchecked)
  txframe.data.u8[0] = 0x02;
  txframe.data.u8[1] = 0x10;
  txframe.data.u8[2] = 0xC0;
  txframe.Write(twizy->m_can1);

  // ClearDiagnosticInformation.All:
  //  - Request 14FFFFFF
  //  - Response 54 (unchecked)
  txframe.data.u8[0] = 0x04;
  txframe.data.u8[1] = 0x14;
  txframe.data.u8[2] = 0xFF;
  txframe.data.u8[3] = 0xFF;
  txframe.data.u8[4] = 0xFF;
  txframe.Write(twizy->m_can1);

  writer->puts("DTC store has been cleared.");
}


void OvmsVehicleRenaultTwizy::shell_obd_resetdtc(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = GetInstance(writer);
  if (!twizy)
    return;

  twizy->ResetDTCStats();

  writer->puts("OVMS DTC buffer has been cleared.");
}
