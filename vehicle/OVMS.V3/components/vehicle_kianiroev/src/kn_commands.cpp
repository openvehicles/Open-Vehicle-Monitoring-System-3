/*
;    Project:       Open Vehicle Monitor System
;    Date:          21th January 2019
;
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
#include "vehicle_kianiroev.h"

OvmsVehicle::vehicle_command_t OvmsVehicleKiaNiroEv::CommandLock(const char* pin)
  {
  return SetDoorLock(false,pin) ? Success:Fail;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleKiaNiroEv::CommandUnlock(const char* pin)
  {
  return SetDoorLock(true,pin) ? Success:Fail;
  }

/**
 * Command to enable IGN1
 */
void xkn_ign1(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaNiroEv* car = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();
  car->IGN1Relay( strcmp(argv[0],"on")==0, argv[1] );
	}

/**
 * Command to enable IGN2
 */
void xkn_ign2(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  //OvmsVehicleKiaNiroEv* car = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();
  //car->IGN2Relay( strcmp(argv[0],"on")==0, argv[1] );
	}


/**
 * Command to enable ACC relay
 */
void xkn_acc_relay(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaNiroEv* car = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();
  car->ACCRelay( strcmp(argv[0],"on")==0, argv[1] );
	}

/**
 * Command to enable start relay
 */
void xkn_start_relay(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaNiroEv* car = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();
  car->StartRelay( strcmp(argv[0],"on")==0, argv[1] );
	}

void xkn_sjb(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  //OvmsVehicleKiaNiroEv* car = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();
	//car->Send_SJB_Command(strtol(argv[0],NULL,16), strtol(argv[1],NULL,16), strtol(argv[2],NULL,16));
	}

void xkn_bcm(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  //OvmsVehicleKiaNiroEv* car = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();
	//car->Send_BCM_Command(strtol(argv[0],NULL,16), strtol(argv[1],NULL,16), strtol(argv[2],NULL,16));
	}

void xkn_set_head_light_delay(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  //OvmsVehicleKiaNiroEv* car = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();
	//car->SetHeadLightDelay(strcmp(argv[0],"on")==0);
	}

void xkn_set_one_touch_turn_signal(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  //OvmsVehicleKiaNiroEv* car = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();
	//car->SetOneThouchTurnSignal(strtol(argv[0],NULL,10));
	}

void xkn_set_auto_door_unlock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  //OvmsVehicleKiaNiroEv* car = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();
  //car->SetAutoDoorUnlock(strtol(argv[0],NULL,10));
	}

void xkn_set_auto_door_lock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  //OvmsVehicleKiaNiroEv* car = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();
	//car->SetAutoDoorLock(strtol(argv[0],NULL,10));
	}

/**
 * Print out the aux battery voltage.
 */
void xkn_aux(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
	if (MyVehicleFactory.m_currentvehicle==NULL)
		{
		writer->puts("Error: No vehicle module selected");
		return;
		}


	writer->printf("AUX BATTERY\n");
	if (StdMetrics.ms_v_bat_12v_voltage->IsDefined())
		{
		const std::string& auxBatt = StdMetrics.ms_v_bat_12v_voltage->AsUnitString("-", Volts, 2);
		writer->printf("Aux battery voltage %s\n", auxBatt.c_str());
		}

	OvmsVehicleKiaNiroEv* niro = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();

	if (niro->m_b_aux_soc->IsDefined())
		{
		const std::string& auxSOC = niro->m_b_aux_soc->AsUnitString("-", Percentage, 1);
		writer->printf("Aux battery SOC %s\n", auxSOC.c_str());
		}

	}

/**
 * Print out the VIN information.
 */
