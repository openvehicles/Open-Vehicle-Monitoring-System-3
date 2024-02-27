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
static const char *TAG = "peripherals";

#include <stdio.h>
#include <string.h>
#include <cstdio>
#include "ovms_config.h"
#include "ovms_command.h"
#include "esp_intr_alloc.h"
#include "driver/gpio.h"
#include "ovms_peripherals.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION_MAJOR >= 5
#include <esp_mac.h>
#endif

Peripherals::Peripherals()
  {
  ESP_LOGI(TAG, "Initialising OVMS Peripherals...");

#if defined(CONFIG_OVMS_COMP_WIFI)||defined(CONFIG_OVMS_COMP_CELLULAR)
  if (MyConfig.IsDefined("network","mac"))
    {
    std::string mac = MyConfig.GetParamValue("network", "mac");
    int mac_addr_k[6];
    uint8_t mac_addr[8];
    memset(mac_addr,0,sizeof(mac_addr));
    if (std::sscanf(mac.c_str(),
                    "%02x:%02x:%02x:%02x:%02x:%02x",
                    &mac_addr_k[0], &mac_addr_k[1], &mac_addr_k[2],
                    &mac_addr_k[3], &mac_addr_k[4], &mac_addr_k[5]) == 6)
      {
      for (int k=0;k<6;k++) mac_addr[k] = mac_addr_k[k];
      esp_base_mac_addr_set(mac_addr);
      ESP_LOGI(TAG, "  Base network MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
        mac_addr[0], mac_addr[1], mac_addr[2],
        mac_addr[3], mac_addr[4], mac_addr[5]);
      }
    }
#if ESP_IDF_VERSION_MAJOR >= 4
  ESP_LOGI(TAG, "  ESP-NETIF");
  ESP_ERROR_CHECK(esp_netif_init());
  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();
#else
  ESP_LOGI(TAG, "  TCP/IP Adaptor");
  tcpip_adapter_init();
#endif
#endif // #if defined(CONFIG_OVMS_COMP_WIFI)||defined(CONFIG_OVMS_COMP_CELLULAR)

  gpio_install_isr_service(ESP_INTR_FLAG_IRAM);

  gpio_set_direction((gpio_num_t)VSPI_PIN_MISO, GPIO_MODE_INPUT);
  gpio_set_direction((gpio_num_t)VSPI_PIN_MOSI, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)VSPI_PIN_CLK, GPIO_MODE_OUTPUT);

#ifdef CONFIG_OVMS_COMP_MAX7317
  gpio_set_direction((gpio_num_t)VSPI_PIN_MAX7317_CS, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)VSPI_PIN_MAX7317_CS, 1); // to prevent SPI crosstalk during initialization
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317

#ifdef CONFIG_OVMS_COMP_MCP2515
  gpio_set_direction((gpio_num_t)VSPI_PIN_MCP2515_1_CS, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)VSPI_PIN_MCP2515_2_CS, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)VSPI_PIN_MCP2515_1_INT, GPIO_MODE_INPUT);
  gpio_set_direction((gpio_num_t)VSPI_PIN_MCP2515_2_INT, GPIO_MODE_INPUT);
  gpio_set_level((gpio_num_t)VSPI_PIN_MCP2515_1_CS, 1); // to prevent SPI crosstalk during initialization
  gpio_set_level((gpio_num_t)VSPI_PIN_MCP2515_2_CS, 1); // to prevent SPI crosstalk during initialization
#endif // #ifdef CONFIG_OVMS_COMP_MCP2515

#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  gpio_set_direction((gpio_num_t)VSPI_PIN_MCP2515_SWCAN_CS, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)VSPI_PIN_MCP2515_SWCAN_INT, GPIO_MODE_INPUT);
  gpio_set_level((gpio_num_t)VSPI_PIN_MCP2515_SWCAN_CS, 1); // to prevent SPI crosstalk during initialization
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN

