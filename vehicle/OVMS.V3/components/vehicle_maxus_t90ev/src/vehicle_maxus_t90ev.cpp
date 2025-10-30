#include "ovms_log.h"
static const char *TAG = "v-maxus-t90ev";

#include "vehicle_maxus_t90ev.h"

// Define the vehicle name and model code for the module manager
#define T90EV_MODEL_CODE "T90EV"
#define T90EV_VEHICLE_NAME "Maxus T90 EV"

// --- Public vehicle properties accessors ---

float OvmsVehicleMaxusT90EV::GetSoc()
{
    return StdMetrics.ms_v_env_soc->AsFloat();
}

float OvmsVehicleMaxusT90EV::GetSoh()
{
    return StdMetrics.ms_v_bat_soh->AsFloat();
}

// --- PID Configuration ---

void OvmsVehicleMaxusT90EV::ConfigMaxusT90EVPIDs(vehicle_pid_t *pid_list, size_t &pid_list_len)
{
    // The T90 EV PIDs are defined here.
    // Format: { PID, Tx/Rx, Type, Polltime(s), Framework, Conversion/Scaling, Min, Max, Metric, Label }

    // ====================================================================
    // 1. CONFIRMED WORKING PIDs
    // ====================================================================
    
    // 1. VIN (Vehicle Identification Number)
    pid_list[pid_list_len++] = {
        0xF190,                   // PID: 22F190
        T90EV_STD_TX | T90EV_STD_RX, // Target ECU (7E3 -> 7EB)
        VEHICLE_PID_TYPE_STRING,
        0,                        // Poll Time: On first connect
        0,
        "0,A",                    // Conversion: ASCII string
        0, 0,
        MS_V_VIN,
        "VIN"
    };

    // 2. SOC (State of Charge)
    pid_list[pid_list_len++] = {
        0xE002,                   // PID: 22E002
        T90EV_STD_TX | T90EV_STD_RX,
        VEHICLE_PID_TYPE_CUSTOM,
        10,                       // Poll Time: 10 seconds
        0,
        "x",                      // Conversion: dat[0] directly is the percentage
        0, 0,
        MS_V_BAT_SOC,
        "SOC"
    };

    // 3. SOH (State of Health)
    pid_list[pid_list_len++] = {
        0xE003,                   // PID: 22E003
        T90EV_STD_TX | T90EV_STD_RX,
        VEHICLE_PID_TYPE_CUSTOM,
        60,                       // Poll Time: 60 seconds
        0,
        "(x*256+y)/100",          // Conversion: ((dat[0]*256 + dat[1])/100)
        0, 0,
        MS_V_BAT_SOH,
        "SOH"
    };

    // ====================================================================
    // 2. EXPERIMENTAL PIDs
    // ====================================================================
    
    // 4. Pack Voltage (?) - 2 bytes
    pid_list[pid_list_len++] = {
        0xE004,                   // PID: 22E004
        T90EV_STD_TX | T90EV_STD_RX,
        VEHICLE_PID_TYPE_CUSTOM,
        10,                       // Poll Time: 10 seconds
        0,
        "x*256+y",                // Conversion: Raw 2-byte value
        0, 0,
        "v.m.maxus.raw_pack_voltage", // Custom Metric Name
        "Raw Pack Voltage"
    };

    // 5. Pack Current (?) - 1 byte
    pid_list[pid_list_len++] = {
        0xE005,                   // PID: 22E005
        T90EV_STD_TX | T90EV_STD_RX,
        VEHICLE_PID_TYPE_CUSTOM,
        10,                       // Poll Time: 10 seconds
        0,
        "x",                      // Conversion: Raw 1-byte value
        0, 0,
        "v.m.maxus.raw_pack_current", // Custom Metric Name
        "Raw Pack Current"
    };

    // 6. Estimated Range (?) - 1 byte
    pid_list[pid_list_len++] = {
        0xE006,                   // PID: 22E006
        T90EV_STD_TX | T90EV_STD_RX,
        VEHICLE_PID_TYPE_CUSTOM,
        10,                       // Poll Time: 10 seconds
        0,
        "x",                      // Conversion: Raw 1-byte value
        0, 0,
        MS_V_BAT_RANGE_EST,       // Standard Metric for Range (you can refine the scaling later)
        "Range Est (Raw)"
    };

    // 7. Charging / Plug State (?) - 2 bytes
    pid_list[pid_list_len++] = {
        0xE007,                   // PID: 22E007
        T90EV_STD_TX | T90EV_STD_RX,
        VEHICLE_PID_TYPE_CUSTOM,
        10,                       // Poll Time: 10 seconds
        0,
        "x*256+y",                // Conversion: Raw 2-byte value
        0, 0,
        "v.m.maxus.raw_plug_state", // Custom Metric Name
        "Raw Plug State"
    };

    // 8. Charging Power (?) - 2 bytes
    pid_list[pid_list_len++] = {
        0xE008,                   // PID: 22E008
        T90EV_STD_TX | T90EV_STD_RX,
        VEHICLE_PID_TYPE_CUSTOM,
        10,                       // Poll Time: 10 seconds
        0,
        "x*256+y",                // Conversion: Raw 2-byte value
        0, 0,
        "v.m.maxus.raw_charge_power", // Custom Metric Name
        "Raw Charge Power"
    };
}

