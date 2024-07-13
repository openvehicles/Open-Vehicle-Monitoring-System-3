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
#include "glob_match.h"

#ifdef CONFIG_OVMS_COMP_EDITOR
#include "vfsedit.h"
#endif // #ifdef CONFIG_OVMS_COMP_EDITOR

/**
 * This class is a kind of wrapper around dirent
 */
class Direntry {
  public:
    std::string basepath; // base path of the file / directory (parent dir) - always ending with '/'
    std::string name;     // name of the file / dirctory
    bool is_dir;          // `true`: is a directory, `false`: is a file
    bool is_protected;    // `true`: this path is protected
    bool is_skip;         // `true`: we should not analyze this entry
    int64_t size;         // size
    time_t mtime;         // modification time

    /**
     * Default constructor, calls stat() on the file.
     */
    Direntry(std::string basepath, struct dirent *direntry, bool dostat = true) {
      struct stat st;
      this->basepath = basepath + '/';
      name = direntry->d_name;
      is_protected = MyConfig.ProtectedPath(path());
      if (dostat) {
        stat(path().c_str(), &st);
        is_dir = S_ISDIR(st.st_mode);
        is_skip = (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) || (direntry->d_name[0] == '.');
        size = st.st_size;
        mtime = st.st_mtime;
      } else {
        is_dir = direntry->d_type == DT_DIR;
        is_skip = (!is_dir && (direntry->d_type != DT_REG)) || (direntry->d_name[0] == '.');
        size = 0;
        mtime = 0;
      }
    }

    std::string path() const {
      return basepath + name;
    }

    bool operator<(const Direntry &other) const {
      static std::less<std::string> const less;
      return less(path(), other.path());
    }

    void display(OvmsWriter* writer, bool show_full_path) {
      char bufsize[64], mod[64];

      if (is_dir) {
        if (is_protected) {
          strcpy(bufsize, "[P][DIR]");
        } else {
          strcpy(bufsize, "[DIR]   ");
        }
      } else {
        if (is_protected) {
          strcpy(bufsize, "[P]     ");
        } else {
          format_file_size(bufsize, sizeof(bufsize), size);
        }
      }
      struct tm timeinfo;
      strftime(mod, sizeof(mod), "%d-%b-%Y %H:%M", localtime_r(&mtime, &timeinfo));

      const char *slash = is_dir ? "/" : "";

      writer->printf("%8.8s  %17.17s  %s%s\n", bufsize, mod, show_full_path ? path().c_str() : name.c_str(), slash);
    }
};

/**
 * Fills a `std::set<Direntry>` with all possible entries starting at `startpath`.
 * if `recurse` is `true`, then all possible directories are recursively entered.
 */
static void read_entries(std::set<Direntry> &entries, std::string startpath, std::string glob, bool recurse = true, bool dostat = true) {
  if (recurse && (startpath.empty() || startpath == "/") )
    {
    read_entries(entries, "/store", glob, recurse, dostat);
#ifdef CONFIG_OVMS_COMP_SDCARD
    read_entries(entries, "/sd", glob, recurse, dostat);
#endif
    return;
    }
  DIR *dir = opendir(startpath.c_str());
  if (!dir && glob.empty()) {
    // not a path - see if we can add it as a glob.
    auto posn = startpath.rfind('/');
    if (posn != std::string::npos) {
      glob.assign(startpath, posn+1, std::string::npos);
      startpath.erase(posn, std::string::npos);
      if (startpath.empty()) {
        if (recurse)
          read_entries(entries, startpath, glob, recurse, dostat);
        return;
      }

      dir = opendir(startpath.c_str());
    }
  }
  if (dir != NULL) {
    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
      if (!glob.empty()) {
        if (!glob_match(glob.c_str(), dp->d_name)){
          if (recurse && dp->d_type == DT_DIR) {
            auto path = startpath;
            if (path.back() != '/')
              path += '/';
            path += dp->d_name;
            read_entries(entries, path, glob, recurse, dostat);
          }
          continue;
        }
      }
      Direntry dirent(startpath, dp, dostat);
      if (dirent.is_skip) {
        continue;
      }
      entries.insert(dirent);
      if (recurse && dirent.is_dir && !dirent.is_protected) {
        read_entries(entries, dirent.path(), glob, recurse, dostat);
      }
    }
    closedir(dir);
  }
}

static bool has_glob_end(const std::string &str) {

  for (auto it = str.rbegin(); it != str.rend(); ++it) {
    switch (*it) {
      case '?':
      case '*':
          return true;
          break;
      case '/':
          return false;
      default:
          break;
    }
  }
  return false;
}

// Splits off the filename part of the string.
static void split_filename(std::string &path, std::string &filepart) {
  int idx = path.rfind('/');
  if (idx < 0) {
    filepart = path;
    path = "";
  }
  else {
    filepart.assign(path, idx+1, std::string::npos);
    path.erase(idx, std::string::npos);
  }
}

/**
 * Fills a `std::set<Direntry>` with all possible entries, and display the sorted result.
 * if `recurse` is `true`:
 * - all possible directories are recursively entered,
 * - the display shows the full file path
 * if `recurse` is `false`:
 * - only `startpath` is listed,
 * - the display shows only file / directory name
 */
