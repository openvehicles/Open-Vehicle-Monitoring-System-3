/*
;    Project:       Open Vehicle Monitor System
;    Date:          10th May 2020
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
static const char *TAG = "tpms";

const char *TPMS_PARAM = "tpms";

#include <iostream>
#include <iomanip>
#include "ovms_tpms.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "vehicle.h"

OvmsTPMS MyTPMS __attribute__ ((init_priority (4700)));

void tpms_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("\nTyre Sets:");
  OvmsConfigParam *p = MyConfig.CachedParam(TPMS_PARAM);
  if (p)
    {
    for (ConfigParamMap::iterator it=p->m_map.begin(); it!=p->m_map.end(); ++it)
      {
      writer->printf("  %s: %s\n", it->first.c_str(),it->second.c_str());
      }
    }
  }

void tpms_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::ostringstream* buffer = new std::ostringstream();

  for (int k=1;k<argc;k++)
    {
    if (k>1) { *buffer << ","; }
    for (const char *p=argv[k];*p!=0;p++)
      {
      if (!(((*p>='0')&&(*p<='9'))||
           ((*p>='a')&&(*p<='f'))||
           ((*p>='A')&&(*p<='F'))))
        {
        delete buffer;
        writer->printf("Error: Tyre ID '%s' if not valid\n",argv[k]);
        return;
        }
      }
    uint32_t id = strtoul(argv[k], (char**)0, 16);
    *buffer << std::nouppercase << std::setfill('0') << std::setw(8) << std::hex << id;
    }

  std::string result = buffer->str();
  MyConfig.SetParamValue(TPMS_PARAM,argv[0],result);
  writer->printf("Tyre set '%s' defined as %s\n",argv[0],result.c_str());

  delete buffer;
  }

void tpms_delete(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyConfig.IsDefined(TPMS_PARAM,argv[0]))
    {
    MyConfig.DeleteInstance(TPMS_PARAM,argv[0]);
    writer->printf("Deleted tyre set '%s'\n",argv[0]);
    }
  else
    {
    writer->printf("Error: Tyre set '%s' is not defined\n",argv[0]);
    }
  }

void tpms_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (StandardMetrics.ms_v_tpms_fl_t->IsStale())
    {
    writer->puts("TPMS data is stale. Information shown below is not up to date\n");
    }

  writer->puts("TPMS status:");
  writer->printf("  FL: %s %s\n",StandardMetrics.ms_v_tpms_fl_t->AsUnitString().c_str(),
                                 StandardMetrics.ms_v_tpms_fl_p->AsUnitString().c_str());
  writer->printf("  FR: %s %s\n",StandardMetrics.ms_v_tpms_fr_t->AsUnitString().c_str(),
                                 StandardMetrics.ms_v_tpms_fr_p->AsUnitString().c_str());
  writer->printf("  RL: %s %s\n",StandardMetrics.ms_v_tpms_rl_t->AsUnitString().c_str(),
                                 StandardMetrics.ms_v_tpms_rl_p->AsUnitString().c_str());
  writer->printf("  RR: %s %s\n",StandardMetrics.ms_v_tpms_rr_t->AsUnitString().c_str(),
                                 StandardMetrics.ms_v_tpms_rr_p->AsUnitString().c_str());

  writer->puts("");
  tpms_list(verbosity, writer, cmd, argc, argv);
  }

void tpms_read(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsVehicle* ov = MyVehicleFactory.ActiveVehicle();
  if (ov == NULL)
    {
    writer->puts("Error: No vehicle module loaded");
    return;
    }

  std::vector<uint32_t> tpms;
  if ((!ov->TPMSRead(&tpms)) || tpms.empty())
    {
    writer->puts("Error: TPMS IDs could not be read from the vehicle (or not implemented)");
    return;
    }

  std::ostringstream* buffer = new std::ostringstream();
  bool first = true;
  for(uint32_t id : tpms)
    {
    if (!first) { *buffer << ","; }
    first = false;
    *buffer << std::nouppercase << std::setfill('0') << std::setw(8) << std::hex << id;
    }

  std::string result = buffer->str();
  delete buffer;

  if (argc>0)
    {
    MyConfig.SetParamValue(TPMS_PARAM,argv[0],result);
    writer->printf("Tyre set '%s' read as %s\n",argv[0],result.c_str());
    }
  else
    {
    writer->printf("TPMS read as %s\n",result.c_str());
    }
  }

void tpms_write(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsVehicle* ov = MyVehicleFactory.ActiveVehicle();
  if (ov == NULL)
    {
    writer->puts("Error: No vehicle module loaded");
    return;
    }

  if (! MyConfig.IsDefined(TPMS_PARAM,argv[0]))
    {
    writer->printf("Error: Tyre set '%s' not found\n",argv[0]);
    return;
    }

  std::string ids = MyConfig.GetParamValue(TPMS_PARAM,argv[0]);
  std::vector<uint32_t> tpms;
  size_t pos;
  while ((pos = ids.find(",")) != std::string::npos)
    {
    std::string token = ids.substr(0, pos);
    tpms.push_back(strtoul(token.c_str(), (char**)0, 16));
    ids.erase(0, pos + 1);
    }
  tpms.push_back(strtoul(ids.c_str(), (char**)0, 16));

  if (ov->TPMSWrite(tpms))
    {
    writer->printf("Tyre set '%s' written to vehicle TPMS successfully\n",argv[0]);
    }
  else
    {
    writer->printf("Error: Tyre IDs could not be writting to vehicle\n");
    }
  }

OvmsTPMS::OvmsTPMS()
  {
  ESP_LOGI(TAG, "Initialising TPMS (4700)");

  // Register our commands
  OvmsCommand* cmd_tpms = MyCommandApp.RegisterCommand("tpms","TPMS framework");
  cmd_tpms->RegisterCommand("status","Show TPMS status",tpms_status);
  cmd_tpms->RegisterCommand("list","Show TPMS tyre sets",tpms_list);
  cmd_tpms->RegisterCommand("read","Read TPMS IDs to sepecified tyre set",tpms_read,"<set>",0,1);
  cmd_tpms->RegisterCommand("write","Write TPMS IDs from sepecified tyre set",tpms_write,"<set>",1,1);
  cmd_tpms->RegisterCommand("set","Manually configure IDs in a tyre set",tpms_set,"<set> {<id>}",1,9);
  cmd_tpms->RegisterCommand("delete","Delete the specified TPMS tyre set configuration",tpms_delete,"<set>",1,1);

  // Register our parameters
  MyConfig.RegisterParam(TPMS_PARAM, "TPMS tyre sets", true, true);
  }

OvmsTPMS::~OvmsTPMS()
  {
  }