void xkn_vin(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  OvmsVehicleKiaNiroEv* soul = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();

	writer->printf("VIN\n");
	writer->printf("Vin: %s \n", soul->m_vin);

	//
	// See https://en.wikipedia.org/wiki/Vehicle_identification_number#World_manufacturer_identifier
	//
	writer->printf("World manufacturer identifier: ");
	if (soul->m_vin[0] == '5' && soul->m_vin[1] == 'X')
		{
		writer->printf("United States Hyundai/Kia\n");
		}
	else if (soul->m_vin[0] == 'K' && soul->m_vin[1] == 'M' && soul->m_vin[2] == 'H')
		{
		writer->printf("South Korea Hyundai\n");
		}
	else if (soul->m_vin[0] == 'T' && soul->m_vin[1] == 'M' && soul->m_vin[2] == 'A')
		{
		writer->printf("Czech Republic Hyundai\n");
		}
	else if (soul->m_vin[0] == '2' && soul->m_vin[1] == 'H' && soul->m_vin[2] == 'M')
		{
		writer->printf("Canada Hyundai\n");
		}
	else if (soul->m_vin[0] == '5' && soul->m_vin[1] == 'N' && soul->m_vin[2] == 'M')
		{
		writer->printf("United States Hyundai\n");
		}
	else if (soul->m_vin[0] == '5' && soul->m_vin[1] == 'N' && soul->m_vin[2] == 'P')
		{
		writer->printf("United States Hyundai\n");
		}
	else if (soul->m_vin[0] == '9' && soul->m_vin[1] == 'B' && soul->m_vin[2] == 'H')
		{
		writer->printf("Brazil Hyundai\n");
		}
	else if (soul->m_vin[0] == '9' && soul->m_vin[1] == '5' && soul->m_vin[2] == 'P')
		{
		writer->printf("Brazil CAOA/Hyundai\n");
		}
	else if (soul->m_vin[0] == 'L' && soul->m_vin[1] == 'B' && soul->m_vin[2] == 'E')
		{
		writer->printf("China Beijing Hyundai\n");
		}
	else if (soul->m_vin[0] == 'K' && soul->m_vin[1] == 'N')
		{
		writer->printf("South Korea Kia ");

		if(soul->m_vin[2]=='A' || soul->m_vin[2]=='D')
			{
			writer->printf("MPV/SUV/RV (Outside USA, Canada, Mexico)\n");
			}
		else if(soul->m_vin[2]=='C')
			{
			writer->printf("Commercial Vehicle\n");
			}
		else if(soul->m_vin[2]=='D')
			{
			writer->printf("MPV/SUV/RV (USA, Canada, Mexico)\n");
			}
		else if(soul->m_vin[2]=='H')
			{
			writer->printf("Van\n");
			}
		else
			{
			writer->printf("\n");
			}
		}
	else if (soul->m_vin[0] == 'M' && soul->m_vin[1] == 'S' && soul->m_vin[2] == '0')
		{
		writer->printf("Myanmar Kia\n");
		}
	else if (soul->m_vin[0] == 'P' && soul->m_vin[1] == 'N' && soul->m_vin[2] == 'A')
		{
		writer->printf("Malaysia Kia\n");
		}
	else if (soul->m_vin[0] == 'U' && soul->m_vin[1] == '5' && soul->m_vin[2] == 'Y')
		{
		writer->printf("Slovakia Kia\n");
		}
	else if (soul->m_vin[0] == '3' && soul->m_vin[1] == 'K' && soul->m_vin[2] == 'P')
		{
		writer->printf("Mexico Kia\n");
		}
	else if (soul->m_vin[0] == '9' && soul->m_vin[1] == 'U' && soul->m_vin[2] == 'W')
		{
		writer->printf("Uruguay Kia\n");
		}
	else if (soul->m_vin[0] == 'L' && soul->m_vin[1] == 'J' && soul->m_vin[2] == 'D')
		{
		writer->printf("China Dongfeng Yueda Kia\n");
		}
	else
		{
		writer->printf("Unknown %c%c%c\n",soul->m_vin[0],soul->m_vin[1],soul->m_vin[2]);
		}

	writer->printf("Vehicle Line: ");
	if(soul->m_vin[3]=='J')
		{
		writer->printf("Soul\n");
		}
	else 	if(soul->m_vin[3]=='C')
		{
		writer->printf("Niro\n");
		}
	else 	if(soul->m_vin[3]=='K')
		{
		writer->printf("Kona\n"); // from Peters car
		}
	else
		{
		writer->printf("Unknown %c\n", soul->m_vin[3]);
		}

	writer->printf("Model: ");
	// 5 = Peters Hyundai Kona
	/*if(soul->m_vin[4]=='M')
		{
		writer->printf("Low grade\n");
		}
	else if(soul->m_vin[4]=='N')
		{
		writer->printf("Middle-low grade\n");
		}
	else if(soul->m_vin[4]=='P')
		{
		writer->printf("Middle grade\n");
		}
	else if(soul->m_vin[4]=='R')
		{
		writer->printf("Middle-high grade\n");
		}
	else if(soul->m_vin[4]=='X')
		{
		writer->printf("High grade\n");
		}
	else*/ if(soul->m_vin[4]=='C')
		{
		writer->printf("High grade.\n");
		}
	else
		{
		writer->printf("Unknown %c\n", soul->m_vin[4]);
		}

	//
	// See https://en.wikibooks.org/wiki/Vehicle_Identification_Numbers_(VIN_codes)/Hyundai/VIN_Codes#Model_Year
	//
	int year = soul->m_vin[9]-'F';
	writer->printf("Model year: %04d\n", 2014 + year); //TODO Is off by a year. O and Q are not used.

	writer->printf("Motor type: ");
	if(soul->m_vin[7]=='E')
		{
		writer->printf("Battery [LiPB 350 V, 75 Ah (39kWh)] + Motor [3-phase AC 80 KW]\n");
		}
	else if(soul->m_vin[7]=='G')
		{
		writer->printf("Battery [LiPB 356 V, 180 Ah (64kWh)] + Motor [3-phase AC 150 KW]\n");
		}
	else
		{
		writer->printf("Unknown %c\n", soul->m_vin[7]);
		}

	//
	// See https://en.wikibooks.org/wiki/Vehicle_Identification_Numbers_(VIN_codes)/Hyundai/VIN_Codes#Manufacturing_Plant_Code
	//
	writer->printf("Production plant: ");
	if(soul->m_vin[10]=='5')
		{
		writer->printf("Hwaseong (Korea)\n");
		}
	else if(soul->m_vin[10]=='6')
		{
		writer->printf("Soha-ri (Korea)\n");
		}
	else if(soul->m_vin[10]=='7')
		{
		writer->printf("Gwangju (Korea)\n");
		}
	else if(soul->m_vin[10]=='T')
		{
		writer->printf("Seosan (Korea)\n");
		}
	else if(soul->m_vin[10]=='Y')
		{
		writer->printf("Yangon (Myanmar)\n");
		}
	else if(soul->m_vin[10]=='U')
		{
		writer->printf("Ulsan (Korea)\n");
		}
	else
		{
		writer->printf("Unknown %c\n", soul->m_vin[10]);
		}

	writer->printf("Sequence number: %c%c%c%c%c%c\n", soul->m_vin[11],soul->m_vin[12],soul->m_vin[13],soul->m_vin[14],soul->m_vin[15],soul->m_vin[16]);
	}


