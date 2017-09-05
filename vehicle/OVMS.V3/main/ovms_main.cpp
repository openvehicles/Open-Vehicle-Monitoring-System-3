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
#include "housekeeping.h"
#include "console_async.h"
#include "config.h"

ConsoleAsync *usbconsole;

extern "C"
  {
  void app_main(void);
  }

Housekeeping* MyHousekeeping = NULL;
Peripherals* MyPeripherals = NULL;

void app_main(void)
  {
  nvs_flash_init();

  printf("ovms_main: Executing on CPU core %d\n",xPortGetCoreID());

  puts("omvs_main: Mounting CONFIG...");
  MyConfig.mount();

  puts("ovms_main: Starting PERIPHERALS...");
  MyPeripherals = new Peripherals();

  puts("ovms_main: Starting HOUSEKEEPING...");
  MyHousekeeping = new Housekeeping();

  puts("ovms_main: Starting USB console...");
  usbconsole = new ConsoleAsync();
  }
