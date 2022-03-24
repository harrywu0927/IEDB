#include "../include/DataTypeConvert.hpp"
#include <stdlib.h>
using namespace std;

short DataTypeConverter::ToInt16(const char *str)
{
    short value = 0;
    value |= str[1];
    value <<= 8;
    value |= str[0];
    return value;
}
uint16_t DataTypeConverter::ToUInt16(const char *str)
{
    uint16_t value = 0;
    value |= str[1];
    value <<= 8;
    value |= str[0];
    return value;
}
int DataTypeConverter::ToInt32(const char *str)
{
    int value = 0;
    void *pf;
    pf = &value;
    for (char i = 0; i < 4; i++)
    {
        *((unsigned char *)pf + i) = str[3-i];
    }
    // for (int i = 3; i > 0; i--)
    // {
    //     value |= str[i];
    //     value <<= 8;
    // }
    // value |= str[0];
    return value;
}
uint32_t DataTypeConverter::ToUInt32(const char *str)
{
    uint32_t value = 0;
    void *pf;
    pf = &value;
    for (char i = 0; i < 4; i++)
    {
        *((unsigned char *)pf + i) = str[3-i];
    }
    // for (int i = 3; i > 0; i--)
    // {
    //     value |= str[i];
    //     value <<= 8;
    // }
    // value |= str[0];
    return value;
}
float DataTypeConverter::ToFloat(const char *str)
{
    //转换字节数组到float数据
    float floatVariable;
    void *pf;
    pf = &floatVariable;
    for (char i = 0; i < 4; i++)
    {
        *((unsigned char *)pf + i) = str[i];
    }
    return floatVariable;
}