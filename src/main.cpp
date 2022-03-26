#include "../include/FS_header.h"
#include "../include/STDFB_header.h"
#include "../include/QueryRequest.hpp"
//#include "../include/utils.hpp"
#include "../include/Schema.h"
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <errno.h>
using namespace std;
/**
 * @brief 从指定路径加载模版文件
 * @param path    路径
 *
 * @return  0:success,
 *          others: StatusCode
 * @note    在文件夹中找到模版文件，找到后读取，每个变量的前30字节为变量名，接下来30字节为数据类型，最后10字节为
 *          路径编码，分别解析之，构造、生成模版结构并将其设置为当前模版
 */
int EDVDB_LoadSchema(const char *path)
{
    vector<string> files;
    readFileList(path, files);
    string temPath = "";
    for (string file : files) //找到带有后缀tem的文件
    {
        string s = file;
        vector<string> vec = DataType::StringSplit(const_cast<char *>(s.c_str()), ".");
        if (vec[vec.size() - 1].find("tem") != string::npos)
        {
            temPath = file;
            break;
        }
    }
    if (temPath == "")
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    long length;
    EDVDB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &length);
    char buf[length];
    int err = EDVDB_OpenAndRead(const_cast<char *>(temPath.c_str()), buf);
    if (err != 0)
        return err;
    int i = 0;
    vector<PathCode> pathEncodes;
    vector<DataType> dataTypes;
    while (i < length)
    {
        char variable[30], dataType[30], pathEncode[10];
        memcpy(variable, buf + i, 30);
        i += 30;
        memcpy(dataType, buf + i, 30);
        i += 30;
        memcpy(pathEncode, buf + i, 10);
        i += 10;
        vector<string> paths;
        PathCode pathCode(pathEncode, sizeof(pathEncode) / 2, variable);
        DataType type;
        if (DataType::GetDataTypeFromStr(dataType, type) == 0)
        {
            dataTypes.push_back(type);
             
        }
        pathEncodes.push_back(pathCode);
    }
    Template tem(pathEncodes, dataTypes, path);
    return TemplateManager::SetTemplate(tem);
}

int EDVDB_UnloadSchema(char pathToUnset[])
{
    // return CurrentSchemaTemplate.UnsetTemplate();
}

int EDVDB_ExecuteQuery(DataBuffer *buffer, QueryParams *params)
{
}