#ifdef CONFIG_OVMS_COMP_SDCARD
  gpio_set_direction((gpio_num_t)SDCARD_PIN_CLK, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)SDCARD_PIN_CMD, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)SDCARD_PIN_CD, GPIO_MODE_INPUT);

  // GPIOs CMD, D0 should have external 10k pull-ups.
  // Internal pull-ups are not sufficient. However, enabling internal pull-ups
  // does make a difference some boards, so we do that here.
  gpio_set_pull_mode((gpio_num_t)SDCARD_PIN_CMD, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
  gpio_set_pull_mode((gpio_num_t)SDCARD_PIN_D0, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

  ESP_LOGI(TAG, "  ESP32 system");
  m_esp32 = new esp32system("esp32");

  ESP_LOGI(TAG, "  SPI bus");
  m_spibus = new spi("spi", VSPI_PIN_MISO, VSPI_PIN_MOSI, VSPI_PIN_CLK);

#ifdef CONFIG_OVMS_COMP_MAX7317
  ESP_LOGI(TAG, "  MAX7317 I/O Expander");
  m_max7317 = new max7317("egpio", m_spibus, VSPI_HOST, 10000000, VSPI_PIN_MAX7317_CS);
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317

#ifdef CONFIG_OVMS_COMP_ESP32CAN
  ESP_LOGI(TAG, "  ESP32 CAN");
  m_esp32can = new esp32can("can1", ESP32CAN_PIN_TX, ESP32CAN_PIN_RX);
#endif // #ifdef CONFIG_OVMS_COMP_ESP32CAN

#ifdef CONFIG_OVMS_COMP_WIFI
  ESP_LOGI(TAG, "  ESP32 WIFI");
  m_esp32wifi = new esp32wifi("wifi");
#endif // #ifdef CONFIG_OVMS_COMP_WIFI

#ifdef CONFIG_OVMS_COMP_BLUETOOTH
  ESP_LOGI(TAG, "  ESP32 BLUETOOTH");
  m_esp32bluetooth = new esp32bluetooth("bluetooth");
#endif // #ifdef CONFIG_OVMS_COMP_BLUETOOTH

#ifdef CONFIG_OVMS_COMP_ADC
  ESP_LOGI(TAG, "  ESP32 ADC");
  m_esp32adc = new esp32adc("adc", ADC1_CHANNEL_0, ADC_WIDTH_BIT_12, ADC_ATTEN_DB_11);

#endif // #ifdef CONFIG_OVMS_COMP_ADC

#ifdef CONFIG_OVMS_COMP_MCP2515
  ESP_LOGI(TAG, "  MCP2515 CAN 1/2");
  m_mcp2515_1 = new mcp2515("can2", m_spibus, VSPI_HOST, 10000000, VSPI_PIN_MCP2515_1_CS, VSPI_PIN_MCP2515_1_INT);
#ifndef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  ESP_LOGI(TAG, "  MCP2515 CAN 2/2");
  m_mcp2515_2 = new mcp2515("can3", m_spibus, VSPI_HOST, 10000000, VSPI_PIN_MCP2515_2_CS, VSPI_PIN_MCP2515_2_INT);
#else
  ESP_LOGI(TAG, "  can3/swcan (MCP2515 + TH8056 DRIVER)");
  // External SWCAN module with MCP2515
  m_mcp2515_swcan = new swcan("can3", m_spibus, VSPI_HOST, 10000000, VSPI_PIN_MCP2515_SWCAN_CS, VSPI_PIN_MCP2515_SWCAN_INT);
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
#endif // #ifdef CONFIG_OVMS_COMP_MCP2515

#ifdef CONFIG_OVMS_COMP_SDCARD
  ESP_LOGI(TAG, "  SD CARD");
  m_sdcard = new sdcard("sdcard", true, true, SDCARD_PIN_CD);
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

#ifdef CONFIG_OVMS_COMP_CELLULAR
  ESP_LOGI(TAG, "  CELLULAR MODEM");
  gpio_config_t gpio_conf =
    {
    .pin_bit_mask = BIT(MODEM_GPIO_RX),
    .mode = GPIO_MODE_OUTPUT ,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
    };
  gpio_config( &gpio_conf );
  gpio_conf =
    {
    .pin_bit_mask = BIT(MODEM_GPIO_TX),
    .mode = GPIO_MODE_INPUT ,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
    };
  gpio_config( &gpio_conf );
  m_cellular_modem = new modem("cellular", UART_NUM_1, 115200, MODEM_GPIO_RX, MODEM_GPIO_TX, MODEM_EGPIO_PWR, MODEM_EGPIO_DTR);
#endif // #ifdef CONFIG_OVMS_COMP_CELLULAR

#ifdef CONFIG_OVMS_COMP_OBD2ECU
  m_obd2ecu = NULL;
#endif // #ifdef CONFIG_OVMS_COMP_OBD2ECU

#ifdef CONFIG_OVMS_COMP_EXT12V
  m_ext12v = new ext12v("ext12v");
#endif // #ifdef CONFIG_OVMS_COMP_EXT12V
  }

Peripherals::~Peripherals()
  {
#if ESP_IDF_VERSION_MAJOR >= 4
  esp_netif_deinit();
#endif
  }
