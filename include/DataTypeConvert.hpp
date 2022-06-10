#include <string>

using namespace std;
#pragma once
class DataTypeConverter
{
private:
public:
    bool isBigEndian;
    void CheckBigEndian()
    {
        static int chk = 0x0201; // used to distinguish CPU's type (BigEndian or LittleEndian)
        isBigEndian = (0x01 != *(char *)(&chk));
    }
    DataTypeConverter() { CheckBigEndian(); }
    //大端存储方式
    //字符数组转成指定数据类型
    short ToInt16(const char *str);
    uint16_t ToUInt16(const char *str);
    int ToInt32(const char *str);
    float ToFloat(const char *str);
    uint32_t ToUInt32(const char *str);
    uint64_t ToLong64(const char *str);
    //指定数据类型转成对应的字符数组
    void ToInt16Buff(short num, char *buff);
    void ToUInt16Buff(uint16_t num, char *buff);
    void ToInt32Buff(int num, char *buff);
    void ToUInt32Buff(uint32_t num, char *buff);
    void ToFloatBuff(float num, char *buff);
    void ToLong64Buff(uint64_t num, char *buff);

    //小端存储方式
    //字符数组转成指定数据类型
    short ToInt16_m(const char *str);
    uint16_t ToUInt16_m(const char *str);
    int ToInt32_m(const char *str);
    float ToFloat_m(const char *str);
    uint32_t ToUInt32_m(const char *str);
    uint64_t ToLong64_m(const char *str);
    //指定数据类型转成对应的字符数组
    void ToInt16Buff_m(short num, char *buff);
    void ToUInt16Buff_m(uint16_t num, char *buff);
    void ToInt32Buff_m(int num, char *buff);
    void ToUInt32Buff_m(uint32_t num, char *buff);
    void ToFloatBuff_m(float num, char *buff);
    void ToLong64Buff_m(uint64_t num, char *buff);
};