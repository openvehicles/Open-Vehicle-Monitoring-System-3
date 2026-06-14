/*
;    Project:       Open Vehicle Monitor System
;    Date:          7th March 2026
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2026       Michael Balzer
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
#ifndef __BACKGROUND_SHELL_H__
#define __BACKGROUND_SHELL_H__

#include "ovms.h"
#include "ovms_shell.h"
#include "task_base.h"


/**
 * BackgroundShell: run commands in a dedicated task, send output via notify framework
 * 
 * Usage:
 *  - construct a BackgroundShell instance
 *  - optionally call SetSecure(), SetNotifyMode() as required
 *  - call Init(), check result (false = out of memory)
 *  - send commands/input via QueueChars()/QueueChar()
 *  - send command "exit" to terminate the task and delete the BackgroundShell instance
 * 
 * ATT: DO NOT delete the BackgroundShell instance after a successful Init()!
 *  If you need to terminate the shell without sending "exit", call `Kill()` (inheritedt from TaskBase),
 *  but be aware this kills the task unconditionally, potentially leaking memory and/or other resources.
 *  `Kill()` deletes the BackgroundShell instance, so be sure to clear your references.
 * 
 * See `ExecuteCommand()` for simplified usage, see `ovms_command::cmd_run()` for the user shell API.
 * 
 * Set log level verbose on component 'bgshell' to enable command output logging.
 * 
 * The complete output of a command will be sent as a text notification if notify mode is 'i' (default),
 *  with notify mode 's', command output lines will be streamed instead.
 * Notification subtype scheme: "cmd.full.command.name"
 * 
 * Notes:
 *  - if proving useful, extending this to support other ways to retrieve the output would be fairly simple;
 *    i.e. add an API call to set an output processor function expecting the command name & output, call that
 *    function from `Execute()`
 */

class BackgroundShell : public OvmsShell, public TaskBase
  {
  public:
    BackgroundShell(std::string taskname, int verbosity=COMMAND_RESULT_VERBOSE,
      int stack=CONFIG_OVMS_SYS_COMMAND_STACK_SIZE, UBaseType_t priority=CONFIG_OVMS_SYS_COMMAND_PRIORITY,
      int queuesize=25);
    virtual ~BackgroundShell();

  public:
    void SetNotifyMode(char mode) { m_notify_mode = mode; }
    bool Init();
    void QueueChar(char c);
    void QueueChars(const char* buf, int len=-1);

  // OvmsWriter:
  public:
    virtual int puts(const char* s);
    virtual int printf(const char* fmt, ...) __attribute__ ((format (printf, 2, 3)));
    virtual ssize_t write(const void *buf, size_t nbyte);
    virtual bool IsInteractive() { return false; }
    virtual void Exit();

  // OvmsShell:
  public:
    virtual void SetCommand(OvmsCommand* cmd);
    virtual void Execute(int argc, const char * const * argv);

  // TaskBase:
  protected:
    virtual void Service();
    virtual void Cleanup();

  // BackgroundShell:
  protected:
    void ProcessOutputLines(bool finish);

  public:
    template <class string_t>
    static bool ExecuteCommand(const string_t command, bool secure=false, int verbosity=COMMAND_RESULT_VERBOSE)
      {
      BackgroundShell* bs = new BackgroundShell(command, verbosity);
      if (!bs) return false;
      bs->SetSecure(secure);
      if (!bs->Init()) return false;
      bs->QueueChars(command.data(), command.size());
      bs->QueueChars("\nexit\n");
      return true;
      }

  protected:
    // need to buffer the task name, as the caller context may be gone
    std::string                   m_taskname;

    // input queue, payload: C string (const char*)
    int                           m_queuesize;
    QueueHandle_t                 m_queue;

    // output buffer
    extram::string                m_output;
    extram::string::size_type     m_linestart;

    // command info:
    std::string                   m_cmd_fullname;
    std::string                   m_cmd_subtype;

    // control:
    bool                          m_exit = false;
    char                          m_notify_mode = 'i';
  };

#endif //#ifndef __BACKGROUND_SHELL_H__
