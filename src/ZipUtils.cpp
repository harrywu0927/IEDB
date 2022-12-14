#include "ZipUtils.h"

using namespace std;

/**
 * @brief 添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
 *
 * @param schemaPos 变量在模板的编号
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 */
void ZipUtils::addZipPos(int schemaPos, char *writebuff, long &writebuff_pos)
{
    DataTypeConverter converter;
    uint16_t posNum = schemaPos;
    char zipPosNum[ZIP_ZIPPOS_SIZE] = {0};
    converter.ToUInt16Buff(posNum, zipPosNum);
    memcpy(writebuff + writebuff_pos, zipPosNum, ZIP_ZIPPOS_SIZE);
    writebuff_pos += ZIP_ZIPPOS_SIZE;
}

/**
 * @brief 当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
 *
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 */
void ZipUtils::addEndFlag(char *writebuff, long &writebuff_pos)
{
    char isTsEnd[ZIP_ENDFLAG_SIZE] = {0};
    isTsEnd[0] = (char)-1;
    memcpy(writebuff + writebuff_pos, isTsEnd, ZIP_ENDFLAG_SIZE);
    writebuff_pos += ZIP_ENDFLAG_SIZE;
}

/**
 * @brief 添加上压缩标志
 *
 * @param ziptype 压缩标志
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 */
void ZipUtils::addZipType(ZipType ziptype, char *writebuff, long &writebuff_pos)
{
    switch (ziptype)
    {
    case ONLY_DATA:
    { //只有数据
        char zipType[ZIP_ZIPTYPE_SIZE] = {0};
        zipType[0] = (char)ONLY_DATA;
        memcpy(writebuff + writebuff_pos, zipType, ZIP_ZIPTYPE_SIZE);
        writebuff_pos += ZIP_ZIPTYPE_SIZE;
        break;
    }
    case ONLY_TIME:
    { //只有时间
        char zipType[ZIP_ZIPTYPE_SIZE] = {0};
        zipType[0] = (char)ONLY_TIME;
        memcpy(writebuff + writebuff_pos, zipType, ZIP_ZIPTYPE_SIZE);
        writebuff_pos += ZIP_ZIPTYPE_SIZE;
        break;
    }
    case DATA_AND_TIME:
    { //既有数据又有时间
        char zipType[ZIP_ZIPTYPE_SIZE] = {0};
        zipType[0] = (char)DATA_AND_TIME;
        memcpy(writebuff + writebuff_pos, zipType, ZIP_ZIPTYPE_SIZE);
        writebuff_pos += ZIP_ZIPTYPE_SIZE;
        break;
    }
    case TS_AND_ARRAY:
    { //既是时间序列又是数组
        char zipType[ZIP_ZIPTYPE_SIZE] = {0};
        zipType[0] = (char)TS_AND_ARRAY;
        memcpy(writebuff + writebuff_pos, zipType, ZIP_ZIPTYPE_SIZE);
        writebuff_pos += ZIP_ZIPTYPE_SIZE;
        break;
    }
    case ONLY_TS:
    { //只是时间序列
        char zipType[ZIP_ZIPTYPE_SIZE] = {0};
        zipType[0] = (char)ONLY_TS;
        memcpy(writebuff + writebuff_pos, zipType, ZIP_ZIPTYPE_SIZE);
        writebuff_pos += ZIP_ZIPTYPE_SIZE;
        break;
    }
    default:
        break;
    }
}

/**
 * @brief Ts类型压缩
 *
 * @param schemaPos 变量在模板的编号
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 * @param readbuff 读数据缓冲
 * @param readbuff_pos 读数据偏移量
 * @param DataType 数据类型
 * @return int
 */
