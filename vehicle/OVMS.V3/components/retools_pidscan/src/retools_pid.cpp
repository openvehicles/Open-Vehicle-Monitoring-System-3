/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th September 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2020       Chris Staite
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

#include "ovms_log.h"
static const char *TAG = "re-pid";

#include "retools_pid.h"
#include "vehicle.h"

namespace {

bool ReadHexString(const char* value, unsigned long& output)
{
    char* ptr = nullptr;
    errno = 0;
    output = strtoul(value, &ptr, 16);
    if (ptr == value || errno != 0 || *ptr)
    {
        return false;
    }
    return true;
}

canbus* GetCan(int bus)
{
    char name[] = "canx";
    name[3] = '0' + bus;
    auto can = reinterpret_cast<canbus*>(MyPcpApp.FindDeviceByName(name));
    if (can != nullptr && (can->GetPowerMode() != On || can->m_mode != CAN_MODE_ACTIVE))
    {
        can = nullptr;
    }
    return can;
}

OvmsReToolsPidScanner* s_scanner = nullptr;

void scanStart(int, OvmsWriter* writer, OvmsCommand*, int, const char* const* argv)
{
    if (s_scanner != nullptr)
    {
        if (s_scanner->Complete())
        {
            delete s_scanner;
            s_scanner = nullptr;
        }
        else
        {
            writer->puts(
                "Error: Scan already in progress, stop it or wait for it to complete"
            );
            return;
        }
    }
    unsigned long bus, ecu, start, end;
    bool valid = true;
    if (!ReadHexString(argv[0], bus) || bus < 1 || bus > 4)
    {
        writer->printf("Error: Invalid bus to scan %s\n", argv[0]);
        valid = false;
    }
    if (!ReadHexString(argv[1], ecu) || ecu <= 0 || ecu >= 0xfff)
    {
        writer->printf("Error: Invalid ECU Id to scan %s\n", argv[1]);
        valid = false;
    }
    if (!ReadHexString(argv[2], start) || start <= 0 || start >= 0xffff)
    {
        writer->printf("Error: Invalid Start PID to scan %s\n", argv[2]);
        valid = false;
    }
    if (!ReadHexString(argv[3], end) || end <= 0 || end >= 0xffff)
    {
        writer->printf("Error: Invalid End PID to scan %s\n", argv[3]);
        valid = false;
    }
    if (start > end)
    {
        writer->printf(
            "Error: Invalid Start PID %04x is after End PID %04x\n", start, end
        );
        valid = false;
    }
    if (!valid)
    {
        return;
    }
    canbus* can = GetCan(bus);
    if (can == nullptr)
    {
        writer->puts("CAN not started in active mode, please start and try again");
        valid = false;
    }
    if (valid)
    {
        s_scanner = new OvmsReToolsPidScanner(can, ecu, start, end);
    }
}

void scanStatus(int, OvmsWriter* writer, OvmsCommand*, int, const char* const*)
{
    if (s_scanner == nullptr)
    {
        writer->puts("No scan running");
    }
    else if (s_scanner->Complete())
    {
        writer->puts("Scan complete");
    }
    else
    {
        writer->printf(
            "Scan running (%03x %04x-%04x): %04x\n",
            s_scanner->Ecu(), s_scanner->Start(), s_scanner->End(), s_scanner->Current()
        );
    }
    if (s_scanner != nullptr)
    {
        s_scanner->Output(writer);
    }
}

void scanStop(int, OvmsWriter* writer, OvmsCommand*, int, const char* const*)
{
    if (s_scanner == nullptr)
    {
        writer->puts("Error: No scan currently in progress");
        return;
    }
    delete s_scanner;
    s_scanner = nullptr;
    writer->puts("Scan stopped");
}

}  // anon namespace

