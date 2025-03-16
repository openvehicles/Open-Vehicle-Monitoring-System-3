/*
;    Project:       Open Vehicle Monitor System
;    Date:          21th January 2019
;
;   (C) 2022 Michael Geddes <frog@bunyip.wheelycreek.net>
;
; ----- Kona/Kia Module -----
;    (C) 2019       Geir Øyvind Vælidalo <geir@validalo.net>
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
#include "vehicle_hyundai_ioniq5.h"

OvmsVehicle::vehicle_command_t OvmsHyundaiIoniqEv::CommandLock(const char *pin)
{
  return NotImplemented;
  /*
  OvmsVehicle::vehicle_command_t res = SetDoorLock(false, pin) ? Success : Fail;
  return res;
  */
}

OvmsVehicle::vehicle_command_t OvmsHyundaiIoniqEv::CommandUnlock(const char *pin)
{
  return NotImplemented;
  /*
  OvmsVehicle::vehicle_command_t res = SetDoorLock(true, pin) ? Success : Fail;
  return res;
  */
}

OvmsVehicle::vehicle_command_t OvmsHyundaiIoniqEv::CommandWakeup()
{
  XARM("OvmsHyundaiIoniqEv::CommandWakeup");
  // Keep Awake for 5 mins.
  hif_keep_awake = 300;
  PollState_Running();
  ESP_LOGI(TAG, "CommandWakeup - Keeping awake for 5 minutes");
  XDISARM;
  return Success;
}

#ifdef XIQ_CAN_WRITE
/**
 * Command to enable IGN1
 */
void xiq_ign1(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  /*
  OvmsHyundaiIoniqEv *car = (OvmsHyundaiIoniqEv *) MyVehicleFactory.ActiveVehicle();
  car->IGN1Relay( strcmp(argv[0], "on") == 0, argv[1] );
  */
}

/**
 * Command to enable IGN2
 */
void xiq_ign2(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  //OvmsHyundaiIoniqEv* car = (OvmsHyundaiIoniqEv*) MyVehicleFactory.ActiveVehicle();
  //car->IGN2Relay( strcmp(argv[0],"on")==0, argv[1] );
}


/**
 * Command to enable ACC relay
 */
void xiq_acc_relay(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  /*
  OvmsHyundaiIoniqEv *car = (OvmsHyundaiIoniqEv *) MyVehicleFactory.ActiveVehicle();
  car->ACCRelay( strcmp(argv[0], "on") == 0, argv[1] );
  */
}

/**
 * Command to enable start relay
 */
void xiq_start_relay(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  /*
  OvmsHyundaiIoniqEv *car = (OvmsHyundaiIoniqEv *) MyVehicleFactory.ActiveVehicle();
  car->StartRelay( strcmp(argv[0], "on") == 0, argv[1] );
  */
}

void xiq_sjb(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  //OvmsHyundaiIoniqEv* car = (OvmsHyundaiIoniqEv*) MyVehicleFactory.ActiveVehicle();
  //car->Send_SJB_Command(strtol(argv[0],NULL,16), strtol(argv[1],NULL,16), strtol(argv[2],NULL,16));
}

void xiq_bcm(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  //OvmsHyundaiIoniqEv* car = (OvmsHyundaiIoniqEv*) MyVehicleFactory.ActiveVehicle();
  //car->Send_BCM_Command(strtol(argv[0],NULL,16), strtol(argv[1],NULL,16), strtol(argv[2],NULL,16));
}

void xiq_set_head_light_delay(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  //OvmsHyundaiIoniqEv* car = (OvmsHyundaiIoniqEv*) MyVehicleFactory.ActiveVehicle();
  //car->SetHeadLightDelay(strcmp(argv[0],"on")==0);
}

void xiq_set_one_touch_turn_signal(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  //OvmsHyundaiIoniqEv* car = (OvmsHyundaiIoniqEv*) MyVehicleFactory.ActiveVehicle();
  //car->SetOneThouchTurnSignal(strtol(argv[0],NULL,10));
}

