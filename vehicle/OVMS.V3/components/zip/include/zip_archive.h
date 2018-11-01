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

#ifndef __zip_archive_h__
#define __zip_archive_h__

#include <string>
#include <zip.h>

/**
 * ZipArchive: zip/unzip utility
 * 
 * Zip usage example:
 *   ZipArchive zip(path, password, ZIP_CREATE|ZIP_TRUNCATE);
 *   zip.chdir("/src/dir");
 *   zip.add("file_or_directory");
 *   zip.add("…");
 *   zip.close();
 *
 * Unzip usage example:
 *   ZipArchive zip(path, password, ZIP_RDONLY);
 *   zip.chdir("/dst/dir");
 *   zip.extract("file_or_dir_prefix");
 *   zip.extract("…");
 *   zip.close();
 * 
 * Encryption:
 *   - empty password ("") = no encryption (or set encmethod to ZIP_EM_NONE)
 *   - default encryption is AES 256 bit (supported by 7z for example)
 */

class ZipArchive
{
private:
  struct zip* m_zip;
  std::string m_basedir;
  std::string m_password;
  zip_uint16_t m_encmethod;
  int m_errno;

public:
  ZipArchive(const std::string& zippath, const std::string& password,
             zip_flags_t flags = 0, zip_uint16_t encmethod = ZIP_EM_AES_256);
  ~ZipArchive();

  bool chdir(const std::string& path);
  bool add(std::string path);
  bool extract(const std::string prefix);
  bool close();
  bool ok();
  const char* strerror();
};

#endif // __zip_archive_h__