/**
 * Print out information of the tpms.
 */
void xkn_tpms(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
	if (MyVehicleFactory.m_currentvehicle==NULL)
		{
		writer->puts("Error: No vehicle module selected");
		return;
		}

	OvmsVehicleKiaNiroEv* car = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();

	writer->printf("TPMS\n");
	// Front left
	if (StdMetrics.ms_v_tpms_pressure->IsDefined())
		{
		const std::string& fl_pressure = StdMetrics.ms_v_tpms_pressure->ElemAsUnitString(MS_V_TPMS_IDX_FL, "-", kPa, 1);
		const std::string& fl_temp = StdMetrics.ms_v_tpms_temp->ElemAsUnitString(MS_V_TPMS_IDX_FL, "-", Celcius, 1);
		writer->printf("1 ID:%u %s %s\n", car->kia_tpms_id[0], fl_pressure.c_str(), fl_temp.c_str());
		}
	// Front right
	if (StdMetrics.ms_v_tpms_pressure->IsDefined())
		{
		const std::string& fr_pressure = StdMetrics.ms_v_tpms_pressure->ElemAsUnitString(MS_V_TPMS_IDX_FR, "-", kPa, 1);
		const std::string& fr_temp = StdMetrics.ms_v_tpms_temp->ElemAsUnitString(MS_V_TPMS_IDX_FR, "-", Celcius, 1);
		writer->printf("2 ID:%u %s %s\n",car->kia_tpms_id[1], fr_pressure.c_str(), fr_temp.c_str());
		}
	// Rear left
	if (StdMetrics.ms_v_tpms_pressure->IsDefined())
		{
		const std::string& rl_pressure = StdMetrics.ms_v_tpms_pressure->ElemAsUnitString(MS_V_TPMS_IDX_RL, "-", kPa, 1);
		const std::string& rl_temp = StdMetrics.ms_v_tpms_temp->ElemAsUnitString(MS_V_TPMS_IDX_RL, "-", Celcius, 1);
		writer->printf("3 ID:%u %s %s\n",car->kia_tpms_id[2], rl_pressure.c_str(), rl_temp.c_str());
		}
	// Rear right
	if (StdMetrics.ms_v_tpms_pressure->IsDefined())
		{
		const std::string& rr_pressure = StdMetrics.ms_v_tpms_pressure->ElemAsUnitString(MS_V_TPMS_IDX_RR, "-", kPa, 1);
		const std::string& rr_temp = StdMetrics.ms_v_tpms_temp->ElemAsUnitString(MS_V_TPMS_IDX_RR, "-", Celcius, 1);
		writer->printf("4 ID:%u %s %s\n",car->kia_tpms_id[3], rr_pressure.c_str(), rr_temp.c_str());
		}
	}


/**
 * Print out information of the current trip.
 */
