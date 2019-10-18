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

#ifndef __PERIPHERALS_H__
#define __PERIPHERALS_H__

#include "pcp.h"
#include "spi.h"
#include "esp32system.h"

#ifdef CONFIG_OVMS_COMP_ADC
#include "esp32adc.h"
#endif // #ifdef CONFIG_OVMS_COMP_ADC

#ifdef CONFIG_OVMS_COMP_MCP2515
#include "mcp2515.h"
#endif // #ifdef CONFIG_OVMS_COMP_MCP2515

#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
#include "swcan.h"
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN

#ifdef CONFIG_OVMS_COMP_ESP32CAN
#include "esp32can.h"
#endif // #ifdef CONFIG_OVMS_COMP_ESP32CAN

#ifdef CONFIG_OVMS_COMP_MAX7317
#include "max7317.h"
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317

#ifdef CONFIG_OVMS_COMP_BLUETOOTH
#include "esp32bluetooth.h"
#endif // #ifdef CONFIG_OVMS_COMP_BLUETOOTH

#ifdef CONFIG_OVMS_COMP_WIFI
#include "esp32wifi.h"
#endif // #ifdef CONFIG_OVMS_COMP_WIFI

#ifdef CONFIG_OVMS_COMP_SDCARD
#include "sdcard.h"
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

#ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM
#include "simcom.h"
#endif // #ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM

#ifdef CONFIG_OVMS_COMP_OBD2ECU
#include "obd2ecu.h"
#endif // #ifdef CONFIG_OVMS_COMP_OBD2ECU

#ifdef CONFIG_OVMS_COMP_EXT12V
#include "ext12v.h"
#endif // #ifdef CONFIG_OVMS_COMP_EXT12V

#define MODULE_GPIO_SW2           0       // SW2: firmware download / factory reset

#define VSPI_PIN_MISO             19
#define VSPI_PIN_MOSI             23
#define VSPI_PIN_CLK              18
#define VSPI_PIN_MCP2515_1_CS     5
#define VSPI_PIN_MAX7317_CS       21
#define VSPI_PIN_MCP2515_2_CS     27
#define VSPI_PIN_MCP2515_1_INT    34
#define VSPI_PIN_MCP2515_2_INT    35

#define SDCARD_PIN_CLK            14
#define SDCARD_PIN_CMD            15
#define SDCARD_PIN_D0             2
#define SDCARD_PIN_CD             39

#define ESP32CAN_PIN_TX           25
#define ESP32CAN_PIN_RX           26

#define MAX7317_MDM_EN            0
#define MAX7317_SW_CTL            1
#define MAX7317_CAN1_EN           2
#define MAX7317_MDM_DTR           3
#define MAX7317_EGPIO_1           2
#define MAX7317_EGPIO_2           3
#define MAX7317_EGPIO_3           4
#define MAX7317_EGPIO_4           5
#define MAX7317_EGPIO_5           6
#define MAX7317_EGPIO_6           7
#define MAX7317_EGPIO_7           8
#define MAX7317_EGPIO_8           9

#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
#define MAX7317_SWCAN_MODE0         4   // EGPIO_3
#define MAX7317_SWCAN_MODE1         5   // EGPIO_4
#define VSPI_PIN_MCP2515_SWCAN_CS   33  // EXP2
#define VSPI_PIN_MCP2515_SWCAN_INT  32  // EXP1
#define MAX7317_SWCAN_STATUS_LED    7   // EGPIO_6
#define MAX7317_SWCAN_TX_LED        8   // EGPIO_7
#define MAX7317_SWCAN_RX_LED        9   // EGPIO_8
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN

#ifdef CONFIG_OVMS_HW_BASE_3_0
#define MODEM_GPIO_RX             16
#define MODEM_GPIO_TX             17
#endif // #ifdef CONFIG_OVMS_HW_BASE_3_0

#ifdef CONFIG_OVMS_HW_BASE_3_1
#define MODEM_GPIO_RX             13
#define MODEM_GPIO_TX             4
#endif // #ifdef CONFIG_OVMS_HW_BASE_3_1

#define MODEM_EGPIO_PWR           0
#define MODEM_EGPIO_DTR           3

class Peripherals : public InternalRamAllocated
  {
  public:
     Peripherals();
     ~Peripherals();

  public:
    esp32system* m_esp32;
    spi* m_spibus;

#ifdef CONFIG_OVMS_COMP_MAX7317
    max7317* m_max7317;
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317

#ifdef CONFIG_OVMS_COMP_ESP32CAN
    esp32can* m_esp32can;
#endif // #ifdef CONFIG_OVMS_COMP_ESP32CAN

#ifdef CONFIG_OVMS_COMP_WIFI
    esp32wifi* m_esp32wifi;
#endif // #ifdef CONFIG_OVMS_COMP_WIFI

#ifdef CONFIG_OVMS_COMP_BLUETOOTH
    esp32bluetooth* m_esp32bluetooth;
#endif // #ifdef CONFIG_OVMS_COMP_BLUETOOTH

#ifdef CONFIG_OVMS_COMP_ADC
    esp32adc* m_esp32adc;
#endif // #ifdef CONFIG_OVMS_COMP_ADC

#ifdef CONFIG_OVMS_COMP_MCP2515
    mcp2515* m_mcp2515_1;
    mcp2515* m_mcp2515_2;
#endif // #ifdef CONFIG_OVMS_COMP_MCP2515

#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
    swcan* m_mcp2515_swcan;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN

#ifdef CONFIG_OVMS_COMP_SDCARD
    sdcard* m_sdcard;
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

#ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM
    simcom* m_simcom;
#endif // #ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM

#ifdef CONFIG_OVMS_COMP_OBD2ECU
    obd2ecu* m_obd2ecu;
#endif // #ifdef CONFIG_OVMS_COMP_OBD2ECU

#ifdef CONFIG_OVMS_COMP_EXT12V
    ext12v* m_ext12v;
#endif // #ifdef CONFIG_OVMS_COMP_EXT12V
  };

extern Peripherals* MyPeripherals;

#endif //#ifndef __PERIPHERALS_H__
