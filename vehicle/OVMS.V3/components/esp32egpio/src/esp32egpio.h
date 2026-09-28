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

#ifndef __ESP32EGPIO_H__
#define __ESP32EGPIO_H__

#include <stdint.h>
#include "pcp.h"
#include <bitset>
#include "ovms_timer.h"
#include "ovms_semaphore.h"
#include "ovms_mutex.h"
#include "driver/gpio.h"

/**
 * @brief ESP32 EGPIO - Direct GPIO control emulating MAX7317 interface
 *
 * This class provides the same interface as max7317 but uses direct ESP32 GPIO
 * pins instead of an external SPI I/O expander. This is particularly useful for
 * boards like Lilygo T-Call that don't have a MAX7317 chip.
 *
 * Features:
 * - 10 configurable GPIO ports (0-9) mapped to ESP32 GPIO pins
 * - Output control with state tracking
 * - Input reading with optional monitoring/polling
 * - Event generation on input state changes
 * - Metrics integration (compatible with standard EGPIO metrics)
 * - Command interface (works with standard 'egpio' commands)
 */
class esp32egpio : public pcp, public InternalRamAllocated
  {
  public:
    esp32egpio(const char* name, const int8_t* gpio_map);
    ~esp32egpio();

  public:
    // Main API - compatible with max7317
    void Output(uint8_t port, uint8_t level);
    std::bitset<10> OutputState();
    uint8_t Input(uint8_t port);
    std::bitset<10> Inputs(std::bitset<10> ports);
    std::bitset<10> InputState();
    bool Monitor(std::bitset<10> ports, bool enable);
    bool Monitor(uint8_t port, bool enable);
    std::bitset<10> MonitorState();

  public:
    // Configuration and initialization
    std::bitset<10> GetConfigMonitorPorts();
    void AutoInit();
    bool CheckMonitor();
    void MonitorTask();

  protected:
    // GPIO pin mapping: index = EGPIO port number, value = GPIO pin number (-1 = unused)
    int8_t m_gpio_map[10];

    // State tracking (same as max7317)
    std::bitset<10> m_outputstate;
    std::bitset<10> m_inputstate;
    std::bitset<10> m_monitor_ports;

    // Monitoring infrastructure (same as max7317)
    OvmsInterval* m_monitor_timer;
    OvmsSemaphore m_monitor_semaphore;
    OvmsMutex m_monitor_mutex;
    TaskHandle_t m_monitor_task;

  protected:
    // Internal helpers
    bool IsValidPort(uint8_t port);
    bool IsConfiguredPort(uint8_t port);
    void InitializeGPIO(uint8_t port, gpio_mode_t mode);
  };

#endif // __ESP32EGPIO_H__