void xiq_set_auto_door_unlock(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  //OvmsHyundaiIoniqEv* car = (OvmsHyundaiIoniqEv*) MyVehicleFactory.ActiveVehicle();
  //car->SetAutoDoorUnlock(strtol(argv[0],NULL,10));
}

void xiq_set_auto_door_lock(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  //OvmsHyundaiIoniqEv* car = (OvmsHyundaiIoniqEv*) MyVehicleFactory.ActiveVehicle();
  //car->SetAutoDoorLock(strtol(argv[0],NULL,10));
}
#endif

/**
 * Print out the aux battery voltage.
 */
void xiq_aux(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  writer->printf("AUX BATTERY\n");
  if (StdMetrics.ms_v_bat_12v_voltage->IsDefined())
    {
    const std::string &auxBatt = StdMetrics.ms_v_bat_12v_voltage->AsUnitString("-", Volts, 2);
    writer->printf("Voltage: %s\n", auxBatt.c_str());
    }

  if (MyVehicleFactory.m_currentvehicle == NULL) {
    writer->puts("No vehicle module selected");
    return;
  }

  OvmsHyundaiIoniqEv *niro = (OvmsHyundaiIoniqEv *) MyVehicleFactory.ActiveVehicle();

  if (niro->m_b_aux_soc->IsDefined()) {
    const std::string &auxSOC = niro->m_b_aux_soc->AsUnitString("-", Percentage, 1);
    writer->printf("SOC: %s\n", auxSOC.c_str());
  }
}

/**
 * Print out the VIN information.
 */
