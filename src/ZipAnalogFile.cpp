#include "../include/utils.hpp"
using namespace std;

/**
 * @brief 压缩接收到只有模拟量类型的.idb文件
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine 存储文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipAnalogFile(const char *ZipTemPath, const char *pathToLine)
{
    int err;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    vector<string> files;
    readIDBFilesList(pathToLine, files);
    if (files.size() == 0)
    {
        cout << "没有文件！" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    DataTypeConverter converter;

    for (size_t fileNum = 0; fileNum < files.size(); fileNum++) //循环以给每个.idb文件进行压缩处理
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(files[fileNum].c_str()), &len);
        char readbuff[len];                                                                    //文件内容
        char writebuff[CurrentZipTemplate.totalBytes + 2 * CurrentZipTemplate.schemas.size()]; //写入没有被压缩的数据

        long readbuff_pos = 0;
        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(files[fileNum].c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        for (int i = 0; i < CurrentZipTemplate.schemas.size(); i++)
        {
            if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentUDintValue != standardUDintValue && (currentUDintValue < maxUDintValue || currentUDintValue > minUDintValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                            writebuff_pos += 12;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 4, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 12;
                    }
                    else //不带时间戳
                    {
                        if (currentUDintValue != standardUDintValue && (currentUDintValue < maxUDintValue || currentUDintValue > minUDintValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                            writebuff_pos += 4;
                        }
                        readbuff_pos += 4;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentUSintValue != standardUSintValue && (currentUSintValue < maxUSintValue || currentUSintValue > minUSintValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 9);
                            writebuff_pos += 9;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 1, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 9;
                    }
                    else //不带时间戳
                    {
                        if (currentUSintValue != standardUSintValue && (currentUSintValue < maxUSintValue || currentUSintValue > minUSintValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                            writebuff_pos += 1;
                        }
                        readbuff_pos += 1;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentUintValue != standardUintValue && (currentUintValue < maxUintValue || currentUintValue > minUintValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 10);
                            writebuff_pos += 10;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 2, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 10;
                    }
                    else //不带时间戳
                    {
                        if (currentUintValue != standardUintValue && (currentUintValue < maxUintValue || currentUintValue > minUintValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                            writebuff_pos += 2;
                        }
                        readbuff_pos += 2;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentSintValue != standardSintValue && (currentSintValue < maxSintValue || currentSintValue > minSintValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 9);
                            writebuff_pos += 9;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 1, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 9;
                    }
                    else //不带时间戳
                    {
                        if (currentSintValue != standardSintValue && (currentSintValue < maxSintValue || currentSintValue > minSintValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                            writebuff_pos += 1;
                        }
                        CurrentZipTemplate.schemas[i].second.hasTime ? readbuff_pos += 9 : readbuff_pos += 1;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentIntValue != standardIntValue && (currentIntValue < maxIntValue || currentIntValue > minIntValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 10);
                            writebuff_pos += 10;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 2, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 10;
                    }
                    else //不带时间戳
                    {
                        if (currentIntValue != standardIntValue && (currentIntValue < maxIntValue || currentIntValue > minIntValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                            writebuff_pos += 2;
                        }
                        CurrentZipTemplate.schemas[i].second.hasTime ? readbuff_pos += 10 : readbuff_pos += 2;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentDintValue != standardDintValue && (currentDintValue < maxDintValue || currentDintValue > minDintValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                            writebuff_pos += 12;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 4, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 12;
                    }
                    else //不带时间戳
                    {
                        if (currentDintValue != standardDintValue && (currentDintValue < maxDintValue || currentDintValue > minDintValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                            writebuff_pos += 4;
                        }
                        readbuff_pos += 4;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
                }
                else
                {
                    float standardFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    float maxFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.maxValue);
                    float minFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.minValue);
                    // 4个字节,暂定，根据后续情况可能进行更改
                    char value[4] = {0};
                    memcpy(value, readbuff + readbuff_pos, 4);
                    int currentRealValue = converter.ToFloat(value);

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentRealValue != standardFloatValue && (currentRealValue < maxFloatValue || currentRealValue > minFloatValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                            writebuff_pos += 12;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 4, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 12;
                    }
                    else //不带时间戳
                    {
                        if (currentRealValue != standardFloatValue && (currentRealValue < maxFloatValue || currentRealValue > minFloatValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                            writebuff_pos += 4;
                        }
                        readbuff_pos += 4;
                    }
                }
            }
            else
            {
                cout << "存在模拟量以外的类型，请检查模板或者更换功能块" << endl;
                return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            }
        }
        if (writebuff_pos >= readbuff_pos) //表明数据没有被压缩,不做处理
        {
            cout << files[fileNum] + "文件数据没有被压缩!" << endl;
            // return 1;//1表示数据没有被压缩
        }
        else
        {
            DB_DeleteFile(const_cast<char *>(files[fileNum].c_str())); //删除原文件
            long fp;
            string finalpath = files[fileNum].append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
            //创建新文件并写入
            err = DB_Open(const_cast<char *>(finalpath.c_str()), "wb", &fp);
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
    }
    return err;
}

/**
 * @brief 还原被压缩的模拟量文件返回原状态
 *
 * @param ZipTemPath 压缩模板所在目录
 * @param pathToLine 压缩文件所在路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ReZipAnalogFile(const char *ZipTemPath, const char *pathToLine)
{
    int err = 0;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;
    vector<string> files;

    readIDBZIPFilesList(pathToLine, files);
    if (files.size() == 0)
    {
        cout << "没有.idbzip文件!" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    for (size_t fileNum = 0; fileNum < files.size(); fileNum++)
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(files[fileNum].c_str()), &len);
        char readbuff[len];                            //文件内容
        char writebuff[CurrentZipTemplate.totalBytes]; //写入没有被压缩的数据
        long readbuff_pos = 0;
        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(files[fileNum].c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++)
        {
            if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT类型
            {
                if (len == 0) //表示文件完全压缩
                {
                    uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char UDintValue[4] = {0};
                    for (int j = 0; j < 4; j++)
                    {
                        UDintValue[3 - j] |= standardUDintValue;
                        standardUDintValue >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                    writebuff_pos += 4;
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
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                    writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                    readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                    writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                    readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                            else
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                                    writebuff_pos += 12;
                                    readbuff_pos += 12;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)1) //只有时间
                                {
                                    uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                    char UDintValue[4] = {0};
                                    for (int j = 0; j < 4; j++)
                                    {
                                        UDintValue[3 - j] |= standardUDintValue;
                                        standardUDintValue >>= 8;
                                    }
                                    memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                                    writebuff_pos += 4;

                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 8);
                                    writebuff_pos += 8;
                                    readbuff_pos += 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                                    writebuff_pos += 4;
                                    readbuff_pos += 4;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                        }
                        else //不是未压缩的编号
                        {
                            uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                            char UDintValue[4] = {0};
                            for (int j = 0; j < 4; j++)
                            {
                                UDintValue[3 - j] |= standardUDintValue;
                                standardUDintValue >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                            writebuff_pos += 4;
                        }
                    }
                    else //没有未压缩的数据了
                    {
                        uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char UDintValue[4] = {0};
                        for (int j = 0; j < 4; j++)
                        {
                            UDintValue[3 - j] |= standardUDintValue;
                            standardUDintValue >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                        writebuff_pos += 4;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT类型
            {
                if (len == 0) //表示文件完全压缩
                {
                    char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                    char UsintValue[1] = {0};
                    UsintValue[0] = StandardUsintValue;
                    memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                    writebuff_pos += 1;
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
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                    writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                    readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                                    writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                    readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                            else
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 9);
                                    writebuff_pos += 9;
                                    readbuff_pos += 9;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)1) //只有时间
                                {
                                    char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                    char UsintValue[1] = {0};
                                    UsintValue[0] = StandardUsintValue;
                                    memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                                    writebuff_pos += 1;

                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 8);
                                    writebuff_pos += 8;
                                    readbuff_pos += 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                                    writebuff_pos += 1;
                                    readbuff_pos += 1;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                        }
                        else //不是未压缩的编号
                        {
                            char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                            char UsintValue[1] = {0};
                            UsintValue[0] = StandardUsintValue;
                            memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                            writebuff_pos += 1;
                        }
                    }
                    else //没有未压缩的数据了
                    {
                        char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                        char UsintValue[1] = {0};
                        UsintValue[0] = StandardUsintValue;
                        memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                        writebuff_pos += 1;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT类型
            {
                if (len == 0) //表示文件完全压缩
                {
                    uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char UintValue[2] = {0};
                    for (int j = 0; j < 2; j++)
                    {
                        UintValue[1 - j] |= standardUintValue;
                        standardUintValue >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                    writebuff_pos += 2;
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
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                    writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                    readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                    writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                    readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                            else
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 10);
                                    writebuff_pos += 10;
                                    readbuff_pos += 10;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)1) //只有时间
                                {
                                    uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                    char UintValue[2] = {0};
                                    for (int j = 0; j < 2; j++)
                                    {
                                        UintValue[1 - j] |= standardUintValue;
                                        standardUintValue >>= 8;
                                    }
                                    memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                                    writebuff_pos += 2;

                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 8);
                                    writebuff_pos += 8;
                                    readbuff_pos += 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                                    writebuff_pos += 2;
                                    readbuff_pos += 2;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                        }
                        else //不是未压缩的编号
                        {
                            uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                            char UintValue[2] = {0};
                            for (int j = 0; j < 2; j++)
                            {
                                UintValue[1 - j] |= standardUintValue;
                                standardUintValue >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                            writebuff_pos += 2;
                        }
                    }
                    else //没有未压缩的数据了
                    {
                        uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char UintValue[2] = {0};
                        for (int j = 0; j < 2; j++)
                        {
                            UintValue[1 - j] |= standardUintValue;
                            standardUintValue >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                        writebuff_pos += 2;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT类型
            {
                if (len == 0) //表示文件完全压缩
                {
                    char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                    char SintValue[1] = {0};
                    SintValue[0] = StandardSintValue;
                    memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                    writebuff_pos += 1;
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
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                    writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                    readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                    writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                    readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                            else
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 9);
                                    writebuff_pos += 9;
                                    readbuff_pos += 9;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)1) //只有时间
                                {
                                    char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                    char SintValue[1] = {0};
                                    SintValue[0] = StandardSintValue;
                                    memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                                    writebuff_pos += 1;

                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 8);
                                    writebuff_pos += 8;
                                    readbuff_pos += 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                                    writebuff_pos += 1;
                                    readbuff_pos += 1;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                        }
                        else //不是未压缩的编号
                        {
                            char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                            char SintValue[1] = {0};
                            SintValue[0] = StandardSintValue;
                            memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                            writebuff_pos += 1;
                        }
                    }
                    else //没有未压缩的数据了
                    {
                        char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                        char SintValue[1] = {0};
                        SintValue[0] = StandardSintValue;
                        memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                        writebuff_pos += 1;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT类型
            {
                if (len == 0) //表示文件完全压缩
                {
                    short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char IntValue[2] = {0};
                    for (int j = 0; j < 2; j++)
                    {
                        IntValue[1 - j] |= standardIntValue;
                        standardIntValue >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                    writebuff_pos += 2;
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
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                    writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                    readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                    writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                    readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                            else
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 10);
                                    writebuff_pos += 10;
                                    readbuff_pos += 10;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)1) //只有时间
                                {
                                    short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                    char IntValue[2] = {0};
                                    for (int j = 0; j < 2; j++)
                                    {
                                        IntValue[1 - j] |= standardIntValue;
                                        standardIntValue >>= 8;
                                    }
                                    memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                                    writebuff_pos += 2;

                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 8);
                                    writebuff_pos += 8;
                                    readbuff_pos += 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                                    writebuff_pos += 2;
                                    readbuff_pos += 2;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                        }
                        else //不是未压缩的编号
                        {
                            short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                            char IntValue[2] = {0};
                            for (int j = 0; j < 2; j++)
                            {
                                IntValue[1 - j] |= standardIntValue;
                                standardIntValue >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                            writebuff_pos += 2;
                        }
                    }
                    else //没有未压缩的数据了
                    {
                        short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char IntValue[2] = {0};
                        for (int j = 0; j < 2; j++)
                        {
                            IntValue[1 - j] |= standardIntValue;
                            standardIntValue >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                        writebuff_pos += 2;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT类型
            {
                if (len == 0) //表示文件完全压缩
                {
                    int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char DintValue[4] = {0};
                    for (int j = 0; j < 4; j++)
                    {
                        DintValue[3 - j] |= standardDintValue;
                        standardDintValue >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                    writebuff_pos += 4;
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
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                    writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                    readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                    writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                    readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                            else
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                                    writebuff_pos += 12;
                                    readbuff_pos += 12;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)1) //只有时间
                                {
                                    int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                    char DintValue[4] = {0};
                                    for (int j = 0; j < 4; j++)
                                    {
                                        DintValue[3 - j] |= standardDintValue;
                                        standardDintValue >>= 8;
                                    }
                                    memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                                    writebuff_pos += 4;

                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 8);
                                    writebuff_pos += 8;
                                    readbuff_pos += 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                                    writebuff_pos += 4;
                                    readbuff_pos += 4;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                        }
                        else //不是未压缩的编号
                        {
                            int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                            char DintValue[4] = {0};
                            for (int j = 0; j < 4; j++)
                            {
                                DintValue[3 - j] |= standardDintValue;
                                standardDintValue >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                            writebuff_pos += 4;
                        }
                    }
                    else //没有未压缩的数据了
                    {
                        int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char DintValue[4] = {0};
                        for (int j = 0; j < 4; j++)
                        {
                            DintValue[3 - j] |= standardDintValue;
                            standardDintValue >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                        writebuff_pos += 4;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL类型
            {
                if (len == 0) //表示文件完全压缩
                {
                    float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char RealValue[4] = {0};
                    void *pf;
                    pf = &standardRealValue;
                    for (int j = 0; j < 4; j++)
                    {
                        *((unsigned char *)pf + j) = RealValue[j];
                    }
                    memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                    writebuff_pos += 4;
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
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                    writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                    readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                    writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                    readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                            else
                            {
                                if (readbuff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                                    writebuff_pos += 12;
                                    readbuff_pos += 12;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)1) //只有时间
                                {
                                    float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                    char RealValue[4] = {0};
                                    void *pf;
                                    pf = &standardRealValue;
                                    for (int j = 0; j < 4; j++)
                                    {
                                        *((unsigned char *)pf + j) = RealValue[j];
                                    }
                                    memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                                    writebuff_pos += 4;

                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 8);
                                    writebuff_pos += 8;
                                    readbuff_pos += 8;
                                }
                                else if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                                {
                                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                                    writebuff_pos += 4;
                                    readbuff_pos += 4;
                                }
                                else
                                {
                                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                    return StatusCode::ZIPTYPE_ERROR;
                                }
                            }
                        }
                        else //不是未压缩的编号
                        {
                            float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                            char RealValue[4] = {0};
                            void *pf;
                            pf = &standardRealValue;
                            for (int j = 0; j < 4; j++)
                            {
                                *((unsigned char *)pf + j) = RealValue[j];
                            }
                            memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                            writebuff_pos += 4;
                        }
                    }
                    else //没有未压缩的数据了
                    {
                        float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char RealValue[4] = {0};
                        void *pf;
                        pf = &standardRealValue;
                        for (int j = 0; j < 4; j++)
                        {
                            *((unsigned char *)pf + j) = RealValue[j];
                        }
                        memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                        writebuff_pos += 4;
                    }
                }
            }
            else
            {
                cout << "存在模拟量以外的类型，请检查模板或者更换功能块" << endl;
                return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            }
        }
        // DB_DeleteFile(const_cast<char *>(files[fileNum].c_str()));//删除原文件
        long fp;
        string finalpath = files[fileNum].substr(0, files[fileNum].length() - 3); //去掉后缀的zip
        //创建新文件并写入
        err = DB_Open(const_cast<char *>(finalpath.c_str()), "wb", &fp);
        if (err == 0)
        {
            err = DB_Write(fp, writebuff, writebuff_pos);

            if (err == 0)
            {
                DB_Close(fp);
            }
        }
    }
    return err;
}

/**
 * @brief 压缩接收到的只有模拟量类型的整条数据
 *
 * @param ZipTempPath 压缩模板路径
 * @param filepath 存储文件路径
 * @param buff 接收到的要压缩的数据
 * @param buffLength 接收到数据的长度
 * @return 0:success,
 *          others:StatusCode
 */
