#include <DataTypeConvert.hpp>
#include <stdlib.h>
#include <string.h>
using namespace std;

short DataTypeConverter::ToInt16(const char *str)
{
    short value = 0;
    void *pf;
    pf = &value;
    for (char i = 0; i < 2; i++)
    {
        *((unsigned char *)pf + i) = str[1 - i];
    }
    // value |= str[0];
    // value <<= 8;
    // value |= str[1];

    return value;
}
uint16_t DataTypeConverter::ToUInt16(const char *str)
{
    uint16_t value = 0;

    void *pf;
    pf = &value;
    for (char i = 0; i < 2; i++)
    {
        *((unsigned char *)pf + i) = str[1 - i];
    }
    // value |= str[0];
    // value <<= 8;
    // value |= str[1];
    return value;
}
int DataTypeConverter::ToInt32(const char *str)
{
    int value = 0;
    ;
    void *pf;
    pf = &value;
    for (char i = 0; i < 4; i++)
    {
        *((unsigned char *)pf + i) = str[3 - i];
    }
    return value;
}
uint32_t DataTypeConverter::ToUInt32(const char *str)
{
    uint32_t value = 0;
    ;
    void *pf;
    pf = &value;
    for (char i = 0; i < 4; i++)
    {
        *((unsigned char *)pf + i) = str[3 - i];
    }
    return value;
}
float DataTypeConverter::ToFloat(const char *str)
{
    float floatVariable;
    void *pf;
    pf = &floatVariable;
    for (char i = 0; i < 4; i++)
    {
        *((unsigned char *)pf + i) = str[i];
    }
    return floatVariable;
}

short DataTypeConverter::ToInt16_m(const char *str)
{
    short value = 0;
    void *pf;
    pf = &value;
    for (char i = 0; i < 2; i++)
    {
        *((unsigned char *)pf + i) = str[i];
    }
    // value |= str[1];
    // value <<= 8;
    // value |= str[0];
    return value;
}
uint16_t DataTypeConverter::ToUInt16_m(const char *str)
{
    uint16_t value = 0;

    void *pf;
    pf = &value;
    for (char i = 0; i < 2; i++)
    {
        *((unsigned char *)pf + i) = str[i];
    }
    // value |= str[1];
    // value <<= 8;
    // value |= str[0];
    return value;
}
int DataTypeConverter::ToInt32_m(const char *str)
{
    int value = 0;
    ;
    void *pf;
    pf = &value;
    for (char i = 0; i < 4; i++)
    {
        *((unsigned char *)pf + i) = str[i];
    }
    return value;
}
uint32_t DataTypeConverter::ToUInt32_m(const char *str)
{
    uint32_t value = 0;
    void *pf;
    pf = &value;
    for (char i = 0; i < 4; i++)
    {
        *((unsigned char *)pf + i) = str[i];
    }
    return value;
}
float DataTypeConverter::ToFloat_m(const char *str)
{
    float floatVariable;
    void *pf;
    pf = &floatVariable;
    for (char i = 0; i < 4; i++)
    {
        *((unsigned char *)pf + i) = str[i];
    }
    return floatVariable;
}

void DataTypeConverter::ToInt16Buff(short num, char *buff)
{
    char IntValue[2] = {0};
    for (int j = 0; j < 2; j++)
    {
        IntValue[1 - j] |= num;
        num >>= 8;
    }
    memcpy(buff, IntValue, 2);
}

void DataTypeConverter::ToUInt16Buff(uint16_t num, char *buff)
{
    char UIntValue[2] = {0};
    for (int j = 0; j < 2; j++)
    {
        UIntValue[1 - j] |= num;
        num >>= 8;
    }
    memcpy(buff, UIntValue, 2);
}

void DataTypeConverter::ToInt32Buff(int num, char *buff)
{
    char DIntValue[4] = {0};
    for (int j = 0; j < 4; j++)
    {
        DIntValue[3 - j] |= num;
        num >>= 8;
    }
    memcpy(buff, DIntValue, 4);
}

void DataTypeConverter::ToUInt32Buff(uint32_t num, char *buff)
{
    char UDintValue[4] = {0};
    for (int j = 0; j < 4; j++)
    {
        UDintValue[3 - j] |= num;
        num >>= 8;
    }
    memcpy(buff, UDintValue, 4);
}

void DataTypeConverter::ToFloatBuff(float num, char *buff)
{
    char RealValue[4] = {0};
    void *pf;
    pf = &num;
    for (int j = 0; j < 4; j++)
    {
        *((unsigned char *)pf + j) = RealValue[j];
    }
    memcpy(buff, RealValue, 4);
}

void DataTypeConverter::ToInt16Buff_m(short num, char *buff)
{
    char IntValue[2] = {0};
    for (int j = 0; j < 2; j++)
    {
        IntValue[j] |= num;
        num >>= 8;
    }
    memcpy(buff, IntValue, 2);
}

void DataTypeConverter::ToUInt16Buff_m(uint16_t num, char *buff)
{
    char UIntValue[2] = {0};
    for (int j = 0; j < 2; j++)
    {
        UIntValue[j] |= num;
        num >>= 8;
    }
    memcpy(buff, UIntValue, 2);
}

void DataTypeConverter::ToInt32Buff_m(int num, char *buff)
{
    char DIntValue[4] = {0};
    for (int j = 0; j < 4; j++)
    {
        DIntValue[j] |= num;
        num >>= 8;
    }
    memcpy(buff, DIntValue, 4);
}

void DataTypeConverter::ToUInt32Buff_m(uint32_t num, char *buff)
{
    char UDintValue[4] = {0};
    for (int j = 0; j < 4; j++)
    {
        UDintValue[j] |= num;
        num >>= 8;
    }
    memcpy(buff, UDintValue, 4);
}

void DataTypeConverter::ToFloatBuff_m(float num, char *buff)
{
    char RealValue[4] = {0};
    void *pf;
    pf = &num;
    for (int j = 0; j < 4; j++)
    {
        *((unsigned char *)pf + j) = RealValue[j];
    }
    memcpy(buff, RealValue, 4);
}