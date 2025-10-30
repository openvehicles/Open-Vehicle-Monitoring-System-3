#ifndef __VEHICLE_MAXUS_T90EV_H__
#define __VEHICLE_MAXUS_T90EV_H__

#include "vehicle_obdii.h"

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
};

#endif // __VEHICLE_MAXUS_T90EV_H__
