/*
;    Project:       Open Vehicle Monitor System
;    Date:          November 2025
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2025       Carsten Schmiemann
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
static const char *TAG = "esp32egpio";

#include <string.h>
#include <sstream>
#include "esp32egpio.h"
#include "ovms_command.h"
#include "ovms_peripherals.h"
#include "ovms_config.h"
#include "ovms_events.h"
#include "metrics_standard.h"

// Default monitoring interval in milliseconds
#ifndef CONFIG_OVMS_COMP_ESP32EGPIO_MONITOR_INTERVAL
#define CONFIG_OVMS_COMP_ESP32EGPIO_MONITOR_INTERVAL 50
#endif

/**
 * Constructor
 * @param name Component name (e.g., "egpio")
 * @param gpio_map Array of 10 GPIO pin numbers (-1 = unused port)
 */
esp32egpio::esp32egpio(const char* name, const int8_t* gpio_map)
  : pcp(name)
  {
  // Copy GPIO mapping
  memcpy(m_gpio_map, gpio_map, sizeof(m_gpio_map));

  // Initialize all configured GPIOs to input mode (high impedance)
  for (uint8_t port = 0; port < 10; port++)
    {
    if (IsConfiguredPort(port))
      {
      InitializeGPIO(port, GPIO_MODE_INPUT);
      ESP_LOGI(TAG, "Port %d mapped to GPIO %d", port, m_gpio_map[port]);
      }
    }

  m_monitor_task = NULL;
  m_monitor_timer = NULL;
  m_inputstate = ~0;      // all inputs default to high
  m_outputstate = ~0;     // power up: all ports in input mode (high)
  m_monitor_ports = 0;

  // Initialize standard metrics
  *StdMetrics.ms_m_egpio_input = m_inputstate;
  *StdMetrics.ms_m_egpio_output = m_outputstate;
  *StdMetrics.ms_m_egpio_monitor = m_monitor_ports;
  }

esp32egpio::~esp32egpio()
  {
  if (m_monitor_timer)
    delete m_monitor_timer;
  if (m_monitor_task)
    vTaskDelete(m_monitor_task);
  }

/**
 * Helper: Check if port number is valid (0-9)
 */
bool esp32egpio::IsValidPort(uint8_t port)
  {
  return (port < 10);
  }

/**
 * Helper: Check if port is configured (has a valid GPIO mapping)
 */
bool esp32egpio::IsConfiguredPort(uint8_t port)
  {
  return IsValidPort(port) && (m_gpio_map[port] >= 0);
  }

/**
 * Helper: Initialize GPIO pin with specified mode
 */
void esp32egpio::InitializeGPIO(uint8_t port, gpio_mode_t mode)
  {
  if (!IsConfiguredPort(port))
    return;

  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = (1ULL << m_gpio_map[port]);
  io_conf.mode = mode;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&io_conf);
  }

/**
 * Output: set specific port output state
 */
