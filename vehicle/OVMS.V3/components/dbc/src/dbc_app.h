/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
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

#ifndef __DBC_APP_H__
#define __DBC_APP_H__

#include "dbc.h"
#include "ovms_command.h"
#include "ovms_mutex.h"
#include "ovms_utils.h"
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
#include "duk_config.h"
#include "duktape.h"
#endif

typedef std::map<std::string, dbcfile*> dbcLoadedFiles_t;

class dbc
  {
  public:
    dbc();
    ~dbc();

  public:
    bool LoadFile(const char* name, const char* path);
    dbcfile* LoadString(const char* name, const char* content);
    bool Unload(const char* name);
    void LoadDirectory(const char* path, bool log=false);
    void LoadAutoExtras(bool log=false);
    dbcfile* Find(const char* name);

  protected:
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    static duk_ret_t DukOvmsDBCLoad(duk_context *ctx);
    static duk_ret_t DukOvmsDBCUnload(duk_context *ctx);
    static duk_ret_t DukOvmsDBCGet(duk_context *ctx);
#endif
  public:
    bool SelectFile(dbcfile* select);
    bool SelectFile(const char* name);
    void DeselectFile();
    dbcfile* SelectedFile();

    bool ExpandComplete(OvmsWriter* writer, const char *token, bool complete);

  public:
    void AutoInit();

  public:
    OvmsMutex m_mutex;
    dbcLoadedFiles_t m_dbclist;
    dbcfile* m_selected;
  };

extern dbc MyDBC;

#endif //#ifndef __DBC_APP_H__
