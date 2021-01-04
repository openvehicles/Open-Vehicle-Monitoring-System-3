

// -------- LIM (Charger port) --------------------------------------------------------------------------------------

// We skipped this because it "takes an argument"
// Inspecting a trace the request is the usual 3 byte extended request:
// ArgString: 'ARG;ZV_LADEKLAPPE'
// executeJob('lim_i1.prg'): 'STATUS_LESEN'
//  (Send): 83 14 F1 22 DE F1 79
// Send SF
// ELM send SPACE
// ELM data mode terminated
// ELM CAN send: '140322DEF1000000'
// ELM CAN rec: '614F10562DEF10001'
// Rec SF
// Received length: 5
//  (Resp): 85 F1 14 62 DE F1 00 01 BC

#define I3_PID_LIM_STATUS_LADEKLAPPE                                  0xDEF1
        // Status of the charging door

    #define I3_RES_LIM_STAT_LADEKLAPPE                                (RXBUF_UINT(0))
    #define I3_RES_LIM_STAT_LADEKLAPPE_UNIT                           ''
    #define I3_RES_LIM_STAT_LADEKLAPPE_TYPE                           unsigned short

// POLL line
  //{ I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_STATUS_LADEKLAPPE,                     {  0,  0,  0,  0 }, 0, ISOTP_EXTADR },   // 0xDEF1


// ArgString: 'ARG;ZV_LADESTECKER'
// executeJob('lim_i1.prg'): 'STATUS_LESEN'
//  (Send): 83 14 F1 22 DE F0 78
// Send SF
// ELM send SPACE
// ELM data mode terminated
// ELM CAN send: '140322DEF0000000'
// ELM CAN rec: '614F10562DEF00100'
// Rec SF
// Received length: 5
//  (Resp): 85 F1 14 62 DE F0 01 00 BB


#define I3_PID_LIM_STATUS_LADESTECKER                                 0xDEF0
        // "Status sensor AC connector lock front (0 = unlocked, 1 = locked)

    #define I3_RES_LIM_STAT_LADESTECKER_0XDEFO                       (RXBUF_UCHAR(0))
    #define I3_RES_LIM_STAT_LADESTECKER_0XDEFO_UNIT                  ''
    #define I3_RES_LIM_STAT_LADESTECKER_0XDEFO_TYPE                  unsigned char

// POLL line
  //{ I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_STATUS_LADESTECKER,                    {  0,  0,  0,  0 }, 0, ISOTP_EXTADR },   // 0xDEF0

// ArgString: 'ARG;LED_LADESTATUS'
// executeJob('lim_i1.prg'): 'STATUS_LESEN'
//  (Send): 83 14 F1 22 DE F3 7B
// Send SF
// ELM send SPACE
// ELM data mode terminated
// ELM CAN send: '140322DEF3000000'
// ELM CAN rec: '614F10462DEF300'
// Rec SF
// Received length: 4
//  (Resp): 84 F1 14 62 DE F3 00 BC

#define I3_PID_LIM_LED_LADESTATUS                                     0xDEF3
        // Status or control LED for charging status (RGB light ring)

    #define I3_RES_LIM_LED_LADESTATUS                                 (RXBUF_UCHAR(0))
    #define I3_RES_LIM_LED_LADESTATUS_UNIT                            ''
    #define I3_RES_LIM_LED_LADESTATUS_TYPE                            unsigned char

// POLL line
  //{ I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_LED_LADESTATUS,                        {  0,  0,  0,  0 }, 0, ISOTP_EXTADR },   // 0xDEF3

