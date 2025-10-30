/*
;    Project:       Open Vehicle Monitor System
;    Date:          2024
;
;    (C) 2024       [Your Name or Handle Here]
;    (C) 2024       OVMS Community
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

#ifndef __VEHICLE_MAXUS_T90EV_H__
#define __VEHICLE_MAXUS_T90EV_H__

#include "vehicle_obdii.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

using namespace std;

class OvmsVehicleMaxusT90EV : public OvmsVehicleObd2
{
public:
    OvmsVehicleMaxusT90EV();
    ~OvmsVehicleMaxusT90EV();

protected:
    // --- OBDII communication settings ---
    #define T90EV_STD_TX 0x7e3
    #define T90EV_STD_RX 0x7eb
    
    // --- Metric Getters (for public access to standard metrics) ---
    float GetSoc();
    float GetSoh();

    // --- Main Poll function & PID configuration ---
    void ConfigMaxusT90EVPIDs(vehicle_pid_t *pid_list, size_t &pid_list_len);
    void MaxusT90EVPoll(uint16_t ticker);
    void MaxusT90EVInitPids();

    // --- Standard OVMS Overrides ---
    virtual void IncomingFrameCan1(CAN_FRAME *frame);
    virtual void IncomingFrameCan2(CAN_FRAME *frame);
    virtual void IncomingFrameCan3(CAN_FRAME *frame);
    virtual vehicle_command_t CommandHandler(int verb, const char* noun, const char* reqs, bool *supported);
    virtual const char* Get(const char* param);
    virtual void Set(const char* param, const char* val);
    virtual void ConfigChanged(OvmsConfigParam* param);

    static OvmsVehicleMaxusT90EV *GetInstance(st_vehicle_protocol_t protocol);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    // --------------------------------------------------------------------------
    // Webserver subsystem
    //
public:
    void WebInit();
    void WebDeInit();
    void GetDashboardConfig(DashboardConfig& cfg);
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
#endif //CONFIG_OVMS_COMP_WEBSERVER
};

#endif // __VEHICLE_MAXUS_T90EV_H__
