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

#include "ovms_log.h"
static const char *TAG = "config";

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <sstream>
#include <dirent.h>
#include "crypt_base64.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_script.h"
#include "ovms_events.h"
#include "ovms_utils.h"
#include "ovms_boot.h"
#include "ovms_semaphore.h"
#include "ovms_vfs.h"

#ifdef CONFIG_OVMS_SC_ZIP
#include "zip_archive.h"
#endif // CONFIG_OVMS_SC_ZIP

#define OVMS_CONFIGPATH "/store/ovms_config"
#define OVMS_MAXVALSIZE 2500
//#define OVMS_PERSIST_METADATA


OvmsConfig MyConfig __attribute__ ((init_priority (1400)));

void store_mount(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyConfig.mount();
  writer->puts("Mounted STORE");
  }

void store_unmount(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyConfig.unmount();
  writer->puts("Unmounted STORE");
  }

int config_validate(OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv, bool complete)
  {
  if (!MyConfig.ismounted())
    return -1;
  auto lock = MyConfig.Lock(pdMS_TO_TICKS(100));
  if (!lock)
    return -1;
  // argv[0] is the <param>
  if (argc == 1)
    return MyConfig.m_params.Validate(writer, argc, argv[0], complete);
  // argv[1] is the <instance>
  if (argc == 2)
    {
    OvmsConfigParam* const* p = MyConfig.m_params.FindUniquePrefix(argv[0]);
    if (!p)	// <param> was not valid, so can't check <instance>
      return -1;
    return (*p)->m_instances.Validate(writer, argc, argv[1], complete);
    }
  // argv[2] is the value, which we can't validate
  return -1;
  }

void config_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyConfig.ismounted()) return;

  auto lock = MyConfig.Lock(pdMS_TO_TICKS(3000));
  if (!lock)
    {
    writer->puts("ERROR: cannot lock config store");
    return;
    }

  if (argc == 0)
    {
    // Show all parameters
    for (ConfigMap::iterator it=MyConfig.m_params.begin(); it!=MyConfig.m_params.end(); ++it)
      {
      writer->printf("%-20s %s\n", it->first.c_str(), it->second->GetTitle());
      }
    }
  else
    {
    // Show all instances for a particular parameter
    OvmsConfigParam *p = MyConfig.CachedParam(argv[0]);
    if (p)
      {
      writer->printf("%s (%s %s)\n",p->GetName().c_str(),
        (p->Readable()?"readable":"protected"),
        (p->Writable()?"writeable":"read-only"));
      for (ConfigParamMap::iterator it=p->m_instances.begin(); it!=p->m_instances.end(); ++it)
        {
        if (p->Readable())
          { writer->printf("  %s: %s\n",it->first.c_str(), it->second.c_str()); }
        else
          { writer->printf("  %s\n",it->first.c_str()); }
        }
      }
    else
      {
      writer->printf("%s not found\n", argv[0]);
      }
    }
  }

void config_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyConfig.ismounted()) return;

  auto lock = MyConfig.Lock(pdMS_TO_TICKS(3000));
  if (!lock)
    {
    writer->puts("ERROR: cannot lock config store");
    return;
    }

  OvmsConfigParam *p = MyConfig.CachedParam(argv[0]);
  if (p==NULL)
    {
    writer->puts("Error: parameter not found");
    return;
    }

  if (!p->Writable())
    {
    writer->puts("Error: parameter is not writeable");
    return;
    }

  p->SetValue(argv[1],argv[2]);
  writer->puts("Parameter has been set.");
  }

void config_rm(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyConfig.ismounted()) return;

  auto lock = MyConfig.Lock(pdMS_TO_TICKS(3000));
  if (!lock)
    {
    writer->puts("ERROR: cannot lock config store");
    return;
    }

  OvmsConfigParam *p = MyConfig.CachedParam(argv[0]);
  if (p==NULL)
    {
    writer->puts("Error: parameter not found");
    return;
    }

  if (!p->Writable())
    {
    writer->puts("Error: parameter is not writeable");
    return;
    }

  if (p->DeleteInstance(argv[1]))
    {
    writer->printf("Instance %s has been removed.\n", argv[1]);
    return;
    }
  if (strcmp(argv[1], "*") != 0)
    return;
  MyConfig.DeregisterParam(argv[0]);
  writer->printf("Parameter %s has been removed.\n", argv[0]);
  }

#ifdef CONFIG_OVMS_SC_ZIP
void config_backup(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyConfig.ismounted())
    {
    writer->puts("Error: config store not mounted");
    return;
    }

  // check path:
  if (MyConfig.ProtectedPath(argv[0]))
    {
    writer->printf("Error: path '%s' is protected\n", argv[0]);
    return;
    }

  // get password:
  std::string password;
  if (argc >= 2)
    password = argv[1];
  else
    password = MyConfig.GetParamValue("password", "module");

  MyConfig.Backup(argv[0], password, writer, verbosity);
  }

void config_restore(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyConfig.ismounted())
    {
    writer->puts("Error: config store not mounted");
    return;
    }

  // check path:
  if (MyConfig.ProtectedPath(argv[0]))
    {
    writer->printf("Error: path '%s' is protected\n", argv[0]);
    return;
    }

  // get password:
  std::string password;
  if (argc >= 2)
    password = argv[1];
  else
    password = MyConfig.GetParamValue("password", "module");

  MyConfig.Restore(argv[0], password, writer, verbosity);
  }
