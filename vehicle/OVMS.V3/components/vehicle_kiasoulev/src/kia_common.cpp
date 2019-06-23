/*
 * ks_trip_counter.cpp
 *
 *  Created on: 4. mar. 2019
 *      Author: goev
 */
#include <fstream>
#include <sdkconfig.h>
#include "ovms_webserver.h"
#include "kia_common.h"

int KiaVehicle::CalcRemainingChargeMinutes(float chargespeed, int fromSoc, int toSoc, int batterySize, charging_profile charge_steps[])
{
       int minutes = 0, percents_In_Step;
       for (int i = 0; charge_steps[i].toPercent != 0; i++) {
             if (charge_steps[i].toPercent > fromSoc && charge_steps[i].fromPercent<toSoc) {
                    percents_In_Step = (charge_steps[i].toPercent>toSoc ? toSoc : charge_steps[i].toPercent) - (charge_steps[i].fromPercent<fromSoc ? fromSoc : charge_steps[i].fromPercent);
                    minutes += batterySize * percents_In_Step * 0.6 / (chargespeed<charge_steps[i].maxChargeSpeed ? chargespeed : charge_steps[i].maxChargeSpeed);
             }
       }
       return minutes;
}

/*
 * A crude voltage to SOC calculator for AUX-battery
 */
int KiaVehicle::CalcAUXSoc(float volt)
	{
	int soc=0;
	if( volt>=12.73)
		{
		soc=100;
		}
	else if(volt>=12.62)
		{
		soc=((volt-12.62)*90.0)+90;
		}
	else if(volt>=12.50)
		{
		soc=((volt-12.50)*83.33)+80;
		}
	else if(volt>=12.37)
		{
		soc=((volt-12.37)*76.92)+70;
		}
	else if(volt>=12.24)
		{
		soc=((volt-12.24)*76.92)+60;
		}
	else if(volt>=12.10)
		{
		soc=((volt-12.10)*71.42)+50;
		}
	else if(volt>=11.96)
		{
		soc=((volt-11.96)*71.42)+40;
		}
	else if(volt>=11.81)
		{
		soc=((volt-11.81)*66.67)+30;
		}
	else if(volt>=11.66)
		{
		soc=((volt-11.66)*66.67)+20;
		}
	else if(volt>=11.51)
		{
		soc=((volt-11.51)*66.67)+10;
		}
	else if (volt>=11.36)
		{
		soc=((volt-11.36)*66.67);
		}
	return soc;
	}

void KiaVehicle::SaveStatus()
	{
	FILE *sf = NULL;
	sf = fopen(SAVE_STATUS_DATA_PATH, "w");
	if (sf == NULL)
		{
		return;
		}

	// Set standard metrics automatically
	kia_save_status.soc = StdMetrics.ms_v_bat_soc->AsFloat(0);

	fwrite(&kia_save_status, sizeof(Kia_Save_Status), 1, sf);
	fclose(sf);
	}

void KiaVehicle::RestoreStatus()
	{
	FILE *sf = NULL;
	sf = fopen(SAVE_STATUS_DATA_PATH, "w");
	if (sf == NULL)
		{
		return;
		}
	fread(&kia_save_status, sizeof(Kia_Save_Status), 1, sf);
	fclose(sf);

	//Set the standard metrics automatically
	StdMetrics.ms_v_bat_soc->SetValue( kia_save_status.soc );

	return;
	}

/*
 * Save the current aux voltage to the history file.
 */
void KiaVehicle::Save12VHistory()
	{
	// Save Aux voltage to history file
	uint64_t timeStamp = StdMetrics.ms_m_timeutc->AsInt()*(uint64_t)1000;
	if(StdMetrics.ms_v_bat_12v_voltage->AsFloat()>0 && timeStamp>0)
		{
		float auxVoltage = StdMetrics.ms_v_bat_12v_voltage->AsFloat();

		FILE *sf = NULL;
		sf = fopen(AUX_VOLTAGE_HISTORY_DATA_PATH, "a");
		if (sf == NULL)
			{
			return;
			}
		fprintf(sf, "[%llu,%.2f],",timeStamp, auxVoltage);
		fclose(sf);
		}
	}

