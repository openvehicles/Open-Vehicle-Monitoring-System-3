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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "string"
#include "map"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include "ovms_mutex.h"
#include "ovms_command.h"

class OvmsConfig;
extern OvmsConfig MyConfig;

typedef enum
  {
  Encoding_HEX = 0,
  Encoding_BASE64
  } BinaryEncoding_t;


/**
 * class ConfigParamMap: std::map<instance,value> with typed & defaulting setters & getters
 * 
 * This is the in-memory key-value storage of an OvmsConfigParam.
 * 
 * Use GetParamMap/SetParamMap or GetMap/SetMap to access or replace an OvmsConfigParam's
 *   map in case you need to loop over the instances or perform multiple updates, or if
 *   working with a std::map is convenient to simplify your code.
 * 
 * NOTE: you *must* hold a config lock when & while accessing the map by reference!
 *   (This only isn't necessary when retrieving a copy of the map or passing a newly
 *    constructed map to SetParamMap/SetMap)
 * 
 * Accessing a ConfigParamMap via the OvmsConfig or OvmsConfigParam API automatically
 *   takes care of setting the lock. You can speed up multiple config changes by
 *   acquiring the lock for the whole series.
 * 
 * On default values: GetValue() is a native lookup, the default only applies on undefined
 *   instances. All other (typed) getters apply defaults when the instance is undefined,
 *   or when the instance value is an empty string ("").
 * 
 * Setters and DeleteInstance() return true if the map has been changed by the call.
 * 
 * Getters do not change the map (i.e. do not add a missing instance like operator[] does).
 */
class ConfigParamMap : public NameMap<std::string>
  {
  public:
    bool IsDefined(std::string instance);

    bool SetValue(std::string instance, std::string value);
    bool SetValueInt(std::string instance, int value);
    bool SetValueFloat(std::string instance, float value);
    bool SetValueBool(std::string instance, bool value);
    bool SetValueBinary(std::string instance, std::string value, BinaryEncoding_t encoding=Encoding_HEX);

    std::string GetValue(std::string instance, std::string defvalue = "");
    int GetValueInt(std::string instance, int defvalue = 0);
    float GetValueFloat(std::string instance, float defvalue = 0);
    bool GetValueBool(std::string instance, bool defvalue = false);
    std::string GetValueBinary(std::string instance, std::string defvalue = "", BinaryEncoding_t encoding=Encoding_HEX);

    bool DeleteInstance(std::string instance);
  };


/**
 * class OvmsConfigParam: cached & persistent key-value storage of a config parameter
 * 
 * Each config parameter maintains a cached (in-memory) storage and a persistent storage
 *  in form of a file in the config store, path '/store/ovms_config/<param>'.
 * 
 * Event 'config.changed' is emitted on changing instances or deleting the param.
 *   Listeners callbacks get the pointer to the changed OvmsConfigParam (data).
 * 
 * Updating a config param via the OvmsConfig or OvmsConfigParam API automatically
 *   acquires the global config lock. To speed up multiple config changes in series,
 *   get the lock for your function/block doing the changes, or use the map API.
 *   All changes with the lock held are collected and compressed into single file
 *   updates on releasing the lock.
 */
class OvmsConfigParam
  {
  friend class OvmsConfig;

  public:
    OvmsConfigParam(std::string name, std::string title, bool writable, bool readable);
    ~OvmsConfigParam();

  public:
    bool IsDefined(std::string instance);

    void SetValue(std::string instance, std::string value);
    void SetValueInt(std::string instance, int value);
    void SetValueFloat(std::string instance, float value);
    void SetValueBool(std::string instance, bool value);
    void SetValueBinary(std::string instance, std::string value, BinaryEncoding_t encoding=Encoding_HEX);

    std::string GetValue(std::string instance, std::string defvalue = "");
    int GetValueInt(std::string instance, int defvalue = 0);
    float GetValueFloat(std::string instance, float defvalue = 0);
    bool GetValueBool(std::string instance, bool defvalue = false);
    std::string GetValueBinary(std::string instance, std::string defvalue = "", BinaryEncoding_t encoding=Encoding_HEX);

    bool DeleteInstance(std::string instance);

    const ConfigParamMap& GetMap() { return m_instances; }
    void SetMap(ConfigParamMap& map);

    bool Writable();
    bool Readable();
    std::string GetName();
    const char* GetTitle() { return m_title.c_str(); }

  protected:
    void SetAccess(bool writable, bool readable);
    void SetTitle(std::string title) { m_title = title; }
    void Load();
    void Save();

  protected:
    void DeleteConfig();
    void RewriteConfig();
    void LoadConfig();

  protected:
    std::string m_name;
    std::string m_title;
    bool m_writable;
    bool m_readable;
    bool m_loaded;

  public:
    ConfigParamMap m_instances;
  };


