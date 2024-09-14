#ifndef VEHICLE_ENERGICA_PIDS_H
#define VEHICLE_ENERGICA_PIDS_H

#include <stdint.h>
#include <arpa/inet.h>
// #include <endian.h> // FIXME: doesn't work, but ESP32 is little-endian so we can keep words as-is
#include <ctime>

#pragma pack(push,1)
class pid_20 { // little-endian

	// IGBT in 0.1° C
	int16_t igbt_min;
	int16_t igbt;
	int16_t igbt_max;

	int16_t gate_temp; // 0.1° C

public:
	float inverter_temp_C() const { return igbt * 0.1f; }
	float gate_temp_C    () const { return gate_temp * 0.1f; }
};
#pragma pack(pop)
static_assert(sizeof(pid_20) == 8, "Energica: sizeof pid_20");

#pragma pack(push,1)
class pid_22 { // little-endian
	uint16_t _unk0;
	uint16_t _unk1;
	int16_t motor_temp; // 0.1° C
	uint16_t _unk3;

public:
	float motor_temp_C() const { return motor_temp * 0.1f; }
};
#pragma pack(pop)
static_assert(sizeof(pid_22) == 8, "Energica: sizeof pid_22");

enum button_state { released, pushed, left, right };
enum beam_state   { off, low, high, both };

#pragma pack(push,1)
class pid_102 { // little-endian

	// Byte 0 - 0:7
	bool _mode_left   : 1;
	bool _mode_right  : 1;
	bool _mode_push   : 1;
	bool _blink_right : 1;
	bool _blink_left  : 1;
	bool _blink_push  : 1;
	bool _high_beam   : 1;
	bool _low_beam    : 1;

	// Byte 1 - 8:15
	bool _unk0         : 1;
	bool _energized    : 1;
	bool _go_req       : 1;
	bool _go	       : 1;
	bool _key_in       : 1;
	bool _stand_up     : 1; // 0 when stand is down
	bool _ignition_btn : 1;
	bool _throttle_on  : 1; // 1 when throttle is not idle (>0%), 0 when 0% throttle

	// Byte 2 - 16:23
	bool _charging    : 1;
	bool _charge_port_unlocked : 1;
	bool _unk3        : 1;
	bool _unk4        : 1;
	bool _unk5        : 1;
	bool _front_brake : 1;
	bool _rear_brake  : 1;
	bool _moving      : 1;

	// Byte 3 - 24:31
	uint8_t _unk7;

	int16_t lateral_acceleration; // g
	int16_t frontal_acceleration; // g

protected:
	static button_state _to_state(bool push, bool left, bool right)
	{
		if (push)  return button_state::pushed;
		if (left)  return button_state::left;
		if (right) return button_state::right;
		return button_state::released;
	}

public:
	button_state blinker_state() const { return _to_state(_blink_push, _blink_left, _blink_right); }
	button_state mode_state   () const { return _to_state(_mode_push, _mode_left, _mode_right); }
	beam_state   light_state  () const
	{
		if (_high_beam && _low_beam) return beam_state::both;
		if (_high_beam)              return beam_state::high;
		if (_low_beam)               return beam_state::low;
		return beam_state::off;
	}

	inline bool front_brake() const { return _front_brake; }
	inline bool rear_brake () const { return _rear_brake; }

	inline bool go    () const { return _go; }
	inline bool moving() const { return _moving; }

	inline bool stand_up  () const	{ return  _stand_up; }
	inline bool stand_down() const	{ return !_stand_up; }

	inline bool ignition_button() const { return _ignition_btn; }
	inline bool throttle_on    () const { return _throttle_on; }
	inline bool key_in         () const { return _key_in; }

	// TODO: double-check
	inline bool energized  () const { return _energized; }
	inline bool charge_lock() const { return !_charge_port_unlocked; }
	inline bool charging   () const { return charge_lock() && stand_down(); }
};
#pragma pack(pop)
static_assert(sizeof(pid_102) == 8, "Energica: sizeof pid_102");

#pragma pack(push,1)
class pid_104 { // little-endian

public:
	uint32_t odometer : 32; // 0.1 km
	uint16_t speed    : 13; // 0.1 km/h
	uint16_t rpm      : 15;
	uint8_t  _unk0    : 3;
	bool     reverse  : 1;

	float odometer_km() const { return odometer * 0.1f; }
	float speed_kmh  () const { return speed * 0.1f; }
};
#pragma pack(pop)
static_assert(sizeof(pid_104) == 8, "Energica: sizeof pid_104");

#pragma pack(push,1)
class pid_109 { // little-endian
	uint16_t _throttle; // 0.1%
	uint16_t _current_max_out; // 0.1A
	uint16_t _current_max_regen; // 0.1A
	uint16_t _unk0; // Some other current

public:
	float throttle_pc()       const { return _throttle * 0.1f; }
	float current_max_out()   const { return _current_max_out * 0.1f; }
	float current_max_regen() const { return _current_max_regen * 0.1f; }
};
#pragma pack(pop)
static_assert(sizeof(pid_109) == 8, "Energica: sizeof pid_109");

#pragma pack(push,1)
class pid_200 { // big-endian
	uint8_t _temp_cell_min;
	uint8_t _soc;
	uint8_t _soh;
	uint8_t _temp_cell_max;
	uint16_t _pack_voltage;
	int16_t _pack_current;

public: // Conversion from big-endian
	uint8_t temp_cell_min() const { return _temp_cell_min; }
	uint8_t soc()           const { return _soc; }
	uint8_t soh()           const { return _soh; }
	uint8_t temp_cell_max() const { return _temp_cell_max; }
	float   pack_voltage()  const { return ::ntohs(_pack_voltage) * 0.1f; }
	float   pack_current()  const { return -0.1f * ((int16_t)::ntohs(_pack_current)); } // Current is battery-centric (<0 when going out of the battery)
	float   power_W()       const { return pack_voltage() * pack_current(); }
};
#pragma pack(pop)
static_assert(sizeof(pid_200) == 8, "Energica: sizeof pid_200");

