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
	  if (MyVehicleFactory.m_currentvehicle==NULL)
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
void xmi_trip(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
			  {
			  if (MyVehicleFactory.m_currentvehicle==NULL)
			    {
			    writer->puts("Error: No vehicle module selected");
			    return;
			    }

			  metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

			  writer->printf("TRIP\n");

			  // Trip distance
			  const char* distance = StdMetrics.ms_v_pos_trip->AsUnitString("-", rangeUnit, 1).c_str();
			  // Total consumption
			  float totalConsumption = StdMetrics.ms_v_bat_energy_used->AsFloat(kWh) - StdMetrics.ms_v_bat_energy_recd->AsFloat(kWh);
			  // Consumption
				float consumption = totalConsumption*100/StdMetrics.ms_v_pos_trip->AsFloat(rangeUnit);
			  float consumption2 = StdMetrics.ms_v_pos_trip->AsFloat(rangeUnit) / totalConsumption;
			  // Discharge
			  const char* discharge = StdMetrics.ms_v_bat_energy_used->AsUnitString("-", kWh, 4).c_str();
			  // Recuperation
			  const char* recuparation = StdMetrics.ms_v_bat_energy_recd->AsUnitString("-", kWh, 4).c_str();
			  // ODO
			  const char* ODO = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();
				// heating kwh
				OvmsVehicleMitsubishi* trio = (OvmsVehicleMitsubishi*) MyVehicleFactory.ActiveVehicle();
				float heatenergy = trio->m_v_env_heating_kwh->AsFloat();
				float coolingenergy = trio->m_v_env_ac_kwh->AsFloat();

			  if (*distance != '-')
			    writer->printf("Dist %s\n", distance);

			  if(MyConfig.GetParamValue("vehicle", "units.distance") == "M")
			  		{
			    writer->printf("Con %.*fkWh/100mi\n", 4, consumption);
			    writer->printf("Con %.*fmi/kWh\n", 4, consumption2);
			  		}
			  else
			  		{
			    writer->printf("Con %.*fkWh/100km\n", 4, consumption);
			    writer->printf("Con %.*fkm/kWh\n", 4, consumption2);
			  		}

			  if (*discharge != '-')
			    writer->printf("Dis %s\n", discharge);

			  if (*recuparation != '-')
			    writer->printf("Rec %s\n", recuparation);

			  writer->printf("Total %.*fkWh\n", 4, totalConsumption);
				writer->printf("Heater energy usage: %.*fkWh\n", 4,heatenergy);
				writer->printf("AC energy usage: %.*fkWh\n", 4,coolingenergy);


			  if (*ODO != '-')
			    writer->printf("ODO %s\n", ODO);
			  }