/**
 * @brief 根据给定的时间段和路径编码或变量名在某一产线文件夹下的数据文件中查询数据，
 *          数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int EDVDB_QueryByTimespan(DataBuffer *buffer, QueryParams *params)
{
    vector<string> files;
    readIDBFilesList(params->pathToLine, files);
    vector<pair<string, long>> selectedFiles;

    //筛选落入时间区间内的文件
    for (auto file : files)
    {
        auto tmp = file;
        vector<string> time = DataType::StringSplit(const_cast<char *>(DataType::StringSplit(const_cast<char *>(tmp.c_str()), "_")[1].c_str()), "-");
        if (time.size() == 0)
        {
            continue;
        }
        time[time.size() - 1] = DataType::StringSplit(const_cast<char *>(time[time.size() - 1].c_str()), ".")[0];
        struct tm t;
        t.tm_year = atoi(time[0].c_str()) - 1900;
        t.tm_mon = atoi(time[1].c_str()) - 1;
        t.tm_mday = atoi(time[2].c_str());
        t.tm_hour = atoi(time[3].c_str());
        t.tm_min = atoi(time[4].c_str());
        t.tm_sec = atoi(time[5].c_str());
        time_t seconds = mktime(&t);
        int ms = atoi(time[6].c_str());
        long millis = seconds * 1000 + ms;
        if (millis >= params->start && millis <= params->end)
        {
            selectedFiles.push_back(make_pair(file, millis));
        }
    }
    //确认当前模版
    string str = params->pathToLine;
    if (CurrentTemplate.path != str || CurrentTemplate.path == "")
    {
        EDVDB_LoadSchema(params->pathToLine);
    }

    //获取数据的偏移量和字节数
    long bytes = 0, pos = 0;
    int err;
    if (params->byPath)
    {
        char *pathCode = params->pathCode;
        err = CurrentTemplate.FindDatatypePosByCode(pathCode, pos, bytes);
    }
    else
        err = CurrentTemplate.FindDatatypePosByName(params->valueName, pos, bytes);
    if (err != 0)
        return err;

    //根据时间升序或降序排序
    switch (params->order)
    {
    case ODR_NONE:
        break;
    case TIME_ASC:
        sort(selectedFiles.begin(), selectedFiles.end(),
             [](pair<string, long> iter1, pair<string, long> iter2) -> bool
             {
                 return iter1.second < iter2.second;
             });
        break;
    case TIME_DSC:
        sort(selectedFiles.begin(), selectedFiles.end(),
             [](pair<string, long> iter1, pair<string, long> iter2) -> bool
             {
                 return iter1.second > iter2.second;
             });
        break;
    default:
        break;
    }

    //动态分配内存
    char *data = (char *)malloc(bytes * selectedFiles.size());
    if (data == NULL)
    {
        buffer->buffer = NULL;
        buffer->bufferMalloced = 0;
        return StatusCode::BUFFER_FULL;
    }

    //拷贝数据
    long cur = 0;
    for (auto &file : selectedFiles)
    {
        long len;
        EDVDB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
        char buff[len];
        EDVDB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
        memcpy(data + cur, buff + pos, bytes);
        cur += bytes;
    }
    buffer->bufferMalloced = 1;
    buffer->buffer = data;
    buffer->length = bytes * selectedFiles.size();
    return 0;
}

int EDVDB_QueryByTimespan2(DataBuffer *buffer, QueryParams *params)
{
    vector<string> files;
    readIDBFilesList(params->pathToLine, files);
    vector<pair<string, long>> selectedFiles;

    //筛选落入时间区间内的文件
    for (auto file : files)
    {
        auto tmp = file;
        vector<string> time = DataType::StringSplit(const_cast<char *>(DataType::StringSplit(const_cast<char *>(tmp.c_str()), "_")[1].c_str()), "-");
        if (time.size() == 0)
        {
            continue;
        }
        time[time.size() - 1] = DataType::StringSplit(const_cast<char *>(time[time.size() - 1].c_str()), ".")[0];
        struct tm t;
        t.tm_year = atoi(time[0].c_str()) - 1900;
        t.tm_mon = atoi(time[1].c_str()) - 1;
        t.tm_mday = atoi(time[2].c_str());
        t.tm_hour = atoi(time[3].c_str());
        t.tm_min = atoi(time[4].c_str());
        t.tm_sec = atoi(time[5].c_str());
        time_t seconds = mktime(&t);
        int ms = atoi(time[6].c_str());
        long millis = seconds * 1000 + ms;
        if (millis >= params->start && millis <= params->end)
        {
            selectedFiles.push_back(make_pair(file, millis));
        }
    }
    //确认当前模版
    string str = params->pathToLine;
    if (CurrentTemplate.path != str || CurrentTemplate.path == "")
    {
        EDVDB_LoadSchema(params->pathToLine);
    }

    //获取数据的偏移量和数据类型
    long pos = 0, bytes = 0;
    DataType type;
    int err;
    if (params->byPath)
    {
        char *pathCode = params->pathCode;
        err = CurrentTemplate.FindDatatypePosByCode(pathCode, pos, bytes);
    }
    else
        err = CurrentTemplate.FindDatatypePosByName(params->valueName, pos, bytes, type);
    if (err != 0)
        return err;

    //根据时间升序或降序排序
    switch (params->order)
    {
    case ODR_NONE:
        break;
    case TIME_ASC:
        sort(selectedFiles.begin(), selectedFiles.end(),
             [](pair<string, long> iter1, pair<string, long> iter2) -> bool
             {
                 return iter1.second < iter2.second;
             });
        break;
    case TIME_DSC:
        sort(selectedFiles.begin(), selectedFiles.end(),
             [](pair<string, long> iter1, pair<string, long> iter2) -> bool
             {
                 return iter1.second > iter2.second;
             });
        break;
    default:
        break;
    }

    //比较指定变量给定的数据值，筛选符合条件的值
    char comparedData[bytes * selectedFiles.size()];
    long cur = 0;
    if (params->compareType != CompareType::CMP_NONE)
    {
        for (auto &file : selectedFiles)
        {
            long len;
            EDVDB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
            char buff[len];
            EDVDB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
            char value[bytes];
            memcpy(value, buff + pos, bytes);

            //根据比较结果决定是否加入结果集
            int compareRes = DataType::CompareValue(type, value, params->compareValue);
            switch (params->compareType)
            {
            case CompareType::GT:
            {
                if (compareRes > 0)
                {
                    memcpy(comparedData + cur, value, bytes);
                    cur += bytes;
                }
                break;
            }
            case CompareType::LT:
            {
                if (compareRes < 0)
                {
                    memcpy(comparedData + cur, value, bytes);
                    cur += bytes;
                }
                break;
            }
            case CompareType::GE:
            {
                if (compareRes >= 0)
                {
                    memcpy(comparedData + cur, value, bytes);
                    cur += bytes;
                }
                break;
            }
            case CompareType::LE:
            {
                if (compareRes <= 0)
                {
                    memcpy(comparedData + cur, value, bytes);
                    cur += bytes;
                }
                break;
            }
            case CompareType::EQ:
            {
                if (compareRes == 0)
                {
                    memcpy(comparedData + cur, value, bytes);
                    cur += bytes;
                }
                break;
            }
            default:
                break;
            }
        }
    }

    //动态分配内存
    char *data = (char *)malloc(cur);
    if (data == NULL)
    {
        buffer->buffer = NULL;
        buffer->bufferMalloced = 0;
        return StatusCode::BUFFER_FULL;
    }

    //拷贝数据
    memcpy(data, comparedData, cur);
    buffer->bufferMalloced = 1;
    buffer->buffer = data;
    buffer->length = bytes * selectedFiles.size();
    return 0;
}

/**
 * @brief 根据路径编码或变量名在某一产线文件夹下的数据文件中查询指定条数最新的数据，
 *          数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int EDVDB_QueryLastData(DataBuffer *buffer, QueryParams *params)
{
    vector<string> files;
    readIDBFilesList(params->pathToLine, files);
    vector<pair<string, long>> selectedFiles;

    //获取每个文件的时间戳
    for (auto file : files)
    {
        auto tmp = file;
        vector<string> time = DataType::StringSplit(const_cast<char *>(DataType::StringSplit(const_cast<char *>(tmp.c_str()), "_")[1].c_str()), "-");
        if (time.size() == 0)
        {
            continue;
        }
        time[time.size() - 1] = DataType::StringSplit(const_cast<char *>(time[time.size() - 1].c_str()), ".")[0];
        struct tm t;
        t.tm_year = atoi(time[0].c_str()) - 1900;
        t.tm_mon = atoi(time[1].c_str()) - 1;
        t.tm_mday = atoi(time[2].c_str());
        t.tm_hour = atoi(time[3].c_str());
        t.tm_min = atoi(time[4].c_str());
        t.tm_sec = atoi(time[5].c_str());
        time_t seconds = mktime(&t);
        int ms = atoi(time[6].c_str());
        long millis = seconds * 1000 + ms;
        selectedFiles.push_back(make_pair(file, millis));
    }

    //确认当前模版
    string str = params->pathToLine;
    if (CurrentTemplate.path != str || CurrentTemplate.path == "")
    {
        EDVDB_LoadSchema(params->pathToLine);
    }

    //获取数据的偏移量和字节数
    long bytes = 0, pos = 0;
    int err;
    if (params->byPath)
    {
        char *pathCode = params->pathCode;
        err = CurrentTemplate.FindDatatypePosByCode(pathCode, pos, bytes);
    }
    else
        err = CurrentTemplate.FindDatatypePosByName(params->valueName, pos, bytes);
    if (err != 0)
        return err;

    //动态分配内存
    char *data = (char *)malloc(bytes * params->queryNums);
    if (data == NULL)
    {
        buffer->buffer = NULL;
        buffer->bufferMalloced = 0;
        return StatusCode::BUFFER_FULL;
    }

    //根据时间降序排序
    sort(selectedFiles.begin(), selectedFiles.end(),
         [](pair<string, long> iter1, pair<string, long> iter2) -> bool
         {
             return iter1.second > iter2.second;
         });

    //取排序后的文件中前queryNums个文件的数据
    long cur = 0;
    for (int i = 0; i < params->queryNums; i++)
    {
        long len;
        EDVDB_GetFileLengthByPath(const_cast<char *>(selectedFiles[i].first.c_str()), &len);
        char buff[len];
        EDVDB_OpenAndRead(const_cast<char *>(selectedFiles[i].first.c_str()), buff);
        memcpy(data + cur, buff + pos, bytes);
        cur += bytes;
    }
    buffer->buffer = data;
    buffer->bufferMalloced = 1;
    buffer->length = bytes * params->queryNums;
    return 0;
}

/**
 * @brief 根据文件ID和路径编码在某一产线文件夹下的数据文件中查询数据，数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note 获取产线文件夹下的所有数据文件，找到带有指定ID的文件后读取，加载模版，根据模版找到变量在数据中的位置
 *          找到后开辟内存空间，将数据放入，将缓冲区首地址赋值给buffer
 */
