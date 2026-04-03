// mock_ovms.hpp — Stubs for OVMS framework types enabling native laptop builds.
// Included via -include flag so it loads before any vehicle module header.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) fprintf(stdout, "[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) do {} while(0)
#define ESP_LOGV(tag, fmt, ...) do {} while(0)

// ---------------------------------------------------------------------------
// FreeRTOS stubs (only the bits used in vehicle_vwegolf.cpp)
// ---------------------------------------------------------------------------
typedef uint32_t TickType_t;
inline void vTaskDelay(TickType_t) {}
inline TickType_t pdMS_TO_TICKS(uint32_t ms) { return ms; }

// ---------------------------------------------------------------------------
// CAN types
// ---------------------------------------------------------------------------
typedef enum { CAN_frame_std, CAN_frame_ext } CAN_frame_format_t;
typedef enum { CAN_MODE_LISTEN, CAN_MODE_ACTIVE, CAN_MODE_OFF } CAN_mode_t;
typedef enum { CAN_SPEED_100KBPS = 100, CAN_SPEED_500KBPS = 500 } CAN_speed_t;

typedef union {
    struct { CAN_frame_format_t FF; uint8_t DLC; } B;
} CAN_FIR_t;

struct canbus {
    uint8_t m_busnumber = 0;
    void WriteStandard(uint32_t /*id*/, uint8_t /*len*/, uint8_t* /*data*/) {}
    void WriteExtended(uint32_t /*id*/, uint8_t /*len*/, uint8_t* /*data*/) {}
};

// 'Minutes' is used as a unit tag in some metric SetValue calls
static constexpr int Minutes = 0;

typedef int esp_err_t;

struct CAN_frame_t {
    canbus*    origin   = nullptr;
    void*      callback = nullptr;
    CAN_FIR_t  FIR      = {};
    uint32_t   MsgID    = 0;
    union {
        uint8_t  u8[8];
        uint32_t u32[2];
        uint64_t u64;
    } data = {};
};

// ---------------------------------------------------------------------------
// Metrics — store values in a global map keyed by metric name.
// The real code accesses metrics via pointer (->), so each StandardMetrics
// member is a pointer to an OvmsMetric.
// ---------------------------------------------------------------------------
struct MetricStore {
    std::map<std::string, double>      numbers;
    std::map<std::string, std::string> strings;
    void reset() { numbers.clear(); strings.clear(); }
};
extern MetricStore g_metrics;

template<typename T>
struct OvmsMetric {
    std::string name;
    explicit OvmsMetric(const char* n) : name(n) {}
    void SetValue(T v)              { g_metrics.numbers[name] = static_cast<double>(v); }
    void SetValue(T v, int /*unit*/) { SetValue(v); }  // unit arg used by real metrics, ignored here
    void Clear()                    { g_metrics.numbers.erase(name); }
    T    AsValue() const  { return static_cast<T>(g_metrics.numbers[name]); }
    float AsFloat() const { return static_cast<float>(g_metrics.numbers[name]); }
};

template<>
struct OvmsMetric<std::string> {
    std::string name;
    explicit OvmsMetric(const char* n) : name(n) {}
    void        SetValue(const std::string& v) { g_metrics.strings[name] = v; }
    void        SetValue(const char* v)        { g_metrics.strings[name] = v ? v : ""; }
    std::string AsValue()  const {
        auto it = g_metrics.strings.find(name);
        return it != g_metrics.strings.end() ? it->second : "";
    }
    std::string AsString() const { return AsValue(); }
};

template<>
struct OvmsMetric<bool> {
    std::string name;
    explicit OvmsMetric(const char* n) : name(n) {}
    void SetValue(bool v) { g_metrics.numbers[name] = v ? 1.0 : 0.0; }
    bool AsValue() const  { return g_metrics.numbers[name] != 0.0; }
    bool AsBool()  const  { return AsValue(); }
};