int DB_ZipRecvAnalogFile(const char *ZipTempPath, const char *filepath, char *buff, long *buffLength)
{
    int err = 0;
    err = DB_LoadZipSchema(ZipTempPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    long len = *buffLength;
    char writebuff[CurrentZipTemplate.totalBytes + 2 * CurrentZipTemplate.schemas.size()]; //写入没有被压缩的数据

    DataTypeConverter converter;
    long buff_pos = 0;
    long writebuff_pos = 0;

    for (int i = 0; i < CurrentZipTemplate.schemas.size(); i++)
    {
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    //既有数据又有时间
                    char zipType[1] = {0};
                    zipType[0] = (char)2;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                    writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    buff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                }
                else
                {
                    //只有数据
                    char zipType[1] = {0};
                    zipType[0] = (char)0;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                    buff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                }
            }
            else
            {
                char standardUSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char maxUSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                char minUSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];
                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, buff + buff_pos, 1);
                char currentUSintValue = value[0];

                if (CurrentZipTemplate.schemas[i].second.hasTime) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (currentUSintValue != standardUSintValue && (currentUSintValue < maxUSintValue || currentUSintValue > minUSintValue))
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 9);
                        writebuff_pos += 9;
                    }
                    else
                    {

                        //只有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)1;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos + 1, 8);
                        writebuff_pos += 8;
                    }
                    buff_pos += 9;
                }
                else //不带时间戳
                {
                    if (currentUSintValue != standardUSintValue && (currentUSintValue < maxUSintValue || currentUSintValue > minUSintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 1);
                        writebuff_pos += 1;
                    }
                    buff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    //既有数据又有时间
                    char zipType[1] = {0};
                    zipType[0] = (char)2;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                    writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    buff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                }
                else
                {
                    //只有数据
                    char zipType[1] = {0};
                    zipType[0] = (char)0;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    buff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                }
            }
            else
            {
                ushort standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                ushort maxUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                ushort minUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, buff + buff_pos, 2);
                ushort currentUintValue = converter.ToUInt16(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (currentUintValue != standardUintValue && (currentUintValue < maxUintValue || currentUintValue > minUintValue))
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 10);
                        writebuff_pos += 10;
                    }
                    else
                    {
                        char zipType[1] = {0};
                        zipType[0] = (char)1;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos + 2, 8);
                        writebuff_pos += 8;
                    }
                    buff_pos += 10;
                }
                else //不带时间戳
                {
                    if (currentUintValue != standardUintValue && (currentUintValue < maxUintValue || currentUintValue > minUintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 2);
                        writebuff_pos += 2;
                    }
                    buff_pos += 2;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    //既有数据又有时间
                    char zipType[1] = {0};
                    zipType[0] = (char)2;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                    writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    buff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                }
                else
                {
                    //只有数据
                    char zipType[1] = {0};
                    zipType[0] = (char)0;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    buff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                }
            }
            else
            {
                uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                uint32 maxUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                uint32 minUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, buff + buff_pos, 4);
                uint32 currentUDintValue = converter.ToUInt32(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (currentUDintValue != standardUDintValue && (currentUDintValue < maxUDintValue || currentUDintValue > minUDintValue))
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 12);
                        writebuff_pos += 12;
                    }
                    else
                    {
                        //只有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)1;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos + 4, 8);
                        writebuff_pos += 8;
                    }
                    buff_pos += 12;
                }
                else //不带时间戳
                {
                    if (currentUDintValue != standardUDintValue && (currentUDintValue < maxUDintValue || currentUDintValue > minUDintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 4);
                        writebuff_pos += 4;
                    }
                    buff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    //既有数据又有时间
                    char zipType[1] = {0};
                    zipType[0] = (char)2;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                    writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    buff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                }
                else
                {
                    //只有数据
                    char zipType[1] = {0};
                    zipType[0] = (char)0;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                    buff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                }
            }
            else
            {
                char standardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char maxSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                char minSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, buff + buff_pos, 1);
                char currentSintValue = value[0];

                if (CurrentZipTemplate.schemas[i].second.hasTime) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (currentSintValue != standardSintValue && (currentSintValue < maxSintValue || currentSintValue > minSintValue))
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 9);
                        writebuff_pos += 9;
                    }
                    else
                    {
                        //只有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)1;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos + 1, 8);
                        writebuff_pos += 8;
                    }
                    buff_pos += 9;
                }
                else //不带时间戳
                {
                    if (currentSintValue != standardSintValue && (currentSintValue < maxSintValue || currentSintValue > minSintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 1);
                        writebuff_pos += 1;
                    }
                    buff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    //既有数据又有时间
                    char zipType[1] = {0};
                    zipType[0] = (char)2;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                    writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    buff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                }
                else
                {
                    //只有数据
                    char zipType[1] = {0};
                    zipType[0] = (char)0;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    buff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                }
            }
            else
            {
                short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                short maxIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                short minIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, buff + buff_pos, 2);
                short currentIntValue = converter.ToInt16(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (currentIntValue != standardIntValue && (currentIntValue < maxIntValue || currentIntValue > minIntValue))
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 10);
                        writebuff_pos += 10;
                    }
                    else
                    {
                        //只有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)1;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos + 2, 8);
                        writebuff_pos += 8;
                    }
                    buff_pos += 10;
                }
                else //不带时间戳
                {
                    if (currentIntValue != standardIntValue && (currentIntValue < maxIntValue || currentIntValue > minIntValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 2);
                        writebuff_pos += 2;
                    }
                    buff_pos += 2;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    //既有数据又有时间
                    char zipType[1] = {0};
                    zipType[0] = (char)2;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                    writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    buff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                }
                else
                {
                    //只有数据
                    char zipType[1] = {0};
                    zipType[0] = (char)0;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    buff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                }
            }
            else
            {
                int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                int maxDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                int minDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, buff + buff_pos, 4);
                int currentDintValue = converter.ToInt32(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (currentDintValue != standardDintValue && (currentDintValue < maxDintValue || currentDintValue > minDintValue))
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 12);
                        writebuff_pos += 12;
                    }
                    else
                    {
                        //只有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)1;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos + 4, 8);
                        writebuff_pos += 8;
                    }
                    buff_pos += 12;
                }
                else //不带时间戳
                {
                    if (currentDintValue != standardDintValue && (currentDintValue < maxDintValue || currentDintValue > minDintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 4);
                        writebuff_pos += 4;
                    }
                    buff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    //既有数据又有时间
                    char zipType[1] = {0};
                    zipType[0] = (char)2;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                    writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    buff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                }
                else
                {
                    //只有数据
                    char zipType[1] = {0};
                    zipType[0] = (char)0;
                    memcpy(writebuff + writebuff_pos, zipType, 1);
                    writebuff_pos += 1;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    buff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                }
            }
            else
            {
                float standardFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                float maxFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.maxValue);
                float minFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, buff + buff_pos, 4);
                int currentRealValue = converter.ToFloat(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (currentRealValue != standardFloatValue && (currentRealValue < maxFloatValue || currentRealValue > minFloatValue))
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 12);
                        writebuff_pos += 12;
                    }
                    else
                    {
                        //只有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)1;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos + 4, 8);
                        writebuff_pos += 8;
                    }
                    buff_pos += 12;
                }
                else //不带时间戳
                {
                    if (currentRealValue != standardFloatValue && (currentRealValue < maxFloatValue || currentRealValue > minFloatValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, buff + buff_pos, 4);
                        writebuff_pos += 4;
                    }
                    buff_pos += 4;
                }
            }
        }
        else
        {
            cout << "存在模拟量以外的类型，请检查模板或者更换功能块" << endl;
            return StatusCode::DATA_TYPE_MISMATCH_ERROR;
        }
    }

    if (writebuff_pos >= buff_pos) //表明数据没有被压缩
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
    return err;
}

