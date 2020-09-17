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

bool ReadHexRange(const char* value, unsigned long& output1, unsigned long& output2)
{
    char* ptr = nullptr;
    errno = 0;
    output1 = strtoul(value, &ptr, 16);
    if (ptr == value || errno != 0 || (*ptr && *ptr != '-'))
    {
        return false;
    }
    if (!*ptr)
    {
        output2 = output1;
    }
    else
    {
        value = ptr + 1;
        output2 = strtoul(value, &ptr, 16);
        if (ptr == value || errno != 0 || *ptr)
        {
            return false;
        }
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

void scanStart(int, OvmsWriter* writer, OvmsCommand*, int argc, const char* const* argv)
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
    unsigned long bus = 0, ecu = 0, rxid_low = 0, rxid_high = 0, start = 0, end = 0;
    int timeout = 3;
    unsigned long polltype = VEHICLE_POLL_TYPE_OBDIIEXTENDED;
    bool valid = true, have_rxid = false;
    int argpos = 0;
    for (int i = 0; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            switch (argv[i][1])
            {
                case 'r':
                    if (!ReadHexRange(argv[i]+2, rxid_low, rxid_high) ||
                        rxid_low > 0x7ff || rxid_high > 0x7ff || rxid_high < rxid_low)
                    {
                        writer->printf("Error: Invalid RX ID range %s\n", argv[i]+2);
                        valid = false;
                    }
                    else
                    {
                        have_rxid = true;
                    }
                    break;
                case 't':
                    if (!ReadHexString(argv[i]+2, polltype) || polltype < 1 || polltype > 0xff)
                    {
                        writer->printf("Error: Invalid poll type %s\n", argv[i]+2);
                        valid = false;
                    }
                    break;
                case 'x':
                    timeout = atoi(argv[i]+2);
                    if (timeout < 1 || timeout > 10)
                    {
                        writer->printf("Error: Invalid timeout %s\n", argv[i]+2);
                        valid = false;
                    }
                    break;
                default:
                    writer->printf("Error: Invalid argument %s\n", argv[i]);
                    valid = false;
                    break;
            }
        }
        else
        {
            switch (++argpos)
            {
                case 1:
                    if (!ReadHexString(argv[i], bus) || bus < 1 || bus > 4)
                    {
                        writer->printf("Error: Invalid bus to scan %s\n", argv[i]);
                        valid = false;
                    }
                    break;
                case 2:
                    if (!ReadHexString(argv[i], ecu) || ecu <= 0 || ecu >= 0xfff)
                    {
                        writer->printf("Error: Invalid ECU Id to scan %s\n", argv[i]);
                        valid = false;
                    }
                    break;
                case 3:
                    if (!ReadHexString(argv[i], start) || start > 0xffff)
                    {
                        writer->printf("Error: Invalid Start PID to scan %s\n", argv[i]);
                        valid = false;
                    }
                    break;
                case 4:
                    if (!ReadHexString(argv[i], end) || end > 0xffff)
                    {
                        writer->printf("Error: Invalid End PID to scan %s\n", argv[i]);
                        valid = false;
                    }
                    break;
                default:
                    writer->printf("Error: Invalid argument %s\n", argv[i]);
                    valid = false;
                    break;
            }
        }
    }
    if (start > end)
    {
        writer->printf(
            "Error: Invalid Start PID %04x is after End PID %04x\n", start, end
        );
        valid = false;
    }
    if (POLL_TYPE_HAS_8BIT_PID(polltype) && end > 0xff)
    {
        writer->printf("Error: Poll type %x PID range is 00..ff\n");
        valid = false;
    }
    if (!valid)
    {
        return;
    }
    if (!have_rxid)
    {
        rxid_low = rxid_high = ecu + 8;
    }
    canbus* can = GetCan(bus);
    if (can == nullptr)
    {
        writer->puts("CAN not started in active mode, please start and try again");
        valid = false;
    }
    if (valid)
    {
        s_scanner = new OvmsReToolsPidScanner(can, ecu, rxid_low, rxid_high, polltype, start, end, timeout);
        writer->printf("Scan started: bus %d, ecu %x, rxid %x-%x, polltype %x, PID %x-%x, timeout %d seconds\n",
                       bus, ecu, rxid_low, rxid_high, polltype, start, end, timeout);
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
    writer->puts("Scan results:");
    s_scanner->Output(writer);
    delete s_scanner;
    s_scanner = nullptr;
    writer->puts("Scan stopped");
}

}  // anon namespace