OvmsReToolsPidScanner::OvmsReToolsPidScanner(
        canbus* bus, uint16_t ecu, uint16_t start, uint16_t end) :
    m_frameCallback(std::bind(
        &OvmsReToolsPidScanner::FrameCallback, this,
        std::placeholders::_1, std::placeholders::_2
    )),
    m_bus(bus),
    m_id(ecu),
    m_startPid(start),
    m_endPid(end),
    m_currentPid(end),
    m_ticker(0u),
    m_lastFrame(0u),
    m_mfRemain(0u),
    m_task(nullptr),
    m_rxqueue(nullptr),
    m_found(),
    m_foundMutex()
{
    m_rxqueue = xQueueCreate(20,sizeof(CAN_frame_t));
    xTaskCreatePinnedToCore(
        &OvmsReToolsPidScanner::Task, "OVMS RE PID", 4096, this, 5, &m_task, CORE(1)
    );
    MyCan.RegisterListener(m_rxqueue, true);
    m_currentPid = m_startPid - 1;
    MyEvents.RegisterEvent(
        TAG, "ticker.1",
        std::bind(
            &OvmsReToolsPidScanner::Ticker1, this,
            std::placeholders::_1, std::placeholders::_2
        )
    );
    SendNextFrame();
}

OvmsReToolsPidScanner::~OvmsReToolsPidScanner()
{
    if (m_rxqueue)
    {
        MyEvents.DeregisterEvent(TAG);
        MyCan.DeregisterListener(m_rxqueue);
        vQueueDelete(m_rxqueue);
        vTaskDelete(m_task);
    }
}

void OvmsReToolsPidScanner::Output(OvmsWriter* writer) const
{
    OvmsMutexLock lock(&m_foundMutex);
    for (auto& found : m_found)
    {
        writer->printf("%03x:%04x", m_id, found.first);
        for (auto& byte : found.second)
        {
            writer->printf(" %02x", byte);
        }
        writer->printf("\n");
    }
}

void OvmsReToolsPidScanner::Task(void *self)
{
    reinterpret_cast<OvmsReToolsPidScanner*>(self)->Task();
}

void OvmsReToolsPidScanner::Task()
{
    CAN_frame_t frame;
    while (1)
    {
        if (xQueueReceive(
                    m_rxqueue, &frame, static_cast<portTickType>(portMAX_DELAY)
                ) == pdTRUE)
        {
            if (frame.origin == m_bus)
            {
                IncomingPollFrame(&frame);
            }
        }
    }
}

void OvmsReToolsPidScanner::Ticker1(std::string, void*)
{
    ++m_ticker;
    if (m_currentPid != m_endPid && m_ticker - m_lastFrame > 10)
    {
        ESP_LOGE(TAG, "Frame response timeout for %x:%x", m_id, m_currentPid);
        SendNextFrame();
    }
}

void OvmsReToolsPidScanner::FrameCallback(const CAN_frame_t* frame, bool success)
{
    if (success == false)
    {
        ESP_LOGE(TAG, "Error sending the frame");
        m_currentPid = m_endPid;
    }
}

void OvmsReToolsPidScanner::SendNextFrame()
{
    if (m_currentPid == m_endPid)
    {
        ESP_LOGI(TAG, "Scan of %x complete", m_id);
        return;
    }
    ++m_currentPid;

    CAN_frame_t sendFrame = {
        m_bus,
        &m_frameCallback,
        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
        m_id,
        // Only support for 16-bit PIDs because that's all we use
        { .u8 = {
            (ISOTP_FT_SINGLE << 4) + 3, VEHICLE_POLL_TYPE_OBDIIEXTENDED,
            static_cast<uint8_t>(m_currentPid >> 8),
            static_cast<uint8_t>(m_currentPid & 0xff), 0, 0, 0, 0
        } }
    };
    m_mfRemain = 0u;
    if (m_bus->Write(&sendFrame) == ESP_FAIL)
    {
        ESP_LOGE(TAG, "Error sending test frame to PID %x:%x", m_id, m_currentPid);
        m_currentPid = m_endPid;
    }
    else
    {
        ESP_LOGV(TAG, "Sending test frame to PID %x:%x", m_id, m_currentPid);
        m_lastFrame = m_ticker;
    }
}

