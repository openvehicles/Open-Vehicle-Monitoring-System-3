// mock_ovms.hpp — Stubs for OVMS framework types enabling native laptop builds.
// Included via -include flag so it loads before any vehicle module header.

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

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
extern uint32_t g_tick_count;
inline TickType_t xTaskGetTickCount() { return g_tick_count; }
static constexpr uint32_t portTICK_PERIOD_MS = 1;

// ---------------------------------------------------------------------------
// CAN types
// ---------------------------------------------------------------------------
typedef enum { CAN_frame_std, CAN_frame_ext } CAN_frame_format_t;
typedef enum { CAN_MODE_LISTEN, CAN_MODE_ACTIVE, CAN_MODE_OFF } CAN_mode_t;
typedef enum { CAN_SPEED_100KBPS = 100, CAN_SPEED_500KBPS = 500 } CAN_speed_t;
typedef enum {
    CAN_errorstate_none = 0,
    CAN_errorstate_active,
    CAN_errorstate_warning,
    CAN_errorstate_passive,
    CAN_errorstate_busoff,
} CAN_errorstate_t;

typedef union {
    struct { CAN_frame_format_t FF; uint8_t DLC; } B;
} CAN_FIR_t;

typedef int esp_err_t;
static constexpr esp_err_t ESP_OK   =  0;
static constexpr esp_err_t ESP_FAIL = -1;

// One transmitted frame, recorded so the clima tests can assert exactly what the
// module put on the bus. The real canbus discards frames after TX; the mock keeps
// an ordered log (std vs ext, ID, DLC, payload).
struct TxRecord {
    bool     extended = false;
    uint32_t id       = 0;
    uint8_t  len      = 0;
    uint8_t  data[8]  = {};
};

struct canbus {
    uint8_t               m_busnumber = 0;
    std::vector<TxRecord> tx_log;
    CAN_errorstate_t      error_state = CAN_errorstate_none;

    esp_err_t WriteStandard(uint32_t id, uint8_t len, uint8_t* data, int /*wait*/ = 0) {
        return LogTx(false, id, len, data);
    }
    esp_err_t WriteExtended(uint32_t id, uint8_t len, uint8_t* data, int /*wait*/ = 0) {
        return LogTx(true, id, len, data);
    }
    CAN_errorstate_t GetErrorState() { return error_state; }
    esp_err_t Reset() { return ESP_OK; }

 private:
    esp_err_t LogTx(bool ext, uint32_t id, uint8_t len, uint8_t* data) {
        TxRecord r;
        r.extended = ext;
        r.id       = id;
        r.len      = len;
        for (uint8_t i = 0; i < len && i < 8; i++) r.data[i] = data[i];
        tx_log.push_back(r);
        return ESP_OK;
    }
};

// 'Minutes' is used as a unit tag in some metric SetValue calls
static constexpr int Minutes = 0;

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
    // Per-metric SetValue call count. Used by the replay test to catch
    // "set once at boot, never updated again" regressions like the stuck
    // range_est bug — a static-value check passes but the metric isn't live.
    std::map<std::string, int>         writes;
    // Explicit staleness override per metric. Tests set this true to simulate a pinned /
    // stale sensor (a value is present but no longer fresh). A fresh SetValue clears it.
    // A metric never written is treated as stale (matches real OvmsMetric::IsStale at boot).
    std::map<std::string, bool>        stale;
    void reset() { numbers.clear(); strings.clear(); writes.clear(); stale.clear(); }
    int  write_count(const std::string& n) const {
        auto it = writes.find(n);
        return it == writes.end() ? 0 : it->second;
    }
    bool is_stale(const std::string& n) const {
        auto it = stale.find(n);
        if (it != stale.end()) return it->second;
        return write_count(n) == 0;
    }
};
extern MetricStore g_metrics;

template<typename T>
struct OvmsMetric {
    std::string name;
    explicit OvmsMetric(const char* n) : name(n) {}
    void SetValue(T v)              {
        g_metrics.numbers[name] = static_cast<double>(v);
        g_metrics.writes[name]++;
        g_metrics.stale[name] = false;  // a fresh write is not stale
    }
    void SetValue(T v, int /*unit*/) { SetValue(v); }  // unit arg used by real metrics, ignored here
    void Clear()                    { g_metrics.numbers.erase(name); }
    T    AsValue() const  { return static_cast<T>(g_metrics.numbers[name]); }
    float AsFloat() const { return static_cast<float>(g_metrics.numbers[name]); }
    bool IsStale() const  { return g_metrics.is_stale(name); }
};

template<>
struct OvmsMetric<std::string> {
    std::string name;
    explicit OvmsMetric(const char* n) : name(n) {}
    void        SetValue(const std::string& v) {
        g_metrics.strings[name] = v;
        g_metrics.writes[name]++;
    }
    void        SetValue(const char* v)        {
        g_metrics.strings[name] = v ? v : "";
        g_metrics.writes[name]++;
    }
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
    void SetValue(bool v) {
        g_metrics.numbers[name] = v ? 1.0 : 0.0;
        g_metrics.writes[name]++;
    }
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
    OvmsMetricFloat*  ms_v_bat_soh              = new OvmsMetricFloat("ms_v_bat_soh");
    OvmsMetricFloat*  ms_v_bat_cac              = new OvmsMetricFloat("ms_v_bat_cac");
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
    OvmsMetricBool*   ms_v_env_awake            = new OvmsMetricBool("ms_v_env_awake");
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
    OvmsMetricFloat*  ms_v_charge_voltage       = new OvmsMetricFloat("ms_v_charge_voltage");
    OvmsMetricFloat*  ms_v_charge_current       = new OvmsMetricFloat("ms_v_charge_current");
    OvmsMetricFloat*  ms_v_charge_power         = new OvmsMetricFloat("ms_v_charge_power");
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
    // Backing store so tests can exercise config-driven paths (e.g. clima cc-temp).
    // Real OvmsConfig persists per param+instance; here a flat "param/instance" map.
    std::map<std::string, std::string> store;