void esp32egpio::Output(uint8_t port, uint8_t level)
  {
  if (!IsConfiguredPort(port))
    {
    ESP_LOGW(TAG, "Output: port %d not configured", port);
    return;
    }

  // Set GPIO to output mode if not already
  InitializeGPIO(port, GPIO_MODE_OUTPUT);

  // Set output level
  gpio_set_level((gpio_num_t)m_gpio_map[port], level ? 1 : 0);

  // Update state & framework (emit events on state changes)
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

std::bitset<10> esp32egpio::OutputState()
  {
  return m_outputstate;
  }

/**
 * Input: read specific port input state
 */
uint8_t esp32egpio::Input(uint8_t port)
  {
  if (!IsConfiguredPort(port))
    {
    ESP_LOGW(TAG, "Input: port %d not configured", port);
    return 0;
    }

  OvmsMutexLock lock(&m_monitor_mutex);

  // Set GPIO to input mode if not already
  InitializeGPIO(port, GPIO_MODE_INPUT);

  // Read GPIO level
  uint8_t level = gpio_get_level((gpio_num_t)m_gpio_map[port]);

  // Update state & framework (emit events on state changes)
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
std::bitset<10> esp32egpio::Inputs(std::bitset<10> ports)
  {
  std::bitset<10> pstate;
  if (ports.none())
    return pstate;

  OvmsMutexLock lock(&m_monitor_mutex);

  // Read all requested ports
  for (uint8_t port = 0; port < 10; port++)
    {
    if (ports[port] && IsConfiguredPort(port))
      {
      // Set to input mode if needed
      InitializeGPIO(port, GPIO_MODE_INPUT);
      // Read level
      pstate[port] = gpio_get_level((gpio_num_t)m_gpio_map[port]);
      }
    }

  // Update state & framework (emit events on state changes)
  pstate &= ports;
  pstate |= (m_inputstate & ~ports);
  for (uint8_t port = 0; port < 10; port++)
    {
    if (ports[port] && (m_inputstate[port] != pstate[port]))
      {
      if (!m_inputstate[port] && pstate[port])
        {
        std::string event = "egpio.input.";
        event.append(1, '0'+port);
        event.append(".high");
        ESP_LOGD(TAG, "%s", event.c_str());
        MyEvents.SignalEvent(event, NULL);
        }
      else if (m_inputstate[port] && !pstate[port])
        {
        std::string event = "egpio.input.";
        event.append(1, '0'+port);
        event.append(".low");
        ESP_LOGD(TAG, "%s", event.c_str());
        MyEvents.SignalEvent(event, NULL);
        }
      }
    }
  m_inputstate = pstate;
  *StdMetrics.ms_m_egpio_input = m_inputstate;

  return pstate;
  }

std::bitset<10> esp32egpio::InputState()
  {
  return m_inputstate;
  }

/**
 * MonitorTask: poll input state
 */
static void MonitorTaskEntry(void* me)
  {
  ((esp32egpio*)me)->MonitorTask();
  }

void esp32egpio::MonitorTask()
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
bool esp32egpio::CheckMonitor()
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
        CONFIG_OVMS_COMP_ESP32EGPIO_MONITOR_INTERVAL);
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
bool esp32egpio::Monitor(std::bitset<10> ports, bool enable)
  {
  OvmsMutexLock lock(&m_monitor_mutex);
  if (enable)
    m_monitor_ports |= ports;
  else
    m_monitor_ports &= ~ports;
  return CheckMonitor();
  }

bool esp32egpio::Monitor(uint8_t port, bool enable)
  {
  OvmsMutexLock lock(&m_monitor_mutex);
  m_monitor_ports[port] = enable;
  return CheckMonitor();
  }

std::bitset<10> esp32egpio::MonitorState()
  {
  OvmsMutexLock lock(&m_monitor_mutex);
  return m_monitor_ports;
  }

/**
 * GetConfigMonitorPorts: parse configuration for monitored ports
 */
std::bitset<10> esp32egpio::GetConfigMonitorPorts()
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

/**
 * AutoInit: initialize from configuration on startup
 */
void esp32egpio::AutoInit()
  {
  if (MyConfig.GetParamValueBool("auto", "egpio", false))
    {
    m_monitor_ports = GetConfigMonitorPorts();
    if (!CheckMonitor())
      ESP_LOGE(TAG, "AutoInit: monitoring could not be started");
    }
  }


// ===================================================================
// COMMAND INTERFACE (compatible with max7317 commands)
// ===================================================================

static void esp32egpio_output(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
#ifdef CONFIG_OVMS_COMP_ESP32EGPIO
  if (!MyPeripherals || !MyPeripherals->m_esp32egpio)
    {
    writer->puts("Error: EGPIO component not available");
    return;
    }

  for (int i=0; i<argc; i+=2)
    {
    int port = atoi(argv[i]);
    int level = atoi(argv[i+1]);
    if (port < 0 || port > 9)
      {
      writer->printf("Error: port %d out of range (0-9)\n", port);
      continue;
      }
    MyPeripherals->m_esp32egpio->Output((uint8_t)port, (uint8_t)level);
    }
#endif
  }

static void esp32egpio_pulse(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
#ifdef CONFIG_OVMS_COMP_ESP32EGPIO
  if (!MyPeripherals || !MyPeripherals->m_esp32egpio)
    {
    writer->puts("Error: EGPIO component not available");
    return;
    }

  int port = atoi(argv[0]);
  int level = atoi(argv[1]);
  int duration = atoi(argv[2]);

  if (port < 0 || port > 9)
    {
    writer->printf("Error: port %d out of range (0-9)\n", port);
    return;
    }

  MyPeripherals->m_esp32egpio->Output((uint8_t)port, (uint8_t)level);
  vTaskDelay(pdMS_TO_TICKS(duration));
  MyPeripherals->m_esp32egpio->Output((uint8_t)port, level ? 0 : 1);
#endif
  }

static void esp32egpio_input(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
#ifdef CONFIG_OVMS_COMP_ESP32EGPIO
  if (!MyPeripherals || !MyPeripherals->m_esp32egpio)
    {
    writer->puts("Error: EGPIO component not available");
    return;
    }

  for (int i=0; i<argc; i++)
    {
    int port = atoi(argv[i]);
    if (port < 0 || port > 9)
      {
      writer->printf("Error: port %d out of range (0-9)\n", port);
      continue;
      }
    int level = MyPeripherals->m_esp32egpio->Input((uint8_t)port);
    writer->printf("Port %d: %d\n", port, level);
    }
#endif
  }

static void esp32egpio_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
#ifdef CONFIG_OVMS_COMP_ESP32EGPIO
  if (!MyPeripherals || !MyPeripherals->m_esp32egpio)
    {
    writer->puts("Error: EGPIO component not available");
    return;
    }

  std::bitset<10> output = MyPeripherals->m_esp32egpio->OutputState();
  std::bitset<10> input = MyPeripherals->m_esp32egpio->InputState();
  std::bitset<10> monitor = MyPeripherals->m_esp32egpio->MonitorState();

  writer->puts("Port  Output  Input  Monitor");
  for (int i=0; i<10; i++)
    {
    writer->printf(" %d      %d       %d      %d\n",
      i, (int)output.test(i), (int)input.test(i), (int)monitor.test(i));
    }
#endif
  }

