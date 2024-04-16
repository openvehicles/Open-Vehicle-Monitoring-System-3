/*
;    Project:       Open Vehicle Monitor System
;    Date:          22th October 2017
;
;    Changes:
;    1.0  Initial stub
;
;    (C) 2018        Geir Øyvind Vælidalo <geir@validalo.net>
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

#ifndef __VEHICLE_KIANIROEV_H__
#define __VEHICLE_KIANIROEV_H__

#include "common.h"
#include "vehicle.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
// #include "ovms_webserver.h"
#endif

using namespace std;

class OvmsVehicleNetaAya : public NetaAya
  {
  public:
		OvmsVehicleNetaAya();
    ~OvmsVehicleNetaAya();

  public:
    bool configured;
    bool fully_configured;
    bool reset_by_config;

    void IncomingFrameCan1(CAN_frame_t *p_frame);
    void Ticker1(uint32_t ticker);
    void Ticker10(uint32_t ticker);
    void Ticker300(uint32_t ticker);
    void EventListener(std::string event, void* data);
    void SendTesterPresent(uint16_t id, uint8_t length);
    bool SetSessionMode(uint16_t id, uint8_t mode);
    void SendCanMessage(uint16_t id, uint8_t count,
						uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
						uint8_t b5, uint8_t b6);
    void SendCanMessageSecondary(uint16_t id, uint8_t count,
                        uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
                        uint8_t b5, uint8_t b6);
    void SendCanMessageTriple(uint16_t id, uint8_t count,
						uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
						uint8_t b5, uint8_t b6);
    bool SendCanMessage_sync(uint16_t id, uint8_t count,
    				uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
						uint8_t b5, uint8_t b6);
    bool SendCommandInSessionMode(uint16_t id, uint8_t count,
    				uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
						uint8_t b5, uint8_t b6, uint8_t mode);

    virtual OvmsVehicle::vehicle_command_t CommandLock(const char* pin);
    virtual OvmsVehicle::vehicle_command_t CommandUnlock(const char* pin);

    metric_unit_t GetConsoleUnits();

  protected:

    void VerifyConfigs(bool verify);
    bool ConfigChanged();
    void VerifySingleConfig(std::string param, std::string instance, std::string defValue, std::string value);
    void VerifySingleConfigInt(std::string param, std::string instance, int defValue, int value);
    void VerifySingleConfigBool(std::string param, std::string instance, bool defValue, bool value);


    void HandleCharging();
    void HandleChargeStop();
    void HandleCarOn();
    void HandleCarOff();

    bool SetDoorLock(bool open);
    void SetChargeMetrics();
    void SendTesterPresentMessages();

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    // --------------------------------------------------------------------------
    // Webserver subsystem
    //  - implementation: ks_web.(h,cpp)
    //
  public:
    virtual void
    WebInit();
    static void WebCfgFeatures(PageEntry_t &p, PageContext_t &c);
    static void WebCfgBattery(PageEntry_t &p, PageContext_t &c);
    static void WebBattMon(PageEntry_t &p, PageContext_t &c);

  public:
    void GetDashboardConfig(DashboardConfig &cfg);
#endif //CONFIG_OVMS_COMP_WEBSERVER
  };

#endif //#ifndef __VEHICLE_KIANIROEV_H__
