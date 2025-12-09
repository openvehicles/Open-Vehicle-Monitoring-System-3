/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
 ;    (C) 2019       Thomas Heuer
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
 ; https://github.com/MyLab-odyssey/ED4scan
 */

static const char *TAG = "v-smarteq";

#include "vehicle_smarteq.h"

void OvmsVehicleSmartEQ::xsq_ddt4all(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;

  if (argc != 1) {
    writer->puts("Error: xsq ddt4all requires one numerical argument");
    return;
  }
  
  smarteq->CommandDDT4all(atoi(argv[0]), writer);
}
OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandDDT4all(int number, OvmsWriter* writer) {
  if (number < 0 || number > 1000) {
    writer->printf("DDT4all command out of range");
    return Fail;
  }

  OvmsVehicle::vehicle_command_t res = Fail;
  ESP_LOGI(TAG, "DDT4all number=%d", number);

  if(number == 999) {
    ESP_LOGI(TAG, "DDT4ALL session activated for 5 minutes");
    writer->printf("DDT4ALL session activated for 5 minutes");
    m_ddt4all = true;
    m_ddt4all_ticker = 0;
    return Success;
  }

  if((!m_ddt4all || !m_enable_write) && number > 9) {
    ESP_LOGE(TAG, "DDT4all failed / no Canbus write access or DDT4all not enabled");
    writer->printf("DDT4all failed / no Canbus write access or DDT4all not enabled");
    return Fail;
  }

  if(m_ddt4all_exec > 0) {
    writer->printf("DDT4all command cooldown active, please wait %d seconds", m_ddt4all_exec);
    return Fail;
  }

  switch (number)
  {
    case 0:
    {
      // indicator 5x on
      CommandCan(0x745, 0x765, "30082002", false, true);
      res = Success;
      break;
    }
    case 1:
    {
      // open trunk
      CommandCan(0x745, 0x765, "300500", false, true);
      res = Success;
      break;
    }
    case 2:
    {
      // AUTO_WIPE false
      CommandCan(0x74d, 0x76d, "2E033C00", false, true);
      res = Success;
      break;
    }
    case 3:
    {
      // AUTO_WIPE true
      CommandCan(0x74d, 0x76d, "2E033C80", false, true);
      res = Success;
      break;
    }
    case 4:
    {
      #ifdef CONFIG_OVMS_COMP_WIFI
        if (MyPeripherals && MyPeripherals->m_esp32wifi) {
            WifiRestart();
            res = Success;
        } else {
            res = Fail;
        }
      #else
        ESP_LOGE(TAG, "WiFi support not enabled");
        res = NotImplemented;
      #endif
      break;
    }
    case 5:
    {
      #ifdef CONFIG_OVMS_COMP_CELLULAR
        if (MyPeripherals && MyPeripherals->m_cellular_modem) {
            ModemRestart();
            res = Success;
        } else {
            res = Fail;
        }
      #else
        ESP_LOGE(TAG, "Cellular support not enabled");
        res = NotImplemented;
      #endif
      break;
    }
    // CommandCan(txid, rxid, enable, request)
    case 6:
    {
      // BIPBIP_Lock false
      CommandCan(0x745, 0x765, "3B1400", false, true);
      res = Success;
      break;
    }
    case 7:
    {
      // BIPBIP_Lock true
      CommandCan(0x745, 0x765, "3B1480", false, true);
      res = Success;
      break;
    }
    case 8:
    {
      // REAR_WIPER_LINK false
      CommandCan(0x745, 0x765, "3B5800", false, true);
      res = Success;
      break;
    }
    case 9:
    {
      // REAR_WIPER_LINK true
      CommandCan(0x745, 0x765, "3B5880", false, true);
      res = Success;
      break;
    }
    case 10:
    {
      // RKE_Backdoor_open false
      CommandCan(0x745, 0x765, "3B7800", false, true);
      res = Success;
      break;
    }
    case 11:
    {
      // RKE_Backdoor_open true
      CommandCan(0x745, 0x765, "3B7880", false, true);
      res = Success;
      break;
    }
    case 12:
    {
      // Precond_by_key 00
      CommandCan(0x745, 0x765, "3B7700", false, true);
      res = Success;
      break;
    }
    case 13:
    {
      // Precond_by_key 03
      CommandCan(0x745, 0x765, "3B7703", false, true);
      res = Success;
      break;
    }
    case 14:
    {
      // ECOMODE_PRE_Restart false
      CommandCan(0x745, 0x765, "3B7600", false, true);
      res = Success;
      break;
    }
    case 15:
    {
      // ECOMODE_PRE_Restart true
      CommandCan(0x745, 0x765, "3B7680", false, true);
      res = Success;
      break;
    }
    case 16:
    {
      // Charging screen false
      CommandCan(0x743, 0x763, "2E013D00", true, false);
      res = Success;
      break;
    }
    case 17:
    {
      // Charging screen true
      CommandCan(0x743, 0x763, "2E013D01", true, false);
      res = Success;
      break;
    }
    case 18:
    {
      // ONLY_DRL_OFF_CF false
      CommandCan(0x74d, 0x76d, "2E045F00", false, true);
      res = Success;
      break;
    }
    case 19:
    {
      // ONLY_DRL_OFF_CF true
      CommandCan(0x74d, 0x76d, "2E045F80", false, true);
      res = Success;
      break;
    }
    case 20:
    {
      // Welcome_Goodbye_CF false
      CommandCan(0x74d, 0x76d, "2E033400", false, true);
      res = Success;
      break;
    }
    case 21:
    {
      // Welcome_Goodbye_CF true
      CommandCan(0x74d, 0x76d, "2E033480", false, true);
      res = Success;
      break;
    }    
    case 22:
    {      
      // Default variant, tailgate open true, Precond_by_key 00
      CommandCanVector(0x745, 0x765, {"3B7880","3B7700"}, false, true);
      res = Success;
      break;
    }
    case 23:
    {      
      // Default variant, tailgate open false, Precond_by_key 03
      CommandCanVector(0x745, 0x765, {"3B7800","3B7703"}, false, true);
      res = Success;
      break;
    }
    case 26:
    {
      // AT_BeepInRPresent_CF false
      CommandCan(0x743, 0x763, "2E014900", true, false);
      res = Success;
      break;
    }
    case 27:
    {
      // AT_BeepInRPresent_CF true
      CommandCan(0x743, 0x763, "2E014980", true, false);
      res = Success;
      break;
    }
    
    case 28:
    {
      // EVStartupSoundInhibition_CF false
      CommandCan(0x743, 0x763, "2E013501", true, false);
      res = Success;
      break;
    }
    case 29:
    {
      // EVStartupSoundInhibition_CF true
      CommandCan(0x743, 0x763, "2E013500", true, false);
      res = Success;
      break;
    }
    case 32:
    {
      // key reminder false
      CommandCan(0x745, 0x765, "3B5E00", false, true);
      res = Success;
      break;
    }
    case 33:
    {
      // key reminder true
      CommandCan(0x745, 0x765, "3B5E80", false, true);
      res = Success;
      break;
    }
    case 34:
    {
      // long tempo display false
      CommandCan(0x745, 0x765, "3B5700", false, true);
      res = Success;
      break;
    }
    case 35:
    {
      // long tempo display true
      CommandCan(0x745, 0x765, "3B5780", false, true);
      res = Success;
      break;
    }
    case 36:
    {
      // AmbientLightPresent_CF false
      CommandCan(0x743, 0x763, "2E018900", true, false);
      res = Success;
      break;
    }
    case 37:
    {
      // AmbientLightPresent_CF true
      CommandCan(0x743, 0x763, "2E018901", true, false);
      res = Success;
      break;
    }
    case 40:
    {
      // ClockDisplayed_CF not displayed
      CommandCan(0x743, 0x763, "2E012100", true, false);
      res = Success;
      break;
    }
    case 41:
    {
      // ClockDisplayed_CF displayed managed
      CommandCan(0x743, 0x763, "2E012101", true, false);
      res = Success;
      break;
    }
    case 42:
    {
      // ClockDisplayed_CF displayed not managed
      CommandCan(0x743, 0x763, "2E012102", true, false);
      res = Success;
      break;
    }
    case 43:
    {
      // ClockDisplayed_CF not used (EQ Smart Connect)
      CommandCan(0x743, 0x763, "2E012103", true, false);
      res = Success;
      break;
    }
    case 44:
    {
      // Auto Light false
      CommandCan(0x74d, 0x76d, "2E002300", false, true);
      res = Success;
      break;
    }
    case 45:
    {
      // Auto Light true
      CommandCan(0x74d, 0x76d, "2E002380", false, true);
      res = Success;
      break;
    }
    case 46:
    {
      // Auto Light false
      CommandCan(0x745, 0x765, "2E104200", false, true);
      res = Success;
      break;
    }
    case 47:
    {
      // Auto Light true
      CommandCan(0x745, 0x765, "2E104201", false, true);
      res = Success;
      break;
    }
    case 48:
    {
      // Light by EMM false
      CommandCan(0x745, 0x765, "3B4F00", false, true);
      res = Success;
      break;
    }
    case 49:
    {
      // Light by EMM true
      CommandCan(0x745, 0x765, "3B4F80", false, true);
      res = Success;
      break;
    }
    case 50:
    {
      // max AC current limitation configuration 20A only for slow charger!
      if (!mt_obl_fastchg->AsBool()) {
        CommandCan(0x719, 0x739, "2E614150", false, true);
        res = Success;
      } else {
        res = Fail;
      }
      break;
    }
    case 51:
    {
      // max AC current limitation configuration 32A only for slow charger!
      if (!mt_obl_fastchg->AsBool()) {
        CommandCan(0x719, 0x739, "2E614180", false, true);
        res = Success;
      } else {
        res = Fail;
      }
      break;
    }
    case 52:
    {
      // SBRLogic_CF Standard
      CommandCan(0x743, 0x763, "2E018500", true, false);
      res = Success;
      break;
    }
    case 53:
    {
      // SBRLogic_CF US
      CommandCan(0x743, 0x763, "2E018501", true, false);
      res = Success;
      break;
    }
    case 54:
    {
      // FrontSBRInhibition_CF false
      CommandCan(0x743, 0x763, "2E010900", true, false);
      res = Success;
      break;
    }
    case 55:
    {
      // FrontSBRInhibition_CF true
      CommandCan(0x743, 0x763, "2E010901", true, false);
      res = Success;
      break;
    }
    case 56:
    {
      // Speedmeter ring (Tacho) DayBacklightsPresent_CF false
      CommandCan(0x743, 0x763, "2E011800", true, false);
      res = Success;
      break;
    }
    case 57:
    {
      // Speedmeter ring (Tacho) DayBacklightsPresent_CF true
      CommandCan(0x743, 0x763, "2E011801", true, false);
      res = Success;
      break;
    }
    case 58:
    {
      // AdditionnalInstrumentPresent_CF false
      CommandCan(0x743, 0x763, "2E018000", true, false);
      res = Success;
      break;
    }
    case 59:
    {
      // AdditionnalInstrumentPresent_CF true
      CommandCan(0x743, 0x763, "2E018001", true, false);
      res = Success;
      break;
    }
    
    case 60:
    {
      // TPMSPresent_CF false
      CommandCan(0x743, 0x763, "2E010E00", true, false);
      res = Success;
      break;
    }
    case 61:
    {
      // TPMSPresent_CF true
      CommandCan(0x743, 0x763, "2E010E01", true, false);
      res = Success;
      break;
    }
    
    case 62:
    {
      // DRL + Tail false
      CommandCan(0x74d, 0x76d, "2E035300", false, true);
      res = Success;
      break;
    }
    case 63:
    {
      // DRL + Tail true
      CommandCan(0x74d, 0x76d, "2E035301", false, true);
      res = Success;
      break;
    }
    case 66:
    {
      // DigitalSpeedometerPresent_CF off
      CommandCan(0x743, 0x763, "2E013900", true, false);
      res = Success;
      break;
    }
    case 67:
    {
      // DigitalSpeedometerPresent_CF in mph
      CommandCan(0x743, 0x763, "2E013901", true, false);
      res = Success;
      break;
    }
    case 68:
    {
      // DigitalSpeedometerPresent_CF in km/h
      CommandCan(0x743, 0x763, "2E013902", true, false);
      res = Success;
      break;
    }
    case 69:
    {
      // DigitalSpeedometerPresent_CF always km/h
      CommandCan(0x743, 0x763, "2E013903", true, false);
      res = Success;
      break;
    }
    case 100:
    {
      // ClearDiagnosticInformation.All
      CommandCan(0x745, 0x765, "14FFFFFF", true, false);
      res = Success;
      break;
    }
    case 719:
    {
      std::string hexbytes = mt_canbyte->AsString();
      CommandCan(0x719, 0x739, hexbytes, false, true);
      res = Success;
      break;
    }
    case 743:
    {
      std::string hexbytes = mt_canbyte->AsString();
      CommandCan(0x743, 0x763, hexbytes, true, false);
      res = Success;
      break;
    }
    case 745:
    {
      std::string hexbytes = mt_canbyte->AsString();
      CommandCan(0x745, 0x765, hexbytes, false, true);
      res = Success;
      break;
    }
    case 746: // 746 is used for the 74d
    {
      std::string hexbytes = mt_canbyte->AsString();
      CommandCan(0x74d, 0x76d, hexbytes, false, true);
      res = Success;
      break;
    }
    default:
      res = Fail;
      break;
  }
  // Notify the user about the success/failed of the command
  if (res == Success) {
    char buf[70];
    snprintf(buf, sizeof(buf), "DDT4all command %d executed, %d seconds command cooldown!", number, m_ddt4all_exec);
    std::string msg(buf);
    ESP_LOGI(TAG, "%s", msg.c_str());
    writer->printf("%s", msg.c_str());
  } else {
    char buf[50];
    snprintf(buf, sizeof(buf), "DDT4all command %d failed", number);
    std::string msg(buf);
    ESP_LOGI(TAG, "%s", msg.c_str());
    writer->printf("%s", msg.c_str());
  }
  return res;
}

