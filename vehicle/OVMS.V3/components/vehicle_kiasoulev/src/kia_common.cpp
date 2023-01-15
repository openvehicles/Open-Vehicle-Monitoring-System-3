/*
 * ks_trip_counter.cpp
 *
 *  Created on: 4. mar. 2019
 *      Author: goev
 */
#include <fstream>
#include <sdkconfig.h>
#include "ovms_webserver.h"
#include "ovms_peripherals.h"
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
	odo_start              = 0;
	tot_discharge_start    = 0;
	tot_charge_start       = 0;
	odo                    = 0;
	tot_discharge          = 0;
	tot_charge             = 0;
	tot_charge_ext         = 0;
	charging               = false;
	charge_start           = 0;

	tot_discharge_ah_start = 0;
	tot_charge_ah_start    = 0;
	tot_discharge_ah       = 0;
	tot_charge_ah          = 0;
	tot_charge_ah_ext      = 0;
	charge_start_ah        = 0;
	}

Kia_Trip_Counter::~Kia_Trip_Counter()
	{
	}

/** Resets the trip counter.
 *
 * \param current_odo The current ODO
 * \param current_cdc Accumulated Energy Discharged
 * \param current_cc Accumulated Energy Charged
 * \param current_cdc_ah Accumulated 'Charge' Discharged
 * \param current_cc_ah Accumulated 'Charge' added.
 */
void Kia_Trip_Counter::Reset(float current_odo, float current_cdc, float current_cc, float current_cdc_ah, float current_cc_ah, bool is_charging)
	{
	tot_charge_ext = 0;
	tot_charge_ah_ext = 0;
	// ODO at Start of trip
	odo_start = odo = current_odo;
	// Register Cumulated discharge
	tot_discharge_start = tot_discharge = current_cdc;
	// Register Cumulated charge
	tot_charge_start = tot_charge = current_cc;
	// Register Cumulated discharge
	tot_discharge_ah_start = tot_discharge_ah = current_cdc_ah;
	// Register Cumulated charge
	tot_charge_ah_start = tot_charge_ah = current_cc_ah;

	// Register charing.
	charging = is_charging;
	if (is_charging)
		{
		charge_start = current_cc;
		charge_start_ah = current_cc_ah;
		}
	else
		{
		charge_start = 0;
		charge_start_ah = 0;
		}
	}

/**
 * Update the trip counter with current ODO, accumulated discharge and accumulated charge..
 *
 * \param current_odo The current ODO
 * \param current_cdc Accumulated Energy Discharged
 * \param current_current_cc Accumulated Energy Charged
 * \param current_cdc_ah Accumulated 'Charge' Discharged
 * \param current_current_cc_ah Accumulated 'Charge' added.
 */
void Kia_Trip_Counter::Update(float current_odo, float current_cdc, float current_cc, float current_cdc_ah, float current_cc_ah)
	{
	odo=current_odo;
	tot_discharge=current_cdc;
	tot_charge=current_cc;
	tot_charge_ah = current_cc_ah;
	tot_discharge_ah = current_cdc_ah;
	}

/**
 * Returns true if the trip counter has been initialized properly.
 */
bool Kia_Trip_Counter::Started()
	{
	return odo_start!=0;
	}

/**
 * Returns true if the trip counter has energy data.
 */
bool Kia_Trip_Counter::HasEnergyData()
	{
	return tot_discharge_start!=0 && tot_charge_start!=0;
	}
/**
 * Returns true if the trip counter has charge data.
 */
bool Kia_Trip_Counter::HasChargeData()
	{
	return tot_discharge_ah_start!=0 && tot_charge_ah_start!=0;
	}

/** Returns the distance travelled.
  */
float Kia_Trip_Counter::GetDistance()
	{
	if( Started())
		return odo-odo_start;
	return 0;
	}

/** Gets the Balance of Energy Used in kWh.
 * Energy Discharged - Energy Recuperated.
 */
float Kia_Trip_Counter::GetEnergyUsed()
	{
	return GetEnergyConsumed() - GetEnergyRecuperated();
	}

float Kia_Trip_Counter::GetEnergyConsumed()
	{
	float res = 0;
	if( HasEnergyData())
		res = (tot_discharge-tot_discharge_start);
	return res;
	}