void xiq_vin(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  if (MyVehicleFactory.m_currentvehicle == NULL) {
    writer->puts("Error: No vehicle module selected");
    return;
  }

  OvmsHyundaiIoniqEv *hif = (OvmsHyundaiIoniqEv *) MyVehicleFactory.ActiveVehicle();
  if (hif->m_vin[0] == 0) {
    writer->printf("Requesting VIN ... ");
    switch (hif->RequestVIN()) {
      case IqVinStatus::Success:
        writer->puts("OK");
        break;
      case IqVinStatus::BadBuffer:
        writer->puts("Bad Buffer");
        return;
      case IqVinStatus::TxFail:
        writer->puts("Transmit Fail");
        return;
      case IqVinStatus::Timeout:
        writer->puts("Timeout");
        return;
      case IqVinStatus::ProtocolErr:
        writer->puts("Protocol Error");
        return;
      case IqVinStatus::BadFormat:
        writer->puts("Unrecognised");
        return;
      case IqVinStatus::NotAwake:
        writer->puts("Car Not Awake");
        return;
      default:
        writer->puts("Failed");
        return;
    }
  }
  writer->printf("VIN\n");
  writer->printf("Vin: %s\n", hif->m_vin);
  if (hif->m_vin[0] == 0) {
    return;
  }
  //
  // See https://en.wikipedia.org/wiki/Vehicle_identification_number#World_manufacturer_identifier
  //
  writer->printf("World manufacturer identifier: ");
  if (hif->m_vin[0] == '5' && hif->m_vin[1] == 'X') {
    writer->printf("United States Hyundai/Kia\n");
  }
  else if (hif->m_vin[0] == 'K' && hif->m_vin[1] == 'M' && hif->m_vin[2] == 'H') {
    writer->printf("South Korea Hyundai\n");
  }
  else if (hif->m_vin[0] == 'T' && hif->m_vin[1] == 'M' && hif->m_vin[2] == 'A') {
    writer->printf("Czech Republic Hyundai\n");
  }
  else if (hif->m_vin[0] == '2' && hif->m_vin[1] == 'H' && hif->m_vin[2] == 'M') {
    writer->printf("Canada Hyundai\n");
  }
  else if (hif->m_vin[0] == '5' && hif->m_vin[1] == 'N' && hif->m_vin[2] == 'M') {
    writer->printf("United States Hyundai\n");
  }
  else if (hif->m_vin[0] == '5' && hif->m_vin[1] == 'N' && hif->m_vin[2] == 'P') {
    writer->printf("United States Hyundai\n");
  }
  else if (hif->m_vin[0] == '9' && hif->m_vin[1] == 'B' && hif->m_vin[2] == 'H') {
    writer->printf("Brazil Hyundai\n");
  }
  else if (hif->m_vin[0] == '9' && hif->m_vin[1] == '5' && hif->m_vin[2] == 'P') {
    writer->printf("Brazil CAOA/Hyundai\n");
  }
  else if (hif->m_vin[0] == 'L' && hif->m_vin[1] == 'B' && hif->m_vin[2] == 'E') {
    writer->printf("China Beijing Hyundai\n");
  }
  else if (hif->m_vin[0] == 'K' && hif->m_vin[1] == 'N') {
    writer->printf("South Korea Kia ");

    if (hif->m_vin[2] == 'A' || hif->m_vin[2] == 'D') {
      writer->printf("MPV/SUV/RV (Outside USA, Canada, Mexico)\n");
    }
    else if (hif->m_vin[2] == 'C') {
      writer->printf("Commercial Vehicle\n");
    }
    else if (hif->m_vin[2] == 'D') {
      writer->printf("MPV/SUV/RV (USA, Canada, Mexico)\n");
    }
    else if (hif->m_vin[2] == 'H') {
      writer->printf("Van\n");
    }
    else {
      writer->printf("\n");
    }
  }
  else if (hif->m_vin[0] == 'M' && hif->m_vin[1] == 'S' && hif->m_vin[2] == '0') {
    writer->printf("Myanmar Kia\n");
  }
  else if (hif->m_vin[0] == 'P' && hif->m_vin[1] == 'N' && hif->m_vin[2] == 'A') {
    writer->printf("Malaysia Kia\n");
  }
  else if (hif->m_vin[0] == 'U' && hif->m_vin[1] == '5' && hif->m_vin[2] == 'Y') {
    writer->printf("Slovakia Kia\n");
  }
  else if (hif->m_vin[0] == '3' && hif->m_vin[1] == 'K' && hif->m_vin[2] == 'P') {
    writer->printf("Mexico Kia\n");
  }
  else if (hif->m_vin[0] == '9' && hif->m_vin[1] == 'U' && hif->m_vin[2] == 'W') {
    writer->printf("Uruguay Kia\n");
  }
  else if (hif->m_vin[0] == 'L' && hif->m_vin[1] == 'J' && hif->m_vin[2] == 'D') {
    writer->printf("China Dongfeng Yueda Kia\n");
  }
  else {
    writer->printf("Unknown %c%c%c\n", hif->m_vin[0], hif->m_vin[1], hif->m_vin[2]);
  }

  writer->printf("Vehicle Line: ");
  switch (hif->m_vin[3]) {
    case 'J':
      writer->printf("Soul\n");
      break;
    case 'C':
      writer->printf("EV6 ");
      switch (hif->m_vin[4]) {
        case '3':
          writer->printf("77.4kwH ");
          break;
        case '4':
          writer->printf("GT-Line ");
          break;
        case '5':
          writer->printf("GT ");
          break;
        default:
          writer->printf("(Unknown %c) ", hif->m_vin[4]);
      }
      switch (hif->m_vin[5]) {
        case '4':
          writer->puts("2WD");
          break;
        case 'D':
          writer->puts("4WD");
          break;
      default: writer->puts("");
      }
      switch (hif->m_vin[7]) {
        case 'A':
          writer->puts("KMA Electric 111.2Ah / RR 160kW (697V)");
          break;
        case 'B':
          writer->puts("KMA Electric 111.2Ah / RR 160kW (522.7V)");
          break;
        case 'C':
          writer->puts("KMA Electric 111.2Ah / FR 70kW / RR 160kW (697V)");
          break;
        case 'D':
          writer->puts("KMA Electric 111.2Ah / FR 70kW / RR 160kW (522.7V)");
          break;
        case 'E':
          writer->puts("KMA Electric 111.2Ah / FR 160kW / RR 270kW (697V)");
          break;
      }

      break;
    case 'K':
      switch (hif->m_vin[4]) {
        case 'R':
          writer->printf("Ioniq 5\n");
          writer->printf("Motor type: ");
          switch (hif->m_vin[7]) {

            case 'A': writer->printf("Electric motor, 160kw, (111.2Ah/72.6 kWh Battery), RWD 22\n"); break;
            case 'B': writer->printf("Electric motor, 125kw, (111.2Ah/58.0 kWh Battery), RWD 22\n"); break;
            case 'E': writer->printf("Electric motor, 168kw, (111.2Ah/77.4 kWh Battery), RWD 22\n"); break;
            case 'F': writer->printf("Electric motors, 239kw, (111.2Ah/77.4 kWh Battery), AWD 22\n"); break;
            default:  writer->printf("Unknown %c\n", hif->m_vin[7]); break;
          }
          break;
        default:
          writer->printf("Kona\n");
      }
      break;
    case 'M': // Entourage, Ioniq 6, Genesis GV70
      switch (hif->m_vin[4]) {
          writer->printf("Ioniq 6\n");
          writer->printf("Motor type: ");
          switch (hif->m_vin[7]) {
            case 'A': writer->printf("Electric motor, 168kw, (111.2Ah/77.4 kWh Battery), RWD 23\n"); break;
            case 'B': writer->printf("Electric motor, 111kw, (111.2Ah/53.0 kWh Battery), RWD 23\n"); break;
            case 'C': writer->printf("Electric motors, 239kw, (111.2Ah/77.4 kWh Battery), AWD 23\n"); break;
            default:  writer->printf("Unknown %c\n", hif->m_vin[7]); break;
          }
      }
      break;
    default:
      writer->printf("Unknown %c\n", hif->m_vin[3]);
      break;
  }

  if (hif->m_vin[4] == 'R') {
    // Ioniq 5. Is this premium?
  }
  else {
    writer->printf("Model: ");
    if (hif->m_vin[4] == 'C') {
      writer->printf("High grade.\n");
    }
    else {
      writer->printf("Unknown %c\n", hif->m_vin[4]);
    }
  }

  //
  // See https://en.wikibooks.org/wiki/Vehicle_Identification_Numbers_(VIN_codes)/Hyundai/VIN_Codes#Model_Year
  //
  int year = 0;
  char ch_year = hif->m_vin[9];
  if (ch_year >= '1' && ch_year <= '9') {
    year = 2001 + (ch_year - '1');
  }
  else if (ch_year == 'X') {
    year = 1999;
  }
  else if (ch_year == 'Y') {
    year = 2000;
  }
  else if (ch_year >= 'A' && ch_year <= 'H') { // Missing I
    year = 2010 + (ch_year - 'A');
  }
  else if (ch_year >= 'J' && ch_year <= 'N') { // Missing O
    year = 2018 + (ch_year - 'J');
  }
  else if (ch_year == 'P') { // Possibly missing Q
    year = 2023;
  }
  else if (ch_year >= 'R' && ch_year <= 'W') {
    year = 2024 + (ch_year - 'R');
  }

  if (year > 0) {
    writer->printf("Model year: %04d\n", year);
  }
  else {
    writer->printf("Model year: Unknown\n");
  }

  //
  // See https://en.wikibooks.org/wiki/Vehicle_Identification_Numbers_(VIN_codes)/Hyundai/VIN_Codes#Manufacturing_Plant_Code
  //
  if (hif->m_vin[10] != 0 && hif->m_vin[10] != '-') {
    writer->printf("Production plant: ");
    switch (hif->m_vin[10]) {
      case '5':
        writer->printf("Hwaseong (Korea)\n");
        break;
      case '6':
        writer->printf("Soha-ri (Korea)\n");
        break;
      case '7':
        writer->printf("Gwangju (Korea)\n");
        break;
      case 'T':
        writer->printf("Seosan (Korea)\n");
        break;
      case 'Y':
        writer->printf("Yangon (Myanmar)\n");
        break;
      case 'U':
        writer->printf("Ulsan (Korea)\n");
        break;
      default:
        writer->printf("Unknown %c\n", hif->m_vin[10]);
    }
  }

  if (hif->m_vin[11] != '-' && hif->m_vin[11] != 0) {
    writer->printf("Sequence number: %c%c%c%c%c%c\n", hif->m_vin[11], hif->m_vin[12], hif->m_vin[13], hif->m_vin[14], hif->m_vin[15], hif->m_vin[16]);
  }
}

