

// -------- LIM (Charger port) --------------------------------------------------------------------------------------


#define I3_PID_LIM_STATUS_LADEKLAPPE                                  0xDEF1
        // Status of the charging door

    #define I3_RES_LIM_STAT_LADEKLAPPE                                (RXBUF_UINT(0))
    #define I3_RES_LIM_STAT_LADEKLAPPE_UNIT                           ''
    #define I3_RES_LIM_STAT_LADEKLAPPE_TYPE                           unsigned short

// POLL line
  //{ I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_STATUS_LADEKLAPPE,                     {  0,  0,  0,  0 }, 0, ISOTP_EXTADR },   // 0xDEF1



#define I3_PID_LIM_STATUS_LADESTECKER                                 0xDEF0
        // "Status sensor AC connector lock front (0 = unlocked, 1 = locked)

    #define I3_RES_LIM_STAT_LADESTECKER_0XDEFO                       (RXBUF_UCHAR(0))
    #define I3_RES_LIM_STAT_LADESTECKER_0XDEFO_UNIT                  ''
    #define I3_RES_LIM_STAT_LADESTECKER_0XDEFO_TYPE                  unsigned char

// POLL line
  //{ I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_STATUS_LADESTECKER,                    {  0,  0,  0,  0 }, 0, ISOTP_EXTADR },   // 0xDEF0


#define I3_PID_LIM_LED_LADESTATUS                                     0xDEF3
        // Status or control LED for charging status (RGB light ring)

    #define I3_RES_LIM_LED_LADESTATUS                                 (RXBUF_UCHAR(0))
    #define I3_RES_LIM_LED_LADESTATUS_UNIT                            ''
    #define I3_RES_LIM_LED_LADESTATUS_TYPE                            unsigned char

// POLL line
  //{ I3_ECU_LIM_TX, I3_ECU_LIM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_LIM_LED_LADESTATUS,                        {  0,  0,  0,  0 }, 0, ISOTP_EXTADR },   // 0xDEF3

