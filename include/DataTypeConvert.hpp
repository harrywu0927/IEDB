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
    short ToInt16(string str);
    uint16_t ToUInt16(string str);
    int ToInt32(string str);
    float ToFloat(char str[]);
    uint32_t ToUInt32(string str);
};