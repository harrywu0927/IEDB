#include "../include/DataTypeConvert.hpp"
#include <stdlib.h>
using namespace std;

short DataTypeConverter::ToInt16(string str)
{
    short value = 0;
    value |= str[0];
    value <<= 8;
    value |= str[1];
    return value;
}
uint16_t DataTypeConverter::ToUInt16(string str)
{
    uint16_t value = 0;
    value |= str[0];
    value <<= 8;
    value |= str[1];
    return value;
}
int DataTypeConverter::ToInt32(string str)
{
    int value = 0;
    for (int i = 0; i < 3; i++)
    {
        value |= str[i];
        value <<= 8;
    }
    value |= str[4];
    return value;
}
uint32_t DataTypeConverter::ToUInt32(string str)
{
    uint32_t value = 0;
    for (int i = 0; i < 3; i++)
    {
        value |= str[i];
        value <<= 8;
    }
    value |= str[4];
    return value;
}
float DataTypeConverter::ToFloat(char str[])
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