static void esp32egpio_monitor(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
#ifdef CONFIG_OVMS_COMP_ESP32EGPIO
  if (!MyPeripherals || !MyPeripherals->m_esp32egpio)
    {
    writer->puts("Error: EGPIO component not available");
    return;
    }

  std::string cmdname = cmd->GetName();

  if (cmdname == "status")
    {
    std::bitset<10> ports = MyPeripherals->m_esp32egpio->MonitorState();
    if (ports.none())
      {
      writer->puts("Status: input monitoring is disabled.");
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
  else if (cmdname == "on")
    {
    std::bitset<10> ports;
    if (argc == 0)
      {
      ports = MyPeripherals->m_esp32egpio->GetConfigMonitorPorts();
      }
    else
      {
      for (int i=0; i<argc; i++)
        {
        int port = atoi(argv[i]);
        if (port >= 0 && port <= 9)
          ports[port] = 1;
        }
      }
    if (!MyPeripherals->m_esp32egpio->Monitor(ports, true))
      writer->puts("Error: could not enable monitoring");
    else
      writer->puts("Monitoring enabled");
    }
  else if (cmdname == "off")
    {
    std::bitset<10> ports;
    if (argc == 0)
      {
      ports = ~0;  // all ports
      }
    else
      {
      for (int i=0; i<argc; i++)
        {
        int port = atoi(argv[i]);
        if (port >= 0 && port <= 9)
          ports[port] = 1;
        }
      }
    if (!MyPeripherals->m_esp32egpio->Monitor(ports, false))
      writer->puts("Error: could not disable monitoring");
    else
      writer->puts("Monitoring disabled");
    }
#endif
  }

// ===================================================================
// INITIALIZATION CLASS
// ===================================================================

class Esp32EgpioInit
  {
  public: Esp32EgpioInit();
  } MyEsp32EgpioInit  __attribute__ ((init_priority (4200)));

Esp32EgpioInit::Esp32EgpioInit()
  {
  ESP_LOGI(TAG, "Initialising ESP32 EGPIO (4200)");

  MyConfig.RegisterParam("egpio", "EGPIO configuration", true, true);

  OvmsCommand* cmd_egpio = MyCommandApp.RegisterCommand(
    "egpio", "EGPIO framework", esp32egpio_status, "", 0, 0, false);
  cmd_egpio->RegisterCommand(
    "output", "Set EGPIO output level(s)", esp32egpio_output,
    "<port> <level> [<port> <level> ...]", 2, 20);
  cmd_egpio->RegisterCommand(
    "pulse", "Pulse EGPIO output level(s)", esp32egpio_pulse,
    "<port> <level> <durationms>", 3, 3);
  cmd_egpio->RegisterCommand(
    "input", "Get EGPIO input level(s)", esp32egpio_input,
    "<port> [<port> ...]", 1, 10);
  cmd_egpio->RegisterCommand(
    "status", "Show EGPIO status", esp32egpio_status);

  OvmsCommand* cmd_mon = cmd_egpio->RegisterCommand(
    "monitor", "EGPIO input monitoring");
  cmd_mon->RegisterCommand(
    "status", "Show input monitoring status", esp32egpio_monitor);
  cmd_mon->RegisterCommand(
    "on", "Enable input monitoring", esp32egpio_monitor,
    "[<port> ...]\n"
    "No ports = enable as configured in egpio/monitor.ports\n"
    "Note: ports specified will be switched to input mode", 0, 10);
  cmd_mon->RegisterCommand(
    "off", "Disable input monitoring", esp32egpio_monitor,
    "[<port> ...]\n"
    "No ports = disable all", 0, 10);
  }
