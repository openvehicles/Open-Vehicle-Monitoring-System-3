/**
 * Project:      Open Vehicle Monitor System
 * Module:       class ZipArchive: libzip wrapper
 * 
 * (c) 2018  Michael Balzer <dexter@dexters-web.de>
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

#include "zip_archive.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <vector>
#include "ovms_utils.h"


/**
 * ZipArchive: open/create ZIP archive
 */
ZipArchive::ZipArchive(const std::string& zippath, const std::string& password,
                       zip_flags_t flags /*=0*/,
                       zip_uint16_t encmethod /*=ZIP_EM_AES_256*/)
{
  m_password = password;
  m_encmethod = encmethod;
  m_errno = 0;
  m_basedir = "";
  m_zip = zip_open(zippath.c_str(), flags, &m_errno);
  if (m_zip)
    zip_set_default_password(m_zip, password.c_str());
}


/**
 * ~ZipArchive: discard & free ZIP archive if unclosed
 */
ZipArchive::~ZipArchive()
{
  if (m_zip)
    zip_discard(m_zip);
}


/**
 * close: write ZIP archive to disk, check for error
 */
bool ZipArchive::close()
{
  if (!m_zip)
    return (m_errno == 0);
  if (zip_close(m_zip) != 0)
    return false;
  m_zip = NULL;
  m_errno = 0;
  return true;
}


/**
 * ok: check status
 */
bool ZipArchive::ok()
{
  if (m_zip) {
    zip_error_t* e = zip_get_error(m_zip);
    if (e->zip_err != 0)
      return false;
  }
  return (m_errno != 0);
}

/**
 * strerror: retrieve error description
 */
const char* ZipArchive::strerror()
{
  if (m_zip) {
    zip_error_t* e = zip_get_error(m_zip);
    if (e->zip_err != 0)
      return zip_error_strerror(e);
  }
  return std::strerror(m_errno);
}


/**
 * chdir: set base directory
 */
bool ZipArchive::chdir(const std::string& path)
{
  struct stat st;
  
  if (stat(path.c_str(), &st)) {
    m_errno = errno;
    return false;
  }
  
  if (!S_ISDIR(st.st_mode)) {
    m_errno = ENOTDIR;
    return false;
  }
  
  m_basedir = path;
  if (!endsWith(m_basedir, '/'))
    m_basedir.append("/");
  
  return true;
}


/**
 * add: recursively add files & directories
 */
bool ZipArchive::add(std::string path, bool ignore_nonexist /*=false*/)
{
  struct stat st;
  std::string rpath;
  zip_int64_t idx;
  
  if (!m_zip)
    return false;
  
  if (startsWith(path, '/'))
    rpath = path;
  else
    rpath = m_basedir + path;
  
  if (stat(rpath.c_str(), &st)) {
    m_errno = errno;
    return (ignore_nonexist) ? true : false;
  }
  
  if (S_ISDIR(st.st_mode))
  {
    // add directory:
    if (!endsWith(path, '/'))
      path.append("/");
    if ((idx = zip_dir_add(m_zip, path.c_str(), 0)) < 0)
      return false;
    zip_file_set_mtime(m_zip, idx, st.st_mtime, 0);
    // zip_set_file_compression(m_zip, idx, ZIP_CM_STORE, 0);
    
    DIR *dir = opendir(rpath.c_str());
    if (!dir) {
      m_errno = errno;
      return false;
    }
    
    struct dirent *dp;
    bool ok = true;
    while ((dp = readdir(dir)) != NULL) {
      if (!add(path + dp->d_name)) {
        ok = false;
        break;
      }
    }
    
    closedir(dir);
    return ok;
  }
  else
  {
    // add file:
    zip_source_t* src = zip_source_file(m_zip, rpath.c_str(), 0, -1);
    if (!src)
      return false;

    zip_int64_t idx = zip_file_add(m_zip, path.c_str(), src, 0);
    if (idx < 0) {
      zip_source_free(src);
      return false;
    }
    zip_file_set_mtime(m_zip, idx, st.st_mtime, 0);
    // zip_set_file_compression(m_zip, idx, ZIP_CM_STORE, 0);

    if (m_encmethod != ZIP_EM_NONE && m_password.size() > 0
        && zip_file_set_encryption(m_zip, idx, m_encmethod, m_password.c_str()) < 0) {
      return false;
    }

    return true;
  }
}

/**
 * extract: extract files or directories matching prefix into current base directory
 */
bool ZipArchive::extract(const std::string prefix, bool ignore_nonexist /*=false*/)
{
  zip_int64_t idx, iend;
  const char* zname;
  std::string rpath;
  size_t sz;
  zip_file_t* file;
  FILE *fp;
  std::vector<uint8_t> buf;
  
  if (!m_zip) {
    m_errno = ENOENT;
    return false;
  }
  else if ((iend = zip_get_num_entries(m_zip, 0)) <= 0) {
    m_errno = ENOENT;
    return (ignore_nonexist) ? true : false;
  }
  
  m_errno = 0;
  buf.reserve(512);
  
  for (idx = 0; idx < iend; idx++)
  {
    // check prefix:
    if ((zname = zip_get_name(m_zip, idx, 0)) == NULL)
      continue;
    if (strncmp(zname, prefix.c_str(), prefix.length()) != 0)
      continue;
    
    // create path:
    rpath = m_basedir + zname;
    if ((sz = rpath.find_last_of('/')) != std::string::npos) {
      if (mkpath(rpath.substr(0, sz), 0) != 0) {
        m_errno = errno;
        return false;
      }
    }
    if (endsWith(rpath, '/'))
      continue;
    
    // extract file:
    if ((file = zip_fopen_index(m_zip, idx, 0)) == NULL)
      return false;
    if ((fp = fopen(rpath.c_str(), "w")) == NULL) {
      m_errno = errno;
      zip_fclose(file);
      return false;
    }
    while ((sz = zip_fread(file, buf.data(), buf.capacity())) > 0) {
      if (fwrite(buf.data(), 1, sz, fp) != sz) {
        m_errno = errno;
        break;
      }
    }
    fclose(fp);
    zip_fclose(file);
    if (m_errno)
      return false;
  }
  
  return true;
}
