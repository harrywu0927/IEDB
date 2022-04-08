

#include "../include/QueryRequest.hpp"
#include "../include/utils.hpp"
#include "../include/Packer.hpp"
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
Template CurrentTemplate;
/**
 * @brief 从指定路径加载模版文件
 * @param path    路径
 *
 * @return  0:success,
 *          others: StatusCode
 * @note    在文件夹中找到模版文件，找到后读取，每个变量的前30字节为变量名，接下来30字节为数据类型，最后10字节为
 *          路径编码，分别解析之，构造、生成模版结构并将其设置为当前模版
 */
int DB_LoadSchema(const char *path)
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
int DB_UnloadSchema(const char *pathToUnset)
{
    return TemplateManager::UnsetTemplate(pathToUnset);
}

/**
 * @brief 根据指定数据类型的数值对查询结果排序或去重，此函数仅对内存地址-长度对操作
 * @param mallocedMemory        已在堆区分配的内存地址-长度对集
 * @param poses           指定数据在缓冲区中的位置
 * @param params        查询参数
 * @param type        数据类型
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int sortResultByValue(vector<pair<char *, long>> &mallocedMemory, vector<long> &poses, DB_QueryParams *params, DataType &type)
{
    vector<pair<pair<char *, long>, long>> memAndPos; //使数据偏移与内存-长度对建立关联
    for (int i = 0; i < poses.size(); i++)
    {
        memAndPos.push_back(make_pair(mallocedMemory[i], poses[i]));
    }

    switch (params->order)
    {
    case ASCEND: //升序
    {
        sort(memAndPos.begin(), memAndPos.end(),
             [&type](pair<pair<char *, long>, long> iter1, pair<pair<char *, long>, long> iter2) -> bool
             {
                 char value1[type.valueBytes], value2[type.valueBytes];
                 memcpy(value1, iter1.first.first + iter1.second, type.valueBytes);
                 memcpy(value2, iter2.first.first + iter2.second, type.valueBytes);
                 return DataType::CompareValueInBytes(type, value1, value2) < 0;
             });
        for (int i = 0; i < mallocedMemory.size(); i++)
        {
            mallocedMemory[i] = memAndPos[i].first;
        }

        break;
    }

    case DESCEND: //降序
    {
        sort(memAndPos.begin(), memAndPos.end(),
             [&type](pair<pair<char *, long>, long> iter1, pair<pair<char *, long>, long> iter2) -> bool
             {
                 char value1[type.valueBytes], value2[type.valueBytes];
                 memcpy(value1, iter1.first.first + iter1.second, type.valueBytes);
                 memcpy(value2, iter2.first.first + iter2.second, type.valueBytes);
                 return DataType::CompareValueInBytes(type, value1, value2) > 0;
             });
        for (int i = 0; i < mallocedMemory.size(); i++)
        {
            mallocedMemory[i] = memAndPos[i].first;
        }
        break;
    }
    case DISTINCT: //去除重复
    {
        vector<pair<char *, long>> existedValues;
        for (int i = 0; i < mallocedMemory.size(); i++)
        {
            char value[type.valueBytes];
            memcpy(value, mallocedMemory[i].first + poses[i], type.valueBytes);
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
                    free(mallocedMemory[i].first);                    //重复值，释放此内存
                    mallocedMemory.erase(mallocedMemory.begin() + i); //注：此操作在查询量大时效率可能很低
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
    // DataTypeConverter converter;
    // int a = 0;
    // for (auto it = mallocedMemory.begin(); it != mallocedMemory.end(); it++)
    // {
    //     char val[type.valueBytes];
    //     memcpy(val, it->first + memAndPos[a].second, type.valueBytes);
    //     cout << converter.ToUInt32(val) << endl;
    //     a++;
    // }

    return 0;
}

//根据时间升序或降序排序
void sortByTime(vector<pair<string, long>> &selectedFiles, DB_Order order)
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
int DB_ExecuteQuery(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    switch (params->queryType)
    {
    case TIMESPAN:
        return DB_QueryByTimespan(buffer, params);
        break;
    case LAST:
        return DB_QueryLastRecords(buffer, params);
        break;
    case FILEID:
        return DB_QueryByFileID(buffer, params);
        break;
    default:
        break;
    }
    return StatusCode::NO_QUERY_TYPE;
}

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
int DB_QueryWholeFile(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;

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
            err = DB_LoadSchema(params->pathToLine);
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
        if (params->compareType != DB_CompareType::CMP_NONE)
        {

            for (auto &file : selectedFiles)
            {
                long len; //文件长度
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char buff[len]; //文件内容缓存
                DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

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
                case DB_CompareType::GT:
                {
                    if (compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::LT:
                {
                    if (compareRes == -1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::GE:
                {
                    if (compareRes == 0 || compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::LE:
                {
                    if (compareRes == 0 || compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::EQ:
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
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char buff[len]; //文件内容缓存
                DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

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
            err = DB_LoadSchema(params->pathToLine);
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
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char buff[len]; //文件内容缓存
                DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

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
                case DB_CompareType::GT:
                {
                    if (compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::LT:
                {
                    if (compareRes == -1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::GE:
                {
                    if (compareRes == 0 || compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::LE:
                {
                    if (compareRes == 0 || compareRes == -1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::EQ:
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
                DB_GetFileLengthByPath(const_cast<char *>(filesWithTime[i].first.c_str()), &len);
                char buff[len];
                DB_OpenAndRead(const_cast<char *>(filesWithTime[i].first.c_str()), buff);
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
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char buff[len];
                DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

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
int DB_QueryByTimespan(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;
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
        err = DB_LoadSchema(params->pathToLine);
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
    vector<DataType> typeList, selectedTypes;
    vector<long> sortDataPoses; //按值排序时要比较的数据的偏移量
    for (auto &file : selectedFiles)
    {
        typeList.clear();
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
        char buff[len];
        DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

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

        char copyValue[copyBytes]; //将要拷贝的数值
        long sortPos = 0;
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
                sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, params->pathCode, typeList);
            // else
            // sortDataPoses.push_back(-1); //没有指定变量名，不可按值排序
        }
        else
            memcpy(copyValue, buff + pos, copyBytes);

        int compareBytes = 0;
        if (params->valueName != NULL)
            compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type) == 0 ? bytes : 0;
        bool canCopy = false;
        if (params->compareType != CMP_NONE && compareBytes != 0) //可比较
        {
            char value[compareBytes]; //数值
            memcpy(value, buff + pos, compareBytes);
            //根据比较结果决定是否加入结果集
            int compareRes = DataType::CompareValue(type, value, params->compareValue);
            switch (params->compareType)
            {
            case DB_CompareType::GT:
            {
                if (compareRes > 0)
                {
                    canCopy = true;
                }
                break;
            }
            case DB_CompareType::LT:
            {
                if (compareRes < 0)
                {
                    canCopy = true;
                }
                break;
            }
            case DB_CompareType::GE:
            {
                if (compareRes >= 0)
                {
                    canCopy = true;
                }
                break;
            }
            case DB_CompareType::LE:
            {
                if (compareRes <= 0)
                {
                    canCopy = true;
                }
                break;
            }
            case DB_CompareType::EQ:
            {
                if (compareRes == 0)
                {
                    canCopy = true;
                }
                break;
            }
            default:
                canCopy = true;
                break;
            }
        }
        else //不可比较，直接拷贝此数据
            canCopy = true;
        if (canCopy) //需要此数据
        {
            sortDataPoses.push_back(sortPos);
            char *memory = (char *)malloc(copyBytes);
            memcpy(memory, copyValue, copyBytes);
            cur += copyBytes;
            mallocedMemory.push_back(make_pair(memory, copyBytes));
        }
    }
    DataTypeConverter converter;
    for (int i = 0; i < sortDataPoses.size(); i++)
    {
        char val[type.valueBytes];
        memcpy(val, mallocedMemory[i].first + sortDataPoses[i], type.valueBytes);
        cout << converter.ToUInt32(val) << endl;
    }

    if (sortDataPoses.size() > 0) //尚有问题
        sortResultByValue(mallocedMemory, sortDataPoses, params, type);

    //动态分配内存
    char typeNum = params->byPath ? typeList.size() : 1; //数据类型总数
    char *data = (char *)malloc(cur + (int)typeNum * 11 + 1);
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
 * @brief 根据给定的时间段和路径编码或变量名在某一产线文件夹下的数据文件中查询数据，可比较数据大小筛选，可按照时间
 *          将结果升序或降序排序，数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note   支持idb文件和pak文件混合查询,此处默认pak文件中的时间均早于idb和idbzip文件！！
 */