/**
 * Print out information of the tpms.
 */
void xiq_tpms(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  if (MyVehicleFactory.m_currentvehicle == NULL) {
    writer->puts("Error: No vehicle module selected");
    return;
  }

  OvmsHyundaiIoniqEv *car = (OvmsHyundaiIoniqEv *) MyVehicleFactory.ActiveVehicle();

  writer->printf("TPMS\n");
  // Front left
  if (StdMetrics.ms_v_tpms_pressure->IsDefined()) {
    const std::string &fl_pressure = StdMetrics.ms_v_tpms_pressure->ElemAsUnitString(MS_V_TPMS_IDX_FL, "-", ToUser, 1);
    const std::string &fl_temp = StdMetrics.ms_v_tpms_temp->ElemAsUnitString(MS_V_TPMS_IDX_FL, "-", ToUser, 1);
    writer->printf("1 Front-Left  ID:%" PRIu32 " %s %s\n", car->kia_tpms_id[0], fl_pressure.c_str(), fl_temp.c_str());
  }
  // Front right
  if (StdMetrics.ms_v_tpms_pressure->IsDefined()) {
    const std::string &fr_pressure = StdMetrics.ms_v_tpms_pressure->ElemAsUnitString(MS_V_TPMS_IDX_FR, "-", ToUser, 1);
    const std::string &fr_temp = StdMetrics.ms_v_tpms_temp->ElemAsUnitString(MS_V_TPMS_IDX_FR, "-", ToUser, 1);
    writer->printf("2 Front-Right ID:%" PRIu32 " %s %s\n", car->kia_tpms_id[1], fr_pressure.c_str(), fr_temp.c_str());
  }
  // Rear left
  if (StdMetrics.ms_v_tpms_pressure->IsDefined()) {
    const std::string &rl_pressure = StdMetrics.ms_v_tpms_pressure->ElemAsUnitString(MS_V_TPMS_IDX_RL, "-", ToUser, 1);
    const std::string &rl_temp = StdMetrics.ms_v_tpms_temp->ElemAsUnitString(MS_V_TPMS_IDX_RL, "-", ToUser, 1);
    writer->printf("3 Rear-Left   ID:%" PRIu32 " %s %s\n", car->kia_tpms_id[2], rl_pressure.c_str(), rl_temp.c_str());
  }
  // Rear right
  if (StdMetrics.ms_v_tpms_pressure->IsDefined()) {
    const std::string &rr_pressure = StdMetrics.ms_v_tpms_pressure->ElemAsUnitString(MS_V_TPMS_IDX_RR, "-", ToUser, 1);
    const std::string &rr_temp = StdMetrics.ms_v_tpms_temp->ElemAsUnitString(MS_V_TPMS_IDX_RR, "-", ToUser, 1);
    writer->printf("4 Rear-Right  ID:%" PRIu32 " %s %s\n", car->kia_tpms_id[3], rr_pressure.c_str(), rr_temp.c_str());
  }
}