int ZipUtils::IsTSZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType)
{
    DataTypeConverter converter;
    if (CurrentZipTemplate.schemas[schemaPos].second.isArray == true) //既是时间序列又是数组，则不压缩
    {
        // //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
        // ZipUtils::addZipPos(schemaPos, writebuff, writebuff_pos);
        // //既是时间序列又是数组
        // ZipUtils::addZipType(TS_AND_ARRAY, writebuff, writebuff_pos);
        // memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, (CurrentZipTemplate.schemas[schemaPos].second.arrayLen * CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE) * CurrentZipTemplate.schemas[schemaPos].second.tsLen);
        // writebuff_pos += (CurrentZipTemplate.schemas[schemaPos].second.arrayLen * CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE) * CurrentZipTemplate.schemas[schemaPos].second.tsLen;
        // readbuff_pos += (CurrentZipTemplate.schemas[schemaPos].second.arrayLen * CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE) * CurrentZipTemplate.schemas[schemaPos].second.tsLen;
        if (ZipUtils::IsTsAndArrayZip(schemaPos, writebuff, writebuff_pos, readbuff, readbuff_pos, DataType))
        {
            cout << "存在未知的数据类型，请检查模板或者更换功能块" << endl;
            return StatusCode::DATA_TYPE_MISMATCH_ERROR;
        }
    }
    else //只是时间序列类型
    {
        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
        ZipUtils::addZipPos(schemaPos, writebuff, writebuff_pos);
        //只是时间序列
        ZipUtils::addZipType(ONLY_TS, writebuff, writebuff_pos);
        //添加第一个采样的时间戳
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[schemaPos].second.valueBytes, ZIP_TIMESTAMP_SIZE);
        writebuff_pos += ZIP_TIMESTAMP_SIZE;

        switch (DataType)
        {
        case ValueType::UDINT: // UDINT
        {
            uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
            uint32 maxUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.maxValue);
            uint32 minUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.minValue);
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.tsLen; j++)
            {
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                uint32 currentUDintValue = converter.ToUInt32(value);

                if (currentUDintValue != standardUDintValue && (currentUDintValue < minUDintValue || currentUDintValue > maxUDintValue))
                {
                    //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                    ZipUtils::addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    ZipUtils::addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case ValueType::USINT: // USINT
        {
            char standardUSintValue = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
            char maxUSintValue = CurrentZipTemplate.schemas[schemaPos].second.maxValue[0];
            char minUSintValue = CurrentZipTemplate.schemas[schemaPos].second.minValue[0];
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.tsLen; j++)
            {
                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readbuff_pos, 1);
                char currentUSintValue = value[0];

                if (currentUSintValue != standardUSintValue && (currentUSintValue < minUSintValue || currentUSintValue > maxUSintValue))
                {
                    //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                    ZipUtils::addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    ZipUtils::addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case ValueType::UINT: // UINT
        {
            ushort standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
            ushort maxUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.maxValue);
            ushort minUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.minValue);
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.tsLen; j++)
            {
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, readbuff + readbuff_pos, 2);
                ushort currentUintValue = converter.ToUInt16(value);

                if (currentUintValue != standardUintValue && (currentUintValue < minUintValue || currentUintValue > maxUintValue))
                {
                    //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                    ZipUtils::addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    ZipUtils::addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case ValueType::SINT: // SINT
        {
            char standardSintValue = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
            char maxSintValue = CurrentZipTemplate.schemas[schemaPos].second.maxValue[0];
            char minSintValue = CurrentZipTemplate.schemas[schemaPos].second.minValue[0];
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.tsLen; j++)
            {
                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readbuff_pos, 1);
                char currentSintValue = value[0];

                if (currentSintValue != standardSintValue && (currentSintValue < minSintValue || currentSintValue > maxSintValue))
                {
                    //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                    ZipUtils::addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    ZipUtils::addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case ValueType::INT: // INT
        {
            short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
            short maxIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.maxValue);
            short minIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.minValue);
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.tsLen; j++)
            {
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, readbuff + readbuff_pos, 2);
                short currentIntValue = converter.ToInt16(value);

                if (currentIntValue != standardIntValue && (currentIntValue < minIntValue || currentIntValue > maxIntValue))
                {
                    //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                    ZipUtils::addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    ZipUtils::addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case ValueType::DINT: // DINT
        {
            int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
            int maxDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.maxValue);
            int minDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.minValue);
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.tsLen; j++)
            {
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                int currentDintValue = converter.ToInt32(value);

                if (currentDintValue != standardDintValue && (currentDintValue < minDintValue || currentDintValue > maxDintValue))
                {
                    //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                    ZipUtils::addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    ZipUtils::addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case ValueType::REAL: // REAL
        {
            float standardFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
            float maxFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.maxValue);
            float minFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.minValue);
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.tsLen; j++)
            {
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                float currentRealValue = converter.ToFloat(value);

                if (currentRealValue != standardFloatValue && (currentRealValue < minFloatValue || currentRealValue > maxFloatValue))
                {
                    //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                    ZipUtils::addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    ZipUtils::addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case ValueType::BOOL: // BOOL
        {
            char standardBool = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.tsLen; j++)
            {
                if (standardBool != readbuff[readbuff_pos])
                {
                    //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                    ZipUtils::addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    ZipUtils::addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        default:
        {
            return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            break;
        }
        }
    }
    return 0;
}

/**
 * @brief 数组类型压缩
 *
 * @param schemaPos 变量在模板的编号
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 * @param readbuff 读数据缓冲
 * @param readbuff_pos 读数据偏移量
 */
void ZipUtils::IsArrayZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos)
{
    DataTypeConverter converter;
    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
    ZipUtils::addZipPos(schemaPos, writebuff, writebuff_pos);

    if (CurrentZipTemplate.schemas[schemaPos].second.hasTime == true) //带有时间戳
    {
        //既有数据又有时间
        ZipUtils::addZipType(DATA_AND_TIME, writebuff, writebuff_pos);
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE);
        writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;
        readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;
    }
    else
    {
        //只有数据
        ZipUtils::addZipType(ONLY_DATA, writebuff, writebuff_pos);
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
        writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
        readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
    }
}

