#include "ovms_log.h"
#include "poll_reply_helper.h"
#include <cmath>
#include <cfloat>

static const char *TAG = "BytesToFloatConverter";

bool PollReplyHelper::AddNewData(uint16_t pid, uint8_t *data, uint8_t length, uint16_t remain)
{
    // When I have a different PID as last time OR
    // if I'm not waiting for more data from last time...
    if (LastPid != pid || LastRemain == 0)
    {
        // ...let's fill the data into the store from the beginning
        StoreLength = 0;
    }
    for (int i = StoreLength; i < DATA_CONVERTER_LENGTH_MAX; i++)
    {
        int dataIdx = i - StoreLength;
        Store[i] = dataIdx < length ? data[dataIdx] : 0;
    }

    LastPid = pid;
    LastRemain = remain;

    StoreLength += length;
    if (StoreLength > DATA_CONVERTER_LENGTH_MAX)
    {
        ESP_LOGE(TAG, "Data length is not supported");
        return false;
    }

    if (remain == 0)
    {
        ESP_LOGV(TAG, "Store[0-15]=%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", Store[0], Store[1], Store[2], Store[3], Store[4], Store[5], Store[6], Store[7], Store[8], Store[9], Store[10], Store[11], Store[12], Store[13], Store[14], Store[15]);
    }

    // TRUE when all data is here
    // FALSE when we have to wait
    return remain == 0;
}

bool PollReplyHelper::FromUint8(const std::string &info, float &value, uint8_t bytesToSkip /*= 0*/)
{
    if (StoreLength < (1 + bytesToSkip))
    {
        ESP_LOGE(TAG, "%s: Data length=%d is too short for FromUint8(skippedBytes=%u)", info.c_str(), StoreLength, bytesToSkip);
        value = NAN;
        return false;
    }

    value = static_cast<float>((uint8_t)Store[0 + bytesToSkip]);
    return true;
}

bool PollReplyHelper::FromUint16(const std::string &info, float &value, uint8_t bytesToSkip /*= 0*/)
{
    if (StoreLength < (2 + bytesToSkip))
    {
        ESP_LOGE(TAG, "%s: Data length=%d is too short for FromUint16(skippedBytes=%u)", info.c_str(), StoreLength, bytesToSkip);
        value = NAN;
        return false;
    }

    value = static_cast<float>((((uint16_t)Store[0 + bytesToSkip]) << 8) + (uint16_t)Store[1 + bytesToSkip]);
    return true;
}

// float BytesToFloatConverter::FromSint12()
// {
//     // Three octets (12 Bytes) with MSB being the sign
//     int16_t value = (((int16_t)Store[0]) << 8) + (int16_t)Store[1];
//     // Setting bits 11-15 to 1. Equals math equation "value - 2048".
//     value |= 0xF800;
//     return static_cast<float>(value);
// }