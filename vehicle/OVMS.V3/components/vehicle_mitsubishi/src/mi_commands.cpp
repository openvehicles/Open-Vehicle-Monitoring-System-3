/*
;    Project:       Open Vehicle Monitor System
;    Date:          04 November 2018
;
;    (C) 2017       Geir Øyvind Vælidalo <geir@validalo.net>
;    (C) 2018       Tamás Kovács
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
#include "vehicle_mitsubishi.h"

/**
* Print out aux battery.
*/
void xmi_aux(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	  {
	  if (MyVehicleFactory.m_currentvehicle == NULL)
	    {
	    writer->puts("Error: No vehicle module selected");
	    return;
	    }

		const char* auxBatt = StdMetrics.ms_v_bat_12v_voltage->AsUnitString("-", Volts, 2).c_str();

		writer->printf("AUX BATTERY\n");
		if (*auxBatt != '-') writer->printf("Aux battery voltage %s\n", auxBatt);
		}

/**
* Print out information of the current trip.
*/
void xmi_trip_since_parked(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
			  if (MyVehicleFactory.m_currentvehicle == NULL)
			    {
			    writer->puts("Error: No vehicle module selected");
			    return;
			    }

				OvmsVehicleMitsubishi* trio = (OvmsVehicleMitsubishi*) MyVehicleFactory.ActiveVehicle();

			  metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

			  writer->printf("TRIP\n\n");

			  // Trip distance
			  const char* distance = trio->ms_v_pos_trip_park->AsUnitString("-", rangeUnit, 1).c_str();
			  // Total consumption
			  float totalConsumption = trio->ms_v_trip_park_energy_used->AsFloat(kWh) - trio->ms_v_trip_park_energy_recd->AsFloat(kWh);
			  // Consumption
				float consumption = totalConsumption * 100 / trio->ms_v_pos_trip_park->AsFloat(rangeUnit);
			  float consumption2 = trio->ms_v_pos_trip_park->AsFloat(rangeUnit) / totalConsumption;
			  // Discharge
			  const char* discharge = trio->ms_v_trip_park_energy_used->AsUnitString("-", kWh, 4).c_str();
			  // Recuperation
			  const char* recuparation = trio->ms_v_trip_park_energy_recd->AsUnitString("-", kWh, 4).c_str();
			  // ODO
			  const char* ODO = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();
				// heating kwh
				float heatenergy = trio->m_v_trip_park_heating_kwh->AsFloat();
				float coolingenergy = trio->m_v_trip_park_ac_kwh->AsFloat();

				float soc_start = trio->ms_v_trip_park_soc_start->AsFloat();
				float soc_stop = trio->ms_v_trip_park_soc_stop->AsFloat();

			  if (*distance != '-')
			    writer->printf("Dist %s\n", distance);

				writer->printf("Soc: %.*f%% - %.*f%% \n\n",1, soc_start,1, soc_stop);

			  if(MyConfig.GetParamValue("vehicle", "units.distance") == "M")
			  		{
			    writer->printf("Con %.*fkWh/100mi\n", 4, consumption);
			    writer->printf("Con %.*fmi/kWh\n\n", 4, consumption2);
			  		}
			  else
			  		{
			    writer->printf("Con %.*fkWh/100km\n", 4, consumption);
			    writer->printf("Con %.*fkm/kWh\n\n", 4, consumption2);
			  		}

			  if (*discharge != '-')
			    writer->printf("Dis %s\n", discharge);

			  if (*recuparation != '-')
			    writer->printf("Rec %s\n", recuparation);

			  writer->printf("Total %.*fkWh\n\n", 4, totalConsumption);
				writer->printf("Heater energy usage: %.*fkWh\n", 4,heatenergy);
				writer->printf("AC energy usage: %.*fkWh\n", 4,coolingenergy);


			  if (*ODO != '-')
			    writer->printf("ODO %s\n", ODO);
		}

/**
* Print out information of the current trip since charge.
*/
void xmi_trip_since_charge(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
				{
				if (MyVehicleFactory.m_currentvehicle == NULL)
					{
					writer->puts("Error: No vehicle module selected");
					return;
					}

				metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

				OvmsVehicleMitsubishi* trio = (OvmsVehicleMitsubishi*) MyVehicleFactory.ActiveVehicle();

				writer->printf("TRIP SINCE CHARGE\n\n");

				// Trip distance
				const char* distance = trio->ms_v_pos_trip_charge->AsUnitString("-", rangeUnit, 1).c_str();
				// Total consumption
				float totalConsumption = trio->ms_v_trip_charge_energy_used->AsFloat(kWh) - trio->ms_v_trip_charge_energy_recd->AsFloat(kWh);
				// Consumption
				float consumption = totalConsumption * 100 / trio->ms_v_pos_trip_charge->AsFloat(rangeUnit);
				float consumption2 = trio->ms_v_pos_trip_charge->AsFloat(rangeUnit) / totalConsumption;
				// Discharge
				const char* discharge = trio->ms_v_trip_charge_energy_used->AsUnitString("-", kWh, 4).c_str();
				// Recuperation
				const char* recuparation = trio->ms_v_trip_charge_energy_recd->AsUnitString("-", kWh, 4).c_str();
				// ODO
				const char* ODO = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();
				// heating kwh
				float heatenergy = trio->m_v_trip_charge_heating_kwh->AsFloat();
				float coolingenergy = trio->m_v_trip_charge_ac_kwh->AsFloat();

				float soc_start = trio->ms_v_trip_charge_soc_start->AsFloat();
				float soc_stop = trio->ms_v_trip_charge_soc_stop->AsFloat();

				if (*distance != '-')
					writer->printf("Dist: %s\n", distance);

				writer->printf("Soc: %.*f%% - %.*f%% \n\n",1, soc_start,1, soc_stop);

				if(MyConfig.GetParamValue("vehicle", "units.distance") == "M")
						{
					writer->printf("Cons: %.*fkWh/100mi\n", 4, consumption);
					writer->printf("Cons: %.*fmi/kWh\n\n", 4, consumption2);
						}
				else
						{
					writer->printf("Cons: %.*fkWh/100km\n", 4, consumption);
					writer->printf("Cons: %.*fkm/kWh\n\n", 4, consumption2);
						}

				if (*discharge != '-')
					writer->printf("Disc. %s\n", discharge);

				if (*recuparation != '-')
					writer->printf("Rec: %s\n", recuparation);

				writer->printf("Tot: %.*fkWh\n\n", 4, totalConsumption);
				writer->printf("Heater energy usage: %.*fkWh\n", 4,heatenergy);
				writer->printf("AC energy usage: %.*fkWh\n", 4,coolingenergy);

				if (*ODO != '-')
					writer->printf("ODO %s\n", ODO);
				}

void xmi_vin(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
			{
				  if (MyVehicleFactory.m_currentvehicle == NULL)
				    {
				    writer->puts("Error: No vehicle module selected");
				    return;
				    }

				  OvmsVehicleMitsubishi* trio = (OvmsVehicleMitsubishi*) MyVehicleFactory.ActiveVehicle();

					writer->printf("VIN\n\n");
					if(strlen(trio->m_vin) != 0)
					{
						writer->printf("Vin: %s \n",trio->m_vin);
						writer->printf("Car: ");
						if(trio->m_vin[0] == 'J' && trio->m_vin[1] == 'F' && trio->m_vin[2] == '3'){
							writer->printf("Mitsubishi i-MiEV \n");
						}else if (trio->m_vin[0] == 'V' && trio->m_vin[1] == 'F'){
							if (trio->m_vin[2] == '3'){
								writer->printf("Peugeot iOn");
							}else if(trio->m_vin[2] == '7'){
							writer->printf("Citroen C-Zero ");
							}
							writer->printf("\n");
						}
						writer->printf("Battery cell count: ");
						if (trio->m_vin[0] == 'V' && trio->m_vin[1] == 'F' && trio->m_vin[2] == 'Y'){
							writer->printf("80 \n");
						}else{
							writer->printf("88 \n");
						}
						writer->printf("Modell year:");
						switch (trio->m_vin[9]) {
							case 'A':
							{
								writer->printf("2010");
								break;
							}
							case 'B':
							{
								writer->printf("2011");
								break;
							}
							case 'C':
							{
								writer->printf("2012");
								break;
							}
							case 'D':
							{
								writer->printf("2013");
								break;
							}
							case 'E':
							{
								writer->printf("2014");
								break;
							}
							case 'F':
							{
								writer->printf("2015");
								break;
							}
							case 'G':
							{
								writer->printf("2016");
								break;
							}
							case 'H':
							{
								writer->printf("2017");
								break;
							}
							case 'I':
							{
								writer->printf("2018");
								break;
							}
							case 'J':
							{
								writer->printf("2019");
								break;
							}
							case 'K':
							{
								writer->printf("2020");
								break;
							}
							case 'L':
							{
								writer->printf("2021");
								break;
							}
						}
					}else{
						writer->printf("No car VIN loaded!\n");
					}
			}
/**
* Print out information of the current trip since charge.
*/
void xmi_charge(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
	if (MyVehicleFactory.m_currentvehicle == NULL)
		{
			writer->puts("Error: No vehicle module selected");
			return;
		}

		OvmsVehicleMitsubishi* trio = (OvmsVehicleMitsubishi*) MyVehicleFactory.ActiveVehicle();

		float ac_energy_usage = trio->m_v_charge_ac_kwh->AsFloat();
		float dc_energy_usage = trio->m_v_charge_dc_kwh->AsFloat();
		float efficiency = dc_energy_usage / ac_energy_usage;

		int num_seconds = trio->v_c_time->AsInt();
		int hours = num_seconds / (60 * 60);
		num_seconds -= hours * (60 * 60);
		int minutes = num_seconds / 60;
		num_seconds -= minutes * 60;

		float soc_start = trio->v_c_soc_start->AsFloat();
		float soc_stop = trio->v_c_soc_stop->AsFloat();

		writer->printf("LAST CHARGE \n\n");
		writer->printf("Soc: %.*f%% - %.*f%% \n",1, soc_start,1, soc_stop);
		//writer->printf("",1, soc_stop);
		writer->printf("Charge time: %d:%d.%d \n", hours, minutes, num_seconds);
		writer->printf("DC energy usage: %.*fkWh \n", 4, dc_energy_usage);
		if (ac_energy_usage != 0)
			{
				writer->printf("AC energy usage: %.*fkWh \n", 4, ac_energy_usage);
				writer->printf("Efficiency: %.*f%% \n",4 ,efficiency * 100);
			}

}
