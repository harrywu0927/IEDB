#include "../include/utils.hpp"
using namespace std;

/**
 * @brief 压缩只有开关量类型的已有.idb文件
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idb文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipSwitchFile(const char *ZipTemPath, const char *pathToLine)
{
    int err;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;
    vector<string> files;
    readIDBFilesList(pathToLine, files);
    if (files.size() == 0)
    {
        cout << "没有文件！" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

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
            if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) //开关量的持续时长
            {
                uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                uint32 maxBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                uint32 minBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节的持续时长,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                uint32 currentBoolTime = converter.ToUInt32(value);

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

                    if (currentBoolTime != standardBoolTime && (currentBoolTime < minBoolTime || currentBoolTime > maxBoolTime))
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
                    if (currentBoolTime != standardBoolTime && (currentBoolTime < minBoolTime || currentBoolTime > maxBoolTime))
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
            else
            {
                cout << "存在开关量以外的类型，请检查模板或者更换功能块" << endl;
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
 * @brief 还原压缩后的开关量文件为原状态
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idbzip压缩文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ReZipSwitchFile(const char *ZipTemPath, const char *pathToLine)
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
            if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) //开关量持续时长
            {
                if (len == 0) //表示文件完全压缩
                {
                    uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char boolTime[4] = {0};
                    for (int j = 0; j < 4; j++)
                    {
                        boolTime[3 - j] |= standardBoolTime;
                        standardBoolTime >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, boolTime, 4); //持续时长
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
                            if (readbuff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
                            }
                            else if (readbuff[readbuff_pos - 1] == (char)1) //只有时间
                            {
                                uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                char boolTime[4] = {0};
                                for (int j = 0; j < 4; j++)
                                {
                                    boolTime[3 - j] |= standardBoolTime;
                                    standardBoolTime >>= 8;
                                }
                                memcpy(writebuff + writebuff_pos, boolTime, 4); //持续时长
                                writebuff_pos += 4;

                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (readbuff[readbuff_pos - 1] == (char)2) //既有数据又有时间
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                                writebuff_pos += 12;
                                readbuff_pos += 12;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有问题" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else //不是未压缩的编号
                        {
                            uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                            char boolTime[4] = {0};
                            for (int j = 0; j < 4; j++)
                            {
                                boolTime[3 - j] |= standardBoolTime;
                                standardBoolTime >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, boolTime, 4); //持续时长
                            writebuff_pos += 4;
                        }
                    }
                    else //没有未压缩的数据了
                    {
                        uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char boolTime[4] = {0};
                        for (int j = 0; j < 4; j++)
                        {
                            boolTime[3 - j] |= standardBoolTime;
                            standardBoolTime >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, boolTime, 4); //持续时长
                        writebuff_pos += 4;
                    }
                }
            }
            else
            {
                cout << "存在开关量以外的类型，请检查模板或者更换功能块" << endl;
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
 * @brief 压缩接收到只有开关量类型的整条数据
 *
 * @param ZipTemPath 压缩模板路径
 * @param filepath 存储文件路径
 * @param buff 接收到的数据
 * @param buffLength 接收的数据长度
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipRecvSwitchBuff(const char *ZipTemPath, const char *filepath, char *buff, long *buffLength)
{
    int err = 0;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
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
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // BOOL变量
        {
            uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
            uint32 maxBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
            uint32 minBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);
            // 4个字节的持续时长,暂定，根据后续情况可能进行更改
            char value[4] = {0};
            memcpy(value, buff + buff_pos, 4);
            uint32 currentBoolTime = converter.ToUInt32(value);

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

                if (currentBoolTime != standardBoolTime && (currentBoolTime < minBoolTime || currentBoolTime > maxBoolTime))
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
                if (currentBoolTime != standardBoolTime && (currentBoolTime < minBoolTime || currentBoolTime > maxBoolTime))
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
        else
        {
            cout << "存在开关量以外的类型，请检查模板或者更换功能块" << endl;
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