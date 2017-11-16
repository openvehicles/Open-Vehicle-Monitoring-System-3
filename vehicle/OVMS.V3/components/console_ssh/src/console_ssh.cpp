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
#define LWIP_POSIX_SOCKETS_IO_NAMES 0
#include <lwip/def.h>
#include <lwip/sockets.h>
#undef bind
#include "freertos/queue.h"
#include "ovms_log.h"
#include "ovms_events.h"
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/coding.h>
#include <wolfssh/ssh.h>
#include "console_ssh.h"

static const char *tag = "ssh";
static uint8_t CRLF[2] = { '\r', '\n' };
static const char newline = '\n';
static int LoadPasswordBuffer(uint8_t* buf, uint32_t bufSz, PwMapList* list);
static int LoadPublicKeyBuffer(uint8_t* buf, uint32_t bufSz, PwMapList* list);
static void PwMapListDelete(PwMapList* list);

static const uint8_t serverKeyBuffer[] =
  {
   48,130,  4,163,  2,  1,  0,  2,130,  1,  1,  0,218, 93,173, 37,
   20,118, 21, 89,243, 64,253, 60,184, 98, 48,179,109,192,249,236,
  236,139,131, 30,158, 66,156,202, 65,106,211,138,225, 82, 52,224,
   13, 19, 98,126,212, 15,174, 92, 77,  4,241,141,250,197,173,119,
  170, 90,  5,202,239,248,141,171,255,138, 41,  9, 76,  4,194,245,
   25,203,237, 31,177,180, 41,211,195,108,169, 35,223,163,160,229,
    8,222,173,140,113,249, 52,136,108,237, 59,240,111,165, 15,172,
   89,255,107, 51,241,112,251,140,164,179, 69, 34,141,157,119,122,
  229, 41, 95,132, 20,217,153,234,234,206, 45, 81,243,227, 88,250,
   91,  2, 15,201,181, 42,188,178, 94,211,194, 48,187, 60,177,195,
  239, 88,243, 80,148, 40,139,196,101, 74,247,  0,217,151,217,107,
   77,141,149,161,138, 98,  6,180, 80, 17, 34,131,180,234, 42,231,
  208,168, 32, 71, 79,255, 70,174,197, 19,225, 56,139,248, 84,175,
   58, 77, 47,248, 31,215,132,144,216,147,  5,  6,194,125,144,219,
  227,156,208,196,101, 90,  3,173,  0,172, 90,162,205,218, 63,137,
   88, 55, 83,191, 43, 70,122,172,137, 65, 43, 90, 46,232,118,231,
   94,227, 41,133,163, 99,234,230,134, 96,124, 45,  2,  3,  1,  0,
    1,  2,129,255, 15,145, 30,  6,198,174,164, 87,  5, 64, 92,205,
   55, 87,200,161,  1,241,255,223, 35,253,206, 27, 32,173, 31,  0,
   76, 41,145,107, 21, 37,  7, 31,241,206,175,246,218,167, 67,134,
  208,246,201, 65,149,223,  1,190,198, 38, 36,195,146,215,229, 65,
  157,181,251,182,237,244,104,241,144, 37, 57,130, 72,232,207, 18,
  137,155,245,114,217, 62,144,249,194,232, 28,247, 38, 40,221,213,
  219,238, 13,151,214, 93,174,  0, 91,106, 25,250, 89,251,243,242,
  210,202,244,226,193,181,184, 14,202,199,104, 71,194, 52,193,  4,
   62, 56,244,130,  1, 89,242,138,110,247,107, 91, 10,188,  5,169,
   39, 55,185,249,  6,128, 84,232,112, 26,180, 50,147,107,245, 38,
  199,134,244, 88,  5, 67,249,114,143,236, 66,160, 59,186, 53, 98,
  204,236,244,179,  4,162,235,174, 60,135, 64,142,254,143,221, 20,
  190,189,131,201,201, 24,202,129,124,  6,249,227,153, 46,236, 41,
  197, 39, 86,234, 30,147,198,232, 12, 68,202,115,104, 74,127,174,
   22, 37, 29, 18, 37, 20, 42,236, 65,105, 37,195, 93,230,174,228,
   89,128, 29,250,189,159, 51, 54,147,157,136,214,136,201, 91, 39,
  123, 11, 97,  2,129,129,  0,222,  1,171,250,101,210,250,210,111,
  254, 63, 87,109,117,127,140,230,189,254,  8,189,199, 19, 52, 98,
   14,135,178,122, 44,169,205,202,147,216, 49,145,129, 45,214,104,
  150,170, 37,227,184,126,165,152,168,232, 21, 60,192,206,222,245,
  171,128,177,245,186,175,172,156,193,179, 67, 52,174, 34,247, 24,
   65,134, 99,162, 68,142, 27, 65,157, 45,117,111, 13, 91, 16, 25,
   93, 20,170,128, 31,238,  2, 62,248,182,246,236,101,142, 56,137,
   13, 11, 80,228, 17, 73,134, 57,130,219,115,229, 58, 15, 19, 34,
  171,173,160,120,155,148, 33,  2,129,129,  0,251,205, 76, 82, 73,
   63, 44,128,148,145, 74, 56,236, 15, 74,125, 58,142,188,  4,144,
   21, 37,132,251,211,104,189,239,160, 71,254,206, 91,191, 29, 42,
  148, 39,252, 81,112,255,201,233,186,190, 43,160, 80, 37,211,225,
  161, 87, 51,204, 92,199,125,  9,246,220,251,114,148, 61,202, 89,
   82,115,224,108, 69, 10,217,218, 48,223, 43, 51,215, 82, 24, 65,
    1,240,223, 27,  1,193,211,183,155, 38,248, 28,143,255,200, 25,
  253, 54,208, 19,165,114, 66,163, 48, 89, 87,180,218, 42,  9,229,
   69, 90, 57,109,112, 34, 12,186, 83, 38,141,  2,129,129,  0,177,
   60,194,112,240,147,196, 60,246,190, 19, 17,152, 72,130,225, 25,
   97,187, 10,125,128, 14, 59,246,192,196,226,223, 25,  3, 35, 81,
   68, 65,  8, 41,178,232,198, 80,207, 95,221, 73,245,  3,222,238,
  134,130,106, 90, 11, 79,220,190, 99,  2, 38,145, 24, 78,161,206,
  175,241,142,136,227, 48,244,245,255,113,235,223, 35, 62, 20, 82,
  136,202, 63,  3,190,180,225,160,110, 40, 78,138,101,115, 93,133,
  170,136, 95,143,144,240, 63,  0, 99, 82,146,108,209,196, 82, 13,
   94,  4, 23,125,124,161,134, 84, 90,157, 14, 12,219,160, 33,  2,
  129,129,  0,234,254, 27,158, 39,177,135,108,176, 58, 47,148,147,
  233,105, 81, 25,151, 31,172,250,114, 97,195,139,233, 46,181, 35,
  174,231,193,203,  0, 32,137,173,180,250,228, 37,117, 89,162, 44,
   57, 21, 69, 77,165,190,199,208,168,107,227,113,115,156,208,250,
  189,162, 90, 32,  2,108,240, 45, 16, 32,  8,111,194,183,111,188,
  139, 35,155,  4, 20,141, 15,  9,140, 48, 41,102,224,234,237, 21,
   74,252,193, 76,150,174,213, 38, 60,  4, 45,136, 72, 61, 44, 39,
  115,245,205, 62,128,227,254,188, 51, 79, 18,141, 41,186,253, 57,
  222, 99,249,  2,129,129,  0,139, 31, 71,162,144, 75,130, 59,137,
   45,233,107,225, 40,229, 34,135,131,208,222, 30, 13,140,204,132,
   67, 61, 35,141,157,108,188,196,198,218, 68, 68,121, 32,182, 62,
  239,207,138,196, 56,176,229,218, 69,172, 90,204,123, 98,186,169,
  115, 31,186, 39, 92,130,248,173, 49, 30,222,243, 55,114,203, 71,
  210,205,247,248,127,  0, 57,219,141, 42,202, 78,193,206,226, 21,
  137,214, 58, 97,174,157,162, 48,165,133,174, 56,234, 70,116,220,
    2, 58,172,233, 95,163,198,115, 79,115,129,144, 86,195,206,119,
   95, 91,186,108, 66,241, 33
  };