// --- Standard OVMS Boilerplate ---

OvmsVehicleMaxusT90EV::OvmsVehicleMaxusT90EV()
{
    ESP_LOGI(TAG, "Maxus T90 EV vehicle module initialising...");

    // 1. Register the module
    Register(T90EV_MODEL_CODE, T90EV_VEHICLE_NAME, 15); 

    // 2. Configure the CAN bus (CAN 1 at 250kbps)
    MyConfig.can.push_back({1, CAN_MODE_250KBPS, (can_speed_t)0, CAN_CS_ACTIVE});
    PollSetBus(1); 

    // 3. Set up the PID list and start polling
    ConfigMaxusT90EVPIDs(MyPids, MyPids.size());

    // 4. Initialise the base class metrics
    BmsSetDefault();
}

OvmsVehicleMaxusT90EV::~OvmsVehicleMaxusT90EV()
{
    ESP_LOGI(TAG, "Maxus T90 EV vehicle module stopping...");
}

// Overload of the base class PID polling loop (needed to start the loop)
void OvmsVehicleMaxusT90EV::MaxusT90EVPoll(uint16_t ticker)
{
    PollSetState(VCS_READY);
    PollSetThrottling(60); 

    // Call the base class polling method
    OvmsVehicleObd2::Poll(ticker);
}

// Simple handler that just calls the main poll function
void OvmsVehicleMaxusT90EV::MaxusT90EVInitPids()
{
    MaxusT90EVPoll(0); 
}

// Default base class overrides (unchanged)
void OvmsVehicleMaxusT90EV::IncomingFrameCan1(CAN_FRAME *frame) {}
void OvmsVehicleMaxusT90EV::IncomingFrameCan2(CAN_FRAME *frame) {}
void OvmsVehicleMaxusT90EV::IncomingFrameCan3(CAN_FRAME *frame) {}
vehicle_command_t OvmsVehicleMaxusT90EV::CommandHandler(int verb, const char* noun, const char* reqs, bool *supported) { return COMMAND_OK; }
OvmsVehicleMaxusT90EV *OvmsVehicleMaxusT90EV::GetInstance(st_vehicle_protocol_t protocol) { return nullptr; }
const char* OvmsVehicleMaxusT90EV::Get(const char* param) { return OvmsVehicleObd2::Get(param); }
void OvmsVehicleMaxusT90EV::Set(const char* param, const char* val) { OvmsVehicleObd2::Set(param, val); }
void OvmsVehicleMaxusT90EV::ConfigChanged(OvmsConfigParam* param) { OvmsVehicleObd2::ConfigChanged(param); }
