#ifndef __VEHICLE_KIA_COMMON_H__
#define __VEHICLE_KIA_COMMON_H__

#include "vehicle.h"
#include "ovms_webserver.h"

using namespace std;
class NetaAya : public OvmsVehicle
{
public:
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

#define UDS_SID_IOCTRL_BY_ID 			0x2F		// InputOutputControlByCommonID
#define UDS_SID_IOCTRL_BY_LOC_ID	0x30 	// InputOutputControlByLocalID
#define UDS_SID_TESTER_PRESENT		0x3E 	// TesterPresent

#define UDS_DEFAULT_SESSION 									0x01
#define UDS_PROGRAMMING_SESSION 							0x02
#define UDS_EXTENDED_DIAGNOSTIC_SESSION 				0x03
#define UDS_SAFETY_SYSTEM_DIAGNOSTIC_SESSION 	0x04
    
#define POLLSTATE_OFF                  PollSetState(0);
#define POLLSTATE_RUNNING           PollSetState(1);
#define POLLSTATE_CHARGING      PollSetState(2);
 
#endif
