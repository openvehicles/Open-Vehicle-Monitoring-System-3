# can3-3.3.005-800-gc60d79ed-dirty_ota_0_edge-20260410-231401.crtd — Capture Notes

## Capture info

| Field | Value |
|---|---|
| Firmware | `3.3.005-800-gc60d79ed-dirty/ota_0/edge` |
| Bus | can3 |
| Captured | 20260410-231401 |
| Duration | ~83s |
| Frames | 50825 |

## Sequence

capture 7

---

## Notes

<!-- Add analysis notes here -->

OVMS# config set xvg cc-temp 18
Parameter has been set.
I (231378) webcommand: HttpCommandStream[0x3f85c86c]: 3325100 bytes free, executing: config set xvg cc-temp 18
I (234298) v-vwegolf: KCAN idle: OCU node presence cleared
I (236778) webserver: HTTP POST /api/execute
I (236788) webcommand: HttpCommandStream[0x3f85d91c]: 3324884 bytes free, executing: climatecontrol on
I (236788) v-vwegolf: Climate control: start
I (236788) v-vwegolf: Climate control: KCAN quiet 12 s, OEM OCU quiet 243 s — waking bus
I (236798) v-vwegolf: WakeKcanBus: asserting dominant bits on KCAN
E (236998) can: can3: intr=173148 rxpkt=173644 txpkt=0 errflags=0x80001080 rxerr=0 txerr=8 rxinval=0 rxovr=0 txovr=0 txdelay=0 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (236998) can: can3: intr=173149 rxpkt=173645 txpkt=0 errflags=0x85001101 rxerr=0 txerr=31 rxinval=0 rxovr=0 txovr=0 txdelay=0 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (236998) can: can3: intr=173153 rxpkt=173648 txpkt=1 errflags=0x80001080 rxerr=9 txerr=31 rxinval=0 rxovr=0 txovr=0 txdelay=0 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (237578) can: can3: intr=173886 rxpkt=174380 txpkt=6 errflags=0x23401c01 rxerr=0 txerr=26 rxinval=0 rxovr=0 txovr=0 txdelay=0 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (237798) can: can3: intr=174172 rxpkt=174669 txpkt=7 errflags=0x23401c01 rxerr=0 txerr=25 rxinval=0 rxovr=0 txovr=0 txdelay=0 txfail=0 wdgreset=0 errreset=0 txqueue=0
OVMS# climatecontrol on
Climate Control 
on
I (238148) v-vwegolf: BAP clima start sent: counter=0x01 temp=18°C
E (238218) can: can3: intr=174640 rxpkt=175136 txpkt=12 errflags=0x23401c01 rxerr=0 txerr=20 rxinval=0 rxovr=0 txovr=0 txdelay=2 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (238408) can: can3: intr=174807 rxpkt=175311 txpkt=14 errflags=0x23401c01 rxerr=0 txerr=18 rxinval=0 rxovr=0 txovr=0 txdelay=2 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (239078) can: can3: intr=175468 rxpkt=175972 txpkt=18 errflags=0x23401c01 rxerr=0 txerr=14 rxinval=0 rxovr=0 txovr=0 txdelay=2 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (239278) can: can3: intr=175666 rxpkt=176173 txpkt=19 errflags=0x23401c01 rxerr=0 txerr=13 rxinval=0 rxovr=0 txovr=0 txdelay=2 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (239448) can: can3: intr=175832 rxpkt=176340 txpkt=21 errflags=0x23401c01 rxerr=0 txerr=11 rxinval=0 rxovr=0 txovr=0 txdelay=2 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (239758) can: can3: intr=176125 rxpkt=176634 txpkt=22 errflags=0x23401c01 rxerr=0 txerr=10 rxinval=0 rxovr=0 txovr=0 txdelay=2 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (240068) can: can3: intr=176421 rxpkt=176932 txpkt=24 errflags=0x23401c01 rxerr=0 txerr=8 rxinval=0 rxovr=0 txovr=0 txdelay=2 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (240378) can: can3: intr=176716 rxpkt=177228 txpkt=27 errflags=0x23401c01 rxerr=0 txerr=5 rxinval=0 rxovr=0 txovr=0 txdelay=2 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (240998) can: can3: intr=177280 rxpkt=177795 txpkt=30 errflags=0x23401c01 rxerr=0 txerr=2 rxinval=0 rxovr=0 txovr=0 txdelay=2 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (241298) can: can3: intr=177596 rxpkt=178112 txpkt=32 errflags=0x23401c01 rxerr=0 txerr=0 rxinval=0 rxovr=0 txovr=0 txdelay=2 txfail=0 wdgreset=0 errreset=0 txqueue=0
I (243038) ovms-duk-util: [lib/abrp.js:131:log] (2026-04-10 23:14:23.000+02:00) WARN: Metrics collected. Finished in 705.96 ms
I (261738) webserver: HTTP POST /api/execute
I (261738) webcommand: HttpCommandStream[0x3f8f6568]: 3283800 bytes free, executing: climatecontrol off
I (261758) v-vwegolf: Climate control: stop
E (261788) can: can3: intr=194226 rxpkt=194785 txpkt=169 errflags=0x23401c01 rxerr=0 txerr=0 rxinval=0 rxovr=0 txovr=0 txdelay=3 txfail=0 wdgreset=0 errreset=0 txqueue=0
OVMS# climatecontrol off
Climate Control 
off
I (261798) v-vwegolf: BAP clima stop sent: counter=0x02 temp=18°C
I (267388) webserver: HTTP POST /api/execute
I (267418) webcommand: HttpCommandStream[0x3f85c574]: 3284896 bytes free, executing: config set xvg cc-temp 22
OVMS# config set xvg cc-temp 22
Parameter has been set.
I (271108) webserver: HTTP POST /api/execute
I (271118) webcommand: HttpCommandStream[0x3f8f6568]: 3322456 bytes free, executing: climatecontrol on
OVMS# climatecontrol on
Climate Control 
on
I (271138) v-vwegolf: Climate control: start
I (271138) v-vwegolf: BAP clima start sent: counter=0x03 temp=18°C
E (272008) can: can3: intr=203625 rxpkt=204297 txpkt=178 errflags=0x23401c01 rxerr=0 txerr=0 rxinval=0 rxovr=0 txovr=0 txdelay=4 txfail=0 wdgreset=0 errreset=0 txqueue=0
I (273198) ovms-duk-util: [lib/abrp.js:131:log] (2026-04-10 23:14:53.000+02:00) WARN: Metrics collected. Finished in 728.21 ms
E (283608) can: can3: intr=214177 rxpkt=214889 txpkt=255 errflags=0x22401c02 rxerr=0 txerr=0 rxinval=0 rxovr=0 txovr=0 txdelay=4 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (283608) can: can3: intr=214177 rxpkt=214890 txpkt=255 errflags=0x23401c01 rxerr=0 txerr=0 rxinval=0 rxovr=0 txovr=0 txdelay=4 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (296208) can: can3: intr=223141 rxpkt=223864 txpkt=337 errflags=0x22401c02 rxerr=0 txerr=0 rxinval=0 rxovr=0 txovr=0 txdelay=4 txfail=0 wdgreset=0 errreset=0 txqueue=0
E (296208) can: can3: intr=223141 rxpkt=223865 txpkt=337 errflags=0x23401c01 rxerr=0 txerr=0 rxinval=0 rxovr=0 txovr=0 txdelay=4 txfail=0 wdgreset=0 errreset=0 txqueue=0
I (302298) housekeeping: 2026-04-10 23:15:22 CEST (RAM: 8b=63492-64704 32b=11920 SPI=3184980-3269816)
I (303398) webserver: HTTP POST /api/execute
I (303398) webcommand: HttpCommandStream[0x3f85d91c]: 3322268 bytes free, executing: climatecontrol off
I (303418) v-vwegolf: Climate control: stop
I (303428) v-vwegolf: BAP clima stop sent: counter=0x04 temp=22°C
OVMS# climatecontrol off
Climate Control 
off
E (303738) can: can3: intr=228198 rxpkt=228908 txpkt=388 errflags=0x23401c01 rxerr=0 txerr=0 rxinval=0 rxovr=0 txovr=0 txdelay=5 txfail=0 wdgreset=0 errreset=0 txqueue=0
