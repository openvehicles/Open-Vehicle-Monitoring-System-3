

// -------- IBS (12V Battery monitor) --------------------------------------------------------------------------------------

#define I3_PID_EDM_STATUS_MESSWERTE_IBS                               0x402B
        // Values from the IBS - the battery monitor

    #define I3_RES_EDM_STAT_T_BATT_WERT                               (RXBUF_SINT(0)/10)
    #define I3_RES_EDM_STAT_T_BATT_WERT_UNIT                          '°C'
    #define I3_RES_EDM_STAT_T_BATT_WERT_TYPE                          float
        // Temp

    #define I3_RES_EDM_STAT_U_BATT_WERT                               (RXBUF_UINT(2)/4000.0f+6.0f)
    #define I3_RES_EDM_STAT_U_BATT_WERT_UNIT                          'V'
    #define I3_RES_EDM_STAT_U_BATT_WERT_TYPE                          float
        // Battery voltage

    #define I3_RES_EDM_STAT_I_BATT_WERT                               (RXBUF_UINT(4)/12.5f-200.0f)
    #define I3_RES_EDM_STAT_I_BATT_WERT_UNIT                          'A'
    #define I3_RES_EDM_STAT_I_BATT_WERT_TYPE                          float
        // Battery current

// POLL line
  //{ I3_ECU_EDM_TX, I3_ECU_EDM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_EDM_STATUS_MESSWERTE_IBS,                  {  0,  0,  0,  0 }, 0, ISOTP_EXTADR },   // 0x402B