void OvmsReToolsPidScanner::IncomingPollFrame(const CAN_frame_t* frame)
{
    uint8_t frameType = frame->data.u8[0] >> 4;
    uint8_t frameLength = frame->data.u8[0] & 0x0f;
    const uint8_t* data = &frame->data.u8[1];
    uint8_t dataLength = frameLength;
    
    if (frame->MsgID != m_id + 0x8)
    {
        // Frame not for us
        return;
    }

    if (frameType == ISOTP_FT_SINGLE)
    {
        // All good
        m_mfRemain = 0;
    }
    else if (frameType == ISOTP_FT_FIRST)
    {
        frameLength = (frameLength << 8) | data[0];
        ++data;
        dataLength = (frameLength > 6 ? 6 : frameLength);
        m_mfRemain = frameLength - dataLength;
    }
    else if (frameType == ISOTP_FT_CONSECUTIVE)
    {
        dataLength = (m_mfRemain > 7 ? 7 : m_mfRemain);
        m_mfRemain -= dataLength;
    }
    else
    {
        // Don't support any other types
        ESP_LOGI(TAG, "Received unknown frame type %x", frameType);
        return;
    }

    if (frameType == ISOTP_FT_CONSECUTIVE)
    {
        {
            OvmsMutexLock lock(&m_foundMutex);
            std::copy(data, &data[dataLength], std::back_inserter(m_found.back().second));
        }
        if (m_mfRemain == 0u)
        {
            SendNextFrame();
        }
        else
        {
            m_lastFrame = m_ticker;
        }
    }
    else if (dataLength == 3 && data[0] == 0x7f)
    {
        // Invalid frame response
        SendNextFrame();
    }
    else if (dataLength > 3 && data[0] == VEHICLE_POLL_TYPE_OBDIIEXTENDED + 0x40)
    {
        // Success
        uint16_t responsePid = data[1] << 8 | data[2];
        if (responsePid == m_currentPid)
        {
            ESP_LOGD(
                TAG,
                "Success response from %x:%x length %d (0x%02x 0x%02x 0x%02x 0x%02x%s)",
                m_id, m_currentPid, frameLength - 3, data[3], data[4], data[5], data[6],
                (frameType == 0 ? "" : " ...")
            );
            if (frameType == ISOTP_FT_FIRST)
            {
                CAN_frame_t flowControl = {
                    m_bus,
                    &m_frameCallback,
                    { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
                    m_id,
                    { .u8 = { 0x30, 0, 25, 0, 0, 0, 0, 0 } }
                };
                if (m_bus->Write(&flowControl) == ESP_FAIL)
                {
                    ESP_LOGE(
                        TAG, "Error sending flow control frame to PID %x:%x",
                        m_id, m_currentPid
                    );
                    m_currentPid = m_endPid;
                }
                else
                {
                    m_lastFrame = m_ticker;
                }
                std::vector<uint8_t> response;
                response.reserve(frameLength);
                std::copy(&data[3], &data[dataLength], std::back_inserter(response));
                OvmsMutexLock lock(&m_foundMutex);
                m_found.push_back(std::make_pair(responsePid, std::move(response)));
            }
            else
            {
                OvmsMutexLock lock(&m_foundMutex);
                m_found.push_back(std::make_pair(
                    responsePid, std::vector<uint8_t>(&data[3], &data[dataLength])
                ));
            }
            if (m_mfRemain == 0u)
            {
                SendNextFrame();
            }
        }
    }
}

class OvmsReToolsPidScannerInit
  {
  public:
    OvmsReToolsPidScannerInit();
} OvmsReToolsPidScannerInit __attribute__ ((init_priority (8801)));

OvmsReToolsPidScannerInit::OvmsReToolsPidScannerInit()
{
    OvmsCommand* cmd_reobdii = MyCommandApp.FindCommandFullName("re obdii");
    if (cmd_reobdii == nullptr)
    {
        ESP_LOGE(TAG, "PID scan command depends on re command");
        return;
    }
    OvmsCommand* cmd_scan = cmd_reobdii->RegisterCommand("scan", "ECU PID scanning tool");
    cmd_scan->RegisterCommand(
        "start", "Scan PIDs on an ECU in a given range", &scanStart,
        "<bus> <ecu> <start_pid> <end_pid>", 4, 4
    );
    cmd_scan->RegisterCommand("status", "The status of the PID scan", &scanStatus);
    cmd_scan->RegisterCommand("stop", "Stop the current scan", &scanStop);
}