/**
 * @brief 根据时间段压缩模拟量类型.idb文件
 * 
 * @param params 压缩请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ZipAnalogFileByTimeSpan(struct DB_ZipParams *params)
{
    params->ZipType=TIME_SPAN;
    int err = CheckZipParams(params);
    if(err!=0)
        return err;

    vector<pair<string, long>> filesWithTime, selectedFiles;
    readIDBFilesWithTimestamps(params->pathToLine,filesWithTime);//获取所有.idb文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
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
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(selectedFiles, TIME_ASC);

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;

    for(size_t fileNum=0;fileNum<selectedFiles.size();fileNum++)
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
        char readbuff[len];                                                                    //文件内容
        char writebuff[CurrentZipTemplate.totalBytes + 2 * CurrentZipTemplate.schemas.size()]; //写入没有被压缩的数据

        long readbuff_pos = 0;
        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        for (int i = 0; i < CurrentZipTemplate.schemas.size(); i++)
        {
            if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentUDintValue != standardUDintValue && (currentUDintValue < maxUDintValue || currentUDintValue > minUDintValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                            writebuff_pos += 12;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 4, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 12;
                    }
                    else //不带时间戳
                    {
                        if (currentUDintValue != standardUDintValue && (currentUDintValue < maxUDintValue || currentUDintValue > minUDintValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                            writebuff_pos += 4;
                        }
                        readbuff_pos += 4;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentUSintValue != standardUSintValue && (currentUSintValue < maxUSintValue || currentUSintValue > minUSintValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 9);
                            writebuff_pos += 9;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 1, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 9;
                    }
                    else //不带时间戳
                    {
                        if (currentUSintValue != standardUSintValue && (currentUSintValue < maxUSintValue || currentUSintValue > minUSintValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                            writebuff_pos += 1;
                        }
                        readbuff_pos += 1;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentUintValue != standardUintValue && (currentUintValue < maxUintValue || currentUintValue > minUintValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 10);
                            writebuff_pos += 10;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 2, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 10;
                    }
                    else //不带时间戳
                    {
                        if (currentUintValue != standardUintValue && (currentUintValue < maxUintValue || currentUintValue > minUintValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                            writebuff_pos += 2;
                        }
                        readbuff_pos += 2;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentSintValue != standardSintValue && (currentSintValue < maxSintValue || currentSintValue > minSintValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 9);
                            writebuff_pos += 9;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 1, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 9;
                    }
                    else //不带时间戳
                    {
                        if (currentSintValue != standardSintValue && (currentSintValue < maxSintValue || currentSintValue > minSintValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                            writebuff_pos += 1;
                        }
                        CurrentZipTemplate.schemas[i].second.hasTime ? readbuff_pos += 9 : readbuff_pos += 1;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentIntValue != standardIntValue && (currentIntValue < maxIntValue || currentIntValue > minIntValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 10);
                            writebuff_pos += 10;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 2, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 10;
                    }
                    else //不带时间戳
                    {
                        if (currentIntValue != standardIntValue && (currentIntValue < maxIntValue || currentIntValue > minIntValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                            writebuff_pos += 2;
                        }
                        CurrentZipTemplate.schemas[i].second.hasTime ? readbuff_pos += 10 : readbuff_pos += 2;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentDintValue != standardDintValue && (currentDintValue < maxDintValue || currentDintValue > minDintValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                            writebuff_pos += 12;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 4, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 12;
                    }
                    else //不带时间戳
                    {
                        if (currentDintValue != standardDintValue && (currentDintValue < maxDintValue || currentDintValue > minDintValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                            writebuff_pos += 4;
                        }
                        readbuff_pos += 4;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                    {
                        //既有数据又有时间
                        char zipType[1] = {0};
                        zipType[0] = (char)2;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                    }
                    else
                    {
                        //只有数据
                        char zipType[1] = {0};
                        zipType[0] = (char)0;
                        memcpy(writebuff + writebuff_pos, zipType, 1);
                        writebuff_pos += 1;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
                }
                else
                {
                    float standardFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    float maxFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.maxValue);
                    float minFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.minValue);
                    // 4个字节,暂定，根据后续情况可能进行更改
                    char value[4] = {0};
                    memcpy(value, readbuff + readbuff_pos, 4);
                    int currentRealValue = converter.ToFloat(value);

                    if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        if (currentRealValue != standardFloatValue && (currentRealValue < maxFloatValue || currentRealValue > minFloatValue))
                        {
                            //既有数据又有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)2;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                            writebuff_pos += 12;
                        }
                        else
                        {
                            //只有时间
                            char zipType[1] = {0};
                            zipType[0] = (char)1;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + 4, 8);
                            writebuff_pos += 8;
                        }
                        readbuff_pos += 12;
                    }
                    else //不带时间戳
                    {
                        if (currentRealValue != standardFloatValue && (currentRealValue < maxFloatValue || currentRealValue > minFloatValue))
                        {
                            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                            uint16_t posNum = i;
                            char zipPosNum[2] = {0};
                            for (char j = 0; j < 2; j++)
                            {
                                zipPosNum[1 - j] |= posNum;
                                posNum >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                            writebuff_pos += 2;

                            //只有数据
                            char zipType[1] = {0};
                            zipType[0] = (char)0;
                            memcpy(writebuff + writebuff_pos, zipType, 1);
                            writebuff_pos += 1;

                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                            writebuff_pos += 4;
                        }
                        readbuff_pos += 4;
                    }
                }
            }
            else
            {
                cout << "存在模拟量以外的类型，请检查模板或者更换功能块" << endl;
                return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            }
        }
        if (writebuff_pos >= readbuff_pos) //表明数据没有被压缩,不做处理
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
            err = DB_Open(const_cast<char *>(finalpath.c_str()), "wb", &fp);
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
    }
    return err;
}