static const char samplePasswordBuffer[] =
    "jill:upthehill\n"
    "jack:fetchapail\n";

static const char samplePublicKeyBuffer[] =
    "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC9P3ZFowOsONXHD5MwWiCciXytBRZGho"
    "MNiisWSgUs5HdHcACuHYPi2W6Z1PBFmBWT9odOrGRjoZXJfDDoPi+j8SSfDGsc/hsCmc3G"
    "p2yEhUZUEkDhtOXyqjns1ickC9Gh4u80aSVtwHRnJZh9xPhSq5tLOhId4eP61s+a5pwjTj"
    "nEhBaIPUJO2C/M0pFnnbZxKgJlX7t1Doy7h5eXxviymOIvaCZKU+x5OopfzM/wFkey0EPW"
    "NmzI5y/+pzU5afsdeEWdiQDIQc80H6Pz8fsoFPvYSG+s4/wz0duu7yeeV1Ypoho65Zr+pE"
    "nIf7dO0B8EblgWt+ud+JI8wrAhfE4x hansel\n"
    "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCqDwRVTRVk/wjPhoo66+Mztrc31KsxDZ"
    "+kAV0139PHQ+wsueNpba6jNn5o6mUTEOrxrz0LMsDJOBM7CmG0983kF4gRIihECpQ0rcjO"
    "P6BSfbVTE9mfIK5IsUiZGd8SoE9kSV2pJ2FvZeBQENoAxEFk0zZL9tchPS+OCUGbK4SDjz"
    "uNZl/30Mczs73N3MBzi6J1oPo7sFlqzB6ecBjK2Kpjus4Y1rYFphJnUxtKvB0s+hoaadru"
    "biE57dK6BrH5iZwVLTQKux31uCJLPhiktI3iLbdlGZEctJkTasfVSsUizwVIyRjhVKmbdI"
    "RGwkU38D043AR1h0mUoGCPIKuqcFMf gretel\n";

