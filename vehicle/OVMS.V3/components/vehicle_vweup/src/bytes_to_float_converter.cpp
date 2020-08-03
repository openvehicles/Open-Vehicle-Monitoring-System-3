#include "ovms_log.h"
#include "bytes_to_float_converter.h"

static const char *TAG = "data.converter";

BytesToFloatConverter::BytesToFloatConverter(uint8_t *data, uint8_t length)
{
    if (length > DATA_CONVERTER_LENGTH_MAX)
    {
        ESP_LOGE(TAG, "Data length is not supported");
    }
    else
    {
        storeLength = length;
        for (int i = 0; i < DATA_CONVERTER_LENGTH_MAX; i++)
        {
            store[i] = i < length ? data[i] : 0;
        }
    }
}

float BytesToFloatConverter::FromUint8()
{
    return static_cast<float>((uint8_t)store[0]);
}

float BytesToFloatConverter::FromSint12()
{
    // Three octets (12 Bytes) with MSB being the sign
    int16_t value = (((int16_t)store[0]) << 8) + (int16_t)store[1];
    // Setting bits 11-15 to 1. Equals math equation "value - 2048".
    value |= 0xF800;
    return static_cast<float>(value);
}

float BytesToFloatConverter::FromUint16()
{
    return static_cast<float>((((uint16_t)store[0]) << 8) + (uint16_t)store[1]);
}