/**
 * Print out information of a trip.
 */
void xiq_trip_( const char *title,  OvmsWriter *writer, OvmsMetricFloat *metric_trip, OvmsMetricFloat *metric_energy, OvmsMetricFloat *metric_energy_recd )
{
  XARM("xiq_trip_");

  metric_unit_t rangeUnit = MyUnitConfig.GetUserUnit(GrpDistance, Kilometers);
  metric_unit_t consumpUnit = MyUnitConfig.GetUserUnit(GrpConsumption, KPkWh);
  metric_unit_t energyUnit = MyUnitConfig.GetUserUnit(GrpEnergy, kWh);

  writer->printf("%s\n", title);

  // Consumption
  float posTrip = metric_trip->AsFloat(rangeUnit);
  float energyUsed = metric_energy->AsFloat(kWh);
  if (posTrip <= 0) {
    writer->puts("No distance travelled\n");
  }
  else if (energyUsed == 0) {
    writer->puts("No energy used\n");
  }
  else {
    float consumption =  UnitConvert(KPkWh, consumpUnit, posTrip/energyUsed);

    // Total consumption
    float totalConsumption = energyUsed + metric_energy_recd->AsFloat(kWh);

    // Trip distance
    if (metric_trip->IsDefined()) {
      const std::string &distance = metric_trip->AsUnitString("-", rangeUnit, 1);
      writer->printf("Dist %s\n", distance.c_str());
    }
    writer->printf("Cons %.*f%s\n", 2, consumption, OvmsMetricUnitLabel(consumpUnit));

    // Discharge
    if (metric_energy->IsDefined()) {
      const std::string &discharge = metric_energy->AsUnitString("-", energyUnit, 1);
      writer->printf("Disc %s\n", discharge.c_str());
    }

    // Recuperation
    if (metric_energy_recd->IsDefined()) {
      const std::string &recuparation = metric_energy_recd->AsUnitString("-", energyUnit, 1);
      writer->printf("Rec %s\n", recuparation.c_str());
    }

    writer->printf("Tot %.*f%s\n", 2, UnitConvert(kWh, consumpUnit, totalConsumption), OvmsMetricUnitLabel(consumpUnit));
  }
  // ODO
  if (StdMetrics.ms_v_pos_odometer->IsDefined()) {
    const std::string &ODO = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1);
    writer->printf("ODO %s\n", ODO.c_str());
  }
  XDISARM;
}

