#include "ovms_log.h"
#include "vehicle_mariner.h"
#include "ovms_metrics.h"
#include "lwip/sockets.h" // Native Espressif/LWIP network socket layer
#include <stdio.h>
#include <string.h>

static const char* TAG = "v-mariner";
static int listen_sock = -1;
static int client_sock = -1;

// Global variables for storage before transmission matching float profiles
float mariner_fuel_rate = 0.0f;
float mariner_oil_pressure = 0.0f;
int mariner_trim = 0;

// Helper function to calculate the mandatory NMEA 0183 XOR Checksum
uint8_t CalcChecksum(const char *sentence) 
{
    uint8_t checksum = 0;
    int i = 1; // Skip the '$' sign
    while (sentence[i] != '\0' && sentence[i] != '*') {
        checksum ^= sentence[i];
        i++;
    }
    return checksum;
}

// Background FreeRTOS task hosting the TCP Server on Marine Port 10110
void nmeaserver_task(void *pvParameters)
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(10110); // Standard Marine IP networking port

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create network stream socket");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "Socket unable to bind to port 10110");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    
    listen(listen_sock, 1);
    ESP_LOGI(TAG, "NMEA 0183 Stream Engine Active on TCP Port 10110");

    while (1) {
        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);
        
        // Code execution blocks here until OpenCPN links over Wi-Fi
        client_sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (client_sock >= 0) {
            ESP_LOGI(TAG, "OpenCPN application handshake complete!");
            
            while (client_sock >= 0) {
                char payload_buf[128];
                uint8_t csum;

                // Pull parameters directly out of the thread-safe OVMS metric engine registers
                int rpm = StandardMetrics.ms_v_mot_rpm->AsInt();
                int temp = StandardMetrics.ms_v_mot_temp->AsInt();
                int gear = StandardMetrics.ms_v_env_gear->AsInt();

                // Convert gear index back to standard marine characters
                char gear_char = 'N'; 
                if (gear > 0)  gear_char = 'F';
                if (gear < 0)  gear_char = 'R';

                // 1. Construct Engine Speed sentence ($EERPM)
                snprintf(payload_buf, sizeof(payload_buf), "$EERPM,E,0,%d,,A*", rpm);
                csum = CalcChecksum(payload_buf);
                char line1[160];
                snprintf(line1, sizeof(line1), "%s%02X\r\n", payload_buf, csum);
                
                // 2. Construct Environmental Transducer multiplexer sentence ($IIXDR)
                // Maps: Coolant Temp (C), Fuel Rate (L/h), Oil Pressure (Bar), Trim (%), and Gear
                snprintf(payload_buf, sizeof(payload_buf), 
                         "$IIXDR,C,%d,C,ENV_ENG_TEMP,V,%.1f,L,ENG_FUEL_RATE,P,%.2f,B,ENG_OIL_PRESS,G,%d,P,ENG_TRIM,S,%c,G,ENG_GEAR*", 
                         temp, mariner_fuel_rate, mariner_oil_pressure, mariner_trim, gear_char);
                csum = CalcChecksum(payload_buf);
                char line2[200];
                snprintf(line2, sizeof(line2), "%s%02X\r\n", payload_buf, csum);

                // Pipe data streams sequentially out of the Wi-Fi card socket
                if (send(client_sock, line1, strlen(line1), 0) < 0 ||
                    send(client_sock, line2, strlen(line2), 0) < 0) {
                    ESP_LOGE(TAG, "OpenCPN client link broken.");
                    close(client_sock);
                    client_sock = -1;
                }

                vTaskDelay(pdMS_TO_TICKS(500)); // Broadcast loop cycle: 2Hz (every 500ms)
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


OvmsVehicleMariner::OvmsVehicleMariner()
{
  ESP_LOGI(TAG, "Initializing Mariner 60HP EFI SmartCraft Module");

  // Configure CAN1 for passive listening mode at standard Marine 250 kbps
  RegisterCanBus(1, CAN_MODE_LISTENONLY, CAN_SPEED_250KBPS);
  
  // Keep the core OVMS active loop awake by faking the ignition state
  StandardMetrics.ms_v_env_on->SetValue(true); 
}

OvmsVehicleMariner::~OvmsVehicleMariner()
{
  ESP_LOGI(TAG, "Unloading Mariner Module");
}

void OvmsVehicleMariner::IncomingFrame(CAN_frame_t* frame)
{
  // Ignore frames that are not standard 29-bit Extended J1939 structures
  if (frame->FIR.B.FF != CAN_frame_ext) return;

  // Mask out the Parameter Group Number (PGN) from the 29-bit CAN ID
  uint32_t pgn = (frame->MsgID >> 8) & 0x03FFFF;
  uint8_t* data = frame->data.u8;

  switch (pgn)
  {
    case 61444: // Engine Speed (RPM)
      {
        uint16_t raw_rpm = ((uint16_t)data[5] << 8) | data[4];
        float rpm = raw_rpm * 0.125f;
        StandardMetrics.ms_v_mot_rpm->SetValue((int)rpm);
      }
      break;

    case 65262: // Engine Coolant Temperature
      {
        int16_t temp = (int16_t)data[0] - 40;
        StandardMetrics.ms_v_mot_temp->SetValue(temp);
      }
      break;

    case 65266: // Fuel Rate (Instant Consumption)
      {
        uint16_t raw_fuel = ((uint16_t)data[1] << 8) | data[0];
        float liters_per_hour = raw_fuel * 0.05f;
        MyMetrics.SetUserMetricFloat("m.mariner.fuelrate", liters_per_hour);
      }
      break;

    case 65263: // Engine Oil Pressure
      {
        uint16_t raw_press = data[3]; // Byte 4
        float kpa = raw_press * 4.0f;
        float bar = kpa * 0.01f;
        MyMetrics.SetUserMetricFloat("m.mariner.oilpressure", bar);
      }
      break;

    case 65249: // Watercraft Trim (Tilt)
      {
        float trim_pct = data[0] * 0.4f; // Byte 1
        MyMetrics.SetUserMetricFloat("m.mariner.trim", trim_pct);
      }
      break;

    case 65253: // Total Engine Hours
      {
        uint32_t raw_hours = ((uint32_t)data[3] << 24) |
                             ((uint32_t)data[2] << 16) |
                             ((uint32_t)data[1] << 8)  |
                             data[0];
        float hours = raw_hours * 0.05f;
        StandardMetrics.ms_v_mot_hours->SetValue(hours);
      }
      break;
	  
	case 61445: // Electronic Transmission Controller 1 (Gear State)
      {
        uint8_t raw_gear = data[3];
        
        switch (raw_gear)
        {
          case 124: // 0x7C - Neutral
            StandardMetrics.ms_v_env_gear->SetValue(0); // 0 maps to Neutral in OVMS
            break;
            
          case 125: // 0x7D - Forward
            StandardMetrics.ms_v_env_gear->SetValue(1); // Positive numbers map to Forward gears
            break;
            
          case 126: // 0x7E - Reverse
            StandardMetrics.ms_v_env_gear->SetValue(-1); // Negative numbers map to Reverse gears
            break;
            
          default:
            // Error or unknown state, do not update metric
            break;
        }
      }
      break;
	  
	case 65226: // Diagnostic Message 1 (Active DTC Alarms)
      {
        // Byte 1 and 2 store the system lamp alerts
        uint8_t alert_lamps = data[0]; 

        // Extract the 19-bit Suspect Parameter Number (SPN)
        uint32_t active_spn = ((uint32_t)(data[4] & 0xE0) << 11) | 
                              ((uint32_t)data[3] << 8) | 
                              data[2];

        // Extract the 5-bit Failure Mode Identifier (FMI)
        uint8_t active_fmi = data[4] & 0x1F;

        // Ensure the frame contains an actual active code (SPN > 0)
        if (active_spn > 0)
        {
          ESP_LOGE(TAG, "CRITICAL FAULT DETECTED! SPN: %d | FMI: %d", active_spn, active_fmi);

          // Evaluate specific common Mercury marine alarm parameters
          switch (active_spn)
          {
            case 110: // Engine Coolant Temperature Fault
              ESP_LOGE(TAG, "ALARM TRIGGERED: ENGINE OVERHEAT");
              break;
              
            case 100: // Low Engine Oil Pressure
              ESP_LOGE(TAG, "ALARM TRIGGERED: OIL PRESSURE CRITICAL");
              break;
              
            case 97:  // Water in Fuel Sensor
              ESP_LOGW(TAG, "WARNING TRIGGERED: WATER IN FUEL DETECTED");
              break;

            default:
              ESP_LOGW(TAG, "GENERIC ECU FAULT TRIGGERED");
              break;
          }
        }
      }
      break;
  }
}

// Module registration hooks into the main firmware architecture choice menu
class OvmsVehicleMarinerInit
{
  public: OvmsVehicleMarinerInit() {
    MyVehicleFactory.RegisterVehicle<OvmsVehicleMariner>("MR","Mariner SmartCraft");
  }
} OvmsVehicleMarinerInitInstance;

// FreeRTOS Task that injects fake J1939 CAN frames every 100ms
void mariner_simulation_task(void *pvParameters)
{
    OvmsVehicleMarinerTest* test_vehicle = (OvmsVehicleMarinerTest*)pvParameters;
    CAN_frame_t mock_frame;
    
    // Setup frame properties for 29-bit J1939 extended IDs
    mock_frame.FIR.B.FF = CAN_frame_ext;
    mock_frame.FIR.B.RTR = CAN_no_rtr;
    mock_frame.FIR.B.DLC = 8;

    uint16_t sim_counter = 0;

    ESP_LOGI("v-mariner-test", "Mariner 60HP Simulation Engine Started");

    while (1) {
        sim_counter++;

        // --- 1. MOCK ENGINE SPEED (RPM) - PGN 61444 ---
        // Dynamically swing RPM between 700 (idle) and 4500 RPM
        int target_rpm = 700 + (abs((int)(sim_counter % 200) - 100) * 38);
        uint16_t raw_rpm = (uint16_t)(target_rpm / 0.125f);
        
        mock_frame.MsgID = (61444 << 8) | 0x00; // PGN 61444, Source Address 0x00
        mock_frame.data.u8[0] = 0xFF; // Unused J1939 bytes
        mock_frame.data.u8[1] = 0xFF;
        mock_frame.data.u8[2] = 0xFF;
        mock_frame.data.u8[3] = (uint8_t)(raw_rpm & 0xFF);        // LSB
        mock_frame.data.u8[4] = (uint8_t)((raw_rpm >> 8) & 0xFF); // MSB
        mock_frame.data.u8[5] = 0xFF;
        mock_frame.data.u8[6] = 0xFF;
        mock_frame.data.u8[7] = 0xFF;
        test_vehicle->IncomingFrame(&mock_frame);

        // --- 2. MOCK TEMPERATURE (PGN 65262) & FUEL RATE (PGN 65266) ---
        // Inject this block every 500ms to mimic slower cyclical broadcast
        if (sim_counter % 5 == 0) {
            // Temperature stabilizes around 75 deg C (75 + 40 offset = 115)
            mock_frame.MsgID = (65262 << 8) | 0x00;
            mock_frame.data.u8[0] = 115; 
            test_vehicle->IncomingFrame(&mock_frame);

            // Fuel rate steps proportionally to the RPM scaling simulation
            uint16_t raw_fuel = (uint16_t)((5.0f + (target_rpm * 0.003f)) / 0.05f);
            mock_frame.MsgID = (65266 << 8) | 0x00;
            mock_frame.data.u8[0] = (uint8_t)(raw_fuel & 0xFF);
            mock_frame.data.u8[1] = (uint8_t)((raw_fuel >> 8) & 0xFF);
            test_vehicle->IncomingFrame(&mock_frame);
            
            // Mock Gear (PGN 61445) - set to Forward (125)
            mock_frame.MsgID = (61445 << 8) | 0x00;
            mock_frame.data.u8[3] = 125; 
            test_vehicle->IncomingFrame(&mock_frame);
        }

        // --- 3. MOCK OIL PRESSURE (PGN 65263) & TRIM (PGN 65249) ---
        if (sim_counter % 10 == 0) {
            // Oil pressure scales with RPM (around 3 to 5 Bar -> 300-500 kPa)
            uint8_t raw_press = (uint8_t)((300 + (target_rpm / 20)) / 4);
            mock_frame.MsgID = (65263 << 8) | 0x00;
            mock_frame.data.u8[3] = raw_press; 
            test_vehicle->IncomingFrame(&mock_frame);

            // Engine Trim static at 25% (25% / 0.4 = 62)
            mock_frame.MsgID = (65249 << 8) | 0x00;
            mock_frame.data.u8[1] = 62; 
            test_vehicle->IncomingFrame(&mock_frame);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Tick frequency: 10Hz
    }
}

// Constructor for the Test Vehicle Class
OvmsVehicleMarinerTest::OvmsVehicleMarinerTest()
{
  ESP_LOGI("v-mariner-test", "Loading Mocked Mariner 60HP Engine Profile");

  // Keep the core awake and run the native NMEA stream task
  StandardMetrics.ms_v_env_on->SetValue(true); 
  xTaskCreate(nmeaserver_task, "nmea_server", 4096, NULL, 5, NULL);

  // Spin up the local mock CAN bus injector loop task
  xTaskCreate(mariner_simulation_task, "mariner_sim", 3072, this, 5, NULL);
}

OvmsVehicleMarinerTest::~OvmsVehicleMarinerTest()
{
  ESP_LOGI("v-mariner-test", "Unloading Mocked Mariner Module");
  if (client_sock >= 0) close(client_sock);
  if (listen_sock >= 0) close(listen_sock);
}

// The Test class routes mock frames right through your existing parsing switch matrix
void OvmsVehicleMarinerTest::IncomingFrame(CAN_frame_t* frame)
{
    // Redirect frames directly into the standard parsing engine logic inside your main class
    // This guarantees your J1939 parsing execution blocks code gets tested
    ((OvmsVehicleMariner*)this)->IncomingFrame(frame);
}

// Append the selection options registry hook inside the main framework entry point
class OvmsVehicleMarinerTestInit
{
  public: OvmsVehicleMarinerTestInit() {
    MyVehicleFactory.RegisterVehicle<OvmsVehicleMarinerTest>("MRT","Mariner Test/Simulation");
  }
} OvmsVehicleMarinerTestInitInstance;
