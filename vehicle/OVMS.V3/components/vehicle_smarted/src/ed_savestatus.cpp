/*
 * SaveStatus to File
 */
 
#include "ovms_log.h"
static const char *TAG = "v-smarted";

#include <fstream>
#include <sdkconfig.h>
#include "ovms_peripherals.h"
#include "vehicle_smarted.h"


void OvmsVehicleSmartED::SaveStatus() {
	if (MyPeripherals->m_sdcard->isavailable()) {
    if (!path_exists("/sd/usr/") && mkpath("/sd/usr/") != 0) {
      return;
    }
    FILE *sf = NULL;
    sf = fopen("/sd/usr/SmartEDsatus.dat", "w");
    if (sf == NULL) {
      return;
    }

    for (OvmsMetric* m=MyMetrics.m_first; m != NULL; m=m->m_next) {
      const char *k = m->m_name;
      std::string v = m->AsString();
      
      if (!v.empty()) {
        if (strncmp(m->m_name,"m.",2)!=0 && 
            strncmp(m->m_name,"s.v2.",5)!=0 &&
            strncmp(m->m_name,"s.v3.",5)!=0 &&
          //  strncmp(m->m_name,"v.b.c.temp",10)!=0 &&
          //  strncmp(m->m_name,"v.b.c.voltage",13)!=0 &&
            strncmp(m->m_name,"v.e.on",6)!=0 &&
            strncmp(m->m_name,"v.e.awake",9)!=0
           ) {
          fprintf(sf, "%s %s\n",k,v.c_str());
        }
      }
    }
    ESP_LOGI(TAG, "SaveStatus");
    fclose(sf);
  }
}

void OvmsVehicleSmartED::RestoreStatus() {
  if (MyPeripherals->m_sdcard->isavailable()) {
    FILE *sf = NULL;
    char k[40];
    char v[1024];
    sf = fopen("/sd/usr/SmartEDsatus.dat", "r");
    if (sf == NULL) {
      return;
    }
    while (fscanf(sf, "%s %s\n", k, v) != EOF) {
      MyMetrics.Set(k,v);
    }
    ESP_LOGI(TAG, "RestoreStatus");
    fclose(sf);
  }
	return;
}