#ifdef CONFIG_OVMS_COMP_WEBSERVER

/**
 * WebAuxBattery: Show AUX voltage
 */
void KiaVehicle::WebAuxBattery(PageEntry_t& p, PageContext_t& c)
{
  std::string error;

  KiaVehicle* kia = (KiaVehicle*) MyVehicleFactory.ActiveVehicle();

  c.head(200);

  PAGE_HOOK("body.pre");
  c.panel_start("primary", "AUX battery status");
  c.printf("Voltage: <b>%.2f</b>V - Current: <b>%.2f</b>A - SOC: <b>%i</b>%%<br>", StdMetrics.ms_v_bat_12v_voltage->AsFloat(), StdMetrics.ms_v_bat_12v_current->AsFloat(), kia->m_b_aux_soc->AsInt() );
  c.panel_end();
  c.panel_start("primary", "AUX battery monitor");
  c.print("<script >"
  			"	$.ajax({url: '12VHistory.dat', dataType: 'text', success: function(dataFile){"
  			"{"
			"dataFile = '['+dataFile.substring(0,dataFile.length-1)+']';"
		  "data = JSON.parse(dataFile);"
			"Highcharts.chart('auxVoltageGraphDiv', {"
			"	chart: {"
			"		zoomType: 'x',"
			"		panning: true,"
			"		panKey: 'ctrl'"
			"},"
			"	title: {text: 'AUX Battery Voltage over time'},"
			"	credits: { enabled: false },"
			"	subtitle: {text: document.ontouchstart === undefined ? 'Click and drag in the plot area to zoom in. Time is in UTC' : 'Pinch the chart to zoom in. Time is in UTC'},"
			"	xAxis: { type: 'datetime' },"
			"	yAxis: { title: {text: 'Voltage'}},"
			"	series: [{type: 'line', name: 'Voltage', data: data}]"
			"});"
		  "}"
  		  "}});"
      "if (window.Highcharts) {\n"
        "init_charts();\n"
      "} else {\n"
        "$.ajax({\n"
          "url: \"" URL_ASSETS_CHARTS_JS "\",\n"
          "dataType: \"script\",\n"
          "cache: true,\n"
          "success: function(){ init_charts(); }\n"
        "});\n"
      "}\n"
			"</script>"
  			"<div id='auxVoltageGraphDiv' style='min-width: 110px; height: 400px; margin: 0 auto'></div>"
  );
  c.panel_end();
  c.done();
}

#endif //CONFIG_OVMS_COMP_WEBSERVER


Kia_Trip_Counter::Kia_Trip_Counter()
	{
	odo_start=0;
	cdc_start=0;
	cc_start=0;
	odo=0;
	cdc=0;
	cc=0;
	}

Kia_Trip_Counter::~Kia_Trip_Counter()
	{
	}

/*
 * Resets the trip counter.
 *
 * odo - The current ODO
 * cdc - Cumulated Discharge
 * cc - Cumulated Charge
 */
void Kia_Trip_Counter::Reset(float odo, float cdc, float cc)
	{
	Update(odo, cdc, cc);
	if(odo_start==0 && odo!=0)
		odo_start = odo;		// ODO at Start of trip
	if(cdc_start==0 && cdc!=0)
	  	cdc_start = cdc; 	// Register Cumulated discharge
	if(cc_start==0 && cc!=0)
	  	cc_start = cc; 		// Register Cumulated charge
	}

/*
 * Update the trip counter with current ODO, accumulated discharge and accumulated charge..
 *
 * odo - The current ODO
 * cdc - Accumulated Discharge
 * cc - Accumulated Charge
 */
void Kia_Trip_Counter::Update(float current_odo, float current_cdc, float current_cc)
	{
	odo=current_odo;
	cdc=current_cdc;
	cc=current_cc;
	}

/*
 * Returns true if the trip counter has been initialized properly.
 */
bool Kia_Trip_Counter::Started()
	{
	return odo_start!=0;
	}

/*
 * Returns true if the trip counter have energy data.
 */
