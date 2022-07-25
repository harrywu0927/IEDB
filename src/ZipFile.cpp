#include "../include/utils.hpp"
using namespace std;

int ZipBuf(char *readbuff, char **writebuff, long &writebuff_pos);
int ReZipBuf(char *readbuff, const long len, char **writebuff, long &writebuff_pos);
int DB_ZipFile_thread(vector<pair<string, long>> selectedFiles, uint16_t begin, uint16_t num, const char *pathToLine);
int DB_ReZipFile_thread(vector<pair<string, long>> selectedFiles, uint16_t begin, uint16_t num, const char *pathToLine);

mutex openMutex;

/**
 * @brief 添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
 *
 * @param schemaPos 变量在模板的编号
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 */
void addZipPos(int schemaPos, char *writebuff, long &writebuff_pos)
{
    DataTypeConverter converter;
    uint16_t posNum = schemaPos;
    char zipPosNum[2] = {0};
    converter.ToUInt16Buff(posNum, zipPosNum);
    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
    writebuff_pos += 2;
}

/**
 * @brief 当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
 *
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 */
void addEndFlag(char *writebuff, long &writebuff_pos)
{
    char isTsEnd[1] = {0};
    isTsEnd[0] = (char)-1;
    memcpy(writebuff + writebuff_pos, isTsEnd, 1);
    writebuff_pos += 1;
}

/**
 * @brief 添加上压缩标志
 *
 * @param ziptype 压缩标志
 * @param writebuff 写数据缓冲
 * @param writebuff_pos 写数据偏移量
 */
void addZipType(int ziptype, char *writebuff, long &writebuff_pos)
{
    switch (ziptype)
    {
    case 0:
    { //只有数据
        char zipType[1] = {0};
        zipType[0] = (char)0;
        memcpy(writebuff + writebuff_pos, zipType, 1);
        writebuff_pos += 1;
        break;
    }
    case 1:
    { //只有时间
        char zipType[1] = {0};
        zipType[0] = (char)1;
        memcpy(writebuff + writebuff_pos, zipType, 1);
        writebuff_pos += 1;
        break;
    }
    case 2:
    { //既有数据又有时间
        char zipType[1] = {0};
        zipType[0] = (char)2;
        memcpy(writebuff + writebuff_pos, zipType, 1);
        writebuff_pos += 1;
        break;
    }
    case 3:
    { //既是时间序列又是数组
        char zipType[1] = {0};
        zipType[0] = (char)3;
        memcpy(writebuff + writebuff_pos, zipType, 1);
        writebuff_pos += 1;
        break;
    }
    case 4:
    { //只是时间序列
        char zipType[1] = {0};
        zipType[0] = (char)4;
        memcpy(writebuff + writebuff_pos, zipType, 1);
        writebuff_pos += 1;
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
int IsTSZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, int DataType)
{
    DataTypeConverter converter;
    if (CurrentZipTemplate.schemas[schemaPos].second.isArray == true) //既是时间序列又是数组，则不压缩
    {
        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
        addZipPos(schemaPos, writebuff, writebuff_pos);
        //既是时间序列又是数组
        addZipType(3, writebuff, writebuff_pos);
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, (CurrentZipTemplate.schemas[schemaPos].second.arrayLen * CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8) * CurrentZipTemplate.schemas[schemaPos].second.tsLen);
        writebuff_pos += (CurrentZipTemplate.schemas[schemaPos].second.arrayLen * CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8) * CurrentZipTemplate.schemas[schemaPos].second.tsLen;
        readbuff_pos += (CurrentZipTemplate.schemas[schemaPos].second.arrayLen * CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8) * CurrentZipTemplate.schemas[schemaPos].second.tsLen;
    }
    else //只是时间序列类型
    {
        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
        addZipPos(schemaPos, writebuff, writebuff_pos);
        //只是时间序列
        addZipType(4, writebuff, writebuff_pos);
        //添加第一个采样的时间戳
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[schemaPos].second.valueBytes, 8);
        writebuff_pos += 8;

        switch (DataType)
        {
        case 3: // UDINT
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
                    addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case 2: // USINT
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
                    addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case 1: // UINT
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
                    addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case 6: // SINT
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
                    addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case 4: // INT
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
                    addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case 7: // DINT
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
                    addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case 8: // REAL
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
                    addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                }
                readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    addEndFlag(writebuff, writebuff_pos);
                }
            }
            break;
        }
        case 5: // BOOL
        {
            char standardBool = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
            for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.tsLen; j++)
            {
                if (standardBool != readbuff[readbuff_pos])
                {
                    //添加编号方便知道未压缩的时间序列是哪个，按照顺序，从0开始，2个字节
                    addZipPos(j, writebuff, writebuff_pos);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                    writebuff_pos += 1;
                }
                readbuff_pos += 9;

                //当时间序列全部压缩完之后，添加一个-1 即0xFF，标志时间序列类型压缩结束，以避免还原数据时无法区分时间序列序号与模板序号
                if (j == CurrentZipTemplate.schemas[schemaPos].second.tsLen - 1)
                {
                    addEndFlag(writebuff, writebuff_pos);
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
void IsArrayZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos)
{
    DataTypeConverter converter;
    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
    addZipPos(schemaPos, writebuff, writebuff_pos);

    if (CurrentZipTemplate.schemas[schemaPos].second.hasTime == true) //带有时间戳
    {
        //既有数据又有时间
        addZipType(2, writebuff, writebuff_pos);
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + 8);
        writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + 8;
        readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + 8;
    }
    else
    {
        //只有数据
        addZipType(0, writebuff, writebuff_pos);
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen);
        writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
        readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen;
    }
}

/**
 * @brief 对readbuff里的数据进行压缩，压缩后数据保存在writebuff里，长度为writebuff_pos
 *
 * @param readbuff 需要进行压缩的数据
 * @param writebuff 压缩后的数据
 * @param writebuff_pos 压缩后数据的长度
 * @return int
 */
