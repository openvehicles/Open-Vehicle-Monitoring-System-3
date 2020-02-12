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
#include "ovms_command.h"
#include "ovms_malloc.h"
#include "ovms_tls.h"

OvmsTLS MyOvmsTLS __attribute__ ((init_priority (1300)));

OvmsTLS::OvmsTLS()
  {
  ESP_LOGI(TAG, "Initialising TLS (1300)");

  m_trustedcache = NULL;

  // Add our embedded trusted CAs
  extern const char addtrust[] asm("_binary_addtrust_crt_start");
  m_trustlist.push_back(OvmsTrustedCert((char*)addtrust, false));

  extern const char dst[] asm("_binary_dst_crt_start");
  m_trustlist.push_back(OvmsTrustedCert((char*)dst, false));

  extern const char digicert_global[] asm("_binary_digicert_global_crt_start");
  m_trustlist.push_back(OvmsTrustedCert((char*)digicert_global, false));
  }

OvmsTLS::~OvmsTLS()
  {
  }

void OvmsTLS::RegisterTrustedCA(char* pem)
  {
  m_trustlist.push_back(OvmsTrustedCert(pem, true));
  ClearTrustedRaw();
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
  ESP_LOGI(TAG, "Building trusted CA cache");

  int count = 0;
  size_t size = 0;
  for (TrustedCert_t::iterator it=m_trustlist.begin();
       it != m_trustlist.end();
       ++it)
    {
    count++;
    size += strlen(it->GetPEM());
    }
  size += 1; // Ending 0 termination

  m_trustedcache = (char*)ExternalRamMalloc(size);

  char* p = m_trustedcache;
  for (TrustedCert_t::iterator it=m_trustlist.begin();
       it != m_trustlist.end();
       ++it)
    {
    char *pem = it->GetPEM();
    strcpy(p,pem);
    p += strlen(pem);
    }
  *p = 0; // NULL terminate

  ESP_LOGI(TAG, "Built trusted CA cache (%d entries, %d bytes)",count,size);
  }

OvmsTrustedCert::OvmsTrustedCert(char* pem, bool needfree)
  {
  m_pem = pem;
  m_needfree = needfree;
  }

OvmsTrustedCert::~OvmsTrustedCert()
  {
  }

char *OvmsTrustedCert::GetPEM()
  {
  return m_pem;
  }
