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

#include "esp_log.h"
static const char *TAG = "vfs";

#include <string>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#include "ovms_vfs.h"
#include "ovms_config.h"
#include "ovms_command.h"

void vfs_ls(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  DIR *dir;
  struct dirent *dp;
  if (argc == 0)
    {
    if ((dir = opendir (".")) == NULL)
      {
      writer->puts("Error: VFS cannot open directory listing");
      return;
      }
    }
  else
    {
    if (MyConfig.ProtectedPath(argv[0]))
      {
      writer->puts("Error: protected path");
      return;
      }
    if ((dir = opendir (argv[0])) == NULL)
      {
      writer->puts("Error: VFS cannot open directory listing for that directory");
      return;
      }
    }

  while ((dp = readdir (dir)) != NULL)
    {
    writer->puts(dp->d_name);
    }

  closedir(dir);
  }

void vfs_cat(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyConfig.ProtectedPath(argv[0]))
    {
    writer->puts("Error: protected path");
    return;
    }

  FILE* f = fopen(argv[0], "r");
  if (f == NULL)
    {
    writer->puts("Error: VFS file cannot be opened");
    return;
    }

  char buf[512];
  while(size_t n = fread(buf, sizeof(char), sizeof(buf), f))
    {
    writer->write(buf, n);
    }
  fclose(f);
  }

void vfs_rm(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyConfig.ProtectedPath(argv[0]))
    {
    writer->puts("Error: protected path");
    return;
    }

  if (unlink(argv[0]) == 0)
    { writer->puts("VFS File deleted"); }
  else
    { writer->puts("Error: Could not delete VFS file"); }
  }

void vfs_mv(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyConfig.ProtectedPath(argv[0]))
    {
    writer->puts("Error: protected path");
    return;
    }
  if (MyConfig.ProtectedPath(argv[1]))
    {
    writer->puts("Error: protected path");
    return;
    }
  if (rename(argv[0],argv[1]) == 0)
    { writer->puts("VFS File renamed"); }
  else
    { writer->puts("Error: Could not rename VFS file"); }
  }

void vfs_mkdir(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyConfig.ProtectedPath(argv[0]))
    {
    writer->puts("Error: protected path");
    return;
    }

  if (mkdir(argv[0],0) == 0)
    { writer->puts("VFS directory created"); }
  else
    { writer->puts("Error: Could not create VFS directory"); }
  }

void vfs_rmdir(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyConfig.ProtectedPath(argv[0]))
    {
    writer->puts("Error: protected path");
    return;
    }

  if (rmdir(argv[0]) == 0)
    { writer->puts("VFS directory removed"); }
  else
    { writer->puts("Error: Could not remove VFS directory"); }
  }

void vfs_cp(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyConfig.ProtectedPath(argv[0]))
    {
    writer->puts("Error: protected path");
    return;
    }
  if (MyConfig.ProtectedPath(argv[1]))
    {
    writer->puts("Error: protected path");
    return;
    }

  FILE* f = fopen(argv[0], "r");
  if (f == NULL)
    {
    writer->puts("Error: VFS source file cannot be opened");
    return;
    }

  FILE* w = fopen(argv[1], "w");
  if (w == NULL)
    {
    writer->puts("Error: VFS target file cannot be opened");
    fclose(f);
    return;
    }

  char buf[512];
  while(size_t n = fread(buf, sizeof(char), sizeof(buf), f))
    {
    fwrite(buf,n,1,w);
    }
  fclose(w);
  fclose(f);
  writer->puts("VFS copy complete");
  }

void vfs_append(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyConfig.ProtectedPath(argv[1]))
    {
    writer->puts("Error: protected path");
    return;
    }

  FILE* w = fopen(argv[1], "a");
  if (w == NULL)
    {
    writer->puts("Error: VFS target file cannot be opened");
    return;
    }

  int len = strlen(argv[0]);
  fwrite(argv[0], len, 1, w);
  fwrite("\n", 1, 1, w);
  fclose(w);
  }

class VfsInit
  {
  public: VfsInit();
} MyVfsInit  __attribute__ ((init_priority (5200)));

VfsInit::VfsInit()
  {
  ESP_LOGI(TAG, "Initialising VFS (5200)");

  OvmsCommand* cmd_vfs = MyCommandApp.RegisterCommand("vfs","VFS framework",NULL,"<$C> <file(s)>");
  cmd_vfs->RegisterCommand("ls","VFS Directory Listing",vfs_ls, "[file]", 0, 1, true);
  cmd_vfs->RegisterCommand("cat","VFS Display a file",vfs_cat, "<file>", 1, 1, true);
  cmd_vfs->RegisterCommand("mkdir","VFS Create a directory",vfs_mkdir, "<path>", 1, 1, true);
  cmd_vfs->RegisterCommand("rmdir","VFS Delete a directory",vfs_rmdir, "<path>", 1, 1, true);
  cmd_vfs->RegisterCommand("rm","VFS Delete a file",vfs_rm, "<file>", 1, 1, true);
  cmd_vfs->RegisterCommand("mv","VFS Rename a file",vfs_mv, "<source> <target>", 2, 2, true);
  cmd_vfs->RegisterCommand("cp","VFS Copy a file",vfs_cp, "<source> <target>", 2, 2, true);
  cmd_vfs->RegisterCommand("append","VFS Append a line to a file",vfs_append, "<quoted line> <file>", 2, 2, true);
  }