/** Gets the Energy Recuperated while moving in kWhh.
  * Totally Energy Charged less Energy Added in Charging.
  */
float Kia_Trip_Counter::GetEnergyRecuperated()
	{
	float res = 0;
	if( HasEnergyData())
		{
		res = (tot_charge - tot_charge_start);
		res -= GetEnergyCharged();
		}
	return res;
	}
/** Gets the amount of energy input via a charger.
  */
float Kia_Trip_Counter::GetEnergyCharged()
	{
	float res = 0;
	if( HasEnergyData())
		{
		res = tot_charge_ext;
		if (charging)
			res += (tot_charge - charge_start);
		}
	return res;
	}

/** Gets the Balance of Charge Used in Ah.
  */
float Kia_Trip_Counter::GetChargeUsed()
	{
	return GetChargeConsumed() - GetChargeRecuperated();
	}

/** Gets the total charge consumed in Ah.
*/
float Kia_Trip_Counter::GetChargeConsumed()
	{{
	float res = 0;
	if( HasChargeData())
		res = (tot_discharge_ah-tot_discharge_ah_start) ;
	return res;
	}
	}
/** Gets the Charge Recuperated in Ah.
  */
float Kia_Trip_Counter::GetChargeRecuperated()
	{
	float res = 0;
	if( HasChargeData())
		{
		res = (tot_charge_ah - tot_charge_ah_start);
		res -= GetChargeCharged();
		}
	return res;
	}

/** Gets the amount of energy input via a charger.
  */
float Kia_Trip_Counter::GetChargeCharged()
	{
	float res = 0;
	if( HasChargeData())
		{
		res = tot_charge_ah_ext;
		if (charging)
			res += (tot_charge_ah - charge_start_ah);
		}
	return res;
	}

/** Set start of charging.
  */
void Kia_Trip_Counter::StartCharge(float current_cc, float current_cc_ah)
	{

	if (!charging && ((tot_discharge_start != 0) || (tot_charge_start != 0)))
		{
		charging = true;
		charge_start = current_cc;
		tot_charge = current_cc;
		charge_start_ah = current_cc_ah;
		tot_charge_ah = current_cc_ah;
		tot_charge_ext = 0;
		tot_charge_ah_ext = 0;
		}
	}

/** Set finish of charging.
  */
void Kia_Trip_Counter::FinishCharge(float current_cc, float current_cc_ah)
	{
	if (charging)
		{
		charging = false;

		if (charge_start > 0) {
			float diff = current_cc-charge_start;
			if (diff > 0)
				tot_charge_ext += diff;
		}
		charge_start = 0;
		tot_charge = current_cc;

		if (charge_start_ah > 0) {
			float diff = current_cc_ah-charge_start_ah;
			if (diff > 0)
				tot_charge_ah_ext += diff;
		}
		charge_start_ah = 0;
		tot_charge_ah = current_cc_ah;
		}
	}

static const char *CALCTAG = "rangecalc";
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
	this->defaultRange = defaultRange;

	clearTrips();
	restoreTrips();
#ifdef bind
	#undef bind  // Kludgy, but works
#endif
	using std::placeholders::_1;
	using std::placeholders::_2;
	MyEvents.RegisterEvent(CALCTAG, "sd.mounted",
			std::bind(&RangeCalculator::SDCardMountListener, this, _1, _2));
	}

RangeCalculator::~RangeCalculator()
	{
  MyEvents.DeregisterEvent(CALCTAG);
	storeTrips();
	}

/*
 * Called whenever the car gets parked and the trip
 * is longer than specified in minimumTrip
 */
void RangeCalculator::storeTrips()
	{
#ifdef CONFIG_OVMS_COMP_SDCARD
	if (!storeToFile)
		return;
	sdcard *sd = MyPeripherals->m_sdcard;
	if ( sd == NULL || !sd->isinserted() || (!sd->ismounted()))
		return;
	FILE *sf = NULL;
	sf = fopen(RANGE_CALC_DATA_PATH, "w");
	if (sf == NULL)
		{
		return;
		}
	fwrite(&currentTripPointer, sizeof(int), 1, sf);
	fwrite(trips, sizeof(TripConsumption), 20, sf);
	fclose(sf);
#endif
	}

