/*
 * ks_trip_counter.cpp
 *
 *  Created on: 4. mar. 2019
 *      Author: goev
 */
#include "kia_common.h"

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
	return odo-odo_start;
	}

float Kia_Trip_Counter::GetEnergyUsed()
	{
	return (cdc-cdc_start) - (cc-cc_start);
	}

float Kia_Trip_Counter::GetEnergyRecuperated()
	{
	return cc - cc_start;
	}




