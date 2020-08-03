#ifndef __DATA_CONVERTER_H__
#define __DATA_CONVERTER_H__

#define DATA_CONVERTER_LENGTH_MAX 4

class BytesToFloatConverter
{
    public:
    BytesToFloatConverter(uint8_t *data, uint8_t length);
    float FromUint8();
    float FromSint12();
    float FromUint16();

    private:
    uint8_t storeLength = 0;
    uint8_t store[DATA_CONVERTER_LENGTH_MAX];
};

#endif //#ifndef __OBD_EUP_H__
