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
static const char *TAG = "max7317";

#include <string.h>
#include "max7317.h"
#include "ovms_command.h"
#include "ovms_peripherals.h"
#include "ovms_config.h"
#include "metrics_standard.h"

max7317::max7317(const char* name, spi* spibus, spi_nodma_host_device_t host, int clockspeed, int cspin)
  : pcp(name)
  {
  m_spibus = spibus;
  m_host = host;
  m_clockspeed = clockspeed;
  m_cspin = cspin;

  memset(&m_devcfg, 0, sizeof(spi_nodma_device_interface_config_t));
  m_devcfg.clock_speed_hz=m_clockspeed;     // Clock speed (in hz)
  m_devcfg.mode=0;                          // SPI mode 0
  m_devcfg.command_bits=0;
  m_devcfg.address_bits=0;
  m_devcfg.dummy_bits=0;
  m_devcfg.spics_io_num=m_cspin;
  m_devcfg.queue_size=7;                    // We want to be able to queue 7 transactions at a time

  esp_err_t ret = spi_nodma_bus_add_device(m_host, &m_spibus->m_buscfg, &m_devcfg, &m_spi);
  assert(ret==ESP_OK);

  m_monitor_task = NULL;
  m_monitor_timer = NULL;
  m_inputstate = ~0;
  m_outputstate = ~0;     // power up: all ports in input mode (high)
  m_monitor_ports = 0;
  *StdMetrics.ms_m_egpio_input = m_inputstate;
  *StdMetrics.ms_m_egpio_output = m_outputstate;
  *StdMetrics.ms_m_egpio_monitor = m_monitor_ports;
  }

max7317::~max7317()
  {
  if (m_monitor_timer)
    delete m_monitor_timer;
  if (m_monitor_task)
    vTaskDelete(m_monitor_task);
  }

std::bitset<10> max7317::GetConfigMonitorPorts()
  {
  std::bitset<10> ports;
  // Parse configuration: list of port numbers separated by spaces
  std::string value = MyConfig.GetParamValue("egpio", "monitor.ports");
  std::istringstream vs(value);
  std::string token;
  int port;
  while (std::getline(vs, token, ' '))
    {
    std::istringstream ts(token);
    ts >> port;
    if (port >= 0 && port <= 9)
      ports[port] = 1;
    }
  return ports;
  }

void max7317::AutoInit()
  {
  if (MyConfig.GetParamValueBool("auto", "egpio", false))
    {
    m_monitor_ports = GetConfigMonitorPorts();
    if (!CheckMonitor())
      ESP_LOGE(TAG, "AutoInit: monitoring could not be started");
    }
  }

/**
 * Output: set specific port output state
 */
void max7317::Output(uint8_t port, uint8_t level)
  {
  uint8_t buf[4];

  // Set port:
  m_spibus->spi_cmd(m_spi, buf, 0, 2, port, level);

  // Update state & framework:
  if (!m_outputstate[port] && level)
    {
    m_outputstate[port] = true;
    *StdMetrics.ms_m_egpio_output = m_outputstate;
    std::string event = "egpio.output.";
    event.append(1, '0'+port);
    event.append(".high");
    ESP_LOGD(TAG, "%s", event.c_str());
    MyEvents.SignalEvent(event, NULL);
    }
  else if (m_outputstate[port] && !level)
    {
    m_outputstate[port] = false;
    *StdMetrics.ms_m_egpio_output = m_outputstate;
    std::string event = "egpio.output.";
    event.append(1, '0'+port);
    event.append(".low");
    ESP_LOGD(TAG, "%s", event.c_str());
    MyEvents.SignalEvent(event, NULL);
    }
  }

std::bitset<10> max7317::OutputState()
  {
  return m_outputstate;
  }

/**
 * Input: read specific port input state
 */
