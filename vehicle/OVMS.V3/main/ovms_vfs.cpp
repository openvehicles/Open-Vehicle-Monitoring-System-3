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
static const char *TAG = "vfs";

#include <string>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#include "ovms_vfs.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_peripherals.h"
#include "crypt_md5.h"

#ifdef CONFIG_OVMS_COMP_EDITOR
#include "vfsedit.h"
#endif // #ifdef CONFIG_OVMS_COMP_EDITOR

void vfs_ls(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  DIR *dir;
  struct dirent *dp;
  char size[64], mod[64], path[PATH_MAX];
  struct stat st;

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
    snprintf(path, sizeof(path), "%s/%s", (argc==0) ? "." : argv[0], dp->d_name);
    stat(path, &st);

    int64_t fsize = st.st_size;
    int is_dir = S_ISDIR(st.st_mode);
    const char *slash = is_dir ? "/" : "";

    if (is_dir) {
      strcpy(size, "[DIR]   ");
    } else {
      if (fsize < 1024) {
        snprintf(size, sizeof(size), "%d ", (int) fsize);
      } else if (fsize < 0x100000) {
        snprintf(size, sizeof(size), "%.1fk", (double) fsize / 1024.0);
      } else if (fsize < 0x40000000) {
        snprintf(size, sizeof(size), "%.1fM", (double) fsize / 1048576);
      } else {
        snprintf(size, sizeof(size), "%.1fG", (double) fsize / 1073741824);
      }
    }
    strftime(mod, sizeof(mod), "%d-%b-%Y %H:%M", localtime(&st.st_mtime));

    writer->printf("%8.8s  %17.17s  %s%s\n", size, mod, dp->d_name, slash);
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

void vfs_head(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char *filename = NULL;
  int nrlines = 20;

  for (int i=0; i<argc; i++)
    {
    if (argv[i][0] == '-')
      nrlines = atoi(argv[i]+1);
    else
      filename = argv[i];
    }

  if (!filename)
    {
    writer->puts("Error: no filename given");
    return;
    }
  if (MyConfig.ProtectedPath(filename))
    {
    writer->puts("Error: protected path");
    return;
    }

  FILE* f = fopen(filename, "r");
  if (f == NULL)
    {
    writer->puts("Error: VFS file cannot be opened");
    return;
    }

  char buf[512];
  int k =0;
  while (fgets(buf,sizeof(buf),f)!=NULL)
    {
    writer->write(buf,strlen(buf));
    if (++k >= nrlines) break;
    }
  fclose(f);
  }

void vfs_stat(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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

  OVMS_MD5_CTX* md5 = new OVMS_MD5_CTX;
  OVMS_MD5_Init(md5);
  int filesize = 0;
  uint8_t *buf = new uint8_t[512];
  while(size_t n = fread(buf, sizeof(char), 512, f))
    {
    filesize += n;
    OVMS_MD5_Update(md5, buf, n);
    }
  uint8_t* rmd5 = new uint8_t[16];
  OVMS_MD5_Final(rmd5, md5);

  char dchecksum[33];
  sprintf(dchecksum,"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    rmd5[0],rmd5[1],rmd5[2],rmd5[3],rmd5[4],rmd5[5],rmd5[6],rmd5[7],
    rmd5[8],rmd5[9],rmd5[10],rmd5[11],rmd5[12],rmd5[13],rmd5[14],rmd5[15]);
  writer->printf("File %s size is %d and digest %s\n",
    argv[0],filesize,dchecksum);

  delete [] rmd5;
  delete [] buf;
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


class VfsTailCommand : public OvmsCommandTask
  {
  using OvmsCommandTask::OvmsCommandTask;
  public:
    char* filename = NULL;
    int nrlines = 0;
    int fd = -1;
    char buf[128];
    off_t fpos;
    ssize_t len;

    OvmsCommandState_t Prepare()
      {
      // parse args:
      for (int i=0; i<argc; i++)
        {
        if (argv[i][0] == '-')
          nrlines = atoi(argv[i]+1);
        else
          filename = argv[i];
        }

      // check file:
      if (!filename || !*filename || MyConfig.ProtectedPath(filename))
        {
        writer->puts("Error: invalid/protected path");
        return OCS_Error;
        }
      fd = open(filename, O_RDONLY|O_NONBLOCK);
      if (fd == -1)
        {
        writer->puts("Error: VFS file cannot be opened");
        return OCS_Error;
        }

      // seek nrlines back from end of file:
      fpos = lseek(fd, 0, SEEK_END);
      int lcnt = ((nrlines > 0) ? nrlines : 10) + 1;
      while (fpos > 0 && lcnt > 0)
        {
        ssize_t rlen = MIN(fpos, sizeof(buf));
        fpos = lseek(fd, fpos-rlen, SEEK_SET);
        len = read(fd, buf, rlen);
        for (int i=len-1; i>=0; i--)
          {
          if (buf[i] == '\n' && (--lcnt == 0))
            {
            fpos += i + 1;
            break;
            }
          }
        }

      // determine run mode:
      if (nrlines <= 0 && writer->IsInteractive())
        {
        writer->puts("[tail: in follow mode, press Ctrl-C to abort]");
        return OCS_RunLoop;
        }
      return OCS_RunOnce;
      }

    void Service()
      {
      while (true)
        {
        // Note: the esp-idf libs do currently not provide read/seek into
        // data appended by other tasks, we need to reopen for each check
        if (fd == -1)
          fd = open(filename, O_RDONLY|O_NONBLOCK);
        if (fd == -1)
          {
          writer->puts("[tail: file lost, abort]");
          break;
          }
        lseek(fd, fpos, SEEK_SET);
        while ((len = read(fd, buf, sizeof buf)) > 0)
          writer->write(buf, len);
        fpos = lseek(fd, 0, SEEK_CUR);
        close(fd);
        fd = -1;

        // done/abort?
        if (m_state != OCS_RunLoop)
          break;

        vTaskDelay(pdMS_TO_TICKS(250));
        }
      }

    static void Execute(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
      {
      VfsTailCommand* me = new VfsTailCommand(verbosity, writer, cmd, argc, argv);
      if (me) me->Run();
      }
  };


void vfs_df(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  FATFS *fs;
  DWORD fre_clust;
  uint64_t fre_sect, tot_sect;
  int sector_size;
  uint64_t size, free;

  // drive "0:" = /store
  if (f_getfree("0:", &fre_clust, &fs) == FR_OK)
    {
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;
    #if FF_MAX_SS != FF_MIN_SS
      sector_size = fs->ssize;
    #else
      sector_size = 512;
    #endif
    size = ((uint64_t) tot_sect) * sector_size / 1024;
    free = ((uint64_t) fre_sect) * sector_size / 1024;
    writer->printf("/store:  Size: %6llu kB  Avail: %6llu kB  Used: %3d%%\n",
      size, free, (int)(100 - fre_sect * 100 / tot_sect));
    }
  else
    {
    writer->puts("ERROR: can't get df info for /store");
    }

#ifdef CONFIG_OVMS_COMP_SDCARD
  // drive "1:" = /sd
  sdmmc_card_t* card = MyPeripherals->m_sdcard->m_card;
  if (card && f_getfree("1:", &fre_clust, &fs) == FR_OK)
    {
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;
    sector_size = card->csd.sector_size;
    size = ((uint64_t) tot_sect) * sector_size / (1024 * 1024);
    free = ((uint64_t) fre_sect) * sector_size / (1024 * 1024);
    writer->printf("/sd   :  Size: %6llu MB  Avail: %6llu MB  Used: %3d%%\n",
      size, free, (int)(100 - fre_sect * 100 / tot_sect));
    }
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD
  }


class VfsInit
  {
  public: VfsInit();
} MyVfsInit  __attribute__ ((init_priority (5200)));

VfsInit::VfsInit()
  {
  ESP_LOGI(TAG, "Initialising VFS (5200)");

  OvmsCommand* cmd_vfs = MyCommandApp.RegisterCommand("vfs","Virtual File System framework");
  cmd_vfs->RegisterCommand("ls","VFS Directory Listing",vfs_ls, "[<file>]", 0, 1);
  cmd_vfs->RegisterCommand("cat","VFS Display a file",vfs_cat, "<file>", 1, 1);
  cmd_vfs->RegisterCommand("head","VFS Display first 20 lines of a file",vfs_head, "[-nrlines] <file>", 1, 2);
  cmd_vfs->RegisterCommand("stat","VFS Status of a file",vfs_stat, "<file>", 1, 1);
  cmd_vfs->RegisterCommand("mkdir","VFS Create a directory",vfs_mkdir, "<path>", 1, 1);
  cmd_vfs->RegisterCommand("rmdir","VFS Delete a directory",vfs_rmdir, "<path>", 1, 1);
  cmd_vfs->RegisterCommand("rm","VFS Delete a file",vfs_rm, "<file>", 1, 1);
  cmd_vfs->RegisterCommand("mv","VFS Rename a file",vfs_mv, "<source> <target>", 2, 2);
  cmd_vfs->RegisterCommand("cp","VFS Copy a file",vfs_cp, "<source> <target>", 2, 2);
  cmd_vfs->RegisterCommand("append","VFS Append a line to a file",vfs_append, "<quoted line> <file>", 2, 2);
  cmd_vfs->RegisterCommand("tail","VFS output tail of a file",VfsTailCommand::Execute, "[-nrlines] <file>", 1, 2);
  cmd_vfs->RegisterCommand("df","VFS show disk usage", vfs_df, "", 0, 0);
  #ifdef CONFIG_OVMS_COMP_EDITOR
  cmd_vfs->RegisterCommand("edit","VFS edit a file",vfs_edit, "<path>", 1, 1);
  #endif // #ifdef CONFIG_OVMS_COMP_EDITOR

  }
