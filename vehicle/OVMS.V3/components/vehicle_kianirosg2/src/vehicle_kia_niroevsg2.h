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

#ifndef __VEHICLE_KIANIROEVSG2_H__
#define __VEHICLE_KIANIROEVSG2_H__

#include "common.h"
#include "vehicle.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
// #include "ovms_webserver.h"
#endif

using namespace std;

class OvmsVehicleKiaNiroEvSg2 : public KiaVehicleSg2
  {
  public:
		OvmsVehicleKiaNiroEvSg2();
    ~OvmsVehicleKiaNiroEvSg2();

  public:
    bool should_poll;
    int poll_counter;

    bool lock_command;
    bool unlock_command;
    int lock_counter;

    void IncomingFrameCan1(CAN_frame_t *p_frame) override;
    void IncomingFrameCan2(CAN_frame_t *p_frame) override;
    void Ticker1(uint32_t ticker) override;
    void Ticker10(uint32_t ticker) override;
    void Ticker300(uint32_t ticker) override;
    void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t *data, uint8_t length) override;
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

    vehicle_command_t CommandLock(const char *pin) override;
    vehicle_command_t CommandUnlock(const char *pin) override;

  protected:
    void HandleCharging();
    void HandleChargeStop();
    void HandleCarOn();
    void HandleCarOff();

    void IncomingVMCU(canbus *bus, uint16_t type, uint16_t pid, const uint8_t *data, uint8_t length, uint16_t mlframe, uint16_t mlremain);
    void IncomingBMC(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlframe, uint16_t mlremain);
    void IncomingBCM(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlframe, uint16_t mlremain);
    void IncomingIGMP(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlframe, uint16_t mlremain);
    void IncomingCM(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlframe, uint16_t mlremain);
    void IncomingSW(canbus* bus, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlframe, uint16_t mlremain);

    bool SetDoorLock(bool open);
    void VerifyDoorLock();
    void VerifyCanActivity();
    void SetChargeMetrics();
    void SendTesterPresentMessages();

    bool windows_open;

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

#endif //#ifndef __VEHICLE_KIANIROEVSG2_H__