/**
 * class OvmsConfig: main config API (singleton: MyConfig)
 * 
 * On config locks/transactions:
 * 
 * Any direct access to the param map or any instance map MUST aquire a lock to prevent race conditions
 * by concurrent writes / deletions. MyConfig main API calls are safe to use without an explicit lock
 * (they use implicit locking), but be aware setters will then do a full rewrite on each change
 * (i.e. slow and wearing the flash memory).
 * 
 * To avoid this, either use the map getters & setters, or encapsulate all blocks of multiple config
 * changes in series in a transactional block. Within a transaction, all actual storage operations and
 * signals are postponed to the commit, to avoid repeated flash file rewrites & `config.changed` events
 * for the same param.
 * 
 * Usage:
 *    {
 *    auto lock = MyConfig.Lock();
 *    … perform changes …
 *    }
 * 
 * Nesting locks is allowed (within the same task), the final (outer) release will do the commit.
 * When trying to lock (short timeout), use `lock.IsLocked()` to query the success.
 * 
 * Note: config event listeners (except scripts) are called within a config transaction already
 *   created by the event framework, so do not need to create their own.
 */

typedef NameMap<OvmsConfigParam*> ConfigMap;

class OvmsConfig
  {
  friend class OvmsConfigParam;

  public:
    OvmsConfig();
    ~OvmsConfig();

  public:
    class Transaction : public OvmsRecMutexLock
      {
      public:
        Transaction(OvmsRecMutex* mutex, TickType_t timeout=portMAX_DELAY);
        Transaction(const Transaction& src) = delete; // copying not allowed
        Transaction& operator=(const Transaction& src) = delete;
        Transaction(Transaction&& src);
        ~Transaction();
      };
    Transaction Lock(TickType_t timeout=portMAX_DELAY) { return Transaction(&m_mutex, timeout); }
    Transaction* CreateLock(TickType_t timeout=portMAX_DELAY) { return new Transaction(&m_mutex, timeout); }

  protected:
    typedef enum
      {
      Save = 0,
      Delete,
      } TransOp;
    void TransactionAdd(OvmsConfigParam* param, TransOp todo);
    void TransactionCommit();

  public:
    void RegisterParam(std::string name, std::string title, bool writable=true, bool readable=true);
    void DeregisterParam(std::string name);

  public:
    bool IsDefined(std::string param, std::string instance);

    void SetParamValue(std::string param, std::string instance, std::string value);
    void SetParamValueInt(std::string param, std::string instance, int value);
    void SetParamValueFloat(std::string param, std::string instance, float value);
    void SetParamValueBool(std::string param, std::string instance, bool value);
    void SetParamValueBinary(std::string param, std::string instance, std::string value, BinaryEncoding_t encoding=Encoding_HEX);

    std::string GetParamValue(std::string param, std::string instance, std::string defvalue = "");
    int GetParamValueInt(std::string param, std::string instance, int defvalue = 0);
    float GetParamValueFloat(std::string param, std::string instance, float defvalue = 0);
    bool GetParamValueBool(std::string param, std::string instance, bool defvalue = false);
    std::string GetParamValueBinary(std::string param, std::string instance, std::string defvalue = "", BinaryEncoding_t encoding=Encoding_HEX);

    void DeleteInstance(std::string param, std::string instance);

    OvmsConfigParam* CachedParam(std::string param);
    ConfigParamMap GetParamMap(std::string param);
    void SetParamMap(std::string param, ConfigParamMap& map);

  public:
    bool ProtectedPath(std::string path);

#ifdef CONFIG_OVMS_SC_ZIP
  public:
    bool Backup(std::string path, std::string password, OvmsWriter* writer=NULL, int verbosity=1024);
    bool Restore(std::string path, std::string password, OvmsWriter* writer=NULL, int verbosity=1024);
#endif // CONFIG_OVMS_SC_ZIP

  public:
    esp_err_t mount();
    esp_err_t unmount();
    bool ismounted();

  public:
    void SupportSummary(OvmsWriter* writer);

  protected:
    void upgrade();

  protected:
    bool m_mounted = false;
    esp_vfs_fat_mount_config_t m_store_fat;
    wl_handle_t m_store_wlh;

  private:
    OvmsRecMutex m_mutex;                                 // config cache/transactional access
    OvmsMutex m_store_mutex;                              // config file storage access
    std::map<OvmsConfigParam*, TransOp> m_transaction;    // deferred transactional operations

  public:
    ConfigMap m_params;
  };


#endif //#ifndef __CONFIG_H__
