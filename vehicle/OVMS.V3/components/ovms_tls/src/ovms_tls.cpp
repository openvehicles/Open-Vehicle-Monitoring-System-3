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
  mbedtls_x509_crt *crt = (mbedtls_x509_crt*)ExternalRamMalloc( sizeof(mbedtls_x509_crt) );
  char *buf = (char*)ExternalRamMalloc(1024);
  for (auto it = MyOvmsTLS.m_trustlist.begin(); it != MyOvmsTLS.m_trustlist.end(); it++)
    {
    OvmsTrustedCert* tc = it->second;
    char* pem = tc->GetPEM();
    writer->printf("%s length %d bytes\n",it->first.c_str(),strlen(tc->GetPEM()));
    mbedtls_x509_crt_init(crt);
    if ( (mbedtls_x509_crt_parse(crt, (const unsigned char *)pem, strlen(pem)+1) == 0) &&
         (mbedtls_x509_crt_info(buf, 1024-1, "  ", crt) > 0) )
      {
      writer->printf("%d byte certificate: %s\n%s\n",strlen(pem),it->first.c_str(),buf);
      }
    else
      {
      writer->printf("%d byte invalid certificate could not be parsed as X509: %s\n\n",
        strlen(pem),it->first.c_str());
      }
    mbedtls_x509_crt_free(crt);
    }
  free(buf);
  free(crt);
  }

OvmsTLS::OvmsTLS()
  {
  ESP_LOGI(TAG, "Initialising TLS (3000)");

  m_trustedcache = NULL;

  OvmsCommand* cmd_tls = MyCommandApp.RegisterCommand("tls","SSL/TLS Framework",NULL,"",0,0);
  OvmsCommand* cmd_trust = cmd_tls->RegisterCommand("trust","SSL/TLS Trusted CA Framework",NULL,"",0,0);
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
  mbedtls_debug_set_threshold(1);
  Clear();

  // Add our embedded trusted CAs
  extern const char addtrust[] asm("_binary_addtrust_crt_start");
  m_trustlist["AddTrust External CA Root"] = new OvmsTrustedCert((char*)addtrust, false);

  extern const char dst[] asm("_binary_dst_crt_start");
  m_trustlist["DST Root CA X3"] = new OvmsTrustedCert((char*)dst, false);

  extern const char digicert_global[] asm("_binary_digicert_global_crt_start");
  m_trustlist["DigiCert Global Root CA"] = new OvmsTrustedCert((char*)digicert_global, false);

  extern const char starfield_class2[] asm("_binary_starfield_class2_crt_start");
  m_trustlist["Starfield Class 2 CA"] = new OvmsTrustedCert((char*)starfield_class2, false);

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
        m_trustlist[dp->d_name] = new OvmsTrustedCert(buf, true);
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
  if (m_trustedcache != NULL) return;

  int count = 0;
  size_t size = 0;
  for (auto it = m_trustlist.begin(); it != m_trustlist.end(); it++)
    {
    count++;
    size += strlen(it->second->GetPEM());
    }
  size += 1; // Ending 0 termination

  m_trustedcache = (char*)ExternalRamMalloc(size);

  char* p = m_trustedcache;
  for (auto it = m_trustlist.begin(); it != m_trustlist.end(); it++)
    {
    char *pem = it->second->GetPEM();
    strcpy(p,pem);
    p += strlen(pem);
    }
  *p = 0; // NULL terminate

  ESP_LOGI(TAG, "Built trusted CA cache (%d entries, %d bytes)",count,size);
  }

OvmsTrustedCert::OvmsTrustedCert(char* pem, bool needfree)
  {
  ESP_LOGD(TAG,"Registered %d byte trusted CA (%d)",strlen(pem),needfree);
  m_pem = pem;
  m_needfree = needfree;
  }

OvmsTrustedCert::~OvmsTrustedCert()
  {
  ESP_LOGD(TAG,"Freeing trusted certificate (length %d bytes)",strlen(m_pem));
  if (m_needfree) free(m_pem);
  }

bool OvmsTrustedCert::IsInternal()
  {
  return (! m_needfree);
  }

char *OvmsTrustedCert::GetPEM()
  {
  return m_pem;
  }
