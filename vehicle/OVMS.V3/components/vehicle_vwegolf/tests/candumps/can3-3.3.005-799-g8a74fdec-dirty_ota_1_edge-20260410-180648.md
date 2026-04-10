# can3-3.3.005-799-g8a74fdec-dirty_ota_1_edge-20260410-180648.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-799-g8a74fdec-dirty/ota_1/edge` |
| Bus | can3 |
| Captured | 20260410-180648 |
| Duration | ~64s |
| Frames | 37769 |

## Sequence

deep sleep clima start with climatecontrol on command from ovsm shell

---

## Notes

works, but lots of noise in the shell 

I (252939) v-vwegolf: Climate control: start
I (252939) v-vwegolf: Climate control: KCAN quiet 146 s, OEM OCU quiet 254 s — waking bus
I (252949) v-vwegolf: WakeKcanBus: asserting dominant bits on KCAN
E (253129) can: can3: intr=99660 rxpkt=99966 txpkt=145 errflags=0x80001080 rxerr=0 txerr=8 rxinval=0 rxovr=0 txovr=0 txdelay=7 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (253129) can: can3: intr=99661 rxpkt=99966 txpkt=145 errflags=0x80001080 rxerr=0 txerr=40 rxinval=0 rxovr=0 txovr=0 txdelay=7 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (253129) can: can3: intr=99662 rxpkt=99966 txpkt=145 errflags=0x80001080 rxerr=0 txerr=64 rxinval=0 rxovr=0 txovr=0 txdelay=7 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (253129) can: can3: intr=99663 rxpkt=99966 txpkt=145 errflags=0x80001080 rxerr=0 txerr=80 rxinval=0 rxovr=0 txovr=0 txdelay=7 txfail=0 wdgreset=0 errreset=0 txqueue=0
W (253129) mcp2515: can3 EFLG: TX_Err_Warn EWARN 
E (253139) can: can3: intr=99664 rxpkt=99966 txpkt=145 errflags=0xa00510a0 rxerr=0 txerr=96 rxinval=0 rxovr=0 txovr=0 txdelay=7 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (253139) can: can3: intr=99664 rxpkt=99968 txpkt=145 errflags=0x23401c01 rxerr=0 txerr=95 rxinval=0 rxovr=0 txovr=0 txdelay=7 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (253689) can: can3: intr=100281 rxpkt=100591 txpkt=151 errflags=0x23401c01 rxerr=0 txerr=90 rxinval=0 rxovr=0 txovr=0 txdelay=7 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (254099) can: can3: intr=100814 rxpkt=101140 txpkt=153 errflags=0x23401c01 rxerr=0 txerr=88 rxinval=0 rxovr=0 txovr=0 txdelay=7 txfail=0 wdgreset=0 errreset=0 txqueue=0
I (254319) v-vwegolf: BAP clima start sent: counter=0x05 temp=21°C
E (254479) can: can3: intr=101213 rxpkt=101538 txpkt=158 errflags=0x23401c01 rxerr=0 txerr=83 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (254559) can: can3: intr=101288 rxpkt=101617 txpkt=159 errflags=0x23401c01 rxerr=0 txerr=82 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (255319) can: can3: intr=102029 rxpkt=102364 txpkt=163 errflags=0x23401c01 rxerr=0 txerr=78 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (255519) can: can3: intr=102236 rxpkt=102574 txpkt=165 errflags=0x23401c01 rxerr=0 txerr=76 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (256369) can: can3: intr=103073 rxpkt=103416 txpkt=170 errflags=0x23401c01 rxerr=0 txerr=71 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (256659) can: can3: intr=103341 rxpkt=103688 txpkt=173 errflags=0x23401c01 rxerr=0 txerr=68 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (257269) can: can3: intr=103900 rxpkt=104252 txpkt=176 errflags=0x23401c01 rxerr=0 txerr=65 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (259169) can: can3: intr=105631 rxpkt=105991 txpkt=188 errflags=0x23401c01 rxerr=0 txerr=53 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (259729) can: can3: intr=106140 rxpkt=106503 txpkt=193 errflags=0x23401c01 rxerr=0 txerr=48 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (260369) can: can3: intr=106711 rxpkt=107090 txpkt=196 errflags=0x23401c01 rxerr=0 txerr=45 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (260539) can: can3: intr=106852 rxpkt=107233 txpkt=198 errflags=0x23401c01 rxerr=0 txerr=43 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (260709) can: can3: intr=107000 rxpkt=107386 txpkt=199 errflags=0x23401c01 rxerr=0 txerr=42 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (260869) can: can3: intr=107134 rxpkt=107521 txpkt=200 errflags=0x23401c01 rxerr=0 txerr=41 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
I (260929) ovms-server-v2: Incoming Msg: MP-0 A
I (260929) ovms-server-v2: Send MP-0 a
E (261459) can: can3: intr=107678 rxpkt=108070 txpkt=204 errflags=0x23401c01 rxerr=0 txerr=37 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (261459) can: can3: intr=107678 rxpkt=108072 txpkt=204 errflags=0x23001001 rxerr=0 txerr=37 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (262809) can: can3: intr=108893 rxpkt=109292 txpkt=213 errflags=0x23401c01 rxerr=0 txerr=28 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (263069) can: can3: intr=109098 rxpkt=109506 txpkt=214 errflags=0x23401c01 rxerr=0 txerr=27 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
I (264029) ovms-duk-util: [lib/abrp.js:131:log] (2026-04-10 18:07:12.000+02:00) WARN: Metrics collected. Finished in 594.40 ms
E (264379) can: can3: intr=110325 rxpkt=110733 txpkt=222 errflags=0x23401c01 rxerr=0 txerr=19 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (266889) can: can3: intr=112608 rxpkt=113018 txpkt=239 errflags=0x23401c01 rxerr=0 txerr=2 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (267139) can: can3: intr=112822 rxpkt=113233 txpkt=241 errflags=0x23401c01 rxerr=0 txerr=0 rxinval=0 rxovr=0 txovr=0 txdelay=9 txfail=0 wdgreset=0 errreset=0 txqueue=0
I (279359) webserver: HTTP GET /metrics
I (285099) webserver: HTTP GET /shell
I (287979) webserver: HTTP POST /api/execute
I (288019) webcommand: HttpCommandStream[0x3f8e07e4]: 3205728 bytes free, executing: climatecontrol off
I (288019) v-vwegolf: Climate control: stop
I (288029) v-vwegolf: BAP clima stop sent: counter=0x06 temp=21°C
E (288069) can: can3: intr=127238 rxpkt=127624 txpkt=381 errflags=0x23401c01 rxerr=0 txerr=0 rxinval=0 rxovr=0 txovr=0 txdelay=11 txfail=0 wdgreset=0 errreset=0 txqueue=0
I (292809) webserver: HTTP GET /metrics
I (297499) webserver: HTTP GET /shell


and ovms crashes soon after capture was terminated

Last boot was 9 second(s) ago
Time at boot: 2026-04-10 18:09:28 CEST
This is reset #20 since last power cycle
Detected boot reason: Crash (12/12)
Reset reason: Task watchdog (6)
Crash counters: 1 total, 0 early

Last crash: abort() was called on core 0
Current task on core 0: IDLE0, 432 stack bytes free
Current task on core 1: IDLE1, 440 stack bytes free
Backtrace:
0x4008dd2e 0x4008dfc9 0x4010cbac 0x4008418e
Event: vehicle.aux.12v.normal@ovms-server-v3 60 secs
WDT tasks: OVMS Events
Version: 3.3.005-799-g8a74fdec-dirty/ota_1/edge (build idf v3.3.4-854-g9063c8662 Apr 9 2026 23:36:04)

Hardware: OVMS WIFI BLE BT cores=2 rev=ESP32/3


and v.e.hvac is still not reliably tracking the true state of hvac as it remains true after climatecontrol off<!-- Add analysis notes here -->
