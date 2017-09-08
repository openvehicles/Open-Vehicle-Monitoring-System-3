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
#include "ovms_peripherals.h"
#include "ovms_housekeeping.h"
#include "ovms_events.h"
#include "console_async.h"
#include "ovms_config.h"

ConsoleAsync *usbconsole;

extern "C"
  {
  void app_main(void);
  }

Housekeeping* MyHousekeeping = NULL;
Peripherals* MyPeripherals = NULL;

int console_logger(const char *format, va_list arg)
  {
  char *buffer = (char*)malloc(512);
  vsnprintf(buffer, 512, format, arg);
  int k = strlen(buffer);
  usbconsole->Log(buffer);
  return k;
  }

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

  esp_log_set_vprintf(console_logger);

  MyEvents.SignalEvent("system.start",NULL);
  }
