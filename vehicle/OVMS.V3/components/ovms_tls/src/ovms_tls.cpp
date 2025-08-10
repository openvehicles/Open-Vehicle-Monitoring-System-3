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
static const char *TAG = "tls";

#include "string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include "ovms_config.h"
#include "ovms_events.h"
#include "ovms_command.h"
#include "ovms_malloc.h"
#include "ovms_tls.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/base64.h"
#include "mbedtls/debug.h"

OvmsTLS MyOvmsTLS __attribute__ ((init_priority (3000)));

void tls_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  char *cache = MyOvmsTLS.GetTrustedList();
  if (cache != NULL)
    {
    writer->printf("SSL/TLS has %d trusted CAs, using %d bytes of memory\n",
      MyOvmsTLS.Count(),strlen(cache));
    }
  else
    {
    writer->printf("SSL/TLS has %d trusted CAs, not currently cached\n", MyOvmsTLS.Count());
    }
  }

void tls_clear(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Clearing SSL/TLS trusted CA list");
  MyOvmsTLS.Clear();
  }

void tls_reload(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Reloading SSL/TLS trusted CA list");
  MyOvmsTLS.Reload();
  tls_status(verbosity,writer,cmd,argc,argv);
  }

void tls_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  char *buf = (char*)ExternalRamMalloc(1024);
  for (auto it = MyOvmsTLS.m_trustlist.begin(); it != MyOvmsTLS.m_trustlist.end(); it++)
    {
    OvmsTrustedCert* tc = it->second;
    mbedtls_x509_crt* crt = tc->GetCert();
    writer->printf("%s\n",it->first.c_str());
    if ( crt && (mbedtls_x509_crt_info(buf, 1024-1, "  ", crt) > 0) )
      {
      writer->printf("%d byte certificate: %s\n%s\n",crt->raw.len,it->first.c_str(),buf);
      }
    else
      {
      writer->printf("Invalid certificate could not be parsed as X509: %s\n\n", it->first.c_str());
      }
    }
  free(buf);
  }

OvmsTLS::OvmsTLS()
  {
  ESP_LOGI(TAG, "Initialising TLS (3000)");

  m_trustedcache = NULL;

  OvmsCommand* cmd_tls = MyCommandApp.RegisterCommand("tls","SSL/TLS Framework",NULL,"",0,0);
  OvmsCommand* cmd_trust = cmd_tls->RegisterCommand("trust","SSL/TLS Trusted CA Framework", tls_status, "", 0, 0, false);
  cmd_trust->RegisterCommand("status","Show SSL/TLS Trusted CA status",tls_status, "",0,0);
  cmd_trust->RegisterCommand("clear","Clear SSL/TLS Trusted CA list",tls_clear, "",0,0);
  cmd_trust->RegisterCommand("reload","Reload SSL/TLS Trusted CA list",tls_reload, "",0,0);
  cmd_trust->RegisterCommand("list","Show SSL/TLS Trusted CA list",tls_list, "",0,0);

  // Register our callbacks
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"config.mounted", std::bind(&OvmsTLS::UpdatedConfig, this, _1, _2));
  }

OvmsTLS::~OvmsTLS()
  {
  Clear();
  }

void OvmsTLS::UpdatedConfig(std::string event, void* data)
  {
  Reload();
  }

void OvmsTLS::Clear()
  {
  for (auto it = m_trustlist.begin(); it != m_trustlist.end(); it++)
    {
    delete it->second;
    }
  m_trustlist.clear();
  ClearTrustedRaw();
  }

void OvmsTLS::Reload()
  {
#ifdef CONFIG_MBEDTLS_DEBUG
  mbedtls_debug_set_threshold(1);
#endif //CONFIG_MBEDTLS_DEBUG
  Clear();

  // Add our embedded trusted CAs
  extern const unsigned char usertrust[] asm("_binary_usertrust_crt_start");
  extern const unsigned char usertrust_end[] asm("_binary_usertrust_crt_end");
  m_trustlist["USERTrust RSA Certification Authority"] = new OvmsTrustedCert(usertrust, usertrust_end - usertrust);

  extern const unsigned char digicert_global[] asm("_binary_digicert_global_crt_start");
  extern const unsigned char digicert_global_end[] asm("_binary_digicert_global_crt_end");
  m_trustlist["DigiCert Global Root CA"] = new OvmsTrustedCert(digicert_global, digicert_global_end - digicert_global);

  extern const unsigned char digicert_g2[] asm("_binary_digicert_g2_crt_start");
  extern const unsigned char digicert_g2_end[] asm("_binary_digicert_g2_crt_end");
  m_trustlist["DigiCert Global Root G2"] = new OvmsTrustedCert(digicert_g2, digicert_g2_end - digicert_g2);

  extern const unsigned char starfield_class2[] asm("_binary_starfield_class2_crt_start");
  extern const unsigned char starfield_class2_end[] asm("_binary_starfield_class2_crt_end");
  m_trustlist["Starfield Class 2 CA"] = new OvmsTrustedCert(starfield_class2, starfield_class2_end - starfield_class2);

  extern const unsigned char baltimore_cybertrust[] asm("_binary_baltimore_cybertrust_crt_start");
  extern const unsigned char baltimore_cybertrust_end[] asm("_binary_baltimore_cybertrust_crt_end");
  m_trustlist["Baltimore CyberTrust Root CA"] = new OvmsTrustedCert(baltimore_cybertrust, baltimore_cybertrust_end - baltimore_cybertrust);

  extern const unsigned char isrg_x1[] asm("_binary_isrg_x1_crt_start");
  extern const unsigned char isrg_x1_end[] asm("_binary_isrg_x1_crt_end");
  m_trustlist["ISRG X1 CA"] = new OvmsTrustedCert(isrg_x1, isrg_x1_end - isrg_x1);

  // Add trusted certs on disk (/store/trustedca)
  DIR *dir;
  struct dirent *dp;
  if ((dir = opendir("/store/trustedca")) != NULL)
    {
    while ((dp = readdir(dir)) != NULL)
      {
      std::string path("/store/trustedca/");
      path.append(dp->d_name);
      FILE* f = fopen(path.c_str(), "r");
      if (f != NULL)
        {
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *buf = (char*)ExternalRamMalloc(size+1);
        fread(buf,1,size,f);
        buf[size] = 0;
        m_trustlist[dp->d_name] = new OvmsTrustedCert(reinterpret_cast<unsigned char*>(buf), size + 1);
        free(buf);
        }
      fclose(f);
      }
    closedir(dir);
    }

  ClearTrustedRaw();
  BuildTrustedRaw();
  }