int ZipUtils::IsArrayZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType)
{
    DataTypeConverter converter;
    bool canZip = true;               //标记最后结果是否可压缩
    long readArrayPos = readbuff_pos; //用于遍历数组值

    //循环对比每个数组值，都符合要求才压缩
    switch (DataType)
    {
    case ValueType::BOOL:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            char standardBool = CurrentZipTemplate.schemas[schemaPos].second.standard[i][0];
            // 1个字节,暂定，根据后续情况可能进行更改
            char value[1] = {0};
            memcpy(value, readbuff + readArrayPos, 1);
            char currentBoolValue = value[0];
            if (standardBool != currentBoolValue)
            {
                canZip = false;
                break;
            }
            else
                readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
        }
        break;
    }
    case ValueType::USINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            char standardUSintValue = CurrentZipTemplate.schemas[schemaPos].second.standard[i][0];
            char maxUSintValue = CurrentZipTemplate.schemas[schemaPos].second.max[i][0];
            char minUSintValue = CurrentZipTemplate.schemas[schemaPos].second.min[i][0];
            // 1个字节,暂定，根据后续情况可能进行更改
            char value[1] = {0};
            memcpy(value, readbuff + readArrayPos, 1);
            char currentUSintValue = value[0];
            if (currentUSintValue != standardUSintValue && (currentUSintValue < minUSintValue || currentUSintValue > maxUSintValue))
            {
                canZip = false;
                break;
            }
            else
                readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
        }
        break;
    }
    case ValueType::UINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            ushort standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
            ushort maxUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.max[i]);
            ushort minUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.min[i]);
            // 2个字节,暂定，根据后续情况可能进行更改
            char value[2] = {0};
            memcpy(value, readbuff + readArrayPos, 2);
            ushort currentUintValue = converter.ToUInt16(value);
            if (currentUintValue != standardUintValue && (currentUintValue < minUintValue || currentUintValue > maxUintValue))
            {
                canZip = false;
                break;
            }
            else
                readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
        }
        break;
    }
    case ValueType::UDINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
            uint32 maxUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.max[i]);
            uint32 minUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.min[i]);
            // 4个字节,暂定，根据后续情况可能进行更改
            char value[4] = {0};
            memcpy(value, readbuff + readArrayPos, 4);
            uint32 currentUDintValue = converter.ToUInt32(value);
            if (currentUDintValue != standardUDintValue && (currentUDintValue < minUDintValue || currentUDintValue > maxUDintValue))
            {
                canZip = false;
                break;
            }
            else
                readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
        }
    }
    case ValueType::SINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            char standardSintValue = CurrentZipTemplate.schemas[schemaPos].second.standard[i][0];
            char maxSintValue = CurrentZipTemplate.schemas[schemaPos].second.max[i][0];
            char minSintValue = CurrentZipTemplate.schemas[schemaPos].second.min[i][0];
            // 1个字节,暂定，根据后续情况可能进行更改
            char value[1] = {0};
            memcpy(value, readbuff + readArrayPos, 1);
            char currentSintValue = value[0];
            if (currentSintValue != standardSintValue && (currentSintValue < minSintValue || currentSintValue > maxSintValue))
            {
                canZip = false;
                break;
            }
            else
                readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
        }
        break;
    }
    case ValueType::INT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
            short maxIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.max[i]);
            short minIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.min[i]);
            // 2个字节,暂定，根据后续情况可能进行更改
            char value[2] = {0};
            memcpy(value, readbuff + readArrayPos, 2);
            short currentIntValue = converter.ToInt16(value);
            if (currentIntValue != standardIntValue && (currentIntValue < minIntValue || currentIntValue > maxIntValue))
            {
                canZip = false;
                break;
            }
            else
                readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
        }
        break;
    }
    case ValueType::DINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
            int maxDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.max[i]);
            int minDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.min[i]);
            // 4个字节,暂定，根据后续情况可能进行更改
            char value[4] = {0};
            memcpy(value, readbuff + readArrayPos, 4);
            int currentDintValue = converter.ToInt32(value);
            if (currentDintValue != standardDintValue && (currentDintValue < minDintValue || currentDintValue > maxDintValue))
            {
                canZip = false;
                break;
            }
            else
                readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
        }
        break;
    }
    case ValueType::REAL:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            float standardFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
            float maxFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.max[i]);
            float minFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.min[i]);
            // 4个字节,暂定，根据后续情况可能进行更改
            char value[4] = {0};
            memcpy(value, readbuff + readArrayPos, 4);
            float currentRealValue = converter.ToFloat(value);
            if (currentRealValue != standardFloatValue && (currentRealValue < minFloatValue || currentRealValue > maxFloatValue))
            {
                canZip = false;
                break;
            }
            else
                readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
        }
        break;
    }
    default:
        return StatusCode::DATA_TYPE_MISMATCH_ERROR;
    }

    //根据上面的结果判断是否可压缩
    if (canZip)
    {
        if (CurrentZipTemplate.schemas[schemaPos].second.hasTime == true) //带有时间戳
        {
            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
            ZipUtils::addZipPos(schemaPos, writebuff, writebuff_pos);
            //只有时间
            ZipUtils::addZipType(ONLY_TIME, writebuff, writebuff_pos);
            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen, ZIP_TIMESTAMP_SIZE);
            writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;
            readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;
        }
        else
        {
            writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
            readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
        }
    }
    else
    {
        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
        ZipUtils::addZipPos(schemaPos, writebuff, writebuff_pos);
        if (CurrentZipTemplate.schemas[schemaPos].second.hasTime == true) //带有时间戳
        {
            //既有数据又有时间
            ZipUtils::addZipType(DATA_AND_TIME, writebuff, writebuff_pos);
            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE);
            writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;
            readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;
        }
        else
        {
            //只有数据
            ZipUtils::addZipType(ONLY_DATA, writebuff, writebuff_pos);
            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
            writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
            readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
        }
    }
    return 0;
}

