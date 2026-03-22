/*
;    Project:       Open Vehicle Monitor System
;    Module:        Renault Zoe Ph2 - DDT Diagnostic Commands
;
;    (C) 2022-2026  Carsten Schmiemann
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

#include "vehicle_renaultzoe_ph2.h"

// ISO TP common function to send ISO TP requests to ECUs connected to V-CAN to change configuration or clear DTCs
OvmsVehicle::vehicle_command_t OvmsVehicleRenaultZoePh2::CommandDDT(uint32_t txid, uint32_t rxid, bool ext_frame, string request)
{
    uint8_t protocol = ext_frame ? ISOTP_EXTADR : ISOTP_STD;
    int timeout_ms = 1000;  // 1 second timeout for busy CAN bus
    std::string response;

    ESP_LOGD_RZ2(TAG, "CommandDDT: TX=0x%03" PRIX32 " RX=0x%03" PRIX32 " Protocol=%s Request=%s",
                 txid, rxid, ext_frame ? "EXTADR" : "STD", hexencode(request).c_str());

    int err = PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, protocol);

    if (err == POLLSINGLE_OK)
    {
        ESP_LOGD_RZ2(TAG, "CommandDDT: Success - Response: %s", hexencode(response).c_str());
        return Success;
    }
    else if (err == POLLSINGLE_TXFAILURE)
    {
        ESP_LOGE(TAG, "CommandDDT: Transmission failure (CAN bus error)");
        return Fail;
    }
    else if (err == POLLSINGLE_TIMEOUT)
    {
        ESP_LOGE(TAG, "CommandDDT: Timeout waiting for response");
        return Fail;
    }
    else
    {
        // UDS/ISO-TP error code (positive value)
        ESP_LOGE(TAG, "CommandDDT: Request failed with error code 0x%02X (%s)",
                 err, OvmsPoller::PollResultCodeName(err));
        return Fail;
    }
}

// Helper function to read ECU register using UDS 0x22 (ReadDataByIdentifier)
int OvmsVehicleRenaultZoePh2::ReadECURegister(uint32_t txid, uint32_t rxid, uint16_t register_id, int timeout_ms, uint8_t *value, size_t value_len)
{
    // Construct UDS 0x22 request: service 0x22 + 2-byte register ID
    char request_hex[7];
    snprintf(request_hex, sizeof(request_hex), "22%04X", register_id);
    std::string request = hexdecode(request_hex);
    std::string response;

    ESP_LOGD_RZ2(TAG, "ReadECURegister: TX=0x%03" PRIX32 " RX=0x%03" PRIX32 " Register=0x%04X",
                 txid, rxid, register_id);

    int err = PollSingleRequest(m_can1, txid, rxid, request, response, timeout_ms, ISOTP_STD);

    if (err != POLLSINGLE_OK)
    {
        ESP_LOGW(TAG, "ReadECURegister: Failed to read register 0x%04X, PollSingleRequest error=%d (%s)",
                 register_id, err, OvmsPoller::PollResultCodeName(err));
        return err;
    }

    if (response.length() < value_len)
    {
        ESP_LOGE(TAG, "ReadECURegister: Response too short (got %zu bytes, expected %zu) for register 0x%04X",
                 response.length(), value_len, register_id);
        ESP_LOGD_RZ2(TAG, "ReadECURegister: Raw response: %s", hexencode(response).c_str());
        return -1;
    }

    if (response.length() > value_len)
    {
        ESP_LOGD_RZ2(TAG, "ReadECURegister: Register 0x%04X returned %zu bytes, using first %zu: %s",
                     register_id, response.length(), value_len, hexencode(response).c_str());
    }

    // Extract data bytes
    memcpy(value, response.data(), value_len);

    ESP_LOGD_RZ2(TAG, "ReadECURegister: Register 0x%04X = %s",
                 register_id, hexencode(std::string((char*)value, value_len)).c_str());

    return POLLSINGLE_OK;
}

// Helper function to verify PTC state by reading all 3 BCM registers
bool OvmsVehicleRenaultZoePh2::VerifyPTCState(bool enabled, int timeout_ms, uint8_t *failed_ptc_mask)
{
    const uint16_t ptc_registers[3] = {0x0533, 0x0534, 0x0535};
    const uint8_t ptc_enabled_values[3] = {0xD4, 0xBC, 0x8C};
    const uint8_t ptc_disabled_values[3] = {0x54, 0x3C, 0x0C};
    const uint8_t *expected_values = enabled ? ptc_enabled_values : ptc_disabled_values;

    bool all_match = true;
    *failed_ptc_mask = 0;
    int communication_failures = 0;

    for (int i = 0; i < 3; i++)
    {
        uint8_t value = 0;
        int err = ReadECURegister(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, ptc_registers[i], timeout_ms, &value, 1);

        if (err != POLLSINGLE_OK)
        {
            // Distinguish between communication errors and ECU rejections
            if (err >= -3)  // POLLSINGLE error codes
            {
                ESP_LOGW(TAG, "VerifyPTCState: Communication error reading PTC%d register 0x%04X (error=%d)",
                         i + 1, ptc_registers[i], err);
                communication_failures++;
            }
            else if (err <= -100)  // UDS negative response codes
            {
                uint8_t nrc = -(err + 100);
                ESP_LOGW(TAG, "VerifyPTCState: PTC%d register 0x%04X not supported by ECU (NRC=0x%02X)",
                         i + 1, ptc_registers[i], nrc);
            }

            *failed_ptc_mask |= (1 << i);
            all_match = false;
            continue;
        }

        if (value != expected_values[i])
        {
            ESP_LOGW(TAG, "VerifyPTCState: PTC%d mismatch - got 0x%02X, expected 0x%02X (%s)",
                     i + 1, value, expected_values[i], enabled ? "enabled" : "disabled");
            *failed_ptc_mask |= (1 << i);
            all_match = false;
        }
        else
        {
            ESP_LOGD_RZ2(TAG, "VerifyPTCState: PTC%d OK - register 0x%04X = 0x%02X",
                         i + 1, ptc_registers[i], value);
        }
    }

    if (all_match)
    {
        ESP_LOGI(TAG, "VerifyPTCState: All 3 PTCs are %s", enabled ? "enabled" : "disabled");
    }
    else
    {
        if (communication_failures > 0)
        {
            ESP_LOGW(TAG, "VerifyPTCState: Verification failed - %d communication error(s), failed_mask=0x%02X",
                     communication_failures, *failed_ptc_mask);
        }
        else
        {
            ESP_LOGW(TAG, "VerifyPTCState: Verification failed - state mismatch, failed_mask=0x%02X",
                     *failed_ptc_mask);
        }
    }

    return all_match;
}

// Helper function to verify compressor state by reading HVAC hot/cold loop registers
bool OvmsVehicleRenaultZoePh2::VerifyCompressorState(bool enabled, int timeout_ms, uint8_t *hot_loop_value, uint8_t *cold_loop_value)
{
    const uint16_t hot_loop_register = 0x4224;
    const uint16_t cold_loop_register = 0x421F;
    const uint8_t expected_hot = enabled ? 0x02 : 0x01;
    const uint8_t expected_cold = enabled ? 0x06 : 0x01;

    bool all_match = true;

    // Read hot loop register
    int err_hot = ReadECURegister(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, hot_loop_register, timeout_ms, hot_loop_value, 1);
    if (err_hot != POLLSINGLE_OK)
    {
        ESP_LOGW(TAG, "VerifyCompressorState: Failed to read hot loop register 0x%04X", hot_loop_register);
        all_match = false;
    }
    else if (*hot_loop_value != expected_hot)
    {
        ESP_LOGW(TAG, "VerifyCompressorState: Hot loop mismatch - got 0x%02X, expected 0x%02X (%s)",
                 *hot_loop_value, expected_hot, enabled ? "enabled" : "disabled");
        all_match = false;
    }
    else
    {
        ESP_LOGD_RZ2(TAG, "VerifyCompressorState: Hot loop OK - 0x4224 = 0x%02X", *hot_loop_value);
    }

    // Read cold loop register
    int err_cold = ReadECURegister(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, cold_loop_register, timeout_ms, cold_loop_value, 1);
    if (err_cold != POLLSINGLE_OK)
    {
        ESP_LOGW(TAG, "VerifyCompressorState: Failed to read cold loop register 0x%04X", cold_loop_register);
        all_match = false;
    }
    else if (*cold_loop_value != expected_cold)
    {
        ESP_LOGW(TAG, "VerifyCompressorState: Cold loop mismatch - got 0x%02X, expected 0x%02X (%s)",
                 *cold_loop_value, expected_cold, enabled ? "enabled" : "disabled");
        all_match = false;
    }
    else
    {
        ESP_LOGD_RZ2(TAG, "VerifyCompressorState: Cold loop OK - 0x421F = 0x%02X", *cold_loop_value);
    }

    if (all_match)
    {
        ESP_LOGI(TAG, "VerifyCompressorState: Compressor is %s (hot=0x%02X, cold=0x%02X)",
                 enabled ? "enabled" : "disabled", *hot_loop_value, *cold_loop_value);
    }
    else
    {
        ESP_LOGW(TAG, "VerifyCompressorState: Verification failed");
    }

    return all_match;
}

void OvmsVehicleRenaultZoePh2::CommandDdtHvacEnableCompressor(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    if (!StandardMetrics.ms_v_env_awake->AsBool() || StandardMetrics.ms_v_pos_speed->AsFloat() != 0)
    {
        ESP_LOGI(TAG, "Zoe is not alive or is driving");
        writer->puts("Zoe is not alive or is driving");
        return;
    }

    ESP_LOGI(TAG, "Try to send HVAC compressor enable command");
    writer->puts("Sending compressor enable sequence...");

    const int max_send_retries = 3;
    const int max_verify_retries = 3;
    OvmsVehicle::vehicle_command_t result;

    // Step 1: Extended session
    int retry_count = 0;
    do {
        result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("1003"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "Extended session command sent successfully");
            writer->puts("  Extended session entered (0x1003)");
            break;
        }
        retry_count++;
        if (retry_count < max_send_retries) {
            ESP_LOGW(TAG, "Extended session command failed, retrying (%d/%d)", retry_count, max_send_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_send_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "Extended session command failed after %d attempts", max_send_retries);
        writer->puts("ERROR: Extended session command failed");
        return;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Step 2: Send hot and cold loop enable commands, then verify BEFORE ECU reset
    bool verified = false;
    int verify_retry_count = 0;
    uint8_t hot_value = 0, cold_value = 0;

    do {
        // Enable hot loop
        retry_count = 0;
        do {
            result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("2E422402"));
            if (result == OvmsVehicle::Success) {
                ESP_LOGI(TAG, "Hot loop enable command sent successfully");
                writer->puts("  Hot loop enabled (0x2E422402)");
                break;
            }
            retry_count++;
            if (retry_count < max_send_retries) {
                ESP_LOGW(TAG, "Hot loop enable command failed, retrying (%d/%d)", retry_count, max_send_retries);
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        } while (retry_count < max_send_retries);

        if (result != OvmsVehicle::Success) {
            ESP_LOGE(TAG, "Hot loop enable command failed after %d attempts", max_send_retries);
            writer->puts("ERROR: Hot loop enable command failed");
            return;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);

        // Enable cold loop
        retry_count = 0;
        do {
            result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("2E421F06"));
            if (result == OvmsVehicle::Success) {
                ESP_LOGI(TAG, "Cold loop enable command sent successfully");
                writer->puts("  Cold loop enabled (0x2E421F06)");
                break;
            }
            retry_count++;
            if (retry_count < max_send_retries) {
                ESP_LOGW(TAG, "Cold loop enable command failed, retrying (%d/%d)", retry_count, max_send_retries);
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        } while (retry_count < max_send_retries);

        if (result != OvmsVehicle::Success) {
            ESP_LOGE(TAG, "Cold loop enable command failed after %d attempts", max_send_retries);
            writer->puts("ERROR: Cold loop enable command failed");
            return;
        }

        // Step 3: Verify registers BEFORE ECU reset
        writer->puts("Verifying compressor state before ECU reset...");
        vTaskDelay(500 / portTICK_PERIOD_MS);

        verified = rz2->VerifyCompressorState(true, 3000, &hot_value, &cold_value);

        if (!verified && verify_retry_count < max_verify_retries) {
            verify_retry_count++;
            ESP_LOGW(TAG, "Compressor verification failed (hot=0x%02X, cold=0x%02X), retry %d/%d",
                     hot_value, cold_value, verify_retry_count, max_verify_retries);
            writer->printf("  Verification failed (hot=0x%02X, cold=0x%02X), retrying (%d/%d)...\n",
                          hot_value, cold_value, verify_retry_count, max_verify_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }

    } while (!verified && verify_retry_count < max_verify_retries);

    if (!verified) {
        ESP_LOGE(TAG, "Compressor enable verification failed after %d retries (hot=0x%02X, cold=0x%02X)",
                 verify_retry_count, hot_value, cold_value);
        writer->puts("ERROR: Compressor verification failed:");
        writer->printf("  Hot loop (0x4224): got 0x%02X, expected 0x02\n", hot_value);
        writer->printf("  Cold loop (0x421F): got 0x%02X, expected 0x06\n", cold_value);
        writer->puts("  ECU reset NOT performed - compressor may not be enabled");
        return;
    }

    // Step 4: Only send ECU reset after successful verification
    writer->puts("  Verification successful, performing ECU reset...");
    retry_count = 0;
    do {
        result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("1101"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "ECU reset command sent successfully");
            writer->puts("  ECU reset (0x1101)");
            break;
        }
        retry_count++;
        if (retry_count < max_send_retries) {
            ESP_LOGW(TAG, "ECU reset command failed, retrying (%d/%d)", retry_count, max_send_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_send_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "ECU reset command failed after %d attempts", max_send_retries);
        writer->puts("ERROR: ECU reset command failed");
        writer->puts("  Loops are enabled but ECU not reset - may need manual reset");
        return;
    }

    // Success
    ESP_LOGI(TAG, "Compressor enabled successfully (hot=0x%02X, cold=0x%02X)", hot_value, cold_value);
    writer->puts("✓ Compressor enabled successfully");
    writer->printf("  Register 0x4224 = 0x%02X (hot loop enabled)\n", hot_value);
    writer->printf("  Register 0x421F = 0x%02X (cold loop enabled)\n", cold_value);
    if (verify_retry_count > 0) {
        writer->printf("  (verified after %d retry cycle%s)\n", verify_retry_count, verify_retry_count > 1 ? "s" : "");
    }
}

void OvmsVehicleRenaultZoePh2::CommandDdtHvacDisableCompressor(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    if (!StandardMetrics.ms_v_env_awake->AsBool() || StandardMetrics.ms_v_pos_speed->AsFloat() != 0)
    {
        ESP_LOGI(TAG, "Zoe is not alive or is driving");
        writer->puts("Zoe is not alive or is driving");
        return;
    }

    ESP_LOGI(TAG, "Try to send HVAC compressor disable command");
    writer->puts("Sending compressor disable sequence...");

    const int max_send_retries = 3;
    const int max_verify_retries = 3;
    OvmsVehicle::vehicle_command_t result;

    // Step 1: Extended session
    int retry_count = 0;
    do {
        result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("1003"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "Extended session command sent successfully");
            writer->puts("  Extended session entered (0x1003)");
            break;
        }
        retry_count++;
        if (retry_count < max_send_retries) {
            ESP_LOGW(TAG, "Extended session command failed, retrying (%d/%d)", retry_count, max_send_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_send_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "Extended session command failed after %d attempts", max_send_retries);
        writer->puts("ERROR: Extended session command failed");
        return;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Step 2: Send hot and cold loop disable commands, then verify BEFORE ECU reset
    bool verified = false;
    int verify_retry_count = 0;
    uint8_t hot_value = 0, cold_value = 0;

    do {
        // Disable hot loop
        retry_count = 0;
        do {
            result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("2E422401"));
            if (result == OvmsVehicle::Success) {
                ESP_LOGI(TAG, "Hot loop disable command sent successfully");
                writer->puts("  Hot loop disabled (0x2E422401)");
                break;
            }
            retry_count++;
            if (retry_count < max_send_retries) {
                ESP_LOGW(TAG, "Hot loop disable command failed, retrying (%d/%d)", retry_count, max_send_retries);
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        } while (retry_count < max_send_retries);

        if (result != OvmsVehicle::Success) {
            ESP_LOGE(TAG, "Hot loop disable command failed after %d attempts", max_send_retries);
            writer->puts("ERROR: Hot loop disable command failed");
            return;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);

        // Disable cold loop
        retry_count = 0;
        do {
            result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("2E421F01"));
            if (result == OvmsVehicle::Success) {
                ESP_LOGI(TAG, "Cold loop disable command sent successfully");
                writer->puts("  Cold loop disabled (0x2E421F01)");
                break;
            }
            retry_count++;
            if (retry_count < max_send_retries) {
                ESP_LOGW(TAG, "Cold loop disable command failed, retrying (%d/%d)", retry_count, max_send_retries);
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        } while (retry_count < max_send_retries);

        if (result != OvmsVehicle::Success) {
            ESP_LOGE(TAG, "Cold loop disable command failed after %d attempts", max_send_retries);
            writer->puts("ERROR: Cold loop disable command failed");
            return;
        }

        // Step 3: Verify registers BEFORE ECU reset
        writer->puts("Verifying compressor state before ECU reset...");
        vTaskDelay(500 / portTICK_PERIOD_MS);

        verified = rz2->VerifyCompressorState(false, 3000, &hot_value, &cold_value);

        if (!verified && verify_retry_count < max_verify_retries) {
            verify_retry_count++;
            ESP_LOGW(TAG, "Compressor verification failed (hot=0x%02X, cold=0x%02X), retry %d/%d",
                     hot_value, cold_value, verify_retry_count, max_verify_retries);
            writer->printf("  Verification failed (hot=0x%02X, cold=0x%02X), retrying (%d/%d)...\n",
                          hot_value, cold_value, verify_retry_count, max_verify_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }

    } while (!verified && verify_retry_count < max_verify_retries);

    if (!verified) {
        ESP_LOGE(TAG, "Compressor disable verification failed after %d retries (hot=0x%02X, cold=0x%02X)",
                 verify_retry_count, hot_value, cold_value);
        writer->puts("ERROR: Compressor verification failed:");
        writer->printf("  Hot loop (0x4224): got 0x%02X, expected 0x01\n", hot_value);
        writer->printf("  Cold loop (0x421F): got 0x%02X, expected 0x01\n", cold_value);
        writer->puts("  ECU reset NOT performed - compressor may not be disabled");
        return;
    }

    // Step 4: Only send ECU reset after successful verification
    writer->puts("  Verification successful, performing ECU reset...");
    retry_count = 0;
    do {
        result = rz2->CommandDDT(HVAC_DDT_REQ_ID, HVAC_DDT_RESP_ID, false, hexdecode("1101"));
        if (result == OvmsVehicle::Success) {
            ESP_LOGI(TAG, "ECU reset command sent successfully");
            writer->puts("  ECU reset (0x1101)");
            break;
        }
        retry_count++;
        if (retry_count < max_send_retries) {
            ESP_LOGW(TAG, "ECU reset command failed, retrying (%d/%d)", retry_count, max_send_retries);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    } while (retry_count < max_send_retries);

    if (result != OvmsVehicle::Success) {
        ESP_LOGE(TAG, "ECU reset command failed after %d attempts", max_send_retries);
        writer->puts("ERROR: ECU reset command failed");
        writer->puts("  Loops are disabled but ECU not reset - may need manual reset");
        return;
    }

    // Success
    ESP_LOGI(TAG, "Compressor disabled successfully (hot=0x%02X, cold=0x%02X)", hot_value, cold_value);
    writer->puts("✓ Compressor disabled successfully");
    writer->printf("  Register 0x4224 = 0x%02X (hot loop disabled)\n", hot_value);
    writer->printf("  Register 0x421F = 0x%02X (cold loop disabled)\n", cold_value);
    if (verify_retry_count > 0) {
        writer->printf("  (verified after %d retry cycle%s)\n", verify_retry_count, verify_retry_count > 1 ? "s" : "");
    }
}

void OvmsVehicleRenaultZoePh2::CommandDdtClusterResetService(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();
    if (StandardMetrics.ms_v_env_awake->AsBool() && StandardMetrics.ms_v_pos_speed->AsFloat() == 0)
    {
        ESP_LOGI(TAG, "Try to send Cluster service reset command");
        writer->puts("Try to send Cluster service reset command");

        // Extended session
        rz2->CommandDDT(METER_DDT_REQ_ID, METER_DDT_RESP_ID, false, hexdecode("1003"));
        vTaskDelay(500 / portTICK_PERIOD_MS);

        // Reset command
        rz2->CommandDDT(METER_DDT_REQ_ID, METER_DDT_RESP_ID, false, hexdecode("3101500500"));
        vTaskDelay(500 / portTICK_PERIOD_MS);
        rz2->CommandDDT(METER_DDT_REQ_ID, METER_DDT_RESP_ID, false, hexdecode("3101500200"));
    }
    else
    {
        ESP_LOGI(TAG, "Zoe is not alive or is driving");
        writer->puts("Zoe is not alive or is driving");
    }
}

void OvmsVehicleRenaultZoePh2::CommandDdtHvacEnablePTC(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    ESP_LOGI(TAG, "Try to send HVAC PTC enable command");
    writer->puts("Sending PTC enable commands...");

    const int max_send_retries = 3;
    const int max_verify_retries = 3;
    const char *ptc_commands[3] = {"2E0533D400", "2E0534BC00", "2E05358C00"};
    const char *ptc_names[3] = {"PTC1", "PTC2", "PTC3"};

    // Step 1: Send all 3 PTC enable commands with 2s delays
    bool send_success[3] = {false, false, false};
    for (int i = 0; i < 3; i++)
    {
        int retry_count = 0;
        OvmsVehicle::vehicle_command_t result;

        do {
            result = rz2->CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode(ptc_commands[i]));
            if (result == OvmsVehicle::Success) {
                ESP_LOGI(TAG, "%s enable command sent successfully", ptc_names[i]);
                writer->printf("  %s enabled (0x%s)\n", ptc_names[i], ptc_commands[i]);
                send_success[i] = true;
                break;
            }
            retry_count++;
            if (retry_count < max_send_retries) {
                ESP_LOGW(TAG, "%s command failed, retrying (%d/%d)", ptc_names[i], retry_count, max_send_retries);
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        } while (retry_count < max_send_retries);

        if (!send_success[i]) {
            ESP_LOGE(TAG, "%s enable command failed after %d attempts", ptc_names[i], max_send_retries);
            writer->printf("  ERROR: %s enable command failed\n", ptc_names[i]);
        }

        // 2 second delay between PTC commands (except after last one)
        if (i < 2) {
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }

    // Step 2: Wait for ECU to process, then verify
    writer->puts("Verifying PTC state...");
    vTaskDelay(500 / portTICK_PERIOD_MS);

    uint8_t failed_ptc_mask = 0;
    bool verified = rz2->VerifyPTCState(true, 2000, &failed_ptc_mask);

    // Step 3: Retry failed PTCs if verification failed
    int verify_retry_count = 0;
    while (!verified && verify_retry_count < max_verify_retries)
    {
        verify_retry_count++;
        ESP_LOGW(TAG, "PTC verification failed (mask=0x%02X), retry %d/%d", failed_ptc_mask, verify_retry_count, max_verify_retries);
        writer->printf("  Verification failed, retrying failed PTCs (%d/%d)...\n", verify_retry_count, max_verify_retries);

        // Only retry the specific failed PTCs
        for (int i = 0; i < 3; i++)
        {
            if (failed_ptc_mask & (1 << i))
            {
                ESP_LOGI(TAG, "Retrying %s enable command", ptc_names[i]);
                OvmsVehicle::vehicle_command_t result = rz2->CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode(ptc_commands[i]));
                if (result == OvmsVehicle::Success) {
                    writer->printf("  %s retry sent\n", ptc_names[i]);
                }
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        }

        // Re-verify
        vTaskDelay(500 / portTICK_PERIOD_MS);
        verified = rz2->VerifyPTCState(true, 2000, &failed_ptc_mask);
    }

    // Step 4: Report final status
    if (verified) {
        ESP_LOGI(TAG, "All 3 PTCs enabled successfully");
        rz2->m_auto_ptc_currently_enabled = true;
        rz2->mt_auto_ptc_state->SetValue(true);
        writer->puts("✓ All 3 PTCs enabled successfully");
        if (verify_retry_count > 0) {
            writer->printf("  (verified after %d retry cycle%s)\n", verify_retry_count, verify_retry_count > 1 ? "s" : "");
        }
    } else {
        ESP_LOGE(TAG, "PTC enable verification failed after %d retries (mask=0x%02X)", verify_retry_count, failed_ptc_mask);
        writer->puts("ERROR: PTC verification failed:");
        for (int i = 0; i < 3; i++) {
            if (failed_ptc_mask & (1 << i)) {
                writer->printf("  %s: FAILED\n", ptc_names[i]);
            } else {
                writer->printf("  %s: OK\n", ptc_names[i]);
            }
        }
    }
}

void OvmsVehicleRenaultZoePh2::CommandDdtHvacDisablePTC(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc, const char *const *argv)
{
    OvmsVehicleRenaultZoePh2 *rz2 = (OvmsVehicleRenaultZoePh2 *)MyVehicleFactory.ActiveVehicle();

    ESP_LOGI(TAG, "Try to send HVAC PTC disable command");
    writer->puts("Sending PTC disable commands...");

    const int max_send_retries = 3;
    const int max_verify_retries = 3;
    const char *ptc_commands[3] = {"2E05335400", "2E05343C00", "2E05350C00"};
    const char *ptc_names[3] = {"PTC1", "PTC2", "PTC3"};

    // Step 1: Send all 3 PTC disable commands with 2s delays
    bool send_success[3] = {false, false, false};
    for (int i = 0; i < 3; i++)
    {
        int retry_count = 0;
        OvmsVehicle::vehicle_command_t result;

        do {
            result = rz2->CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode(ptc_commands[i]));
            if (result == OvmsVehicle::Success) {
                ESP_LOGI(TAG, "%s disable command sent successfully", ptc_names[i]);
                writer->printf("  %s disabled (0x%s)\n", ptc_names[i], ptc_commands[i]);
                send_success[i] = true;
                break;
            }
            retry_count++;
            if (retry_count < max_send_retries) {
                ESP_LOGW(TAG, "%s command failed, retrying (%d/%d)", ptc_names[i], retry_count, max_send_retries);
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        } while (retry_count < max_send_retries);

        if (!send_success[i]) {
            ESP_LOGE(TAG, "%s disable command failed after %d attempts", ptc_names[i], max_send_retries);
            writer->printf("  ERROR: %s disable command failed\n", ptc_names[i]);
        }

        // 2 second delay between PTC commands (except after last one)
        if (i < 2) {
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }

    // Step 2: Wait for ECU to process, then verify
    writer->puts("Verifying PTC state...");
    vTaskDelay(500 / portTICK_PERIOD_MS);

    uint8_t failed_ptc_mask = 0;
    bool verified = rz2->VerifyPTCState(false, 2000, &failed_ptc_mask);

    // Step 3: Retry failed PTCs if verification failed
    int verify_retry_count = 0;
    while (!verified && verify_retry_count < max_verify_retries)
    {
        verify_retry_count++;
        ESP_LOGW(TAG, "PTC verification failed (mask=0x%02X), retry %d/%d", failed_ptc_mask, verify_retry_count, max_verify_retries);
        writer->printf("  Verification failed, retrying failed PTCs (%d/%d)...\n", verify_retry_count, max_verify_retries);

        // Only retry the specific failed PTCs
        for (int i = 0; i < 3; i++)
        {
            if (failed_ptc_mask & (1 << i))
            {
                ESP_LOGI(TAG, "Retrying %s disable command", ptc_names[i]);
                OvmsVehicle::vehicle_command_t result = rz2->CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode(ptc_commands[i]));
                if (result == OvmsVehicle::Success) {
                    writer->printf("  %s retry sent\n", ptc_names[i]);
                }
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        }

        // Re-verify
        vTaskDelay(500 / portTICK_PERIOD_MS);
        verified = rz2->VerifyPTCState(false, 2000, &failed_ptc_mask);
    }

    // Step 4: Report final status
    if (verified) {
        ESP_LOGI(TAG, "All 3 PTCs disabled successfully");
        rz2->m_auto_ptc_currently_enabled = false;
        rz2->mt_auto_ptc_state->SetValue(false);
        writer->puts("✓ All 3 PTCs disabled successfully");
        if (verify_retry_count > 0) {
            writer->printf("  (verified after %d retry cycle%s)\n", verify_retry_count, verify_retry_count > 1 ? "s" : "");
        }
    } else {
        ESP_LOGE(TAG, "PTC disable verification failed after %d retries (mask=0x%02X)", verify_retry_count, failed_ptc_mask);
        writer->puts("ERROR: PTC verification failed:");
        for (int i = 0; i < 3; i++) {
            if (failed_ptc_mask & (1 << i)) {
                writer->printf("  %s: FAILED\n", ptc_names[i]);
            } else {
                writer->printf("  %s: OK\n", ptc_names[i]);
            }
        }
    }
}

// Internal PTC enable function for background use (without OvmsWriter)
// Returns true if successful, false otherwise
bool OvmsVehicleRenaultZoePh2::EnablePTCInternal()
{
    ESP_LOGI(TAG, "EnablePTCInternal: Sending PTC enable commands");

    const int max_send_retries = 3;
    const int max_verify_retries = 3;
    const char *ptc_commands[3] = {"2E0533D400", "2E0534BC00", "2E05358C00"};
    const char *ptc_names[3] = {"PTC1", "PTC2", "PTC3"};

    // Step 1: Send all 3 PTC enable commands with 2s delays
    for (int i = 0; i < 3; i++)
    {
        int retry_count = 0;
        OvmsVehicle::vehicle_command_t result;

        do {
            result = CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode(ptc_commands[i]));
            if (result == OvmsVehicle::Success) {
                ESP_LOGI(TAG, "EnablePTCInternal: %s enable command sent successfully", ptc_names[i]);
                break;
            }
            retry_count++;
            if (retry_count < max_send_retries) {
                ESP_LOGW(TAG, "EnablePTCInternal: %s command failed, retrying (%d/%d)", ptc_names[i], retry_count, max_send_retries);
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        } while (retry_count < max_send_retries);

        if (result != OvmsVehicle::Success) {
            ESP_LOGE(TAG, "EnablePTCInternal: %s enable command failed after %d attempts", ptc_names[i], max_send_retries);
        }

        // 2 second delay between PTC commands (except after last one)
        if (i < 2) {
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }

    // Step 2: Wait for ECU to process, then verify
    vTaskDelay(500 / portTICK_PERIOD_MS);

    uint8_t failed_ptc_mask = 0;
    bool verified = VerifyPTCState(true, 2000, &failed_ptc_mask);

    // Step 3: Retry failed PTCs if verification failed
    int verify_retry_count = 0;
    while (!verified && verify_retry_count < max_verify_retries)
    {
        verify_retry_count++;
        ESP_LOGW(TAG, "EnablePTCInternal: Verification failed (mask=0x%02X), retry %d/%d", failed_ptc_mask, verify_retry_count, max_verify_retries);

        // Only retry the specific failed PTCs
        for (int i = 0; i < 3; i++)
        {
            if (failed_ptc_mask & (1 << i))
            {
                ESP_LOGI(TAG, "EnablePTCInternal: Retrying %s enable command", ptc_names[i]);
                CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode(ptc_commands[i]));
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        }

        // Re-verify
        vTaskDelay(500 / portTICK_PERIOD_MS);
        verified = VerifyPTCState(true, 2000, &failed_ptc_mask);
    }

    // Step 4: Log final status
    if (verified) {
        ESP_LOGI(TAG, "EnablePTCInternal: All 3 PTCs enabled successfully");
        m_auto_ptc_currently_enabled = true;
        mt_auto_ptc_state->SetValue(true);
        if (verify_retry_count > 0) {
            ESP_LOGI(TAG, "EnablePTCInternal: Verified after %d retry cycle(s)", verify_retry_count);
        }
        return true;
    } else {
        ESP_LOGE(TAG, "EnablePTCInternal: Verification failed after %d retries (mask=0x%02X)", verify_retry_count, failed_ptc_mask);
        for (int i = 0; i < 3; i++) {
            if (failed_ptc_mask & (1 << i)) {
                ESP_LOGE(TAG, "EnablePTCInternal: %s FAILED", ptc_names[i]);
            }
        }
        return false;
    }
}

// Internal PTC disable function for background use (without OvmsWriter)
// Returns true if successful, false otherwise
bool OvmsVehicleRenaultZoePh2::DisablePTCInternal()
{
    ESP_LOGI(TAG, "DisablePTCInternal: Sending PTC disable commands");

    const int max_send_retries = 3;
    const int max_verify_retries = 3;
    const char *ptc_commands[3] = {"2E05335400", "2E05343C00", "2E05350C00"};
    const char *ptc_names[3] = {"PTC1", "PTC2", "PTC3"};

    // Step 1: Send all 3 PTC disable commands with 2s delays
    for (int i = 0; i < 3; i++)
    {
        int retry_count = 0;
        OvmsVehicle::vehicle_command_t result;

        do {
            result = CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode(ptc_commands[i]));
            if (result == OvmsVehicle::Success) {
                ESP_LOGI(TAG, "DisablePTCInternal: %s disable command sent successfully", ptc_names[i]);
                break;
            }
            retry_count++;
            if (retry_count < max_send_retries) {
                ESP_LOGW(TAG, "DisablePTCInternal: %s command failed, retrying (%d/%d)", ptc_names[i], retry_count, max_send_retries);
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        } while (retry_count < max_send_retries);

        if (result != OvmsVehicle::Success) {
            ESP_LOGE(TAG, "DisablePTCInternal: %s disable command failed after %d attempts", ptc_names[i], max_send_retries);
        }

        // 2 second delay between PTC commands (except after last one)
        if (i < 2) {
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }

    // Step 2: Wait for ECU to process, then verify
    vTaskDelay(500 / portTICK_PERIOD_MS);

    uint8_t failed_ptc_mask = 0;
    bool verified = VerifyPTCState(false, 2000, &failed_ptc_mask);

    // Step 3: Retry failed PTCs if verification failed
    int verify_retry_count = 0;
    while (!verified && verify_retry_count < max_verify_retries)
    {
        verify_retry_count++;
        ESP_LOGW(TAG, "DisablePTCInternal: Verification failed (mask=0x%02X), retry %d/%d", failed_ptc_mask, verify_retry_count, max_verify_retries);

        // Only retry the specific failed PTCs
        for (int i = 0; i < 3; i++)
        {
            if (failed_ptc_mask & (1 << i))
            {
                ESP_LOGI(TAG, "DisablePTCInternal: Retrying %s disable command", ptc_names[i]);
                CommandDDT(BCM_DDT_REQ_ID, BCM_DDT_RESP_ID, false, hexdecode(ptc_commands[i]));
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        }

        // Re-verify
        vTaskDelay(500 / portTICK_PERIOD_MS);
        verified = VerifyPTCState(false, 2000, &failed_ptc_mask);
    }

    // Step 4: Log final status
    if (verified) {
        ESP_LOGI(TAG, "DisablePTCInternal: All 3 PTCs disabled successfully");
        m_auto_ptc_currently_enabled = false;
        mt_auto_ptc_state->SetValue(false);
        if (verify_retry_count > 0) {
            ESP_LOGI(TAG, "DisablePTCInternal: Verified after %d retry cycle(s)", verify_retry_count);
        }
        return true;
    } else {
        ESP_LOGE(TAG, "DisablePTCInternal: Verification failed after %d retries (mask=0x%02X)", verify_retry_count, failed_ptc_mask);
        for (int i = 0; i < 3; i++) {
            if (failed_ptc_mask & (1 << i)) {
                ESP_LOGE(TAG, "DisablePTCInternal: %s FAILED", ptc_names[i]);
            }
        }
        return false;
    }
}