int EDVDB_QueryByFileID(DataBuffer *buffer, QueryParams *params)
{
    vector<string> files;
    readIDBFilesList(params->pathToLine, files);
    for (string &file : files)
    {
        if (file.find(params->fileID) != string::npos)
        {
            long len;
            EDVDB_GetFileLengthByPath(const_cast<char *>(file.c_str()), &len);
            char buff[len];
            EDVDB_OpenAndRead(const_cast<char *>(file.c_str()), buff);
            EDVDB_LoadSchema(params->pathToLine);

            long pos = 0;
            long bytes = 0;
            int err;
            if (params->byPath)
            {
                char *pathCode = params->pathCode;
                err = CurrentTemplate.FindDatatypePosByCode(pathCode, pos, bytes);
            }
            else
                err = CurrentTemplate.FindDatatypePosByName(params->valueName, pos, bytes);
            if (err != 0)
            {
                return err;
            }
            char *data = (char *)malloc(bytes);
            if (data == NULL)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return StatusCode::BUFFER_FULL;
            }
            //内存分配成功，传入数据
            buffer->bufferMalloced = 1;
            buffer->length = bytes;
            memcpy(data, buff + pos, bytes);
            buffer->buffer = data;
            return 0;

            break;
        }
    }
    return StatusCode::DATAFILE_NOT_FOUND;
}