void OvmsVehicleSmartEQ::xsq_ddt4list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv) {
  OvmsVehicleSmartEQ* smarteq = GetInstance(writer);
  if (!smarteq)
    return;
  
    smarteq->CommandDDT4List(verbosity, writer);
}

OvmsVehicle::vehicle_command_t OvmsVehicleSmartEQ::CommandDDT4List(int verbosity, OvmsWriter* writer) {
  writer->puts("DDT4all Command list:");
  writer->printf("--unlocked Car and key ignition on!\n");
  writer->printf("--use: xsq ddt4all <number>\n");
  writer->printf("--not all Commands works with all smart 453 (GAS/ED/EQ)\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  Clear Diagnostic Information: 100\n");
  writer->printf("  activate DDT4all Commands for 5 minutes: 999\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  indicator 5x on: 0\n");
  writer->printf("  open tailgate (unlocked Car): 1\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  AUTO WIPE true: 3\n");
  writer->printf("  AUTO WIPE false: 2\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  Wifi restart: 4\n");
  writer->printf("  Modem restart: 5\n");
  writer->printf("  -------------------------------\n");
  writer->printf("--OEM Alarm needed!\n");
  writer->printf("  BIPBIP Lock true: 7\n");
  writer->printf("  BIPBIP Lock false: 6\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  REAR WIPER LINK R true: 9\n");
  writer->printf("  REAR WIPER LINK R false: 8\n");
  writer->printf("  -------------------------------\n");  
  writer->printf("--swapped the tailgate open Button to Pre-Climate\n");
  writer->printf("  Precond by key 3: 13\n");
  writer->printf("  Precond by no: 12\n");
  writer->printf("  -------------------------------\n");
  writer->printf("--Deactivate tailgate open for Pre-Climate on key 3,\n");
  writer->printf("--but this has no effect for convertible rooftop opening!\n");
  writer->printf("--Convertible driver should pull the fuse for the roof opening\n");
  writer->printf("  tailgate open key 3 true: 11\n");
  writer->printf("  tailgate open key 3 false: 10\n");
  writer->printf("  -------------------------------\n");
  writer->printf("--tailgate key 3 and Pre-Climate, all in one step\n");
  writer->printf("  tailgate open true, Precond false: 22\n");
  writer->printf("  tailgate open false, Precond true: 23\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  ECOMODE PRE Restart true: 15\n");
  writer->printf("  ECOMODE PRE Restart false: 14\n");
  writer->printf("  -------------------------------\n");
  writer->printf("--Show Charging screen when Car is locked\n");
  writer->printf("  Charging screen true: 17\n");
  writer->printf("  Charging screen false: 16\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  only DRL off true: 19\n");
  writer->printf("  only DRL off false: 18\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  Welcome/Goodbye true: 21\n");
  writer->printf("  Welcome/Goodbye false: 20\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  Beep in Gear R true: 27\n");
  writer->printf("  Beep in Gear R false: 26\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  EV Startup Sound true: 29\n");
  writer->printf("  EV Startup Sound false: 28\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  key reminder true: 33\n");
  writer->printf("  key reminder false: 32\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  long tempo display true: 35\n");
  writer->printf("  long tempo display false: 34\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  Ambient light true: 37\n");
  writer->printf("  Ambient light false: 36\n");
  writer->printf("  -------------------------------\n");
  writer->printf("--MMI displayed Clock sync with Radio\n");
  writer->printf("  Clock not displayed: 40\n");
  writer->printf("  Clock displayed managed: 41\n");
  writer->printf("  Clock displayed not managed: 42\n");
  writer->printf("  Clock not used: 43\n");
  writer->printf("  -------------------------------\n");
  writer->printf("--if the light sensor and steering column have been retrofitted,\n");
  writer->printf("--set Auto Light 1, 2 and EMM true\n");
  writer->printf("  Auto light 1 true: 45\n");
  writer->printf("  Auto light 1 false: 44\n");
  writer->printf("  Auto light 2 true: 47\n");
  writer->printf("  Auto light 2 false: 46\n");
  writer->printf("  Light by EMM true: 49\n");
  writer->printf("  Light by EMM false: 48\n");
  writer->printf("  -------------------------------\n");
  writer->printf("--not for 22kW Charger!\n");
  writer->printf("  max AC current limit 20A: 50\n");
  writer->printf("  max AC current limit 32A: 51\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  SBR Logic US: 52\n");
  writer->printf("  SBR Logic Standard: 53\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  Front SBR true: 55\n");
  writer->printf("  Front SBR false: 54\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  Speedometer Day Backlights true: 57\n");
  writer->printf("  Speedometer Day Backlights false: 56\n");
  writer->printf("  -------------------------------\n");
  writer->printf("--only makes sense with petrol engines\n");
  writer->printf("  Additionnal Instrument true: 59\n");
  writer->printf("  Additionnal Instrument false: 58\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  TPMS Present true: 61\n");
  writer->printf("  TPMS Present false: 60\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  DRL + Tail true: 63\n");
  writer->printf("  DRL + Tail false: 62\n");
  writer->printf("  -------------------------------\n");
  writer->printf("  Digital Speedometer off: 66\n");
  writer->printf("  Digital Speedometer in mph: 67\n");
  writer->printf("  Digital Speedometer in km/h: 68\n");
  writer->printf("  Digital Speedometer always km/h: 69\n");
  return Success;
}
