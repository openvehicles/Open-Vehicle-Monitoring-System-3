/*
;    Project:       Open Vehicle Monitor System
;    Module:        CAN dump CRTD format
;    Date:          18th January 2018
;
;    (C) 2018       Mark Webb-Johnson
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

#ifndef __CANFORMAT_GVRET_H__
#define __CANFORMAT_GVRET_H__

#include "canformat.h"

#define CANFORMAT_GVRET_MAXLEN 48

#define GVRET_SET_BINARY 0xe7
#define GVRET_START_BYTE 0xf1
#define GVRET_NOTDEAD_1  0xde
#define GVRET_NOTDEAD_2  0xed

typedef enum
  {
  BUILD_CAN_FRAME = 0,
  TIME_SYNC,
  GET_DIG_INPUTS,
  GET_ANALOG_INPUTS,
  SET_DIG_OUTPUTS,
  SETUP_CANBUS,
  GET_CANBUS_PARAMS,
  GET_DEVICE_INFO,
  SET_SINGLEWIRE_MODE,
  KEEP_ALIVE,
  SET_SYSTEM_TYPE,
  ECHO_CAN_FRAME,
  GET_NUM_BUSES,
  GET_EXT_BUSES
  } gvret_cmd_t;

typedef struct __attribute__ ((__packed__))
  {
  uint8_t startbyte;
  uint8_t command;
  union
    {
    struct // 0 - Send a frame out one of the buses
      {
      uint32_t id;           // Frame ID LSB to MSB
      uint8_t bus;           // which bus (0 = CAN0, 1 = CAN1, 2 = SWCAN, 3 = LIN1, 4 = LIN2)
      uint8_t length;        // Frame length
      uint8_t data[8];       // Data bytes
      } build_can_frame;
    struct // 4 - Set state of digital outputs
      {
      uint8_t outputvals;    // Bitfield - Bit 0 = DIgital output 1, etc. 0 = Low, 1 = High
      } set_dig_outputs;
    struct // 5 - Set CAN bus configuration
      {
      uint32_t can1;         // CAN0 Speed. If Bit 31 set then Bit 30 = Enable Bit 29 = Listen Only
      uint32_t can2;         // CAN1 Speed. Same deal, if bit 31 set then use 30 and 29 for config options
      } setup_canbus;
    struct // 8 - Set singlewire mode
      {
      uint8_t mode;          // if 0x10 then single wire mode, otherwise no. Not very applicable to M2
      } set_singlewire_mode;
    struct // 10 - Set system type
      {
      uint8_t type;          // System type (right now only 0 for M2 project but GVRET takes 0-2)
      } set_system_type;
    struct // 11 - Echo CAN frame
      {
      uint32_t id;           // Frame ID LSB to MSB
      uint8_t bus;           // which bus (0 = CAN0, 1 = CAN1, 2 = SWCAN, 3 = LIN1, 4 = LIN2)
      uint8_t length;        // Frame length
      uint8_t data[8];       // Byte 6-? - Data bytes
      } echo_can_frame;
    } body;
  } gvret_commandmsg_t;

typedef struct __attribute__ ((__packed__))
  {
  uint8_t startbyte;
  uint8_t command;
  union
    {
    struct // 1 - Time Sync
      {
      uint32_t microseconds; // Microseconds since start up LSB to MSB
      } time_sync;
    struct // 2 - Set state of digital inputs
      {
      uint8_t inputvals;     // Bitfield of inputs (Bit 0 = Input 0, etc) 0 = Low, 1 = High
      uint8_t checksum;      // Checksum byte
      } get_dig_inputs;
    struct // 3 - Get state of analog input pins
      {
      uint16_t inputvals[4]; // Analog input LSB then MSB
      uint8_t checksum;      // Checksum byte
      } get_analog_inputs;
    struct // 6 - Get CAN bus config
      {
      uint8_t can1_mode;     // Bit 0 - Enable  Bit 4 - Listen only
      uint32_t can1_speed;   // CAN0 Speed LSB to MSB
      uint8_t can2_mode;     // Bit 0 - Enable  Bit 4 - ListenOnly
      uint32_t can2_speed;   // CAN1 Speed LSB to MSB
      } get_canbus_params;
    struct // 7 - Get device info
      {
      uint16_t build;        // Build number LSB to MSB
      uint8_t eeprom;        // EEPROM version
      uint8_t filetype;      // File output type
      uint8_t autolog;       // Auto start logging
      uint8_t singlewire;    // Single wire mode
      } get_device_info;
    struct // 9 - Keep alive
      {
      uint8_t notdead1;      // 0xDE
      uint8_t notdead2;      // 0xAD - So, if it isn't dead it responds with DEAD.
      } keep_alive;
    struct // 12 - Get number of buses
      {
      uint8_t buses;         // Number of buses
      } get_num_buses;
    struct // 13 - Get extended buses
      {
      uint8_t swcan_mode;    // Bit 0 - Enable  Bit 4 - Listen only
      uint32_t swcan_speed;  // CAN0 Speed LSB to MSB
      uint8_t lin1_mode;     // Bit 0 - Enable  Bit 4 - ListenOnly
      uint32_t lin1_speed;   // CAN1 Speed LSB to MSB
      uint8_t lin2_mode;     // Bit 0 - Enable  Bit 4 - ListenOnly
      uint32_t lin2_speed;   // CAN1 Speed LSB to MSB
      } get_ext_buses;
    } body;
  } gvret_replymsg_t;

typedef struct __attribute__ ((__packed__))
  {
  uint8_t startbyte;         // 0xF1
  uint8_t command;           // 0x00
  uint32_t microseconds;     // Microseconds since start up LSB to MSB
  uint32_t id;               // Frame ID, Bit 31 - Extended Frame
  uint8_t lenbus;            // Frame length in bottom 4 bits, Bus received on in upper 4 bits
  uint8_t data[9];           // Data bytes (plus zero termination)
  } gvret_binary_frame_t;

class canformat_gvret : public canformat
  {
  public:
    canformat_gvret(const char* type);
    virtual ~canformat_gvret();

  public:
    virtual std::string get(CAN_log_message_t* message);
    virtual std::string getheader(struct timeval *time);
    virtual size_t put(CAN_log_message_t* message, uint8_t *buffer, size_t len, void* userdata=NULL);
  };

class canformat_gvret_ascii : public canformat_gvret
  {
  public:
    canformat_gvret_ascii(const char* type);
    virtual std::string get(CAN_log_message_t* message);
    virtual size_t put(CAN_log_message_t* message, uint8_t *buffer, size_t len, void* userdata=NULL);
  };

class canformat_gvret_binary : public canformat_gvret
  {
  public:
    canformat_gvret_binary(const char* type);
    virtual std::string get(CAN_log_message_t* message);
    virtual size_t put(CAN_log_message_t* message, uint8_t *buffer, size_t len, void* userdata=NULL);
  };

#endif // __CANFORMAT_GVRET_H__