// deprecated
int EDVDB_QueryByFileID_Old(DataBuffer *buffer, QueryParams *params)
{
    // vector<string> arr = DataType::StringSplit(params->fileID,"_");
    vector<string> files;
    readIDBFilesList(params->pathToLine, files);
    for (string file : files)
    {
        if (file.find(params->fileID) != string::npos)
        {
            long len;
            EDVDB_GetFileLengthByPath(const_cast<char *>(file.c_str()), &len);
            char buff[len];
            EDVDB_OpenAndRead(const_cast<char *>(file.c_str()), buff);
            EDVDB_LoadSchema(params->pathToLine);
            DataTypeConverter converter;
            char *pathCode = params->pathCode;
            long pos = 0;
            for (size_t i = 0; i < CurrentTemplate.schemas.size(); i++)
            {
                bool codeEquals = true;
                for (size_t k = 0; k < 10; k++) //判断路径编码是否相等
                {
                    if (pathCode[k] != CurrentTemplate.schemas[i].first.code[k])
                        codeEquals = false;
                }
                if (codeEquals)
                {
                    int num = 1;
                    if (CurrentTemplate.schemas[i].second.isArray)
                    {
                        /*
                            图片数据中前2个字节为长度，长度不包括这2个字节
                            格式改变时，此处需要更改，下面else同理
                            请注意！
                        */

                        if (CurrentTemplate.schemas[i].second.valueType == ValueType::IMAGE)
                        {

                            char imgLen[2];
                            imgLen[0] = buff[pos];
                            imgLen[1] = buff[pos + 1];
                            num = (int)converter.ToUInt16(imgLen) + 2;
                        }
                        else
                            num = CurrentTemplate.schemas[i].second.arrayLen;
                    }
                    buffer->length = (long)(num * CurrentTemplate.schemas[i].second.valueBytes);
                    int j = 0;
                    if (CurrentTemplate.schemas[i].second.valueType == ValueType::IMAGE)
                    {
                        buffer->length -= 2;
                        j = 2;
                    }
                    char *data = (char *)malloc(buffer->length);
                    if (data == NULL)
                    {
                        //内存分配失败，需要作处理
                        //方案1:直接返回错误
                        //方案2:尝试分配更小的内存
                        buffer->buffer = NULL;
                        buffer->bufferMalloced = 0;
                    }
                    //内存分配成功，传入数据
                    buffer->bufferMalloced = 1;
                    for (; j < buffer->length; j++)
                    {
                        data[j] = buff[pos + j];
                    }
                    buffer->buffer = data;
                    return 0;
                }
                else
                {
                    int num = 1;
                    if (CurrentTemplate.schemas[i].second.isArray)
                    {
                        if (CurrentTemplate.schemas[i].second.valueType == ValueType::IMAGE)
                        {

                            char imgLen[2];
                            imgLen[0] = buff[pos];
                            imgLen[1] = buff[pos + 1];
                            num = (int)converter.ToUInt16(imgLen) + 2;
                        }
                        else
                            num = CurrentTemplate.schemas[i].second.arrayLen;
                    }
                    pos += num * CurrentTemplate.schemas[i].second.valueBytes;
                }
            }

            break;
        }
    }
    return StatusCode::DATAFILE_NOT_FOUND;
}

