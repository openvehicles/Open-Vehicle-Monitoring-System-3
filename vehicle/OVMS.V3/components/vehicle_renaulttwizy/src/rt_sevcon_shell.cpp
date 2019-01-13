/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy SEVCON Gen4 access: shell commands
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

// #include "ovms_log.h"
// static const char *TAG = "v-twizy";

#include <stdio.h>
#include <string>
#include <iomanip>
#include <sys/types.h>

#include "crypt_base64.h"
#include "ovms_notify.h"

#include "vehicle_renaulttwizy.h"
#include "rt_sevcon.h"


/**
 * sdo_buffer: simple shell utility for SDO value storage, parsing & printing
 * 
 * TODO: transform this into a general dynamic SDO class for simple reads/writes,
 *  maybe adding support for chunked transfers of large objects like EDS
 */
struct sdo_buffer
{
  // storage:
  union
  {
    uint32_t num;     // 8-32 bit numerical value
    char txt[128];    // max 127 chars string value
  };
  size_t size;        // ←→ job.sdo.xfersize
  char type;          // parse result: H=hex, D=dec, S=str
  
  // parse a shell argument:
  //  - "0x[0-9a-f]*" → hex
  //  - "[0-9-]*" → dec
  //  - else → str
  void parse(const char* arg) {
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
      size = strlen(txt)+1;
      type = 'S';
    }
  }
  
  // output buffer to writer:
  //  - prints decimal & hexadecimal representations
  //  - adds printable string prefix (if any)
  void print(OvmsWriter* writer) {
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
void SevconClient::shell_cfg_mode(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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
void SevconClient::shell_cfg_read(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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
void SevconClient::shell_cfg_write(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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


// Shell command: xrt cfg …
//    set <key> <base64data>
//    reset [key]
// 
//   key 0 → update working set & try to apply
//   key 1..99 → update stored profile (no SEVCON access)
void SevconClient::shell_cfg_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  int key = 0;
  if (argc >= 1) {
    key = strtol(argv[0], NULL, 10);
    if (key < 0 || key > 99) {
      writer->puts("ERROR: Key invalid (0=working set / 1..99=stored profile)");
      return;
    }
  }
  
  cfg_profile profile = {};
  if (argc >= 2) {
    if (strlen(argv[1]) > howmany(sizeof(profile), 3) * 4) {
      writer->puts("ERROR: Base64 string too long");
      return;
    }
    // decode & validate:
    base64decode(argv[1], (uint8_t*) &profile);
    if (profile.checksum != me->CalcProfileChecksum(profile)) {
      writer->puts("ERROR: Base64 string invalid, wrong checksum");
      return;
    }
  }
  
  // execute:
  
  if (key > 0) {
    
    // store:
    if (!me->SetParamProfile(key, profile)) {
      writer->puts("ERROR: could not store profile");
      return;
    }
    writer->printf("OK, profile #%d has been %s\n", key, cmd->GetName());
    
    // signal "unsaved" if WS now differs from stored profile:
    if (key == me->m_drivemode.profile_user)
      me->m_drivemode.unsaved = 1;
  }
  else {

    // update working set:
    me->m_profile = profile;
    
    // signal "unsaved" if custom profile active:
    me->m_drivemode.unsaved = (me->m_drivemode.profile_user > 0);
    
    // try to apply profile:

    CANopenResult_t res = me->CheckBus();
    if (res != COR_OK) {
      writer->printf("Profile updated, SEVCON unchanged: %s\n", me->GetResultString(res).c_str());
      return;
    }
    
    res = me->CfgApplyProfile(me->m_drivemode.profile_user);
    if (res == COR_OK)
      writer->puts("OK, live & base tuning changed");
    else if (res == COR_ERR_StateChangeFailed)
      writer->puts("Live tuning changed, base tuning needs STOP mode");
    else
      writer->printf("ERROR: %s\n", me->GetResultString(res));
  }
}


// Shell command: xrt cfg …
//    get [key]
// 
//   key 0 → working set
//   key 1..99 → stored profile
void SevconClient::shell_cfg_get(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  int key = 0;
  if (argc >= 1) {
    key = strtol(argv[0], NULL, 10);
    if (key < 0 || key > 99) {
      writer->puts("ERROR: Key invalid (0=working set / 1..99=stored profile)");
      return;
    }
  }
  
  // execute:
  
  if (key > 0) {
    // stored profile:
    cfg_profile profile = {};
    if (!me->GetParamProfile(key, profile)) {
      writer->puts("ERROR: could not read profile");
      return;
    }
    string base64 = base64encode(string((char*) &profile, sizeof(profile)));
    writer->printf("#%d= %s\n", key, base64.c_str());
  }
  else {
    // working set:
    me->m_profile.checksum = me->CalcProfileChecksum(me->m_profile);
    string base64 = base64encode(string((char*) &me->m_profile, sizeof(me->m_profile)));
    writer->printf("WS= %s\n", base64.c_str());
  }
}


/*
  PrintProfile: output profile characteristics
  
  SPEED 123 123
  POWER 123 123 123 123
  DRIVE 123 123 123
  RECUP 123 123 123 123
  RAMPS 123 123 123 123 123
  RAMPL 123 123
  SMOOTH 123
  TSMAP B 123@123 123@123 123@123 123@123
  TSMAP N 123@123 123@123 123@123 123@123
  TSMAP D 123@123 123@123 123@123 123@123
  BRAKELIGHT 123 123
*/
int SevconClient::PrintProfile(int capacity, OvmsWriter* writer, cfg_profile& profile)
{
  #define _cfgparam(NAME)  (((int)(profile.NAME))-1)

	int i;
	char map[3] = { 'D', 'N', 'B' };
	
	if (capacity > 14) capacity -= writer->printf("SPEED %d %d\n",
		_cfgparam(speed), _cfgparam(warn));
	
	if (capacity > 22) capacity -= writer->printf("POWER %d %d %d %d\n",
		_cfgparam(torque), _cfgparam(power_low), _cfgparam(power_high), _cfgparam(current));
	
	if (capacity > 18) capacity -= writer->printf("DRIVE %d %d %d\n",
		_cfgparam(drive), _cfgparam(autodrive_ref), _cfgparam(autodrive_minprc));
	
	if (capacity > 22) capacity -= writer->printf("RECUP %d %d %d %d\n",
		_cfgparam(neutral), _cfgparam(brake), _cfgparam(autorecup_ref), _cfgparam(autorecup_minprc));
	
	if (capacity > 26) capacity -= writer->printf("RAMPS %d %d %d %d %d\n",
		_cfgparam(ramp_start), _cfgparam(ramp_accel), _cfgparam(ramp_decel),
		_cfgparam(ramp_neutral), _cfgparam(ramp_brake));
	
	if (capacity > 14) capacity -= writer->printf("RAMPL %d %d\n",
		_cfgparam(ramplimit_accel), _cfgparam(ramplimit_decel));
	
	if (capacity > 11) capacity -= writer->printf("SMOOTH %d\n",
		_cfgparam(smooth));

	for (i = 2; i >= 0; i--) {
		if (capacity > 40) capacity -= writer->printf("TSMAP %c %d@%d %d@%d %d@%d %d@%d\n", map[i],
			_cfgparam(tsmap[i].prc1), _cfgparam(tsmap[i].spd1),
			_cfgparam(tsmap[i].prc2), _cfgparam(tsmap[i].spd2),
			_cfgparam(tsmap[i].prc3), _cfgparam(tsmap[i].spd3),
			_cfgparam(tsmap[i].prc4), _cfgparam(tsmap[i].spd4));
	}
	
	if (capacity > 19) capacity -= writer->printf("BRAKELIGHT %d %d\n",
		_cfgparam(brakelight_on), _cfgparam(brakelight_off));
	
	return capacity;
}


// Shell command: xrt cfg …
//    info [key]
// 
//   key 0 → working set
//   key 1..99 → stored profile
void SevconClient::shell_cfg_info(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s ", cmd->GetName());
  
  // parse args:
  
  int key = 0;
  if (argc >= 1) {
    key = strtol(argv[0], NULL, 10);
    if (key < 0 || key > 99) {
      writer->puts("ERROR: Key invalid (0=working set / 1..99=stored profile)");
      return;
    }
  }
  
  // execute:
  
  if (key > 0) {
    // stored profile:
    verbosity -= writer->printf("#%d:\n", key);
    cfg_profile profile = {};
    if (!me->GetParamProfile(key, profile)) {
      writer->puts("ERROR: could not read profile");
      return;
    }
    PrintProfile(verbosity, writer, profile);
  }
  else {
    // working set:
    verbosity -= writer->printf("WS:\n");
    PrintProfile(verbosity, writer, me->m_profile);
  }
}


// Shell command: xrt cfg …
//    save [key]
// 
//   no key → save into current profile
void SevconClient::shell_cfg_save(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s ", cmd->GetName());
  
  // parse args:
  
  int key = me->m_drivemode.profile_user;
  if (argc >= 1) {
    key = strtol(argv[0], NULL, 10);
  }
  if (key < 1 || key > 99) {
    writer->printf("ERROR: Key %d invalid (1..99)\n", key);
    return;
  }
  
  // execute:
  
  writer->printf("#%d: ", key);
  
  if (!me->SetParamProfile(key, me->m_profile)) {
    writer->puts("ERROR: could not save profile");
  }
  else {
    writer->puts("OK");
    me->m_drivemode.unsaved = 0;

    // make destination new current:
    if (me->m_drivemode.profile_cfgmode == me->m_drivemode.profile_user) {
      me->m_drivemode.profile_cfgmode = key;
      MyConfig.SetParamValueInt("xrt", "profile_cfgmode", me->m_drivemode.profile_cfgmode);
    }
    me->m_drivemode.profile_user = key;
    MyConfig.SetParamValueInt("xrt", "profile_user", me->m_drivemode.profile_user);
  }
}


// Shell command: xrt cfg …
//    load [key]
// 
//   no key → reload current profile (if set)
void SevconClient::shell_cfg_load(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  // parse args:
  
  int key = me->m_drivemode.profile_user;
  if (argc >= 1) {
    key = strtol(argv[0], NULL, 10);
  }
  if (key < 0 || key > 99) {
    writer->printf("ERROR: Key %d invalid (0..99)\n", key);
    return;
  }
  
  // execute:
  
  CANopenResult_t res = me->CfgSwitchProfile(key);
  writer->printf("CFG LOAD #%d: %s\n", key, me->FmtSwitchProfileResult(res).c_str());
}


// Shell command: xrt cfg …
//    drive [max_prc] [autopower_ref] [autopower_minprc] [kickdown_threshold] [kickdown_compzero]
void SevconClient::shell_cfg_drive(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  int max_prc = -1;
  int autopower_ref = -1;
  int autopower_minprc = -1;
  
  if (argc > 0)
    max_prc = strtol(argv[0], NULL, 10);
  if (argc > 1)
    autopower_ref = strtol(argv[1], NULL, 10);
  if (argc > 2)
    autopower_minprc = strtol(argv[2], NULL, 10);
  
  if (argc > 3) {
    int arg = strtol(argv[3], NULL, 10);
    MyConfig.SetParamValueInt("xrt", "kd_threshold", arg);
  }
  if (argc > 4) {
    int arg = strtol(argv[4], NULL, 10);
    MyConfig.SetParamValueInt("xrt", "kd_compzero", arg);
  }
  
  // execute:
  
  // update profile:
  me->m_profile.drive = cfgvalue(max_prc);
  me->m_profile.autodrive_ref = cfgvalue(autopower_ref);
  me->m_profile.autodrive_minprc = cfgvalue(autopower_minprc);
  me->m_drivemode.unsaved = 1;

  // reset kickdown detection:
  twizy->twizy_kickdown_hold = 0;
  twizy->twizy_kickdown_level = 0;
  
  // try to apply:
  
  CANopenResult_t res = me->CheckBus();
  if (res != COR_OK) {
    writer->printf("Profile updated (unsaved), SEVCON unchanged: %s\n", me->GetResultString(res).c_str());
    return;
  }
  
  res = me->CfgDrive(max_prc, autopower_ref, autopower_minprc);
  if (res != COR_OK)
    writer->printf("Profile updated (unsaved), SEVCON ERROR: %s\n", me->GetResultString(res).c_str());
  else
    writer->puts("OK, profile & tuning changed");
}


// Shell command: xrt cfg …
//    recup [neutral_prc] [brake_prc] [autopower_ref] [autopower_minprc]
void SevconClient::shell_cfg_recup(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  int neutral_prc = -1;
  int brake_prc = -1;
  int autopower_ref = -1;
  int autopower_minprc = -1;
  
  if (argc > 0)
    neutral_prc = strtol(argv[0], NULL, 10);
  if (argc > 1)
    brake_prc = strtol(argv[1], NULL, 10);
  else
    brake_prc = neutral_prc;
  
  if (argc > 2)
    autopower_ref = strtol(argv[2], NULL, 10);
  if (argc > 3)
    autopower_minprc = strtol(argv[3], NULL, 10);
  
  // execute:
  
  // update profile:
  me->m_profile.neutral = cfgvalue(neutral_prc);
  me->m_profile.brake = cfgvalue(brake_prc);
  me->m_profile.autorecup_ref = cfgvalue(autopower_ref);
  me->m_profile.autorecup_minprc = cfgvalue(autopower_minprc);
  me->m_drivemode.unsaved = 1;

  // try to apply:
  
  CANopenResult_t res = me->CheckBus();
  if (res != COR_OK) {
    writer->printf("Profile updated (unsaved), SEVCON unchanged: %s\n", me->GetResultString(res).c_str());
    return;
  }
  
  res = me->CfgRecup(neutral_prc, brake_prc, autopower_ref, autopower_minprc);
  if (res != COR_OK)
    writer->printf("Profile updated (unsaved), SEVCON ERROR: %s\n", me->GetResultString(res).c_str());
  else
    writer->puts("OK, profile & tuning changed");
}


// Shell command: xrt cfg …
//    ramps [start_prc] [accel_prc] [decel_prc] [neutral_prc] [brake_prc]
void SevconClient::shell_cfg_ramps(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  int start_prc = -1;
  int accel_prc = -1;
  int decel_prc = -1;
  int neutral_prc = -1;
  int brake_prc = -1;
  
  if (argc > 0)
    start_prc = strtol(argv[0], NULL, 10);
  if (argc > 1)
    accel_prc = strtol(argv[1], NULL, 10);
  if (argc > 2)
    decel_prc = strtol(argv[2], NULL, 10);
  if (argc > 3)
    neutral_prc = strtol(argv[3], NULL, 10);
  if (argc > 4)
    brake_prc = strtol(argv[4], NULL, 10);
  
  // execute:
  
  // update profile:
  me->m_profile.ramp_start = cfgvalue(start_prc);
  me->m_profile.ramp_accel = cfgvalue(accel_prc);
  me->m_profile.ramp_decel = cfgvalue(decel_prc);
  me->m_profile.ramp_neutral = cfgvalue(neutral_prc);
  me->m_profile.ramp_brake = cfgvalue(brake_prc);

  me->m_drivemode.unsaved = 1;

  // try to apply:
  
  CANopenResult_t res = me->CheckBus();
  if (res != COR_OK) {
    writer->printf("Profile updated (unsaved), SEVCON unchanged: %s\n", me->GetResultString(res).c_str());
    return;
  }
  
  res = me->CfgRamps(start_prc, accel_prc, decel_prc, neutral_prc, brake_prc);
  if (res != COR_OK)
    writer->printf("Profile updated (unsaved), SEVCON ERROR: %s\n", me->GetResultString(res).c_str());
  else
    writer->puts("OK, profile & tuning changed");
}


// Shell command: xrt cfg …
//    ramplimits [accel_prc] [decel_prc]
void SevconClient::shell_cfg_ramplimits(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  int accel_prc = -1;
  int decel_prc = -1;
  
  if (argc > 0)
    accel_prc = strtol(argv[0], NULL, 10);
  if (argc > 1)
    decel_prc = strtol(argv[1], NULL, 10);
  
  // execute:
  
  // update profile:
  me->m_profile.ramplimit_accel = cfgvalue(accel_prc);
  me->m_profile.ramplimit_decel = cfgvalue(decel_prc);

  me->m_drivemode.unsaved = 1;

  // try to apply:
  
  CANopenResult_t res = me->CheckBus();
  if (res != COR_OK) {
    writer->printf("Profile updated (unsaved), SEVCON unchanged: %s\n", me->GetResultString(res).c_str());
    return;
  }
  
  res = me->CfgRampLimits(accel_prc, decel_prc);
  if (res != COR_OK)
    writer->printf("Profile updated (unsaved), SEVCON ERROR: %s\n", me->GetResultString(res).c_str());
  else
    writer->puts("OK, profile & tuning changed");
}


// Shell command: xrt cfg …
//    smooth [prc]
void SevconClient::shell_cfg_smooth(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  int prc = -1;
  
  if (argc > 0)
    prc = strtol(argv[0], NULL, 10);
  
  // execute:
  
  // update profile:
  me->m_profile.smooth = cfgvalue(prc);

  me->m_drivemode.unsaved = 1;

  // try to apply:
  
  CANopenResult_t res = me->CheckBus();
  if (res != COR_OK) {
    writer->printf("Profile updated (unsaved), SEVCON unchanged: %s\n", me->GetResultString(res).c_str());
    return;
  }
  
  res = me->CfgSmoothing(prc);
  if (res != COR_OK)
    writer->printf("Profile updated (unsaved), SEVCON ERROR: %s\n", me->GetResultString(res).c_str());
  else
    writer->puts("OK, profile & tuning changed");
}


// Shell command: xrt cfg …
//    speed [max_kph] [warn_kph]
void SevconClient::shell_cfg_speed(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  int max_kph = -1;
  int warn_kph = -1;
  
  if (argc > 0)
    max_kph = strtol(argv[0], NULL, 10);
  if (argc > 1)
    warn_kph = strtol(argv[1], NULL, 10);
  
  // execute:
  
  // update profile:
  me->m_profile.speed = cfgvalue(max_kph);
  me->m_profile.warn = cfgvalue(warn_kph);

  me->m_drivemode.unsaved = 1;

  // try to apply:
  
  SevconJob sc(me);
  CANopenResult_t res = sc.CfgMode(true);
  if (res != COR_OK) {
    writer->printf("Profile updated (unsaved), SEVCON unchanged: %s\n", me->GetResultString(res).c_str());
    return;
  }
  
  res = me->CfgSpeed(max_kph, warn_kph);
  if (res == COR_OK)
    res = me->CfgMakePowermap();
  
  if (res != COR_OK)
    writer->printf("Profile updated (unsaved), SEVCON ERROR: %s\n", me->GetResultString(res).c_str());
  else
    writer->puts("OK, profile & tuning changed, restart Twizy to activate!");
  
  sc.CfgMode(false);
}


// Shell command: xrt cfg …
//    power [trq_prc] [pwr_lo_prc] [pwr_hi_prc] [curr_prc]
void SevconClient::shell_cfg_power(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  int trq_prc = -1;
  int pwr_lo_prc = -1;
  int pwr_hi_prc = -1;
  int curr_prc = -1;
  
  if (argc > 0)
    trq_prc = strtol(argv[0], NULL, 10);
  
  if (argc > 1)
    pwr_lo_prc = strtol(argv[1], NULL, 10);
  else
    pwr_lo_prc = trq_prc;
  
  if (argc > 2)
    pwr_hi_prc = strtol(argv[2], NULL, 10);
  else
    pwr_hi_prc = pwr_lo_prc;
  
  if (argc > 3)
    curr_prc = strtol(argv[3], NULL, 10);
  
  // execute:
  
  // update profile:
  me->m_profile.torque = cfgvalue(trq_prc);
  me->m_profile.power_low = cfgvalue(pwr_lo_prc);
  me->m_profile.power_high = cfgvalue(pwr_hi_prc);
  me->m_profile.current = cfgvalue(curr_prc);

  me->m_drivemode.unsaved = 1;

  // try to apply:
  
  SevconJob sc(me);
  CANopenResult_t res = sc.CfgMode(true);
  if (res != COR_OK) {
    writer->printf("Profile updated (unsaved), SEVCON unchanged: %s\n", me->GetResultString(res).c_str());
    return;
  }
  
  res = me->CfgPower(trq_prc, pwr_lo_prc, pwr_hi_prc, curr_prc);
  if (res == COR_OK)
    res = me->CfgMakePowermap();
  
  if (res != COR_OK)
    writer->printf("Profile updated (unsaved), SEVCON ERROR: %s\n", me->GetResultString(res).c_str());
  else
    writer->puts("OK, profile & tuning changed, restart Twizy to activate!");
  
  sc.CfgMode(false);
}


// Shell command: xrt cfg …
//    tsmap [maps] [t1_prc[@t1_spd]] [t2_prc[@t2_spd]] [t3_prc[@t3_spd]] [t4_prc[@t4_spd]]
void SevconClient::shell_cfg_tsmap(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  char maps[4] = { 'D', 'N', 'B', 0 };
  int prc[4] = { -1, -1, -1, -1 };
  int spd[4] = { -1, -1, -1, -1 };
  
  int i;
  char *s;
  
  if (argc > 0) {
    for (i=0; i<3 && argv[0][i]; i++)
      maps[i] = argv[0][i] & ~0x20;
    maps[i] = 0;
  }
  
  for (i=1; i<=4 && i<argc; i++) {
    prc[i] = strtol(argv[i], &s, 10);
    if (*s == '@')
      spd[i] = strtol(s+1, NULL, 10);
  }
  
  // execute:
  
  // update profile:
  for (i=0; maps[i]; i++) {
    int m = (maps[i]=='D') ? 0 : ((maps[i]=='N') ? 1 : 2);
    me->m_profile.tsmap[m].prc1 = cfgvalue(prc[0]);
    me->m_profile.tsmap[m].prc2 = cfgvalue(prc[1]);
    me->m_profile.tsmap[m].prc3 = cfgvalue(prc[2]);
    me->m_profile.tsmap[m].prc4 = cfgvalue(prc[3]);
    me->m_profile.tsmap[m].spd1 = cfgvalue(spd[0]);
    me->m_profile.tsmap[m].spd2 = cfgvalue(spd[1]);
    me->m_profile.tsmap[m].spd3 = cfgvalue(spd[2]);
    me->m_profile.tsmap[m].spd4 = cfgvalue(spd[3]);
  }
  
  me->m_drivemode.unsaved = 1;

  // try to apply:
  
  SevconJob sc(me);
  CANopenResult_t res = sc.CfgMode(true);
  if (res != COR_OK) {
    writer->printf("Profile updated (unsaved), SEVCON unchanged: %s\n", me->GetResultString(res).c_str());
    return;
  }
  
  for (i=0; maps[i]; i++) {
    if ((res = me->CfgTSMap(maps[i],
                    prc[0], prc[1], prc[2], prc[3],
                    spd[0], spd[1], spd[2], spd[3])) != COR_OK)
      break;
  }
  
  if (res != COR_OK)
    writer->printf("Profile updated (unsaved), SEVCON ERROR: %s\n", me->GetResultString(res).c_str());
  else
    writer->puts("OK, profile & tuning changed, restart Twizy to activate!");
  
  sc.CfgMode(false);
}


// Shell command: xrt cfg …
//    brakelight [on_lev] [off_lev]
void SevconClient::shell_cfg_brakelight(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  int on_lev = -1;
  int off_lev = -1;
  
  if (argc > 0)
    on_lev = strtol(argv[0], NULL, 10);
  
  if (argc > 1)
    off_lev = strtol(argv[1], NULL, 10);
  else
    off_lev = on_lev;
  
  // execute:
  
  // update profile:
  me->m_profile.brakelight_on = cfgvalue(on_lev);
  me->m_profile.brakelight_off = cfgvalue(off_lev);

  me->m_drivemode.unsaved = 1;

  // try to apply:
  
  SevconJob sc(me);
  CANopenResult_t res = sc.CfgMode(true);
  if (res != COR_OK) {
    writer->printf("Profile updated (unsaved), SEVCON unchanged: %s\n", me->GetResultString(res).c_str());
    return;
  }
  
  res = me->CfgBrakelight(on_lev, off_lev);
  if (res != COR_OK)
    writer->printf("Profile updated (unsaved), SEVCON ERROR: %s\n", me->GetResultString(res).c_str());
  else
    writer->puts("OK, profile & tuning changed, restart Twizy to activate!");
  
  sc.CfgMode(false);
}


// Shell command: xrt cfg …
//    querylogs [which=1] [start=0]
void SevconClient::shell_cfg_querylogs(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  int which = 1;
  int start = 0;
  
  if (argc > 0)
    which = strtol(argv[0], NULL, 10);
  if (argc > 1)
    start = strtol(argv[1], NULL, 10);
  
  // execute:
  
  int totalcnt = 0, sendcnt = 0;
  CANopenResult_t res;
  
  if (strcmp(cmd->GetName(), "showlogs") == 0)
    res = me->QueryLogs(verbosity, writer, which, start, &totalcnt, &sendcnt);
  else
    res = me->QueryLogs(0, NULL, which, start, &totalcnt, &sendcnt);
  
  if (res != COR_OK)
    writer->printf("Failed: %s\n", me->GetResultString(res).c_str());
  else if (sendcnt == 0)
    writer->printf("No log entries retrieved.\n");
  else
    writer->printf("Log entries #%d-%d of %d retrieved.\n", start, start+sendcnt-1, totalcnt);
}


// Shell command: xrt cfg …
//    clearlogs [which=99]
void SevconClient::shell_cfg_clearlogs(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = OvmsVehicleRenaultTwizy::GetInstance(writer);
  if (!twizy)
    return;
  SevconClient* me = twizy->m_sevcon;
  
  verbosity -= writer->printf("CFG %s: ", cmd->GetName());
  
  // parse args:
  
  int which = 99;
  if (argc > 0)
    which = strtol(argv[0], NULL, 10);
  
  // execute:
  
  int cnt = 0;
  CANopenResult_t res = me->ResetLogs(which, &cnt);
  if (res != COR_OK)
    writer->printf("Failed: %s\n", me->GetResultString(res).c_str());
  else
    writer->printf("%d log entries cleared.\n", cnt);
}


