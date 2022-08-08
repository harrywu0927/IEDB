#include <DataTypeConvert.hpp>
#include <stdlib.h>
#include <string.h>
using namespace std;

//将大端存储方式的字符数组转换为指定类型的数值
/**
 * @brief 将大端存储方式的字符数组转换为2字节有符号整数
 *
 * @param str
 * @return short
 */
short DataTypeConverter::ToInt16(void *str)
{
    short value = 0;
    if (this->isBigEndian)
    {
        value = *((short *)str);
    }
    else
    {
        value = __builtin_bswap16(*(short *)str);
    }
    return value;
    // void *pf;
    // pf = &value;
    // for (char i = 0; i < 2; i++)
    // {
    //     *((unsigned char *)pf + i) = str[1 - i];
    // }
    // value |= str[0];
    // value <<= 8;
    // value |= str[1];

    // return value;
}

/**
 * @brief 将大端存储方式的字符数组转换为2字节无符号整数
 *
 * @param str
 * @return short
 */
uint16_t DataTypeConverter::ToUInt16(void *str)
{
    uint16_t value = 0;
    if (this->isBigEndian)
    {
        value = *((unsigned short *)str);
    }
    else
    {
        value = __builtin_bswap16(*(unsigned short *)str);
    }
    return value;
    // void *pf;
    // pf = &value;
    // for (char i = 0; i < 2; i++)
    // {
    //     *((unsigned char *)pf + i) = str[1 - i];
    // }
    // value |= str[0];
    // value <<= 8;
    // value |= str[1];
    // return value;
}

/**
 * @brief 将大端存储方式的字符数组转换为4字节有符号整数
 *
 * @param str
 * @return short
 */
int DataTypeConverter::ToInt32(void *str)
{
    int value = 0;
    if (this->isBigEndian)
    {
        value = *((int *)str);
    }
    else
    {
        value = __builtin_bswap32(*(int *)str);
    }
    return value;
    // void *pf;
    // pf = &value;
    // for (char i = 0; i < 4; i++)
    // {
    //     *((unsigned char *)pf + i) = str[3 - i];
    // }
    // return value;
}

/**
 * @brief 将大端存储方式的字符数组转换为4字节无符号整数
 *
 * @param str
 * @return short
 */
uint32_t DataTypeConverter::ToUInt32(void *str)
{
    uint32_t value = 0;
    if (this->isBigEndian)
    {
        value = *((uint32_t *)str);
    }
    else
    {
        value = __builtin_bswap32(*(uint32_t *)str);
    }
    return value;
    // void *pf;
    // pf = &value;
    // for (char i = 0; i < 4; i++)
    // {
    //     *((unsigned char *)pf + i) = str[3 - i];
    // }
    // return value;
}

/**
 * @brief 将大端存储方式的字符数组转换为4字节浮点数
 *
 * @param str
 * @return short
 */
float DataTypeConverter::ToFloat(void *str)
{
    float floatVariable;
    if (isBigEndian)
    {
        floatVariable = *((float *)str);
        return floatVariable;
    }
    // else
    // {
    //     floatVariable = __builtin_bswap32(*(float *)str);
    // }
    // return floatVariable;
    void *pf;
    pf = &floatVariable;
    for (char i = 0; i < 4; i++)
    {
        *((unsigned char *)pf + i) = ((unsigned char *)str)[3 - i];
    }
    return floatVariable;
}

/**
 * @brief 将大端存储方式的字符数组转换为8字节无符号整数
 *
 * @param str
 * @return short
 */
uint64_t DataTypeConverter::ToLong64(void *str)
{
    uint64_t value = 0;
    if (isBigEndian)
    {
        value = *((uint64_t *)str);
    }
    else
    {
        value = __builtin_bswap64(*(uint64_t *)str);
    }
    return value;
    // void *pf;
    // pf = &value;
    // for (char i = 0; i < 8; i++)
    // {
    //     *((unsigned char *)pf + i) = str[7 - i];
    // }
    // return value;
}

//将小端存储方式的字符数组转换为指定类型数值
/**
 * @brief 将小端存储方式的字符数组转换为2字节有符号整数
 *
 * @param str
 * @return short
 */
short DataTypeConverter::ToInt16_m(void *str)
{
    short value = 0;
    if (!isBigEndian)
    {
        value = *((short *)str);
    }
    else
    {
        value = __builtin_bswap16(*(short *)str);
    }
    return value;
    // void *pf;
    // pf = &value;
    // for (char i = 0; i < 2; i++)
    // {
    //     *((unsigned char *)pf + i) = str[i];
    // }
    // // value |= str[1];
    // // value <<= 8;
    // // value |= str[0];
    // return value;
}

/**
 * @brief 将小端存储方式的字符数组转换为2字节无符号整数
 *
 * @param str
 * @return short
 */
uint16_t DataTypeConverter::ToUInt16_m(void *str)
{
    uint16_t value = 0;
    if (!isBigEndian)
    {
        value = *((uint16_t *)str);
    }
    else
    {
        value = __builtin_bswap16(*(uint16_t *)str);
    }
    return value;
    // void *pf;
    // pf = &value;
    // for (char i = 0; i < 2; i++)
    // {
    //     *((unsigned char *)pf + i) = str[i];
    // }
    // // value |= str[1];
    // // value <<= 8;
    // // value |= str[0];
    // return value;
}

/**
 * @brief 将小端存储方式的字符数组转换为4字节有符号整数
 *
 * @param str
 * @return short
 */