/**
 * @brief 将一个缓冲区中的一条数据(文件)存放在指定路径下，以文件ID+时间的方式命名
 * @param buffer    数据缓冲区
 * @param addTime    是否添加时间戳
 *
 * @return  0:success,
 *          others: StatusCode
 * @note 文件ID的暂定的命名方式为当前文件夹下的文件数量+1，
 *  时间戳格式为yyyy-mm-dd-hh-min-ss-ms
 */
int EDVDB_InsertRecord(DataBuffer *buffer, int addTime)
{
    long fp;
    long curtime = getMilliTime();
    time_t time = curtime / 1000;
    struct tm *dateTime = localtime(&time);
    string fileID = FileIDManager::GetFileID(buffer->savePath);
    string finalPath = "";
    finalPath = finalPath.append(buffer->savePath).append("/").append(fileID).append(to_string(1900 + dateTime->tm_year)).append("-").append(to_string(1 + dateTime->tm_mon)).append("-").append(to_string(dateTime->tm_mday)).append("-").append(to_string(dateTime->tm_hour)).append("-").append(to_string(dateTime->tm_min)).append("-").append(to_string(dateTime->tm_sec)).append("-").append(to_string(curtime % 1000)).append(".idb");
    int err = EDVDB_Open(const_cast<char *>(finalPath.c_str()), "ab", &fp);
    if (err == 0)
    {
        err = EDVDB_Write(fp, buffer->buffer, buffer->length);
        if (err == 0)
        {
            return EDVDB_Close(fp);
        }
    }
    return err;
}
/**
 * @brief 将多条数据(文件)存放在指定路径下，以文件ID+时间的方式命名
 * @param buffer[]    数据缓冲区
 * @param recordsNum  数据(文件)条数
 * @param addTime    是否添加时间戳
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  文件ID的暂定的命名方式为当前文件夹下的文件数量+1，
 *  时间戳格式为yyyy-mm-dd-hh-min-ss-ms
 */
int EDVDB_InsertRecords(DataBuffer buffer[], int recordsNum, int addTime)
{
    long curtime = getMilliTime();
    time_t time = curtime / 1000;
    struct tm *dateTime = localtime(&time);
    string timestr = "";
    timestr.append(to_string(1900 + dateTime->tm_year)).append("-").append(to_string(1 + dateTime->tm_mon)).append("-").append(to_string(dateTime->tm_mday)).append("-").append(to_string(dateTime->tm_hour)).append("-").append(to_string(dateTime->tm_min)).append("-").append(to_string(dateTime->tm_sec)).append("-").append(to_string(curtime % 1000)).append(".idb");
    for (int i = 0; i < recordsNum; i++)
    {
        long fp;

        string fileID = FileIDManager::GetFileID(buffer->savePath);
        string finalPath = "";
        finalPath = finalPath.append(buffer->savePath).append("/").append(fileID).append(timestr);
        int err = EDVDB_Open(const_cast<char *>(buffer[i].savePath), "ab", &fp);
        if (err == 0)
        {
            err = EDVDB_Write(fp, buffer[i].buffer, buffer[i].length);
            if (err != 0)
            {
                return err;
            }
            EDVDB_Close(fp);
        }
        else
        {
            return err;
        }
    }
}
int testQuery2(DataBuffer *buffer, long *len)
{
    char *buf = (char *)malloc(10);
    for (size_t i = 0; i < 10; i++)
    {
        buf[i] = 'a';
    }
    cout << buf << endl;
    *len = 10;
    buffer->buffer = buf;
}

int main()
{
    EDVDB_ZipFile("./","XinFeng_0100.dat");
    //cout << EDVDB_LoadZipSchema("./");
    //cout << EDVDB_LoadSchema("./");
    return 0;
}