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
 * Command to reset battery min / max
 */
void CommandBatteryReset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleMitsubishi* trio = (OvmsVehicleMitsubishi*) MyVehicleFactory.ActiveVehicle();
	trio->BatteryReset();
	}

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
* Print out the cell voltages.
*/
void xmi_bms(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
	float tmin = 1000;
  float tmax = -1000;
  float vmin = 1000;
  float vmax = 0;
  float vtot = 0;

	OvmsVehicleMitsubishi* trio = (OvmsVehicleMitsubishi*) MyVehicleFactory.ActiveVehicle();

	if (MyVehicleFactory.m_currentvehicle==NULL)
		{
			writer->puts("Error: No vehicle module selected");
		  return;
		}

	if (trio->cell_volts_act == 0)
		{
			writer->puts("No BMS status data available");
		  return;
		}

		for (int i=0;i<66;i++)
		    {
		    if (trio->cell_temps_act->GetElemValue(i)<tmin) tmin = trio->cell_temps_act->GetElemValue(i);
		    if (trio->cell_temps_act->GetElemValue(i)>tmax) tmax = trio->cell_temps_act->GetElemValue(i);
		    }

		for (int j=0;j<88;j++)
		    {
		    if (trio->cell_volts_act->GetElemValue(j)<vmin) vmin = trio->cell_volts_act->GetElemValue(j);
		    if (trio->cell_volts_act->GetElemValue(j)>vmax) vmax = trio->cell_volts_act->GetElemValue(j);
				vtot += trio->cell_volts_act->GetElemValue(j);
				}


		writer->puts("   Mitsubishi iMiew, Citroen C-Zero, Peugeot iOn BMS status");
  	writer->puts("   -------------------------------");
		for (int cmu=1;cmu<12;cmu++ )
		{
			writer->printf("%2d |",cmu);
			for (int cell=1;cell<=8;cell++)
			 {
				 if( cell < 4)
				 {
				 	writer->printf(" %4.3f V |",trio->cell_volts_act->GetElemValue(cmu*cell));
			 	 	writer->printf(" %3.2f C |",trio->cell_temps_act->GetElemValue(cmu*cell));
				}else if(cell > 3 && cell < 5){
					writer->printf(" %4.3f V |",trio->cell_volts_act->GetElemValue(cmu*cell));
				}else if( cell > 4 && cell < 8){
 				 	writer->printf(" %4.3f V |",trio->cell_volts_act->GetElemValue(cmu*cell));
 			 	 	writer->printf(" %3.2f C |",trio->cell_temps_act->GetElemValue(cmu*cell-1));
				}else {
					writer->printf(" %4.3f V \n",trio->cell_volts_act->GetElemValue(cmu*cell));
 					writer->puts("   -------------------------------");
				}

			 }
		}

		writer->printf("   Tmin: %3.1f  Tmax: %3.1f  Vmax: %4.3f  Vmin: %4.3f  Vmax-Vmin: %4.3f  Vtot: %5.2f\n",
    tmin, tmax, vmax, vmin, vmax-vmin, vtot);
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
			  // Consumption
			  float consumption = StdMetrics.ms_v_bat_energy_used->AsFloat(kWh) * 100 / StdMetrics.ms_v_pos_trip->AsFloat(rangeUnit);
			  float consumption2 = StdMetrics.ms_v_pos_trip->AsFloat(rangeUnit) / StdMetrics.ms_v_bat_energy_used->AsFloat(kWh);
			  // Discharge
			  const char* discharge = StdMetrics.ms_v_bat_energy_used->AsUnitString("-", kWh, 1).c_str();
			  // Recuperation
			  const char* recuparation = StdMetrics.ms_v_bat_energy_recd->AsUnitString("-", kWh, 1).c_str();
			  // Total consumption
			  float totalConsumption = StdMetrics.ms_v_bat_energy_used->AsFloat(kWh) + StdMetrics.ms_v_bat_energy_recd->AsFloat(kWh);
			  // ODO
			  const char* ODO = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();

			  if (*distance != '-')
			    writer->printf("Dist %s\n", distance);

			  if(MyConfig.GetParamValue("vehicle", "units.distance") == "M")
			  		{
			    writer->printf("Con %.*fkWh/100mi\n", 2, consumption);
			    writer->printf("Con %.*fmi/kWh\n", 2, consumption2);
			  		}
			  else
			  		{
			    writer->printf("Con %.*fkWh/100km\n", 2, consumption);
			    writer->printf("Con %.*fkm/kWh\n", 2, consumption2);
			  		}

			  if (*discharge != '-')
			    writer->printf("Dis %s\n", discharge);

			  if (*recuparation != '-')
			    writer->printf("Rec %s\n", recuparation);

			  writer->printf("Total %.*fkWh\n", 2, totalConsumption);

			  if (*ODO != '-')
			    writer->printf("ODO %s\n", ODO);
			  }
