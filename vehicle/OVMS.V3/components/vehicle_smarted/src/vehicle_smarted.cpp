/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
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
 */

#include "ovms_log.h"
#include "vehicle_smarted.h"

static const char *TAG = "v-smarted";

#include <stdio.h>
#include "ovms_metrics.h"
#include "ovms_peripherals.h"

#define SE_CANDATA_TIMEOUT 10

/**
 * Constructor & destructor
 */
size_t OvmsVehicleSmartED::m_modifier = 0;

OvmsVehicleSmartED::OvmsVehicleSmartED() {
    ESP_LOGI(TAG, "Start Smart ED vehicle module");

    StandardMetrics.ms_v_env_hvac->SetValue(false);
    // init metrics:
    if (m_modifier == 0) {
        m_modifier = MyMetrics.RegisterModifier();
        ESP_LOGD(TAG, "registered metric modifier is #%d", m_modifier);

        mt_displayed_soc = MyMetrics.InitInt("v.display.soc", SM_STALE_MIN, 0);
        mt_vehicle_time = MyMetrics.InitInt("v.display.time", SM_STALE_MIN, 0);
        mt_max_avail_power = MyMetrics.InitInt("v.display.power.max", SM_STALE_MIN, 0);
        mt_energy_used_reset = MyMetrics.InitInt("v.display.energy.reset", SM_STALE_MIN, 0);
        mt_trip_start = MyMetrics.InitInt("v.display.trip.start", SM_STALE_MIN, 0);
        mt_trip_reset = MyMetrics.InitInt("v.display.trip.reset", SM_STALE_MIN, 0); 
        mt_hv_active = MyMetrics.InitBool("v.b.hv.active", SM_STALE_MIN, false);

        RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
    }
}

OvmsVehicleSmartED::~OvmsVehicleSmartED() {
    ESP_LOGI(TAG, "Stop Smart ED vehicle module");
}