int ZipUtils::IsTsAndArrayZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType)
{
    DataTypeConverter converter;
    long readArrayPos = readbuff_pos;
    bool canZip = true;

    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
    ZipUtils::addZipPos(schemaPos, writebuff, writebuff_pos);
    //既是时间序列又是数组
    ZipUtils::addZipType(TS_AND_ARRAY, writebuff, writebuff_pos);

    //添加第一个采样的时间戳
    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen, ZIP_TIMESTAMP_SIZE);
    writebuff_pos += ZIP_TIMESTAMP_SIZE;

    switch (DataType)
    {
    case ValueType::BOOL:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.tsLen; i++)
        {
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; j++)
            {
                char standardBool = CurrentZipTemplate.schemas[schemaPos].second.standard[j][0];
                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readArrayPos, 1);
                char currentBoolValue = value[0];
                if (standardBool != currentBoolValue)
                {
                    canZip = false;
                    break;
                }
                else
                    readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
            }

            if (!canZip)
            {
                //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                ZipUtils::addZipPos(i, writebuff, writebuff_pos);
                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
            }
            readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;

            //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
            if (i == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
            {
                ZipUtils::addEndFlag(writebuff, writebuff_pos);
            }
        }
        break;
    }
    case ValueType::USINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.tsLen; i++)
        {
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; j++)
            {
                char standardUSintValue = CurrentZipTemplate.schemas[schemaPos].second.standard[j][0];
                char maxUSintValue = CurrentZipTemplate.schemas[schemaPos].second.max[j][0];
                char minUSintValue = CurrentZipTemplate.schemas[schemaPos].second.min[j][0];
                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readArrayPos, 1);
                char currentUSintValue = value[0];
                if (currentUSintValue != standardUSintValue && (currentUSintValue < minUSintValue || currentUSintValue > maxUSintValue))
                {
                    canZip = false;
                    break;
                }
                else
                    readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
            }

            if (!canZip)
            {
                //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                ZipUtils::addZipPos(i, writebuff, writebuff_pos);
                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
            }
            readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;

            //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
            if (i == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
            {
                ZipUtils::addEndFlag(writebuff, writebuff_pos);
            }
        }
        break;
    }
    case ValueType::UINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.tsLen; i++)
        {
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; j++)
            {
                ushort standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standard[j]);
                ushort maxUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.max[j]);
                ushort minUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.min[j]);
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, readbuff + readArrayPos, 2);
                ushort currentUintValue = converter.ToUInt16(value);
                if (currentUintValue != standardUintValue && (currentUintValue < minUintValue || currentUintValue > maxUintValue))
                {
                    canZip = false;
                    break;
                }
                else
                    readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
            }

            if (!canZip)
            {
                //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                ZipUtils::addZipPos(i, writebuff, writebuff_pos);
                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
            }
            readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;

            //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
            if (i == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
            {
                ZipUtils::addEndFlag(writebuff, writebuff_pos);
            }
        }
        break;
    }
    case ValueType::UDINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.tsLen; i++)
        {
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; j++)
            {
                uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standard[j]);
                uint32 maxUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.max[j]);
                uint32 minUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.min[j]);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readArrayPos, 4);
                uint32 currentUDintValue = converter.ToUInt32(value);
                if (currentUDintValue != standardUDintValue && (currentUDintValue < minUDintValue || currentUDintValue > maxUDintValue))
                {
                    canZip = false;
                    break;
                }
                else
                    readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
            }

            if (!canZip)
            {
                //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                ZipUtils::addZipPos(i, writebuff, writebuff_pos);
                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
            }
            readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;

            //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
            if (i == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
            {
                ZipUtils::addEndFlag(writebuff, writebuff_pos);
            }
        }
        break;
    }
    case ValueType::SINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.tsLen; i++)
        {
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; j++)
            {
                char standardSintValue = CurrentZipTemplate.schemas[schemaPos].second.standard[j][0];
                char maxSintValue = CurrentZipTemplate.schemas[schemaPos].second.max[j][0];
                char minSintValue = CurrentZipTemplate.schemas[schemaPos].second.min[j][0];
                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readArrayPos, 1);
                char currentSintValue = value[0];
                if (currentSintValue != standardSintValue && (currentSintValue < minSintValue || currentSintValue > maxSintValue))
                {
                    canZip = false;
                    break;
                }
                else
                    readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
            }

            if (!canZip)
            {
                //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                ZipUtils::addZipPos(i, writebuff, writebuff_pos);
                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
            }
            readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;

            //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
            if (i == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
            {
                ZipUtils::addEndFlag(writebuff, writebuff_pos);
            }
        }
        break;
    }
    case ValueType::INT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.tsLen; i++)
        {
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; j++)
            {
                short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standard[j]);
                short maxIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.max[j]);
                short minIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.min[j]);
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, readbuff + readArrayPos, 2);
                short currentIntValue = converter.ToInt16(value);
                if (currentIntValue != standardIntValue && (currentIntValue < minIntValue || currentIntValue > maxIntValue))
                {
                    canZip = false;
                    break;
                }
                else
                    readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
            }

            if (!canZip)
            {
                //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                ZipUtils::addZipPos(i, writebuff, writebuff_pos);
                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
            }
            readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;

            //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
            if (i == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
            {
                ZipUtils::addEndFlag(writebuff, writebuff_pos);
            }
        }
        break;
    }
    case ValueType::DINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.tsLen; i++)
        {
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; j++)
            {
                int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standard[j]);
                int maxDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.max[j]);
                int minDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.min[j]);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readArrayPos, 4);
                int currentDintValue = converter.ToInt32(value);
                if (currentDintValue != standardDintValue && (currentDintValue < minDintValue || currentDintValue > maxDintValue))
                {
                    canZip = false;
                    break;
                }
                else
                    readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
            }

            if (!canZip)
            {
                //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                ZipUtils::addZipPos(i, writebuff, writebuff_pos);
                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
            }
            readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;

            //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
            if (i == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
            {
                ZipUtils::addEndFlag(writebuff, writebuff_pos);
            }
        }
        break;
    }
    case ValueType::REAL:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.tsLen; i++)
        {
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; j++)
            {
                float standardFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.standard[j]);
                float maxFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.max[j]);
                float minFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.min[j]);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readArrayPos, 4);
                float currentRealValue = converter.ToFloat(value);
                if (currentRealValue != standardFloatValue && (currentRealValue < minFloatValue || currentRealValue > maxFloatValue))
                {
                    canZip = false;
                    break;
                }
                else
                    readArrayPos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
            }

            if (!canZip)
            {
                //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                ZipUtils::addZipPos(i, writebuff, writebuff_pos);
                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
            }
            readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;

            //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
            if (i == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
            {
                ZipUtils::addEndFlag(writebuff, writebuff_pos);
            }
        }
        break;
    }
    default:
    {
        return StatusCode::DATA_TYPE_MISMATCH_ERROR;
        break;
    }
    }
    return 0;
}

