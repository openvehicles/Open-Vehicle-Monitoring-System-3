#include <esp_log.h>
#include <string>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>
#include "ovms.h"
#include "peripherals.h"
#include "console_async.h"

ConsoleAsync *usbconsole;

extern "C"
  {
  void app_main(void);
  }

void app_main(void)
  {
  nvs_flash_init();

  usbconsole = new ConsoleAsync();
  MyPeripherals.m_esp32can->Init(CAN_SPEED_1000KBPS);
  MyPeripherals.m_mcp2515_1->Init(CAN_SPEED_1000KBPS);
  MyPeripherals.m_mcp2515_2->Init(CAN_SPEED_1000KBPS);
  }