/**
 * Print out information of the current trip.
 */
void xiq_trip_since_parked(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  XARM("xiq_trip_since_parked");
  if (MyVehicleFactory.m_currentvehicle == NULL) {
    writer->puts("Error: No vehicle module selected");
    XDISARM;
    return;
  }
  xiq_trip_("TRIP SINCE PARK", writer, StdMetrics.ms_v_pos_trip, StdMetrics.ms_v_bat_energy_used, StdMetrics.ms_v_bat_energy_recd);
  XDISARM;
}

/**
 * Print out information of the current trip.
 */
void xiq_trip_since_charge(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  XARM("xiq_trip_since_charge");
  if (MyVehicleFactory.m_currentvehicle == NULL) {
    writer->puts("Error: No vehicle module selected");
    XDISARM;
    return;
  }

  OvmsHyundaiIoniqEv *mycar = (OvmsHyundaiIoniqEv *)(MyVehicleFactory.ActiveVehicle());

  xiq_trip_( "TRIP SINCE CHARGE", writer, mycar->ms_v_pos_trip,  mycar->ms_v_trip_energy_used, mycar->ms_v_trip_energy_recd);

  XDISARM;
}

void OvmsHyundaiIoniqEv::RangeCalcStat(OvmsWriter *writer)
{
  if (iq_range_calc) {
    iq_range_calc->displayStoredTrips(writer);
  }

  if (kia_park_trip_counter.Started()) {
    metric_unit_t rangeUnit = MyUnitConfig.GetUserUnit(GrpDistance, Kilometers);

    writer->puts("Current Trip Counter");
    writer->printf("Dist: %.2f%s\n",
      UnitConvert( Kilometers, rangeUnit, kia_park_trip_counter.GetDistance()),
      OvmsMetricUnitLabel(rangeUnit));

    if (kia_park_trip_counter.HasEnergyData()) {
      metric_unit_t energyUnit = MyUnitConfig.GetUserUnit(GrpEnergy, kWh);
      const char *energyLabel = OvmsMetricUnitLabel(energyUnit);
      writer->printf("Energy Consumed: %.2f%s\n",
        UnitConvert(kWh, energyUnit, kia_park_trip_counter.GetEnergyConsumed()), energyLabel);
      writer->printf("Energy Recovered: %.2f%s\n",
        UnitConvert(kWh, energyUnit, kia_park_trip_counter.GetEnergyRecuperated()), energyLabel);
      writer->printf("Total Energy Used: %.2f%s\n",
        UnitConvert(kWh, energyUnit, kia_park_trip_counter.GetEnergyUsed()), energyLabel);
      auto charged = kia_park_trip_counter.GetEnergyCharged();
      if (charged != 0) {
        writer->printf("Energy Charged: %.2f%s\n",
          UnitConvert(kWh, energyUnit, charged), energyLabel);
      }
    }

    if (kia_park_trip_counter.HasChargeData()) {
      metric_unit_t chargeUnit = MyUnitConfig.GetUserUnit(GrpCharge, AmpHours);
      const char *chargeLabel = OvmsMetricUnitLabel(chargeUnit);
      writer->printf("Charge Consumed: %.2f%s\n",
        UnitConvert(AmpHours, chargeUnit,kia_park_trip_counter.GetChargeConsumed()), chargeLabel);
      writer->printf("Charge Recovered: %.2f%s\n",
        UnitConvert(AmpHours, chargeUnit, kia_park_trip_counter.GetChargeRecuperated()), chargeLabel);
      writer->printf("Total Charge Used: %.2f%s\n",
        UnitConvert(AmpHours, chargeUnit, kia_park_trip_counter.GetChargeUsed()), chargeLabel);
      writer->printf("Charging: %s", kia_park_trip_counter.Charging() ? "Yes\n" : "No\n");
      auto charged = kia_park_trip_counter.GetChargeCharged();
      if (charged != 0) {
        writer->printf("Charge Charged: %.2f%s\n",
          UnitConvert(AmpHours, chargeUnit, charged), chargeLabel);
      }
    }
  }

}

void xiq_range_stat(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  if (MyVehicleFactory.m_currentvehicle == NULL) {
    writer->puts("Error: No vehicle module selected");
    return;
  }
  OvmsHyundaiIoniqEv *mycar = (OvmsHyundaiIoniqEv *)(MyVehicleFactory.ActiveVehicle());
  mycar->RangeCalcStat(writer);
}

void OvmsHyundaiIoniqEv::RangeCalcReset()
{
  iq_range_calc->resetTrips();
}

void xiq_range_reset(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  OvmsHyundaiIoniqEv *mycar = (OvmsHyundaiIoniqEv *)(MyVehicleFactory.ActiveVehicle());
  if (mycar) {
    writer->puts("Error: No vehicle module selected");
    return;
  }
  mycar->RangeCalcReset();
}

void xiq_aux_monitor(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
  OvmsVehicle *mycar = MyVehicleFactory.ActiveVehicle();
  if (mycar == NULL) {
    writer->puts("Error: No vehicle module selected");
    return;
  }
  std::string stat = mycar->BatteryMonStat();
  writer->puts(stat.c_str());
}