//-----------------------------------------------------------------------------
//    Class OvmsSSH
//-----------------------------------------------------------------------------

OvmsSSH MySSH __attribute__ ((init_priority (8300)));

OvmsSSH::OvmsSSH()
  {
  ESP_LOGI(tag, "Initialising SSH (8300)");

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(tag,"network.wifi.up", std::bind(&OvmsSSH::WifiUp, this, _1, _2));
  MyEvents.RegisterEvent(tag,"network.wifi.down", std::bind(&OvmsSSH::WifiDown, this, _1, _2));
  }

void OvmsSSH::WifiUp(std::string event, void* data)
  {
  ESP_LOGI(tag, "Launching SSH Server");
  AddChild(new SSHServer(this));
  }

void OvmsSSH::WifiDown(std::string event, void* data)
  {
  ESP_LOGI(tag, "Stopping SSH Server");
  DeleteChildren();
  }

//-----------------------------------------------------------------------------
//    Class SSHServer
//-----------------------------------------------------------------------------

SSHServer::SSHServer(Parent* parent)
  : TaskBase(parent)
  {
  m_ctx = NULL;
  m_socket = -1;
  }

bool SSHServer::Instantiate()
  {
  if (CreateTaskPinned(1, "SSHServer", 3000) != pdPASS)
    {
    ::printf("\nInsufficient memory to create SSHServer task\n");
    return false;
    }
  return true;
  }

SSHServer::~SSHServer()
  {
  Cleanup();
  }