int OvmsTLS::Count()
  {
  return m_trustlist.size();
  }

char* OvmsTLS::GetTrustedList()
  {
  if (m_trustedcache == NULL) BuildTrustedRaw();

  return m_trustedcache;
  }

void OvmsTLS::ClearTrustedRaw()
  {
  if (m_trustedcache != NULL)
    {
    ESP_LOGI(TAG, "Clearing trusted CA cache");
    free(m_trustedcache);
    m_trustedcache = NULL;
    }
  }

void OvmsTLS::BuildTrustedRaw()
  {
  const char DASHES[] = "-----";
  const char BEGIN[] = "BEGIN";
  const char END[] = "END";
  const char CERTIFICATE[] = " CERTIFICATE";

  if (m_trustedcache != NULL) return;

  size_t totalLength = 0u;
  size_t count = 0u;
  for (auto it = m_trustlist.begin(); it != m_trustlist.end(); ++it)
    {
    mbedtls_x509_crt* crt = it->second->GetCert();
    if (crt)
      {
      size_t encodedLength;
      mbedtls_base64_encode(nullptr, 0, &encodedLength, nullptr, crt->raw.len);
      // Add the new lines added every 64 bytes
      encodedLength += (encodedLength >> 6);
      // Add the header and footer (the NUL will be included as new lines)
      encodedLength += sizeof(DASHES) + sizeof(BEGIN) + sizeof(CERTIFICATE) + sizeof(DASHES) - 3;
      encodedLength += sizeof(DASHES) + sizeof(END) + sizeof(CERTIFICATE) + sizeof(DASHES) - 3;
      totalLength += encodedLength;
      ++count;
      }
    }

  m_trustedcache = (char*)ExternalRamMalloc(totalLength + 1);
  if (m_trustedcache == nullptr)
    {
    return;
    }

  auto current = m_trustedcache;
  for (auto it = m_trustlist.begin(); it != m_trustlist.end(); ++it)
    {
    mbedtls_x509_crt* crt = it->second->GetCert();
    if (crt)
      {
      current = std::copy(DASHES, &DASHES[sizeof(DASHES) - 1], current);
      current = std::copy(BEGIN, &BEGIN[sizeof(BEGIN) - 1], current);
      current = std::copy(CERTIFICATE, &CERTIFICATE[sizeof(CERTIFICATE) - 1], current);
      current = std::copy(DASHES, &DASHES[sizeof(DASHES) - 1], current);
      *current++ = '\n';
      // Encode 48 bytes at a time with new lines between them
      const unsigned char* der = crt->raw.p;
      size_t derLength = crt->raw.len;
      while (derLength)
        {
        size_t pemLength;
        size_t inLength = derLength > 48 ? 48 : derLength;
        mbedtls_base64_encode((unsigned char*)current, 65, &pemLength, der, inLength);
        current += pemLength;
        *current++ = '\n';
        der += inLength;
        derLength -= inLength;
        }
      current = std::copy(DASHES, &DASHES[sizeof(DASHES) - 1], current);
      current = std::copy(END, &END[sizeof(END) - 1], current);
      current = std::copy(CERTIFICATE, &CERTIFICATE[sizeof(CERTIFICATE) - 1], current);
      current = std::copy(DASHES, &DASHES[sizeof(DASHES) - 1], current);
      *current++ = '\n';
      }
    }
  *current = '\0';

  ESP_LOGI(TAG, "Built trusted CA cache (%d entries, %d bytes)",count,totalLength);
  }

OvmsTrustedCert::OvmsTrustedCert(const unsigned char* cert, size_t length) :
    m_cert(reinterpret_cast<mbedtls_x509_crt*>(ExternalRamMalloc(sizeof(mbedtls_x509_crt))))
  {
  mbedtls_x509_crt_init(m_cert);
  if (mbedtls_x509_crt_parse(m_cert, cert, length) != 0)
    {
    ESP_LOGE(TAG,"Invalid %d byte trusted CA - ignored", length);
    free(m_cert);
    m_cert = nullptr;
    }
  else
    {
    ESP_LOGD(TAG,"Registered %d byte trusted CA", length);
    }
  }

OvmsTrustedCert::~OvmsTrustedCert()
  {
  if (m_cert)
    {
    ESP_LOGD(TAG,"Freeing trusted certificate");
    mbedtls_x509_crt_free(m_cert);
    free(m_cert);
    }
  }

mbedtls_x509_crt* OvmsTrustedCert::GetCert()
  {
  return m_cert;
  }