void OvmsVehicleSmartED::IncomingFrameCan1(CAN_frame_t* p_frame) {
    
    if (m_candata_poll != 1) {
        ESP_LOGI(TAG,"Car has woken (CAN bus activity)");
        StandardMetrics.ms_v_env_awake->SetValue(true);
        m_candata_poll = 1;
    }
    m_candata_timer = SE_CANDATA_TIMEOUT;
    
    uint8_t *d = p_frame->data.u8;
    
    switch (p_frame->MsgID) {
    case 0x518: // displayed SOC
    {
        /*ID:518 Nibble 15,16 (Wert halbieren)
         Beispiel:
         ID: 518  Data: 01 24 00 00 F8 7F C8 A7
         = A7 (in HEX)
         = 167 (in base10) und das nun halbieren
         = 83,5%*/
        float soc = (d[7]/2);
        mt_displayed_soc->SetValue((float) soc);
        StandardMetrics.ms_v_bat_soc->SetValue((float) soc);
        break;
    }
    case 0x2D5: //realSOC
    {
        /*ID:2D5 Nibble 10,11,12 (Wert durch zehn)
         Beispiel:
         ID: 2D5  Data: 00 00 4F 14 03 40 00 00
         = 340 (in HEX)
         = 832 (in base10) und das nun durch zehn
         = 83,2% */
        //StandardMetrics.ms_v_bat_soc->SetValue(((float) ((d[4] & 0x03) * 256 + d[5])) / 10, Percentage);
        StandardMetrics.ms_v_bat_soh->SetValue(((float) ((d[4] & 0x03) * 256 + d[5])) / 10, Percentage);
        break;
    }
    case 0x508: //HV ampere and charging yes/no
    {
        float HVA = 0;
        HVA = (float) (d[2] & 0x3F) * 256 + (float) d[3];
        HVA = (HVA / 10.0) - 819.2;
        StandardMetrics.ms_v_bat_current->SetValue(HVA, Amps);
        //StandardMetrics.ms_v_charge_state->SetValue(d[?]);
        //HVA = (((rxBuf[2] & 0x3F) * 256 + rxBuf[3]) * 0.1) - 819.2; 
        break;
    }
    case 0x448: //HV Voltage
    {
        float HV;
        float HVA = StandardMetrics.ms_v_bat_current->AsFloat();
        HV = ((float) d[6] * 256 + (float) d[7]);
        HV = HV / 10.0;
        float HVP = (HV * HVA) / 1000;
        StandardMetrics.ms_v_bat_voltage->SetValue(HV, Volts);
        StandardMetrics.ms_v_bat_power->SetValue(HVP);
        break;
    }
    case 0x3D5: //LV Voltage
    {
        float LV;
        LV = ((float) d[3]);
        LV = LV / 10.0;
        StandardMetrics.ms_v_bat_12v_voltage->SetValue(LV, Volts);
        break;
    }
    case 0x412: //ODO
    {
        /*ID:412 Nibble 7,8,9,10
         Beispiel:
         ID: 412  Data: 3B FF 00 63 5D 80 00 10
         = 635D (in HEX)
         = 25437 (in base10) Kilometer*/
        unsigned long ODO = (unsigned long) (d[2] * 65535 + (uint16_t) d[3] * 256
                + (uint16_t) d[4]);
        StandardMetrics.ms_v_pos_odometer->SetValue(ODO, Kilometers);
        break;
    }
    case 0x512: // system time? and departure time [0] = hour, [1]= minutes
    {
        uint32_t time = (d[0] * 60 + d[1]) * 60;
        mt_vehicle_time->SetValue(time, Seconds);
        time = (d[2] * 60 + d[3]) * 60;
        StandardMetrics.ms_v_charge_timerstart->SetValue(time, Seconds);
        break;
    }
    case 0x423: // Zündung,Türen,Fenster,Schloss, Licht, Blinker, Heckscheibenheizung
    {
        /*D0 is the state of the Ignition Key
        No Key is 0x40 in D0, Key on is 0x01

         D1 state of the Turn Lights
         D2 are the doors (0x00 all close, 0x01 driver open, 0x02 passenger open, etc)
         D3 state of the Headlamps, etc*/

        StandardMetrics.ms_v_env_on->SetValue((d[0] & 0x01) > 0);
        StandardMetrics.ms_v_door_fl->SetValue((d[2] & 0x01) > 0);
        StandardMetrics.ms_v_door_fr->SetValue((d[2] & 0x02) > 0);
        StandardMetrics.ms_v_door_trunk->SetValue((d[2] & 0x04) > 0);
        StandardMetrics.ms_v_env_headlights->SetValue((d[3] & 0x02) > 0);
        //StandardMetrics.ms_v_env_locked->SetBValue();
        break;
    }
    case 0x200: // hand brake and speed
    {
        StandardMetrics.ms_v_env_handbrake->SetValue(d[0]);
        float velocity = (d[2] * 256 + d[3]) / 18;
        StandardMetrics.ms_v_pos_speed->SetValue(velocity, Kph);
        break;
    }
    case 0x236: // paddels recu up and down
    {
        break;
    }
    case 0x318: // range and powerbar
    {
        float soc = StandardMetrics.ms_v_bat_soc->AsFloat();
        if(soc > 0) {
            float smart_range_ideal = (135 * soc) / 100;
            StandardMetrics.ms_v_bat_range_ideal->SetValue(smart_range_ideal); // ToDo
        }
        StandardMetrics.ms_v_bat_range_est->SetValue(d[7], Kilometers);
        StandardMetrics.ms_v_env_throttle->SetValue(d[5]);
        mt_max_avail_power->SetValue(d[5]);
        break;
    }
    case 0x443: //air condition and fan
    {
        //MyMetrics.
        break;
    }
    case 0x3F2: //Eco display
    {
        /*myMetrics.->ECO_accel                             = rxBuf[0] >> 1;
         myMetrics.->ECO_const                               = rxBuf[1] >> 1;
         myMetrics.->ECO_coast                               = rxBuf[2] >> 1;
         myMetrics.->ECO_total                               = rxBuf[3] >> 1;
         */
        break;
    }
    case 0x418: //gear shift
    {
        StandardMetrics.ms_v_env_gear->SetValue(d[0]);
        break;
    }
    case 0x408: //temp outdoor
    {
        float TEMPERATUR = ((d[4] - 50 ) - 32 ) / 1.8; 
        StandardMetrics.ms_v_env_temp->SetValue(TEMPERATUR);
        break;
    }
    case 0x3CE: //Verbrauch ab Start und ab Reset
    {
        float energy_used = (d[0] * 256 + d[1]) / 100;
        float energy_recd = (d[2] * 256 + d[3]) / 100;
        
        StandardMetrics.ms_v_bat_energy_used->SetValue(energy_used);
        StandardMetrics.ms_v_bat_energy_recd->SetValue(energy_recd);
        mt_energy_used_reset->SetValue(d[2]*256 + d[3]);
        break;
    }
    case 0x504: //Strecke ab Start und ab Reset
    {
        uint16_t value = d[1] * 256 + d[2];
        if (value != 254) {
            mt_trip_start->SetValue(value);
        }
        value = d[4] * 256 + d[5];
        if (value != 254) {
            mt_trip_reset->SetValue(value);
        }
        break;
    }
    case 0x3D7: //HV Status
    {   //HV active
        mt_hv_active->SetValue(d[0]);
        break;
    }
    case 0x312: //motor ?
    {
        //StandardMetrics
        break;
    }
    }
}