void SSHServer::Service()
  {
  if (wolfSSH_Init() != WS_SUCCESS)
    {
    ESP_LOGE(tag, "Couldn't initialize wolfSSH.");
    return;
    }

  m_ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL);
  if (m_ctx == NULL)
    {
    ::printf("\nInsufficient memory to allocate SSH CTX data\n");
    return;
    }

  memset(&pwMapList, 0, sizeof(pwMapList));
  wolfSSH_SetUserAuth(m_ctx, SSHUserAuthCallback);
  wolfSSH_CTX_SetBanner(m_ctx, "WolfSSH Server\n");

  if (wolfSSH_CTX_UsePrivateKey_buffer(m_ctx, serverKeyBuffer,
      sizeof(serverKeyBuffer),  WOLFSSH_FORMAT_ASN1) < 0)
    {
    ESP_LOGE(tag, "Couldn't use server key buffer.");
    return;
    }

  #define SCRATCH_BUFFER_SIZE     1200
  uint8_t buf[SCRATCH_BUFFER_SIZE];
  uint32_t bufSz = (uint32_t)strlen((char*)samplePasswordBuffer);
  memcpy(buf, samplePasswordBuffer, bufSz);
  buf[bufSz] = 0;
  LoadPasswordBuffer(buf, bufSz, &pwMapList);

  bufSz = (uint32_t)strlen((char*)samplePublicKeyBuffer);
  memcpy(buf, samplePublicKeyBuffer, bufSz);
  buf[bufSz] = 0;
  LoadPublicKeyBuffer(buf, bufSz, &pwMapList);

  m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_socket < 0)
    {
    ESP_LOGE(tag, "socket: %d (%s)", errno, strerror(errno));
    return;
    }

  struct sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddr.sin_port = htons(22);

  if (lwip_bind(m_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
    ESP_LOGE(tag, "bind socket %d: %d (%s)", m_socket, errno, strerror(errno));
    return;
    }

  if (listen(m_socket, 5) < 0)
    {
    ESP_LOGE(tag, "listen: %d (%s)", errno, strerror(errno));
    return;
    }

  fd_set readfds, writefds, errorfds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&errorfds);
  FD_SET(m_socket, &readfds);
  while (true) {
    int rc = select(m_socket+1, &readfds, &writefds, &errorfds, (struct timeval *)NULL);
    if (rc < 0)
      {
      ESP_LOGE(tag, "select: %d (%s)", errno, strerror(errno));
      break;
      }
    if (m_socket < 0)   // Was server socket closed (by wifi shutting down)?
      break;

    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
    int partnerSocket = accept(m_socket, (struct sockaddr *)&clientAddr, &len);
    if (partnerSocket < 0)
      {
      if (errno != ECONNABORTED)
        ESP_LOGE(tag, "accept: %d (%s)", errno, strerror(errno));
      break;
      }

    AddChild(new ConsoleSSH(this, partnerSocket));
    }
  }

void SSHServer::Cleanup()
  {
  PwMapListDelete(&pwMapList);
  wolfSSH_CTX_free(m_ctx);
  if (wolfSSH_Cleanup() != WS_SUCCESS)
    {
    ESP_LOGE(tag, "Couldn't clean up wolfSSH.");
    }

  if (m_socket >= 0)
    {
    int socket = m_socket;
    m_socket = -1;
    if (closesocket(socket) < 0)
      {
      ESP_LOGE(tag, "closesocket %d: %d (%s)", socket, errno, strerror(errno));
      }
    }
  }

//-----------------------------------------------------------------------------
//    Class SSHReceiver
//-----------------------------------------------------------------------------

// The SSHReceiver task is required so that we can use select() to learn when
// input is available on the ssh connection and then queue a RECV Event to the
// ConsoleSSH task so that task can wait either for these Events or for ALERT
// Events to output messages on this console.  This task must not directly
// execute code in WolfSSH.

SSHReceiver::SSHReceiver(ConsoleSSH* parent, int socket,
                         QueueHandle_t queue, SemaphoreHandle_t sem)
  : TaskBase(parent)
  {
  m_socket = socket;
  m_queue = queue;
  m_semaphore = sem;
  }

bool SSHReceiver::Instantiate()
  {
  if (CreateTaskPinned(1, "SSHReceiver", 1000) != pdPASS)
    {
    ::printf("\nInsufficient memory to create SSHReceiver task\n");
    return false;
    }
  return true;
  }

SSHReceiver::~SSHReceiver()
  {
  }

void SSHReceiver::Service()
  {
  int rc;
  if ((rc = lwip_fcntl(m_socket, F_SETFL, O_NONBLOCK) < 0))
    {
    ::printf("fcntl SETFL returned %d, errno = %d\n", rc, errno);
    }
  fd_set readfds, writefds, errorfds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&errorfds);
  FD_SET(m_socket, &readfds);
  while (true) {
    int rc = select(m_socket+1, &readfds, &writefds, &errorfds, (struct timeval *)NULL);
    if (rc < 0)
      {
      ESP_LOGE(tag, "select: %d (%s)", errno, strerror(errno));
      break;
      }
    if (m_socket < 0)   // Was socket closed (by client exiting)?
      break;
    OvmsConsole::Event event;
    event.type = OvmsConsole::event_type_t::RECV;
    event.size = 0;
    BaseType_t ret = xQueueSendToBack(m_queue, (void * )&event, (portTickType)(1000 / portTICK_PERIOD_MS));
    if (ret == pdPASS)
      {
      // Block here until the queued message has been taken.
      xSemaphoreTake(m_semaphore, portMAX_DELAY);
      }
    else
      ESP_LOGE(tag, "Timeout queueing message in SSHReceiver::Service\n");
    }
  }

