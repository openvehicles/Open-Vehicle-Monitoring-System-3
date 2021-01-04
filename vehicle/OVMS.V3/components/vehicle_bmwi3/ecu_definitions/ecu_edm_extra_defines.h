

// -------- IBS (12V Battery monitor) --------------------------------------------------------------------------------------

// executeJob('edmei1.prg'): 'STATUS_MESSWERTE_IBS'
//  (Send): 83 12 F1 22 40 2B 13
// Send SF
// ELM send SPACE
// ELM data mode terminated
// ELM CAN send: '120322402B000000'
// ELM CAN rec: '612F1101F62402B00D2'
// Rec FF
// ELM send SPACE
// ELM data mode terminated
// ELM CAN send: '1230080000000000'
// ELM CAN rec: '612F12182D809AE0000'
// Rec CF
// ELM CAN rec: '612F1220400000E0002'
// Rec CF
// ELM CAN rec: '612F123000000000000'
// Rec CF
// ELM CAN rec: '612F124000000000000'
// Rec CF
// ELM CAN rec: '612F1250000FFFFFFFF'
// Rec CF
// Received length: 31
//  (Resp): 9F F1 12 62 40 2B 00 D2 82 D8 09 AE 00 00 04 00 00 0E 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 66


// ;    JOBNAME:STATUS_MESSWERTE_IBS
// ;    JOBCOMMENT:0x22402B STATUS_MESSWERTE_IBS
// ;    JOBCOMMENT:Messwerte IBS auslesen

// ;    RESULT:STAT_T_BATT_WERT
// ;    RESULTTYPE:real
// ;    RESULTCOMMENT:Batterietemperatur
// ;    RESULTCOMMENT:T_BATT   Einheit: C   Min: -3276.8 Max: 3276.7

// ;    RESULT:STAT_T_BATT_EINH
// ;    RESULTTYPE:string
// ;    RESULTCOMMENT:degreeC

// ;    RESULT:STAT_U_BATT_WERT
// ;    RESULTTYPE:real
// ;    RESULTCOMMENT:Batteriespannung von IBS gemessen
// ;    RESULTCOMMENT:U_BATT   Einheit: V   Min: 6 Max: 22.3837

// ;    RESULT:STAT_U_BATT_EINH
// ;    RESULTTYPE:string
// ;    RESULTCOMMENT:V

// ;    RESULT:STAT_I_BATT_WERT
// ;    RESULTTYPE:real
// ;    RESULTCOMMENT:Batteriestrom von IBS gemessen
// ;    RESULTCOMMENT:I_BATT   Einheit: A   Min: -200 Max: 5042.8 - so 0 represents -200.    5242.8

// ;    RESULT:STAT_I_BATT_EINH
// ;    RESULTTYPE:string
// ;    RESULTCOMMENT:A

// 31 - 3 (the 62 40 2B) = 28
// here they are: 00 D2 82 D8 09 AE 00 00 04 00 00 0E 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00
// this contains a temp, volts and amps
//  00D2 - 210 - 21.0C - that's easy
//  82D8 - should be 14.something - readings from 6 to 22.3837.
//      22.3837 - 6 = 16.3837  more or less 16384 (float vagaries)
//      if we use full range assune it goes to 65535
//      so for 22.384 volts we have 65535 (#ffff)
//      so for 14.37 volts we are looking for 14.37-6=8.37=33480 or 0x82c8
//      SO: formula is (value / 4000 + 6) eg 0x82d8 or 33496/4000+6=14.374V  <-----
//  09Ae - So this must be the amps.
//      Range is -200 Max: 5042.8 -> total 5242.8.
//      Assuming that 5242.8 is represented by 65536, formula is value/12.5 - 200 eg 0x09ae=2478/12.5-200 = -1.76A

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
    #define I3_RES_EDM_STAT_I_BATT_WERT_UNIT                          'V'
    #define I3_RES_EDM_STAT_I_BATT_WERT_TYPE                          float
        // Battery current

// POLL line
  //{ I3_ECU_EDM_TX, I3_ECU_EDM_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_EDM_STATUS_MESSWERTE_IBS,                  {  0,  0,  0,  0 }, 0, ISOTP_EXTADR },   // 0x402B