#pragma pack(push,1)
class pid_203 { // big-endian
	uint16_t _avg_cell_voltage; // mV
	uint8_t  _min_cell_num;
	uint8_t  _max_cell_num;
	uint16_t _min_cell_voltage; // mV
	uint16_t _max_cell_voltage; // mV

public: // Conversion from big-endian
	uint16_t avg_cell_voltage() const { return ::ntohs(_avg_cell_voltage); }
	uint8_t  min_cell_num    () const { return _min_cell_num; }
	uint8_t  max_cell_num    () const { return _max_cell_num; }
	uint16_t min_cell_voltage() const { return ::ntohs(_min_cell_voltage); }
	uint16_t max_cell_voltage() const { return ::ntohs(_max_cell_voltage); }
	uint16_t cell_balance    () const { return max_cell_voltage() - min_cell_voltage(); }
};
#pragma pack(pop)
static_assert(sizeof(pid_203) == 8, "Energica: sizeof pid_203");


constexpr uint16_t msg_gps_latspeed  = 0x001a;
constexpr uint16_t msg_gps_longalt   = 0x011a;
constexpr uint16_t msg_gps_datetime  = 0xfe1a;

/**
 * All 0x410 PIDs start with a 2-bytes marker identifying the rest of the data (message type).
 * Classes describing those data inherit the `_pid_410` class that include this maker, then
 * define their memory mapping.
 */

#pragma pack(push,1)
class _pid_410 {
	uint16_t _msg : 16;

public:
	uint16_t msg() const { return _msg; }
};
#pragma pack(pop)
static_assert(sizeof(_pid_410) == 2, "Energica: sizeof _pid_410");

#pragma pack(push,1)
class pid_410_gps_latspdcourse : _pid_410 {
	uint16_t _course : 9; // ° mag
	uint16_t _speed  : 9; // km/h

	uint16_t _lat_min_10000 : 14;
	uint8_t _lat_min        : 6;
	uint8_t _lat_degree     : 8;
	bool    _lat_neg        : 1;

	bool _unk1 : 1;

public:
	float course() const { return _course; }
	float speed()  const { return _speed; }
	float latitude() const {
		float lat = (_lat_min_10000 /  600000.0f) + (_lat_min / 60.0f) + _lat_degree;
		return (_lat_neg ? -lat : lat);
	}
};
#pragma pack(pop)
static_assert(sizeof(pid_410_gps_latspdcourse) == 8, "Energica: sizeof pid_410 (latitude)");

// See https://support.dewesoft.com/en/support/solutions/articles/14000108531-explanation-of-gps-fix-quality-values
enum gps_fix : uint8_t {
	not_fixed = 0,
	standalone, // Only from GNSS satellites
	dgps,       // Differential GPS -> beside GNSS satellites an additional correction from base station is used. Sometimes this status is shown if the received uses additional SBAS signal for the position
	gps_pps,
	// Those values are beyond the 2-bits representation but given for educational purposes
	// rtk_fixed,
	// rtk_float,
};

#pragma pack(push,1)
class pid_410_gps_longalt : _pid_410 {
	uint8_t  _fix  : 2; // Real type enum gps_fix

	uint16_t _altitude : 15;
	bool     _alt_neg  : 1;

	uint16_t _long_min_10000 : 14;
	uint8_t _long_min        : 6;
	uint8_t _long_degree     : 8;
	bool    _long_neg        : 1;

	bool _unk1 : 1;

public:
	gps_fix fix      () const { return static_cast<gps_fix>(_fix); }
	float   altitude () const { return (_alt_neg ? -_altitude : _altitude); }
	float   longitude() const {
		float lon = (_long_min_10000 /  600000.0f) + (_long_min / 60.0f) + _long_degree;
		return (_long_neg ? -lon : lon);
	}
};
#pragma pack(pop)
static_assert(sizeof(pid_410_gps_longalt) == 8, "Energica: sizeof pid_410 (longitude)");

#pragma pack(push,1)
class pid_410_gps_date : _pid_410 {
	uint16_t _ms    : 10;
	uint8_t  _sec   : 6;
	uint8_t  _min   : 6;
	uint8_t  _hours : 5;

	uint8_t  _day   : 5;
	uint8_t  _month : 4;
	uint8_t  _year  : 7;

	uint8_t  _nsat	: 5;

public:
	float   seconds() const { return _sec + _ms * 0.001f; }
	uint8_t	minutes() const { return _min; }
	uint8_t	hours  () const { return _hours; }
	uint8_t day    () const { return _day; }
	uint8_t month  () const { return _month; }
	int     year   () const { return _year + 2000; }

	uint8_t n_satellites() const { return _nsat; }
	int64_t time_utc() const {
		std::tm tm{};
		tm.tm_sec  = _sec;
		tm.tm_min  = _min;
		tm.tm_hour = _hours;
		tm.tm_mday = _day;
		tm.tm_mon  = _month;
		tm.tm_year = _year + 100;
		return static_cast<int64_t>(std::mktime(&tm)); // GPS time is already in UTC // TODO: is it since epoch?
	}
};
#pragma pack(pop)
static_assert(sizeof(pid_410_gps_date) == 8, "Energica: sizeof pid_410 (date)");

#endif // VEHICLE_ENERGICA_PIDS_H