//-----------------------------------------------------------------------------
//    Class ConsoleSSH
//-----------------------------------------------------------------------------

ConsoleSSH::ConsoleSSH(SSHServer* server, int socket)
  : OvmsConsole(server)
  {
  m_socket = socket;
  m_queue = xQueueCreate(100, sizeof(Event));
  m_semaphore = xSemaphoreCreateCounting(1, 0);
  m_ssh = NULL;
  }

// This destructor may be called by ConsoleSSHTask to delete itself or by
// SSHServer task if the server gets shut down, but not by SSHReceiver task.

ConsoleSSH::~ConsoleSSH()
  {
  m_ready = false;
  DeleteChildren();
  int socket = m_socket;
  m_socket = -1;
  if (socket >= 0 && closesocket(socket) < 0)
    ESP_LOGE(tag, "closesocket %d: %d (%s)", socket, errno, strerror(errno));
  WOLFSSH* ssh = m_ssh;
  m_ssh = NULL;
  wolfSSH_free(ssh);
  vSemaphoreDelete(m_semaphore);
  vQueueDelete(m_queue);
  }

bool ConsoleSSH::Instantiate()
  {
  if (CreateTaskPinned(1, "SSHConsole", 6000) != pdPASS)
    {
    ::printf("\nInsufficient memory to create SSHConsole task\n");
    return false;
    }
  return true;
  }

//-----------------------------------------------------------------------------
//    SSH Console Task
//-----------------------------------------------------------------------------

// The following code is executed by the SSHConsole task created by the
// TaskBase::AddChild() calling ConsoleSSH::Instantiate() and must not be
// executed by any other task.

void ConsoleSSH::Service()
  {
  m_ssh = wolfSSH_new(server()->ctx());
  if (m_ssh == NULL)
    {
    ::printf("Couldn't allocate SSH data.\n");
    return;
    }
  wolfSSH_SetUserAuthCtx(m_ssh, &server()->pwMapList);
  /* Use the session object for its own highwater callback ctx */
  wolfSSH_SetHighwaterCtx(m_ssh, (void*)m_ssh);
  wolfSSH_SetHighwater(m_ssh, DEFAULT_HIGHWATER_MARK);
  wolfSSH_set_fd(m_ssh, m_socket);
  //wolfSSH_Debugging_ON();
  if (wolfSSH_accept(m_ssh) != WS_SUCCESS)
    {
    ::printf("wolfSSH_accept failed\n");
    return;
    }
  if (!AddChild(new SSHReceiver(this, m_socket, m_queue, m_semaphore)))
    {
    ::printf("\nInsufficient memory to create SSHReceiver object\n");
    printf("Insufficient memory for connection\n");
    return;
    }
  Initialize("SSH");
  OvmsConsole::Service();
  }

void ConsoleSSH::HandleDeviceEvent(void* pEvent)
  {
  Event event = *(Event*)pEvent;
  switch (event.type)
    {
    case RECV:
      while (true)
        {
        int size = wolfSSH_stream_read(m_ssh, (uint8_t*)m_buffer, BUFFER_SIZE);
        if (size > 0)
          {
          // Translate CR (Enter) from ssh client into \n for microrl
          for (int i = 0; i < size; ++i)
            {
            if (m_buffer[i] == '\r')
              m_buffer[i] = '\n';
            }
          ProcessChars(m_buffer, size);
          }
        else
          {
          // Unblock SSHReceiverTask now that we have consumed all the
          // available data.
          xSemaphoreGive(m_semaphore);
          if (size == WS_WANT_READ)
            break;
          // Some error occurred, presume it is connection closing.
          DeleteFromParent();
          // Execution does not return here because this task kills itself.
          }
        }
      break;

    default:
      ESP_LOGE(tag, "Unknown event type in ConsoleSSH");
      break;
    }
  }

