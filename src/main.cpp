#include "../include/FS_header.h"
#include "../include/STDFB_header.h"
#include "../include/QueryRequest.hpp"
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
    return TemplateManager::SetTemplate(path);
}

/**
 * @brief 卸载指定路径下的模版文件
 * @param pathToUnset    路径
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int EDVDB_UnloadSchema(const char *pathToUnset)
{
    return TemplateManager::UnsetTemplate(pathToUnset);
}

/**
 * @brief 根据指定数据类型的数值对查询结果排序或去重，此函数仅对内存地址-长度对操作
 * @param mallocedMemory        已在堆区分配的内存地址-长度对集
 * @param pos           指定数据在缓冲区中的位置
 * @param params        查询参数
 * @param type        数据类型
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int sortResultByValue(vector<pair<char *, long>> &mallocedMemory, long pos, QueryParams *params, DataType &type)
{
    switch (params->order)
    {
    case ASCEND: //升序
    {
        sort(mallocedMemory.begin(), mallocedMemory.end(),
             [&type, &pos](pair<char *, long> iter1, pair<char *, long> iter2) -> bool
             {
                 char value1[type.valueBytes], value2[type.valueBytes];
                 memcpy(value1, iter1.first + pos, type.valueBytes);
                 memcpy(value2, iter2.first + pos, type.valueBytes);
                 return DataType::CompareValueInBytes(type, value1, value2) < 0;
             });
        break;
    }

    case DESCEND: //降序
    {
        sort(mallocedMemory.begin(), mallocedMemory.end(),
             [&type, &pos](pair<char *, long> iter1, pair<char *, long> iter2) -> bool
             {
                 char value1[type.valueBytes], value2[type.valueBytes];
                 memcpy(value1, iter1.first + pos, type.valueBytes);
                 memcpy(value2, iter2.first + pos, type.valueBytes);
                 return DataType::CompareValueInBytes(type, value1, value2) > 0;
             });
        break;
    }
    case DISTINCT: //去除重复
    {
        vector<pair<char *, long>> existedValues;
        for (auto mem = mallocedMemory.begin(); mem != mallocedMemory.end(); mem++)
        {
            char value[type.valueBytes];
            memcpy(value, mem->first + pos, type.valueBytes);
            bool isRepeat = false;
            for (auto &&added : existedValues)
            {
                bool equals = true;
                for (int i = 0; i < added.second; i++)
                {
                    if (value[i] != added.first[i])
                        equals = false;
                }
                if (equals)
                {
                    free(mem->first);          //重复值，释放此内存
                    mallocedMemory.erase(mem); //注：此操作在查询量大时效率可能很低
                    isRepeat = true;
                }
            }
            if (!isRepeat) //不是重复值
            {
                char *v = value;
                existedValues.push_back(make_pair(v, type.valueBytes));
            }
        }

        break;
    }
    default:
        break;
    }
    DataTypeConverter converter;
    for (auto it = mallocedMemory.begin(); it != mallocedMemory.end(); it++)
    {
        char val[type.valueBytes];
        memcpy(val, it->first + pos, type.valueBytes);
        cout << converter.ToInt16(val) << endl;
    }

    return 0;
}

//根据时间升序或降序排序
void sortByTime(vector<pair<string, long>> &selectedFiles, Order order)
{
    switch (order)
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
    default: //其他Order类型延后处理
        break;
    }
}
/**
 * @brief 执行自定义查询，自由组合查询请求参数，按照3个主查询条件之一，附加辅助条件查询
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int EDVDB_ExecuteQuery(DataBuffer *buffer, QueryParams *params)
{
    switch (params->queryType)
    {
    case TIMESPAN:
        return EDVDB_QueryByTimespan(buffer, params);
        break;
    case LAST:
        return EDVDB_QueryLastRecords(buffer, params);
        break;
    case FILEID:
        return EDVDB_QueryByFileID(buffer, params);
        break;
    default:
        break;
    }
    return StatusCode::NO_QUERY_TYPE;
}

/**
 * @brief 根据给定的时间段和路径编码或变量名在某一产线文件夹下的数据文件中查询数据，
 *          数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note    deprecated
 */