bool Kia_Trip_Counter::HasEnergyData()
	{
	return cdc_start!=0 && cc_start!=0;
	}

float Kia_Trip_Counter::GetDistance()
	{
	if( Started())
		return odo-odo_start;
	return 0;
	}

float Kia_Trip_Counter::GetEnergyUsed()
	{
	if( HasEnergyData())
		return (cdc-cdc_start) - (cc-cc_start);
	return 0;
	}

float Kia_Trip_Counter::GetEnergyRecuperated()
	{
	if( HasEnergyData())
		return cc - cc_start;
	return 0;
	}

/*
 * Constructor for the range calculator
 * int minimumTrip  - The minium length a trip has to be in order to be included in the future calculations.
 * float weightOfCurrentTrip - How much the current trip should be weighted compared to the others. Should be 1 or higher
 * float defaultRange - A default start range in km with 100% battery. WLTP.
 * float batteryCapacity - Battery capacity in kWh
 */
RangeCalculator::RangeCalculator(float minimumTrip, float weightOfCurrentTrip, float defaultRange, float batteryCapacity)
	{
	this->minimumTrip = minimumTrip;
	this->weightOfCurrentTrip = weightOfCurrentTrip;
	this->batteryCapacity = batteryCapacity;
	for (int i = 0; i < 20; i++)
		{
		trips[i].distance=defaultRange;
		trips[i].consumption=batteryCapacity;
		}

	restoreTrips();
	}

RangeCalculator::~RangeCalculator()
	{
  storeTrips();
	}

/*
 * Called whenever the car gets parked and the trip
 * is longer than specified in minimumTrip
 */
void RangeCalculator::storeTrips()
	{
	FILE *sf = NULL;
	sf = fopen(RANGE_CALC_DATA_PATH, "w");
	if (sf == NULL)
		{
		return;
		}
	fwrite(&currentTripPointer, sizeof(int), 1, sf);
	fwrite(trips, sizeof(TripConsumption), 20, sf);
	fclose(sf);
	}

/*
 * Restores the saved trips history used for calculations
 */
void RangeCalculator::restoreTrips()
	{
	FILE *sf = NULL;
	sf = fopen(RANGE_CALC_DATA_PATH, "r");
	if (sf == NULL)
		{
		return;
		}
	fread(&currentTripPointer, sizeof(int), 1, sf);
	fread(trips, sizeof(TripConsumption), 20, sf);
	fclose(sf);
	if (currentTripPointer >= 20 || currentTripPointer<0) currentTripPointer = 0;
	}

/*
 * Updates the internal current trip
 */
void RangeCalculator::updateTrip(float distance, float consumption)
	{
	trips[currentTripPointer].consumption = consumption;
  trips[currentTripPointer].distance = distance;
	}

/*
 * Stores the current trip and rotates the pointer so that the oldest trip
 * will be over written next.
 *
 * Should be called when the car gets parked.
 * Note that only trips longer than specified in minimumTrip get stored.
 */
void RangeCalculator::tripEnded(float distance, float consumption)
	{
	updateTrip(distance, consumption);
	if (distance > minimumTrip)
		{
		currentTripPointer++;
		if (currentTripPointer >= 20)
			{
			currentTripPointer = 0;
			}
		storeTrips();
		}
	}

/*
 * Returns the calculated full range based on driving history
 */
float RangeCalculator::getRange()
	{
	float totalDistance = 0, totalConsumption = 0;
	for (int i = 0; i < 20; i++)
		{
		totalDistance += trips[i].distance;
		totalConsumption += trips[i].consumption;
		//TODO ESP_LOGI("v-kianiroev", "%i %.2f km %.2f kWh",i, trips[i].distance, trips[i].consumption);
		}

	// Make current trip count more than the rest
	totalDistance += trips[currentTripPointer].distance * (weightOfCurrentTrip - 1);
	totalConsumption += trips[currentTripPointer].consumption * (weightOfCurrentTrip - 1);

	float averageConsumption = totalConsumption / totalDistance;

	return batteryCapacity / averageConsumption;
	}



