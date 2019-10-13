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

class Kia_Trip_Counter
    {
    private:
      float odo_start;
      float cdc_start;                  // Used to calculate trip power use (Cumulated discharge)
      float cc_start;                   // Used to calculate trip recuperation (Cumulated charge)
      float odo;
      float cdc;
      float cc;

    public:
      Kia_Trip_Counter();
      ~Kia_Trip_Counter();
      void Reset(float odo, float cdc, float cc);
      void Update(float odo, float cdc, float cc);
      float GetDistance();
      float GetEnergyUsed();
      float GetEnergyRecuperated();
      bool Started();
      bool HasEnergyData();
    };

class KiaVehicle : public OvmsVehicle
	{
#define     SAVE_STATUS_DATA_PATH "/sd/SaveStatus.dat"
#define     AUX_VOLTAGE_HISTORY_DATA_PATH "/sd/12VHistory.dat"
public:
  char m_vin[18];

  uint32_t kia_tpms_id[4];

  OvmsMetricInt* 		m_b_cell_volt_max_no;			// Max cell voltage no           02 21 01 -> 23 7
  OvmsMetricInt* 		m_b_cell_volt_min_no; 		// Min cell voltage no           02 21 01 -> 24 2
  OvmsMetricFloat*		m_b_cell_volt_max;     		// Battery cell maximum voltage
  OvmsMetricFloat*		m_b_cell_volt_min;     		// Battery cell minimum voltage
  OvmsMetricInt* 		m_b_cell_det_max_no; 			// 02 21 05 -> 24 3
  OvmsMetricInt*			m_b_cell_det_min_no; 			// 02 21 05 -> 24 6
  OvmsMetricFloat*		m_b_cell_det_max;      		// Battery cell maximum detoriation
  OvmsMetricFloat*		m_b_cell_det_min;      		// Battery cell minimum detoriation
  OvmsMetricInt* 		m_b_min_temperature; 			// 02 21 05 -> 21 7
  OvmsMetricInt*			m_b_inlet_temperature; 		// 02 21 05 -> 21 6
  OvmsMetricInt*			m_b_max_temperature; 			// 02 21 05 -> 22 1
  OvmsMetricInt*			m_b_heat_1_temperature; 	// 02 21 05 -> 23 6
  OvmsMetricInt*			m_b_heat_2_temperature; 	// 02 21 05 -> 23 7
  OvmsMetricFloat*		m_b_bms_soc; 						// The bms soc, which differs from displayed soc.
  OvmsMetricInt*			m_b_aux_soc; 						// The soc for aux battery.

  OvmsMetricBool*		m_v_env_lowbeam;
  OvmsMetricBool*		m_v_env_highbeam;

  OvmsMetricFloat* ms_v_pos_trip;
  OvmsMetricFloat* ms_v_trip_energy_used;
  OvmsMetricFloat* ms_v_trip_energy_recd;

  OvmsMetricFloat* m_obc_pilot_duty;
  OvmsMetricBool*  m_obc_timer_enabled;

  OvmsMetricFloat* m_ldc_out_voltage;
  OvmsMetricFloat* m_ldc_out_current;
  OvmsMetricFloat* m_ldc_in_voltage;
  OvmsMetricFloat* m_ldc_temperature;

  OvmsMetricBool*  m_v_seat_belt_driver;
  OvmsMetricBool*  m_v_seat_belt_passenger;
  OvmsMetricBool*  m_v_seat_belt_back_right;
  OvmsMetricBool*  m_v_seat_belt_back_middle;
  OvmsMetricBool*  m_v_seat_belt_back_left;

  OvmsMetricBool*  m_v_traction_control;
  OvmsMetricBool*  m_v_cruise_control;

  OvmsMetricString*	  m_v_steering_mode;

  OvmsMetricBool*  m_v_door_lock_fl;
  OvmsMetricBool*  m_v_door_lock_fr;
  OvmsMetricBool*  m_v_door_lock_rl;
  OvmsMetricBool*  m_v_door_lock_rr;

  OvmsMetricBool*  m_v_preheat_timer1_enabled;
  OvmsMetricBool*  m_v_preheat_timer2_enabled;
  OvmsMetricBool*  m_v_preheating;

  OvmsMetricBool*  m_v_heated_handle;
  OvmsMetricBool*  m_v_rear_defogger;

  OvmsMetricFloat* m_v_power_usage;

  OvmsMetricFloat* m_v_trip_consumption1;
  OvmsMetricFloat* m_v_trip_consumption2;

  OvmsMetricBool*  m_v_emergency_lights;

#ifdef CONFIG_OVMS_COMP_WEBSERVER
    public:
      static void WebAuxBattery(PageEntry_t& p, PageContext_t& c);
#endif //CONFIG_OVMS_COMP_WEBSERVER


protected:
  struct {
    uint8_t byte[8];
    uint8_t status;
    uint16_t id;
  } kia_send_can;

  typedef struct {
  		float soc;
  }Kia_Save_Status;


  int CalcRemainingChargeMinutes(float chargespeed, int fromSoc, int toSoc, int batterySize, charging_profile charge_steps[]);
  int CalcAUXSoc(float volt);
  void SaveStatus();
  void RestoreStatus();
  void Save12VHistory();

  Kia_Trip_Counter kia_park_trip_counter;
  Kia_Trip_Counter kia_charge_trip_counter;

  Kia_Save_Status kia_save_status;

  OvmsMetricString* m_version;
  OvmsMetricFloat*  m_c_power;            				// Available charge power
  OvmsMetricFloat*  m_c_speed;									// km/h

  uint32_t kia_battery_cum_charge_current; 		//Cumulated charge current
  uint32_t kia_battery_cum_discharge_current;	//Cumulated discharge current
  uint32_t kia_battery_cum_charge; 						//Cumulated charge power
  uint32_t kia_battery_cum_discharge; 					//Cumulated discharge power
  uint32_t kia_battery_cum_op_time; 						//Cumulated operating time
  uint32_t kia_last_battery_cum_charge;					

  float kia_obc_ac_voltage;
  float kia_obc_ac_current;

  float kia_last_soc;
  float kia_last_ideal_range;
  float kia_cum_charge_start; 		// Used to calculate charged power.
  

  bool kia_ready_for_chargepollstate;
  bool kia_check_door_lock;
  bool kia_lockDoors;
  bool kia_unlockDoors;

  bool kia_enable_write;

  uint8_t kia_secs_with_no_client;

  const TickType_t xDelay = 50 / portTICK_PERIOD_MS;
	};