void OvmsVehicleSmartED::Ticker1(uint32_t ticker) {
    if (m_candata_timer > 0) {
        if (--m_candata_timer == 0) {
            // Car has gone to sleep
            ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
            //StandardMetrics.ms_v_env_on->SetValue(false);
            StandardMetrics.ms_v_env_awake->SetValue(false);
            //PollSetState(0);
            m_candata_poll = 0;
        }
    }
    
    if (StandardMetrics.ms_v_pos_speed->AsInt() == 0 && StandardMetrics.ms_v_bat_current->AsInt() > 1 && mt_hv_active) {
        if (StandardMetrics.ms_v_charge_state->AsString() != "charging") {
            StandardMetrics.ms_v_charge_state->SetValue("charging");
            StandardMetrics.ms_v_charge_inprogress->SetValue(true);
        }
    } else {
        if (StandardMetrics.ms_v_charge_state->AsString() != "done") {
            StandardMetrics.ms_v_charge_state->SetValue("done");
            StandardMetrics.ms_v_charge_inprogress->SetValue(false);
        }
    }
}

void OvmsVehicleSmartED::Ticker60(uint32_t ticker) {

}


OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandClimateControl(
        bool enable) {
    return CommandSetChargeTimer(enable, StandardMetrics.ms_m_timeutc->AsInt());
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandSetChargeTimer(
        bool timeron, uint32_t timerstart) {
//Set the charge start time as seconds since midnight or 8 Bit hour and 8 bit minutes?
    /*Mit
     0x512 00 00 12 1E 00 00 00 00
     setzt man z.B. die Uhrzeit auf 18:30. Maskiert man nun Byte 3 (0x12) mit 0x40 (und setzt so dort das zweite Bit auf High) wird die A/C Funktion mit aktiviert.
     */
    if(timerstart == 0) { 
        return Fail;
    }
    int t = timerstart + 3600; // Store the current time in time + GMT+1
    int days = (t / 86400);
    t = t - (days * 86400);
    int hours = (t / 3600);
    t = t - (hours * 3600);
    int minutes = (t / 60);
    minutes = (minutes - (minutes % 5)) + 5;
    
    CAN_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    frame.origin = m_can1;
    frame.FIR.U = 0;
    frame.FIR.B.DLC = 8;
    frame.FIR.B.FF = CAN_frame_std;
    frame.MsgID = 0x512;
    frame.data.u8[0] = 0x00;
    frame.data.u8[1] = 0x00;
    frame.data.u8[2] = (hours & 0xff);
    if (timeron) {
        frame.data.u8[3] = (minutes & 0xff) + 0x40; //(timerstart >> 8) & 0xff;
    } else {
        frame.data.u8[3] = (minutes & 0xff); //((timerstart >> 8) & 0xff) + 0x40;
    }
    frame.data.u8[4] = 0x00;
    frame.data.u8[5] = 0x00;
    frame.data.u8[6] = 0x00;
    frame.data.u8[7] = 0x00;
    m_can1->Write(&frame);
    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandSetChargeCurrent(
        uint16_t limit) {
    /*Den Ladestrom ändert man mit
     0x512 00 00 1F FF 00 7C 00 00 auf 12A.
     8A = 0x74, 12A = 0x7C and 32A = 0xA4 (als max. bei 22kW).
     Also einen Offset berechnen 0xA4 = d164: z.B. 12A = 0xA4 - 0x7C = 0x28 = d40 -> vom Maximum (0xA4 = 32A) abziehen
     und dann durch zwei dividieren, um auf den Wert für 20A zu kommen (mit 0,5A Auflösung).*/

    CAN_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    frame.origin = m_can1;
    frame.FIR.U = 0;
    frame.FIR.B.DLC = 8;
    frame.FIR.B.FF = CAN_frame_std;
    frame.MsgID = 0x512;
    frame.data.u8[0] = 0x00;
    frame.data.u8[1] = 0x00;
    frame.data.u8[2] = 0x1F;
    frame.data.u8[3] = 0xFF;
    frame.data.u8[4] = 0x00;
    frame.data.u8[5] = (uint8_t) (100 + 2 * limit);
    frame.data.u8[6] = 0x00;
    frame.data.u8[7] = 0x00;
    m_can1->Write(&frame);

    return Success;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandHomelink(int button, int durationms) {
    bool enable;
    if (button == 0) {
        enable = true;
        return CommandSetChargeTimer(enable, StandardMetrics.ms_m_timeutc->AsInt());
    }
    if (button == 1) {
        enable = false;
        return CommandSetChargeTimer(enable, StandardMetrics.ms_m_timeutc->AsInt());
    }
    return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandWakeup() {
    /*So we still have to get the car to wake up. Can someone please test these two queries when the car is asleep:
    0x218 00 00 00 00 00 00 00 00 or
    0x210 00 00 00 01 00 00 00 00
    Both patterns should comply with the Bosch CAN bus spec. for waking up a sleeping bus with recessive bits.*/

    ESP_LOGI(TAG, "Send Wakeup Command");
    
    CAN_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    frame.origin = m_can1;
    frame.FIR.U = 0;
    frame.FIR.B.DLC = 8;
    frame.FIR.B.FF = CAN_frame_std;
    frame.MsgID = 0x0e0;
    frame.data.u8[0] = 0xff;
    frame.data.u8[1] = 0xff;
    frame.data.u8[2] = 0xff;
    frame.data.u8[3] = 0x00;
    frame.data.u8[4] = 0x00;
    frame.data.u8[5] = 0x00;
    frame.data.u8[6] = 0x00;
    frame.data.u8[7] = 0x00;
    m_can1->Write(&frame);

    return Success;
}
#ifdef CONFIG_OVMS_COMP_MAX7317
void SmartEDLockingTimer(TimerHandle_t timer) {
    xTimerStop(timer, 0);
    xTimerDelete(timer, 0);
    //reset GEP 1 + 2
    
    MyPeripherals->m_max7317->Output(MAX7317_EGPIO_1, 0);
    MyPeripherals->m_max7317->Output(MAX7317_EGPIO_2, 0);
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandLock() {
    //switch 12v to GEP 1
    MyPeripherals->m_max7317->Output(MAX7317_EGPIO_1, 1);
    m_locking_timer = xTimerCreate("Smart ED Locking Timer", 500 / portTICK_PERIOD_MS, pdTRUE, this, SmartEDLockingTimer);
    xTimerStart(m_locking_timer, 0);
    return Success;
    //return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartED::CommandUnlock() {
    //switch 12v to GEP 2 
    MyPeripherals->m_max7317->Output(MAX7317_EGPIO_2, 1);
    m_locking_timer = xTimerCreate("Smart ED Locking Timer", 500 / portTICK_PERIOD_MS, pdTRUE, this, SmartEDLockingTimer);
    xTimerStart(m_locking_timer, 0);
    return Success;
    //return NotImplemented;
}
#endif

class OvmsVehicleSmartEDInit {
    public:
    OvmsVehicleSmartEDInit();
} MyOvmsVehicleSmartEDInit __attribute__ ((init_priority (9000)));

OvmsVehicleSmartEDInit::OvmsVehicleSmartEDInit() {
    ESP_LOGI(TAG, "Registering Vehicle: SMART ED (9000)");
    MyVehicleFactory.RegisterVehicle<OvmsVehicleSmartED>("SE", "Smart ED");
}