/**
 * @brief 根据数据类型添加标准值
 *
 * @param schemaPos 变量在模板的编号
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 * @param DataType 数据类型
 */
void ZipUtils::addStandardValue(int schemaPos, char *writebuff, long &writebuff_pos, ValueType::ValueType DataType)
{
    DataTypeConverter converter;
    //添加上标准值到writebuff
    switch (DataType)
    {
    case ValueType::UDINT: // UDINT
    {
        uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
        char boolTime[4] = {0};
        converter.ToUInt32Buff(standardBoolTime, boolTime);
        memcpy(writebuff + writebuff_pos, boolTime, 4); //持续时长
        writebuff_pos += 4;
        break;
    }
    case ValueType::USINT: // USINT
    {
        char StandardUsintValue = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
        char UsintValue[1] = {0};
        UsintValue[0] = StandardUsintValue;
        memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
        writebuff_pos += 1;
        break;
    }
    case ValueType::UINT: // UINT
    {
        uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
        char UintValue[2] = {0};
        converter.ToUInt16Buff(standardUintValue, UintValue);
        memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
        writebuff_pos += 2;
        break;
    }
    case ValueType::SINT: // SINT
    {
        char StandardSintValue = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
        char SintValue[1] = {0};
        SintValue[0] = StandardSintValue;
        memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
        writebuff_pos += 1;
        break;
    }
    case ValueType::INT: // INT
    {
        short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
        char IntValue[2] = {0};
        converter.ToInt16Buff(standardIntValue, IntValue);
        memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
        writebuff_pos += 2;
        break;
    }
    case ValueType::DINT: // DINT
    {
        int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
        char DintValue[4] = {0};
        converter.ToInt32Buff(standardDintValue, DintValue);
        memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
        writebuff_pos += 4;
        break;
    }
    case ValueType::REAL: // REAL
    {
        float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
        char RealValue[4] = {0};
        converter.ToFloatBuff(standardRealValue, RealValue);
        memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
        writebuff_pos += 4;
        break;
    }
    case ValueType::BOOL: // BOOL
    {
        char standardBoolValue = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
        char BoolValue[1] = {0};
        BoolValue[0] = standardBoolValue;
        memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
        writebuff_pos += 1;
        break;
    }
    default:
        break;
    }
}