uint8_t max7317::Input(uint8_t port)
  {
  uint8_t buf[2];
  uint8_t level;
  OvmsMutexLock lock(&m_monitor_mutex);

  // Read port state:
  //  Note: MAX7317 SPI read needs a deselect between tx and rx (see specs pg. 8).
  //  SPI always does tx & rx in parallel, so we must send a NOP (0x20,0) on the "rx"
  //  command, as the MAX7317 would interpret (0,0) as a write of level 0 to port 0.
  if (port < 8)
    {
    m_spibus->spi_cmd(m_spi, buf, 0, 2, 0x8E, 0);
    m_spibus->spi_deselect(m_spi);
    m_spibus->spi_cmd(m_spi, buf, 0, 2, 0x20, 0);
    level = (buf[1] & (1 << port)) ? 1 : 0;
    }
  else
    {
    m_spibus->spi_cmd(m_spi, buf, 0, 2, 0x8F, 0);
    m_spibus->spi_deselect(m_spi);
    m_spibus->spi_cmd(m_spi, buf, 0, 2, 0x20, 0);
    level = (buf[1] & (1 << (port-8))) ? 1 : 0;
    }

  // Update state & framework:
  if (m_inputstate[port] != level)
    {
    std::bitset<10> pstate = m_inputstate;
    pstate[port] = level;
    *StdMetrics.ms_m_egpio_input = pstate;
    if (!m_inputstate[port] && level)
      {
      std::string event = "egpio.input.";
      event.append(1, '0'+port);
      event.append(".high");
      ESP_LOGD(TAG, "%s", event.c_str());
      MyEvents.SignalEvent(event, NULL);
      }
    else if (m_inputstate[port] && !level)
      {
      std::string event = "egpio.input.";
      event.append(1, '0'+port);
      event.append(".low");
      ESP_LOGD(TAG, "%s", event.c_str());
      MyEvents.SignalEvent(event, NULL);
      }
    m_inputstate[port] = level;
    }

  return level;
  }

/**
 * Inputs: read input states for a set of ports
 */
std::bitset<10> max7317::Inputs(std::bitset<10> ports)
  {
  uint8_t buf[2];
  std::bitset<10> pstate;
  if (ports.none())
    return pstate;
  OvmsMutexLock lock(&m_monitor_mutex);

  // Read port state: see notes above
  if ((ports & std::bitset<10>(0b0011111111)).any())
    {
    m_spibus->spi_cmd(m_spi, buf, 0, 2, 0x8E, 0);
    m_spibus->spi_deselect(m_spi);
    m_spibus->spi_cmd(m_spi, buf, 0, 2, 0x20, 0);
    pstate = buf[1];
    }
  if ((ports & std::bitset<10>(0b1100000000)).any())
    {
    m_spibus->spi_cmd(m_spi, buf, 0, 2, 0x8F, 0);
    m_spibus->spi_deselect(m_spi);
    m_spibus->spi_cmd(m_spi, buf, 0, 2, 0x20, 0);
    pstate |= ((uint16_t)buf[1] << 8);
    }

  // Update state & framework:
  pstate &= ports;
  pstate |= (m_inputstate & ~ports);
  *StdMetrics.ms_m_egpio_input = pstate;
  if ((m_inputstate ^ pstate).any())
    {
    std::string event;
    for (int i=0; i<10; i++)
      {
      if (!ports[i])
        continue;
      else if (!m_inputstate[i] && pstate[i])
        {
        event = "egpio.input.";
        event.append(1, '0'+i);
        event.append(".high");
        ESP_LOGD(TAG, "%s", event.c_str());
        MyEvents.SignalEvent(event, NULL);
        }
      else if (m_inputstate[i] && !pstate[i])
        {
        event = "egpio.input.";
        event.append(1, '0'+i);
        event.append(".low");
        ESP_LOGD(TAG, "%s", event.c_str());
        MyEvents.SignalEvent(event, NULL);
        }
      }
    }
  m_inputstate = pstate;

  return pstate;
  }

std::bitset<10> max7317::InputState()
  {
  return m_inputstate;
  }

/**
 * MonitorTask: poll input state
 */
static void MonitorTaskEntry(void* me)
  {
  ((max7317*)me)->MonitorTask();
  }
void max7317::MonitorTask()
  {
  while (true)
    {
    if (m_monitor_semaphore.Take())
      Inputs(m_monitor_ports);
    }
  }

/**
 * CheckMonitor: activate/deactivate port input monitoring as necessary
 */
