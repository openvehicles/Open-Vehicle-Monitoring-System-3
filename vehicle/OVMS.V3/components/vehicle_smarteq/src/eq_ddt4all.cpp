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
      m_hl_canbyte = "30082002";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 1:
    {
      // open trunk
      m_hl_canbyte = "300500";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 2:
    {
      // AUTO_WIPE false
      m_hl_canbyte = "2E033C00";
      CommandCan(0x74d, 0x76d, false, true);
      res = Success;
      break;
    }
    case 3:
    {
      // AUTO_WIPE true
      m_hl_canbyte = "2E033C80";
      CommandCan(0x74d, 0x76d, false, true);
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
      m_hl_canbyte = "3B1400";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 7:
    {
      // BIPBIP_Lock true
      m_hl_canbyte = "3B1480";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 8:
    {
      // REAR_WIPER_LINK false
      m_hl_canbyte = "3B5800";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 9:
    {
      // REAR_WIPER_LINK true
      m_hl_canbyte = "3B5880";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 10:
    {
      // RKE_Backdoor_open false
      m_hl_canbyte = "3B7800";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 11:
    {
      // RKE_Backdoor_open true
      m_hl_canbyte = "3B7880";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 12:
    {
      // Precond_by_key 00
      m_hl_canbyte = "3B7700";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 13:
    {
      // Precond_by_key 03
      m_hl_canbyte = "3B7703";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 14:
    {
      // ECOMODE_PRE_Restart false
      m_hl_canbyte = "3B7600";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 15:
    {
      // ECOMODE_PRE_Restart true
      m_hl_canbyte = "3B7680";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 16:
    {
      // Charging screen false
      m_hl_canbyte = "2E013D00";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 17:
    {
      // Charging screen true
      m_hl_canbyte = "2E013D01";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 18:
    {
      // ONLY_DRL_OFF_CF false
      m_hl_canbyte = "2E045F00";
      CommandCan(0x74d, 0x76d, false, true);
      res = Success;
      break;
    }
    case 19:
    {
      // ONLY_DRL_OFF_CF true
      m_hl_canbyte = "2E045F80";
      CommandCan(0x74d, 0x76d, false, true);
      res = Success;
      break;
    }
    case 20:
    {
      // Welcome_Goodbye_CF false
      m_hl_canbyte = "2E033400";
      CommandCan(0x74d, 0x76d, false, true);
      res = Success;
      break;
    }
    case 21:
    {
      // Welcome_Goodbye_CF true
      m_hl_canbyte = "2E033480";
      CommandCan(0x74d, 0x76d, false, true);
      res = Success;
      break;
    }
    case 26:
    {
      // AT_BeepInRPresent_CF false
      m_hl_canbyte = "2E014900";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 27:
    {
      // AT_BeepInRPresent_CF true
      m_hl_canbyte = "2E014980";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    
    case 28:
    {
      // EVStartupSoundInhibition_CF false
      m_hl_canbyte = "2E013501";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 29:
    {
      // EVStartupSoundInhibition_CF true
      m_hl_canbyte = "2E013500";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 32:
    {
      // key reminder false
      m_hl_canbyte = "3B5E00";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 33:
    {
      // key reminder true
      m_hl_canbyte = "3B5E80";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 34:
    {
      // long tempo display false
      m_hl_canbyte = "3B5700";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 35:
    {
      // long tempo display true
      m_hl_canbyte = "3B5780";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 36:
    {
      // AmbientLightPresent_CF false
      m_hl_canbyte = "2E018900";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 37:
    {
      // AmbientLightPresent_CF true
      m_hl_canbyte = "2E018901";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 40:
    {
      // ClockDisplayed_CF not displayed
      m_hl_canbyte = "2E012100";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 41:
    {
      // ClockDisplayed_CF displayed managed
      m_hl_canbyte = "2E012101";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 42:
    {
      // ClockDisplayed_CF displayed not managed
      m_hl_canbyte = "2E012102";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 43:
    {
      // ClockDisplayed_CF not used (EQ Smart Connect)
      m_hl_canbyte = "2E012103";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 44:
    {
      // Auto Light false
      m_hl_canbyte = "2E002300";
      CommandCan(0x74d, 0x76d, false, true);
      res = Success;
      break;
    }
    case 45:
    {
      // Auto Light true
      m_hl_canbyte = "2E002380";
      CommandCan(0x74d, 0x76d, false, true);
      res = Success;
      break;
    }
    case 46:
    {
      // Auto Light false
      m_hl_canbyte = "2E104200";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 47:
    {
      // Auto Light true
      m_hl_canbyte = "2E104201";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 48:
    {
      // Light by EMM false
      m_hl_canbyte = "3B4F00";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 49:
    {
      // Light by EMM true
      m_hl_canbyte = "3B4F80";
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 50:
    {
      // max AC current limitation configuration 20A only for slow charger!
      if (!mt_obl_fastchg->AsBool()) {
        m_hl_canbyte = "2E614150";
        CommandCan(0x719, 0x739, false, true);
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
        m_hl_canbyte = "2E614180";
        CommandCan(0x719, 0x739, false, true);
        res = Success;
      } else {
        res = Fail;
      }
      break;
    }
    case 52:
    {
      // SBRLogic_CF Standard
      m_hl_canbyte = "2E018500";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 53:
    {
      // SBRLogic_CF US
      m_hl_canbyte = "2E018501";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 54:
    {
      // FrontSBRInhibition_CF false
      m_hl_canbyte = "2E010900";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 55:
    {
      // FrontSBRInhibition_CF true
      m_hl_canbyte = "2E010901";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 56:
    {
      // Speedmeter ring (Tacho) DayBacklightsPresent_CF false
      m_hl_canbyte = "2E011800";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 57:
    {
      // Speedmeter ring (Tacho) DayBacklightsPresent_CF true
      m_hl_canbyte = "2E011801";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 58:
    {
      // AdditionnalInstrumentPresent_CF false
      m_hl_canbyte = "2E018001";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 59:
    {
      // AdditionnalInstrumentPresent_CF true
      m_hl_canbyte = "2E018001";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    
    case 60:
    {
      // TPMSPresent_CF false
      m_hl_canbyte = "2E010E00";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 61:
    {
      // TPMSPresent_CF true
      m_hl_canbyte = "2E010E01";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    
    case 62:
    {
      // DRL + Tail false
      m_hl_canbyte = "2E035300";
      CommandCan(0x74d, 0x76d, false, true);
      res = Success;
      break;
    }
    case 63:
    {
      // DRL + Tail true
      m_hl_canbyte = "2E035301";
      CommandCan(0x74d, 0x76d, false, true);
      res = Success;
      break;
    }
    case 66:
    {
      // DigitalSpeedometerPresent_CF off
      m_hl_canbyte = "2E013900";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 67:
    {
      // DigitalSpeedometerPresent_CF in mph
      m_hl_canbyte = "2E013901";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 68:
    {
      // DigitalSpeedometerPresent_CF in km/h
      m_hl_canbyte = "2E013902";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 69:
    {
      // DigitalSpeedometerPresent_CF always km/h
      m_hl_canbyte = "2E013903";
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 100:
    {
      // ClearDiagnosticInformation.All
      m_hl_canbyte = "14FFFFFF";
      CommandCan(0x745, 0x765, true, false);
      res = Success;
      break;
    }
    case 719:
    {
      m_hl_canbyte = mt_canbyte->AsString();
      CommandCan(0x719, 0x739, false, true);
      res = Success;
      break;
    }
    case 743:
    {
      m_hl_canbyte = mt_canbyte->AsString();
      CommandCan(0x743, 0x763, true, false);
      res = Success;
      break;
    }
    case 745:
    {
      m_hl_canbyte = mt_canbyte->AsString();
      CommandCan(0x745, 0x765, false, true);
      res = Success;
      break;
    }
    case 746: // 746 is used for the 74d
    {
      m_hl_canbyte = mt_canbyte->AsString();
      CommandCan(0x74d, 0x76d, false, true);
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