// This is called to shut down the SSH connection when the "exit" command is input.
void ConsoleSSH::Exit()
  {
  DeleteFromParent();
  }

int ConsoleSSH::puts(const char* s)
  {
  if (!m_ssh)
    return -1;
  write(s, strlen(s));
  write(&newline, 1);
  return 0;
  }

int ConsoleSSH::printf(const char* fmt, ...)
  {
  if (!m_ssh)
    return 0;
  char *buffer = NULL;
  va_list args;
  va_start(args,fmt);
  int ret = vasprintf(&buffer, fmt, args);
  va_end(args);
  if (ret < 0)
    {
    if (buffer)
      free(buffer);
    return ret;
    }
  ret = write(buffer, ret);
  free(buffer);
  return ret;
  }

ssize_t ConsoleSSH::write(const void *buf, size_t nbyte)
  {
  if (!m_ssh || !nbyte)
    return 0;
  uint8_t* start = (uint8_t*)buf;
  uint8_t* eol = start;
  uint8_t* end = start + nbyte;
  int ret = 0;
  while (eol < end)
    {
    if (*eol == '\n')
      {
      if (eol > start)
        {
        ret = wolfSSH_stream_send(m_ssh, start, eol - start);
        if (ret <= 0) break;
        }
      ret = wolfSSH_stream_send(m_ssh, CRLF, 2);
      if (ret <= 0) break;
      ++eol;
      start = eol;
      }
    else if (++eol == end)
      {
      ret = wolfSSH_stream_send(m_ssh, start, eol - start);
      break;
      }
  }  
  
  if (ret <= 0)
    {
    ::printf("wolfSSH_stream_send returned %d, wolfSSH_get_error = %d\n", ret, wolfSSH_get_error(m_ssh));
    if (ret != WS_REKEYING)
      DeleteFromParent();
    }
  return ret;
  }

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static inline void c32toa(uint32_t u32, uint8_t* c)
{
    c[0] = (u32 >> 24) & 0xff;
    c[1] = (u32 >> 16) & 0xff;
    c[2] = (u32 >>  8) & 0xff;
    c[3] =  u32 & 0xff;
}


static PwMap* PwMapNew(PwMapList* list, uint8_t type, const uint8_t* username,
                       uint32_t usernameSz, const uint8_t* p, uint32_t pSz)
{
    PwMap* map;
    map = (PwMap*)malloc(sizeof(PwMap));
    if (map != NULL) {
        Sha256 sha;
        uint8_t flatSz[4];
        map->type = type;
        if (usernameSz >= sizeof(map->username))
            usernameSz = sizeof(map->username) - 1;
        memcpy(map->username, username, usernameSz + 1);
        map->username[usernameSz] = 0;
        map->usernameSz = usernameSz;
        wc_InitSha256(&sha);
        c32toa(pSz, flatSz);
        wc_Sha256Update(&sha, flatSz, sizeof(flatSz));
        wc_Sha256Update(&sha, p, pSz);
        wc_Sha256Final(&sha, map->p);
        map->next = list->head;
        list->head = map;
    }
    return map;
}


static void PwMapListDelete(PwMapList* list)
{
    if (list != NULL) {
        PwMap* head = list->head;
        while (head != NULL) {
            PwMap* cur = head;
            head = head->next;
            memset(cur, 0, sizeof(PwMap));
            free(cur);
        }
    }
}

static int LoadPasswordBuffer(uint8_t* buf, uint32_t bufSz, PwMapList* list)
{
    char* str = (char*)buf;
    char* delimiter;
    char* username;
    char* password;
    /* Each line of passwd.txt is in the format
     *     username:password\n
     * This function modifies the passed-in buffer. */
    if (list == NULL)
        return -1;
    if (buf == NULL || bufSz == 0)
        return 0;
    while (*str != 0) {
        delimiter = strchr(str, ':');
        username = str;
        *delimiter = 0;
        password = delimiter + 1;
        str = strchr(password, '\n');
        *str = 0;
        str++;
        if (PwMapNew(list, WOLFSSH_USERAUTH_PASSWORD,
                     (uint8_t*)username, (uint32_t)strlen(username),
                     (uint8_t*)password, (uint32_t)strlen(password)) == NULL ) {
            return -1;
        }
    }
    return 0;
}