using OvmsMetricFloat  = OvmsMetric<float>;
using OvmsMetricInt    = OvmsMetric<int>;
using OvmsMetricBool   = OvmsMetric<bool>;
using OvmsMetricString = OvmsMetric<std::string>;

// StandardMetrics — members are POINTERS to match how the real code accesses them.
struct StandardMetricsType {
    // Battery
    OvmsMetricFloat*  ms_v_bat_soc             = new OvmsMetricFloat("ms_v_bat_soc");
    OvmsMetricFloat*  ms_v_bat_voltage          = new OvmsMetricFloat("ms_v_bat_voltage");
    OvmsMetricFloat*  ms_v_bat_current          = new OvmsMetricFloat("ms_v_bat_current");
    OvmsMetricFloat*  ms_v_bat_power            = new OvmsMetricFloat("ms_v_bat_power");
    OvmsMetricFloat*  ms_v_bat_temp             = new OvmsMetricFloat("ms_v_bat_temp");
    OvmsMetricFloat*  ms_v_bat_capacity         = new OvmsMetricFloat("ms_v_bat_capacity");
    OvmsMetricFloat*  ms_v_bat_range_est        = new OvmsMetricFloat("ms_v_bat_range_est");
    OvmsMetricFloat*  ms_v_bat_range_ideal      = new OvmsMetricFloat("ms_v_bat_range_ideal");
    OvmsMetricFloat*  ms_v_bat_energy_used      = new OvmsMetricFloat("ms_v_bat_energy_used");
    OvmsMetricFloat*  ms_v_bat_energy_recd      = new OvmsMetricFloat("ms_v_bat_energy_recd");
    // Position / speed
    OvmsMetricFloat*  ms_v_pos_speed            = new OvmsMetricFloat("ms_v_pos_speed");
    OvmsMetricFloat*  ms_v_pos_odometer         = new OvmsMetricFloat("ms_v_pos_odometer");
    OvmsMetricFloat*  ms_v_pos_latitude         = new OvmsMetricFloat("ms_v_pos_latitude");
    OvmsMetricFloat*  ms_v_pos_longitude        = new OvmsMetricFloat("ms_v_pos_longitude");
    OvmsMetricBool*   ms_v_pos_gpslock          = new OvmsMetricBool("ms_v_pos_gpslock");
    // Environment / body
    OvmsMetricInt*    ms_v_env_gear             = new OvmsMetricInt("ms_v_env_gear");
    OvmsMetricInt*    ms_v_env_drivemode        = new OvmsMetricInt("ms_v_env_drivemode");
    OvmsMetricFloat*  ms_v_env_temp             = new OvmsMetricFloat("ms_v_env_temp");
    OvmsMetricFloat*  ms_v_env_cabintemp        = new OvmsMetricFloat("ms_v_env_cabintemp");
    OvmsMetricFloat*  ms_v_env_cabinsetpoint    = new OvmsMetricFloat("ms_v_env_cabinsetpoint");
    OvmsMetricBool*   ms_v_env_on               = new OvmsMetricBool("ms_v_env_on");
    OvmsMetricBool*   ms_v_env_locked           = new OvmsMetricBool("ms_v_env_locked");
    OvmsMetricBool*   ms_v_env_hvac             = new OvmsMetricBool("ms_v_env_hvac");
    OvmsMetricInt*    ms_v_env_parktime         = new OvmsMetricInt("ms_v_env_parktime");
    // Doors
    OvmsMetricBool*   ms_v_door_fl              = new OvmsMetricBool("ms_v_door_fl");
    OvmsMetricBool*   ms_v_door_fr              = new OvmsMetricBool("ms_v_door_fr");
    OvmsMetricBool*   ms_v_door_rl              = new OvmsMetricBool("ms_v_door_rl");
    OvmsMetricBool*   ms_v_door_rr              = new OvmsMetricBool("ms_v_door_rr");
    OvmsMetricBool*   ms_v_door_hood            = new OvmsMetricBool("ms_v_door_hood");
    OvmsMetricBool*   ms_v_door_trunk           = new OvmsMetricBool("ms_v_door_trunk");
    OvmsMetricBool*   ms_v_door_chargeport      = new OvmsMetricBool("ms_v_door_chargeport");
    // Charge
    OvmsMetricBool*   ms_v_charge_inprogress    = new OvmsMetricBool("ms_v_charge_inprogress");
    OvmsMetricString* ms_v_charge_state         = new OvmsMetricString("ms_v_charge_state");
    OvmsMetricString* ms_v_charge_substate      = new OvmsMetricString("ms_v_charge_substate");
    OvmsMetricString* ms_v_charge_type          = new OvmsMetricString("ms_v_charge_type");
    OvmsMetricBool*   ms_v_charge_timermode     = new OvmsMetricBool("ms_v_charge_timermode");
    OvmsMetricInt*    ms_v_charge_duration_full = new OvmsMetricInt("ms_v_charge_duration_full");
    // Identity
    OvmsMetricString* ms_v_vin                  = new OvmsMetricString("ms_v_vin");
    // Monotonic time
    OvmsMetricInt*    ms_m_monotonic            = new OvmsMetricInt("ms_m_monotonic");
};
extern StandardMetricsType StandardMetrics;
// The source uses both 'StandardMetrics' and 'StdMetrics' — make them the same object.
#define StdMetrics StandardMetrics

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
struct OvmsConfig {
    void RegisterParam(const char*, const char*, bool=true, bool=true) {}
    std::string GetParamValue(const char*, const char*, const char* def="") const { return def; }
    int  GetParamValueInt(const char*, const char*, int def=0) const { return def; }
    bool GetParamValueBool(const char*, const char*, bool def=false) const { return def; }
    void SetParamValue(const char*, const char*, const char*) {}
};
extern OvmsConfig MyConfig;

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------
struct OvmsWriter {};
struct OvmsCommand {
    OvmsCommand* RegisterCommand(const char*, const char*, ...) { return this; }
};
struct OvmsCommandApp {
    OvmsCommand root;
    OvmsCommand* RegisterCommand(const char*, const char*, ...) { return &root; }
    void UnregisterCommand(const char*) {}
};
extern OvmsCommandApp MyCommandApp;