static void list_entries(OvmsWriter* writer, std::string startpath, bool recurse = true) {

  std::set<Direntry> entries;
  while((startpath.back() == '/') || (startpath.back() == '.')) {
    startpath.pop_back();
  }

  if (MyConfig.ProtectedPath(startpath)) {
    writer->puts("Error: protected path");
    return;
  }
  std::string glob;

  if (has_glob_end(startpath)) {
    split_filename(startpath, glob);
  }

  read_entries(entries, startpath, glob, recurse);

  for (auto it = entries.begin(); it != entries.end(); it++) {
    Direntry dirent = *it;
    dirent.display(writer, recurse);
  }
}

/**
 * Recursive listing of the files and directories starting at `argv[0]` (or '.')
 * - all possible directories are recursively entered,
 * - the display shows the full file / directory path
 * - protected directories are identified and not entered
 */
void vfs_rls(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  list_entries(writer, (argc==0) ? "." : argv[0], true);
}

/**
 * Listing of the files and directories at `argv[0]` (or '.')
 * - the display shows only the file  / directory name
 * - protected directories are identified
 */
void vfs_ls(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  list_entries(writer, (argc==0) ? "." : argv[0], false);
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
      {
      char* ep;
      int32_t v = strtol(argv[i] + 1, &ep, 10);
      if (*ep != '\0')
        {
        cmd->PutUsage(writer);
        return;
        }
      nrlines = v;
      }
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
  std::string filename(argv[0]);
  if (!has_glob_end(filename)) // Only match multiple files if there is a glob expression.
    {
    if (MyConfig.ProtectedPath(argv[0]))
      {
      writer->puts("Error: protected path");
      return;
      }
    if (unlink(filename.c_str()) == 0)
      { writer->puts("VFS File deleted"); }
    else
      { writer->puts("Error: Could not delete VFS file"); }
    }
  else
    {
    std::string glob;
    split_filename(filename, glob);
    if (MyConfig.ProtectedPath(filename.c_str()))
      {
      writer->puts("Error: protected path");
      return;
      }

    std::set<Direntry> entries;
    // read without stat.
    read_entries(entries, filename, glob, false, false);
    if (entries.size() == 0)
      {
      writer->puts("VFS: No matching files");
      return;
      }

    int delcount = 0;
    for (auto it = entries.begin(); it != entries.end(); ++it)
      {
      const std::string &path = it->path();
      if (unlink(path.c_str()) == 0)
        ++delcount;
      else
        writer->printf("Error: Could not delete VFS file '%s'", path.c_str());
      }

    if (delcount > 0)
      writer->printf("VFS: Deleted %d files\n", delcount );
    }
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
  const char *dirpath = NULL;
  bool parents = false;

  for (int i = 0; i < argc; i++)
    {
    if (strcmp(argv[i], "-p") == 0)
      parents = true;
    else
      dirpath = argv[i];
    }

  if (!dirpath)
    {
    writer->puts("Error: no path specified");
    return;
    }
  if (MyConfig.ProtectedPath(dirpath))
    {
    writer->puts("Error: protected path");
    return;
    }

  int res = (parents) ? mkpath(dirpath,0) : mkdir(dirpath,0);

  if (res == 0)
    { writer->puts("VFS directory created"); }
  else
    { writer->puts("Error: Could not create VFS directory"); }
  }

void vfs_rmdir(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char *dirpath = NULL;
  bool recursive = false;

  for (int i = 0; i < argc; i++)
    {
    if (strcmp(argv[i], "-r") == 0)
      recursive = true;
    else
      dirpath = argv[i];
    }

  if (!dirpath)
    {
    writer->puts("Error: no path specified");
    return;
    }
  if (MyConfig.ProtectedPath(dirpath))
    {
    writer->puts("Error: protected path");
    return;
    }

  int res = (recursive) ? rmtree(dirpath) : rmdir(dirpath);

  if (res == 0)
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
          {
          char* ep;
          int32_t v = strtol(argv[i] + 1, &ep, 10);
          if (*ep != '\0')
            {
            cmd->PutUsage(writer);
            return OCS_Error;
            }
          nrlines = v;
          }
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
  cmd_vfs->RegisterCommand("rls","VFS Recursive Directory Listing",vfs_rls, "[<file>]", 0, 1);
  cmd_vfs->RegisterCommand("cat","VFS Display a file",vfs_cat, "<file>", 1, 1);
  cmd_vfs->RegisterCommand("head","VFS Display first 20 lines of a file",vfs_head, "[-nrlines] <file>", 1, 2);
  cmd_vfs->RegisterCommand("stat","VFS Status of a file",vfs_stat, "<file>", 1, 1);
  cmd_vfs->RegisterCommand("mkdir","VFS Create a directory",vfs_mkdir, "[-p] <path>\n"
    "-p = create full path including missing parents", 1, 2);
  cmd_vfs->RegisterCommand("rmdir","VFS Delete a directory",vfs_rmdir, "[-r] <path>\n"
    "-r = delete recursively including all files and subdirectories", 1, 2);
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