void ZipUtils::addArrayStandardValue(int schemaPos, char *writebuff, long &writebuff_pos, ValueType::ValueType DataType)
{
    DataTypeConverter converter;
    switch (DataType)
    {
    case ValueType::BOOL:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            char standardBoolValue = CurrentZipTemplate.schemas[schemaPos].second.standard[i][0];
            char BoolValue[1] = {0};
            BoolValue[0] = standardBoolValue;
            memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
            writebuff_pos += 1;
        }
        break;
    }
    case ValueType::USINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            char StandardUsintValue = CurrentZipTemplate.schemas[schemaPos].second.standard[i][0];
            char UsintValue[1] = {0};
            UsintValue[0] = StandardUsintValue;
            memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
            writebuff_pos += 1;
        }
        break;
    }
    case ValueType::UINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
            char UintValue[2] = {0};
            converter.ToUInt16Buff(standardUintValue, UintValue);
            memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
            writebuff_pos += 2;
        }
        break;
    }
    case ValueType::UDINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
            char boolTime[4] = {0};
            converter.ToUInt32Buff(standardBoolTime, boolTime);
            memcpy(writebuff + writebuff_pos, boolTime, 4); //持续时长
            writebuff_pos += 4;
        }
        break;
    }
    case ValueType::SINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            char StandardSintValue = CurrentZipTemplate.schemas[schemaPos].second.standard[i][0];
            char SintValue[1] = {0};
            SintValue[0] = StandardSintValue;
            memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
            writebuff_pos += 1;
        }
        break;
    }
    case ValueType::INT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
            char IntValue[2] = {0};
            converter.ToInt16Buff(standardIntValue, IntValue);
            memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
            writebuff_pos += 2;
        }
        break;
    }
    case ValueType::DINT:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
            char DintValue[4] = {0};
            converter.ToInt32Buff(standardDintValue, DintValue);
            memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
            writebuff_pos += 4;
        }
        break;
    }
    case ValueType::REAL:
    {
        for (auto i = 0; i < CurrentZipTemplate.schemas[schemaPos].second.arrayLen; i++)
        {
            float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
            char RealValue[4] = {0};
            converter.ToFloatBuff(standardRealValue, RealValue);
            memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
            writebuff_pos += 4;
        }
        break;
    }
    default:
        break;
    }
}

/**
 * @brief 还原Ts类型的时间戳
 *
 * @param schemaPos 变量在模板的编号
 * @param startTime 开始时间
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 * @param tsPos Ts目前所在编号
 */
void ZipUtils::addTsTime(int schemaPos, uint64_t startTime, char *writebuff, long &writebuff_pos, int tsPos)
{
    DataTypeConverter converter;
    //添加上时间戳
    uint64_t zipTime = startTime + CurrentZipTemplate.schemas[schemaPos].second.timeseriesSpan * tsPos;
    char zipTimeBuff[ZIP_TIMESTAMP_SIZE] = {0};
    converter.ToLong64Buff(zipTime, zipTimeBuff);
    memcpy(writebuff + writebuff_pos, zipTimeBuff, ZIP_TIMESTAMP_SIZE);
    writebuff_pos += ZIP_TIMESTAMP_SIZE;
}

/**
 * @brief Ts类型还原
 *
 * @param schemaPos 变量在模板的编号
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 * @param readbuff 读数据缓冲
 * @param readbuff_pos 读数据偏移量
 * @return int
 */