bool max7317::CheckMonitor()
  {
  *StdMetrics.ms_m_egpio_monitor = m_monitor_ports;
  if (m_monitor_ports.any())
    {
    // activate monitoring:
    if (!m_monitor_task)
      {
      xTaskCreatePinnedToCore(MonitorTaskEntry, "OVMS EGPIOmon",
        4*512, (void*)this, 15, &m_monitor_task, CORE(1));
      if (!m_monitor_task)
        {
        ESP_LOGE(TAG, "Monitor: unable to create task");
        return false;
        }
      }
    if (!m_monitor_timer)
      {
      m_monitor_timer = new OvmsInterval("EGPIOmon");
      if (!m_monitor_timer)
        {
        ESP_LOGE(TAG, "Monitor: unable to create timer");
        return false;
        }
      }
    if (!m_monitor_timer->IsActive())
      {
      int interval = MyConfig.GetParamValueInt("egpio", "monitor.interval",
        CONFIG_OVMS_COMP_MAX7317_MONITOR_INTERVAL);
      if (interval < 10)
        interval = 10;
      if (!m_monitor_timer->Start(interval, [&]{ m_monitor_semaphore.Give(); }))
        {
        ESP_LOGE(TAG, "Monitor: unable to start timer");
        return false;
        }
      }
    }
  else
    {
    // deactivate monitoring:
    if (m_monitor_timer && !m_monitor_timer->Stop())
      {
      ESP_LOGW(TAG, "Monitor: unable to stop timer");
      return false;
      }
    }
  return true;
  }

/**
 * Monitor: enable/disable input monitoring for specific ports
 */
bool max7317::Monitor(std::bitset<10> ports, bool enable)
  {
  OvmsMutexLock lock(&m_monitor_mutex);
  if (enable)
    m_monitor_ports |= ports;
  else
    m_monitor_ports &= ~ports;
  return CheckMonitor();
  }

bool max7317::Monitor(uint8_t port, bool enable)
  {
  OvmsMutexLock lock(&m_monitor_mutex);
  m_monitor_ports[port] = enable;
  return CheckMonitor();
  }

std::bitset<10> max7317::MonitorState()
  {
  OvmsMutexLock lock(&m_monitor_mutex);
  return m_monitor_ports;
  }


void max7317_output(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  for (int i = 0; i < argc-1; i += 2)
    {
    int port = atoi(argv[i]);
    if ((port <0)||(port>9))
      {
      writer->puts("Error: Port should be in range 0..9");
      return;
      }
    int level = atoi(argv[i+1]);
    if ((level<0)||(level>255))
      {
      writer->puts("Error: Level should be in range 0..255");
      return;
      }

    MyPeripherals->m_max7317->Output((uint8_t)port,(uint8_t)level);
    writer->printf("EGPIO port %d set to output level %d\n", port, level);
    }
  }

void max7317_pulse(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int port = atoi(argv[0]);
  int level = atoi(argv[1]);
  int durationms = atoi(argv[2]);
  int baselevel = 1-level;

  writer->printf("EGPIO port %d set to base level %d\n", port, baselevel);
  MyPeripherals->m_max7317->Output((uint8_t)port,(uint8_t)baselevel);

  writer->printf("EGPIO port %d setting to level %d for %dms\n", port, level, durationms);
  MyPeripherals->m_max7317->Output((uint8_t)port,(uint8_t)level);

  vTaskDelay(pdMS_TO_TICKS(durationms));

  writer->printf("EGPIO port %d set back to base level %d\n", port, baselevel);
  MyPeripherals->m_max7317->Output((uint8_t)port,(uint8_t)baselevel);
  }

void max7317_input(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  for (int i = 0; i < argc; i += 1)
    {
    int port = atoi(argv[i]);
    if ((port <0)||(port>9))
      {
      writer->puts("Error: Port should be in range 0..9");
      return;
      }

    MyPeripherals->m_max7317->Output((uint8_t)port,(uint8_t)1); // set port to input
    int level = MyPeripherals->m_max7317->Input((uint8_t)port);
    writer->printf("EGPIO port %d has input level %d\n", port, level);
    }
  }

