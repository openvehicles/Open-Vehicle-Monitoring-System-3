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
#include "max7317.h"
#include "esp32can.h"
#include "esp32wifi.h"
#include "esp32bluetooth.h"
#include "esp32adc.h"
#include "mcp2515.h"

#ifdef CONFIG_OVMS_COMP_SDCARD
#include "sdcard.h"
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

#ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM
#include "simcom.h"
#endif // #ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM

#ifdef CONFIG_OVMS_COMP_OBD2ECU
#include "obd2ecu.h"
#endif // #ifdef CONFIG_OVMS_COMP_OBD2ECU

#include "ext12v.h"

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
#define SDCARD_PIN_D1             4
#define SDCARD_PIN_D2             12
#define SDCARD_PIN_D3             13
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

#ifdef CONFIG_OVMS_HW_BASE_3_0
#define MODEM_GPIO_RX             16
#define MODEM_GPIO_TX             17
#define MODEM_PIN_RX              27
#define MODEM_PIN_TX              28
#else // #ifdef CONFIG_OVMS_HW_BASE_3_0
#define MODEM_GPIO_RX             4
#define MODEM_GPIO_TX             13
#define MODEM_PIN_RX              26
#define MODEM_PIN_TX              16
#endif // #ifdef CONFIG_OVMS_HW_BASE_3_0
#define MODEM_EGPIO_PWR           0
#define MODEM_EGPIO_DTR           3

class Peripherals
  {
  public:
     Peripherals();
     ~Peripherals();

  public:
    esp32system* m_esp32;
    spi* m_spibus;
    max7317* m_max7317;
    esp32can* m_esp32can;
    esp32wifi* m_esp32wifi;
    esp32bluetooth* m_esp32bluetooth;
    esp32adc* m_esp32adc;
    mcp2515* m_mcp2515_1;
    mcp2515* m_mcp2515_2;
#ifdef CONFIG_OVMS_COMP_SDCARD
    sdcard* m_sdcard;
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD
#ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM
    simcom* m_simcom;
#endif // #ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM
#ifdef CONFIG_OVMS_COMP_OBD2ECU
    obd2ecu* m_obd2ecu;
#endif // #ifdef CONFIG_OVMS_COMP_OBD2ECU
    ext12v* m_ext12v;
  };

extern Peripherals* MyPeripherals;

#endif //#ifndef __PERIPHERALS_H__