// ---------------------------------------------------------------------------
// Vehicle factory
// ---------------------------------------------------------------------------
struct OvmsVehicleFactory {
    template<typename T>
    void RegisterVehicle(const char*, const char*) {}
};
extern OvmsVehicleFactory MyVehicleFactory;

// ---------------------------------------------------------------------------
// OvmsVehicle base class
// ---------------------------------------------------------------------------
enum vehicle_command_t { Success, Fail, NotImplemented };

struct OvmsVehicle {
    // Expose the enum in the OvmsVehicle scope to match OvmsVehicle::vehicle_command_t
    using vehicle_command_t = ::vehicle_command_t;

    canbus* m_can1 = nullptr;
    canbus* m_can2 = nullptr;
    canbus* m_can3 = nullptr;

    void RegisterCanBus(int /*bus*/, CAN_mode_t, CAN_speed_t) {}

    virtual void IncomingFrameCan1(CAN_frame_t*) {}
    virtual void IncomingFrameCan2(CAN_frame_t*) {}
    virtual void IncomingFrameCan3(CAN_frame_t*) {}
    virtual void Ticker1(uint32_t) {}
    virtual void Ticker10(uint32_t) {}
    bool PinCheck(const char* /*pin*/) { return true; }  // always pass in tests

    virtual vehicle_command_t CommandLock(const char*)   { return NotImplemented; }
    virtual vehicle_command_t CommandUnlock(const char*) { return NotImplemented; }
    virtual vehicle_command_t CommandWakeup()                 { return NotImplemented; }
    virtual vehicle_command_t CommandClimateControl(bool)     { return NotImplemented; }
    virtual ~OvmsVehicle() = default;
};
