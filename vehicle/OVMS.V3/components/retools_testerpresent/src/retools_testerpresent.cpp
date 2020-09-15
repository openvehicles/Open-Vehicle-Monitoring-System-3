/*
;    Project:       Open Vehicle Monitor System
;    Date:          15th September 2020
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
static const char *TAG = "re-tp";

#include "retools_testerpresent.h"
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

}  // anon namespace

OvmsReToolsTesterPresent::OvmsReToolsTesterPresent(
        canbus* bus, uint16_t ecu, int interval) :
    m_bus(bus),
    m_id(ecu),
    m_ticker(0u),
    m_interval(interval)
{ }

void OvmsReToolsTesterPresent::Ticker1()
{
    ++m_ticker;
    if ((m_ticker % m_interval) == 0)
    {
        SendTesterPresent();
    }
}

void OvmsReToolsTesterPresent::SendTesterPresent()
{
    CAN_frame_t testerFrame = {
        m_bus,
        nullptr,
        { .B = { 8, 0, CAN_no_RTR, CAN_frame_std, 0 } },
        m_id,
        { .u8 = {
            (ISOTP_FT_SINGLE<<4) + 2, VEHICLE_POLL_TYPE_TESTERPRESENT, 0, 0, 0, 0, 0, 0
        } }
    };
    if (m_bus->Write(&testerFrame) == ESP_FAIL)
    {
        ESP_LOGE(TAG, "Error sending tester present to %03x", m_id);
    }
}

class OvmsReToolsTesterPresentInit
{
  public:
    OvmsReToolsTesterPresentInit();
    ~OvmsReToolsTesterPresentInit();
    
    void Start();
    void Stop();

  private:
    static void Start(int, OvmsWriter*, OvmsCommand*, int, const char* const*);
    static void List(int, OvmsWriter*, OvmsCommand*, int, const char* const*);
    static void Stop(int, OvmsWriter*, OvmsCommand*, int, const char* const*);
    static void StopAll(int, OvmsWriter*, OvmsCommand*, int, const char* const*);

    void Ticker1(std::string, void*);

    bool m_registered;
    OvmsMutex m_testersMutex;
    std::vector<OvmsReToolsTesterPresent> m_testers;
} OvmsReToolsTesterPresentInitInstance __attribute__ ((init_priority (8801)));

OvmsReToolsTesterPresentInit::OvmsReToolsTesterPresentInit() :
    m_registered(false),
    m_testersMutex(),
    m_testers()
{
    OvmsCommand* cmd_reobdii = MyCommandApp.FindCommandFullName("re obdii");
    if (cmd_reobdii == nullptr)
    {
        ESP_LOGE(TAG, "Tester present command depends on re command");
        return;
    }
    OvmsCommand* cmd_tester = cmd_reobdii->RegisterCommand(
        "tester", "Tester present sender"
    );
    cmd_tester->RegisterCommand(
        "start", "Start sending a tester present signal to an ECU",
        &OvmsReToolsTesterPresentInit::Start,
        "<bus> <ecu> <interval>", 3, 3
    );
    cmd_tester->RegisterCommand(
        "list", "List current tester present signals",
        &OvmsReToolsTesterPresentInit::List
    );
    cmd_tester->RegisterCommand(
        "stop", "Stop tester present for an ECU", &OvmsReToolsTesterPresentInit::Stop,
        "<bus> <ecu>", 2, 2
    );
    cmd_tester->RegisterCommand(
        "stopall", "Stop tester present for all ECUs",
        &OvmsReToolsTesterPresentInit::StopAll
    );
}

void OvmsReToolsTesterPresentInit::Start()
{
    if (m_registered == true)
    {
        return;
    }
    ESP_LOGD(TAG, "Starting tester present ticker");
    MyEvents.RegisterEvent(
        TAG, "ticker.1",
        std::bind(
            &OvmsReToolsTesterPresentInit::Ticker1, this,
            std::placeholders::_1, std::placeholders::_2
        )
    );
    m_registered = true;
}

void OvmsReToolsTesterPresentInit::Stop()
{
    if (m_registered == false)
    {
        return;
    }
    ESP_LOGD(TAG, "Stopping tester present ticker");
    MyEvents.DeregisterEvent(TAG);
    m_registered = false;
}

OvmsReToolsTesterPresentInit::~OvmsReToolsTesterPresentInit()
{
    Stop();
}

void OvmsReToolsTesterPresentInit::Ticker1(std::string, void*)
{
    OvmsMutexLock lock(&OvmsReToolsTesterPresentInitInstance.m_testersMutex);
    for (auto& tester : OvmsReToolsTesterPresentInitInstance.m_testers)
    {
        tester.Ticker1();
    }
}

void OvmsReToolsTesterPresentInit::Start(
        int, OvmsWriter* writer, OvmsCommand*, int, const char* const* argv)
{
    unsigned long bus, ecu;
    bool valid = true;
    if (!ReadHexString(argv[0], bus) || bus < 1 || bus > 4)
    {
        writer->printf("Error: Invalid bus to scan %s", argv[0]);
        valid = false;
    }
    if (!ReadHexString(argv[1], ecu) || ecu <= 0 || ecu >= 0xfff)
    {
        writer->printf("Error: Invalid ECU Id to scan %s", argv[1]);
        valid = false;
    }
    int interval = atoi(argv[2]);
    if (interval <= 0)
    {
        writer->printf("Error: Invalid interval %s", argv[2]);
        valid = false;
    }
    if (!valid)
    {
        return;
    }
    canbus* can = GetCan(bus);
    if (can == nullptr)
    {
        writer->printf("CAN not started in active mode, please start and try again");
        valid = false;
    }
    if (valid)
    {
        OvmsMutexLock lock(&OvmsReToolsTesterPresentInitInstance.m_testersMutex);
        for (auto& tester : OvmsReToolsTesterPresentInitInstance.m_testers)
        {
            if (tester.Bus() == can && tester.Ecu() == ecu)
            {
                writer->printf("Already sending tester present to that ECU");
                return;
            }
        }
        if (OvmsReToolsTesterPresentInitInstance.m_testers.empty())
        {
            OvmsReToolsTesterPresentInitInstance.Start();
        }
        OvmsReToolsTesterPresentInitInstance.m_testers.push_back(
            OvmsReToolsTesterPresent(can, ecu, interval)
        );
    }
}

void OvmsReToolsTesterPresentInit::List(
        int, OvmsWriter* writer, OvmsCommand*, int, const char* const*)
{
    OvmsMutexLock lock(&OvmsReToolsTesterPresentInitInstance.m_testersMutex);
    for (auto& tester : OvmsReToolsTesterPresentInitInstance.m_testers)
    {
        writer->printf(
            "Tester present sending to %03x on bus %d every %d seconds",
            tester.Ecu(), tester.Bus()->m_busnumber + 1, tester.Interval()
        );
    }
}

void OvmsReToolsTesterPresentInit::Stop(
        int, OvmsWriter* writer, OvmsCommand*, int, const char* const* argv)
{
    unsigned long bus, ecu;
    bool valid = true;
    if (!ReadHexString(argv[0], bus) || bus < 1 || bus > 4)
    {
        writer->printf("Error: Invalid bus %s", argv[0]);
        valid = false;
    }
    if (!ReadHexString(argv[1], ecu) || ecu <= 0 || ecu >= 0xfff)
    {
        writer->printf("Error: Invalid ECU Id %s", argv[1]);
        valid = false;
    }
    if (!valid)
    {
        return;
    }

    OvmsMutexLock lock(&OvmsReToolsTesterPresentInitInstance.m_testersMutex);
    auto it = OvmsReToolsTesterPresentInitInstance.m_testers.begin();
    while (it != OvmsReToolsTesterPresentInitInstance.m_testers.end())
    {
        if (it->Bus()->m_busnumber + 1 == bus && it->Ecu() == ecu)
        {
            OvmsReToolsTesterPresentInitInstance.m_testers.erase(it);
            writer->puts("Stopped sending");
            break;
        }
        ++it;
    }
    if (it == OvmsReToolsTesterPresentInitInstance.m_testers.end())
    {
        writer->puts("Error: Not sending tester present to that");
    }
    if (OvmsReToolsTesterPresentInitInstance.m_testers.empty())
    {
        OvmsReToolsTesterPresentInitInstance.Stop();
    }
}

void OvmsReToolsTesterPresentInit::StopAll(
        int, OvmsWriter* writer, OvmsCommand*, int, const char* const*)
{
    OvmsMutexLock lock(&OvmsReToolsTesterPresentInitInstance.m_testersMutex);
    if (OvmsReToolsTesterPresentInitInstance.m_testers.empty())
    {
        writer->puts("Error: Not sending tester present to anything");
    }
    else
    {
        OvmsReToolsTesterPresentInitInstance.m_testers.clear();
        OvmsReToolsTesterPresentInitInstance.Stop();
        writer->puts("Stopped all");
    }
}