int ZipUtils::IsTSReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType)
{
    DataTypeConverter converter;
    if (readbuff[readbuff_pos - 1] == (char)TS_AND_ARRAY) //既是时间序列又是数组
    {
        // //直接拷贝
        // memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, (CurrentZipTemplate.schemas[schemaPos].second.arrayLen * CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE) * CurrentZipTemplate.schemas[schemaPos].second.tsLen);
        // writebuff_pos += (CurrentZipTemplate.schemas[schemaPos].second.arrayLen * CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE) * CurrentZipTemplate.schemas[schemaPos].second.tsLen;
        // readbuff_pos += (CurrentZipTemplate.schemas[schemaPos].second.arrayLen * CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE) * CurrentZipTemplate.schemas[schemaPos].second.tsLen;
        if (ZipUtils::IsTsAndArrayReZip(schemaPos, writebuff, writebuff_pos, readbuff, readbuff_pos, DataType))
        {
            cout << "存在未知的数据类型，请检查模板或者更换功能块" << endl;
            return StatusCode::DATA_TYPE_MISMATCH_ERROR;
        }
    }
    else if (readbuff[readbuff_pos - 1] == (char)ONLY_TS) //只是时间序列
    {
        //先获得第一次采样的时间
        char time[ZIP_TIMESTAMP_SIZE];
        memcpy(time, readbuff + readbuff_pos, ZIP_TIMESTAMP_SIZE);
        uint64_t startTime = converter.ToLong64(time);
        readbuff_pos += ZIP_TIMESTAMP_SIZE;

        for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.tsLen; j++)
        {
            if (readbuff[readbuff_pos] == (char)-1) //说明没有未压缩的时间序列了
            {
                //将标准值数据拷贝到writebuff
                ZipUtils::addStandardValue(schemaPos, writebuff, writebuff_pos, DataType);
                //添加上时间戳
                ZipUtils::addTsTime(schemaPos, startTime, writebuff, writebuff_pos, j);
            }
            else
            {
                //对比编号是否等于未压缩的时间序列编号
                char zipTsPosNum[ZIP_ZIPPOS_SIZE] = {0};
                memcpy(zipTsPosNum, readbuff + readbuff_pos, ZIP_ZIPPOS_SIZE);
                uint16_t tsPosCmp = converter.ToUInt16(zipTsPosNum);

                if (tsPosCmp == j) //是未压缩时间序列的编号
                {
                    //将未压缩的数据拷贝到writebuff
                    readbuff_pos += ZIP_ZIPPOS_SIZE;
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                    //添加上时间戳
                    ZipUtils::addTsTime(schemaPos, startTime, writebuff, writebuff_pos, j);
                }
                else //不是未压缩时间序列的编号
                {
                    //将标准值数据拷贝到writebuff
                    ZipUtils::addStandardValue(schemaPos, writebuff, writebuff_pos, DataType);
                    //添加上时间戳
                    ZipUtils::addTsTime(schemaPos, startTime, writebuff, writebuff_pos, j);
                }
            }
            if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1) //时间序列还原结束，readbuff_pos+1跳过0xFF标志
                readbuff_pos += 1;
        }
    }
    else
    {
        cout << "还原类型出错！请检查压缩功能是否有误" << endl;
        return StatusCode::ZIPTYPE_ERROR;
    }
    return 0;
}

/**
 * @brief 数组类型还原
 *
 * @param schemaPos 变量在模板的编号
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 * @param readbuff 读数据缓冲
 * @param readbuff_pos 读数据偏移量
 * @return int
 */
int ZipUtils::IsArrayReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos)
{
    if (readbuff[readbuff_pos - 1] == (char)DATA_AND_TIME) //既有时间又有数据
    {
        //直接拷贝
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE);
        writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;
        readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;
    }
    else if (readbuff[readbuff_pos - 1] == (char)ONLY_DATA) //只有数据
    {
        //直接拷贝
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
        writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
        readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
    }
    else
    {
        cout << "还原类型出错！请检查压缩功能是否有误" << endl;
        return StatusCode::ZIPTYPE_ERROR;
    }
    return 0;
}

int ZipUtils::IsArrayReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType valueType)
{
    if (readbuff[readbuff_pos - 1] == (char)DATA_AND_TIME) //既有时间又有数据
    {
        //直接拷贝
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE);
        writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;
        readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + ZIP_TIMESTAMP_SIZE;
    }
    else if (readbuff[readbuff_pos - 1] == (char)ONLY_DATA) //只有数据
    {
        //直接拷贝
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
        writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
        readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
    }
    else if (readbuff[readbuff_pos - 1] == (char)ONLY_TIME) //只有时间戳
    {
        //添加数组标准值，然后添加原时间戳
        addArrayStandardValue(schemaPos, writebuff, writebuff_pos, valueType);
        memcpy(writebuff, readbuff, ZIP_TIMESTAMP_SIZE);
        writebuff_pos += ZIP_TIMESTAMP_SIZE;
        readbuff_pos += ZIP_TIMESTAMP_SIZE;
    }
    else
    {
        cout << "还原类型出错！请检查压缩功能是否有误" << endl;
        return StatusCode::ZIPTYPE_ERROR;
    }
    return 0;
}

