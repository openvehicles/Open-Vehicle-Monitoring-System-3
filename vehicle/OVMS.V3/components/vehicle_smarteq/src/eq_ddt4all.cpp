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
    if (!IsAwakeEQ())
      CommandWakeup();         // enable CAN write access and wakeup vehicle
    if (IsAwakeEQ() && !m_can_last_acc_state) 
      smartCANbusAccess(true); // enable CAN write access
    return Success;
  }

  if((!m_ddt4all || !IsCANwrite()) && number > 9) {
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
      res = CommandCanVector(0x745, 0x765, {"30082002","30082002"}, false, true);
      break;
    }
    case 1:
    {
      // open trunk
      res = CommandCanVector(0x745, 0x765, {"300500","300500","300500","300500","300500"}, false, true);
      break;
    }
    case 2:
    {
      // AUTO_WIPE false
      res = CommandCanVector(0x74d, 0x76d, {"2E033C00"}, false, false);
      break;
    }
    case 3:
    {
      // AUTO_WIPE true
      res = CommandCanVector(0x74d, 0x76d, {"2E033C80"}, false, false);
      break;
    }
    case 4:
    {
      #ifdef CONFIG_OVMS_COMP_WIFI
        if (MyPeripherals && MyPeripherals->m_esp32wifi) 
          {
          MyPeripherals->m_esp32wifi->Restart();
          ESP_LOGI(TAG, "WiFi restart initiated");
          
          }
        else 
          {
          ESP_LOGE(TAG, "WiFi restart failed - WiFi not available");
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
        if (MyPeripherals && MyPeripherals->m_cellular_modem) 
          {
          MyPeripherals->m_cellular_modem->Restart();
          ESP_LOGI(TAG, "Cellular modem restart initiated");
          
          }
        else
          {
          ESP_LOGE(TAG, "Cellular modem restart failed - modem not available");
          res = Fail;
          }
      #else
        ESP_LOGE(TAG, "Cellular support not enabled");
        res = NotImplemented;
      #endif
      break;
    }
    // CommandCanVector(txid, rxid, hexbytes = {"30010000","30082002"}, reset CAN = true/false, CommandWakeup = true/ false)
    case 6:
    {
      // BIPBIP_Lock false
      res = CommandCanVector(0x745, 0x765, {"3B1400"}, false, false);
      break;
    }
    case 7:
    {
      // BIPBIP_Lock true
      res = CommandCanVector(0x745, 0x765, {"3B1480"}, false, false);
      break;
    }
    case 8:
    {
      // REAR_WIPER_LINK false
      res = CommandCanVector(0x745, 0x765, {"3B5800"}, false, false);
      break;
    }
    case 9:
    {
      // REAR_WIPER_LINK true
      res = CommandCanVector(0x745, 0x765, {"3B5880"}, false, false);
      break;
    }
    case 10:
    {
      // RKE_Backdoor_open false
      res = CommandCanVector(0x745, 0x765, {"3B7800"}, false, false);
      break;
    }
    case 11:
    {
      // RKE_Backdoor_open true
      res = CommandCanVector(0x745, 0x765, {"3B7880"}, false, false);
      break;
    }
    case 12:
    {
      // Precond_by_key 00
      res = CommandCanVector(0x745, 0x765, {"3B7700"}, false, false);
      break;
    }
    case 13:
    {
      // Precond_by_key 03
      res = CommandCanVector(0x745, 0x765, {"3B7703"}, false, false);
      break;
    }
    case 14:
    {
      // ECOMODE_PRE_Restart false
      res = CommandCanVector(0x745, 0x765, {"3B7600"}, false, false);
      break;
    }
    case 15:
    {
      // ECOMODE_PRE_Restart true
      res = CommandCanVector(0x745, 0x765, {"3B7680"}, false, false);
      break;
    }
    case 16:
    {
      // Charging screen false
      res = CommandCanVector(0x743, 0x763, {"2E013D00"}, true, false);
      break;
    }
    case 17:
    {
      // Charging screen true
      res = CommandCanVector(0x743, 0x763, {"2E013D01"}, true, false);
      break;
    }
    case 18:
    {
      // ONLY_DRL_OFF_CF false
      res = CommandCanVector(0x74d, 0x76d, {"2E045F00"}, false, false);
      break;
    }
    case 19:
    {
      // ONLY_DRL_OFF_CF true
      res = CommandCanVector(0x74d, 0x76d, {"2E045F80"}, false, false);
      break;
    }
    case 20:
    {
      // Welcome_Goodbye_CF false
      res = CommandCanVector(0x74d, 0x76d, {"2E033400"}, false, false);
      break;
    }
    case 21:
    {
      // Welcome_Goodbye_CF true
      res = CommandCanVector(0x74d, 0x76d, {"2E033480"}, false, false);
      break;
    }    
    case 22:
    {      
      // Default variant, tailgate open true, Precond_by_key 00
      res = CommandCanVector(0x745, 0x765, {"3B7880","3B7700"}, false, false);
      break;
    }
    case 23:
    {      
      // Default variant, tailgate open false, Precond_by_key 03
      res = CommandCanVector(0x745, 0x765, {"3B7800","3B7703"}, false, false);
      break;
    }
    case 26:
    {
      // AT_BeepInRPresent_CF false
      res = CommandCanVector(0x743, 0x763, {"2E014900"}, true, false);
      break;
    }
    case 27:
    {
      // AT_BeepInRPresent_CF true
      res = CommandCanVector(0x743, 0x763, {"2E014980"}, true, false);
      break;
    }
    
    case 28:
    {
      // EVStartupSoundInhibition_CF false
      res = CommandCanVector(0x743, 0x763, {"2E013501"}, true, false);
      break;
    }
    case 29:
    {
      // EVStartupSoundInhibition_CF true
      res = CommandCanVector(0x743, 0x763, {"2E013500"}, true, false);
      break;
    }
    case 32:
    {
      // key reminder false
      res = CommandCanVector(0x745, 0x765, {"3B5E00"}, false, false);
      break;
    }
    case 33:
    {
      // key reminder true
      res = CommandCanVector(0x745, 0x765, {"3B5E80"}, false, false);
      break;
    }
    case 34:
    {
      // long tempo display false
      res = CommandCanVector(0x745, 0x765, {"3B5700"}, false, false);
      break;
    }
    case 35:
    {
      // long tempo display true
      res = CommandCanVector(0x745, 0x765, {"3B5780"}, false, false);
      break;
    }
    case 36:
    {
      // AmbientLightPresent_CF false
      res = CommandCanVector(0x743, 0x763, {"2E018900"}, true, false);
      break;
    }
    case 37:
    {
      // AmbientLightPresent_CF true
      res = CommandCanVector(0x743, 0x763, {"2E018901"}, true, false);
      break;
    }
    case 40:
    {
      // ClockDisplayed_CF not displayed
      res = CommandCanVector(0x743, 0x763, {"2E012100"}, true, false);
      break;
    }
    case 41:
    {
      // ClockDisplayed_CF displayed managed
      res = CommandCanVector(0x743, 0x763, {"2E012101"}, true, false);
      break;
    }
    case 42:
    {
      // ClockDisplayed_CF displayed not managed
      res = CommandCanVector(0x743, 0x763, {"2E012102"}, true, false);
      break;
    }
    case 43:
    {
      // ClockDisplayed_CF not used (EQ Smart Connect)
      res = CommandCanVector(0x743, 0x763, {"2E012103"}, true, false);
      break;
    }
    case 44:
    {
      // Auto Light false
      res = CommandCanVector(0x74d, 0x76d, {"2E002300"}, false, true);
      break;
    }
    case 45:
    {
      // Auto Light true
      res = CommandCanVector(0x74d, 0x76d, {"2E002380"}, false, true);
      break;
    }
    case 46:
    {
      // Auto Light false
      res = CommandCanVector(0x745, 0x765, {"2E104200"}, false, false);
      break;
    }
    case 47:
    {
      // Auto Light true
      res = CommandCanVector(0x745, 0x765, {"2E104201"}, false, false);
      break;
    }
    case 48:
    {
      // Light by EMM false
      res = CommandCanVector(0x745, 0x765, {"3B4F00"}, false, false);
      break;
    }
    case 49:
    {
      // Light by EMM true
      res = CommandCanVector(0x745, 0x765, {"3B4F80"}, false, false);
      break;
    }
    case 50:
    {
      // max AC current limitation configuration 20A only for slow charger!
      if (!mt_obl_fastchg->AsBool()) {
        res = CommandCanVector(0x719, 0x739, {"2E614150"}, false, true);
      } else {
        res = Fail;
      }
      break;
    }
    case 51:
    {
      // max AC current limitation configuration 32A only for slow charger!
      if (!mt_obl_fastchg->AsBool()) {
        res = CommandCanVector(0x719, 0x739, {"2E614180"}, false, true);
      } else {
        res = Fail;
      }
      break;
    }
    case 52:
    {
      // SBRLogic_CF Standard
      res = CommandCanVector(0x743, 0x763, {"2E018500"}, true, false);
      break;
    }
    case 53:
    {
      // SBRLogic_CF US
      res = CommandCanVector(0x743, 0x763, {"2E018501"}, true, false);
      break;
    }
    case 54:
    {
      // FrontSBRInhibition_CF false
      res = CommandCanVector(0x743, 0x763, {"2E010900"}, true, false);
      break;
    }
    case 55:
    {
      // FrontSBRInhibition_CF true
      res = CommandCanVector(0x743, 0x763, {"2E010901"}, true, false);
      break;
    }
    case 56:
    {
      // Speedmeter ring (Tacho) DayBacklightsPresent_CF false
      res = CommandCanVector(0x743, 0x763, {"2E011800"}, true, false);
      break;
    }
    case 57:
    {
      // Speedmeter ring (Tacho) DayBacklightsPresent_CF true
      res = CommandCanVector(0x743, 0x763, {"2E011801"}, true, false);
      break;
    }
    case 58:
    {
      // AdditionnalInstrumentPresent_CF false
      res = CommandCanVector(0x743, 0x763, {"2E018000"}, true, false);
      break;
    }
    case 59:
    {
      // AdditionnalInstrumentPresent_CF true
      res = CommandCanVector(0x743, 0x763, {"2E018001"}, true, false);
      break;
    }
    case 60:
    {
      // TPMSPresent_CF false
      res = CommandCanVector(0x743, 0x763, {"2E010E00"}, true, false);
      break;
    }
    case 61:
    {
      // TPMSPresent_CF true
      res = CommandCanVector(0x743, 0x763, {"2E010E01"}, true, false);
      break;
    }
    case 62:
    {
      // DRL + Tail false
      res = CommandCanVector(0x74d, 0x76d, {"2E035300"}, false, true);
      break;
    }
    case 63:
    {
      // DRL + Tail true
      res = CommandCanVector(0x74d, 0x76d, {"2E035301"}, false, true);
      break;
    }
    case 66:
    {
      // DigitalSpeedometerPresent_CF off
      res = CommandCanVector(0x743, 0x763, {"2E013900"}, true, false);
      break;
    }
    case 67:
    {
      // DigitalSpeedometerPresent_CF in mph
      res = CommandCanVector(0x743, 0x763, {"2E013901"}, true, false);
      break;
    }
    case 68:
    {
      // DigitalSpeedometerPresent_CF in km/h
      res = CommandCanVector(0x743, 0x763, {"2E013902"}, true, false);
      break;
    }
    case 69:
    {
      // DigitalSpeedometerPresent_CF always km/h
      res = CommandCanVector(0x743, 0x763, {"2E013903"}, true, false);
      break;
    }
    case 100:
    {
      // ClearDiagnosticInformation.All
      res = CommandCanVector(0x745, 0x765, {"14FFFFFF"}, true, false);
      break;
    }
    case 719:
    {
      std::string hexbytes = mt_canbyte->AsString();
      res = CommandCanVector(0x719, 0x739, {hexbytes}, false, true);
      break;
    }
    case 743:
    {
      std::string hexbytes = mt_canbyte->AsString();
      res = CommandCanVector(0x743, 0x763, {hexbytes}, true, false);
      break;
    }
    case 745:
    {
      std::string hexbytes = mt_canbyte->AsString();
      res = CommandCanVector(0x745, 0x765, {hexbytes}, false, true);
      break;
    }
    case 746: // 746 is used for the 74d
    {
      std::string hexbytes = mt_canbyte->AsString();
      res = CommandCanVector(0x74d, 0x76d, {hexbytes}, false, true);
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
  writer->puts("--unlocked Car and key ignition on!");
  writer->puts("--use: xsq ddt4all <number>");
  writer->puts("--not all Commands works with all smart 453 (GAS/ED/EQ)");
  writer->puts("  -------------------------------");
  writer->puts("  Clear Diagnostic Information: 100");
  writer->puts("  activate DDT4all Commands for 5 minutes: 999");
  writer->puts("  -------------------------------");
  writer->puts("  indicator 5x on: 0");
  writer->puts("  open tailgate (unlocked Car): 1");
  writer->puts("  -------------------------------");
  writer->puts("  AUTO WIPE true: 3");
  writer->puts("  AUTO WIPE false: 2");
  writer->puts("  -------------------------------");
  writer->puts("  Wifi restart: 4");
  writer->puts("  Modem restart: 5");
  writer->puts("  -------------------------------");
  writer->puts("--OEM Alarm needed!");
  writer->puts("  BIPBIP Lock true: 7");
  writer->puts("  BIPBIP Lock false: 6");
  writer->puts("  -------------------------------");
  writer->puts("  REAR WIPER LINK R true: 9");
  writer->puts("  REAR WIPER LINK R false: 8");
  writer->puts("  -------------------------------");
  writer->puts("--swapped the tailgate open Button to Pre-Climate");
  writer->puts("  Precond by key 3: 13");
  writer->puts("  Precond by no: 12");
  writer->puts("  -------------------------------");
  writer->puts("--Deactivate tailgate open for Pre-Climate on key 3,");
  writer->puts("--but this has no effect for convertible rooftop opening!");
  writer->puts("--Convertible driver should pull the fuse for the roof opening");
  writer->puts("  tailgate open key 3 true: 11");
  writer->puts("  tailgate open key 3 false: 10");
  writer->puts("  -------------------------------");
  writer->puts("--tailgate key 3 and Pre-Climate, all in one step");
  writer->puts("  tailgate open true, Precond false: 22");
  writer->puts("  tailgate open false, Precond true: 23");
  writer->puts("  -------------------------------");
  writer->puts("  ECOMODE PRE Restart true: 15");
  writer->puts("  ECOMODE PRE Restart false: 14");
  writer->puts("  -------------------------------");
  writer->puts("--Show Charging screen when Car is locked");
  writer->puts("  Charging screen true: 17");
  writer->puts("  Charging screen false: 16");
  writer->puts("  -------------------------------");
  writer->puts("  only DRL off true: 19");
  writer->puts("  only DRL off false: 18");
  writer->puts("  -------------------------------");
  writer->puts("  Welcome/Goodbye true: 21");
  writer->puts("  Welcome/Goodbye false: 20");
  writer->puts("  -------------------------------");
  writer->puts("  Beep in Gear R true: 27");
  writer->puts("  Beep in Gear R false: 26");
  writer->puts("  -------------------------------");
  writer->puts("  EV Startup Sound true: 29");
  writer->puts("  EV Startup Sound false: 28");
  writer->puts("  -------------------------------");
  writer->puts("  key reminder true: 33");
  writer->puts("  key reminder false: 32");
  writer->puts("  -------------------------------");
  writer->puts("  long tempo display true: 35");
  writer->puts("  long tempo display false: 34");
  writer->puts("  -------------------------------");
  writer->puts("  Ambient light true: 37");
  writer->puts("  Ambient light false: 36");
  writer->puts("  -------------------------------");
  writer->puts("--MMI displayed Clock sync with Radio");
  writer->puts("  Clock not displayed: 40");
  writer->puts("  Clock displayed managed: 41");
  writer->puts("  Clock displayed not managed: 42");
  writer->puts("  Clock not used: 43");
  writer->puts("  -------------------------------");
  writer->puts("--if the light sensor and steering column have been retrofitted,");
  writer->puts("--set Auto Light 1, 2 and EMM true");
  writer->puts("  Auto light 1 true: 45");
  writer->puts("  Auto light 1 false: 44");
  writer->puts("  Auto light 2 true: 47");
  writer->puts("  Auto light 2 false: 46");
  writer->puts("  Light by EMM true: 49");
  writer->puts("  Light by EMM false: 48");
  writer->puts("  -------------------------------");
  writer->puts("--not for 22kW Charger!");
  writer->puts("  max AC current limit 20A: 50");
  writer->puts("  max AC current limit 32A: 51");
  writer->puts("  -------------------------------");
  writer->puts("  SBR Logic US: 52");
  writer->puts("  SBR Logic Standard: 53");
  writer->puts("  -------------------------------");
  writer->puts("  Front SBR true: 55");
  writer->puts("  Front SBR false: 54");
  writer->puts("  -------------------------------");
  writer->puts("  Speedometer Day Backlights true: 57");
  writer->puts("  Speedometer Day Backlights false: 56");
  writer->puts("  -------------------------------");
  writer->puts("--only makes sense with petrol engines");
  writer->puts("  Additionnal Instrument true: 59");
  writer->puts("  Additionnal Instrument false: 58");
  writer->puts("  -------------------------------");
  writer->puts("  TPMS Present true: 61");
  writer->puts("  TPMS Present false: 60");
  writer->puts("  -------------------------------");
  writer->puts("  DRL + Tail true: 63");
  writer->puts("  DRL + Tail false: 62");
  writer->puts("  -------------------------------");
  writer->puts("  Digital Speedometer off: 66");
  writer->puts("  Digital Speedometer in mph: 67");
  writer->puts("  Digital Speedometer in km/h: 68");
  writer->puts("  Digital Speedometer always km/h: 69");
  writer->puts("  -------------------------------");
  return Success;
}
