#include "ovms_log.h"
#include "vehicle_vweup_all.h"

class OvmsVehicleVWeUpAllInit
{
public:
    OvmsVehicleVWeUpAllInit()
    {
        ESP_LOGI("vwup", "Registering Vehicle: VW e-Up (9000)");
        // Preparing for different VWUP classes.
        // Example:
        //
        // MyVehicleFactory.RegisterVehicle<VWeUpObd>("VWUP.OBD", "VW e-Up (OBD2)");
        MyVehicleFactory.RegisterVehicle<OvmsVehicleVWeUpAll>("VWUP.ALL", "VW e-Up (KCAN / OBD)");
    }
}
 
OvmsVehicleVWeUpAllInit __attribute__((init_priority(9000)));
