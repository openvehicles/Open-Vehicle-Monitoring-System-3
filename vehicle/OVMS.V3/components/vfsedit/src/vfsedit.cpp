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

#include "vfsedit.h"
#include "openemacs.h"

size_t vfs_edit_write(struct editor_state* E, const char *buf, size_t nbyte)
  {
  OvmsWriter* writer = (OvmsWriter*)E->editor_userdata;

  return writer->write(buf,nbyte);
  }

bool vfs_edit_insert(OvmsWriter* writer, void* ctx, char ch)
  {
  struct editor_state* ed = (struct editor_state*)ctx;

  editor_process_keypress(ed, ch);
  if (ed->editor_completed)
    {
    editor_free(ed);
    free(ed);
    return false;
    }
  else
    {
    return true;
    }
  }

void vfs_edit(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyConfig.ProtectedPath(argv[0]))
    {
    writer->puts("Error: protected path");
    return;
    }

  struct editor_state* ed = (struct editor_state*)malloc(sizeof(struct editor_state));
  editor_init(ed,vfs_edit_write,(void*)writer);
  editor_open(ed,argv[0]);

  writer->RegisterInsertCallback(vfs_edit_insert, ed);
  }