    void RegisterParam(const char*, const char*, bool=true, bool=true) {}
    std::string GetParamValue(const char* param, const char* instance,
                              const char* def="") const {
        auto it = store.find(std::string(param) + "/" + instance);
        return it != store.end() ? it->second : def;
    }
    int GetParamValueInt(const char* param, const char* instance, int def=0) const {
        auto it = store.find(std::string(param) + "/" + instance);
        return it != store.end() ? atoi(it->second.c_str()) : def;
    }
    bool GetParamValueBool(const char* param, const char* instance, bool def=false) const {
        auto it = store.find(std::string(param) + "/" + instance);
        if (it == store.end()) return def;
        return it->second == "yes" || it->second == "true" || it->second == "1";
    }
    void SetParamValue(const char* param, const char* instance, const char* value) {
        store[std::string(param) + "/" + instance] = value;
    }
    void SetParamValueBool(const char* param, const char* instance, bool value) {
        store[std::string(param) + "/" + instance] = value ? "yes" : "no";
    }
};
extern OvmsConfig MyConfig;

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------
struct OvmsWriter {
    int puts(const char*) { return 0; }
    int printf(const char*, ...) { return 0; }
};
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
// Poller (subset of components/poller/src/vehicle_poller.h used by vwegolf)
// ---------------------------------------------------------------------------
#define VEHICLE_POLL_NSTATES 4
#define VEHICLE_POLL_TYPE_READDATA 0x22
#define ISOTP_STD 0

class OvmsPoller {
 public:
    typedef struct {
        uint32_t txmoduleid;
        uint32_t rxmoduleid;
        uint16_t type;
        uint16_t pid;  // real header wraps this in a union with args/xargs — plain pid
                       // keeps the same aggregate-init syntax for simple tables
        uint16_t polltime[VEHICLE_POLL_NSTATES];
        uint8_t pollbus;
        uint8_t protocol;
    } poll_pid_t;

    typedef struct {
        canbus* bus;
        uint8_t bus_no;
        uint32_t protocol;
        uint16_t type;
        uint16_t pid;
        uint32_t moduleid_sent;
        uint32_t moduleid_low;
        uint32_t moduleid_high;
        uint32_t moduleid_rec;
        uint16_t mlframe;
        uint16_t mloffset;
        uint16_t mlremain;
        poll_pid_t entry;
        uint32_t ticker;
    } poll_job_t;
};

#define POLL_LIST_END {0, 0, 0x00, 0x00, {0, 0, 0}, 0, 0}

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

    // Allocate a real canbus per registered bus so the command/wake/heartbeat paths
    // (which call m_canN->WriteStandard/WriteExtended/GetErrorState) have a live object
    // and the clima tests can read its tx_log. Decode-only tests never touch the bus, but
    // a null m_canN segfaults the moment any TX is attempted. (The ctor's
    // static_cast<mcp2515*>(m_can2)->SetAcceptanceFilter is safe on a plain canbus —
    // mcp2515 adds no members and the mock method ignores `this`.)
    void RegisterCanBus(int bus, CAN_mode_t, CAN_speed_t) {
        canbus** slot = (bus == 1) ? &m_can1 : (bus == 2) ? &m_can2 : &m_can3;
        if (!*slot) {
            *slot = new canbus();
            (*slot)->m_busnumber = static_cast<uint8_t>(bus);
        }
    }

    virtual void IncomingFrameCan1(CAN_frame_t*) {}
    virtual void IncomingFrameCan2(CAN_frame_t*) {}
    virtual void IncomingFrameCan3(CAN_frame_t*) {}
    virtual void Ticker1(uint32_t) {}
    virtual void Ticker10(uint32_t) {}

    // Poller (real base manages poll scheduling; mock just records state so tests
    // can assert transitions and call IncomingPollReply directly)
    uint8_t m_poll_state = 0;
    canbus* m_poll_bus = nullptr;
    const OvmsPoller::poll_pid_t* m_poll_plist = nullptr;
    void PollSetPidList(canbus* bus, const OvmsPoller::poll_pid_t* plist) {
        m_poll_bus = bus;
        m_poll_plist = plist;
    }
    void PollSetState(uint8_t state, canbus* = nullptr) { m_poll_state = state; }
    virtual void IncomingPollReply(const OvmsPoller::poll_job_t&, uint8_t*, uint8_t) {}
    bool PinCheck(const char* /*pin*/) { return true; }  // always pass in tests
    void NotifyChargeStart() {}
    void NotifyChargeStopped() {}

    virtual vehicle_command_t CommandLock(const char*)   { return NotImplemented; }
    virtual vehicle_command_t CommandUnlock(const char*) { return NotImplemented; }
    virtual vehicle_command_t CommandWakeup()                 { return NotImplemented; }
    virtual vehicle_command_t CommandClimateControl(bool)     { return NotImplemented; }
    virtual ~OvmsVehicle() { delete m_can1; delete m_can2; delete m_can3; }
};