#endif // CONFIG_OVMS_SC_ZIP

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

static duk_ret_t DukOvmsConfigParams(duk_context *ctx)
  {
  if (!MyConfig.ismounted()) return 0;
  auto lock = MyConfig.Lock();

  duk_idx_t arr_idx = duk_push_array(ctx);
  int count = 0;
  for (ConfigMap::iterator it=MyConfig.m_params.begin(); it!=MyConfig.m_params.end(); ++it)
    {
    duk_push_string(ctx, it->first.c_str());
    duk_put_prop_index(ctx, arr_idx, count++);
    }

  return 1;
  }

static duk_ret_t DukOvmsConfigInstances(duk_context *ctx)
  {
  const char *param = duk_to_string(ctx,0);

  if (!MyConfig.ismounted()) return 0;
  auto lock = MyConfig.Lock();

  OvmsConfigParam *p = MyConfig.CachedParam(param);
  if (p)
    {
    if (! p->Readable()) return 0;  // Parameter is protected, and not readable

    duk_idx_t arr_idx = duk_push_array(ctx);
    int count = 0;
    for (ConfigParamMap::iterator it=p->m_instances.begin(); it!=p->m_instances.end(); ++it)
      {
      duk_push_string(ctx, it->first.c_str());
      duk_put_prop_index(ctx, arr_idx, count++);
      }
    return 1;
    }

  return 0;
  }

static duk_ret_t DukOvmsConfigGetValues(duk_context *ctx)
  {
  const char *param = duk_to_string(ctx,0);
  std::string prefix = duk_opt_string(ctx,1,"");

  if (!MyConfig.ismounted()) return 0;
  auto lock = MyConfig.Lock();

  OvmsConfigParam *p = MyConfig.CachedParam(param);
  if (p)
    {
    if (! p->Readable()) return 0;  // Parameter is protected, and not readable

    duk_idx_t obj_idx = duk_push_object(ctx);
    for (ConfigParamMap::iterator it=p->m_instances.lower_bound(prefix); it!=p->m_instances.end(); ++it)
      {
      if (!startsWith(it->first, prefix)) break;
      duk_push_string(ctx, it->second.c_str());
      duk_put_prop_string(ctx, obj_idx, it->first.substr(prefix.length()).c_str());
      }
    return 1;
    }

  return 0;
  }

static duk_ret_t DukOvmsConfigSetValues(duk_context *ctx)
  {
  const char *param = duk_to_string(ctx,0);
  std::string prefix = duk_opt_string(ctx,1,"");
  duk_require_object(ctx,2);

  if (!duk_is_object(ctx,2)) return 0;
  if (!MyConfig.ismounted()) return 0;
  auto lock = MyConfig.Lock();

  OvmsConfigParam *p = MyConfig.CachedParam(param);
  if (p)
    {
    if (! p->Writable()) return 0;  // Parameter is not writeable

    ConfigParamMap pmap = p->GetMap();
    std::string key, val;
    duk_enum(ctx, 2, 0);
    while (duk_next(ctx, -1, true))
      {
      key = duk_to_string(ctx, -2);
      val = duk_to_string(ctx, -1);
      duk_pop_2(ctx);
      pmap[prefix+key] = val;
      }
    duk_pop(ctx);
    p->SetMap(pmap);
    }

  return 0;
  }

static duk_ret_t DukOvmsConfigGet(duk_context *ctx)
  {
  const char *param = duk_to_string(ctx,0);
  const char *instance = duk_to_string(ctx,1);
  const char *defvalue = duk_to_string(ctx,2);

  if (!MyConfig.ismounted()) return 0;
  auto lock = MyConfig.Lock();

  OvmsConfigParam *p = MyConfig.CachedParam(param);
  if (p)
    {
    if (! p->Readable()) return 0;  // Parameter is protected, and not readable
    if (p->IsDefined(instance))
      {
      std::string v = p->GetValue(instance);
      duk_push_string(ctx, v.c_str());
      }
    else
      {
      duk_push_string(ctx, defvalue);
      }
    return 1;
    }
  else
    {
    return 0;
    }
  return 0;
  }

static duk_ret_t DukOvmsConfigSet(duk_context *ctx)
  {
  const char *param = duk_to_string(ctx,0);
  const char *instance = duk_to_string(ctx,1);
  const char *value = duk_to_string(ctx,2);

  if (!MyConfig.ismounted()) return 0;
  auto lock = MyConfig.Lock();

  OvmsConfigParam *p = MyConfig.CachedParam(param);
  if (p)
    {
    if (! p->Writable()) return 0;  // Parameter is not writeable
    p->SetValue(instance, value);
    }
  return 0;
  }

static duk_ret_t DukOvmsConfigDelete(duk_context *ctx)
  {
  const char *param = duk_to_string(ctx,0);
  const char *instance = duk_to_string(ctx,1);

  if (!MyConfig.ismounted()) return 0;
  auto lock = MyConfig.Lock();

  OvmsConfigParam *p = MyConfig.CachedParam(param);
  if (p)
    {
    if (! p->Writable()) return 0;  // Parameter is not writeable
    p->DeleteInstance(instance);
    }
  return 0;
  }

#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE


/**
 * Event phase handlers for "config.changed" & "config.mounted"
 * 
 * To reduce mutex overhead & simplify config listeners, we lock the config store around the callback loop.
 * 
 * Note: scripts bound to a config event can only use atomic config calls (commands / JS API), and may also
 * be executed asynchronously in another context (i.e. Duktape), so the lock doesn't cover these.
 */

