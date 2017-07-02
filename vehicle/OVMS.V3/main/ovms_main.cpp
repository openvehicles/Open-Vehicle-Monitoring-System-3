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
  }
