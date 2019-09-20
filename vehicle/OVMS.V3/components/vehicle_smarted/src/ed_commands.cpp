/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
 ;    (C) 2019       Thomas Heuer
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
 ;
 ; Most of the CAN Messages are based on https://github.com/MyLab-odyssey/ED_BMSdiag
 ; http://ed.no-limit.de/wiki/index.php/Hauptseite
 */

#include "vehicle_smarted.h"


/**
 * Set Recu wippen.
 */
void xse_recu(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  if (MyVehicleFactory.m_currentvehicle==NULL) {
    writer->puts("Error: No vehicle module selected");
    return;
  }
  bool on;
	if (strcmp("up", argv[0]) == 0) {
    on = true;
  }
  else if (strcmp("down", argv[0]) == 0) {
    on = false;
  }
  else {
    writer->puts("Error: argument must be 'up' or 'down'");
    return;
  }
  OvmsVehicleSmartED* smart = (OvmsVehicleSmartED*) MyVehicleFactory.ActiveVehicle();
  bool status = smart->CommandSetRecu( on );
  if (status)
    writer->printf("Recu set to: %s\n", (on == true ? "up" : "down"));
  else
    writer->puts("Error: Function need CAN-Write enable");
}

/**
 * Set ChargeTimer.
 */
void xse_chargetimer(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  if (MyVehicleFactory.m_currentvehicle==NULL) {
    writer->puts("Error: No vehicle module selected");
    return;
  }
  bool enable = false;
  int hours    = strtol(argv[0], NULL, 10);
  int minutes  = strtol(argv[1], NULL, 10);
  if (hours < 0 || hours > 23) {
    writer->puts("ERROR: Hour invalid (0-23)");
    return;
  }
  if (minutes < 0 || minutes > 59) {
    writer->puts("ERROR: Minute invalid (0-59)");
    return;
  }
  OvmsVehicleSmartED* smart = (OvmsVehicleSmartED*) MyVehicleFactory.ActiveVehicle();
  if( smart->CommandSetChargeTimer(enable, hours, minutes) == OvmsVehicle::Success ) {
    writer->printf("Charging Time set to: %d:%d\n", hours, minutes);
  }
  else {
    writer->puts("Error: Function need CAN-Write enable");
  }
}