static void config_event_lock_loop(const char* event, void* data, event_phase_t phase, void** phasedata)
  {
  switch (phase)
    {
    case EVPH_PreCallbackLoop:
      *phasedata = (void*) MyConfig.CreateLock();
      break;
    case EVPH_PostCallbackLoop:
      delete (OvmsConfig::Transaction*) *phasedata;
      *phasedata = NULL;
      break;
    default:
      break;
    }
  }

static void config_event_lock_loop_and_delete(const char* event, void* data, event_phase_t phase, void** phasedata)
  {
  if (phase == EVPH_FreeData)
    delete (OvmsConfigParam*) data;
  else
    config_event_lock_loop(event, data, phase, phasedata);
  }


/**
 * OvmsConfig (MyConfig) constructor
 */
OvmsConfig::OvmsConfig()
  {
  ESP_LOGI(TAG, "Initialising CONFIG (1400)");

  m_mounted = false;

  OvmsCommand* cmd_store = MyCommandApp.RegisterCommand("store","STORE framework");
  cmd_store->RegisterCommand("mount","Mount STORE",store_mount);
  cmd_store->RegisterCommand("unmount","Unmount STORE",store_unmount);

  OvmsCommand* cmd_config = MyCommandApp.RegisterCommand("config","CONFIG framework");
  cmd_config->RegisterCommand("list","Show configuration parameters/instances",config_list,"[<param>]",0,1, true, config_validate);
  cmd_config->RegisterCommand("set","Set parameter:instance=value",config_set,"<param> <instance> <value>",3,3, true, config_validate);
  cmd_config->RegisterCommand("rm","Remove parameter:instance",config_rm,"<param> {<instance> | *}",2,2, true, config_validate);

#ifdef CONFIG_OVMS_SC_ZIP
  cmd_config->RegisterCommand("backup", "Backup to file", config_backup,
    "<zipfile> [password=module password]\n"
    "Backup system configuration & scripts into password protected ZIP file.\n"
    "Note: user files or directories in /store will not be included.\n"
    "<password> defaults to the current module password, set to \"\" to disable encryption.\n"
    "Hint: use 7z to unzip/create backup ZIPs on a PC.", 1, 2, true, vfs_file_validate);
  cmd_config->RegisterCommand("restore", "Restore from file", config_restore,
    "<zipfile> [password=module password]\n"
    "Restore system configuration & scripts from password protected ZIP file.\n"
    "Note: user files or directories in /store will not be touched.\n"
    "The module will perform a reboot after successful restore.\n"
    "<password> defaults to the current module password.\n"
    "Note: You need to supply the password used for the backup creation.\n"
    "The default password is not available after flash is erased.", 1, 2, true, vfs_file_validate);
#endif // CONFIG_OVMS_SC_ZIP

  RegisterParam("password", "Password store", true, false);
  RegisterParam("module", "Module configuration", true, true);
  RegisterParam("usr", "Custom plugin configuration", true, true);

  #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  DuktapeObjectRegistration* dto = new DuktapeObjectRegistration("OvmsConfig");
  dto->RegisterDuktapeFunction(DukOvmsConfigParams, 0, "Params");
  dto->RegisterDuktapeFunction(DukOvmsConfigInstances, 1, "Instances");
  dto->RegisterDuktapeFunction(DukOvmsConfigGet, 3, "Get");
  dto->RegisterDuktapeFunction(DukOvmsConfigSet, 3, "Set");
  dto->RegisterDuktapeFunction(DukOvmsConfigDelete, 2, "Delete");
  dto->RegisterDuktapeFunction(DukOvmsConfigGetValues, 2, "GetValues");
  dto->RegisterDuktapeFunction(DukOvmsConfigSetValues, 3, "SetValues");
  MyDuktape.RegisterDuktapeObject(dto);
  #endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }

OvmsConfig::~OvmsConfig()
  {
  }

esp_err_t OvmsConfig::mount()
  {
//  if (!spiffs_is_registered)
//    vfs_spiffs_register();

//  if (!spiffs_is_mounted)
//    spiffs_mount();

  if (m_mounted)
    return ESP_OK;

  auto lock = Lock();

  memset(&m_store_fat,0,sizeof(esp_vfs_fat_sdmmc_mount_config_t));
  m_store_fat.format_if_mount_failed = true;
  m_store_fat.max_files = 5;

#if ESP_IDF_VERSION_MAJOR >= 5
  esp_vfs_fat_spiflash_mount_rw_wl("/store", "store", &m_store_fat, &m_store_wlh);
#else
  esp_vfs_fat_spiflash_mount("/store", "store", &m_store_fat, &m_store_wlh);
#endif
  m_mounted = true;

  struct stat ds;
  if (stat(OVMS_CONFIGPATH, &ds) != 0)
    {
    ESP_LOGI(TAG, "Initialising OVMS CONFIG within STORE");
    mkdir(OVMS_CONFIGPATH,0);
    }

  DIR *dir;
  struct dirent *dp;
  if ((dir = opendir(OVMS_CONFIGPATH)) == NULL)
    {
    ESP_LOGE(TAG, "Error: Cannot open config store directory");
    return ESP_ERR_NOT_FOUND;
    }
  while ((dp = readdir(dir)) != NULL)
    {
    // Register the param in case this was not already done
    if (CachedParam(dp->d_name) == NULL)
      RegisterParam(dp->d_name, "", true, false);
    }
  closedir(dir);

  // load & upgrade params:
  for (ConfigMap::iterator it=m_params.begin(); it!=m_params.end(); ++it)
    {
    it->second->Load();
    }
  upgrade();

  MyEvents.SignalEvent("config.mounted", NULL, config_event_lock_loop);
  return ESP_OK;
  }

esp_err_t OvmsConfig::unmount()
  {
//  if (spiffs_is_mounted)
//    spiffs_unmount(0);

  if (m_mounted)
    {
    auto lock = Lock();
#if ESP_IDF_VERSION_MAJOR >= 5
    esp_vfs_fat_spiflash_unmount_rw_wl("/store", m_store_wlh);
#else
    esp_vfs_fat_spiflash_unmount("/store", m_store_wlh);
#endif
    m_mounted = false;
    MyEvents.SignalEvent("config.unmounted", NULL);
    }

  return ESP_OK;
  }

bool OvmsConfig::ismounted()
  {
  return m_mounted;
  }

void OvmsConfig::upgrade()
  {
  auto lock = Lock();

  // Migrate password/changed → module/init:
  if (GetParamValueBool("password", "changed") == true)
    {
    SetParamValue("module", "init", "done");
    DeleteInstance("password", "changed");
    }

  // Migrate vehicle.require.* signals to config:
  if (GetParamValueInt("module", "cfgversion") < 2018112200)
    {
    std::string vt = GetParamValue("auto", "vehicle.type");
    if (vt=="FT5E" || vt=="KS" || vt=="MI" || vt=="RT" || vt=="TGTC" || vt=="VA" || vt=="ZEVA")
      {
      SetParamValueBool("modem", "enable.gps", true);
      SetParamValueBool("modem", "enable.gpstime", true);
      }
    }

  // Migrate server.v2 password (from server.v2 to password)
  if (GetParamValueInt("module", "cfgversion") < 2020053100)
    {
    if (IsDefined("server.v2","password"))
      {
      std::string p = GetParamValue("server.v2", "password");
      SetParamValue("password","server.v2",p);
      DeleteInstance("server.v2","password");
      }
    }

  // Migrate vehicle IDs VWUP.T26/.OBD back to VWUP
  std::string vt = GetParamValue("auto", "vehicle.type");
  if (vt == "VWUP.T26" || vt == "VWUP.OBD")
    {
    SetParamValue("auto", "vehicle.type", "VWUP");
    }
  // Move obsolete VWUP "xut" instances to "xvu":
  if (CachedParam("xut"))
    {
    RegisterParam("xvu", "VW e-Up", true, true);
    for (const auto& instance : { "canwrite", "modelyear", "cc_temp" })
      {
      if (!IsDefined("xvu", instance) && IsDefined("xut", instance))
        SetParamValue("xvu", instance, GetParamValue("xut", instance));
      }
    DeregisterParam("xut");
    }
  // Remove obsolete VWUP "vwup" param:
  if (CachedParam("vwup"))
    {
    DeregisterParam("vwup");
    }

  // Update single units setting of miles/km to multiple units settings
  if (GetParamValueInt("module", "cfgversion") < 2022111900)
    {
    bool ismiles = GetParamValue("vehicle", "units.distance") == "M";
    if (ismiles)
      {
      SetParamValue("vehicle", "units.speed", "miph");
      SetParamValue("vehicle", "units.accel", "miphps");
      SetParamValue("vehicle", "units.accelshort", "ftpss");
      SetParamValue("vehicle", "units.consumption", "mipkwh");
      }
    }
  if (GetParamValueInt("module", "cfgversion") < 2022121400) 
    {
    auto val = GetParamValue("vehicle", "units.preasure");
    if (val != "")
      {
      if (GetParamValue("vehicle", "units.pressure") != "")
        SetParamValue("vehicle", "units.pressure", val);
      }
    DeleteInstance("vehicle", "units.preasure");
    }

  // Done, set config version:
  SetParamValueInt("module", "cfgversion", 2022121400);
  }


/**
 * Transaction (recursive mutex lock with auto commit)
 */
OvmsConfig::Transaction::Transaction(OvmsRecMutex* mutex, TickType_t timeout /*=portMAX_DELAY*/)
  : OvmsRecMutexLock(mutex, timeout)
  {
  }

OvmsConfig::Transaction::Transaction(OvmsConfig::Transaction&& src)
  : OvmsRecMutexLock(std::move(src))
  {
  }

OvmsConfig::Transaction::~Transaction()
  {
  if (m_mutex->GetCount() == 1)
    {
    // last lock is about to be released, commit changes:
    MyConfig.TransactionCommit();
    }
  }

/**
 * TransactionAdd: add a param save/delete operation to the current transaction
 */
void OvmsConfig::TransactionAdd(OvmsConfigParam* param, TransOp todo)
  {
  TransOp have = m_transaction[param]; // creates TransOp::Save entry if not yet registered
  // upgrade to TransOp::Delete?
  if (todo > have)
    {
    m_transaction[param] = todo;
    }
  }

/**
 * TransactionCommit: perform postponed operations
 */
void OvmsConfig::TransactionCommit()
  {
  if (m_transaction.size() == 0) return;
  ESP_LOGD(TAG, "TransactionCommit: %u operation(s) pending", m_transaction.size());

  // Try to lock config storage:
  OvmsMutexLock store_lock(&m_store_mutex, 0);
  if (!store_lock)
    {
    ESP_LOGD(TAG, "TransactionCommit: deferring, storage locked");
    return;
    }

  // Process pending operations:
  for (auto it = m_transaction.begin(); it != m_transaction.end(); it++)
    {
    OvmsConfigParam* param = it->first;
    if (it->second == TransOp::Delete)
      {
      ESP_LOGD(TAG, "TransactionCommit: Delete param=%s", param->GetName().c_str());
      param->DeleteConfig();
      MyEvents.SignalEvent("config.changed", param, config_event_lock_loop_and_delete);
      }
    else // TransOp::Save
      {
      ESP_LOGD(TAG, "TransactionCommit: Save param=%s", param->GetName().c_str());
      param->RewriteConfig();
      MyEvents.SignalEvent("config.changed", param, config_event_lock_loop);
      }
    }
  m_transaction.clear();
  }

void OvmsConfig::RegisterParam(std::string name, std::string title, bool writable, bool readable)
  {
  auto lock = Lock();
  auto k = m_params.find(name);
  if (k == m_params.end())
    {
    OvmsConfigParam* p = new OvmsConfigParam(name, title, writable, readable);
    m_params[name] = p;
    }
  else
    {
    k->second->SetTitle(title);
    k->second->SetAccess(writable, readable);
    }
  }

void OvmsConfig::DeregisterParam(std::string name)
  {
  auto lock = Lock();
  auto k = m_params.find(name);
  if (k != m_params.end())
    {
    k->second->m_instances.clear();
    m_params.erase(k);
    TransactionAdd(k->second, TransOp::Delete);
    }
  }

void OvmsConfig::SetParamValue(std::string param, std::string instance, std::string value)
  {
  if (!m_mounted) return;
  auto lock = Lock();
  OvmsConfigParam *p = CachedParam(param);
  if (p)
    {
    p->SetValue(instance,value);
    }
  }

void OvmsConfig::SetParamValueBinary(std::string param, std::string instance, std::string value, BinaryEncoding_t encoding /*=Encoding_HEX*/)
  {
  std::string encval;
  if (encoding == Encoding_BASE64)
    {
    encval = base64encode(value);
    }
  else // Encoding_HEX
    {
    encval = hexencode(value);
    }
  SetParamValue(param, instance, encval);
  }

void OvmsConfig::SetParamValueInt(std::string param, std::string instance, int value)
  {
  std::ostringstream ss;
  ss << value;
  SetParamValue(param, instance, std::string(ss.str()));
  }

void OvmsConfig::SetParamValueFloat(std::string param, std::string instance, float value)
  {
  std::ostringstream ss;
  ss << value;
  SetParamValue(param, instance, std::string(ss.str()));
  }

void OvmsConfig::SetParamValueBool(std::string param, std::string instance, bool value)
  {
  SetParamValue(param, instance, std::string(value ? "yes" : "no"));
  }

void OvmsConfig::DeleteInstance(std::string param, std::string instance)
  {
  auto lock = Lock();
  OvmsConfigParam *p = CachedParam(param);
  if (p)
    {
    p->DeleteInstance(instance);
    }
  }

std::string OvmsConfig::GetParamValue(std::string param, std::string instance, std::string defvalue)
  {
  if (!m_mounted) return defvalue;
  auto lock = Lock();
  OvmsConfigParam *p = CachedParam(param);
  if (p && p->IsDefined(instance))
    {
    return p->GetValue(instance);
    }
  else
    {
    return defvalue;
    }
  }

std::string OvmsConfig::GetParamValueBinary(std::string param, std::string instance, std::string defvalue, BinaryEncoding_t encoding /*=Encoding_HEX*/)
  {
  std::string encval = GetParamValue(param,instance);
  size_t len = encval.length();
  if (len == 0) return defvalue;
  if (encoding == Encoding_BASE64)
    {
    return base64decode(encval);
    }
  else // Encoding_HEX
    {
    std::string value = hexdecode(encval);
    if (value.empty())
      {
      ESP_LOGE(TAG, "Invalid non-hex value for config param %s instance %s",
        param.c_str(), instance.c_str());
      return defvalue;
      }
    return value;
    }
  }

int OvmsConfig::GetParamValueInt(std::string param, std::string instance, int defvalue)
  {
  std::string value = GetParamValue(param,instance);
  if (value.length() == 0) return defvalue;
  return atoi(value.c_str());
  }

float OvmsConfig::GetParamValueFloat(std::string param, std::string instance, float defvalue)
  {
  std::string value = GetParamValue(param,instance);
  if (value.length() == 0) return defvalue;
  return atof(value.c_str());
  }

bool OvmsConfig::GetParamValueBool(std::string param, std::string instance, bool defvalue)
  {
  std::string value = GetParamValue(param,instance);
  if (value.length() == 0) return defvalue;
  return strtobool(value);
  }

bool OvmsConfig::IsDefined(std::string param, std::string instance)
  {
  if (!m_mounted) return false;
  auto lock = Lock();
  OvmsConfigParam *p = CachedParam(param);
  if (p == NULL) return false;
  return p->IsDefined(instance);
  }

OvmsConfigParam* OvmsConfig::CachedParam(std::string param)
  {
  if (!m_mounted) return NULL;
  auto lock = Lock();
  OvmsConfigParam* const* p = m_params.FindUniquePrefix(param.c_str());
  if (!p)
    return NULL;
  return *p;
  }

/**
 * ProtectedPath: `true` if the path is protected (e.g. config)
 * - Note: path is canonicalized before comparison, in case the path
 * does not exist in the filesystem, the result will be `false`
 * (i.e.: not protected).
 */
bool OvmsConfig::ProtectedPath(std::string path)
  {
#ifdef CONFIG_OVMS_DEV_CONFIGVFS
  return false;
#else
  char canonical_path[PATH_MAX];
  char * result = realpath(path.c_str(), canonical_path);
  if (NULL == result) {
    // If error during path resolution, we consider it as not protected
    return false;
  }
  std::string resolved_path(canonical_path);
  return (resolved_path.find(OVMS_CONFIGPATH) != std::string::npos);
#endif // #ifdef CONFIG_OVMS_DEV_CONFIGVFS
  }

/**
 * GetParamMap: get map (copy) of param instances
 */
ConfigParamMap OvmsConfig::GetParamMap(std::string param)
  {
  ConfigParamMap pmap;
  if (!m_mounted) return pmap;
  auto lock = Lock();
  if (!CachedParam(param))
    RegisterParam(param, "", true, false);
  OvmsConfigParam* p = CachedParam(param);
  if (p)
    pmap = p->GetMap();
  return pmap;
  }

/**
 * SetParamMap: replace all param instances
 * - Note: items will be removed from source map, map is empty afterwards
 */
void OvmsConfig::SetParamMap(std::string param, ConfigParamMap& map)
  {
  if (!m_mounted) return;
  auto lock = Lock();
  if (!CachedParam(param))
    RegisterParam(param, "", true, false);
  OvmsConfigParam* p = CachedParam(param);
  if (p)
    p->SetMap(map);
  }

#ifdef CONFIG_OVMS_SC_ZIP

/**
 * Backup:
 */

static struct
  {
  const char* name;
  bool optional;
  }
  backup_dir[] =
  {
    { "ovms_config", false },
    { "events", true },
    { "scripts", true },
    { "obd2ecu", true },
    { "dbc", true },
    { "plugin", true },
    { "tls", true },
    { "trustedca", true },
    { "usr", true },
    { NULL, false }
  };

bool OvmsConfig::Backup(std::string path, std::string password, OvmsWriter* writer /*=NULL*/, int verbosity /*=1024*/)
  {
  if (writer)
    writer->printf("Creating config backup '%s'...\n", path.c_str());
  else
    ESP_LOGD(TAG, "Backup: creating '%s'...", path.c_str());

  // Lock config cache & file storage:
  auto lock = Lock(pdMS_TO_TICKS(5000));
  if (!lock || !m_store_mutex.Lock(pdMS_TO_TICKS(5000)))
    {
    if (writer)
      writer->puts("Error: config store currently in use by another process");
    else
      ESP_LOGE(TAG, "Backup: timeout waiting for config lock");
    return false;
    }

  // Storage is locked, allow cache access during the (long) ZIP operation:
  lock.Unlock();

  bool ok = true;

  ZipArchive zip(path, password, ZIP_CREATE|ZIP_TRUNCATE);
  if (ok) ok = zip.chdir("/store");
  for (int i = 0; ok && backup_dir[i].name; i++)
    {
    vTaskDelay(pdMS_TO_TICKS(500)); // be nice / avoid WDT
    if (writer && verbosity >= COMMAND_RESULT_NORMAL)
      writer->printf("..add '%s'\n", backup_dir[i].name);
    else if (!writer)
      ESP_LOGD(TAG, "Backup '%s': add '%s'", path.c_str(), backup_dir[i].name);
    ok = zip.add(backup_dir[i].name, backup_dir[i].optional);
    }
  if (ok) ok = zip.close();

  if (!ok)
    {
    if (writer)
      writer->printf("Error: zip failed: %s\n", zip.strerror());
    else
      ESP_LOGE(TAG, "Backup '%s': zip failed: %s", path.c_str(), zip.strerror());
    }
  else
    {
    if (writer)
      writer->puts("Done.");
    else
      ESP_LOGI(TAG, "Backup '%s' done", path.c_str());
    }

  // Done, unlock storage…
  m_store_mutex.Unlock();

  // …and relock cache to trigger commit for deferred file operations on exit:
  if (!lock.Lock(pdMS_TO_TICKS(1000)))
    {
    size_t pending = m_transaction.size();
    if (pending > 0)
      ESP_LOGW(TAG, "Backup: cannot relock, %d operation(s) pending for commit", pending);
    // Note: the next config access will do the commit, so this isn't critical
    }

  return ok;
  }

/**
 * Restore:
 */

static bool install_dir(std::string src, std::string dst)
  {
  if (!path_exists(src))
    {
    rmtree(dst);
    return true;
    }
  std::string dstbak = dst + ".old";
  rmtree(dstbak);
  if (mkpath(dst) != 0) // ensure dst exists
    return false;
  if (rename(dst.c_str(), dstbak.c_str()) == 0)
    {
    if (rename(src.c_str(), dst.c_str()) == 0)
      {
      rmtree(dstbak);
      return true;
      }
    rename(dstbak.c_str(), dst.c_str());
    }
  return false;
  }

bool OvmsConfig::Restore(std::string path, std::string password, OvmsWriter* writer /*=NULL*/, int verbosity /*=1024*/)
  {
  if (writer)
    writer->printf("Restoring config from '%s'...\n", path.c_str());
  else
    ESP_LOGD(TAG, "Restore: reading '%s'...", path.c_str());

  // Signal & wait for components to shutdown:
  // (this needs to be done before locking the config, as the listeners may need the lock)
    {
    OvmsSemaphore eventdone;
    MyEvents.SignalEvent("config.restore", NULL, eventdone);
    eventdone.Take();
    }

  // Lock config cache & file storage:
  auto lock = Lock(pdMS_TO_TICKS(5000));
  if (!lock || !m_store_mutex.Lock(pdMS_TO_TICKS(5000)))
    {
    if (writer)
      writer->puts("Error: config store currently in use by another process");
    else
      ESP_LOGE(TAG, "Restore: timeout waiting for config lock");
    return false;
    }

  // Storage is locked, allow cache access during the (long) ZIP operation:
  lock.Unlock();

  bool ok = true;

  // unzip into restore directory:
  // (Note: all paths beginning with "/store/ovms_config" are protected)
  std::string tempdir = "/store/ovms_config_restore";

  if (rmtree(tempdir) != 0 || mkpath(tempdir) != 0)
    {
    if (writer)
      writer->printf("Error: prepare failed: %s\n", strerror(errno));
    else
      ESP_LOGE(TAG, "Restore '%s': prepare failed: %s", path.c_str(), strerror(errno));
    m_store_mutex.Unlock();
    return false;
    }

  ZipArchive zip(path, password, ZIP_RDONLY);
  if (ok) ok = zip.chdir(tempdir);
  for (int i = 0; ok && backup_dir[i].name; i++)
    {
    vTaskDelay(pdMS_TO_TICKS(500)); // be nice / avoid WDT
    if (writer && verbosity >= COMMAND_RESULT_NORMAL)
      writer->printf("..extract '%s'\n", backup_dir[i].name);
    else if (!writer)
      ESP_LOGD(TAG, "Restore '%s': extract '%s'", path.c_str(), backup_dir[i].name);
    ok = zip.extract(backup_dir[i].name, backup_dir[i].optional);
    }
  if (ok) ok = zip.close();

  if (!ok)
    {
    if (writer)
      writer->printf("Error: unzip failed: %s%s\n", zip.strerror(),
        password.empty() ? " (password required?)" : "");
    else
      ESP_LOGE(TAG, "Restore '%s': unzip failed: %s%s", path.c_str(), zip.strerror(),
        password.empty() ? " (password required?)" : "");
    rmtree(tempdir);
    m_store_mutex.Unlock();
    return false;
    }

  // replace config by restored version:

  if (writer)
    writer->puts("Installing...");
  else
    ESP_LOGD(TAG, "Restore '%s': installing...", path.c_str());

  std::string dstbase = "/store/";
  for (int i = 0; backup_dir[i].name; i++)
    {
    vTaskDelay(pdMS_TO_TICKS(500)); // be nice / avoid WDT
    if (writer && verbosity >= COMMAND_RESULT_NORMAL)
      writer->printf("..install '%s'\n", backup_dir[i].name);
    else if (!writer)
      ESP_LOGD(TAG, "Restore '%s':  install '%s'", path.c_str(), backup_dir[i].name);
    if (!install_dir(tempdir + "/" + backup_dir[i].name, dstbase + backup_dir[i].name))
      {
      ok = false;
      if (!backup_dir[i].optional)
        {
        if (writer)
          writer->printf("Error: install '%s' failed: %s\n", backup_dir[i].name, strerror(errno));
        else
          ESP_LOGE(TAG, "Restore '%s': install '%s' failed: %s", path.c_str(), backup_dir[i].name, strerror(errno));
        break;
        }
      else
        {
        if (writer)
          writer->printf("Warning: install '%s' failed: %s\n", backup_dir[i].name, strerror(errno));
        else
          ESP_LOGW(TAG, "Restore '%s': install '%s' failed: %s", path.c_str(), backup_dir[i].name, strerror(errno));
        }
      }
    }

  // cleanup:

  vTaskDelay(pdMS_TO_TICKS(500)); // be nice / avoid WDT
  rmtree(tempdir);

  if (!ok)
    {
    m_store_mutex.Unlock();
    return false;
    }

  // Restore successful, as we need to reboot now anyway, skip reloading the params, and
  // keep the storage lock in place to avoid changes from TransactionCommit

  if (writer)
    writer->puts("Done, rebooting now...");
  else
    ESP_LOGI(TAG, "Restore '%s': done, rebooting...", path.c_str());

  vTaskDelay(pdMS_TO_TICKS(1000));
  MyBoot.Restart();

  return true;
  }

#endif // CONFIG_OVMS_SC_ZIP

void OvmsConfig::SupportSummary(OvmsWriter* writer)
  {
  auto lock = Lock();
  writer->puts("\nConfiguration");

  for (ConfigMap::iterator mi=m_params.begin(); mi!=m_params.end(); ++mi)
    {
    writer->printf("  [%s]\n",mi->first.c_str());
    OvmsConfigParam* p = mi->second;
    for (ConfigParamMap::iterator it=p->m_instances.begin(); it!=p->m_instances.end(); ++it)
      {
      if (p->Readable())
        { writer->printf("    %s: %s\n",it->first.c_str(), it->second.c_str()); }
      else
        { writer->printf("    %s: **redacted**\n",it->first.c_str()); }
      }
    }
  }

OvmsConfigParam::OvmsConfigParam(std::string name, std::string title, bool writable, bool readable)
  {
  m_name = name;
  m_title = title;
  m_writable = writable;
  m_readable = readable;
  m_loaded = false;

  if (MyConfig.ismounted())
    {
    LoadConfig();
    }
  }

OvmsConfigParam::~OvmsConfigParam()
  {
  }

void OvmsConfigParam::LoadConfig()
  {
  if (m_loaded) return;  // Protected against loading more than once

  // Lock config cache & storage:
  auto lock = MyConfig.Lock();
  OvmsMutexLock store_lock(&MyConfig.m_store_mutex);

  std::string path(OVMS_CONFIGPATH);
  path.append("/");
  path.append(m_name);
  // ESP_LOGI(TAG, "Trying %s",path.c_str());
  FILE* f = fopen(path.c_str(), "r");
  if (f)
    {
    char* buf = new char[OVMS_MAXVALSIZE];
    while (fgets(buf, OVMS_MAXVALSIZE, f))
      {
      buf[strlen(buf)-1] = 0; // Remove trailing newline
#ifdef OVMS_PERSIST_METADATA
      // check for meta data:
      if (buf[0] == '#')
        {
        if (strncmp(buf, "#access=", 8) == 0)
          {
          m_readable = (strchr(buf+8, 'r') != NULL);
          m_writable = (strchr(buf+8, 'w') != NULL);
          continue;
          }
        else if (strncmp(buf, "#title=", 7) == 0)
          {
          m_title = buf+7;
          continue;
          }
        }
#endif // OVMS_PERSIST_METADATA
      // read instance:
      char *p = index(buf,char(9));
      if (p == NULL) p = index(buf,' ');
      if (p)
        {
        *p = 0; // Null terminate the key
        p++;    // and point to the value
        m_instances[std::string(buf)] = std::string(p);
        // ESP_LOGI(TAG, "Loaded %s/%s=%s", m_name.c_str(), buf, p);
        }
      }
    delete[] buf;
    fclose(f);
    }
  m_loaded = true;
  }

void OvmsConfigParam::SetValue(std::string instance, std::string value)
  {
  auto lock = MyConfig.Lock();
  if (m_instances.find(instance) == m_instances.end() || m_instances[instance] != value)
    {
    m_instances[instance] = value;
    Save();
    }
  }

void OvmsConfigParam::DeleteConfig()
  {
  // Att: must only be called by OvmsConfig::DeregisterParam(), never directly!
  std::string path(OVMS_CONFIGPATH);
  path.append("/");
  path.append(m_name);
  unlink(path.c_str());
  }

bool OvmsConfigParam::DeleteInstance(std::string instance)
  {
  bool ret = false;
  auto lock = MyConfig.Lock();
  auto k = m_instances.find(instance);
  if (k != m_instances.end())
    {
    m_instances.erase(k);
    Save();
    ret = true;
    }
  return ret;
  }

std::string OvmsConfigParam::GetValue(std::string instance)
  {
  auto lock = MyConfig.Lock();
  auto k = m_instances.find(instance);
  if (k == m_instances.end())
    return std::string("");
  else
    return k->second;
  }

bool OvmsConfigParam::IsDefined(std::string instance)
  {
  auto lock = MyConfig.Lock();
  if (instance.empty())
    return !m_instances.empty();
  auto k = m_instances.find(instance);
  if (k == m_instances.end())
    return false;
  else
    return true;
  }

bool OvmsConfigParam::Writable()
  {
  return m_writable;
  }

bool OvmsConfigParam::Readable()
  {
  return m_readable;
  }

void OvmsConfigParam::SetAccess(bool writable, bool readable)
  {
  m_writable = writable;
  m_readable = readable;
  }

std::string OvmsConfigParam::GetName()
  {
  return m_name;
  }

void OvmsConfigParam::RewriteConfig()
  {
  std::string path(OVMS_CONFIGPATH);
  path.append("/");
  path.append(m_name);
  FILE* f = fopen(path.c_str(), "w");
  if (!f)
    ESP_LOGE(TAG, "RewriteConfig: can't open '%s': %s", path.c_str(), strerror(errno));
  else
    {
#ifdef OVMS_PERSIST_METADATA
    // write meta data:
    fprintf(f, "#access=%s%s\n", m_readable ? "r" : "", m_writable ? "w" : "");
    fprintf(f, "#title=%s\n", m_title.c_str());
#endif
    // write instances:
    for (ConfigParamMap::iterator it=m_instances.begin(); it!=m_instances.end(); ++it)
      {
      fprintf(f,"%s\t%s\n",it->first.c_str(),it->second.c_str());
      }
    if (fclose(f))
      ESP_LOGE(TAG, "RewriteConfig: error writing '%s': %s", path.c_str(), strerror(errno));
    }
  }

void OvmsConfigParam::Load()
  {
  if (!m_loaded) LoadConfig();
  }

void OvmsConfigParam::Save()
  {
  if (m_name != "")
    {
    auto lock = MyConfig.Lock();
    MyConfig.TransactionAdd(this, OvmsConfig::TransOp::Save);
    }
  }

/**
 * SetMap: replace all param instances
 * - Note: items will be removed from source map, map is empty afterwards
 */
void OvmsConfigParam::SetMap(ConfigParamMap& map)
  {
  auto lock = MyConfig.Lock();
  if (!map_equal(map, m_instances))
    {
    m_instances.clear();
    m_instances = std::move(map);
    Save();
    }
  else
    {
    map.clear();
    }
  }
