#ifndef __DATA_CONVERTER_H__
#define __DATA_CONVERTER_H__

#include <string>

#define DATA_CONVERTER_LENGTH_MAX 16 // DANGER: When changed also change DEBUG output of Store[]

class PollReplyHelper
{
    public:    
    bool AddNewData(uint16_t pid, uint8_t *data, uint8_t length, uint16_t remain);

    float FromUint8(const std::string& info, uint8_t shiftLeft = 0);
    float FromUint16(const std::string& info, uint8_t shiftLeft = 0);
    //float FromSint12();

    private:
    uint8_t StoreLength = 0;
    uint8_t Store[DATA_CONVERTER_LENGTH_MAX];
    uint16_t LastPid = 0;
    uint16_t LastRemain = 0;
};

#endif //#ifndef __OBD_EUP_H__
