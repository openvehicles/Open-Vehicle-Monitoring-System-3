#include "esp_log.h"
static const char *TAG = "ovms_main";

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

  ESP_LOGI(TAG, "Executing on CPU core %d",xPortGetCoreID());

  ESP_LOGI(TAG, "Mounting CONFIG...");
  MyConfig.mount();

  ESP_LOGI(TAG, "Starting PERIPHERALS...");
  MyPeripherals = new Peripherals();

  ESP_LOGI(TAG, "Starting HOUSEKEEPING...");
  MyHousekeeping = new Housekeeping();

  ESP_LOGI(TAG, "Starting USB console...");
  usbconsole = new ConsoleAsync();
  }
