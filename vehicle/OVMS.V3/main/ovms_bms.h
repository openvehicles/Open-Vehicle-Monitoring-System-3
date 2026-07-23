/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/
#ifndef __OVMS_BMS_H__
#define __OVMS_BMS_H__

#include "ovms_config.h"
#include "ovms_events.h"
#include "ovms_metrics.h"
#include "ovms_command.h"
#include "dbc_number.h"
#include "metrics_standard.h"

enum class OvmsBmsEventType : short {
  Unknown,
  Reset,
  ResetWithValue,
  Value,
  SetNumber,
  SetModuleSize
};

enum class OvmsBmsStatus : short {
  OK = 0,
  Warn = 1,
  Alert = 2
};
inline bool operator<(OvmsBmsStatus lhs, OvmsBmsStatus rhs) {
  return static_cast<short>(lhs) < static_cast<short>(rhs);
}
const float BMS_DEFTHR_VMAXGRAD   = 0.010;  // [V]
const float BMS_DEFTHR_VMAXSDDEV  = 0.010;  // [V]
const float BMS_DEFTHR_VWARN      = 0.020;  // [V]
const float BMS_DEFTHR_VALERT     = 0.030;  // [V]
const float BMS_DEFTHR_TWARN      = 2.00;   // [°C]
const float BMS_DEFTHR_TALERT     = 3.00;   // [°C]
class OvmsBmsMonitor : public InternalRamAllocated
  {
  public:
    OvmsBmsMonitor();
    ~OvmsBmsMonitor();
  public:

  protected:
    float* m_bms_voltages;                    // BMS voltages (current value)
    float* m_bms_vmins;                       // BMS minimum voltages seen (since reset)
    float* m_bms_vmaxs;                       // BMS maximum voltages seen (since reset)
    float* m_bms_vdevmaxs;                    // BMS maximum voltage deviations seen (since reset)
    OvmsBmsStatus* m_bms_valerts;                // BMS voltage deviation alerts (since reset)
    int m_bms_valerts_new;                    // BMS new voltage alerts since last notification
    int m_bms_vstddev_cnt;                    // BMS internal stddev counter
    float m_bms_vstddev_avg;                  // BMS internal stddev average
    bool m_bms_has_voltages;                  // True if BMS has a complete set of voltage values
    float* m_bms_temperatures;                // BMS temperatures (celcius current value)
    float* m_bms_tmins;                       // BMS minimum temperatures seen (since reset)
    float* m_bms_tmaxs;                       // BMS maximum temperatures seen (since reset)
    float* m_bms_tdevmaxs;                    // BMS maximum temperature deviations seen (since reset)
    OvmsBmsStatus* m_bms_talerts;                // BMS temperature deviation alerts (since reset)
    int m_bms_talerts_new;                    // BMS new temperature alerts since last notification
    bool m_bms_has_temperatures;              // True if BMS has a complete set of temperature values
    std::vector<bool> m_bms_bitset_v;         // BMS tracking: true if corresponding voltage set
    std::vector<bool> m_bms_bitset_t;         // BMS tracking: true if corresponding temperature set
    int m_bms_bitset_cv;                      // BMS tracking: count of unique voltage values set
    int m_bms_bitset_ct;                      // BMS tracking: count of unique temperature values set
    int m_bms_readings_v;                     // Number of BMS voltage readings expected
    int m_bms_readingspermodule_v;            // Number of BMS voltage readings per module
    int m_bms_readings_t;                     // Number of BMS temperature readings expected
    int m_bms_readingspermodule_t;            // Number of BMS temperature readings per module
    float m_bms_limit_tmin;                   // Minimum temperature limit (for sanity checking)
    float m_bms_limit_tmax;                   // Maximum temperature limit (for sanity checking)
    float m_bms_limit_vmin;                   // Minimum voltage limit (for sanity checking)
    float m_bms_limit_vmax;                   // Maximum voltage limit (for sanity checking)
    float m_bms_defthr_vmaxgrad;              // Default voltage deviation max valid gradient [V]
    float m_bms_defthr_vmaxsddev;             // Default voltage deviation max valid stddev deviation [V]
    float m_bms_defthr_vwarn;                 // Default voltage deviation warn threshold [V]
    float m_bms_defthr_valert;                // Default voltage deviation alert threshold [V]
    float m_bms_defthr_twarn;                 // Default temperature deviation warn threshold [°C]
    float m_bms_defthr_talert;                // Default temperature deviation alert threshold [°C]
    uint32_t m_bms_vlog_last;                 // Last log time for voltages
    uint32_t m_bms_tlog_last;                 // Last log time for temperatures
    bool m_autonotifications;
    bool m_webmodule_enabled;

    void BmsTicker(std::string event, void* data);

    void NotifyBmsAlerts();
    void ClearVoltages();
    void ClearTemperatures();
  public:
    void SetCellArrangementVoltage(int readings, int readingspermodule);
    void SetCellArrangementTemperature(int readings, int readingspermodule);
    void SetCellDefaultThresholdsVoltage(float warn, float alert, float maxgrad=-1, float maxsddev=-1);
    void SetCellDefaultThresholdsTemperature(float warn, float alert);
    void SetCellLimitsVoltage(float min, float max);
    void SetCellLimitsTemperature(float min, float max);
    void SetCellVoltage(int index, float value);
    void ResetCellVoltages(bool full = false);
    void SetCellTemperature(int index, float value);
    void ResetCellTemperatures(bool full = false);
    void RestartCellVoltages();
    void RestartCellTemperatures();
    bool HasVoltageValues() { return m_bms_bitset_cv > 0;}
    bool HasTemperatureValues() { return m_bms_bitset_ct > 0;}

    int ReadingsPerModuleVoltages() { return m_bms_readingspermodule_v; }
    int ReadingsPerModuleTemperatures() { return m_bms_readingspermodule_t; }
    float ReadingForModuleVoltages(int n) { return m_bms_voltages[n]; }
    float ReadingForModuleTemeratures(int n) { return m_bms_temperatures[n]; }

    void SetAutoNotifications(bool newval) { m_autonotifications = newval; }
    void Clear()
      {
      ClearVoltages();
      ClearTemperatures();
      SetWebmoduleEnabled(false);
      }
    void SetWebmoduleEnabled(bool enabled);
    void RegisterPage()
      {
      // Force it at the current position
      SetWebmoduleEnabled(false);
      SetWebmoduleEnabled(true);
      }
    void DeregisterPage()
      {
      SetWebmoduleEnabled(false);
      }
  public:
    enum class bms_status_t
      {
      Both,
      Voltage,
      Temperature
      };
    int GetCellArangementVoltage(int* readings=NULL, int* readingspermodule=NULL);
    int GetCellArangementTemperature(int* readings=NULL, int* readingspermodule=NULL);
    void GetCellDefaultThresholdsVoltage(float* warn, float* alert, float* maxgrad=NULL, float* maxsddev=NULL);
    void GetCellDefaultThresholdsTemperature(float* warn, float* alert);
    void ResetCellStats();
    void Status(int verbosity, OvmsWriter* writer, bms_status_t statusmode );
    bool FormatBmsAlerts(int verbosity, OvmsWriter* writer, bool show_warnings);
    bool CheckChangeCellArrangementVoltage(int readings, int readingspermodule = 0);
    bool CheckChangeCellArrangementTemperature(int readings, int readingspermodule = 0);

    /// Return true if there are section enabled.
    bool IsEnabled(bms_status_t statusmode);

  protected:
    static void bms_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void bms_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void bms_alerts(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    // BMS
    static duk_ret_t DukOvmsBmsVoltageSetCellArrangement(duk_context *ctx);
    static duk_ret_t DukOvmsBmsVoltageSetCellDefaultThresholds(duk_context *ctx);
    static duk_ret_t DukOvmsBmsVoltageSetCellLimits(duk_context *ctx);
    static duk_ret_t DukOvmsBmsVoltageSetCell(duk_context *ctx);
    static duk_ret_t DukOvmsBmsVoltageResetCells(duk_context *ctx);
    static duk_ret_t DukOvmsBmsVoltagesRestartCells(duk_context *ctx);

    static duk_ret_t DukOvmsBmsTemperatureSetCellArrangement(duk_context *ctx);
    static duk_ret_t DukOvmsBmsTemperatureSetCellDefaultThresholds(duk_context *ctx);
    static duk_ret_t DukOvmsBmsTemperatureSetCellLimits(duk_context *ctx);
    static duk_ret_t DukOvmsBmsTemperatureSetCell(duk_context *ctx);
    static duk_ret_t DukOvmsBmsTemperatureResetCells(duk_context *ctx);
    static duk_ret_t DukOvmsBmsTemperatureRestartCells(duk_context *ctx);
  public:
    void RegisterBmsDuktape();

#endif // CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

  };

extern OvmsBmsMonitor MyBmsMonitor;

#endif //#ifndef __OVMS_BMS_H__