/*int EDVDB_QueryByTimespan(DataBuffer *buffer, QueryParams *params)
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
        int err = 0;
        err = EDVDB_LoadSchema(params->pathToLine);
        if (err != 0)
        {
            buffer->buffer = NULL;
            buffer->bufferMalloced = 0;
            return err;
        }
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
        if (len < pos)
        {
            buffer->bufferMalloced = 1;
            buffer->buffer = data;
            buffer->length = bytes * selectedFiles.size();
            return StatusCode::UNKNWON_DATAFILE;
        }
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
*/

/**
 * @brief 根据给定的查询条件在某一产线文件夹下的数据文件中获取符合条件的整个文件的数据，可比较数据大小筛选，可按照时间
 *          将结果升序或降序排序，数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int EDVDB_QueryWholeFile(DataBuffer *buffer, QueryParams *params)
{
    vector<pair<string, long>> filesWithTime, selectedFiles;

    //获取每个数据文件，并带有时间戳
    readIDBFilesWithTimestamps(params->pathToLine, filesWithTime);

    //根据主查询方式选择不同的方案
    switch (params->queryType)
    {
    case TIMESPAN: //根据时间段，附加辅助查询条件筛选
    {
        //筛选落入时间区间内的文件
        for (auto &file : filesWithTime)
        {
            if (file.second >= params->start && file.second <= params->end)
            {
                selectedFiles.push_back(make_pair(file.first, file.second));
            }
        }
        //确认当前模版
        string str = params->pathToLine;
        if (CurrentTemplate.path != str || CurrentTemplate.path == "")
        {
            int err = 0;
            err = EDVDB_LoadSchema(params->pathToLine);
            if (err != 0)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return err;
            }
        }

        //根据时间升序或降序排序
        sortByTime(selectedFiles, params->order);

        //比较指定变量给定的数据值，筛选符合条件的值
        vector<pair<char *, long>> mallocedMemory; //已在堆区分配的进入筛选范围数据的内存地址和长度集
        long cur = 0;                              //记录已选中的文件总长度
        /*<-----!!!警惕内存泄露!!!----->*/
        if (params->compareType != CompareType::CMP_NONE)
        {

            for (auto &file : selectedFiles)
            {
                long len; //文件长度
                EDVDB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char buff[len]; //文件内容缓存
                EDVDB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

                //获取数据的偏移量和数据类型
                long pos = 0, bytes = 0;
                DataType type;
                int err;
                if (params->byPath == 1)
                {
                    char *pathCode = params->pathCode;
                    err = CurrentTemplate.FindDatatypePosByCode(pathCode, buff, pos, bytes, type);
                }
                else
                    err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
                if (err != 0)
                    return err;

                char value[bytes]; //值缓存
                memcpy(value, buff + pos, bytes);
                //根据比较结果决定是否加入结果集
                int compareRes = DataType::CompareValue(type, value, params->compareValue);
                bool canCopy = false;
                switch (params->compareType)
                {
                case CompareType::GT:
                {
                    if (compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::LT:
                {
                    if (compareRes == -1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::GE:
                {
                    if (compareRes == 0 || compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::LE:
                {
                    if (compareRes == 0 || compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::EQ:
                {
                    if (compareRes == 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                default:
                    break;
                }
                if (canCopy) //需要此数据
                {
                    char *memory = (char *)malloc(len); //一次分配整个文件长度的内存
                    memcpy(memory, buff, len);
                    cur += len;
                    mallocedMemory.push_back(make_pair(memory, len));
                }
            }
        }
        else //不需要比较数据，直接拷贝数据
        {
            for (auto &file : selectedFiles)
            {
                long len; //文件长度
                EDVDB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char buff[len]; //文件内容缓存
                EDVDB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

                char *memory = (char *)malloc(len); //一次分配整个文件长度的内存
                memcpy(memory, buff, len);
                cur += len;
                mallocedMemory.push_back(make_pair(memory, len));
            }
        }

        //动态分配内存
        char *data;
        if (cur != 0)
        {
            data = (char *)malloc(cur);
            if (data == NULL)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return StatusCode::BUFFER_FULL;
            }
            //拷贝数据
            cur = 0;
            for (auto &mem : mallocedMemory)
            {
                memcpy(data, mem.first, mem.second);
                free(mem.first);
                cur += mem.second;
            }

            buffer->bufferMalloced = 1;
            buffer->buffer = data;
            buffer->length = cur;
        }
        /*<-----!!!!!!----->*/
        else
        {
            buffer->bufferMalloced = 0;
        }

        break;
    }
    case LAST: //查询最新若干条，附加辅助查询条件筛选
    {
        //确认当前模版
        string str = params->pathToLine;
        if (CurrentTemplate.path != str || CurrentTemplate.path == "")
        {
            int err = 0;
            err = EDVDB_LoadSchema(params->pathToLine);
            if (err != 0)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return err;
            }
        }

        //根据时间降序排序
        sort(filesWithTime.begin(), filesWithTime.end(),
             [](pair<string, long> iter1, pair<string, long> iter2) -> bool
             {
                 return iter1.second > iter2.second;
             });
        vector<pair<char *, long>> mallocedMemory; //已在堆区分配的进入筛选范围数据的内存地址和长度集
        long cur = 0;                              //记录已选中的文件总长度
        if (params->compareType != CMP_NONE)       //需要比较数值
        {
            int selectedNum = 0;
            /*<-----!!!警惕内存泄露!!!----->*/
            for (auto &file : filesWithTime)
            {
                long len; //文件长度
                EDVDB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char buff[len]; //文件内容缓存
                EDVDB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

                //获取数据的偏移量和字节数
                long bytes = 0, pos = 0;
                DataType type;
                int err;
                if (params->byPath)
                {
                    char *pathCode = params->pathCode;
                    err = CurrentTemplate.FindDatatypePosByCode(pathCode, buff, pos, bytes, type);
                }
                else
                    err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
                if (err != 0)
                {
                    buffer->buffer = NULL;
                    buffer->bufferMalloced = 0;
                    return err;
                }
                char value[bytes]; //值缓存
                memcpy(value, buff + pos, bytes);

                //根据比较结果决定是否加入结果集
                int compareRes = DataType::CompareValue(type, value, params->compareValue);
                bool canCopy = false; //根据比较结果决定是否允许拷贝
                switch (params->compareType)
                {
                case CompareType::GT:
                {
                    if (compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::LT:
                {
                    if (compareRes == -1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::GE:
                {
                    if (compareRes == 0 || compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::LE:
                {
                    if (compareRes == 0 || compareRes == -1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::EQ:
                {
                    if (compareRes == 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                default:
                    break;
                }
                if (canCopy) //需要此数据
                {
                    char *memory = (char *)malloc(len); //一次分配整个文件长度的内存
                    memcpy(memory, buff, len);
                    cur += len;
                    mallocedMemory.push_back(make_pair(memory, len));
                    selectedNum++;
                }
                if (selectedNum == params->queryNums)
                    break;
            }

            //已获取指定数量的数据，开始拷贝内存
            char *data;
            if (cur != 0)
            {
                data = (char *)malloc(cur);
                if (data == NULL)
                {
                    buffer->buffer = NULL;
                    buffer->bufferMalloced = 0;
                    return StatusCode::BUFFER_FULL;
                }
                //拷贝数据
                cur = 0;
                for (auto &mem : mallocedMemory)
                {
                    memcpy(data + cur, mem.first, mem.second);
                    free(mem.first);
                    cur += mem.second;
                }

                buffer->bufferMalloced = 1;
                buffer->buffer = data;
                buffer->length = cur;
            }
            else
            {
                buffer->bufferMalloced = 0;
            }
        }
        else //不需要比较数值,直接拷贝前N个文件
        {
            for (int i = 0; i < params->queryNums; i++)
            {
                long len;
                EDVDB_GetFileLengthByPath(const_cast<char *>(filesWithTime[i].first.c_str()), &len);
                char buff[len];
                EDVDB_OpenAndRead(const_cast<char *>(filesWithTime[i].first.c_str()), buff);
                char *memory = (char *)malloc(len);
                memcpy(memory, buff, len);
                mallocedMemory.push_back(make_pair(memory, len));
                cur += len;
            }
            if (cur != 0)
            {
                char *data = (char *)malloc(cur);
                cur = 0;
                for (auto &mem : mallocedMemory)
                {
                    memcpy(data + cur, mem.first, mem.second);
                    free(mem.first);
                    cur += mem.second;
                }
                buffer->bufferMalloced = 1;
                buffer->buffer = data;
                buffer->length = cur;
            }
            else
                buffer->bufferMalloced = 0;
        }

        break;
    }
    case FILEID: //指定文件ID
    {
        for (auto &file : filesWithTime)
        {
            if (file.first.find(params->fileID) != string::npos)
            {
                long len;
                EDVDB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char buff[len];
                EDVDB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

                char *data = (char *)malloc(len);
                if (data == NULL)
                {
                    buffer->buffer = NULL;
                    buffer->bufferMalloced = 0;
                    return StatusCode::BUFFER_FULL;
                }
                //内存分配成功，传入数据
                buffer->bufferMalloced = 1;
                buffer->length = len;
                memcpy(data, buff, len);
                buffer->buffer = data;
                return 0;

                break;
            }
        }

        break;
    }

    default:
        return StatusCode::NO_QUERY_TYPE;
        break;
    }
    return 0;
}

/**
 * @brief 根据给定的时间段和路径编码或变量名在某一产线文件夹下的数据文件中查询数据，可比较数据大小筛选，可按照时间
 *          将结果升序或降序排序，数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int EDVDB_QueryByTimespan(DataBuffer *buffer, QueryParams *params)
{
    vector<pair<string, long>> filesWithTime, selectedFiles;

    //获取每个数据文件，并带有时间戳
    readIDBFilesWithTimestamps(params->pathToLine, filesWithTime);

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }

    //确认当前模版
    string str = params->pathToLine;
    if (CurrentTemplate.path != str || CurrentTemplate.path == "")
    {
        int err = 0;
        err = EDVDB_LoadSchema(params->pathToLine);
        if (err != 0)
        {
            buffer->buffer = NULL;
            buffer->bufferMalloced = 0;
            return err;
        }
    }

    //根据时间升序或降序排序
    sortByTime(selectedFiles, params->order);

    //比较指定变量给定的数据值，筛选符合条件的值
    vector<pair<char *, long>> mallocedMemory;
    long cur = 0; //已选择的数据总长
    DataType type;
    vector<DataType> typeList;
    long sortDataPos = 0; //按值排序时要比较的数据的偏移量
    if (params->compareType != CompareType::CMP_NONE)
    {
        for (auto &file : selectedFiles)
        {
            typeList.clear();
            long len;
            EDVDB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
            char buff[len];
            EDVDB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

            //获取数据的偏移量和数据类型
            long pos = 0, bytes = 0;
            vector<long> posList, bytesList;
            long copyBytes = 0;
            int err;
            if (params->byPath)
            {
                char *pathCode = params->pathCode;
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
                for (int i = 0; i < bytesList.size(); i++)
                {
                    copyBytes += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
                }
            }
            else
            {
                err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
                copyBytes = type.hasTime ? bytes + 8 : bytes;
            }
            if (err != 0)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return err;
            }

            // if (len < pos)
            // {
            //     buffer->buffer = NULL;
            //     buffer->bufferMalloced = 0;
            //     return StatusCode::UNKNWON_DATAFILE;
            // }

            char copyValue[copyBytes]; //将要拷贝的数值

            if (params->byPath)
            {
                long lineCur = 0; //记录此行当前写位置
                for (int i = 0; i < bytesList.size(); i++)
                {
                    long curBytes = typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
                    memcpy(copyValue + lineCur, buff + posList[i], curBytes);
                    lineCur += curBytes;
                }
                if (params->valueName != NULL)
                    CurrentTemplate.FindDatatypePosByName(params->valueName, buff, sortDataPos, bytes, type);
                else
                    sortDataPos = -1; //没有指定变量名，不可按值排序
            }
            else
                memcpy(copyValue, buff + pos, copyBytes);

            int compareBytes = 0;
            if (params->valueName != NULL)
                compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type) == 0 ? bytes : 0;
            bool canCopy = false;
            if (compareBytes != 0) //可比较
            {
                char value[compareBytes]; //数值
                memcpy(value, buff + pos, compareBytes);
                //根据比较结果决定是否加入结果集
                int compareRes = DataType::CompareValue(type, value, params->compareValue);
                switch (params->compareType)
                {
                case CompareType::GT:
                {
                    if (compareRes > 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::LT:
                {
                    if (compareRes < 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::GE:
                {
                    if (compareRes >= 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::LE:
                {
                    if (compareRes <= 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::EQ:
                {
                    if (compareRes == 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                default:
                    break;
                }
            }
            else //不可比较，直接拷贝此数据
                canCopy = true;
            if (canCopy) //需要此数据
            {
                char *memory = (char *)malloc(copyBytes);
                memcpy(memory, copyValue, copyBytes);
                cur += copyBytes;
                mallocedMemory.push_back(make_pair(memory, copyBytes));
            }
        }
    }

    if (sortDataPos >= 0)
        sortResultByValue(mallocedMemory, sortDataPos, params, type);

    //动态分配内存
    char typeNum = params->byPath ? typeList.size() : 1; //数据类型总数
    char *data = (char *)malloc(cur + (int)typeNum + 1);
    int startPos;                                                                  //数据区起始位置
    if (!params->byPath)                                                           //根据变量名查询，仅单个变量
        startPos = CurrentTemplate.writeBufferHead(params->valueName, type, data); //写入缓冲区头，获取数据区起始位置
    else                                                                           //根据路径编码查询，可能有多个变量
        startPos = CurrentTemplate.writeBufferHead(params->pathCode, typeList, data);
    if (data == NULL)
    {
        buffer->buffer = NULL;
        buffer->bufferMalloced = 0;
        return StatusCode::BUFFER_FULL;
    }
    //拷贝数据
    cur = 0;
    for (auto &mem : mallocedMemory)
    {
        memcpy(data + cur + startPos, mem.first, mem.second);
        free(mem.first);
        cur += mem.second;
    }
    buffer->bufferMalloced = 1;
    buffer->buffer = data;
    buffer->length = cur + startPos;
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
int EDVDB_QueryLastRecords(DataBuffer *buffer, QueryParams *params)
{
    vector<pair<string, long>> selectedFiles;

    //获取每个数据文件，并带有时间戳
    readIDBFilesWithTimestamps(params->pathToLine, selectedFiles);

    //确认当前模版
    string str = params->pathToLine;
    if (CurrentTemplate.path != str || CurrentTemplate.path == "")
    {
        int err = 0;
        err = EDVDB_LoadSchema(params->pathToLine);
        if (err != 0)
        {
            buffer->buffer = NULL;
            buffer->bufferMalloced = 0;
            return err;
        }
    }

    //根据时间降序排序
    sort(selectedFiles.begin(), selectedFiles.end(),
         [](pair<string, long> iter1, pair<string, long> iter2) -> bool
         {
             return iter1.second > iter2.second;
         });

    //取排序后的文件中前queryNums个符合条件的文件的数据
    vector<pair<char *, long>> mallocedMemory; //已在堆区分配的进入筛选范围数据的内存地址和长度集
    long cur = 0;                              //记录已选中的数据总长度
    DataType type;
    vector<DataType> typeList;
    /*<-----!!!警惕内存泄露!!!----->*/
    if (params->compareType != CMP_NONE) //需要比较数值
    {
        int selectedNum = 0;
        long sortDataPos = 0;
        for (auto &file : selectedFiles)
        {
            typeList.clear();
            long len; //文件长度
            EDVDB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
            char buff[len]; //文件内容缓存
            EDVDB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

            //获取数据的偏移量和字节数
            long bytes = 0, pos = 0;         //单个变量
            vector<long> posList, bytesList; //多个变量
            long copyBytes = 0;

            int err;
            if (params->byPath)
            {
                char *pathCode = params->pathCode;
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
                for (int i = 0; i < bytesList.size(); i++)
                {
                    copyBytes += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
                }
            }
            else
            {
                err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
                copyBytes = type.hasTime ? bytes + 8 : bytes;
            }

            if (err != 0)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return err;
            }
            char copyValue[copyBytes];
            if (params->byPath)
            {
                long lineCur = 0; //记录此行当前写位置
                for (int i = 0; i < bytesList.size(); i++)
                {
                    long curBytes = typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
                    memcpy(copyValue + lineCur, buff + posList[i], curBytes);
                    lineCur += curBytes;
                }
                if (params->valueName != NULL)
                    CurrentTemplate.FindDatatypePosByName(params->valueName, buff, sortDataPos, bytes, type);
                else
                    sortDataPos = -1; //没有指定变量名，不可按值排序
            }
            else
                memcpy(copyValue, buff + pos, copyBytes);

            bool canCopy = false; //根据比较结果决定是否允许拷贝
            int compareBytes = 0;
            if (params->valueName != NULL)
                compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type) == 0 ? bytes : 0;

            if (compareBytes != 0) //可比较
            {
                char value[compareBytes]; //值缓存
                memcpy(value, buff + pos, compareBytes);
                //根据比较结果决定是否加入结果集
                int compareRes = DataType::CompareValue(type, value, params->compareValue);
                switch (params->compareType)
                {
                case CompareType::GT:
                {
                    if (compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::LT:
                {
                    if (compareRes == -1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::GE:
                {
                    if (compareRes == 0 || compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::LE:
                {
                    if (compareRes == 0 || compareRes == -1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case CompareType::EQ:
                {
                    if (compareRes == 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                default:
                    break;
                }
            }
            else //不可比较，直接拷贝此数据
                canCopy = true;

            if (canCopy) //需要此数据
            {
                char *memory = (char *)malloc(copyBytes);
                memcpy(memory, copyValue, copyBytes);
                cur += copyBytes;
                mallocedMemory.push_back(make_pair(memory, copyBytes));
                selectedNum++;
            }
            if (selectedNum == params->queryNums)
                break;
        }
        //查询最新数据时仅按时间排序，不可按值排序,仅可去除重复
        if (sortDataPos >= 0 && params->order == DISTINCT)
            sortResultByValue(mallocedMemory, sortDataPos, params, type);

        //已获取指定数量的数据，开始拷贝内存
        char *data;
        if (cur != 0)
        {
            char typeNum = typeList.size() == 0 ? 1 : typeList.size(); //数据类型总数
            int startPos;                                              //数据区起始位置

            data = (char *)malloc(cur + (int)typeNum * 11 + 1);
            if (!params->byPath)
                startPos = CurrentTemplate.writeBufferHead(params->valueName, type, data); //写入缓冲区头，获取数据区起始位置
            else
                startPos = CurrentTemplate.writeBufferHead(params->pathCode, typeList, data);
            if (data == NULL)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return StatusCode::BUFFER_FULL;
            }
            //拷贝数据
            cur = 0;
            for (auto &mem : mallocedMemory)
            {
                memcpy(data + cur + startPos, mem.first, mem.second);
                free(mem.first);
                cur += mem.second;
            }

            buffer->bufferMalloced = 1;
            buffer->buffer = data;
            buffer->length = cur + startPos;
        }
        else
        {
            buffer->bufferMalloced = 0;
        }
    }
    else //不需要比较数值,直接拷贝前N个文件的数据
    {
        DataType type;
        long sortDataPos = 0; //按值排序时要比较的数据的偏移量
        for (int i = 0; i < params->queryNums; i++)
        {
            typeList.clear();
            long len;
            EDVDB_GetFileLengthByPath(const_cast<char *>(selectedFiles[i].first.c_str()), &len);
            char buff[len];
            EDVDB_OpenAndRead(const_cast<char *>(selectedFiles[i].first.c_str()), buff);

            //获取数据的偏移量和数据类型
            long bytes = 0, pos = 0;         //单个变量
            vector<long> posList, bytesList; //多个变量
            long copyBytes = 0;
            int err;
            if (params->byPath)
            {
                char *pathCode = params->pathCode;
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
                for (int i = 0; i < bytesList.size(); i++)
                {
                    copyBytes += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
                }
            }
            else
            {
                err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
                copyBytes = type.hasTime ? bytes + 8 : bytes;
            }
            if (err != 0)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return err;
            }

            if (len < pos)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return StatusCode::UNKNWON_DATAFILE;
            }

            char *memory = (char *)malloc(copyBytes);
            if (params->byPath)
            {
                long lineCur = 0; //记录此行当前写位置
                for (int i = 0; i < bytesList.size(); i++)
                {
                    long curBytes = typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i]; //本次写字节数
                    memcpy(memory + lineCur, buff + posList[i], curBytes);
                    lineCur += curBytes;
                }
                if (params->valueName != NULL)
                    CurrentTemplate.FindDatatypePosByName(params->valueName, buff, sortDataPos, bytes, type);
                else
                    sortDataPos = -1; //没有指定变量名，不可按值排序
            }
            else
                memcpy(memory, buff + pos, copyBytes);
            mallocedMemory.push_back(make_pair(memory, copyBytes));
            cur += copyBytes;
        }
        //查询最新数据时仅按时间排序，不可按值排序,仅可去除重复
        if (sortDataPos >= 0 && params->order == DISTINCT)
            sortResultByValue(mallocedMemory, sortDataPos, params, type);
        if (cur != 0)
        {
            char typeNum = typeList.size() == 0 ? 1 : typeList.size(); //数据类型总数
            int startPos;                                              //数据区起始位置
            char *data = (char *)malloc(cur + (int)typeNum * 11 + 1);
            if (data == NULL)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return StatusCode::BUFFER_FULL;
            }
            if (!params->byPath)
                startPos = CurrentTemplate.writeBufferHead(params->valueName, type, data); //写入缓冲区头，获取数据区起始位置
            else
                startPos = CurrentTemplate.writeBufferHead(params->pathCode, typeList, data);
            cur = 0;
            for (auto &mem : mallocedMemory)
            {
                memcpy(data + cur + startPos, mem.first, mem.second);
                free(mem.first);
                cur += mem.second;
            }
            buffer->bufferMalloced = 1;
            buffer->buffer = data;
            buffer->length = cur + startPos;
        }
        else
            buffer->bufferMalloced = 0;
    }
    /*<-----!!!!!!----->*/
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

            //获取数据的偏移量和字节数
            long bytes = 0, pos = 0;         //单个变量
            vector<long> posList, bytesList; //多个变量
            long copyBytes = 0;
            int err;
            DataType type;
            vector<DataType> typeList;
            char *pathCode;
            if (params->byPath)
            {
                pathCode = params->pathCode;
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
                for (int i = 0; i < bytesList.size(); i++)
                {
                    copyBytes += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
                }
            }
            else
            {
                err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
                copyBytes = type.hasTime ? bytes + 8 : bytes;
            }
            if (err != 0)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return err;
            }
            if (len < pos)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return StatusCode::UNKNWON_DATAFILE;
            }

            //开始拷贝数据

            char typeNum = typeList.size() == 0 ? 1 : typeList.size(); //数据类型总数

            char *data = (char *)malloc(copyBytes + 1 + (int)typeNum * 11);
            int startPos;
            if (!params->byPath)
                startPos = CurrentTemplate.writeBufferHead(params->valueName, type, data); //写入缓冲区头，获取数据区起始位置
            else
                startPos = CurrentTemplate.writeBufferHead(params->pathCode, typeList, data);
            if (data == NULL)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                return StatusCode::BUFFER_FULL;
            }

            buffer->bufferMalloced = 1;
            if (params->byPath)
            {
                long lineCur = 0;
                for (int i = 0; i < bytesList.size(); i++)
                {
                    long curBytes = typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i]; //本次写字节数
                    memcpy(data + startPos + lineCur, buff + posList[i], curBytes);
                    lineCur += curBytes;
                }
            }
            else
                memcpy(data + startPos, buff + pos, copyBytes);
            buffer->buffer = data;
            buffer->length = copyBytes + startPos;
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
    return 0;
}

int TEST_MAX(DataBuffer *buffer, QueryParams *params)
{
    EDVDB_ExecuteQuery(buffer, params);
    cout<<CurrentTemplate.schemas.size()<<endl;
    int typeNum = buffer->buffer[0];
    DataType typeList[typeNum];
    int recordLength = 0;
    for (int i = 0; i < typeNum; i++)
    {
        DataType type;
        int typeVal = buffer->buffer[i * 11 + 1];
        switch ((typeVal - 1) / 10)
        {
        case 0:
        {
            type.isArray = false;
            type.hasTime = false;
            break;
        }
        case 1:
        {
            type.isArray = false;
            type.hasTime = true;
            break;
        }
        case 2:
        {
            type.isArray = true;
            type.hasTime = false;
            break;
        }
        case 3:
        {
            type.isArray = true;
            type.hasTime = true;
            break;
        }
        default:
            break;
        }
        type.valueType = (ValueType::ValueType)((typeVal - 1) % 10 + 1);
        type.valueBytes = DataType::GetValueBytes(type.valueType);
        typeList[i] = type;
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
}
int main()
{
    DataTypeConverter converter;

    long length;
    converter.CheckBigEndian();
    // cout << EDVDB_LoadSchema("/");
    QueryParams params;
    params.pathToLine = "";
    params.fileID = "XinFeng2";
    char code[10];
    code[0] = (char)0;
    code[1] = (char)1;
    code[2] = (char)0;
    code[3] = (char)1;
    code[4] = 'R';
    code[5] = (char)1;
    code[6] = 0;
    code[7] = (char)0;
    code[8] = (char)0;
    code[9] = (char)0;
    params.pathCode = code;
    params.valueName = "S1R8";
    // params.valueName = NULL;
    params.start = 1648516212100;
    params.end = 1648516221100;
    params.order = TIME_ASC;
    params.compareType = GT;
    params.compareValue = "6";
    params.queryType = FILEID;
    params.byPath = 1;
    params.queryNums = 3;
    DataBuffer buffer;
    buffer.length = 0;
    vector<long> bytes, positions;
    vector<DataType> types;
    // CurrentTemplate.FindMultiDatatypePosByCode(code, positions, bytes, types);
    //  EDVDB_QueryLastRecords(&buffer, &params);
    // EDVDB_QueryLastRecords(&buffer, &params);
    // EDVDB_MAX(&buffer, &params);
    EDVDB_MAX(&buffer, &params);
    TEST_MAX(&buffer, &params);
    //EDVDB_QueryByTimespan(&buffer, &params);

    if (buffer.bufferMalloced)
    {
        char buf[buffer.length];
        memcpy(buf, buffer.buffer, buffer.length);
        for (int i = 0; i < 300; i++)
        {
            cout << (int)buf[i];
            if (i % 11 == 0)
                cout << endl;
        }

//         free(buffer.buffer);
//     }

//     buffer.buffer = NULL;
//     return 0;
 }