int ZipUtils::IsTsAndArrayReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType)
{
    DataTypeConverter converter;
    //先获得第一次采样的时间
    char time[ZIP_TIMESTAMP_SIZE];
    memcpy(time, readbuff + readbuff_pos, ZIP_TIMESTAMP_SIZE);
    uint64_t startTime = converter.ToLong64(time);
    readbuff_pos += ZIP_TIMESTAMP_SIZE;

    for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.tsLen; j++)
    {
        if (readbuff[readbuff_pos] == (char)-1) //说明没有未压缩的时间序列了
        {
            //将标准值数据拷贝到writebuff
            ZipUtils::addArrayStandardValue(schemaPos, writebuff, writebuff_pos, DataType);
            //添加上时间戳
            ZipUtils::addTsTime(schemaPos, startTime, writebuff, writebuff_pos, j);
        }
        else
        {
            //对比编号是否等于未压缩的时间序列编号
            char zipTsPosNum[ZIP_ZIPPOS_SIZE] = {0};
            memcpy(zipTsPosNum, readbuff + readbuff_pos, ZIP_ZIPPOS_SIZE);
            uint16_t tsPosCmp = converter.ToUInt16(zipTsPosNum);

            if (tsPosCmp == j) //是未压缩时间序列的编号
            {
                //将未压缩的数据拷贝到writebuff
                readbuff_pos += ZIP_ZIPPOS_SIZE;
                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
                writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
                //添加上时间戳
                ZipUtils::addTsTime(schemaPos, startTime, writebuff, writebuff_pos, j);
            }
            else //不是未压缩时间序列的编号
            {
                //将标准值数据拷贝到writebuff
                ZipUtils::addArrayStandardValue(schemaPos, writebuff, writebuff_pos, DataType);
                //添加上时间戳
                ZipUtils::addTsTime(schemaPos, startTime, writebuff, writebuff_pos, j);
            }
        }
        if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1) //时间序列还原结束，readbuff_pos+1跳过0xFF标志
            readbuff_pos += 1;
    }
    return 0;
}

/**
 * @brief 既不是Ts又不是数组还原
 *
 * @param schemaPos 变量在模板的编号
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 * @param readbuff 读数据缓冲
 * @param readbuff_pos 读数据偏移量
 * @return int
 */
int ZipUtils::IsNotArrayAndTSReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType)
{
    if (readbuff[readbuff_pos - 1] == (char)DATA_AND_TIME) //既有时间又有数据
    {
        //直接拷贝
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE);
        writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE;
        readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + ZIP_TIMESTAMP_SIZE;
    }
    else if (readbuff[readbuff_pos - 1] == (char)ONLY_TIME) //只有时间
    {
        //先添加上标准值到writebuff
        ZipUtils::addStandardValue(schemaPos, writebuff, writebuff_pos, DataType);
        //再拷贝时间
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, ZIP_TIMESTAMP_SIZE);
        writebuff_pos += ZIP_TIMESTAMP_SIZE;
        readbuff_pos += ZIP_TIMESTAMP_SIZE;
    }
    else if (readbuff[readbuff_pos - 1] == (char)ONLY_DATA) //只有数据
    {
        //直接拷贝
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
        writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
        readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
    }
    else
    {
        cout << "还原类型出错！请检查压缩功能是否有误" << endl;
        return StatusCode::ZIPTYPE_ERROR;
    }
    return 0;
}

/**
 * @brief 在进行压缩操作前，检查输入的压缩参数是否合法
 * @param params        压缩请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int CheckZipParams(DB_ZipParams *params)
{
    if (params->pathToLine == NULL)
    {
        return StatusCode::EMPTY_PATH_TO_LINE;
    }
    switch (params->ZipType)
    {
    case TIME_SPAN:
    {
        if ((params->start == 0 && params->end == 0) || params->start > params->end)
        {
            return StatusCode::INVALID_TIMESPAN;
        }
        else if (params->end == 0)
        {
            params->end = getMilliTime();
        }
        break;
    }
    case PATH_TO_LINE:
    {
        if (params->pathToLine == 0)
        {
            return StatusCode::EMPTY_PATH_TO_LINE;
        }
        break;
    }
    case FILE_ID:
    {
        if (params->fileID == NULL)
        {
            return StatusCode::NO_FILEID;
        }
        else if (params->zipNums < 0 && params->EID == NULL)
            return StatusCode::ZIPNUM_ERROR;
        else if (params->zipNums > 1 && params->EID != NULL)
            return StatusCode::ZIPNUM_AND_EID_ERROR;
        break;
    }
    default:
        return StatusCode::NO_ZIP_TYPE;
        break;
    }
    return 0;
}