/*
 * Restores the saved trips history used for calculations
 */
void RangeCalculator::restoreTrips()
	{
#ifdef CONFIG_OVMS_COMP_SDCARD
	sdcard *sd = MyPeripherals->m_sdcard;
	if ( sd == NULL || !sd->isinserted() || (!sd->ismounted()))
		return;
	// At least we tried.
	storeToFile = true;
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
#endif
	}

void RangeCalculator::updateCapacity(float capacity)
	{
	if (capacity != batteryCapacity)
		{
		float oldCap = batteryCapacity;
		batteryCapacity = capacity;
		for (int i = 0; i < 20; i++)
			{
			TripConsumption &entry = trips[i];
			if (entry.distance == defaultRange && entry.consumption == oldCap)
				entry.consumption = capacity;
			}
		}
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

 /// Efficiency In km/kWh
float RangeCalculator::getEfficiency()
	{
	float totalDistance = 0, totalConsumption = 0;
	for (int i = 0; i < 20; i++)
		{
		totalDistance += trips[i].distance;
		totalConsumption += trips[i].consumption;
		}

	// Make current trip count more than the rest
	totalDistance += trips[currentTripPointer].distance * (weightOfCurrentTrip - 1);
	totalConsumption += trips[currentTripPointer].consumption * (weightOfCurrentTrip - 1);

	return totalDistance / totalConsumption ;
	}

/**
 * Returns the calculated full range based on driving history
 */
float RangeCalculator::getRange()
	{
	return batteryCapacity * getEfficiency();
	}
/**
 * Reset the trips used for the range calculator.
 * Stores to the file.
 */
void RangeCalculator::resetTrips()
	{
	clearTrips();
	currentTripPointer = 0;
	TripConsumption &entry = trips[0];
	entry.distance = 0;
	entry.consumption = 0;
	storeToFile = true;
	storeTrips();
	}
/**
 * Initialise the trips to the default.
 */
void RangeCalculator::clearTrips()
	{
	for (int i = 0; i < 20; ++i)
		{
		TripConsumption &entry = trips[i];
		entry.distance = defaultRange;
		entry.consumption = batteryCapacity;
		}
	storeToFile = false;
	}

void RangeCalculator::SDCardMountListener(std::string event, void* data)
	{
	restoreTrips();
	}

void RangeCalculator::displayStoredTrips(OvmsWriter *writer)
	{
	writer->printf("Range Calculator%s\n", storeToFile ? "" : " (not stored)");
#ifndef CONFIG_OVMS_COMP_SDCARD
	writer->puts("SD Card Not Implemented");
#else
	sdcard *sd = MyPeripherals->m_sdcard;
	if (!sd)
		writer->puts("SD Card System Missing");
	if (!sd->isinserted())
		writer->puts("SD Card Not Inserted");
	else if (!sd->ismounted())
		writer->puts("SD Card Not Mounted");
#endif

	writer->puts("+Dist(km)+Power(kWh)+-kWh/100km+");
	// Put the 'current' one at the end (it's a circular buffer)
	for (int i= 1; i < 21; ++i)
		{
		const TripConsumption &entry = trips[(i+currentTripPointer)%20];
		float effic = UnitConvert(KPkWh, kWhP100K,
			entry.distance/entry.consumption);
		writer->printf("|%8.8g|%10.10g|%10.10g|%s\n",
			entry.distance,
			ROUNDPREC(entry.consumption,2),
			ROUNDPREC(effic,2),
			(i == 20) ? "<< current" : "");
		}
	writer->puts("+--------+----------+----------+");
	writer->printf("Battery Capacity: %gkWh\n", batteryCapacity);
	auto efficiency = UnitConvert(KPkWh, kWhP100K, getEfficiency());
	writer->printf("Efficiency: %gkWh/100km\n", ROUNDPREC(efficiency,2));
	float soc = StdMetrics.ms_v_bat_soc->AsFloat();
	auto curRange = getRange();
	writer->printf("Full Range: %gkm\n", ROUNDPREC(curRange,1));
	writer->printf("SOC: %g%%\n", soc);
	writer->printf("Range: %gkm\n", ROUNDPREC(curRange* soc / 100.0,1));
	}