OvmsReToolsPidScanner::OvmsReToolsPidScanner(
        canbus* bus, uint16_t ecu, uint16_t rxid_low, uint16_t rxid_high,
        uint8_t polltype, int start, int end, uint8_t timeout) :
    m_frameCallback(std::bind(
        &OvmsReToolsPidScanner::FrameCallback, this,
        std::placeholders::_1, std::placeholders::_2
    )),
    m_bus(bus),
    m_id(ecu),
    m_rxid_low(rxid_low),
    m_rxid_high(rxid_high),
    m_pollType(polltype),
    m_startPid(start),
    m_endPid(end),
    m_currentPid(end+1),
    m_ticker(0u),
    m_timeout(timeout),
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
        uint16_t rxid, pid;
        std::vector<uint8_t> data;
        std::tie(rxid, pid, data) = found;
        writer->printf("%03x[%03x]:%04x", m_id, rxid, pid);
        for (auto& byte : data)
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
    if (m_currentPid <= m_endPid && m_ticker - m_lastFrame > m_timeout)
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
        m_currentPid = m_endPid + 1;
    }
}

void OvmsReToolsPidScanner::SendNextFrame()
{
    if (m_currentPid == m_endPid)
    {
        ++m_currentPid;
        ESP_LOGI(TAG, "Scan of %x complete", m_id);
        return;
    }
    ++m_currentPid;

    CAN_frame_t sendFrame = {
        m_bus,
        &m_frameCallback,
        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
        m_id,
        0
    };

    if (POLL_TYPE_HAS_16BIT_PID(m_pollType))
    {
        sendFrame.data = { .u8 = {
            (ISOTP_FT_SINGLE << 4) + 3, m_pollType,
            static_cast<uint8_t>(m_currentPid >> 8),
            static_cast<uint8_t>(m_currentPid & 0xff)
        } };
    }
    else
    {
        sendFrame.data = { .u8 = {
            (ISOTP_FT_SINGLE << 4) + 2, m_pollType,
            static_cast<uint8_t>(m_currentPid & 0xff)
        } };
    }

    m_mfRemain = 0u;
    if (m_bus->Write(&sendFrame) == ESP_FAIL)
    {
        ESP_LOGE(TAG, "Error sending test frame to PID %x:%x", m_id, m_currentPid);
        m_currentPid = m_endPid + 1;
    }
    else
    {
        ESP_LOGV(TAG, "Sending test frame to PID %x:%x", m_id, m_currentPid);
        m_lastFrame = m_ticker;
    }
}

void OvmsReToolsPidScanner::IncomingPollFrame(const CAN_frame_t* frame)
{
    if (m_currentPid > m_endPid)
    {
        return;
    }

    uint8_t frameType = frame->data.u8[0] >> 4;
    uint8_t frameLength = frame->data.u8[0] & 0x0f;
    const uint8_t* data = &frame->data.u8[1];
    uint8_t dataLength = frameLength;

    if (frame->MsgID < m_rxid_low || frame->MsgID > m_rxid_high)
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
            std::copy(data, &data[dataLength], std::back_inserter(std::get<2>(m_found.back())));
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
    else if (dataLength == 3 && data[0] == UDS_RESP_TYPE_NRC)
    {
        // Invalid frame response
        SendNextFrame();
    }
    else if (dataLength > 3 && data[0] == m_pollType + 0x40)
    {
        // Success
        uint16_t responsePid;
        if (POLL_TYPE_HAS_16BIT_PID(m_pollType))
        {
            responsePid = data[1] << 8 | data[2];
        }
        else
        {
            responsePid = data[1];
        }
        if (responsePid == m_currentPid)
        {
            ESP_LOGD(
                TAG,
                "Success response from %x[%x]:%x length %d (0x%02x 0x%02x 0x%02x 0x%02x%s)",
                m_id, frame->MsgID, m_currentPid, frameLength - 3, data[3], data[4], data[5], data[6],
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
                    m_currentPid = m_endPid + 1;
                }
                else
                {
                    m_lastFrame = m_ticker;
                }
                std::vector<uint8_t> response;
                response.reserve(frameLength);
                std::copy(&data[3], &data[dataLength], std::back_inserter(response));
                OvmsMutexLock lock(&m_foundMutex);
                m_found.push_back(std::make_tuple(frame->MsgID, responsePid, std::move(response)));
            }
            else
            {
                OvmsMutexLock lock(&m_foundMutex);
                m_found.push_back(std::make_tuple(frame->MsgID,
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
        "<bus> <ecu> <start_pid> <end_pid> [-r<rxid>[-<rxid>]] [-t<poll_type>] [-x<timeout>]\n"
        "Give all values except bus and timeout hexadecimal. Options can be positioned anywhere.\n"
        "Default <rxid> is <ecu>+8, try 0-7ff if you don't know the responding ID.\n"
        "Default <poll_type> is 22 (ReadDataByIdentifier, 16 bit PID).\n"
        "Default <timeout> is 3 seconds.",
        4, 7
    );
    cmd_scan->RegisterCommand("status", "The status of the PID scan", &scanStatus);
    cmd_scan->RegisterCommand("stop", "Stop the current scan", &scanStop);
}
