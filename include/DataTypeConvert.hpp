//#include"STDFB_header.h"
#include <string>

using namespace std;

class DataTypeConverter
{
private:
    bool isBigEndian;

public:
    void CheckBigEndian()
    {
        static int chk = 0x0201; // used to distinguish CPU's type (BigEndian or LittleEndian)
        isBigEndian = (0x01 != *(char *)(&chk));
    }
    short ToInt16(const char *str);
    uint16_t ToUInt16(const char *str);
    int ToInt32(const char* str);
    float ToFloat(const char *str);
    uint32_t ToUInt32(const char *str);
};