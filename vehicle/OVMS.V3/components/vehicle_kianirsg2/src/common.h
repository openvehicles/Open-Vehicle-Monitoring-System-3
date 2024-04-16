#ifndef __VEHICLE_KIA_COMMON_H__
#define __VEHICLE_KIA_COMMON_H__

#include "vehicle.h"
#include "ovms_webserver.h"

using namespace std;

typedef struct{
       int fromPercent;
       int toPercent;
       float maxChargeSpeed;
}charging_profile;


class KiaVehicleSg2 : public OvmsVehicle
{
public:
  OvmsMetricBool*  m_v_door_lock_fl;
  OvmsMetricBool*  m_v_door_lock_fr;
  OvmsMetricBool*  m_v_door_lock_rl;
  OvmsMetricBool*  m_v_door_lock_rr;
  OvmsMetricBool*  m_v_polling;

  // #ifdef CONFIG_OVMS_COMP_WEBSERVER
  //     public:
  //       static void WebAuxBattery(PageEntry_t& p, PageContext_t& c);
  // #endif //CONFIG_OVMS_COMP_WEBSERVER

protected:
  struct {
    uint8_t byte[8];
    uint8_t status;
    uint16_t id;
  } message_send_can;

  bool can_2_sending;

  const TickType_t xDelay = 50 / portTICK_PERIOD_MS;
};

// CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
#define CAN_BYTE(b)     data[b]
#define CAN_INT(b)      ((int16_t)CAN_UINT(b))
#define CAN_UINT(b)     (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b+1))
#define CAN_UINT24(b)   (((uint32_t)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b+1) << 8) | CAN_BYTE(b+2))
#define CAN_UINT32(b)   (((uint32_t)CAN_BYTE(b) << 24) | ((uint32_t)CAN_BYTE(b+1) << 16)  | ((UINT)CAN_BYTE(b+2) << 8) | CAN_BYTE(b+3))
#define CAN_NIBL(b)     (can_databuffer[b] & 0x0f)
#define CAN_NIBH(b)     (can_databuffer[b] >> 4)
#define CAN_NIB(n)      (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))
#define CAN_BIT(b,pos) !!(data[b] & (1<<(pos)))

#define TO_CELCIUS(n)	((float)n-40)
#define TO_PSI(n)		((float)n/4.0)

// #define BAT_SOC			StdMetrics.ms_v_bat_soc->AsFloat(100)
// #define BAT_SOH			StdMetrics.ms_v_bat_soh->AsFloat(100)
// #define LIMIT_SOC		StdMetrics.ms_v_charge_limit_soc->AsFloat(0)
// #define LIMIT_RANGE		StdMetrics.ms_v_charge_limit_range->AsFloat(0, Kilometers)
// #define EST_RANGE		StdMetrics.ms_v_bat_range_est->AsFloat(100, Kilometers)
// #define IDEAL_RANGE		StdMetrics.ms_v_bat_range_ideal->AsFloat(100, Kilometers)
// #define FULL_RANGE		StdMetrics.ms_v_bat_range_full->AsFloat(160, Kilometers)
// #define POS_ODO			StdMetrics.ms_v_pos_odometer->AsFloat(0, Kilometers)
// #define CHARGE_CURRENT	StdMetrics.ms_v_charge_current->AsFloat(0, Amps)
// #define CHARGE_VOLTAGE	StdMetrics.ms_v_charge_voltage->AsFloat(0, Volts)

#define UDS_SID_IOCTRL_BY_ID 			0x2F		// InputOutputControlByCommonID
#define UDS_SID_IOCTRL_BY_LOC_ID	0x30 	// InputOutputControlByLocalID
#define UDS_SID_TESTER_PRESENT		0x3E 	// TesterPresent

#define UDS_DEFAULT_SESSION 									0x01
#define UDS_PROGRAMMING_SESSION 							0x02
#define UDS_EXTENDED_DIAGNOSTIC_SESSION 				0x03
#define UDS_SAFETY_SYSTEM_DIAGNOSTIC_SESSION 	0x04
#define KIA_90_DIAGNOSTIC_SESSION 				0x90

// ECUs
#define SMART_JUNCTION_BOX 		0x771
#define INTEGRATED_GATEWAY 		0x770
#define BODY_CONTROL_MODULE  	0x7A0
#define SMART_KEY_UNIT 				0x7A5
#define ABS_EBP_UNIT 					0x7A5
#define ON_BOARD_CHARGER_UNIT 0x794
    
#define POLLSTATE_OFF                  PollSetState(0);
#define POLLSTATE_RUNNING           PollSetState(1);
#define POLLSTATE_CHARGING      PollSetState(2);
 
 #endif //#ifndef __VEHICLE_KIA_COMMON_H__