int DB_QueryByTimespan_New(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;
    vector<pair<string, long>> filesWithTime, selectedFiles, selectedPacks;
    vector<string> packFiles;

    //获取每个数据文件，并带有时间戳
    readDataFilesWithTimestamps(params->pathToLine, filesWithTime);

    //获取打包文件
    readPakFilesList(params->pathToLine, packFiles);

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }
    for (auto &file : packFiles)
    {
        string tmp = file;
        vector<string> timespan = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "-");
        if (timespan.size() > 0)
        {
            long start = atol(timespan[0].c_str());
            long end = atol(timespan[1].c_str());
            if ((start < params->start && end >= params->start) || (end < params->end && end >= params->start) || (start <= params->start && end >= params->end)) //落入或部分落入时间区间
            {
                selectedPacks.push_back(make_pair(file, start));
            }
        }
        else
        {
            return StatusCode::FILENAME_MODIFIED;
        }
    }

    //确认当前模版
    string str = params->pathToLine;

    if (CurrentTemplate.path != str || CurrentTemplate.path == "")
    {
        int err = 0;
        err = DB_LoadSchema(params->pathToLine);
        if (err != 0)
        {
            buffer->buffer = NULL;
            buffer->bufferMalloced = 0;
            return err;
        }
    }

    //根据时间升序排序
    sortByTime(selectedFiles, TIME_ASC);
    sortByTime(selectedPacks, TIME_ASC);

    vector<pair<char *, long>> mallocedMemory; //当前已分配的内存地址-长度对列表
    long cur = 0;                              //已选择的数据总长
    DataType type;
    vector<DataType> typeList, selectedTypes;
    vector<long> sortDataPoses; //按值排序时要比较的数据的偏移量

    //先对时序在前的对包文件检索
    for (auto &pack : selectedPacks)
    {
        PackFileReader packReader(pack.first);
        int fileNum;
        string templateName;
        packReader.ReadPackHead(fileNum, templateName);
        // TemplateManager::CheckTemplate(templateName);
        for (int i = 0; i < fileNum; i++)
        {
            typeList.clear();
            long timestamp;
            int readLength, zipType;
            long dataPos = packReader.Next(readLength, timestamp, zipType);
            if (timestamp < params->start || timestamp > params->end) //在时间区间外
                continue;
            char *buff = (char *)malloc(CurrentTemplate.totalBytes);
            switch (zipType)
            {
            case 0:
            {
                memcpy(buff, packReader.packBuffer + dataPos, readLength);
                break;
            }
            case 1:
            {
                ReZipBuff(buff, readLength, params->pathToLine);
                break;
            }
            case 2:
            {
                memcpy(buff, packReader.packBuffer + dataPos, readLength);
                for (int j = 0; j < readLength; j++)
                {
                    cout << (int)buff[j] << " ";
                }
                ReZipBuff(buff, readLength, params->pathToLine);
                for (int j = 0; j < readLength; j++)
                {
                    cout << (int)buff[j] << " ";
                }
                cout << endl;
                break;
            }
            default:
                free(buff);
                return StatusCode::UNKNWON_DATAFILE;
                break;
            }
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

            char copyValue[copyBytes]; //将要拷贝的数值
            long sortPos = 0;
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
                    sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, params->pathCode, typeList);
            }
            else
                memcpy(copyValue, buff + pos, copyBytes);

            int compareBytes = 0;
            if (params->valueName != NULL)
                compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type) == 0 ? bytes : 0;
            bool canCopy = false;
            if (params->compareType != CMP_NONE && compareBytes != 0) //可比较
            {
                char value[compareBytes]; //数值
                memcpy(value, buff + pos, compareBytes);

                //根据比较结果决定是否加入结果集
                int compareRes = DataType::CompareValue(type, value, params->compareValue);
                switch (params->compareType)
                {
                case DB_CompareType::GT:
                {
                    if (compareRes > 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::LT:
                {
                    if (compareRes < 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::GE:
                {
                    if (compareRes >= 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::LE:
                {
                    if (compareRes <= 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::EQ:
                {
                    if (compareRes == 0)
                    {
                        canCopy = true;
                    }
                    break;
                }
                default:
                    canCopy = true;
                    break;
                }
            }
            else //不可比较，直接拷贝此数据
                canCopy = true;
            if (canCopy) //需要此数据
            {
                sortDataPoses.push_back(sortPos);
                char *memory = (char *)malloc(copyBytes);
                memcpy(memory, copyValue, copyBytes);
                cur += copyBytes;
                mallocedMemory.push_back(make_pair(memory, copyBytes));
            }
            free(buff);
        }
    }

    //对时序在后的普通文件检索
    for (auto &file : selectedFiles)
    {
        typeList.clear();
        long len;

        if (file.first.find(".idbzip") != string::npos)
        {
            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
        }
        else if (file.first.find("idb") != string::npos)
        {
            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
        }
        char *buff = (char *)malloc(CurrentTemplate.totalBytes);
        DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
        if (file.first.find(".idbzip") != string::npos)
        {
            ReZipBuff(buff, (int &)len, params->pathToLine);
        }
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

        char copyValue[copyBytes]; //将要拷贝的数值
        long sortPos = 0;
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
                sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, params->pathCode, typeList);
        }
        else
            memcpy(copyValue, buff + pos, copyBytes);

        int compareBytes = 0;
        if (params->valueName != NULL)
            compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type) == 0 ? bytes : 0;
        bool canCopy = false;
        if (params->compareType != CMP_NONE && compareBytes != 0) //可比较
        {
            char value[compareBytes]; //数值
            memcpy(value, buff + pos, compareBytes);
            //根据比较结果决定是否加入结果集
            int compareRes = DataType::CompareValue(type, value, params->compareValue);
            switch (params->compareType)
            {
            case DB_CompareType::GT:
            {
                if (compareRes > 0)
                {
                    canCopy = true;
                }
                break;
            }
            case DB_CompareType::LT:
            {
                if (compareRes < 0)
                {
                    canCopy = true;
                }
                break;
            }
            case DB_CompareType::GE:
            {
                if (compareRes >= 0)
                {
                    canCopy = true;
                }
                break;
            }
            case DB_CompareType::LE:
            {
                if (compareRes <= 0)
                {
                    canCopy = true;
                }
                break;
            }
            case DB_CompareType::EQ:
            {
                if (compareRes == 0)
                {
                    canCopy = true;
                }
                break;
            }
            default:
                canCopy = true;
                break;
            }
        }
        else //不可比较，直接拷贝此数据
            canCopy = true;
        if (canCopy) //需要此数据
        {
            sortDataPoses.push_back(sortPos);
            char *memory = (char *)malloc(copyBytes);
            memcpy(memory, copyValue, copyBytes);
            cur += copyBytes;
            mallocedMemory.push_back(make_pair(memory, copyBytes));
        }
        free(buff);
    }
    DataTypeConverter converter;
    for (int i = 0; i < sortDataPoses.size(); i++)
    {
        char val[type.valueBytes];
        memcpy(val, mallocedMemory[i].first + sortDataPoses[i], type.valueBytes);
        cout << converter.ToUInt32(val) << endl;
    }

    if (sortDataPoses.size() > 0) //尚有问题
        sortResultByValue(mallocedMemory, sortDataPoses, params, type);

    //动态分配内存
    char typeNum = params->byPath ? typeList.size() : 1; //数据类型总数
    char *data = (char *)malloc(cur + (int)typeNum * 11 + 1);
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
    if (params->order == TIME_DSC) //时间降序，从内存地址集中反向拷贝
    {
        for (int i = mallocedMemory.size() - 1; i >= 0; i--)
        {
            memcpy(data + cur + startPos, mallocedMemory[i].first, mallocedMemory[i].second);
            free(mallocedMemory[i].first);
            cur += mallocedMemory[i].second;
        }
    }
    else //否则按照默认顺序升序排列即可
    {
        for (auto &mem : mallocedMemory)
        {
            memcpy(data + cur + startPos, mem.first, mem.second);
            free(mem.first);
            cur += mem.second;
        }
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
int DB_QueryLastRecords(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;
    vector<pair<string, long>> selectedFiles;

    //获取每个数据文件，并带有时间戳
    readIDBFilesWithTimestamps(params->pathToLine, selectedFiles);

    //确认当前模版
    string str = params->pathToLine;
    if (CurrentTemplate.path != str || CurrentTemplate.path == "")
    {
        int err = 0;
        err = DB_LoadSchema(params->pathToLine);
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
            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
            char buff[len]; //文件内容缓存
            DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

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
                case DB_CompareType::GT:
                {
                    if (compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::LT:
                {
                    if (compareRes == -1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::GE:
                {
                    if (compareRes == 0 || compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::LE:
                {
                    if (compareRes == 0 || compareRes == -1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::EQ:
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
        // if (sortDataPos >= 0 && params->order == DISTINCT)
        //    sortResultByValue(mallocedMemory, sortDataPos, params, type);

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
            DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[i].first.c_str()), &len);
            char buff[len];
            DB_OpenAndRead(const_cast<char *>(selectedFiles[i].first.c_str()), buff);

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
        // if (sortDataPos >= 0 && params->order == DISTINCT)
        // sortResultByValue(mallocedMemory, sortDataPos, params, type);
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
int DB_QueryByFileID(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;
    vector<string> files;
    readIDBFilesList(params->pathToLine, files);
    for (string &file : files)
    {
        if (file.find(params->fileID) != string::npos)
        {
            long len;
            DB_GetFileLengthByPath(const_cast<char *>(file.c_str()), &len);
            char buff[len];
            DB_OpenAndRead(const_cast<char *>(file.c_str()), buff);
            DB_LoadSchema(params->pathToLine);

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
int DB_QueryByFileID_New(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;
    vector<string> dataFiles, packFiles;
    readDataFiles(params->pathToLine, dataFiles);
    readPakFilesList(params->pathToLine, packFiles);
    // if (paths.size() > 0)
    //     prefix = paths[paths.size() - 1];
    for (auto &pack : packFiles)
    {
        PackFileReader packReader(pack);
        int fileNum;
        string templateName;
        packReader.ReadPackHead(fileNum, templateName);
        for (int i = 0; i < fileNum; i++)
        {
            string fileID;
            int readLength, zipType;
            long dataPos = packReader.Next(readLength, fileID, zipType);
            if (fileID == params->fileID)
            {
                char *buff = (char *)malloc(CurrentTemplate.totalBytes);
                switch (zipType)
                {
                case 0:
                {
                    memcpy(buff, packReader.packBuffer + dataPos, readLength);
                    break;
                }
                case 1:
                {
                    ReZipBuff(buff, readLength, params->pathToLine);
                    break;
                }
                case 2:
                {
                    memcpy(buff, packReader.packBuffer + dataPos, readLength);
                    for (int j = 0; j < readLength; j++)
                    {
                        cout << (int)buff[j] << " ";
                    }
                    ReZipBuff(buff, readLength, params->pathToLine);
                    for (int j = 0; j < readLength; j++)
                    {
                        cout << (int)buff[j] << " ";
                    }
                    cout << endl;
                    break;
                }
                default:
                    free(buff);
                    return StatusCode::UNKNWON_DATAFILE;
                    break;
                }
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
            }
        }
    }

    for (string &file : dataFiles)
    {
        if (file.find(params->fileID) != string::npos)
        {
            long len;
            DB_GetFileLengthByPath(const_cast<char *>(file.c_str()), &len);
            char buff[len];
            DB_OpenAndRead(const_cast<char *>(file.c_str()), buff);
            DB_LoadSchema(params->pathToLine);

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

//暂不支持带有图片或数组的多变量聚合
int TEST_MAX(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    if (CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;

    DB_ExecuteQuery(buffer, params);
    // cout << CurrentTemplate.schemas.size() << endl;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    // vector<long> curs;
    long bufPos = 0;
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
        typeList.push_back(type);
        // if (type.valueType == ValueType::IMAGE)
        // {
        //     int num;
        //     char imgLen[2];
        //     imgLen[0] = buffer->buffer[recordLength];
        //     imgLen[1] = buffer->buffer[recordLength + 1];
        //     num = (int)converter.ToUInt16(imgLen);

        // }
        // curs.push_back(recordLength);
        // bufPos += type.valueBytes + (type.hasTime ? 8 : 0);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        // cur = curs[i];
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            short max = INT16_MIN;
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                short value = converter.ToInt16(val);
                if (max < value)
                    max = value;
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, &max, 2);
            newBufCur += 2;
            break;
        }
        case ValueType::UINT:
        {
            uint16_t max = 0;
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                short value = converter.ToUInt16(val);
                if (max < value)
                    max = value;
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 2);
            newBufCur += 2;
            break;
        }
        case ValueType::DINT:
        {
            int max = INT_MIN;
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToInt32(val);
                if (max < value)
                    max = value;
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            uint max = 0;
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToUInt32(val);
                if (max < value)
                    max = value;
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float max = __FLT_MIN__;
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToFloat(val);
                if (max < value)
                    max = value;
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            int max = INT_MIN;
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToInt32(val);
                if (max < value)
                    max = value;
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char max = INT8_MIN;
            char value;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                if (max < value)
                    max = value;
            }
            cout << "max:" << (int)max << endl;
            newBuffer[newBufCur++] = value;
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，获取单个或多个非数组变量各自的最大值
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_MAX(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::SetTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
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
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            short max = INT16_MIN;
            char res[2];
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                short value = converter.ToInt16(val);
                if (max < value)
                {
                    max = value;
                    memcpy(res, val, 2);
                }
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, res, 2);
            newBufCur += 2;
            break;
        }
        case ValueType::UINT:
        {
            uint16_t max = 0;
            char val[2];
            char res[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                short value = converter.ToUInt16(val);
                if (max < value)
                {
                    max = value;
                    memcpy(res, val, 2);
                }
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 2);
            newBufCur += 2;
            break;
        }
        case ValueType::DINT:
        {
            int max = INT_MIN;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToInt32(val);
                if (max < value)
                {
                    max = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            uint max = 0;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToUInt32(val);
                cout << value << endl;
                if (max < value)
                {
                    max = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float max = __FLT_MIN__;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToFloat(val);
                if (max < value)
                {
                    max = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            int max = INT_MIN;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToInt32(val);
                if (max < value)
                {
                    max = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char max = INT8_MIN;
            char value;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                if (max < value)
                    max = value;
            }
            cout << "max:" << (int)max << endl;
            memcpy(newBuffer + newBufCur++, &value, 1);
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，获取单个或多个非数组变量各自的最小值
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_MIN(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::SetTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
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
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            short min = INT16_MAX;
            char res[2];
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                short value = converter.ToInt16(val);
                if (min > value)
                {
                    min = value;
                    memcpy(res, val, 2);
                }
            }
            cout << "min:" << min << endl;
            memcpy(newBuffer + newBufCur, res, 2);
            newBufCur += 2;
            break;
        }
        case ValueType::UINT:
        {
            uint16_t min = UINT16_MAX;
            char val[2];
            char res[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                short value = converter.ToUInt16(val);
                if (min > value)
                {
                    min = value;
                    memcpy(res, val, 2);
                }
            }
            cout << "min:" << min << endl;
            memcpy(newBuffer + newBufCur, val, 2);
            newBufCur += 2;
            break;
        }
        case ValueType::DINT:
        {
            int min = INT_MAX;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToInt32(val);
                if (min > value)
                {
                    min = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "min:" << min << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            uint min = UINT32_MAX;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToUInt32(val);
                if (min > value)
                {
                    min = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "min:" << min << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float min = __FLT_MAX__;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToFloat(val);
                if (min > value)
                {
                    min = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "min:" << min << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            int min = INT_MAX;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToInt32(val);
                if (min > value)
                {
                    min = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "min:" << min << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char min = INT8_MAX;
            char value;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                if (min > value)
                    min = value;
            }
            cout << "min:" << (int)min << endl;
            memcpy(newBuffer + newBufCur++, &value, 1);
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，对单个或多个非数组变量各自求和
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_SUM(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::SetTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 1; //每行的长度
    long bufPos = 0;
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
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        int sum = 0;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            char res[4] = {0};
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                sum += converter.ToInt16(val);
            }
            for (int k = 0; k < 4; k++)
            {
                res[3 - k] |= sum;
                sum >>= 8;
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UINT:
        {
            char res[4] = {0};
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                sum += converter.ToUInt16(val);
            }
            for (int k = 0; k < 4; k++)
            {
                res[3 - k] |= sum;
                sum >>= 8;
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::DINT:
        {
            char res[4] = {0};
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                sum += converter.ToInt32(val);
            }
            for (int k = 0; k < 4; k++)
            {
                res[3 - k] |= sum;
                sum >>= 8;
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            char res[4] = {0};
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                sum += converter.ToUInt32(val);
            }
            for (int k = 0; k < 4; k++)
            {
                res[3 - k] |= sum;
                sum >>= 8;
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float floatSum = 0;
            char res[4] = {0};
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                floatSum += converter.ToFloat(val);
            }
            memcpy(newBuffer + newBufCur, &floatSum, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            char res[4] = {0};
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                sum += converter.ToUInt32(val);
            }
            for (int k = 0; k < 4; k++)
            {
                res[3 - k] |= sum;
                sum >>= 8;
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char res[4] = {0};
            char value;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                sum += value;
            }
            for (int k = 0; k < 4; k++)
            {
                res[3 - k] |= sum;
                sum >>= 8;
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    for (int i = 0; i < typeList.size(); i++)
    {
        if (typeList[i].valueType != ValueType::REAL)
            typeList[i].valueType = ValueType::DINT; //可能超出32位数字表示范围，不是浮点数暂时统一用DINT表示
    }
    // CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，对单个或多个非数组变量各自求平均值
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_AVG(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::SetTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    DB_ExecuteQuery(buffer, params);
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
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
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        float sum = 0;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                sum += converter.ToInt16(val);
            }
            float res = sum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UINT:
        {
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                sum += converter.ToUInt16(val);
            }
            float res = sum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::DINT:
        {
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                sum += converter.ToInt32(val);
            }
            float res = sum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                sum += converter.ToUInt32(val);
            }
            float res = sum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float floatSum = 0;
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                floatSum += converter.ToFloat(val);
            }
            float res = floatSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                sum += converter.ToUInt32(val);
            }
            float res = sum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char value;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                sum += value;
            }
            float res = sum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    for (int i = 0; i < typeList.size(); i++)
    {
        if (typeList[i].valueType != ValueType::REAL)
            typeList[i].valueType = ValueType::REAL; //均值统一用浮点数表示
    }
    // CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，对单个或多个非数组变量各自求计数
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_COUNT(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::SetTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
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
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    char res[4] = {0};
    for (int k = 0; k < 4; k++)
    {
        res[3 - k] |= rows;
        rows >>= 8;
    }
    memcpy(newBuffer + newBufCur, res, 4);
    DataTypeConverter converter;
    cout << "count:" << converter.ToUInt32(res) << endl;
    free(buffer->buffer);
    buffer->buffer = NULL;
    for (int i = 0; i < typeList.size(); i++)
    {
        if (typeList[i].valueType != ValueType::UDINT)
            typeList[i].valueType = ValueType::UDINT; //计数统一用32位无符号数表示
    }
    // CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，对单个或多个非数组变量各自求标准差
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_STD(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::SetTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
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
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        float sum = 0;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            char val[2];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToInt16(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);

            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UINT:
        {
            char val[2];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt16(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::DINT:
        {
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToInt32(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt32(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);
            cout << "std:" << res << endl;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float floatSum = 0;
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt16(val);
                floatSum += value;
                vals.push_back(value);
            }
            float avg = floatSum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt32(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char value;
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    for (int i = 0; i < typeList.size(); i++)
    {
        if (typeList[i].valueType != ValueType::REAL)
            typeList[i].valueType = ValueType::REAL; //标准差统一用浮点数表示
    }
    // CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，对单个或多个非数组变量各自求方差
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_STDEV(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::CheckTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
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
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        float sum = 0;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            char val[2];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToInt16(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            cout << "stdev:" << res << endl;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UINT:
        {
            char val[2];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt16(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::DINT:
        {
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToInt32(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt32(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float floatSum = 0;
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt16(val);
                floatSum += value;
                vals.push_back(value);
            }
            float avg = floatSum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt32(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char value;
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    for (int i = 0; i < typeList.size(); i++)
    {
        if (typeList[i].valueType != ValueType::REAL)
            typeList[i].valueType = ValueType::REAL; //方差统一用浮点数表示
    }
    // CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    buffer->buffer = newBuffer;
    return 0;
}

int main()
{
    DataTypeConverter converter;
    // PackFileReader reader;
    // return 0;
    long length;
    converter.CheckBigEndian();
    // cout << EDVDB_LoadSchema("/");
    DB_QueryParams params;
    params.pathToLine = "/";
    params.fileID = "Jinfei9111";
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
    // params.pathCode = code;
    params.valueName = "S1ON";
    // params.valueName = NULL;
    params.start = 1648812610100;
    params.end = 1648812630100;
    params.order = TIME_ASC;
    params.compareType = LT;
    params.compareValue = "666";
    params.queryType = LAST;
    params.byPath = 0;
    params.queryNums = 3;
    DB_DataBuffer buffer;
    buffer.savePath = "/";
    // buffer.length = 4;
    // buffer.buffer = "test";
    vector<long> bytes, positions;
    vector<DataType> types;
    cout << settings("Pack_Mode") << endl;
    // vector<pair<string, long>> files;
    // readDataFilesWithTimestamps("", files);
    // Packer::Pack("/",files);
    DB_QueryByTimespan(&buffer, &params);
    // DB_QueryByFileID(&buffer, &params);
    // TemplateManager::CheckTemplate(params.pathToLine);
    // EDVDB_ExecuteQuery(&buffer, &params);
    // EDVDB_QueryLastRecords(&buffer, &params);
    // EDVDB_InsertRecord(&buffer,0);
    // cout<<DB_MAX(&buffer, &params)<<endl;
    // EDVDB_COUNT(&buffer, &params);
    //  TEST_MAX(&buffer, &params);
    // EDVDB_QueryByTimespan(&buffer, &params);

    if (buffer.bufferMalloced)
    {
        char buf[buffer.length];
        memcpy(buf, buffer.buffer, buffer.length);
        cout << buffer.length << endl;
        for (int i = 0; i <= buffer.length; i++)
        {
            cout << (int)buf[i] << " ";
            if (i % 11 == 0)
                cout << endl;
        }

        free(buffer.buffer);
    }

    // buffer.buffer = NULL;
    return 0;
}