int DataTypeConverter::ToInt32_m(void *str)
{
    int value = 0;
    if (!isBigEndian)
    {
        value = *((int *)str);
    }
    else
    {
        value = __builtin_bswap32(*(int *)str);
    }
    return value;
    // void *pf;
    // pf = &value;
    // for (char i = 0; i < 4; i++)
    // {
    //     *((unsigned char *)pf + i) = str[i];
    // }
    // return value;
}

/**
 * @brief 将小端存储方式的字符数组转换为4字节无符号整数
 *
 * @param str
 * @return short
 */
uint32_t DataTypeConverter::ToUInt32_m(void *str)
{
    uint32_t value = 0;
    if (!isBigEndian)
    {
        value = *((uint32_t *)str);
    }
    else
    {
        value = __builtin_bswap32(*(uint32_t *)str);
    }
    return value;
    // void *pf;
    // pf = &value;
    // for (char i = 0; i < 4; i++)
    // {
    //     *((unsigned char *)pf + i) = str[i];
    // }
    // return value;
}

/**
 * @brief 将小端存储方式的字符数组转换为4字节浮点数
 *
 * @param str
 * @return short
 */
float DataTypeConverter::ToFloat_m(void *str)
{
    float floatVariable;
    if (!isBigEndian)
    {
        floatVariable = *((float *)str);
        return floatVariable;
    }
    // else
    // {
    //     floatVariable = __builtin_bswap32(*(float *)str);
    // }
    // return floatVariable;
    void *pf;
    pf = &floatVariable;
    for (char i = 0; i < 4; i++)
    {
        *((unsigned char *)pf + i) = ((unsigned char *)str)[i];
    }
    return floatVariable;
}

/**
 * @brief 将小端存储方式的字符数组转换为8字节无符号整数
 *
 * @param str
 * @return short
 */
uint64_t DataTypeConverter::ToLong64_m(void *str)
{
    uint64_t value = 0;
    if (!isBigEndian)
    {
        value = *((uint64_t *)str);
    }
    else
    {
        value = __builtin_bswap64(*(uint64_t *)str);
    }
    return value;
    // void *pf;
    // pf = &value;
    // for (char i = 0; i < 8; i++)
    // {
    //     *((unsigned char *)pf + i) = str[i];
    // }
    // return value;
}

//将指定类型的数值转换为大端存储方式的字符数组
/**
 * @brief 将2字节有符号整数转换为对应的大端存储字符数组
 *
 * @param num 2字节有符号整数
 * @param buff
 */
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

/**
 * @brief 将2字节无符号整数转换为对应的大端存储字符数组
 *
 * @param num 2字节无符号整数
 * @param buff
 */
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

/**
 * @brief 将4字节有符号整数转换为对应的大端存储字符数组
 *
 * @param num 4字节有符号整数
 * @param buff
 */
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

/**
 * @brief 将4字节无符号整数转换为对应的大端存储字符数组
 *
 * @param num 4字节无符号整数
 * @param buff
 */
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

/**
 * @brief 将4字节浮点数转换为对应的大端存储字符数组
 *
 * @param num 4字节浮点数
 * @param buff
 */
void DataTypeConverter::ToFloatBuff(float num, char *buff)
{
    char RealValue[4] = {0};

    void *pf;
    pf = &num;
    for (int j = 0; j < 4; j++)
    {
        RealValue[3 - j] = *((unsigned char *)pf + j);
    }
    memcpy(buff, RealValue, 4);
}

/**
 * @brief 将8字节无符号整数转换为对应的大端存储字符数组
 *
 * @param num 8字节有符号整数
 * @param buff
 */
void DataTypeConverter::ToLong64Buff(uint64_t num, char *buff)
{
    char LongValue[8] = {0};

    for (int j = 0; j < 8; j++)
    {
        LongValue[7 - j] |= num;
        num >>= 8;
    }
    memcpy(buff, LongValue, 8);
}

//将指定类型的数值转换为小端存储方式的字符数组
/**
 * @brief 将2字节有符号整数转换为对应的小端存储字符数组
 *
 * @param num 2字节有符号整数
 * @param buff
 */
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

/**
 * @brief 将2字节无符号整数转换为对应的小端存储字符数组
 *
 * @param num 2字节无符号整数
 * @param buff
 */
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

/**
 * @brief 将4字节有符号整数转换为对应的小端存储字符数组
 *
 * @param num 4字节有符号整数
 * @param buff
 */
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

/**
 * @brief 将4字节无符号整数转换为对应的小端存储字符数组
 *
 * @param num 4字节无符号整数
 * @param buff
 */
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

/**
 * @brief 将4字节浮点数转换为对应的小端存储字符数组
 *
 * @param num 4字节浮点数
 * @param buff
 */
void DataTypeConverter::ToFloatBuff_m(float num, char *buff)
{
    char RealValue[4] = {0};

    void *pf;
    pf = &num;
    for (int j = 0; j < 4; j++)
    {
        RealValue[j] = *((unsigned char *)pf + j);
    }
    memcpy(buff, RealValue, 4);
}

/**
 * @brief 将8字节无符号整数转换为对应的小端存储字符数组
 *
 * @param num 8字节无符号整数
 * @param buff
 */
void DataTypeConverter::ToLong64Buff_m(uint64_t num, char *buff)
{
    char LongValue[8] = {0};

    for (int j = 0; j < 8; j++)
    {
        LongValue[j] |= num;
        num >>= 8;
    }
    memcpy(buff, LongValue, 8);
}