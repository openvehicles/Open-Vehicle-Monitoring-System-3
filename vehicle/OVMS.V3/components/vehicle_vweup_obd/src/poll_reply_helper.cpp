#include "ovms_log.h"
#include "poll_reply_helper.h"

static const char *TAG = "BytesToFloatConverter";

bool PollReplyHelper::AddNewData(uint16_t pid, uint8_t *data, uint8_t length, uint16_t remain)
{
    // When I have a different PID as last time OR
    // if I'm not waiting for more data from last time...
    if (LastPid != pid || LastRemain == 0)
    {
        // ...let's fill the data into the store from the beginning
        Store.clear();
    }
    Store.append((char *)data, length);

    LastPid = pid;
    LastRemain = remain;

    stringstream msg;
    msg << "Store[0-" << (Store.size() - 1) << "] =";
    msg << hex << setfill('0');
    for (int i = 0; i < Store.size(); i++)
    {
        msg << " ";
        // Have to cast to int as uint8_t doesn't work!!
        msg << uppercase << hex << setw(2) << static_cast<int>(Store[i]);
    }
    ESP_LOGV(TAG, "%s", msg.str().c_str());

    // TRUE when all data is here
    // FALSE when we have to wait
    return remain == 0;
}

bool PollReplyHelper::FromUint8(const std::string &info, float &value, uint8_t bytesToSkip /*= 0*/)
{
    if (Store.size() < (1 + bytesToSkip))
    {
        ESP_LOGE(TAG, "%s: Data length=%d is too short for FromUint8(skippedBytes=%u)", info.c_str(), Store.size(), bytesToSkip);
        value = NAN;
        return false;
    }

    value = static_cast<float>((uint8_t)Store[0 + bytesToSkip]);
    return true;
}

bool PollReplyHelper::FromUint16(const std::string &info, float &value, uint8_t bytesToSkip /*= 0*/)
{
    if (Store.size() < (2 + bytesToSkip))
    {
        ESP_LOGE(TAG, "%s: Data length=%d is too short for FromUint16(skippedBytes=%u)", info.c_str(), Store.size(), bytesToSkip);
        value = NAN;
        return false;
    }

    value = static_cast<float>((uint16_t)((Store[0 + bytesToSkip] << 8) + ((uint16_t)Store[1 + bytesToSkip])));
    return true;
}

bool PollReplyHelper::FromInt32(const std::string &info, float &value, uint8_t bytesToSkip /*= 0*/)
{
    if (Store.size() < (4 + bytesToSkip))
    {
        ESP_LOGE(TAG, "%s: Data length=%d is too short for FromInt32(skippedBytes=%u)", info.c_str(), Store.size(), bytesToSkip);
        value = NAN;
        return false;
    }

    value = static_cast<float>((int32_t)((Store[0 + bytesToSkip] << 24) + (Store[1 + bytesToSkip] << 16) + (Store[2 + bytesToSkip] << 8) + ((uint16_t)Store[3 + bytesToSkip])));
    return true;
}