class RangeCalculator
{
#define     RANGE_CALC_DATA_PATH "/sd/RangeCalc.dat"

private:
       typedef struct {
             float consumption;  //Total kWh
             float distance;            //km
       }TripConsumption;

       TripConsumption trips[20];
       int currentTripPointer = 0;
       float minimumTrip = 1;
       float weightOfCurrentTrip = 4;
       float batteryCapacity = 64;

       void storeTrips();
       void restoreTrips();

public:
       RangeCalculator(float minimumTrip, float weightOfCurrentTrip, float defaultRange, float batteryCapacity);
       ~RangeCalculator();
       void updateTrip(float distance, float consumption);
       void tripEnded(float distance, float consumption);
       float getRange();
};

#define SQR(n) ((n)*(n))
#define ABS(n) (((n) < 0) ? -(n) : (n))
#define LIMIT_MIN(n,lim) ((n) < (lim) ? (lim) : (n))
#define LIMIT_MAX(n,lim) ((n) > (lim) ? (lim) : (n))


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

#define BAT_SOC			StdMetrics.ms_v_bat_soc->AsFloat(100)
#define BAT_SOH			StdMetrics.ms_v_bat_soh->AsFloat(100)
#define LIMIT_SOC		StdMetrics.ms_v_charge_limit_soc->AsFloat(0)
#define LIMIT_RANGE		StdMetrics.ms_v_charge_limit_range->AsFloat(0, Kilometers)
#define EST_RANGE		StdMetrics.ms_v_bat_range_est->AsFloat(100, Kilometers)
#define IDEAL_RANGE		StdMetrics.ms_v_bat_range_ideal->AsFloat(100, Kilometers)
#define FULL_RANGE		StdMetrics.ms_v_bat_range_full->AsFloat(160, Kilometers)
#define POS_ODO			StdMetrics.ms_v_pos_odometer->AsFloat(0, Kilometers)
#define CHARGE_CURRENT	StdMetrics.ms_v_charge_current->AsFloat(0, Amps)
#define CHARGE_VOLTAGE	StdMetrics.ms_v_charge_voltage->AsFloat(0, Volts)
#define SET_CHARGE_STATE(n,m)		StdMetrics.ms_v_charge_state->SetValue(n); if(m!=NULL) StdMetrics.ms_v_charge_substate->SetValue(m)

#define CUM_CHARGE		((float)kia_battery_cum_charge/10.0)
#define CUM_DISCHARGE	((float)kia_battery_cum_discharge/10.0)
#define SET_TPMS_ID(n, v)	if (v > 0) kia_tpms_id[n] = v;

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
