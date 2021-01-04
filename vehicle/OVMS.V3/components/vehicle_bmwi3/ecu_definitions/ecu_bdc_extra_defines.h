

// VIN:

// 2020-11-15 10:34:29.188 --> 22␣F1␣90⏎
// 2020-11-15 10:34:29.276 <-- 640␣F1␣10␣14␣62␣F1␣90␣57␣42␣⏎640␣F1␣21␣59␣38␣50␣36␣32␣30␣⏎64
//                             0␣F1␣22␣36␣30␣37␣44␣32␣33␣⏎640␣F1␣23␣30␣32␣34␣FF␣FF␣FF␣⏎
// 2020-11-15 10:34:29.276                                                            22␣F1␣90⏎:          VIN: WBY8P620607D23024



#define I3_PID_BDC_VIN                               0xF190
        // Returns the VIN - 17 byte string response

// POLL line
  //{ I3_ECU_BDC_TX, I3_ECU_BDC_RX, VEHICLE_POLL_TYPE_OBDIIEXTENDED, I3_PID_BDC_VIN,                  {  0,300,300,300 }, 0, ISOTP_EXTADR },   // 0xF190