int ZipBuf(char *readbuff, char **writebuff, long &writebuff_pos)
{
    long readbuff_pos = 0;
    DataTypeConverter converter;

    for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++) //循环比较
    {
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::BOOL) // BOOL变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true) //是时间序列类型
            {
                if (IsTSZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 5))
                {
                    cout << "存在未知的数据类型，请检查模板或者更换功能块" << endl;
                    return StatusCode::DATA_TYPE_MISMATCH_ERROR;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                IsArrayZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos);
            }
            else
            {
                char standardBool = CurrentZipTemplate.schemas[i].second.standardValue[0];
                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readbuff_pos, 1);
                char currentBoolValue = value[0];

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    addZipPos(i, *writebuff, writebuff_pos);
                    if (standardBool != readbuff[readbuff_pos])
                    {
                        //既有数据又有时间
                        addZipType(2, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes + 8);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                    }
                    else
                    {
                        //只有时间
                        addZipType(1, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[i].second.valueBytes, 8);
                        writebuff_pos += 8;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                }
                else //不带时间戳
                {
                    if (standardBool != readbuff[readbuff_pos])
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        addZipPos(i, *writebuff, writebuff_pos);
                        //只有数据
                        addZipType(0, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true) //是时间序列类型
            {
                if (IsTSZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 2))
                {
                    cout << "存在未知的数据类型，请检查模板或者更换功能块" << endl;
                    return StatusCode::DATA_TYPE_MISMATCH_ERROR;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                IsArrayZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos);
            }
            else
            {
                char standardUSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char maxUSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                char minUSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];
                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readbuff_pos, 1);
                char currentUSintValue = value[0];

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    addZipPos(i, *writebuff, writebuff_pos);
                    if (currentUSintValue != standardUSintValue && (currentUSintValue < minUSintValue || currentUSintValue > maxUSintValue))
                    {
                        //既有数据又有时间
                        addZipType(2, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes + 8);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                    }
                    else
                    {
                        //只有时间
                        addZipType(1, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[i].second.valueBytes, 8);
                        writebuff_pos += 8;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                }
                else //不带时间戳
                {
                    if (currentUSintValue != standardUSintValue && (currentUSintValue < minUSintValue || currentUSintValue > maxUSintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        addZipPos(i, *writebuff, writebuff_pos);
                        //只有数据
                        addZipType(0, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true) //是时间序列类型
            {
                if (IsTSZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 1))
                {
                    cout << "存在未知的数据类型，请检查模板或者更换功能块" << endl;
                    return StatusCode::DATA_TYPE_MISMATCH_ERROR;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                IsArrayZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos);
            }
            else
            {
                ushort standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                ushort maxUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                ushort minUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, readbuff + readbuff_pos, 2);
                ushort currentUintValue = converter.ToUInt16(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    addZipPos(i, *writebuff, writebuff_pos);
                    if (currentUintValue != standardUintValue && (currentUintValue < minUintValue || currentUintValue > maxUintValue))
                    {
                        //既有数据又有时间
                        addZipType(2, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes + 8);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                    }
                    else
                    {
                        //只有时间
                        addZipType(1, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[i].second.valueBytes, 8);
                        writebuff_pos += 8;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                }
                else //不带时间戳
                {
                    if (currentUintValue != standardUintValue && (currentUintValue < minUintValue || currentUintValue > maxUintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        addZipPos(i, *writebuff, writebuff_pos);
                        //只有数据
                        addZipType(0, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true) //是时间序列类型
            {
                if (IsTSZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 3))
                {
                    cout << "存在未知的数据类型，请检查模板或者更换功能块" << endl;
                    return StatusCode::DATA_TYPE_MISMATCH_ERROR;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                IsArrayZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos);
            }
            else
            {
                uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                uint32 maxUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                uint32 minUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                uint32 currentUDintValue = converter.ToUInt32(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    addZipPos(i, *writebuff, writebuff_pos);
                    if (currentUDintValue != standardUDintValue && (currentUDintValue < minUDintValue || currentUDintValue > maxUDintValue))
                    {
                        //既有数据又有时间
                        addZipType(2, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes + 8);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                    }
                    else
                    {
                        //只有时间
                        addZipType(1, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[i].second.valueBytes, 8);
                        writebuff_pos += 8;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                }
                else //不带时间戳
                {
                    if (currentUDintValue != standardUDintValue && (currentUDintValue < minUDintValue || currentUDintValue > maxUDintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        addZipPos(i, *writebuff, writebuff_pos);
                        //只有数据
                        addZipType(0, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true) //是时间序列类型
            {
                if (IsTSZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 6))
                {
                    cout << "存在未知的数据类型，请检查模板或者更换功能块" << endl;
                    return StatusCode::DATA_TYPE_MISMATCH_ERROR;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                IsArrayZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos);
            }
            else
            {
                char standardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char maxSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                char minSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readbuff_pos, 1);
                char currentSintValue = value[0];

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    addZipPos(i, *writebuff, writebuff_pos);
                    if (currentSintValue != standardSintValue && (currentSintValue < minSintValue || currentSintValue > maxSintValue))
                    {
                        //既有数据又有时间
                        addZipType(2, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes + 8);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                    }
                    else
                    {
                        //只有时间
                        addZipType(1, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[i].second.valueBytes, 8);
                        writebuff_pos += 8;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                }
                else //不带时间戳
                {
                    if (currentSintValue != standardSintValue && (currentSintValue < minSintValue || currentSintValue > maxSintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        addZipPos(i, *writebuff, writebuff_pos);
                        //只有数据
                        addZipType(0, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true) //是时间序列类型
            {
                if (IsTSZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 4))
                {
                    cout << "存在未知的数据类型，请检查模板或者更换功能块" << endl;
                    return StatusCode::DATA_TYPE_MISMATCH_ERROR;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                IsArrayZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos);
            }
            else
            {
                short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                short maxIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                short minIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, readbuff + readbuff_pos, 2);
                short currentIntValue = converter.ToInt16(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    addZipPos(i, *writebuff, writebuff_pos);
                    if (currentIntValue != standardIntValue && (currentIntValue < minIntValue || currentIntValue > maxIntValue))
                    {
                        //既有数据又有时间
                        addZipType(2, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes + 8);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                    }
                    else
                    {
                        //只有时间
                        addZipType(1, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[i].second.valueBytes, 8);
                        writebuff_pos += 8;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                }
                else //不带时间戳
                {
                    if (currentIntValue != standardIntValue && (currentIntValue < minIntValue || currentIntValue > maxIntValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        addZipPos(i, *writebuff, writebuff_pos);
                        //只有数据
                        addZipType(0, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true) //是时间序列类型
            {
                if (IsTSZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 7))
                {
                    cout << "存在未知的数据类型，请检查模板或者更换功能块" << endl;
                    return StatusCode::DATA_TYPE_MISMATCH_ERROR;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                IsArrayZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos);
            }
            else
            {
                int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                int maxDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                int minDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                int currentDintValue = converter.ToInt32(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    addZipPos(i, *writebuff, writebuff_pos);
                    if (currentDintValue != standardDintValue && (currentDintValue < minDintValue || currentDintValue > maxDintValue))
                    {
                        //既有数据又有时间
                        addZipType(2, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes + 8);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                    }
                    else
                    {
                        //只有时间
                        addZipType(1, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[i].second.valueBytes, 8);
                        writebuff_pos += 8;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                }
                else //不带时间戳
                {
                    if (currentDintValue != standardDintValue && (currentDintValue < minDintValue || currentDintValue > maxDintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        addZipPos(i, *writebuff, writebuff_pos);
                        //只有数据
                        addZipType(0, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true) //是时间序列类型
            {
                if (IsTSZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 8))
                {
                    cout << "存在未知的数据类型，请检查模板或者更换功能块" << endl;
                    return StatusCode::DATA_TYPE_MISMATCH_ERROR;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                IsArrayZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos);
            }
            else
            {
                float standardFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                float maxFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.maxValue);
                float minFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                float currentRealValue = converter.ToFloat(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    addZipPos(i, *writebuff, writebuff_pos);
                    if (currentRealValue != standardFloatValue && (currentRealValue < minFloatValue || currentRealValue > maxFloatValue))
                    {
                        //既有数据又有时间
                        addZipType(2, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes + 8);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                    }
                    else
                    {
                        //只有时间
                        addZipType(1, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[i].second.valueBytes, 8);
                        writebuff_pos += 8;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
                }
                else //不带时间戳
                {
                    if (currentRealValue != standardFloatValue && (currentRealValue < minFloatValue || currentRealValue > maxFloatValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        converter.ToUInt16Buff(posNum, zipPosNum);
                        memcpy(*writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        //只有数据
                        addZipType(0, *writebuff, writebuff_pos);
                        memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::IMAGE)
        {
            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
            addZipPos(i, *writebuff, writebuff_pos);
            //暂定图片前面有2字节长度，2字节宽度和2字节通道
            char length[2] = {0};
            memcpy(length, readbuff + readbuff_pos, 2);
            uint16_t imageLength = converter.ToUInt16(length);
            char width[2] = {0};
            memcpy(width, readbuff + readbuff_pos + 2, 2);
            uint16_t imageWidth = converter.ToUInt16(width);
            char channel[2] = {0};
            memcpy(channel, readbuff + readbuff_pos + 4, 2);
            uint16_t imageChannel = converter.ToUInt16(channel);
            uint32_t imageSize = imageChannel * imageLength * imageWidth;
            char *writebuff_new = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size() + imageSize + 6];
            memcpy(writebuff_new, *writebuff, writebuff_pos);
            delete[] * writebuff;
            *writebuff = writebuff_new;

            if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
            {
                //既有数据又有时间
                addZipType(2, *writebuff, writebuff_pos);

                memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, 6); //存储图片的长度、宽度、通道
                readbuff_pos += 6;
                writebuff_pos += 6;

                memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, imageSize + 8);
                writebuff_pos += imageSize + 8;
                readbuff_pos += imageSize + 8;
            }
            else
            {
                //只有数据
                addZipType(0, *writebuff, writebuff_pos);

                memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, 6); //存储图片的长度、宽度、通道
                readbuff_pos += 6;
                writebuff_pos += 6;

                memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, imageSize);
                writebuff_pos += imageSize;
                readbuff_pos += imageSize;
            }
        }
        else
        {
            cout << "存在未知的数据类型，请检查模板或者更换功能块" << endl;
            return StatusCode::DATA_TYPE_MISMATCH_ERROR;
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
void addStandardValue(int schemaPos, char *writebuff, long &writebuff_pos, int DataType)
{
    DataTypeConverter converter;
    //添加上标准值到writebuff
    switch (DataType)
    {
    case 3: // UDINT
    {
        uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
        char boolTime[4] = {0};
        converter.ToUInt32Buff(standardBoolTime, boolTime);
        memcpy(writebuff + writebuff_pos, boolTime, 4); //持续时长
        writebuff_pos += 4;
        break;
    }
    case 2: // USINT
    {
        char StandardUsintValue = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
        char UsintValue[1] = {0};
        UsintValue[0] = StandardUsintValue;
        memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
        writebuff_pos += 1;
        break;
    }
    case 1: // UINT
    {
        uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
        char UintValue[2] = {0};
        converter.ToUInt16Buff(standardUintValue, UintValue);
        memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
        writebuff_pos += 2;
        break;
    }
    case 6: // SINT
    {
        char StandardSintValue = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
        char SintValue[1] = {0};
        SintValue[0] = StandardSintValue;
        memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
        writebuff_pos += 1;
        break;
    }
    case 4: // INT
    {
        short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
        char IntValue[2] = {0};
        converter.ToInt16Buff(standardIntValue, IntValue);
        memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
        writebuff_pos += 2;
        break;
    }
    case 7: // DINT
    {
        int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
        char DintValue[4] = {0};
        converter.ToInt32Buff(standardDintValue, DintValue);
        memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
        writebuff_pos += 4;
        break;
    }
    case 8: // REAL
    {
        float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
        char RealValue[4] = {0};
        converter.ToFloatBuff(standardRealValue, RealValue);
        memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
        writebuff_pos += 4;
        break;
    }
    case 5: // BOOL
    {
        char standardBoolValue = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
        char BoolValue[1] = {0};
        BoolValue[0] = standardBoolValue;
        memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
        writebuff_pos += 1;
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
void addTsTime(int schemaPos, uint64_t startTime, char *writebuff, long &writebuff_pos, int tsPos)
{
    DataTypeConverter converter;
    //添加上时间戳
    uint64_t zipTime = startTime + CurrentZipTemplate.schemas[schemaPos].second.timeseriesSpan * tsPos;
    char zipTimeBuff[8] = {0};
    converter.ToLong64Buff(zipTime, zipTimeBuff);
    memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
    writebuff_pos += 8;
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
int IsTSReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, int DataType)
{
    DataTypeConverter converter;
    if (readbuff[readbuff_pos - 1] == (char)3) //既是时间序列又是数组
    {
        //直接拷贝
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, (CurrentZipTemplate.schemas[schemaPos].second.arrayLen * CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8) * CurrentZipTemplate.schemas[schemaPos].second.tsLen);
        writebuff_pos += (CurrentZipTemplate.schemas[schemaPos].second.arrayLen * CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8) * CurrentZipTemplate.schemas[schemaPos].second.tsLen;
        readbuff_pos += (CurrentZipTemplate.schemas[schemaPos].second.arrayLen * CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8) * CurrentZipTemplate.schemas[schemaPos].second.tsLen;
    }
    else if (readbuff[readbuff_pos - 1] == (char)4) //只是时间序列
    {
        //先获得第一次采样的时间
        char time[8];
        memcpy(time, readbuff + readbuff_pos, 8);
        uint64_t startTime = converter.ToLong64(time);
        readbuff_pos += 8;

        for (auto j = 0; j < CurrentZipTemplate.schemas[schemaPos].second.tsLen; j++)
        {
            if (readbuff[readbuff_pos] == (char)-1) //说明没有未压缩的时间序列了
            {
                //将标准值数据拷贝到writebuff
                addStandardValue(schemaPos, writebuff, writebuff_pos, DataType);
                //添加上时间戳
                addTsTime(schemaPos, startTime, writebuff, writebuff_pos, j);
            }
            else
            {
                //对比编号是否等于未压缩的时间序列编号
                char zipTsPosNum[2] = {0};
                memcpy(zipTsPosNum, readbuff + readbuff_pos, 2);
                uint16_t tsPosCmp = converter.ToUInt16(zipTsPosNum);

                if (tsPosCmp == j) //是未压缩时间序列的编号
                {
                    //将未压缩的数据拷贝到writebuff
                    readbuff_pos += 2;
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes);
                    readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                    writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes;
                    //添加上时间戳
                    addTsTime(schemaPos, startTime, writebuff, writebuff_pos, j);
                }
                else //不是未压缩时间序列的编号
                {
                    //将标准值数据拷贝到writebuff
                    addStandardValue(schemaPos, writebuff, writebuff_pos, DataType);
                    //添加上时间戳
                    addTsTime(schemaPos, startTime, writebuff, writebuff_pos, j);
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
int IsArrayReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos)
{
    if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
    {
        //直接拷贝
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + 8);
        writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + 8;
        readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes * CurrentZipTemplate.schemas[schemaPos].second.arrayLen + 8;
    }
    else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
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
int IsNotArrayAndTSReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, int DataType)
{
    if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
    {
        //直接拷贝
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8);
        writebuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8;
        readbuff_pos += CurrentZipTemplate.schemas[schemaPos].second.valueBytes + 8;
    }
    else if (readbuff[readbuff_pos - 1] == (char)1) //只有时间
    {
        //先添加上标准值到writebuff
        addStandardValue(schemaPos, writebuff, writebuff_pos, DataType);
        //再拷贝时间
        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 8);
        writebuff_pos += 8;
        readbuff_pos += 8;
    }
    else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
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
 * @brief 对readbuff里的数据进行还原，还原后数据保存在writebuff里，长度为writebuff_pos
 *
 * @param readbuff 需要进行还原的数据
 * @param len 还原数据的长度
 * @param writebuff 还原后的数据
 * @param writebuff_pos 还原后数据的长度
 * @return int
 */
int ReZipBuf(char *readbuff, const long len, char **writebuff, long &writebuff_pos)
{
    long readbuff_pos = 0;
    DataTypeConverter converter;

    for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++)
    {
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::BOOL) // BOOL类型
        {
            if (len == 0) //表示文件完全压缩
            {
                //添加上标准值到writebuff
                addStandardValue(i, *writebuff, writebuff_pos, 5);
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);

                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (IsTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 5))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (IsArrayReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else
                        {
                            if (IsNotArrayAndTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 5))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                    }
                    else //不是未压缩的编号
                    {
                        //添加上标准值到writebuff
                        addStandardValue(i, *writebuff, writebuff_pos, 5);
                    }
                }
                else //没有未压缩的数据了
                {
                    //添加上标准值到writebuff
                    addStandardValue(i, *writebuff, writebuff_pos, 5);
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                //添加上标准值到writebuff
                addStandardValue(i, *writebuff, writebuff_pos, 3);
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);

                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (IsTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 3))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (IsArrayReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else
                        {
                            if (IsNotArrayAndTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 3))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                    }
                    else //不是未压缩的编号
                    {
                        //添加上标准值到writebuff
                        addStandardValue(i, *writebuff, writebuff_pos, 3);
                    }
                }
                else //没有未压缩的数据了
                {
                    //添加上标准值到writebuff
                    addStandardValue(i, *writebuff, writebuff_pos, 3);
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                //添加上标准值到writebuff
                addStandardValue(i, *writebuff, writebuff_pos, 2);
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);

                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (IsTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 2))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (IsArrayReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else
                        {
                            if (IsNotArrayAndTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 2))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                    }
                    else //不是未压缩的编号
                    {
                        //添加上标准值到writebuff
                        addStandardValue(i, *writebuff, writebuff_pos, 2);
                    }
                }
                else //没有未压缩的数据了
                {
                    //添加上标准值到writebuff
                    addStandardValue(i, *writebuff, writebuff_pos, 2);
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                //添加上标准值到writebuff
                addStandardValue(i, *writebuff, writebuff_pos, 1);
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);

                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (IsTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 1))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (IsArrayReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else
                        {
                            if (IsNotArrayAndTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 1))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                    }
                    else //不是未压缩的编号
                    {
                        //添加上标准值到writebuff
                        addStandardValue(i, *writebuff, writebuff_pos, 1);
                    }
                }
                else //没有未压缩的数据了
                {
                    //添加上标准值到writebuff
                    addStandardValue(i, *writebuff, writebuff_pos, 1);
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                //添加上标准值到writebuff
                addStandardValue(i, *writebuff, writebuff_pos, 6);
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);

                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (IsTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 6))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (IsArrayReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else
                        {
                            if (IsNotArrayAndTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 6))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                    }
                    else //不是未压缩的编号
                    {
                        //添加上标准值到writebuff
                        addStandardValue(i, *writebuff, writebuff_pos, 6);
                    }
                }
                else //没有未压缩的数据了
                {
                    //添加上标准值到writebuff
                    addStandardValue(i, *writebuff, writebuff_pos, 6);
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                //添加上标准值到writebuff
                addStandardValue(i, *writebuff, writebuff_pos, 4);
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);

                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (IsTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 4))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (IsArrayReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else
                        {
                            if (IsNotArrayAndTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 4))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                    }
                    else //不是未压缩的编号
                    {
                        //添加上标准值到writebuff
                        addStandardValue(i, *writebuff, writebuff_pos, 4);
                    }
                }
                else //没有未压缩的数据了
                {
                    //添加上标准值到writebuff
                    addStandardValue(i, *writebuff, writebuff_pos, 4);
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                //添加上标准值到writebuff
                addStandardValue(i, *writebuff, writebuff_pos, 7);
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);

                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (IsTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 7))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (IsArrayReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else
                        {
                            if (IsNotArrayAndTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 7))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                    }
                    else //不是未压缩的编号
                    {
                        //添加上标准值到writebuff
                        addStandardValue(i, *writebuff, writebuff_pos, 7);
                    }
                }
                else //没有未压缩的数据了
                {
                    //添加上标准值到writebuff
                    addStandardValue(i, *writebuff, writebuff_pos, 7);
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL类型
        {
            if (len == 0) //表示文件完全压缩
            {
                //添加上标准值到writebuff
                addStandardValue(i, *writebuff, writebuff_pos, 8);
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);

                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (IsTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 8))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (IsArrayReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else
                        {
                            if (IsNotArrayAndTSReZip(i, *writebuff, writebuff_pos, readbuff, readbuff_pos, 8))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                    }
                    else //不是未压缩的编号
                    {
                        //添加上标准值到writebuff
                        addStandardValue(i, *writebuff, writebuff_pos, 8);
                    }
                }
                else //没有未压缩的数据了
                {
                    //添加上标准值到writebuff
                    addStandardValue(i, *writebuff, writebuff_pos, 8);
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::IMAGE) // IMAGE类型
        {
            //对比编号是否等于当前模板所在条数
            char zipPosNum[2] = {0};
            memcpy(zipPosNum, readbuff + readbuff_pos, 2);
            uint16_t posCmp = converter.ToUInt16(zipPosNum);

            if (posCmp == i) //是未压缩数据的编号
            {
                readbuff_pos += 2;
                //暂定图片之前有2字节的长度，2字节的宽度和2字节的通道
                char length[2] = {0};
                memcpy(length, readbuff + readbuff_pos + 1, 2);
                uint16_t imageLength = converter.ToUInt16(length);
                char width[2] = {0};
                memcpy(width, readbuff + readbuff_pos + 3, 2);
                uint16_t imageWidth = converter.ToUInt16(width);
                char channel[2] = {0};
                memcpy(channel, readbuff + readbuff_pos + 5, 2);
                //算出图片大小
                uint16_t imageChannel = converter.ToUInt16(channel);
                uint32_t imageSize = imageChannel * imageLength * imageWidth;
                readbuff_pos += 1;
                char *writebuff_new = new char[CurrentZipTemplate.totalBytes + imageSize + 6];
                memcpy(writebuff_new, *writebuff, writebuff_pos);
                delete[] * writebuff;
                *writebuff = writebuff_new;

                if (readbuff[readbuff_pos - 1] == (char)2) //既有数据又有时间戳
                {
                    //存储图片的长度、宽度、通道
                    memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, 6);
                    readbuff_pos += 6;
                    writebuff_pos += 6;

                    //存储图片
                    memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, imageSize + 8);
                    writebuff_pos += imageSize + 8;
                    readbuff_pos += imageSize + 8;
                }
                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                {
                    //存储图片的长度、宽度、通道
                    memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, 6);
                    readbuff_pos += 6;
                    writebuff_pos += 6;

                    //存储图片
                    memcpy(*writebuff + writebuff_pos, readbuff + readbuff_pos, imageSize);
                    writebuff_pos += imageSize;
                    readbuff_pos += imageSize;
                }
                else
                {
                    cout << "还原类型出错！请检查压缩功能是否有误" << endl;
                    return StatusCode::ZIPTYPE_ERROR;
                }
            }
            else //不是未压缩的编号
            {
                cout << "图片还原出现问题！" << endl;
                return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            }
        }
        else
        {
            cout << "存在未知数据类型，请检查模板" << endl;
            return StatusCode::UNKNOWN_TYPE;
        }
    }
    return 0;
}

/**
 * @brief 按文件夹压缩.idb文件，线程函数
 *
 * @param filesWithTime 带时间戳的.idb文件
 * @param begin 文件开始下标
 * @param num 文件数量
 * @param pathToLine .idb文件路径
 * @return 0:success,
 *          other:StatusCode
 */
int DB_ZipFile_thread(vector<pair<string, long>> selectedFiles, uint16_t begin, uint16_t num, const char *pathToLine)
{
    int err = 0;

    for (auto fileNum = begin; fileNum < num; fileNum++) //循环压缩文件夹下的所有.idb文件
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
        char *readbuff = new char[len];                                                                    //文件内容
        char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被还原的数据

        if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            IOBusy = false;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        long writebuff_pos = 0;

        err = ZipBuf(readbuff, &writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后数据在writebuff里
        if (err)
            continue;

        if (writebuff_pos >= len) //表明数据没有被压缩,保持原文件
        {
            cout << selectedFiles[fileNum].first + "文件数据没有被压缩!" << endl;
            // return 1;//1表示数据没有被压缩
        }
        else
        {
            DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
            long fp;
            string finalpath = selectedFiles[fileNum].first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
            //创建新文件并写入
            char mode[2] = {'w', 'b'};
            // openMutex.lock();
            err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
            if (err == 0)
            {
                if (writebuff_pos != 0)
                    err = fwrite(writebuff, writebuff_pos, 1, (FILE *)fp);
                err = DB_Close(fp);
            }
            // openMutex.unlock();
        }
        delete[] readbuff;
        delete[] writebuff;
    }
    IOBusy = false;
    return err;
}

/**
 * @brief 按还原文件夹.idbzip文件，线程函数
 *
 * @param filesWithTime 带时间戳的.idbzip文件
 * @param begin 文件开始下标
 * @param num 文件数量
 * @param pathToLine .idbzip文件路径
 * @return 0:success,
 *          other:StatusCode
 */
int DB_ReZipFile_thread(vector<pair<string, long>> selectedFiles, uint16_t begin, uint16_t num, const char *pathToLine)
{
    int err = 0;
    //循环以给每个.idbzip文件进行还原处理
    for (auto fileNum = begin; fileNum < num; fileNum++)
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
        char *readbuff = new char[len];                            //文件内容
        char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入被还原的数据

        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            IOBusy = false;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        err = ReZipBuf(readbuff, len, &writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
        if (err)
            continue;

        DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
        long fp;
        string finalpath = selectedFiles[fileNum].first.substr(0, selectedFiles[fileNum].first.length() - 3); //去掉后缀的zip
        //创建新文件并写入
        char mode[2] = {'w', 'b'};
        // openMutex.lock();
        err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
        if (err == 0)
        {
            // err = DB_Write(fp, writebuff, writebuff_pos);
            err = fwrite(writebuff, writebuff_pos, 1, (FILE *)fp);
            err = DB_Close(fp);
        }
        // openMutex.unlock();
        delete[] readbuff;
        delete[] writebuff;
    }
    IOBusy = false;
    return err;
}

/**
 * @brief 按文件夹压缩已有的.idb文件,所有数据类型都支持，单线程
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idb文件路径
 * @return  0:success,
 *          other:StatusCode
 */
int DB_ZipFile_Single(const char *ZipTemPath, const char *pathToLine)
{
    IOBusy = true;
    int err = 0;
    if(ZipTemPath==NULL || pathToLine==NULL)
        return StatusCode::EMPTY_SAVE_PATH;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    vector<pair<string, long>> filesWithTime;
    readIDBFilesWithTimestamps(pathToLine, filesWithTime); //获取所有.idb文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(filesWithTime, TIME_ASC); //将文件按时间升序

    DataTypeConverter converter;

    for (size_t fileNum = 0; fileNum < filesWithTime.size(); fileNum++) //循环压缩文件夹下的所有.idb文件
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(filesWithTime[fileNum].first.c_str()), &len);
        char *readbuff = new char[len];                                                                    //文件内容
        char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被还原的数据

        if (DB_OpenAndRead(const_cast<char *>(filesWithTime[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            IOBusy = false;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        long writebuff_pos = 0;

        err = ZipBuf(readbuff, &writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后数据在writebuff里
        if (err)
            continue;

        if (writebuff_pos >= len) //表明数据没有被压缩,保持原文件
        {
            cout << filesWithTime[fileNum].first + "文件数据没有被压缩!" << endl;
            // return 1;//1表示数据没有被压缩
        }
        else
        {
            DB_DeleteFile(const_cast<char *>(filesWithTime[fileNum].first.c_str())); //删除原文件
            long fp;
            string finalpath = filesWithTime[fileNum].first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
            //创建新文件并写入
            char mode[2] = {'w', 'b'};
            err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
            if (err == 0)
            {
                if (writebuff_pos != 0)
                    // err = DB_Write(fp, writebuff, writebuff_pos);
                    err = fwrite(writebuff, writebuff_pos, 1, (FILE *)fp);
                err = DB_Close(fp);
            }

            // int fd = sysOpen(const_cast<char *>(finalpath.c_str()));
            // err = write(fd, writebuff, writebuff_pos);
            // if (err == -1)
            //     return errno;
            // err = close(fd);
        }
        delete[] readbuff;
        delete[] writebuff;
    }
    IOBusy = false;
    return err;
}

/**
 * @brief 按文件夹压缩已有的.idb文件,所有数据类型都支持，多线程，内核数>2时才有效
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idb文件路径
 * @return  0:success,
 *          other:StatusCode
 */
int DB_ZipFile(const char *ZipTemPath, const char *pathToLine)
{
    int err;
    if(ZipTemPath==NULL || pathToLine==NULL)
        return StatusCode::EMPTY_SAVE_PATH;
    maxThreads = thread::hardware_concurrency();
    if (maxThreads <= 2) //如果内核数小于等于2则不如直接使用单线程
    {
        err = DB_ZipFile_Single(ZipTemPath, pathToLine);
        return err;
    }

    IOBusy = true;

    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    vector<pair<string, long>> filesWithTime;
    readIDBFilesWithTimestamps(pathToLine, filesWithTime); //获取所有.idb文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    //如果文件数小于1000则采用单线程压缩
    if (filesWithTime.size() < 1000)
    {
        IOBusy = false;
        err = DB_ZipFile_Single(ZipTemPath, pathToLine);
        return err;
    }
    sortByTime(filesWithTime, TIME_ASC); //将文件按时间升序

    uint16_t singleThreadFileNum = filesWithTime.size() / (maxThreads - 1);
    future_status status[maxThreads - 1];
    future<int> f[maxThreads - 1];
    uint16_t begin = 0;
    for (int j = 0; j < maxThreads - 2; j++) //循环开启多线程
    {
        f[j] = async(std::launch::async, DB_ZipFile_thread, filesWithTime, begin, singleThreadFileNum * (j + 1), pathToLine);
        begin += singleThreadFileNum;
        status[j] = f[j].wait_for(chrono::milliseconds(1));
    }
    f[maxThreads - 2] = async(std::launch::async, DB_ZipFile_thread, filesWithTime, begin, filesWithTime.size(), pathToLine);
    status[maxThreads - 2] = f[maxThreads - 2].wait_for(chrono::milliseconds(1));
    for (int j = 0; j < maxThreads - 1; j++) //等待所有线程结束
    {
        if (status[j] != future_status::ready)
            f[j].wait();
    }

    IOBusy = false;
    return err;
}

/**
 * @brief 还原压缩后的.idb文件为原状态，所有类型都支持，单线程
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idbzip压缩文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ReZipFile_Single(const char *ZipTemPath, const char *pathToLine)
{
    IOBusy = true;
    int err = 0;
    if(ZipTemPath==NULL || pathToLine==NULL)
        return StatusCode::EMPTY_SAVE_PATH;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;

    vector<pair<string, long>> filesWithTime;
    readIDBZIPFilesWithTimestamps(pathToLine, filesWithTime); //获取所有.idbzip文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(filesWithTime, TIME_ASC); //将文件按时间升序

    for (size_t fileNum = 0; fileNum < filesWithTime.size(); fileNum++) //循环以给每个.idbzip文件进行还原处理
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(filesWithTime[fileNum].first.c_str()), &len);
        char *readbuff = new char[len];                            //文件内容
        char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入被还原的数据

        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(filesWithTime[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            IOBusy = false;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        err = ReZipBuf(readbuff, len, &writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
        if (err)
            continue;

        DB_DeleteFile(const_cast<char *>(filesWithTime[fileNum].first.c_str())); //删除原文件
        long fp;
        string finalpath = filesWithTime[fileNum].first.substr(0, filesWithTime[fileNum].first.length() - 3); //去掉后缀的zip
        //创建新文件并写入
        char mode[2] = {'w', 'b'};
        err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
        if (err == 0)
        {
            // err = DB_Write(fp, writebuff, writebuff_pos);
            err = fwrite(writebuff, writebuff_pos, 1, (FILE *)fp);
            err = DB_Close(fp);
        }

        // int fd = sysOpen(const_cast<char *>(finalpath.c_str()));
        // err = write(fd, writebuff, writebuff_pos);
        // if (err == -1)
        //     return errno;
        // err = close(fd);
    }
    IOBusy = false;
    return err;
}

/**
 * @brief 按文件夹还原已有的.idbzip文件,所有数据类型都支持，多线程，内核数>2时才有效
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idbzip文件路径
 * @return  0:success,
 *          other:StatusCode
 */
int DB_ReZipFile(const char *ZipTemPath, const char *pathToLine)
{
    int err;
    if(ZipTemPath==NULL || pathToLine==NULL)
        return StatusCode::EMPTY_SAVE_PATH;
    maxThreads = thread::hardware_concurrency();
    if (maxThreads <= 2) //如果内核数小于等于2则不如直接使用单线程
    {
        err = DB_ReZipFile_Single(ZipTemPath, pathToLine);
        return err;
    }

    IOBusy = true;

    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    vector<pair<string, long>> filesWithTime;
    readIDBZIPFilesWithTimestamps(pathToLine, filesWithTime); //获取所有.idb文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    //如果文件数小于1000则采用单线程还原
    if (filesWithTime.size() < 1000)
    {
        IOBusy = false;
        err = DB_ReZipFile_Single(ZipTemPath, pathToLine);
        return err;
    }
    sortByTime(filesWithTime, TIME_ASC); //将文件按时间升序

    uint16_t singleThreadFileNum = filesWithTime.size() / (maxThreads - 1);
    future_status status[maxThreads - 1];
    future<int> f[maxThreads - 1];
    uint16_t begin = 0;
    for (int j = 0; j < maxThreads - 2; j++) //循环开启多线程
    {
        f[j] = async(std::launch::async, DB_ReZipFile_thread, filesWithTime, begin, singleThreadFileNum * (j + 1), pathToLine);
        begin += singleThreadFileNum;
        status[j] = f[j].wait_for(chrono::milliseconds(1));
    }
    f[maxThreads - 2] = async(std::launch::async, DB_ReZipFile_thread, filesWithTime, begin, filesWithTime.size(), pathToLine);
    status[maxThreads - 2] = f[maxThreads - 2].wait_for(chrono::milliseconds(1));
    for (int j = 0; j < maxThreads - 1; j++) //等待所有线程结束
    {
        if (status[j] != future_status::ready)
            f[j].wait();
    }

    IOBusy = false;
    return err;
}

/**
 * @brief 压缩收到的一整条数据
 *
 * @param ZipTemPath 压缩模板路径
 * @param filepath 存储文件路径
 * @param buff 接收到的数据
 * @param buffLength 接收到的数据长度
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipRecvBuff(const char *ZipTemPath, const char *filepath, char *buff, long *buffLength)
{
    int err = 0;
    if(ZipTemPath==NULL)
        return StatusCode::EMPTY_SAVE_PATH;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    long len = *buffLength;
    char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据

    DataTypeConverter converter;
    long writebuff_pos = 0;

    err = ZipBuf(buff, &writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后数据在writebuff里
    if (err)
        return err;
    if (writebuff_pos >= len) //表明数据没有被压缩
    {
        char isZip[1] = {0};
        char finnalBuf[len + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, buff, *buffLength);
        memcpy(buff, finnalBuf, len + 1);
        *buffLength += 1;
    }
    else if (writebuff_pos == 0) //数据完全压缩
    {
        char isZip[1] = {1};
        char finnalBuf[writebuff_pos + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, writebuff, writebuff_pos);
        memcpy(buff, finnalBuf, writebuff_pos + 1);
        *buffLength = writebuff_pos + 1;
    }
    else //数据未完全压缩
    {
        char isZip[1] = {2};
        char finnalBuf[writebuff_pos + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, writebuff, writebuff_pos);
        memcpy(buff, finnalBuf, writebuff_pos + 1);
        *buffLength = writebuff_pos + 1;
    }

    DB_DataBuffer databuff;
    databuff.length = *buffLength;
    databuff.buffer = buff;
    databuff.savePath = filepath;
    DB_InsertRecord(&databuff, 1);
    delete[] writebuff;
    return err;
}

/**
 * @brief 根据时间段压缩.idb文件，所有类型都支持，单线程
 *
 * @param params 压缩请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ZipFileByTimeSpan_Single(struct DB_ZipParams *params)
{
    IOBusy = true;
    params->ZipType = TIME_SPAN;
    int err = CheckZipParams(params);
    if (err != 0)
        return err;

    vector<pair<string, long>> filesWithTime, selectedFiles;
    readIDBFilesWithTimestamps(params->pathToLine, filesWithTime); //获取所有.idb文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }
    if (selectedFiles.size() == 0)
    {
        cout << "没有这个时间段的文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(selectedFiles, TIME_ASC);

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;

    for (size_t fileNum = 0; fileNum < selectedFiles.size(); fileNum++) //循环以给每个.idb文件进行压缩处理
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
        char *readbuff = new char[len];                                                                    //文件内容
        char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据
        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            IOBusy = false;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        err = ZipBuf(readbuff, &writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后数据在writebuff里
        if (err)
            continue;

        if (writebuff_pos >= len) //表明数据没有被压缩,保持原文件
        {
            cout << selectedFiles[fileNum].first + "文件数据没有被压缩!" << endl;
            // return 1;//1表示数据没有被压缩
        }
        else
        {
            DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
            long fp;
            string finalpath = selectedFiles[fileNum].first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
            //创建新文件并写入
            char mode[2] = {'w', 'b'};
            err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
            if (err == 0)
            {
                if (writebuff_pos != 0)
                    err = DB_Write(fp, writebuff, writebuff_pos);
                err = DB_Close(fp);
            }
        }
        delete[] readbuff;
        delete[] writebuff;
    }
    IOBusy = false;
    return err;
}

/**
 * @brief 根据时间段压缩.idb文件，所有类型都支持，多线程，内核数>2时才有效
 *
 * @param params 压缩请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ZipFileByTimeSpan(struct DB_ZipParams *params)
{
    params->ZipType = TIME_SPAN;
    int err = CheckZipParams(params);
    if (err != 0)
        return err;

    maxThreads = thread::hardware_concurrency();
    if (maxThreads <= 2) //如果内核数小于等于2则不如使用单线程
    {
        err = DB_ZipFileByTimeSpan_Single(params);
        return err;
    }

    IOBusy = true;

    vector<pair<string, long>> filesWithTime, selectedFiles;
    readIDBFilesWithTimestamps(params->pathToLine, filesWithTime); //获取所有.idb文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }
    if (selectedFiles.size() == 0)
    {
        cout << "没有这个时间段的文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    //如果文件数小于1000则采用单线程压缩
    if (selectedFiles.size() < 1000)
    {
        IOBusy = false;
        err = DB_ZipFileByTimeSpan_Single(params);
        return err;
    }
    sortByTime(selectedFiles, TIME_ASC);

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    uint16_t singleThreadFileNum = selectedFiles.size() / maxThreads;
    future_status status[maxThreads];
    future<int> f[maxThreads];
    uint16_t begin = 0;
    for (int j = 0; j < maxThreads - 1; j++) //循环开启线程
    {
        f[j] = async(std::launch::async, DB_ZipFile_thread, selectedFiles, begin, singleThreadFileNum * (j + 1), params->pathToLine);
        begin += singleThreadFileNum;
        status[j] = f[j].wait_for(chrono::milliseconds(1));
    }
    f[maxThreads - 1] = async(std::launch::async, DB_ZipFile_thread, selectedFiles, begin, selectedFiles.size(), params->pathToLine);
    status[maxThreads - 1] = f[maxThreads - 1].wait_for(chrono::milliseconds(1));
    for (int j = 0; j < maxThreads - 1; j++) //等待线程结束
    {
        if (status[j] != future_status::ready)
            f[j].wait();
    }

    IOBusy = false;
    return err;
}

/**
 * @brief 根据时间段还原.idbzip文件，单线程
 *
 * @param params 压缩请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ReZipFileByTimeSpan_Single(struct DB_ZipParams *params)
{
    IOBusy = true;
    params->ZipType = TIME_SPAN;
    int err = CheckZipParams(params);
    if (err != 0)
        return err;

    vector<pair<string, long>> filesWithTime, selectedFiles;
    readIDBZIPFilesWithTimestamps(params->pathToLine, filesWithTime); //获取所有.idbzip文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }
    if (selectedFiles.size() == 0)
    {
        cout << "没有这个时间段的文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(selectedFiles, TIME_ASC);

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;

    for (size_t fileNum = 0; fileNum < selectedFiles.size(); fileNum++) //循环以给每个.idbzip文件进行还原处理
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
        char *readbuff = new char[len];                            //文件内容
        char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入被还原的数据
        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            IOBusy = false;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        err = ReZipBuf(readbuff, len, &writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
        if (err)
            continue;

        DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
        long fp;
        string finalpath = selectedFiles[fileNum].first.substr(0, selectedFiles[fileNum].first.length() - 3); //去掉后缀的zip
        //创建新文件并写入
        char mode[2] = {'w', 'b'};
        err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
        if (err == 0)
        {
            err = DB_Write(fp, writebuff, writebuff_pos);
            err = DB_Close(fp);
        }
    }
    IOBusy = false;
    return err;
}

/**
 * @brief 根据时间段还原.idbzip文件，单线程
 *
 * @param params 压缩请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ReZipFileByTimeSpan(struct DB_ZipParams *params)
{
    params->ZipType = TIME_SPAN;
    int err = CheckZipParams(params);
    if (err != 0)
        return err;

    maxThreads = thread::hardware_concurrency();
    if (maxThreads <= 2) //如果内核数小于等于2则不如使用单线程
    {
        err = DB_ReZipFileByTimeSpan_Single(params);
        return err;
    }

    IOBusy = true;

    vector<pair<string, long>> filesWithTime, selectedFiles;
    readIDBZIPFilesWithTimestamps(params->pathToLine, filesWithTime); //获取所有.idb文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }
    if (selectedFiles.size() == 0)
    {
        cout << "没有这个时间段的文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    //如果文件数小于1000则采用单线程还原
    if (selectedFiles.size() < 1000)
    {
        IOBusy = false;
        err = DB_ReZipFileByTimeSpan_Single(params);
        return err;
    }
    sortByTime(selectedFiles, TIME_ASC);

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    uint16_t singleThreadFileNum = selectedFiles.size() / maxThreads;
    future_status status[maxThreads];
    future<int> f[maxThreads];
    uint16_t begin = 0;
    for (int j = 0; j < maxThreads - 1; j++) //循环开启线程
    {
        f[j] = async(std::launch::async, DB_ReZipFile_thread, selectedFiles, begin, singleThreadFileNum * (j + 1), params->pathToLine);
        begin += singleThreadFileNum;
        status[j] = f[j].wait_for(chrono::milliseconds(1));
    }
    f[maxThreads - 1] = async(std::launch::async, DB_ReZipFile_thread, selectedFiles, begin, selectedFiles.size(), params->pathToLine);
    status[maxThreads - 1] = f[maxThreads - 1].wait_for(chrono::milliseconds(1));
    for (int j = 0; j < maxThreads - 1; j++) //等待线程结束
    {
        if (status[j] != future_status::ready)
            f[j].wait();
    }

    IOBusy = false;
    return err;
}

/**
 * @brief 根据文件ID压缩.idb文件
 *
 * @param params 压缩请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ZipFileByFileID_Single(struct DB_ZipParams *params)
{
    params->ZipType = FILE_ID;
    int err = CheckZipParams(params);
    if (err != 0)
        return err;

    string pathToLine = params->pathToLine;
    string fileid = params->fileID; //用于记录fileID，前面会带产线名称
    while (pathToLine[pathToLine.length() - 1] == '/')
    {
        pathToLine.pop_back();
    }

    vector<string> paths = DataType::splitWithStl(pathToLine, "/");
    if (paths.size() > 0)
    {
        if (fileid.find(paths[paths.size() - 1]) == string::npos)
            fileid = paths[paths.size() - 1] + fileid;
    }
    else
    {
        if (fileid.find(paths[0]) == string::npos)
            fileid = paths[0] + fileid;
    }

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    if (params->zipNums > 1) //根据SID和zipNums进行压缩
    {
        vector<pair<string, long>> selectedFiles;
        readIDBFilesListBySIDandNum(params->pathToLine, params->fileID, params->zipNums, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        for (auto fileNum = 0; fileNum < selectedFiles.size(); fileNum++) //循环以给每个.idb文件进行压缩处理
        {
            long len;
            DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
            char *readbuff = new char[len];                                                                    //文件内容
            char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据
            long writebuff_pos = 0;

            if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
            {
                cout << "未找到文件" << endl;
                return StatusCode::DATAFILE_NOT_FOUND;
            }

            err = ZipBuf(readbuff, &writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后的数据存在writebuff中
            if (err)
                continue;

            if (writebuff_pos >= len) //表明数据没有被压缩,不做处理
            {
                cout << selectedFiles[fileNum].first + "文件数据没有被压缩!" << endl;
                // return 1;//1表示数据没有被压缩
            }
            else
            {
                DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
                long fp;
                string finalpath = selectedFiles[fileNum].first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
                //创建新文件并写入
                char mode[2] = {'w', 'b'};

                err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
                if (err == 0)
                {
                    if (writebuff_pos != 0)
                        err = DB_Write(fp, writebuff, writebuff_pos);
                    err = DB_Close(fp);
                }
            }
            delete[] readbuff;
            delete[] writebuff;
        }
    }
    else if (params->EID != NULL) //根据SID和EID进行压缩
    {
        vector<pair<string, long>> selectedFiles;
        readIDBFilesListBySIDandEID(params->pathToLine, params->fileID, params->EID, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        for (auto fileNum = 0; fileNum < selectedFiles.size(); fileNum++) //循环以给每个.idb文件进行压缩处理
        {
            long len;
            DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
            char *readbuff = new char[len];                                                                    //文件内容
            char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据
            long writebuff_pos = 0;

            if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
            {
                cout << "未找到文件" << endl;
                IOBusy = false;
                return StatusCode::DATAFILE_NOT_FOUND;
            }

            err = ZipBuf(readbuff, &writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后数据在writebuff里
            if (err)
                continue;

            if (writebuff_pos >= len) //表明数据没有被压缩,不做处理
            {
                cout << selectedFiles[fileNum].first + "文件数据没有被压缩!" << endl;
                // return 1;//1表示数据没有被压缩
            }
            else
            {
                DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
                long fp;
                string finalpath = selectedFiles[fileNum].first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
                //创建新文件并写入
                char mode[2] = {'w', 'b'};

                err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
                if (err == 0)
                {
                    if (writebuff_pos != 0)
                        err = DB_Write(fp, writebuff, writebuff_pos);
                    err = DB_Close(fp);
                }
            }
            delete[] readbuff;
            delete[] writebuff;
        }
    }
    else //只压缩一个SID
    {
        vector<pair<string, long>> Files;
        readIDBFilesWithTimestamps(params->pathToLine, Files);
        if (Files.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        sortByTime(Files, TIME_ASC);
        int flag = 0;

        for (auto &file : Files) //遍历寻找ID
        {
            if (file.first.find(fileid) != string::npos) //找到了
            {
                flag = 1;
                long len;
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char *readbuff = new char[len];                                                                    //文件内容
                char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据
                long writebuff_pos = 0;

                if (DB_OpenAndRead(const_cast<char *>(file.first.c_str()), readbuff)) //将文件内容读取到readbuff
                {
                    cout << "未找到文件" << endl;
                    return StatusCode::DATAFILE_NOT_FOUND;
                }

                err = ZipBuf(readbuff, &writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后的数据存在writebuff中
                if (err)
                    break;

                if (writebuff_pos >= len) //表明数据没有被压缩,不做处理
                {
                    cout << file.first + "文件数据没有被压缩!" << endl;
                    // return 1;//1表示数据没有被压缩
                    char *data = (char *)malloc(len);
                    memcpy(data, readbuff, len);
                    params->buffer = data; //将数据记录在params->buffer中
                    params->bufferLen = len;
                }
                else
                {
                    char *data = (char *)malloc(writebuff_pos);
                    memcpy(data, writebuff, writebuff_pos);
                    params->buffer = data; //将压缩后数据记录在params->buffer中
                    params->bufferLen = writebuff_pos;

                    DB_DeleteFile(const_cast<char *>(file.first.c_str())); //删除原文件
                    long fp;
                    string finalpath = file.first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
                    //创建新文件并写入
                    char mode[2] = {'w', 'b'};
                    err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
                    if (err == 0)
                    {
                        if (writebuff_pos != 0)
                            err = DB_Write(fp, writebuff, writebuff_pos);

                        if (err == 0)
                        {
                            DB_Close(fp);
                        }
                    }
                }
                delete[] readbuff;
                delete[] writebuff;
                break;
            }
        }
        if (flag == 0)
            return StatusCode::DATAFILE_NOT_FOUND;
    }
    return err;
}

/**
 * @brief 多线程按时间段压缩.idb文件函数，内核数>２时才有效
 *
 * @param params 压缩请求参数
 * @return int
 */
int DB_ZipFileByFileID(struct DB_ZipParams *params)
{
    params->ZipType = FILE_ID;
    int err = CheckZipParams(params);
    if (err != 0)
        return err;

    maxThreads = thread::hardware_concurrency();
    if (maxThreads <= 2) //如果内核数小于等于2则不如使用单线程
    {
        err = DB_ZipFileByFileID_Single(params);
        return err;
    }

    string pathToLine = params->pathToLine;
    string fileid = params->fileID; //用于记录fileID，前面会带产线名称
    while (pathToLine[pathToLine.length() - 1] == '/')
    {
        pathToLine.pop_back();
    }

    vector<string> paths = DataType::splitWithStl(pathToLine, "/");
    if (paths.size() > 0)
    {
        if (fileid.find(paths[paths.size() - 1]) == string::npos)
            fileid = paths[paths.size() - 1] + fileid;
    }
    else
    {
        if (fileid.find(paths[0]) == string::npos)
            fileid = paths[0] + fileid;
    }

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    IOBusy = true;

    if (params->zipNums > 1) //根据SID和zipNums进行压缩
    {
        vector<pair<string, long>> selectedFiles;
        readIDBFilesListBySIDandNum(params->pathToLine, params->fileID, params->zipNums, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        //如果文件数小于1000则采用单线程压缩
        if (selectedFiles.size() < 1000)
        {
            IOBusy = false;
            err = DB_ZipFileByFileID_Single(params);
            return err;
        }

        uint16_t singleThreadFileNum = selectedFiles.size() / maxThreads;
        future_status status[maxThreads];
        future<int> f[maxThreads];
        uint16_t begin = 0;
        for (int j = 0; j < maxThreads - 1; j++) //循环开启线程
        {
            f[j] = async(std::launch::async, DB_ZipFile_thread, selectedFiles, begin, singleThreadFileNum * (j + 1), params->pathToLine);
            begin += singleThreadFileNum;
            status[j] = f[j].wait_for(chrono::milliseconds(1));
        }
        f[maxThreads - 1] = async(std::launch::async, DB_ZipFile_thread, selectedFiles, begin, selectedFiles.size(), params->pathToLine);
        status[maxThreads - 1] = f[maxThreads - 1].wait_for(chrono::milliseconds(1));
        for (int j = 0; j < maxThreads - 1; j++) //等待线程结束
        {
            if (status[j] != future_status::ready)
                f[j].wait();
        }

        IOBusy = false;
        return err;
    }
    else if (params->EID != NULL) //根据SID和EID进行压缩
    {
        vector<pair<string, long>> selectedFiles;
        readIDBFilesListBySIDandEID(params->pathToLine, params->fileID, params->EID, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        //如果文件数小于1000则采用单线程压缩
        if (selectedFiles.size() < 1000)
        {
            IOBusy = false;
            err = DB_ZipFileByFileID_Single(params);
            return err;
        }

        uint16_t singleThreadFileNum = selectedFiles.size() / maxThreads;
        future_status status[maxThreads];
        future<int> f[maxThreads];
        uint16_t begin = 0;
        for (int j = 0; j < maxThreads - 1; j++) //循环开启线程
        {
            f[j] = async(std::launch::async, DB_ZipFile_thread, selectedFiles, begin, singleThreadFileNum * (j + 1), params->pathToLine);
            begin += singleThreadFileNum;
            status[j] = f[j].wait_for(chrono::milliseconds(1));
        }
        f[maxThreads - 1] = async(std::launch::async, DB_ZipFile_thread, selectedFiles, begin, selectedFiles.size(), params->pathToLine);
        status[maxThreads - 1] = f[maxThreads - 1].wait_for(chrono::milliseconds(1));
        for (int j = 0; j < maxThreads - 1; j++) //等待线程结束
        {
            if (status[j] != future_status::ready)
                f[j].wait();
        }

        IOBusy = false;
        return err;
    }
    else //只压缩一个SID
    {
        IOBusy = false;
        vector<pair<string, long>> Files;
        readIDBFilesWithTimestamps(params->pathToLine, Files);
        if (Files.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        sortByTime(Files, TIME_ASC);
        int flag = 0;

        for (auto &file : Files) //遍历寻找ID
        {
            if (file.first.find(fileid) != string::npos) //找到了
            {
                flag = 1;
                long len;
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char *readbuff = new char[len];                                                                    //文件内容
                char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据
                long writebuff_pos = 0;

                if (DB_OpenAndRead(const_cast<char *>(file.first.c_str()), readbuff)) //将文件内容读取到readbuff
                {
                    cout << "未找到文件" << endl;
                    return StatusCode::DATAFILE_NOT_FOUND;
                }

                err = ZipBuf(readbuff, &writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后的数据存在writebuff中
                if (err)
                    break;

                if (writebuff_pos >= len) //表明数据没有被压缩,不做处理
                {
                    cout << file.first + "文件数据没有被压缩!" << endl;
                    // return 1;//1表示数据没有被压缩
                    char *data = (char *)malloc(len);
                    memcpy(data, readbuff, len);
                    params->buffer = data; //将数据记录在params->buffer中
                    params->bufferLen = len;
                }
                else
                {
                    char *data = (char *)malloc(writebuff_pos);
                    memcpy(data, writebuff, writebuff_pos);
                    params->buffer = data; //将压缩后数据记录在params->buffer中
                    params->bufferLen = writebuff_pos;

                    DB_DeleteFile(const_cast<char *>(file.first.c_str())); //删除原文件
                    long fp;
                    string finalpath = file.first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
                    //创建新文件并写入
                    char mode[2] = {'w', 'b'};
                    err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
                    if (err == 0)
                    {
                        if (writebuff_pos != 0)
                            err = DB_Write(fp, writebuff, writebuff_pos);

                        if (err == 0)
                        {
                            DB_Close(fp);
                        }
                    }
                }
                delete[] readbuff;
                delete[] writebuff;
                break;
            }
        }
        if (flag == 0)
            return StatusCode::DATAFILE_NOT_FOUND;
    }
    return err;
}

/**
 * @brief 根据文件ID还原.idbzip文件
 *
 * @param params 还原请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ReZipFileByFileID_Single(struct DB_ZipParams *params)
{
    params->ZipType = FILE_ID;
    int err = CheckZipParams(params);
    if (err != 0)
        return err;

    string pathToLine = params->pathToLine;
    string fileid = params->fileID; //用于记录fileID，前面会带产线名称
    while (pathToLine[pathToLine.length() - 1] == '/')
    {
        pathToLine.pop_back();
    }

    vector<string> paths = DataType::splitWithStl(pathToLine, "/");
    if (paths.size() > 0)
    {
        if (fileid.find(paths[paths.size() - 1]) == string::npos)
            fileid = paths[paths.size() - 1] + fileid;
    }
    else
    {
        if (fileid.find(paths[0]) == string::npos)
            fileid = paths[0] + fileid;
    }

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    if (params->zipNums > 1) //根据SID和zipNums进行还原
    {
        vector<pair<string, long>> selectedFiles;
        readIDBZIPFilesListBySIDandNum(params->pathToLine, params->fileID, params->zipNums, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        for (auto fileNum = 0; fileNum < selectedFiles.size(); fileNum++) //循环以给每个.idbzip文件进行压缩处理
        {
            long len;
            DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
            char *readbuff = new char[len];                            //文件内容
            char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入被还原的数据
            long writebuff_pos = 0;

            if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
            {
                cout << "未找到文件" << endl;
                IOBusy = false;
                return StatusCode::DATAFILE_NOT_FOUND;
            }

            err = ReZipBuf(readbuff, len, &writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
            if (err)
                continue;

            DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
            long fp;
            string finalpath = selectedFiles[fileNum].first.substr(0, selectedFiles[fileNum].first.length() - 3); //去掉后缀的zip
            //创建新文件并写入
            char mode[2] = {'w', 'b'};

            err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
            if (err == 0)
            {
                err = DB_Write(fp, writebuff, writebuff_pos);
                err = DB_Close(fp);
            }
            delete[] readbuff;
            delete[] writebuff;
        }
    }
    else if (params->EID != NULL) //根据SID和EID进行还原
    {
        vector<pair<string, long>> selectedFiles;
        readIDBZIPFilesListBySIDandEID(params->pathToLine, params->fileID, params->EID, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        for (auto fileNum = 0; fileNum < selectedFiles.size(); fileNum++) //循环以给每个.idbzip文件进行压缩处理
        {
            long len;
            DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
            char *readbuff = new char[len];                            //文件内容
            char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入被还原的数据
            long writebuff_pos = 0;

            if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
            {
                cout << "未找到文件" << endl;
                IOBusy = false;
                return StatusCode::DATAFILE_NOT_FOUND;
            }

            err = ReZipBuf(readbuff, len, &writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
            if (err)
                continue;

            DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
            long fp;
            string finalpath = selectedFiles[fileNum].first.substr(0, selectedFiles[fileNum].first.length() - 3); //去掉后缀的zip
            //创建新文件并写入
            char mode[2] = {'w', 'b'};

            err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
            if (err == 0)
            {
                err = DB_Write(fp, writebuff, writebuff_pos);
                DB_Close(fp);
            }
            delete[] readbuff;
            delete[] writebuff;
        }
    }
    else //只还原SID一个文件
    {
        vector<pair<string, long>> Files;
        readIDBZIPFilesWithTimestamps(params->pathToLine, Files);
        if (Files.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        sortByTime(Files, TIME_ASC);
        int flag = 0;

        for (auto &file : Files) //遍历寻找ID
        {
            if (file.first.find(fileid) != string::npos) //找到了
            {
                flag = 1;
                long len;
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char *readbuff = new char[len];                            //文件内容
                char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入没有被压缩的数据
                long writebuff_pos = 0;

                if (DB_OpenAndRead(const_cast<char *>(file.first.c_str()), readbuff)) //将文件内容读取到readbuff
                {
                    cout << "未找到文件" << endl;
                    return StatusCode::DATAFILE_NOT_FOUND;
                }

                err = ReZipBuf(readbuff, len, &writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
                if (err)
                    break;

                char *data = (char *)malloc(writebuff_pos);
                memcpy(data, writebuff, writebuff_pos);
                params->buffer = data; //将还原后的数据记录在params->buffer中
                params->bufferLen = writebuff_pos;

                DB_DeleteFile(const_cast<char *>(file.first.c_str())); //删除原文件
                long fp;
                string finalpath = file.first.substr(0, file.first.length() - 3); //去掉后缀的zip
                //创建新文件并写入
                char mode[2] = {'w', 'b'};
                err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
                if (err == 0)
                {
                    err = DB_Write(fp, writebuff, writebuff_pos);

                    if (err == 0)
                    {
                        DB_Close(fp);
                    }
                }
                delete[] readbuff;
                delete[] writebuff;
                break;
            }
        }
        if (flag == 0)
            return StatusCode::DATAFILE_NOT_FOUND;
    }
    return err;
}

/**
 * @brief 多线程按时间段还原.idbzip文件函数，内核数>２时才有效
 *
 * @param params 还原请求参数
 * @return int
 */
int DB_ReZipFileByFileID(struct DB_ZipParams *params)
{
    params->ZipType = FILE_ID;
    int err = CheckZipParams(params);
    if (err != 0)
        return err;

    maxThreads = thread::hardware_concurrency();
    if (maxThreads <= 2) //如果内核数小于等于2则不如使用单线程
    {
        err = DB_ReZipFileByFileID_Single(params);
        return err;
    }

    string pathToLine = params->pathToLine;
    string fileid = params->fileID; //用于记录fileID，前面会带产线名称
    while (pathToLine[pathToLine.length() - 1] == '/')
    {
        pathToLine.pop_back();
    }

    vector<string> paths = DataType::splitWithStl(pathToLine, "/");
    if (paths.size() > 0)
    {
        if (fileid.find(paths[paths.size() - 1]) == string::npos)
            fileid = paths[paths.size() - 1] + fileid;
    }
    else
    {
        if (fileid.find(paths[0]) == string::npos)
            fileid = paths[0] + fileid;
    }

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    IOBusy = true;

    if (params->zipNums > 1) //根据SID和zipNums进行还原
    {
        vector<pair<string, long>> selectedFiles;
        readIDBZIPFilesListBySIDandNum(params->pathToLine, params->fileID, params->zipNums, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            IOBusy = false;
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        //如果文件数小于1000则采用单线程还原
        if (selectedFiles.size() < 1000)
        {
            IOBusy = false;
            err = DB_ReZipFileByFileID_Single(params);
            return err;
        }

        uint16_t singleThreadFileNum = selectedFiles.size() / maxThreads;
        future_status status[maxThreads];
        future<int> f[maxThreads];
        uint16_t begin = 0;
        for (int j = 0; j < maxThreads - 1; j++) //循环开启线程
        {
            f[j] = async(std::launch::async, DB_ReZipFile_thread, selectedFiles, begin, singleThreadFileNum * (j + 1), params->pathToLine);
            begin += singleThreadFileNum;
            status[j] = f[j].wait_for(chrono::milliseconds(1));
        }
        f[maxThreads - 1] = async(std::launch::async, DB_ReZipFile_thread, selectedFiles, begin, selectedFiles.size(), params->pathToLine);
        status[maxThreads - 1] = f[maxThreads - 1].wait_for(chrono::milliseconds(1));
        for (int j = 0; j < maxThreads - 1; j++) //等待线程结束
        {
            if (status[j] != future_status::ready)
                f[j].wait();
        }

        IOBusy = false;
        return err;
    }
    else if (params->EID != NULL) //根据SID和EID进行还原
    {
        vector<pair<string, long>> selectedFiles;
        readIDBZIPFilesListBySIDandEID(params->pathToLine, params->fileID, params->EID, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        //如果文件数小于1000则采用单线程还原
        if (selectedFiles.size() < 1000)
        {
            IOBusy = false;
            err = DB_ReZipFileByFileID_Single(params);
            return err;
        }

        uint16_t singleThreadFileNum = selectedFiles.size() / maxThreads;
        future_status status[maxThreads];
        future<int> f[maxThreads];
        uint16_t begin = 0;
        for (int j = 0; j < maxThreads - 1; j++) //循环开启线程
        {
            f[j] = async(std::launch::async, DB_ReZipFile_thread, selectedFiles, begin, singleThreadFileNum * (j + 1), params->pathToLine);
            begin += singleThreadFileNum;
            status[j] = f[j].wait_for(chrono::milliseconds(1));
        }
        f[maxThreads - 1] = async(std::launch::async, DB_ReZipFile_thread, selectedFiles, begin, selectedFiles.size(), params->pathToLine);
        status[maxThreads - 1] = f[maxThreads - 1].wait_for(chrono::milliseconds(1));
        for (int j = 0; j < maxThreads - 1; j++) //等待线程结束
        {
            if (status[j] != future_status::ready)
                f[j].wait();
        }

        IOBusy = false;
        return err;
    }
    else //只还原SID一个文件
    {
        IOBusy = false;
        vector<pair<string, long>> Files;
        readIDBZIPFilesWithTimestamps(params->pathToLine, Files);
        if (Files.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        sortByTime(Files, TIME_ASC);
        int flag = 0;

        for (auto &file : Files) //遍历寻找ID
        {
            if (file.first.find(fileid) != string::npos) //找到了
            {
                flag = 1;
                long len;
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char *readbuff = new char[len];                            //文件内容
                char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入没有被压缩的数据
                long writebuff_pos = 0;

                if (DB_OpenAndRead(const_cast<char *>(file.first.c_str()), readbuff)) //将文件内容读取到readbuff
                {
                    cout << "未找到文件" << endl;
                    return StatusCode::DATAFILE_NOT_FOUND;
                }

                err = ReZipBuf(readbuff, len, &writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
                if (err)
                    break;

                char *data = (char *)malloc(writebuff_pos);
                memcpy(data, writebuff, writebuff_pos);
                params->buffer = data; //将还原后的数据记录在params->buffer中
                params->bufferLen = writebuff_pos;

                DB_DeleteFile(const_cast<char *>(file.first.c_str())); //删除原文件
                long fp;
                string finalpath = file.first.substr(0, file.first.length() - 3); //去掉后缀的zip
                //创建新文件并写入
                char mode[2] = {'w', 'b'};
                err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
                if (err == 0)
                {
                    err = DB_Write(fp, writebuff, writebuff_pos);

                    if (err == 0)
                    {
                        DB_Close(fp);
                    }
                }
                delete[] readbuff;
                delete[] writebuff;
                break;
            }
        }
        if (flag == 0)
            return StatusCode::DATAFILE_NOT_FOUND;
    }
    return err;
}

// int main()
// {
//     DB_ZipParams param;
//     param.ZipType = FILE_ID;
//     param.pathToLine = "RobotTsTest";
//     param.fileID = "RobotTsTest2";
//     DB_ZipFileByFileID(&param);
//     return 0;
//     // std::cout<<"start 多类型文件 单线程压缩"<<std::endl;
//     // auto startTime = std::chrono::system_clock::now();
//     // DB_ZipFile("jinfei","jinfei");
//     // auto endTime = std::chrono::system_clock::now();
//     // std::cout << "多类型文件 单线程压缩耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     // std::cout<<"end 多类型文件 单线程压缩"<<std::endl<<std::endl;

//     // sleep(3);
//     // std::cout<<"start 多类型文件 单线程还原"<<std::endl;
//     // startTime = std::chrono::system_clock::now();
//     // DB_ReZipFile("jinfei","jinfei");
//     // endTime = std::chrono::system_clock::now();
//     // std::cout << "多类型文件 单线程还原耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     // std::cout<<"end 多类型文件 单线程还原"<<std::endl<<std::endl;

//     // sleep(3);
//     // std::cout<<"start 多类型文件 多线程压缩"<<std::endl;
//     // startTime = std::chrono::system_clock::now();
//     // DB_ZipFile_MultiThread("jinfei", "jinfei");
//     // endTime = std::chrono::system_clock::now();
//     // std::cout << "多类型文件 多线程压缩耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     // std::cout<<"end 多类型文件 多线程压缩"<<std::endl<<std::endl;

//     // sleep(3);
//     // std::cout<<"start 多类型文件 多线程还原"<<std::endl;
//     // startTime = std::chrono::system_clock::now();
//     // DB_ReZipFile_MultiThread("jinfei", "jinfei");
//     // endTime = std::chrono::system_clock::now();
//     // std::cout << "多类型文件 多线程还原耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     // std::cout<<"end 多类型文件 多线程还原"<<std::endl<<std::endl;

//     struct tm t;
//     t.tm_year = atoi("2022") - 1900;
//     t.tm_mon = atoi("5") - 1;
//     t.tm_mday = atoi("7");
//     t.tm_hour = atoi("15");
//     t.tm_min = atoi("0");
//     t.tm_sec = atoi("0");
//     t.tm_isdst = -1; //不设置夏令时
//     time_t seconds = mktime(&t);
//     int ms = atoi("0");
//     long start = seconds * 1000 + ms;
//     cout << start << endl;
//     DB_ZipParams zipParam;
//     zipParam.pathToLine = "jinfei";
//     zipParam.start = 1651903200000;
//     zipParam.end = 1651906800000;

//     sleep(3);
//     std::cout << "start 多类型文件按时间段 单线程压缩" << std::endl;
//     auto startTime = std::chrono::system_clock::now();
//     DB_ZipFileByTimeSpan(&zipParam);
//     auto endTime = std::chrono::system_clock::now();
//     std::cout << "多类型文件按时间段 单线程压缩耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     std::cout << "end 多类型文件按时间段 单线程压缩" << std::endl
//               << std::endl;

//     sleep(3);
//     std::cout << "start 多类型文件按时间段 单线程还原" << std::endl;
//     startTime = std::chrono::system_clock::now();
//     DB_ReZipFileByTimeSpan(&zipParam);
//     endTime = std::chrono::system_clock::now();
//     std::cout << "多类型件按时间段 单线程还原耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     std::cout << "end 多类型文件按时间段 单线程还原" << std::endl
//               << std::endl;

//     sleep(3);
//     std::cout << "start 多类型文件按时间段 多线程压缩" << std::endl;
//     startTime = std::chrono::system_clock::now();
//     DB_ZipFileByTimeSpan(&zipParam);
//     endTime = std::chrono::system_clock::now();
//     std::cout << "多类型文件按时间段 多线程时间段压缩耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     std::cout << "end 多类型文件按时间段 多线程压缩" << std::endl
//               << std::endl;

//     sleep(3);
//     std::cout << "start 多类型文件按时间段 多线程还原" << std::endl;
//     startTime = std::chrono::system_clock::now();
//     DB_ReZipFileByTimeSpan(&zipParam);
//     endTime = std::chrono::system_clock::now();
//     std::cout << "多类型文件按时间段 多线程时间段还原耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     std::cout << "end 多类型文件按时间段 多线程还原" << std::endl
//               << std::endl;

//     return 0;
// }
// int main()
// {
//     // DB_ReZipSwitchFile("JinfeiSeven", "JinfeiSeven");
//     // DB_ZipParams param;
//     // param.ZipType = FILE_ID;
//     // param.pathToLine = "JinfeiSeven";
//     // param.fileID = "JinfeiSeven1527000";
//     // param.zipNums = 100;
//     // param.EID = NULL;
//     // cout << DB_ZipFileByFileID(&param) << endl;
//     // cout << DB_ReZipFileByFileID(&param) << endl;
//     DB_ZipFile("JinfeiSeven","JinfeiSeven");
//     // DB_ReZipSwitchFile("RobotTsTest", "RobotTsTest");
//     return 0;
// }