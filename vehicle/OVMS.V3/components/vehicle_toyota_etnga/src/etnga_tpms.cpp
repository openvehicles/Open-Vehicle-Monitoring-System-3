/*
   Project:       Open Vehicle Monitor System
   Module:        Vehicle Toyota e-TNGA platform — TPMS (gateway-relayed)

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include <vector>
#include "ovms_log.h"
#include "ovms_config.h"
#include "vehicle_toyota_etnga.h"

// Freshness: TPMS sensors are motion-activated — they transmit only while the wheels are
// rolling. The ECU receiver holds the last values reported in motion, and those are what we
// read, so the published values are NOT guaranteed fresh for a car that has been parked a
// while. This is inherent to the sensors, not a polling gap; do not "fix" it by polling more.
//
// Slot vs corner: the TPMS ECU numbers sensor SLOTS (physical transmitter positions as learned
// during the last relearn), not corners. The slot->corner map (PID_TPMS_CORNERS) is therefore
// re-read on every poll cycle rather than cached once, because the mapping is car-specific and
// changes after a tyre rotation or a TPMS relearn; re-reading picks up the new assignment
// within one cycle with no restart.

bool OvmsVehicleToyotaETNGA::TPMSCornerMapValid()
{
    for (int s = 0; s < TPMS_SLOT_COUNT; s++)
        if (m_tpms_corner[s] >= 1 && m_tpms_corner[s] <= 4) return true;
    return false;
}

// Threshold ordering matters: pressure uses a low-pressure test (<=) and temperature an
// overheat test (>=), so for the three-level normal/warning/alert behaviour to work the
// pressure warn threshold must be ABOVE its alert threshold and the temperature warn threshold
// BELOW its alert threshold. Reversing either collapses the intermediate warning state and a
// tyre jumps straight from normal to alert.
void OvmsVehicleToyotaETNGA::UpdateTPMSAlert()
{
    float p_warn  = MyConfig.GetParamValueFloat("xte", "tpms.pressure.warn",  240.0f);  // kPa
    float p_alert = MyConfig.GetParamValueFloat("xte", "tpms.pressure.alert", 220.0f);  // kPa
    float t_warn  = MyConfig.GetParamValueFloat("xte", "tpms.temp.warn",       90.0f);  // C
    float t_alert = MyConfig.GetParamValueFloat("xte", "tpms.temp.alert",     100.0f);  // C

    std::vector<float> p = StandardMetrics.ms_v_tpms_pressure->AsVector();
    std::vector<float> t = StandardMetrics.ms_v_tpms_temp->AsVector();
    std::vector<short> alert(4, 0);
    for (int i = 0; i < 4; i++) {
        float pi = (i < (int)p.size()) ? p[i] : 0.0f;
        float ti = (i < (int)t.size()) ? t[i] : 0.0f;
        if (pi <= 0.0f) { alert[i] = 0; continue; }   // no reading on this wheel — no false alert
        if (pi <= p_alert || (ti > 0.0f && ti >= t_alert))      alert[i] = 2;
        else if (pi <= p_warn || (ti > 0.0f && ti >= t_warn))   alert[i] = 1;
        else                                                     alert[i] = 0;
    }
    StandardMetrics.ms_v_tpms_alert->SetValue(alert);
}

void OvmsVehicleToyotaETNGA::IncomingTPMS(uint16_t pid)
{
    switch (pid) {
        case PID_TPMS_CORNERS: {
            // 5x u8 corner enum, one per slot (0 none / 1 FL / 2 FR / 3 RL / 4 RR)
            if (m_rxbuf.size() < TPMS_SLOT_COUNT) { ESP_LOGW(TAG, "IncomingTPMS: short buffer for PID %04X", pid); break; }
            for (int s = 0; s < TPMS_SLOT_COUNT; s++)
                m_tpms_corner[s] = static_cast<int8_t>(GetRxBByte(m_rxbuf, s));
            break;
        }

        case PID_TPMS_PRESSURES: {
            // 5x u16 [status][raw]; psi_gauge = raw*0.25 - 7.35; kPa = psi * 6.894757
            if (m_rxbuf.size() < TPMS_SLOT_COUNT * 2) { ESP_LOGW(TAG, "IncomingTPMS: short buffer for PID %04X", pid); break; }
            if (!TPMSCornerMapValid()) { ESP_LOGD(TAG, "IncomingTPMS %04X: corner map not yet cached, deferring", pid); break; }
            std::vector<float> v(4, 0.0f);      // [FL,FR,RL,RR]
            for (int s = 0; s < TPMS_SLOT_COUNT; s++) {
                int corner = m_tpms_corner[s];
                if (corner < 1 || corner > 4) continue;     // unpopulated/spare slot
                uint8_t raw = GetRxBByte(m_rxbuf, 2 * s + 1);
                if (raw == 0) continue;                     // no sensor / "Initial Value"
                float psi = static_cast<float>(raw) * 0.25f - 7.35f;
                v[corner - 1] = psi * 6.894757f;
            }
            StandardMetrics.ms_v_tpms_pressure->SetValue(v);
            UpdateTPMSAlert();
            break;
        }

        case PID_TPMS_TEMPS: {
            // 5x u8; C = raw - 40
            if (m_rxbuf.size() < TPMS_SLOT_COUNT) { ESP_LOGW(TAG, "IncomingTPMS: short buffer for PID %04X", pid); break; }
            if (!TPMSCornerMapValid()) { ESP_LOGD(TAG, "IncomingTPMS %04X: corner map not yet cached, deferring", pid); break; }
            std::vector<float> v(4, 0.0f);      // [FL,FR,RL,RR]
            for (int s = 0; s < TPMS_SLOT_COUNT; s++) {
                int corner = m_tpms_corner[s];
                if (corner < 1 || corner > 4) continue;
                uint8_t raw = GetRxBByte(m_rxbuf, s);
                if (raw == 0) continue;                     // no sensor / "Initial Value"
                v[corner - 1] = static_cast<float>(raw) - 40.0f;
            }
            StandardMetrics.ms_v_tpms_temp->SetValue(v);
            UpdateTPMSAlert();
            break;
        }

        default:
            ESP_LOGW(TAG, "IncomingTPMS: unsupported PID %04X", pid);
            break;
    }
}
