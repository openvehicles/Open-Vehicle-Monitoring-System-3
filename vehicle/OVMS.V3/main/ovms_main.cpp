#include <esp_log.h>
#include <string>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"

static char tag[]="ovms";

extern "C"
  {
  void app_main(void);
  }

void app_main(void)
  {
  nvs_flash_init();

  ESP_LOGI(tag, "Welcome to the Open Vehicle Monitoring System (OVMS)");
  }