static int LoadPublicKeyBuffer(uint8_t* buf, uint32_t bufSz, PwMapList* list)
{
    char* str = (char*)buf;
    char* delimiter;
    uint8_t* publicKey64;
    uint32_t publicKey64Sz;
    uint8_t* username;
    uint32_t usernameSz;
    uint8_t  publicKey[300];
    uint32_t publicKeySz;

    /* Each line of passwd.txt is in the format
     *     ssh-rsa AAAB3BASE64ENCODEDPUBLICKEYBLOB username\n
     * This function modifies the passed-in buffer. */
    if (list == NULL)
        return -1;
    if (buf == NULL || bufSz == 0)
        return 0;
    while (*str != 0) {
        /* Skip the public key type. This example will always be ssh-rsa. */
        delimiter = strchr(str, ' ');
        str = delimiter + 1;
        delimiter = strchr(str, ' ');
        publicKey64 = (uint8_t*)str;
        *delimiter = 0;
        publicKey64Sz = (uint32_t)(delimiter - str);
        str = delimiter + 1;
        delimiter = strchr(str, '\n');
        username = (uint8_t*)str;
        *delimiter = 0;
        usernameSz = (uint32_t)(delimiter - str);
        str = delimiter + 1;
        publicKeySz = sizeof(publicKey);
        if (Base64_Decode(publicKey64, publicKey64Sz,
                          publicKey, &publicKeySz) != 0) {
            return -1;
        }
        if (PwMapNew(list, WOLFSSH_USERAUTH_PUBLICKEY,
                     username, usernameSz,
                     publicKey, publicKeySz) == NULL ) {
            return -1;
        }
    }
    return 0;
}


int SSHServer::SSHUserAuthCallback(uint8_t authType, const WS_UserAuthData* authData, void* ctx)
{
    uint8_t authHash[SHA256_DIGEST_SIZE];
    if (ctx == NULL) {
        fprintf(stderr, "wsUserAuth: ctx not set");
        return WOLFSSH_USERAUTH_FAILURE;
    }
    if (authType != WOLFSSH_USERAUTH_PASSWORD &&
        authType != WOLFSSH_USERAUTH_PUBLICKEY) {
        return WOLFSSH_USERAUTH_FAILURE;
    }

    /* Hash the password or public key with its length. */
    {
        Sha256 sha;
        uint8_t flatSz[4];
        wc_InitSha256(&sha);
        if (authType == WOLFSSH_USERAUTH_PASSWORD) {
            c32toa(authData->sf.password.passwordSz, flatSz);
            wc_Sha256Update(&sha, flatSz, sizeof(flatSz));
            wc_Sha256Update(&sha,
                            authData->sf.password.password,
                            authData->sf.password.passwordSz);
        }
        else if (authType == WOLFSSH_USERAUTH_PUBLICKEY) {
            c32toa(authData->sf.publicKey.publicKeySz, flatSz);
            wc_Sha256Update(&sha, flatSz, sizeof(flatSz));
            wc_Sha256Update(&sha,
                            authData->sf.publicKey.publicKey,
                            authData->sf.publicKey.publicKeySz);
        }
        wc_Sha256Final(&sha, authHash);
    }
    PwMapList* list = (PwMapList*)ctx;
    PwMap* map = list->head;
    while (map != NULL) {
        if (authData->usernameSz == map->usernameSz &&
            memcmp(authData->username, map->username, map->usernameSz) == 0) {
            if (authData->type == map->type) {
                if (memcmp(map->p, authHash, SHA256_DIGEST_SIZE) == 0) {
                    return WOLFSSH_USERAUTH_SUCCESS;
                }
                else {
                    return (authType == WOLFSSH_USERAUTH_PASSWORD ?
                            WOLFSSH_USERAUTH_INVALID_PASSWORD :
                            WOLFSSH_USERAUTH_INVALID_PUBLICKEY);
                }
            }
            else {
                return WOLFSSH_USERAUTH_INVALID_AUTHTYPE;
            }
        }
        map = map->next;
    }
    return WOLFSSH_USERAUTH_INVALID_USER;
}