void xkn_trip_since_parked(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
	if (MyVehicleFactory.m_currentvehicle==NULL)
		{
		writer->puts("Error: No vehicle module selected");
		return;
		}

	metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

	writer->printf("TRIP\n");

	// Consumption
	float consumption = StdMetrics.ms_v_bat_energy_used->AsFloat(kWh) * 100 / StdMetrics.ms_v_pos_trip->AsFloat(rangeUnit);
	float consumption2 = StdMetrics.ms_v_pos_trip->AsFloat(rangeUnit) / StdMetrics.ms_v_bat_energy_used->AsFloat(kWh);
	// Total consumption
	float totalConsumption = StdMetrics.ms_v_bat_energy_used->AsFloat(kWh) + StdMetrics.ms_v_bat_energy_recd->AsFloat(kWh);

	// Trip distance
	if (StdMetrics.ms_v_pos_trip->IsDefined())
		{
		const std::string& distance = StdMetrics.ms_v_pos_trip->AsUnitString("-", rangeUnit, 1);
		writer->printf("Dist %s\n", distance.c_str());
		}

	if(MyConfig.GetParamValue("vehicle", "units.distance") == "M")
		{
		writer->printf("Cons %.*fkWh/100mi\n", 2, consumption);
		writer->printf("Cons %.*fmi/kWh\n", 2, consumption2);
		}
	else
		{
		writer->printf("Cons %.*fkWh/100km\n", 2, consumption);
		writer->printf("Cons %.*fkm/kWh\n", 2, consumption2);
		}

	// Discharge
	if (StdMetrics.ms_v_bat_energy_used->IsDefined())
		{
		const std::string& discharge = StdMetrics.ms_v_bat_energy_used->AsUnitString("-", kWh, 1);
		writer->printf("Disc %s\n", discharge.c_str());
		}

	// Recuperation
	if (StdMetrics.ms_v_bat_energy_recd->IsDefined())
		{
		const std::string& recuparation = StdMetrics.ms_v_bat_energy_recd->AsUnitString("-", kWh, 1);
		writer->printf("Rec %s\n", recuparation.c_str());
		}

	writer->printf("Tot %.*fkWh\n", 2, totalConsumption);

	// ODO
	if (StdMetrics.ms_v_pos_odometer->IsDefined())
		{
		const std::string& ODO = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1);
		writer->printf("ODO %s\n", ODO.c_str());
		}
	}

/**
 * Print out information of the current trip.
 */
void xkn_trip_since_charge(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
	if (MyVehicleFactory.m_currentvehicle==NULL)
		{
		writer->puts("Error: No vehicle module selected");
		return;
		}

	metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

	OvmsVehicleKiaNiroEv* niro = (OvmsVehicleKiaNiroEv*) MyVehicleFactory.ActiveVehicle();

	writer->printf("TRIP SINCE CHARGE\n");

	// Consumption
	float consumption = niro->ms_v_trip_energy_used->AsFloat(kWh) * 100 / niro->ms_v_pos_trip->AsFloat(rangeUnit);
	float consumption2 = niro->ms_v_pos_trip->AsFloat(rangeUnit) / niro->ms_v_trip_energy_used->AsFloat(kWh);
	// Total consumption
	float totalConsumption = niro->ms_v_trip_energy_used->AsFloat(kWh) + niro->ms_v_trip_energy_recd->AsFloat(kWh);

	// Trip distance
	if (niro->ms_v_pos_trip->IsDefined())
		{
		const std::string& distance = niro->ms_v_pos_trip->AsUnitString("-", rangeUnit, 1);
		writer->printf("Dist %s\n", distance.c_str());
		}

	if(MyConfig.GetParamValue("vehicle", "units.distance") == "M")
		{
		writer->printf("Cons %.*fkWh/100mi\n", 2, consumption);
		writer->printf("Cons %.*fmi/kWh\n", 2, consumption2);
		}
	else
		{
		writer->printf("Cons %.*fkWh/100km\n", 2, consumption);
		writer->printf("Cons %.*fkm/kWh\n", 2, consumption2);
		}

	// Discharge
	if (niro->ms_v_trip_energy_used->IsDefined())
		{
		const std::string& discharge = niro->ms_v_trip_energy_used->AsUnitString("-", kWh, 1);
		writer->printf("Disc %s\n", discharge.c_str());
		}

	// Recuperation
	if (niro->ms_v_trip_energy_recd->IsDefined())
		{
		const std::string& recuparation = niro->ms_v_trip_energy_recd->AsUnitString("-", kWh, 1);
		writer->printf("Rec %s\n", recuparation.c_str());
		}

	writer->printf("Tot %.*fkWh\n", 2, totalConsumption);

	// ODO
	if (StdMetrics.ms_v_pos_odometer->IsDefined())
		{
		const std::string& ODO = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1);
		writer->printf("ODO %s\n", ODO.c_str());
		}
	}