void max7317_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd,
  int argc, const char* const* argv)
  {
  max7317* me = MyPeripherals->m_max7317;

  auto printports = [writer](const char* intro, std::bitset<10> ports)
    {
    writer->printf("%s", intro);
    for (int i=0; i<10; i++)
      writer->printf(" %d:%d", i, ports.test(i));
    writer->puts("");
    };

  writer->puts("EGPIO port status:");
  printports("Output :", me->OutputState());
  printports("Input  :", me->InputState());
  printports("Monitor:", me->MonitorState());
  }

void max7317_monitor(int verbosity, OvmsWriter* writer, OvmsCommand* cmd,
  int argc, const char* const* argv)
  {
  max7317* me = MyPeripherals->m_max7317;
  std::bitset<10> ports;

  // Enable/disable port monitoring?
  if (strcmp(cmd->GetName(), "status") != 0)
    {
    bool enable = (strcmp(cmd->GetName(), "on") == 0);
    for (int i=0; i<argc; i++)
      {
      int port = atoi(argv[i]);
      if (port < 0 || port > 9)
        {
        writer->puts("Error: ports must be in range 0..9");
        }
      else
        {
        ports[port] = 1;
        }
      }
    // no ports given on command line?
    if (ports.none())
      {
      if (enable)
        {
        // enable configured ports:
        ports = me->GetConfigMonitorPorts();
        if (ports.none())
          writer->puts("ERROR: no ports configured");
        }
      else
        {
        // disable all enabled ports:
        ports = me->MonitorState();
        }
      }
    // change monitoring:
    if (ports.any())
      {
      if (enable)
        {
        // set ports to input mode:
        for (int i=0; i<10; i++)
          if (ports[i]) me->Output(i, 1);
        }
      if (me->Monitor(ports, enable))
        {
        writer->printf("OK: input monitoring %s for %d port%s.\n",
          enable ? "enabled" : "disabled",
          ports.count(),
          ports.count() == 1 ? "" : "s");
        }
      else
        {
        writer->printf("ERROR: %s input monitoring failed (see logs for details)\n",
          enable ? "enabling" : "disabling");
        }
      }
    }

  // Show monitoring status:
  ports = me->MonitorState();
  if (ports.none())
    {
    writer->puts("Status: input monitoring disabled for all ports.");
    }
  else if (ports.all())
    {
    writer->puts("Status: input monitoring enabled for all ports.");
    }
  else
    {
    writer->printf("Status: input monitoring enabled for ports:");
    for (int i=0; i<10; i++)
      {
      if (ports[i])
        writer->printf(" %d", i);
      }
    writer->puts("");
    }
  }

class Max7317Init
  {
  public: Max7317Init();
} MyMax7317Init  __attribute__ ((init_priority (4200)));

Max7317Init::Max7317Init()
  {
  ESP_LOGI(TAG, "Initialising MAX7317 EGPIO (4200)");

  MyConfig.RegisterParam("egpio", "EGPIO configuration", true, true);

  OvmsCommand* cmd_egpio = MyCommandApp.RegisterCommand(
    "egpio", "EGPIO framework", max7317_status, "", 0, 0, false);
  cmd_egpio->RegisterCommand(
    "output", "Set EGPIO output level(s)", max7317_output,
    "<port> <level> [<port> <level> ...]", 2, 20);
  cmd_egpio->RegisterCommand(
    "pulse", "Pulse EGPIO output level(s)", max7317_pulse,
    "<port> <level> <durationms>", 3, 3);
  cmd_egpio->RegisterCommand(
    "input", "Get EGPIO input level(s)", max7317_input,
    "<port> [<port> ...]", 1, 10);
  cmd_egpio->RegisterCommand(
    "status", "Show EGPIO status", max7317_status);

  OvmsCommand* cmd_mon = cmd_egpio->RegisterCommand(
    "monitor", "EGPIO input monitoring");
  cmd_mon->RegisterCommand(
    "status", "Show input monitoring status", max7317_monitor);
  cmd_mon->RegisterCommand(
    "on", "Enable input monitoring", max7317_monitor,
    "[<port> ...]\n"
    "No ports = enable as configured in egpio/monitor.ports\n"
    "Note: ports specified will be switched to input mode", 0, 10);
  cmd_mon->RegisterCommand(
    "off", "Disable input monitoring", max7317_monitor,
    "[<port> ...]\n"
    "No ports = disable all", 0, 10);
  }
