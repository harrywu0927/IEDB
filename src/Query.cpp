#include <utils.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <stack>
#include <algorithm>
#include <tuple>
#include <sstream>
#include <errno.h>
#include <atomic>
using namespace std;
Template CurrentTemplate;
mutex curMutex; //防止多线程访问冲突
mutex memMutex;
mutex pathCodeMutex;

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
                    i--;
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

/**
 * @brief 根据指定数据类型的数值对查询结果排序或去重，此函数仅对内存地址-长度对操作
 * @param mallocedMemory        已在堆区分配的内存地址-长度-排序值偏移量-时间戳元组
 * @param params        查询参数
 * @param type        数据类型
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int sortResult(vector<tuple<char *, long, long, long>> &mallocedMemory, DB_QueryParams *params, DataType &type)
{
    switch (params->order)
    {
    case ASCEND: //升序
    {
        sort(mallocedMemory.begin(), mallocedMemory.end(),
             [&type](tuple<char *, long, long, long> iter1, tuple<char *, long, long, long> iter2) -> bool
             {
                 char value1[type.valueBytes], value2[type.valueBytes];
                 memcpy(value1, get<0>(iter1) + get<2>(iter1), type.valueBytes);
                 memcpy(value2, get<0>(iter2) + get<2>(iter2), type.valueBytes);
                 return DataType::CompareValueInBytes(type, value1, value2) < 0;
             });
        break;
    }

    case DESCEND: //降序
    {
        sort(mallocedMemory.begin(), mallocedMemory.end(),
             [&type](tuple<char *, long, long, long> iter1, tuple<char *, long, long, long> iter2) -> bool
             {
                 char value1[type.valueBytes], value2[type.valueBytes];
                 memcpy(value1, get<0>(iter1) + get<2>(iter1), type.valueBytes);
                 memcpy(value2, get<0>(iter2) + get<2>(iter2), type.valueBytes);
                 return DataType::CompareValueInBytes(type, value1, value2) > 0;
             });
        break;
    }
    case DISTINCT: //去除重复
    {
        vector<pair<char *, int>> existedValues;
        for (int i = 0; i < mallocedMemory.size(); i++)
        {
            char value[type.valueBytes];
            memcpy(value, get<0>(mallocedMemory[i]) + get<2>(mallocedMemory[i]), type.valueBytes);
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
                    free(get<0>(mallocedMemory[i]));                  //重复值，释放此内存
                    mallocedMemory.erase(mallocedMemory.begin() + i); //注：此操作在查询量大时效率可能很低
                    isRepeat = true;
                    i--;
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
    case TIME_ASC:
    {
        sort(mallocedMemory.begin(), mallocedMemory.end(),
             [](tuple<char *, long, long, long> iter1, tuple<char *, long, long, long> iter2) -> bool
             {
                 return get<3>(iter1) < get<3>(iter2);
             });
        break;
    }
    case TIME_DSC:
    {
        sort(mallocedMemory.begin(), mallocedMemory.end(),
             [](tuple<char *, long, long, long> iter1, tuple<char *, long, long, long> iter2) -> bool
             {
                 return get<3>(iter1) > get<3>(iter2);
             });
        break;
    }
    default:
        break;
    }

    return 0;
}

//对文件根据时间升序或降序排序
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
 * @note    deprecated
 */
int DB_QueryWholeFile_Old(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;

    vector<pair<string, long>> filesWithTime, selectedFiles, selectedPacks;

    //获取每个数据文件，并带有时间戳
    readDataFilesWithTimestamps(params->pathToLine, filesWithTime);

    //根据主查询方式选择不同的方案
    switch (params->queryType)
    {
    case TIMESPAN: //根据时间段，附加辅助查询条件筛选
    {
        vector<string> packFiles;
        //筛选落入时间区间内的文件
        for (auto &file : filesWithTime)
        {
            if (file.second >= params->start && file.second <= params->end)
            {
                selectedFiles.push_back(make_pair(file.first, file.second));
            }
        }
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &file : packFiles)
        {
            string tmp = file;
            while (tmp.back() == '/')
                tmp.pop_back();
            vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
            string packName = vec[vec.size() - 1];
            vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
            if (timespan.size() > 0)
            {
                long start = atol(timespan[0].c_str());
                long end = atol(timespan[1].c_str());
                if ((start < params->start && end >= params->start) || (start < params->end && end >= params->start) || (start <= params->start && end >= params->end) || (start >= params->start && end <= params->end)) //落入或部分落入时间区间
                {
                    selectedPacks.push_back(make_pair(file, start));
                }
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
        sortByTime(selectedFiles, TIME_ASC);
        sortByTime(selectedPacks, TIME_ASC);

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
                int err = params->byPath == 1 ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, bytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
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
                    char *memory = new char[len]; //一次分配整个文件长度的内存
                    memcpy(memory, buff, len);
                    cur += len;
                    mallocedMemory.push_back(make_pair(memory, len));
                }
            }

            for (auto &pack : selectedPacks)
            {
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                TemplateManager::CheckTemplate(templateName);

                for (int i = 0; i < fileNum; i++)
                {
                    long timestamp;
                    int readLength, zipType;
                    long dataPos = packReader.Next(readLength, timestamp, zipType);
                    if (timestamp < params->start || timestamp > params->end) //在时间区间外
                        continue;
                    char *buff = new char[CurrentTemplate.totalBytes];
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
                        ReZipBuff(buff, readLength, params->pathToLine);
                        break;
                    }
                    default:
                        delete[] buff;
                        continue;
                        break;
                    }
                    //获取数据的偏移量和数据类型
                    long pos = 0, bytes = 0;
                    DataType type;
                    int err = params->byPath == 1 ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, bytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
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
                        char *memory = new char[readLength];
                        memcpy(memory, buff, readLength);
                        cur += readLength;
                        mallocedMemory.push_back(make_pair(memory, readLength));
                    }
                }
            }
        }
        else //不需要比较数据，直接拷贝
        {
            for (auto &file : selectedFiles)
            {
                long len; //文件长度
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char buff[len]; //文件内容缓存
                DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

                char *memory = new char[len]; //一次分配整个文件长度的内存
                memcpy(memory, buff, len);
                cur += len;
                mallocedMemory.push_back(make_pair(memory, len));
            }

            for (auto &pack : selectedPacks)
            {
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);

                for (int i = 0; i < fileNum; i++)
                {
                    long timestamp;
                    int readLength, zipType;
                    long dataPos = packReader.Next(readLength, timestamp, zipType);
                    if (timestamp < params->start || timestamp > params->end) //在时间区间外
                        continue;
                    char *buff = new char[CurrentTemplate.totalBytes];
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
                        ReZipBuff(buff, readLength, params->pathToLine);
                        break;
                    }
                    default:
                        delete[] buff;
                        continue;
                        break;
                    }
                    char *memory = new char[readLength];
                    memcpy(memory, buff, readLength);
                    cur += readLength;
                    mallocedMemory.push_back(make_pair(memory, readLength));
                }
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
                memcpy(data + cur, mem.first, mem.second);
                delete[] mem.first;
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
        sortByTime(filesWithTime, TIME_DSC);
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
                int err = params->byPath == 1 ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, bytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
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
                    char *memory = new char[len]; //一次分配整个文件长度的内存
                    memcpy(memory, buff, len);
                    cur += len;
                    mallocedMemory.push_back(make_pair(memory, len));
                    selectedNum++;
                }
                if (selectedNum == params->queryNums)
                    break;
            }
            if (selectedNum < params->queryNums)
            {
                //检索到的数量不够，继续从打包文件中获取
                vector<string> packList;
                vector<pair<string, long>> packsWithTime;
                readPakFilesList(params->pathToLine, packList);
                for (auto &pack : packList)
                {
                    string tmp = pack;
                    while (tmp.back() == '/')
                        tmp.pop_back();
                    vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
                    string packName = vec[vec.size() - 1];
                    vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
                    if (timespan.size() > 0)
                    {
                        long start = atol(timespan[0].c_str());
                        packsWithTime.push_back(make_pair(pack, start));
                    }
                    else
                    {
                        return StatusCode::FILENAME_MODIFIED;
                    }
                }
                for (auto &pack : packsWithTime)
                {
                    if (selectedNum == params->queryNums)
                        break;
                    PackFileReader packReader(pack.first);
                    if (packReader.packBuffer == NULL)
                        continue;
                    int fileNum;
                    string templateName;
                    packReader.ReadPackHead(fileNum, templateName);
                    if (TemplateManager::CheckTemplate(templateName) != 0)
                        continue;
                    //由于pak中的文件按时间升序存放，首先依次将此包中文件信息压入栈中，弹出时即为时间降序型

                    stack<pair<long, tuple<int, long, int>>> filestk;
                    for (int i = 0; i < fileNum; i++)
                    {
                        long timestamp; //暂时用不到时间戳
                        int readLength, zipType;
                        long dataPos = packReader.Next(readLength, timestamp, zipType);
                        auto t = make_tuple(readLength, timestamp, zipType);
                        filestk.push(make_pair(dataPos, t));
                    }
                    while (!filestk.empty())
                    {
                        auto fileInfo = filestk.top();
                        filestk.pop();
                        long dataPos = fileInfo.first;
                        int readLength = get<0>(fileInfo.second);
                        long timestamp = get<1>(fileInfo.second);
                        int zipType = get<2>(fileInfo.second);

                        char *buff = new char[CurrentTemplate.totalBytes];
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
                            ReZipBuff(buff, readLength, params->pathToLine);
                            break;
                        }
                        default:
                        {
                            delete[] buff;
                            return StatusCode::UNKNWON_DATAFILE;
                            break;
                        }
                        }
                        //获取数据的偏移量和字节数
                        long bytes = 0, pos = 0;
                        DataType type;
                        int err = params->byPath == 1 ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, bytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
                        if (err != 0)
                        {
                            buffer->buffer = NULL;
                            buffer->bufferMalloced = 0;
                            return err;
                        }
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
                            char *memory = new char[readLength];
                            memcpy(memory, buff, readLength);
                            cur += readLength;
                            mallocedMemory.push_back(make_pair(memory, readLength));
                            selectedNum++;
                        }
                        delete[] buff;
                        if (selectedNum == params->queryNums)
                            break;
                    }
                }
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
                    delete[] mem.first;
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
            int selected = 0;
            for (int i = 0; i < params->queryNums && i < filesWithTime.size(); i++)
            {
                long len;
                DB_GetFileLengthByPath(const_cast<char *>(filesWithTime[i].first.c_str()), &len);
                char buff[len];
                DB_OpenAndRead(const_cast<char *>(filesWithTime[i].first.c_str()), buff);
                char *memory = new char[len];
                memcpy(memory, buff, len);
                mallocedMemory.push_back(make_pair(memory, len));
                cur += len;
                selected++;
            }
            if (selected < params->queryNums)
            {
                vector<string> packFiles;
                //检索到的数量不够，继续从打包文件中获取
                readPakFilesList(params->pathToLine, packFiles);
                vector<pair<string, long>> packsWithTime;
                for (auto &pack : packFiles)
                {
                    string tmp = pack;
                    while (tmp.back() == '/')
                        tmp.pop_back();
                    vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
                    string packName = vec[vec.size() - 1];
                    vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
                    if (timespan.size() > 0)
                    {
                        long start = atol(timespan[0].c_str());
                        packsWithTime.push_back(make_pair(pack, start));
                    }
                    else
                    {
                        return StatusCode::FILENAME_MODIFIED;
                    }
                }
                sortByTime(packsWithTime, TIME_DSC);

                for (auto &pack : packsWithTime)
                {
                    if (selected == params->queryNums)
                        break;
                    PackFileReader packReader(pack.first);
                    if (packReader.packBuffer == NULL)
                        continue;
                    int fileNum;
                    string templateName;
                    packReader.ReadPackHead(fileNum, templateName);
                    TemplateManager::CheckTemplate(templateName);
                    //由于pak中的文件按时间升序存放，首先依次将此包中文件信息压入栈中，弹出时即为时间降序型

                    stack<pair<long, tuple<int, long, int>>> filestk;
                    for (int i = 0; i < fileNum; i++)
                    {
                        long timestamp; //暂时用不到时间戳
                        int readLength, zipType;
                        long dataPos = packReader.Next(readLength, timestamp, zipType);
                        auto t = make_tuple(readLength, timestamp, zipType);
                        filestk.push(make_pair(dataPos, t));
                    }
                    while (!filestk.empty())
                    {
                        auto fileInfo = filestk.top();
                        filestk.pop();
                        long dataPos = fileInfo.first;
                        int readLength = get<0>(fileInfo.second);
                        long timestamp = get<1>(fileInfo.second);
                        int zipType = get<2>(fileInfo.second);

                        char *buff = new char[CurrentTemplate.totalBytes];
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
                            ReZipBuff(buff, readLength, params->pathToLine);
                            break;
                        }
                        default:
                        {
                            delete[] buff;
                            return StatusCode::UNKNWON_DATAFILE;
                            break;
                        }
                        }
                        char *memory = new char[readLength];
                        memcpy(memory, buff, readLength);
                        cur += readLength;
                        mallocedMemory.push_back(make_pair(memory, readLength));
                        selected++;

                        delete[] buff;
                        if (selected == params->queryNums)
                            break;
                    }
                }
            }
            if (cur != 0)
            {
                char *data = (char *)malloc(cur);
                cur = 0;
                for (auto &mem : mallocedMemory)
                {
                    memcpy(data + cur, mem.first, mem.second);
                    delete[] mem.first;
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
            }
        }
        vector<string> packList;
        readPakFilesList(params->pathToLine, packList);
        for (auto &pack : packList)
        {
            PackFileReader packReader(pack);
            if (packReader.packBuffer == NULL)
                continue;
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            TemplateManager::CheckTemplate(templateName);
            for (int i = 0; i < fileNum; i++)
            {
                string fileID;
                int readLength, zipType;
                long dataPos = packReader.Next(readLength, fileID, zipType);
                string fid = params->fileID;
                if (fileID == fid)
                {
                    char *buff = new char[CurrentTemplate.totalBytes];
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
                        ReZipBuff(buff, readLength, params->pathToLine);
                        break;
                    }
                    default:
                        delete[] buff;
                        continue;
                        break;
                    }
                    char *data = (char *)malloc(readLength);
                    if (data == NULL)
                    {
                        buffer->buffer = NULL;
                        buffer->bufferMalloced = 0;
                        return StatusCode::BUFFER_FULL;
                    }
                    //内存分配成功，传入数据
                    buffer->bufferMalloced = 1;
                    buffer->length = readLength;
                    memcpy(data, buff, readLength);
                    buffer->buffer = data;
                    return 0;
                }
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
 * @brief 根据给定的查询条件在某一产线文件夹下的数据文件中获取符合条件的整个文件的数据，可比较数据大小筛选，可按照时间
 *          将结果升序或降序排序，数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note 单线程
 */
int DB_QueryWholeFile(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    IOBusy = true;
    int check = CheckQueryParams(params);
    if (check != 0)
    {
        IOBusy = false;
        return check;
    }

    vector<pair<string, long>> filesWithTime, selectedFiles;

    //获取每个数据文件，并带有时间戳
    readDataFilesWithTimestamps(params->pathToLine, filesWithTime);

    //根据主查询方式选择不同的方案
    switch (params->queryType)
    {
    case TIMESPAN: //根据时间段，附加辅助查询条件筛选
    {
        vector<string> packFiles;
        //筛选落入时间区间内的文件
        for (auto &file : filesWithTime)
        {
            if (file.second >= params->start && file.second <= params->end)
            {
                selectedFiles.push_back(make_pair(file.first, file.second));
            }
        }
        auto selectedPacks = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
        //确认当前模版
        if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
        {
            IOBusy = false;
            return StatusCode::SCHEMA_FILE_NOT_FOUND;
        }

        //根据时间升序或降序排序
        sortByTime(selectedFiles, TIME_ASC);

        //比较指定变量给定的数据值，筛选符合条件的值
        vector<pair<char *, long>> mallocedMemory; //已在堆区分配的进入筛选范围数据的内存地址和长度集
        long cur = 0;                              //记录已选中的文件总长度
        /*<-----!!!警惕内存泄露!!!----->*/
        if (params->compareType != DB_CompareType::CMP_NONE || params->order != ODR_NONE)
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
                int err = params->byPath == 1 ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, bytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
                if (err != 0)
                {
                    buffer->buffer = NULL;
                    buffer->bufferMalloced = 0;
                    continue;
                }

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
                    char *memory = new char[len]; //一次分配整个文件长度的内存
                    memcpy(memory, buff, len);
                    cur += len;
                    mallocedMemory.push_back(make_pair(memory, len));
                }
            }

            for (auto &pack : selectedPacks)
            {
                auto pk = packManager.GetPack(pack.first);
                PackFileReader packReader(pk.first, pk.second);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
                    continue;

                for (int i = 0; i < fileNum; i++)
                {
                    long timestamp;
                    int readLength, zipType;
                    long dataPos = packReader.Next(readLength, timestamp, zipType);
                    if (timestamp < params->start || timestamp > params->end) //在时间区间外
                        continue;
                    char *buff = new char[CurrentTemplate.totalBytes];
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
                        ReZipBuff(buff, readLength, params->pathToLine);
                        break;
                    }
                    default:
                        delete[] buff;
                        continue;
                        break;
                    }
                    //获取数据的偏移量和数据类型
                    long pos = 0, bytes = 0;
                    DataType type;
                    int err = params->byPath == 1 ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, bytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
                    if (err != 0)
                    {
                        buffer->buffer = NULL;
                        buffer->bufferMalloced = 0;
                        IOBusy = false;
                        return err;
                    }

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
                        char *memory = new char[readLength];
                        memcpy(memory, buff, readLength);
                        cur += readLength;
                        mallocedMemory.push_back(make_pair(memory, readLength));
                    }
                    delete[] buff;
                }
            }
        }
        else //不需要比较数据，直接拷贝
        {
            for (auto &file : selectedFiles)
            {
                long len; //文件长度
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char buff[len]; //文件内容缓存
                DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

                char *memory = new char[len]; //一次分配整个文件长度的内存
                memcpy(memory, buff, len);
                cur += len;
                mallocedMemory.push_back(make_pair(memory, len));
            }

            for (auto &pack : selectedPacks)
            {
                auto pk = packManager.GetPack(pack.first);
                PackFileReader packReader(pk.first, pk.second);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
                    continue;
                for (int i = 0; i < fileNum; i++)
                {
                    long timestamp;
                    int readLength, zipType;
                    long dataPos = packReader.Next(readLength, timestamp, zipType);
                    if (timestamp < params->start || timestamp > params->end) //在时间区间外
                        continue;
                    char *buff = new char[CurrentTemplate.totalBytes];
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
                        ReZipBuff(buff, readLength, params->pathToLine);
                        break;
                    }
                    default:
                        delete[] buff;
                        continue;
                        break;
                    }
                    char *memory = new char[readLength];
                    memcpy(memory, buff, readLength);
                    cur += readLength;
                    mallocedMemory.push_back(make_pair(memory, readLength));
                }
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
                IOBusy = false;
                return StatusCode::BUFFER_FULL;
            }
            //拷贝数据
            cur = 0;
            for (auto &mem : mallocedMemory)
            {
                memcpy(data + cur, mem.first, mem.second);
                delete[] mem.first;
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
        if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
        {
            IOBusy = false;
            return StatusCode::SCHEMA_FILE_NOT_FOUND;
        }

        //根据时间降序排序
        sortByTime(filesWithTime, TIME_DSC);
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
                int err = params->byPath == 1 ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, bytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
                if (err != 0)
                {
                    buffer->buffer = NULL;
                    buffer->bufferMalloced = 0;
                    continue;
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
                    char *memory = new char[len]; //一次分配整个文件长度的内存
                    memcpy(memory, buff, len);
                    cur += len;
                    mallocedMemory.push_back(make_pair(memory, len));
                    selectedNum++;
                }
                if (selectedNum == params->queryNums)
                    break;
            }
            if (selectedNum < params->queryNums)
            {
                //检索到的数量不够，继续从打包文件中获取
                int packNums = packManager.allPacks[params->pathToLine].size();
                for (int index = 1; selectedNum < params->queryNums && index <= packNums; index++)
                {
                    auto pack = packManager.GetLastPack(params->pathToLine, index);
                    PackFileReader packReader(pack.second.first, pack.second.second);
                    if (packReader.packBuffer == NULL)
                        continue;
                    int fileNum;
                    string templateName;
                    packReader.ReadPackHead(fileNum, templateName);
                    if (TemplateManager::CheckTemplate(templateName) != 0)
                        continue;
                    //由于pak中的文件按时间升序存放，首先依次将此包中文件信息压入栈中，弹出时即为时间降序型

                    stack<pair<long, tuple<int, long, int>>> filestk;
                    for (int i = 0; i < fileNum; i++)
                    {
                        long timestamp; //暂时用不到时间戳
                        int readLength, zipType;
                        long dataPos = packReader.Next(readLength, timestamp, zipType);
                        auto t = make_tuple(readLength, timestamp, zipType);
                        filestk.push(make_pair(dataPos, t));
                    }
                    while (!filestk.empty())
                    {
                        auto fileInfo = filestk.top();
                        filestk.pop();
                        long dataPos = fileInfo.first;
                        int readLength = get<0>(fileInfo.second);
                        long timestamp = get<1>(fileInfo.second);
                        int zipType = get<2>(fileInfo.second);

                        char *buff = new char[CurrentTemplate.totalBytes];
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
                            ReZipBuff(buff, readLength, params->pathToLine);
                            break;
                        }
                        default:
                        {
                            delete[] buff;
                            continue;
                            break;
                        }
                        }
                        //获取数据的偏移量和字节数
                        long bytes = 0, pos = 0;
                        DataType type;
                        int err = params->byPath == 1 ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, bytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
                        if (err != 0)
                        {
                            buffer->buffer = NULL;
                            buffer->bufferMalloced = 0;
                            IOBusy = false;
                            return err;
                        }
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
                            char *memory = new char[readLength];
                            memcpy(memory, buff, readLength);
                            cur += readLength;
                            mallocedMemory.push_back(make_pair(memory, readLength));
                            selectedNum++;
                        }
                        delete[] buff;
                        if (selectedNum == params->queryNums)
                            break;
                    }
                }
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
                    IOBusy = false;
                    return StatusCode::BUFFER_FULL;
                }
                //拷贝数据
                cur = 0;
                for (auto &mem : mallocedMemory)
                {
                    memcpy(data + cur, mem.first, mem.second);
                    delete[] mem.first;
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
            int selected = 0;
            for (int i = 0; i < params->queryNums && i < filesWithTime.size(); i++)
            {
                long len;
                DB_GetFileLengthByPath(const_cast<char *>(filesWithTime[i].first.c_str()), &len);
                char buff[len];
                DB_OpenAndRead(const_cast<char *>(filesWithTime[i].first.c_str()), buff);
                char *memory = new char[len];
                memcpy(memory, buff, len);
                mallocedMemory.push_back(make_pair(memory, len));
                cur += len;
                selected++;
            }
            if (selected < params->queryNums)
            {
                //检索到的数量不够，继续从打包文件中获取
                int packNums = packManager.allPacks[params->pathToLine].size();
                for (int index = 1; selected < params->queryNums && index <= packNums; index++)
                {
                    auto pack = packManager.GetLastPack(params->pathToLine, index);
                    PackFileReader packReader(pack.second.first, pack.second.second);
                    if (packReader.packBuffer == NULL)
                        continue;
                    int fileNum;
                    string templateName;
                    packReader.ReadPackHead(fileNum, templateName);
                    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
                        continue;
                    //由于pak中的文件按时间升序存放，首先依次将此包中文件信息压入栈中，弹出时即为时间降序型

                    stack<pair<long, tuple<int, long, int>>> filestk;
                    for (int i = 0; i < fileNum; i++)
                    {
                        long timestamp; //暂时用不到时间戳
                        int readLength, zipType;
                        long dataPos = packReader.Next(readLength, timestamp, zipType);
                        auto t = make_tuple(readLength, timestamp, zipType);
                        filestk.push(make_pair(dataPos, t));
                    }
                    while (!filestk.empty())
                    {
                        auto fileInfo = filestk.top();
                        filestk.pop();
                        long dataPos = fileInfo.first;
                        int readLength = get<0>(fileInfo.second);
                        long timestamp = get<1>(fileInfo.second);
                        int zipType = get<2>(fileInfo.second);

                        char *buff = new char[CurrentTemplate.totalBytes];
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
                            ReZipBuff(buff, readLength, params->pathToLine);
                            break;
                        }
                        default:
                        {
                            delete[] buff;
                            return StatusCode::UNKNWON_DATAFILE;
                            break;
                        }
                        }
                        char *memory = new char[readLength];
                        memcpy(memory, buff, readLength);
                        cur += readLength;
                        mallocedMemory.push_back(make_pair(memory, readLength));
                        selected++;

                        delete[] buff;
                        if (selected == params->queryNums)
                            break;
                    }
                }
            }
            if (cur != 0)
            {
                char *data = (char *)malloc(cur);
                cur = 0;
                for (auto &mem : mallocedMemory)
                {
                    memcpy(data + cur, mem.first, mem.second);
                    delete[] mem.first;
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
        string pathToLine = params->pathToLine;
        string fileid = params->fileID;
        while (pathToLine[pathToLine.length() - 1] == '/')
        {
            pathToLine.pop_back();
        }
        //对FILEID预处理
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
                IOBusy = false;
                return 0;
            }
        }

        auto pack = packManager.GetPackByID(params->pathToLine, fileid);
        if (pack.first != NULL && pack.second != 0)
        {
            PackFileReader packReader(pack.first, pack.second);
            if (packReader.packBuffer == NULL)
                return StatusCode::NO_DATA_QUERIED;
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
                return StatusCode::SCHEMA_FILE_NOT_FOUND;
            for (int i = 0; i < fileNum; i++)
            {
                string fileID;
                int readLength, zipType;
                long dataPos = packReader.Next(readLength, fileID, zipType);
                string fid = params->fileID;
                if (fileID == fid)
                {
                    char *buff = new char[CurrentTemplate.totalBytes];
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
                        ReZipBuff(buff, readLength, params->pathToLine);
                        break;
                    }
                    default:
                        delete[] buff;
                        continue;
                        break;
                    }
                    char *data = (char *)malloc(readLength);
                    if (data == NULL)
                    {
                        buffer->buffer = NULL;
                        buffer->bufferMalloced = 0;
                        return StatusCode::BUFFER_FULL;
                    }
                    //内存分配成功，传入数据
                    buffer->bufferMalloced = 1;
                    buffer->length = readLength;
                    memcpy(data, buff, readLength);
                    buffer->buffer = data;
                    return 0;
                }
            }
        }
        IOBusy = false;
        break;
    }

    default:
        return StatusCode::NO_QUERY_TYPE;
        break;
    }
    IOBusy = false;
    return 0;
}

/**
 * @brief 线程任务，处理单个包
 *
 * @param pack 包的内存地址-长度对
 * @param params 查询参数
 * @param cur 当前已分配的内存总长度，临界资源，需要加线程互斥锁
 * @param mallocedMemory 已分配的内存地址-长度-排序值偏移量-时间戳元组，临界资源，需要加线程互斥锁
 * @param type  排序值的数据类型
 * @return int
 */
int PackProcess_WholeFile(pair<char *, long> pack, DB_QueryParams *params, long *cur, vector<tuple<char *, long, long, long>> *mallocedMemory)
{
    PackFileReader packReader(pack.first, pack.second);
    if (packReader.packBuffer == NULL)
        return StatusCode::DATAFILE_NOT_FOUND;
    int fileNum;
    string templateName;
    packReader.ReadPackHead(fileNum, templateName);
    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
        return StatusCode::SCHEMA_FILE_NOT_FOUND;

    for (int i = 0; i < fileNum; i++)
    {
        long timestamp;
        int readLength, zipType;
        long dataPos = packReader.Next(readLength, timestamp, zipType);
        if (timestamp < params->start || timestamp > params->end) //在时间区间外
            continue;
        char *buff = new char[CurrentTemplate.totalBytes];
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
            ReZipBuff(buff, readLength, params->pathToLine);
            break;
        }
        default:
            delete[] buff;
            continue;
            break;
        }
        //获取数据的偏移量和数据类型
        long pos = 0, bytes = 0;
        DataType type;
        int err = params->byPath == 1 ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, bytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
        if (err != 0)
        {
            cout << err << endl;
            return err;
        }

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
            canCopy = true;
            break;
        }
        if (canCopy) //需要此数据
        {
            char *memory = new (std::nothrow) char[readLength];
            if (memory == nullptr)
            {
                cout << "memory null" << endl;
            }
            else
            {
                memcpy(memory, buff, readLength);
                curMutex.lock();
                *cur += readLength;
                curMutex.unlock();
                memMutex.lock();
                mallocedMemory->push_back(make_tuple(memory, readLength, pos, timestamp));
                memMutex.unlock();
            }
        }
        delete[] buff;
    }
    return 0;
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
int DB_QueryWholeFile_MultiThread(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;

    vector<pair<string, long>> filesWithTime, selectedFiles;

    //根据主查询方式选择不同的方案
    switch (params->queryType)
    {
    case TIMESPAN: //根据时间段，附加辅助查询条件筛选
    {
        //获取每个数据文件，并带有时间戳
        readDataFilesWithTimestamps(params->pathToLine, filesWithTime);
        vector<string> packFiles;
        //筛选落入时间区间内的文件
        for (auto &file : filesWithTime)
        {
            if (file.second >= params->start && file.second <= params->end)
            {
                selectedFiles.push_back(make_pair(file.first, file.second));
            }
        }
        auto selectedPacks = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
        //确认当前模版
        if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
            return StatusCode::SCHEMA_FILE_NOT_FOUND;

        //根据时间升序或降序排序
        // sortByTime(selectedFiles, TIME_ASC);

        //比较指定变量给定的数据值，筛选符合条件的值
        vector<tuple<char *, long, long, long>> mallocedMemory; //已在堆区分配的进入筛选范围数据的内存地址-长度-排序值偏移量-时间戳元组集
        long cur = 0;                                           //记录已选中的文件总长度
        DataType type;
        /*<-----!!!警惕内存泄露!!!----->*/
        if (params->compareType != DB_CompareType::CMP_NONE || params->order != ODR_NONE)
        {

            for (auto &file : selectedFiles)
            {
                long len; //文件长度
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char buff[len]; //文件内容缓存
                DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

                //获取数据的偏移量和数据类型
                long pos = 0, bytes = 0;

                int err = params->byPath == 1 ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, bytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
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
                    canCopy = true;
                    break;
                }
                if (canCopy) //需要此数据
                {
                    char *memory = new char[len]; //一次分配整个文件长度的内存
                    memcpy(memory, buff, len);
                    cur += len;
                    mallocedMemory.push_back(make_tuple(memory, len, pos, file.second));
                }
            }
        }
        else //不需要比较数据，直接拷贝
        {
            for (auto &file : selectedFiles)
            {
                long len; //文件长度
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char buff[len]; //文件内容缓存
                DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);

                char *memory = new char[len]; //一次分配整个文件长度的内存
                memcpy(memory, buff, len);
                cur += len;
                mallocedMemory.push_back(make_tuple(memory, len, 0, file.second));
            }
        }
        int index = 0;
        future_status status[maxThreads - 1];
        future<int> f[maxThreads - 1];
        for (int j = 0; j < maxThreads - 1; j++)
        {
            status[j] = future_status::ready;
        }
        do
        {
            for (int j = 0; j < maxThreads - 1; j++) //留一个线程循环遍历线程集，确认每个线程的运行状态
            {
                if (status[j] == future_status::ready)
                {
                    auto pk = packManager.GetPack(selectedPacks[index].first);
                    f[j] = async(std::launch::async, PackProcess_WholeFile, pk, params, &cur, &mallocedMemory);
                    status[j] = f[j].wait_for(chrono::milliseconds(2));
                    index++;
                    if (index == selectedPacks.size())
                        break;
                }
                else
                {
                    status[j] = f[j].wait_for(chrono::milliseconds(2));
                }
            }
        } while (index < selectedPacks.size());
        for (int j = 0; j < maxThreads - 1; j++)
        {
            if (status[j] != future_status::ready)
                f[j].wait();
        }
        sortResult(mallocedMemory, params, type);
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
                memcpy(data + cur, get<0>(mem), get<1>(mem));
                delete[] get<0>(mem);
                cur += get<1>(mem);
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
        if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
            return StatusCode::SCHEMA_FILE_NOT_FOUND;

        //根据时间降序排序
        sortByTime(filesWithTime, TIME_DSC);
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
                int err = params->byPath == 1 ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, bytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
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
                    char *memory = new char[len]; //一次分配整个文件长度的内存
                    memcpy(memory, buff, len);
                    cur += len;
                    mallocedMemory.push_back(make_pair(memory, len));
                    selectedNum++;
                }
                if (selectedNum == params->queryNums)
                    break;
            }
            if (selectedNum < params->queryNums)
            {
                //检索到的数量不够，继续从打包文件中获取
                int packNums = packManager.allPacks[params->pathToLine].size();
                for (int index = 1; selectedNum < params->queryNums && index <= packNums; index++)
                {
                    auto pack = packManager.GetLastPack(params->pathToLine, index);
                    PackFileReader packReader(pack.second.first, pack.second.second);
                    if (packReader.packBuffer == NULL)
                        continue;
                    int fileNum;
                    string templateName;
                    packReader.ReadPackHead(fileNum, templateName);
                    if (TemplateManager::CheckTemplate(templateName) != 0)
                        continue;
                    //由于pak中的文件按时间升序存放，首先依次将此包中文件信息压入栈中，弹出时即为时间降序型

                    stack<pair<long, tuple<int, long, int>>> filestk;
                    for (int i = 0; i < fileNum; i++)
                    {
                        long timestamp; //暂时用不到时间戳
                        int readLength, zipType;
                        long dataPos = packReader.Next(readLength, timestamp, zipType);
                        auto t = make_tuple(readLength, timestamp, zipType);
                        filestk.push(make_pair(dataPos, t));
                    }
                    while (!filestk.empty())
                    {
                        auto fileInfo = filestk.top();
                        filestk.pop();
                        long dataPos = fileInfo.first;
                        int readLength = get<0>(fileInfo.second);
                        long timestamp = get<1>(fileInfo.second);
                        int zipType = get<2>(fileInfo.second);

                        char *buff = new char[CurrentTemplate.totalBytes];
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
                            ReZipBuff(buff, readLength, params->pathToLine);
                            break;
                        }
                        default:
                        {
                            delete[] buff;
                            return StatusCode::UNKNWON_DATAFILE;
                            break;
                        }
                        }
                        //获取数据的偏移量和字节数
                        long bytes = 0, pos = 0;
                        DataType type;
                        int err = params->byPath == 1 ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, bytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
                        if (err != 0)
                        {
                            buffer->buffer = NULL;
                            buffer->bufferMalloced = 0;
                            return err;
                        }
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
                            char *memory = new char[readLength];
                            memcpy(memory, buff, readLength);
                            cur += readLength;
                            mallocedMemory.push_back(make_pair(memory, readLength));
                            selectedNum++;
                        }
                        delete[] buff;
                        if (selectedNum == params->queryNums)
                            break;
                    }
                }
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
                    delete[] mem.first;
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
            int selected = 0;
            for (int i = 0; i < params->queryNums && i < filesWithTime.size(); i++)
            {
                long len;
                DB_GetFileLengthByPath(const_cast<char *>(filesWithTime[i].first.c_str()), &len);
                char buff[len];
                DB_OpenAndRead(const_cast<char *>(filesWithTime[i].first.c_str()), buff);
                char *memory = new char[len];
                memcpy(memory, buff, len);
                mallocedMemory.push_back(make_pair(memory, len));
                cur += len;
                selected++;
            }
            if (selected < params->queryNums)
            {
                //检索到的数量不够，继续从打包文件中获取
                int packNums = packManager.allPacks[params->pathToLine].size();
                for (int index = 1; selected < params->queryNums && index <= packNums; index++)
                {
                    auto pack = packManager.GetLastPack(params->pathToLine, index);
                    PackFileReader packReader(pack.second.first, pack.second.second);
                    if (packReader.packBuffer == NULL)
                        continue;
                    int fileNum;
                    string templateName;
                    packReader.ReadPackHead(fileNum, templateName);
                    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
                        continue;
                    //由于pak中的文件按时间升序存放，首先依次将此包中文件信息压入栈中，弹出时即为时间降序型

                    stack<pair<long, tuple<int, long, int>>> filestk;
                    for (int i = 0; i < fileNum; i++)
                    {
                        long timestamp; //暂时用不到时间戳
                        int readLength, zipType;
                        long dataPos = packReader.Next(readLength, timestamp, zipType);
                        auto t = make_tuple(readLength, timestamp, zipType);
                        filestk.push(make_pair(dataPos, t));
                    }
                    while (!filestk.empty())
                    {
                        auto fileInfo = filestk.top();
                        filestk.pop();
                        long dataPos = fileInfo.first;
                        int readLength = get<0>(fileInfo.second);
                        long timestamp = get<1>(fileInfo.second);
                        int zipType = get<2>(fileInfo.second);

                        char *buff = new char[CurrentTemplate.totalBytes];
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
                            ReZipBuff(buff, readLength, params->pathToLine);
                            break;
                        }
                        default:
                        {
                            delete[] buff;
                            return StatusCode::UNKNWON_DATAFILE;
                            break;
                        }
                        }
                        char *memory = new char[readLength];
                        memcpy(memory, buff, readLength);
                        cur += readLength;
                        mallocedMemory.push_back(make_pair(memory, readLength));
                        selected++;

                        delete[] buff;
                        if (selected == params->queryNums)
                            break;
                    }
                }
            }
            if (cur != 0)
            {
                char *data = (char *)malloc(cur);
                cur = 0;
                for (auto &mem : mallocedMemory)
                {
                    memcpy(data + cur, mem.first, mem.second);
                    delete[] mem.first;
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
        string pathToLine = params->pathToLine;
        string fileid = params->fileID;
        while (pathToLine[pathToLine.length() - 1] == '/')
        {
            pathToLine.pop_back();
        }
        //对FILEID预处理
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
            }
        }

        auto pack = packManager.GetPackByID(params->pathToLine, fileid);
        if (pack.first != NULL && pack.second != 0)
        {
            PackFileReader packReader(pack.first, pack.second);
            if (packReader.packBuffer == NULL)
                return StatusCode::NO_DATA_QUERIED;
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
                return StatusCode::SCHEMA_FILE_NOT_FOUND;
            for (int i = 0; i < fileNum; i++)
            {
                string fileID;
                int readLength, zipType;
                long dataPos = packReader.Next(readLength, fileID, zipType);
                string fid = params->fileID;
                if (fileID == fid)
                {
                    char *buff = new char[CurrentTemplate.totalBytes];
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
                        ReZipBuff(buff, readLength, params->pathToLine);
                        break;
                    }
                    default:
                        delete[] buff;
                        continue;
                        break;
                    }
                    char *data = (char *)malloc(readLength);
                    if (data == NULL)
                    {
                        buffer->buffer = NULL;
                        buffer->bufferMalloced = 0;
                        return StatusCode::BUFFER_FULL;
                    }
                    //内存分配成功，传入数据
                    buffer->bufferMalloced = 1;
                    buffer->length = readLength;
                    memcpy(data, buff, readLength);
                    buffer->buffer = data;
                    return 0;
                }
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
 * @note   支持idb文件和pak文件混合查询,此处默认pak文件中的时间均早于idb和idbzip文件！！
 *          不再维护
 */

int DB_QueryByTimespan_Old(DB_DataBuffer *buffer, DB_QueryParams *params)
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
        while (tmp.back() == '/')
            tmp.pop_back();
        vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
        string packName = vec[vec.size() - 1];
        vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
        if (timespan.size() > 0)
        {
            long start = atol(timespan[0].c_str());
            long end = atol(timespan[1].c_str());
            if ((start < params->start && end >= params->start) || (start < params->end && end >= params->start) || (start <= params->start && end >= params->end) || (start >= params->start && end <= params->end)) //落入或部分落入时间区间
            {
                selectedPacks.push_back(make_pair(file, start));
            }
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
        if (packReader.packBuffer == NULL)
            continue;
        int fileNum;
        string templateName;
        packReader.ReadPackHead(fileNum, templateName);
        TemplateManager::CheckTemplate(templateName);
        for (int i = 0; i < fileNum; i++)
        {
            typeList.clear();
            long timestamp;
            int readLength, zipType;
            long dataPos = packReader.Next(readLength, timestamp, zipType);
            if (timestamp < params->start || timestamp > params->end) //在时间区间外
                continue;
            char *buff = new char[CurrentTemplate.totalBytes];
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
                ReZipBuff(buff, readLength, params->pathToLine);
                break;
            }
            default:
                delete[] buff;
                continue;
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
                if (params->valueName != NULL) //此处，若编码为精确搜索，而又输入了不同的变量名，FindSortPosFromSelectedData将返回0
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
            delete[] buff;
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
        if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
            return StatusCode::SCHEMA_FILE_NOT_FOUND;
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

    if (sortDataPoses.size() > 0) //尚有问题
        sortResultByValue(mallocedMemory, sortDataPoses, params, type);
    if (cur == 0)
    {
        buffer->buffer = NULL;
        buffer->bufferMalloced = 0;
        return StatusCode::NO_DATA_QUERIED;
    }
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
 * @brief 根据给定的时间段和路径编码或变量名在某一产线文件夹下的数据文件中查询数据，可比较数据大小筛选，可按照时间
 *          将结果升序或降序排序，数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note   单线程
 */
int DB_QueryByTimespan_Single(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    IOBusy = true;
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;
    vector<pair<string, long>> filesWithTime, selectedFiles;
    auto selectedPacks = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
    //获取每个数据文件，并带有时间戳
    readDataFilesWithTimestamps(params->pathToLine, filesWithTime);

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }

    //确认当前模版
    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
    {
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    //根据时间升序排序
    sortByTime(selectedFiles, TIME_ASC);

    vector<pair<char *, long>> mallocedMemory; //当前已分配的内存地址-长度对列表
    long cur = 0;                              //已选择的数据总长
    DataType type;
    vector<DataType> typeList, selectedTypes;
    vector<long> sortDataPoses; //按值排序时要比较的数据的偏移量
    vector<PathCode> pathCodes;
    //先对时序在前的包文件检索
    for (auto &pack : selectedPacks)
    {
        auto pk = packManager.GetPack(pack.first);
        PackFileReader packReader(pk.first, pk.second);
        if (packReader.packBuffer == NULL)
            continue;
        int fileNum;
        string templateName;
        packReader.ReadPackHead(fileNum, templateName);
        if (TemplateManager::CheckTemplate(templateName) != 0)
            return StatusCode::SCHEMA_FILE_NOT_FOUND;
        if (params->byPath)
        {
            CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
        }
        for (int i = 0; i < fileNum; i++)
        {
            // typeList.clear();
            long timestamp;
            int readLength, zipType;
            long dataPos = packReader.Next(readLength, timestamp, zipType);
            if (timestamp < params->start || timestamp > params->end) //在时间区间外
                continue;
            char *buff = new char[CurrentTemplate.totalBytes];
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
                ReZipBuff(buff, readLength, params->pathToLine);
                break;
            }
            default:
                delete[] buff;
                continue;
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
                if (typeList.size() == 0)
                    err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
                else
                    err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList);
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
                IOBusy = false;
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
                if (params->valueName != NULL) //此处，若编码为精确搜索，而又输入了不同的变量名，FindSortPosFromSelectedData将返回0
                    sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, pathCodes, typeList);
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
                    if (compareRes >= 0 || compareRes == 1)
                    {
                        canCopy = true;
                    }
                    break;
                }
                case DB_CompareType::LE:
                {
                    if (compareRes <= 0 || compareRes == 1)
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
                sortDataPoses.push_back(sortPos);
                char *memory = new char[copyBytes];
                memcpy(memory, copyValue, copyBytes);
                cur += copyBytes;
                mallocedMemory.push_back(make_pair(memory, copyBytes));
            }
            delete[] buff;
        }
    }

    //对时序在后的普通文件检索
    for (auto &file : selectedFiles)
    {
        // typeList.clear();
        long len;

        if (file.first.find(".idbzip") != string::npos)
        {
            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
        }
        else if (file.first.find("idb") != string::npos)
        {
            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
        }
        char *buff = new char[CurrentTemplate.totalBytes];
        DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
        if (file.first.find(".idbzip") != string::npos)
        {
            ReZipBuff(buff, (int &)len, params->pathToLine);
        }
        if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
        {
            IOBusy = false;
            return StatusCode::SCHEMA_FILE_NOT_FOUND;
        }
        if (params->byPath)
            CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
        //获取数据的偏移量和数据类型
        long pos = 0, bytes = 0;
        vector<long> posList, bytesList;
        long copyBytes = 0;
        int err;
        if (params->byPath)
        {
            char *pathCode = params->pathCode;
            if (typeList.size() == 0)
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
            else
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList);
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
            IOBusy = false;
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
                sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, pathCodes, typeList);
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
                break;
            }
        }
        else //不可比较，直接拷贝此数据
            canCopy = true;
        if (canCopy) //需要此数据
        {
            sortDataPoses.push_back(sortPos);
            char *memory = new char[copyBytes];
            memcpy(memory, copyValue, copyBytes);
            cur += copyBytes;
            mallocedMemory.push_back(make_pair(memory, copyBytes));
        }
        delete[] buff;
    }

    if (sortDataPoses.size() > 0) //尚有问题
        sortResultByValue(mallocedMemory, sortDataPoses, params, type);
    if (cur == 0)
    {
        buffer->buffer = NULL;
        buffer->bufferMalloced = 0;
        IOBusy = false;
        return StatusCode::NO_DATA_QUERIED;
    }
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
        IOBusy = false;
        return StatusCode::BUFFER_FULL;
    }
    //拷贝数据
    cur = 0;
    if (params->order == TIME_DSC) //时间降序，从内存地址集中反向拷贝
    {
        for (int i = mallocedMemory.size() - 1; i >= 0; i--)
        {
            memcpy(data + cur + startPos, mallocedMemory[i].first, mallocedMemory[i].second);
            delete[] mallocedMemory[i].first;
            cur += mallocedMemory[i].second;
        }
    }
    else //否则按照默认顺序升序排列即可
    {
        for (auto &mem : mallocedMemory)
        {
            memcpy(data + cur + startPos, mem.first, mem.second);
            delete[] mem.first;
            cur += mem.second;
        }
    }

    buffer->bufferMalloced = 1;
    buffer->buffer = data;
    buffer->length = cur + startPos;
    IOBusy = false;
    return 0;
}

/**
 * @brief 线程任务，处理单个包
 *
 * @param pack 包的内存地址-长度对
 * @param params 查询参数
 * @param cur 当前已分配的内存总长度，临界资源，需要加线程互斥锁
 * @param mallocedMemory 已分配的内存地址-长度-排序值偏移量-时间戳元组，临界资源，需要加线程互斥锁
 * @param type  排序值的数据类型
 * @param typeList 选中的数据类型列表
 * @return int
 */
int PackProcess_NonAtomic(pair<char *, long> pack, DB_QueryParams *params, long *cur, vector<tuple<char *, long, long, long>> *mallocedMemory, DataType *type, vector<DataType> *typeList)
{
    PackFileReader packReader(pack.first, pack.second);
    if (packReader.packBuffer == NULL)
        return StatusCode::DATAFILE_NOT_FOUND;
    int fileNum;
    string templateName;
    packReader.ReadPackHead(fileNum, templateName);
    if (TemplateManager::CheckTemplate(templateName) != 0)
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    vector<PathCode> pathCodes;
    if (params->byPath)
    {
        CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
    }

    for (int i = 0; i < fileNum; i++)
    {
        long timestamp;
        int readLength, zipType;
        long dataPos = packReader.Next(readLength, timestamp, zipType);
        if (timestamp < params->start || timestamp > params->end) //在时间区间外
            continue;
        char *buff = new char[CurrentTemplate.totalBytes];
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
            ReZipBuff(buff, readLength, params->pathToLine);
            break;
        }
        default:
            delete[] buff;
            continue;
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
            if (typeList->size() == 0)
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, *typeList);
            else
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList);
            for (int i = 0; i < bytesList.size(); i++)
            {
                copyBytes += typeList->at(i).hasTime ? bytesList[i] + 8 : bytesList[i];
            }
        }
        else
        {
            err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, *type);
            copyBytes = type->hasTime ? bytes + 8 : bytes;
        }
        if (err != 0)
        {
            continue;
        }

        char copyValue[copyBytes]; //将要拷贝的数值
        long sortPos = 0;
        if (params->byPath)
        {
            long lineCur = 0; //记录此行当前写位置
            for (int i = 0; i < bytesList.size(); i++)
            {
                long curBytes = typeList->at(i).hasTime ? bytesList[i] + 8 : bytesList[i];
                memcpy(copyValue + lineCur, buff + posList[i], curBytes);
                lineCur += curBytes;
            }
            if (params->valueName != NULL) //此处，若编码为精确搜索，而又输入了不同的变量名，FindSortPosFromSelectedData将返回0
                sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, pathCodes, *typeList);
        }
        else
            memcpy(copyValue, buff + pos, copyBytes);

        int compareBytes = 0;
        if (params->valueName != NULL)
            compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, *type) == 0 ? bytes : 0;
        bool canCopy = false;
        if (params->compareType != CMP_NONE && compareBytes != 0) //可比较
        {
            char value[compareBytes]; //数值
            memcpy(value, buff + pos, compareBytes);

            //根据比较结果决定是否加入结果集
            int compareRes = DataType::CompareValue(*type, value, params->compareValue);
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
                break;
            }
        }
        else //不可比较，直接拷贝此数据
            canCopy = true;
        if (canCopy) //需要此数据
        {
            char *memory = new (std::nothrow) char[copyBytes];
            if (memory == nullptr)
            {
                cout << "memory null" << endl;
            }
            else
            {
                memcpy(memory, copyValue, copyBytes);
                curMutex.lock();
                *cur += copyBytes;
                curMutex.unlock();
                memMutex.lock();
                mallocedMemory->push_back(make_tuple(memory, copyBytes, sortPos, timestamp));
                memMutex.unlock();
            }
        }
        delete[] buff;
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
 * @note   支持idb文件和pak文件混合查询,此处默认pak文件中的时间均早于idb和idbzip文件！！
 */
int DB_QueryByTimespan_NonAtomic(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    IOBusy = true;
    if (maxThreads <= 2) //线程数量<=2时，多线程已无必要
    {
        return DB_QueryByTimespan_Single(buffer, params);
    }
    //确认当前模版
    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    if (params->byPath)
    {
        vector<PathCode> vec;
        CurrentTemplate.GetAllPathsByCode(params->pathCode, vec);
        if (vec.size() == 1)
            return DB_QueryByTimespan_Single(buffer, params);
    }
    int check = CheckQueryParams(params);
    if (check != 0)
    {
        IOBusy = false;
        return check;
    }
    vector<pair<string, long>> filesWithTime, selectedFiles;
    auto selectedPacks = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
    //获取每个数据文件，并带有时间戳
    readDataFilesWithTimestamps(params->pathToLine, filesWithTime);

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }

    vector<tuple<char *, long, long, long>> mallocedMemory; //当前已分配的内存地址-长度-排序值偏移-时间戳元组
    long cur = 0;                                           //已选择的数据总长
    DataType type;
    vector<DataType> typeList, selectedTypes;
    vector<long> sortDataPoses; //按值排序时要比较的数据的偏移量
    vector<PathCode> pathCodes;
    //先对时序在前的包文件检索
    if (selectedPacks.size() > 0)
    {
        int index = 0;
        future_status status[maxThreads - 1];
        future<int> f[maxThreads - 1];
        for (int j = 0; j < maxThreads - 1; j++)
        {
            status[j] = future_status::ready;
        }
        do
        {
            for (int j = 0; j < maxThreads - 1; j++) //留一个线程循环遍历线程集，确认每个线程的运行状态
            {
                if (status[j] == future_status::ready)
                {
                    auto pk = packManager.GetPack(selectedPacks[index].first);
                    f[j] = async(std::launch::async, PackProcess_NonAtomic, pk, params, &cur, &mallocedMemory, &type, &typeList);
                    status[j] = f[j].wait_for(chrono::milliseconds(1));
                    index++;
                    if (index == selectedPacks.size())
                        break;
                }
                else
                {
                    status[j] = f[j].wait_for(chrono::milliseconds(1));
                }
            }
        } while (index < selectedPacks.size());
        for (int j = 0; j < maxThreads - 1; j++)
        {
            if (status[j] != future_status::ready)
                f[j].wait();
        }
    }

    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
    {
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    if (params->byPath)
        CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
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
        char *buff = new char[CurrentTemplate.totalBytes];
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
            continue;
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
                sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, pathCodes, typeList);
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
            char *memory = new char[copyBytes];
            memcpy(memory, copyValue, copyBytes);
            cur += copyBytes;
            mallocedMemory.push_back(make_tuple(memory, copyBytes, sortPos, file.second));
        }
        delete[] buff;
    }

    sortResult(mallocedMemory, params, type);
    if (cur == 0)
    {
        buffer->buffer = NULL;
        buffer->bufferMalloced = 0;
        IOBusy = false;
        return StatusCode::NO_DATA_QUERIED;
    }
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
        IOBusy = false;
        return StatusCode::BUFFER_FULL;
    }
    //拷贝数据
    cur = 0;
    if (params->order == TIME_DSC) //时间降序，从内存地址集中反向拷贝
    {
        for (int i = mallocedMemory.size() - 1; i >= 0; i--)
        {
            memcpy(data + cur + startPos, get<0>(mallocedMemory[i]), get<1>(mallocedMemory[i]));
            delete[] get<0>(mallocedMemory[i]);
            cur += get<1>(mallocedMemory[i]);
        }
    }
    else //否则按照默认顺序升序排列即可
    {
        for (auto &mem : mallocedMemory)
        {
            memcpy(data + cur + startPos, get<0>(mem), get<1>(mem));
            delete[] get<0>(mem);
            cur += get<1>(mem);
        }
    }

    buffer->bufferMalloced = 1;
    buffer->buffer = data;
    buffer->length = cur + startPos;
    IOBusy = false;
    return 0;
}

/**
 * @brief 线程任务，处理单个包
 *
 * @param pack 包的内存地址-长度对
 * @param params 查询参数
 * @param cur 当前已分配的内存总长度，临界资源，需要加线程互斥锁
 * @param mallocedMemory 已分配的内存地址-长度-排序值偏移量-时间戳元组，临界资源，需要加线程互斥锁
 * @param type  排序值的数据类型
 * @param typeList 选中的数据类型列表
 * @return int
 */
int PackProcess(pair<char *, long> pack, DB_QueryParams *params, atomic<long> *cur, vector<tuple<char *, long, long, long>> *mallocedMemory, DataType *type, vector<DataType> *typeList)
{
    PackFileReader packReader(pack.first, pack.second);
    if (packReader.packBuffer == NULL)
        return StatusCode::DATAFILE_NOT_FOUND;
    int fileNum;
    string templateName;
    packReader.ReadPackHead(fileNum, templateName);
    if (TemplateManager::CheckTemplate(templateName) != 0)
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    vector<PathCode> pathCodes;
    if (params->byPath)
    {
        CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
    }

    for (int i = 0; i < fileNum; i++)
    {
        long timestamp;
        int readLength, zipType;
        long dataPos = packReader.Next(readLength, timestamp, zipType);
        if (timestamp < params->start || timestamp > params->end) //在时间区间外
            continue;
        char *buff = new char[CurrentTemplate.totalBytes];
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
            ReZipBuff(buff, readLength, params->pathToLine);
            break;
        }
        default:
            delete[] buff;
            continue;
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
            if (typeList->size() == 0)
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, *typeList);
            else
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList);
            for (int i = 0; i < bytesList.size(); i++)
            {
                copyBytes += typeList->at(i).hasTime ? bytesList[i] + 8 : bytesList[i];
            }
        }
        else
        {
            err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, *type);
            copyBytes = type->hasTime ? bytes + 8 : bytes;
        }
        if (err != 0)
        {
            continue;
        }

        char copyValue[copyBytes]; //将要拷贝的数值
        long sortPos = 0;
        int compareBytes = 0;
        if (params->byPath)
        {
            int lineCur = 0; //记录此行当前写位置
            for (int i = 0; i < bytesList.size(); i++)
            {
                int curBytes = typeList->at(i).hasTime ? bytesList[i] + 8 : bytesList[i];
                memcpy(copyValue + lineCur, buff + posList[i], curBytes);
                lineCur += curBytes;
            }
            if (params->valueName != NULL) //此处，若编码为精确搜索，而又输入了不同的变量名，FindSortPosFromSelectedData将返回0
            {
                sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, pathCodes, *typeList);
                compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, *type) == 0 ? bytes : 0;
            }
        }
        else
        {
            memcpy(copyValue, buff + pos, copyBytes);
            compareBytes = bytes;
        }

        bool canCopy = false;
        if (params->compareType != CMP_NONE && compareBytes != 0) //可比较
        {
            char value[compareBytes]; //数值
            memcpy(value, buff + pos, compareBytes);

            //根据比较结果决定是否加入结果集
            int compareRes = DataType::CompareValue(*type, value, params->compareValue);
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
                break;
            }
        }
        else //不可比较，直接拷贝此数据
            canCopy = true;
        if (canCopy) //需要此数据
        {
            char *memory = new (std::nothrow) char[copyBytes];
            if (memory == nullptr)
            {
                cout << "memory null" << endl;
            }
            else
            {
                memcpy(memory, copyValue, copyBytes);
                *cur += copyBytes;
                auto t = make_tuple(memory, copyBytes, sortPos, timestamp);
                memMutex.lock();
                mallocedMemory->push_back(t);
                memMutex.unlock();
            }
        }
        delete[] buff;
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
 * @note   支持idb文件和pak文件混合查询,此处默认pak文件中的时间均早于idb和idbzip文件！！
 */
int DB_QueryByTimespan(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    IOBusy = true;
    if (maxThreads <= 2) //线程数量<=2时，多线程已无必要
    {
        return DB_QueryByTimespan_Single(buffer, params);
    }
    //确认当前模版
    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    if (params->byPath)
    {
        vector<PathCode> vec;
        CurrentTemplate.GetAllPathsByCode(params->pathCode, vec);
        if (vec.size() == 1)
            return DB_QueryByTimespan_Single(buffer, params);
    }
    int check = CheckQueryParams(params);
    if (check != 0)
    {
        IOBusy = false;
        return check;
    }
    vector<pair<string, long>> filesWithTime, selectedFiles;
    auto selectedPacks = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
    //获取每个数据文件，并带有时间戳
    readDataFilesWithTimestamps(params->pathToLine, filesWithTime);

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }

    vector<tuple<char *, long, long, long>> mallocedMemory; //当前已分配的内存地址-长度-排序值偏移-时间戳元组
    atomic<long> cur(0);                                    //已选择的数据总长
    // atomic<vector<tuple<char *, long, long, long>>> mallocedMemory;
    DataType type;
    vector<DataType> typeList, selectedTypes;
    vector<long> sortDataPoses; //按值排序时要比较的数据的偏移量
    vector<PathCode> pathCodes;
    //先对时序在前的包文件检索
    if (selectedPacks.size() > 0)
    {
        int index = 0;
        future_status status[maxThreads - 1];
        future<int> f[maxThreads - 1];
        for (int j = 0; j < maxThreads - 1; j++)
        {
            status[j] = future_status::ready;
        }
        do
        {
            for (int j = 0; j < maxThreads - 1; j++) //留一个线程循环遍历线程集，确认每个线程的运行状态
            {
                if (status[j] == future_status::ready)
                {
                    auto pk = packManager.GetPack(selectedPacks[index].first);
                    f[j] = async(std::launch::async, PackProcess, pk, params, &cur, &mallocedMemory, &type, &typeList);
                    status[j] = f[j].wait_for(chrono::milliseconds(1));
                    index++;
                    if (index == selectedPacks.size())
                        break;
                }
                else
                {
                    status[j] = f[j].wait_for(chrono::milliseconds(1));
                }
            }
        } while (index < selectedPacks.size());
        for (int j = 0; j < maxThreads - 1; j++)
        {
            if (status[j] != future_status::ready)
                f[j].wait();
        }
    }

    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
    {
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    if (params->byPath)
        CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
    //对时序在后的普通文件检索
    for (auto &file : selectedFiles)
    {
        // typeList.clear();
        long len;

        if (file.first.find(".idbzip") != string::npos)
        {
            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
        }
        else if (file.first.find("idb") != string::npos)
        {
            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
        }
        char *buff = new char[CurrentTemplate.totalBytes];
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
            if (typeList.size() == 0)
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
            else
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList);
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
            continue;
        }

        char copyValue[copyBytes]; //将要拷贝的数值
        long sortPos = 0;
        int compareBytes = 0;
        if (params->byPath)
        {
            int lineCur = 0; //记录此行当前写位置
            for (int i = 0; i < bytesList.size(); i++)
            {
                int curBytes = typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
                memcpy(copyValue + lineCur, buff + posList[i], curBytes);
                lineCur += curBytes;
            }
            if (params->valueName != NULL)
            {
                sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, pathCodes, typeList);
                compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type) == 0 ? bytes : 0;
            }
        }
        else
        {
            memcpy(copyValue, buff + pos, copyBytes);
            compareBytes = bytes;
        }

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
            char *memory = new char[copyBytes];
            memcpy(memory, copyValue, copyBytes);
            cur += copyBytes;
            mallocedMemory.push_back(make_tuple(memory, copyBytes, sortPos, file.second));
        }
        delete[] buff;
    }

    sortResult(mallocedMemory, params, type);
    if (cur == 0)
    {
        buffer->buffer = NULL;
        buffer->bufferMalloced = 0;
        IOBusy = false;
        return StatusCode::NO_DATA_QUERIED;
    }
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
        IOBusy = false;
        return StatusCode::BUFFER_FULL;
    }
    //拷贝数据
    cur = 0;
    if (params->order == TIME_DSC) //时间降序，从内存地址集中反向拷贝
    {
        for (int i = mallocedMemory.size() - 1; i >= 0; i--)
        {
            memcpy(data + cur + startPos, get<0>(mallocedMemory[i]), get<1>(mallocedMemory[i]));
            delete[] get<0>(mallocedMemory[i]);
            cur += get<1>(mallocedMemory[i]);
        }
    }
    else //否则按照默认顺序升序排列即可
    {
        for (auto &mem : mallocedMemory)
        {
            memcpy(data + cur + startPos, get<0>(mem), get<1>(mem));
            delete[] get<0>(mem);
            cur += get<1>(mem);
        }
    }

    buffer->bufferMalloced = 1;
    buffer->buffer = data;
    buffer->length = cur + startPos;
    IOBusy = false;
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
 * @note 不再维护
 */
int DB_QueryLastRecords_Old(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;
    vector<pair<string, long>> selectedFiles, packsWithTime;
    vector<string> packFiles;

    //获取每个数据文件，并带有时间戳
    readDataFilesWithTimestamps(params->pathToLine, selectedFiles);

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
    sortByTime(selectedFiles, TIME_DSC);

    //取排序后的文件中前queryNums个符合条件的文件的数据
    vector<pair<char *, long>> mallocedMemory; //已在堆区分配的进入筛选范围数据的内存地址和长度集
    long cur = 0;                              //记录已选中的数据总长度
    DataType type;
    vector<DataType> typeList, selectedTypes;
    vector<long> sortDataPoses; //按值排序时要比较的数据的偏移量
    /*<-----!!!警惕内存泄露!!!----->*/
    //先对时序在后的普通文件检索
    int selectedNum = 0;
    long sortDataPos = 0;
    for (auto &file : selectedFiles)
    {
        typeList.clear();
        long len; //文件长度
        if (file.first.find(".idbzip") != string::npos)
        {
            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
        }
        else if (file.first.find("idb") != string::npos)
        {
            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
        }
        // char *buff = (char *)malloc(CurrentTemplate.totalBytes);
        char *buff = new char[CurrentTemplate.totalBytes];
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
        char copyValue[copyBytes];
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

        bool canCopy = false; //根据比较结果决定是否允许拷贝
        int compareBytes = 0;
        if (params->valueName != NULL)
            compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type) == 0 ? bytes : 0;

        if (params->compareType != CMP_NONE && compareBytes != 0) //可比较
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
            sortDataPoses.push_back(sortPos);
            char *memory = new char[copyBytes];
            memcpy(memory, copyValue, copyBytes);
            cur += copyBytes;
            mallocedMemory.push_back(make_pair(memory, copyBytes));
            selectedNum++;
        }
        // free(buff);
        delete[] buff;
        if (selectedNum == params->queryNums)
            break;
    }

    if (selectedNum < params->queryNums)
    {
        //检索到的数量不够，继续从打包文件中获取
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &pack : packFiles)
        {
            string tmp = pack;
            while (tmp.back() == '/')
                tmp.pop_back();
            vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
            string packName = vec[vec.size() - 1];
            vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
            if (timespan.size() > 0)
            {
                long start = atol(timespan[0].c_str());
                packsWithTime.push_back(make_pair(pack, start));
            }
            else
            {
                return StatusCode::FILENAME_MODIFIED;
            }
        }
        sortByTime(packsWithTime, TIME_DSC);
        for (auto &pack : packsWithTime)
        {
            if (selectedNum == params->queryNums)
                break;
            PackFileReader packReader(pack.first);
            if (packReader.packBuffer == NULL)
                continue;
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            // TemplateManager::CheckTemplate(templateName);
            //由于pak中的文件按时间升序存放，首先依次将此包中文件信息压入栈中，弹出时即为时间降序型

            stack<pair<long, tuple<int, long, int>>> filestk;
            for (int i = 0; i < fileNum; i++)
            {
                typeList.clear();
                long timestamp; //暂时用不到时间戳
                int readLength, zipType;
                long dataPos = packReader.Next(readLength, timestamp, zipType);
                auto t = make_tuple(readLength, timestamp, zipType);
                filestk.push(make_pair(dataPos, t));
            }
            while (!filestk.empty())
            {
                auto fileInfo = filestk.top();
                filestk.pop();
                long dataPos = fileInfo.first;
                int readLength = get<0>(fileInfo.second);
                long timestamp = get<1>(fileInfo.second);
                int zipType = get<2>(fileInfo.second);

                char *buff = new char[CurrentTemplate.totalBytes];
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
                    ReZipBuff(buff, readLength, params->pathToLine);
                    break;
                }
                default:
                {
                    delete[] buff;
                    return StatusCode::UNKNWON_DATAFILE;
                    break;
                }
                }
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

                bool canCopy = false; //根据比较结果决定是否允许拷贝
                int compareBytes = 0;
                if (params->valueName != NULL)
                    compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type) == 0 ? bytes : 0;

                if (compareBytes != 0 && params->compareType != CMP_NONE) //可比较
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
                    sortDataPoses.push_back(sortPos);
                    char *memory = new char[copyBytes];
                    memcpy(memory, copyValue, copyBytes);
                    cur += copyBytes;
                    mallocedMemory.push_back(make_pair(memory, copyBytes));
                    selectedNum++;
                }
                delete[] buff;
                if (selectedNum == params->queryNums)
                    break;
            }
        }
    }
    if (sortDataPoses.size() > 0)
        sortResultByValue(mallocedMemory, sortDataPoses, params, type);
    //已获取指定数量的数据，开始拷贝内存
    char *data;
    if (cur != 0)
    {
        char typeNum = params->byPath ? typeList.size() : 1; //数据类型总数
        int startPos;                                        //数据区起始位置

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
            delete[] mem.first;
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
    /*<-----!!!!!!----->*/
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
    IOBusy = true;
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;
    vector<pair<string, long>> selectedFiles, packsWithTime;
    vector<string> packFiles;

    //获取每个数据文件，并带有时间戳
    readDataFilesWithTimestamps(params->pathToLine, selectedFiles);

    //确认当前模版
    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
    {
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    vector<PathCode> pathCodes;
    if (params->byPath)
        CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
    //根据时间降序排序
    sortByTime(selectedFiles, TIME_DSC);

    //取排序后的文件中前queryNums个符合条件的文件的数据
    vector<pair<char *, long>> mallocedMemory; //已在堆区分配的进入筛选范围数据的内存地址和长度集
    long cur = 0;                              //记录已选中的数据总长度
    DataType type;
    vector<DataType> typeList, selectedTypes;
    vector<long> sortDataPoses; //按值排序时要比较的数据的偏移量
    /*<-----!!!警惕内存泄露!!!----->*/
    //先对时序在后的普通文件检索
    int selectedNum = 0;
    long sortDataPos = 0;
    for (auto &file : selectedFiles)
    {
        // typeList.clear();
        long len; //文件长度
        if (file.first.find(".idbzip") != string::npos)
        {
            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
        }
        else if (file.first.find("idb") != string::npos)
        {
            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
        }
        char *buff = new char[CurrentTemplate.totalBytes];
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
            if (typeList.size() == 0)
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
            else
                err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList);
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
            IOBusy = false;
            return err;
        }
        char copyValue[copyBytes];
        long sortPos = 0;
        int compareBytes = 0;
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
            {
                sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, pathCodes, typeList);
                compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type) == 0 ? bytes : 0;
            }
        }
        else
        {
            memcpy(copyValue, buff + pos, copyBytes);
            compareBytes = bytes;
        }

        bool canCopy = false; //根据比较结果决定是否允许拷贝

        if (params->compareType != CMP_NONE && compareBytes != 0) //可比较
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
            sortDataPoses.push_back(sortPos);
            char *memory = new char[copyBytes];
            memcpy(memory, copyValue, copyBytes);
            cur += copyBytes;
            mallocedMemory.push_back(make_pair(memory, copyBytes));
            selectedNum++;
        }
        delete[] buff;
        if (selectedNum == params->queryNums)
            break;
    }

    if (selectedNum < params->queryNums)
    {
        //检索到的数量不够，继续从打包文件中获取
        int packNums = packManager.allPacks[params->pathToLine].size();
        for (int index = 1; selectedNum < params->queryNums && index <= packNums; index++)
        {
            auto pack = packManager.GetLastPack(params->pathToLine, index);
            PackFileReader packReader(pack.second.first, pack.second.second);
            if (packReader.packBuffer == NULL)
                continue;
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            if (TemplateManager::CheckTemplate(templateName) != 0)
                continue;
            if (params->byPath)
                CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
            stack<pair<long, tuple<int, long, int>>> filestk;
            for (int i = 0; i < fileNum; i++)
            {

                long timestamp; //暂时用不到时间戳
                int readLength, zipType;
                long dataPos = packReader.Next(readLength, timestamp, zipType);
                auto t = make_tuple(readLength, timestamp, zipType);
                filestk.push(make_pair(dataPos, t));
            }
            while (!filestk.empty())
            {
                auto fileInfo = filestk.top();
                filestk.pop();
                long dataPos = fileInfo.first;
                int readLength = get<0>(fileInfo.second);
                long timestamp = get<1>(fileInfo.second);
                int zipType = get<2>(fileInfo.second);

                char *buff = new char[CurrentTemplate.totalBytes];
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
                    ReZipBuff(buff, readLength, params->pathToLine);
                    break;
                }
                default:
                {
                    delete[] buff;
                    break;
                }
                }
                //获取数据的偏移量和字节数
                long bytes = 0, pos = 0;         //单个变量
                vector<long> posList, bytesList; //多个变量
                long copyBytes = 0;

                int err;
                if (params->byPath)
                {
                    char *pathCode = params->pathCode;
                    if (typeList.size() == 0)
                        err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
                    else
                        err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList);
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
                    IOBusy = false;
                    return err;
                }
                char copyValue[copyBytes];
                long sortPos = 0;
                int compareBytes = 0;
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
                    {
                        sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, pathCodes, typeList);
                        compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type) == 0 ? bytes : 0;
                    }
                }
                else
                {
                    memcpy(copyValue, buff + pos, copyBytes);
                    compareBytes = bytes;
                }

                bool canCopy = false; //根据比较结果决定是否允许拷贝

                if (params->compareType != CMP_NONE && compareBytes != 0) //可比较
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
                        break;
                    }
                }
                else //不可比较，直接拷贝此数据
                    canCopy = true;

                if (canCopy) //需要此数据
                {
                    sortDataPoses.push_back(sortPos);
                    char *memory = new char[copyBytes];
                    memcpy(memory, copyValue, copyBytes);
                    cur += copyBytes;
                    mallocedMemory.push_back(make_pair(memory, copyBytes));
                    selectedNum++;
                }
                delete[] buff;
                if (selectedNum == params->queryNums)
                    break;
            }
        }
    }
    if (sortDataPoses.size() > 0)
        sortResultByValue(mallocedMemory, sortDataPoses, params, type);
    //已获取指定数量的数据，开始拷贝内存
    char *data;
    if (cur != 0)
    {
        char typeNum = params->byPath ? typeList.size() : 1; //数据类型总数
        int startPos;                                        //数据区起始位置

        data = (char *)malloc(cur + (int)typeNum * 11 + 1);
        if (!params->byPath)
            startPos = CurrentTemplate.writeBufferHead(params->valueName, type, data); //写入缓冲区头，获取数据区起始位置
        else
            startPos = CurrentTemplate.writeBufferHead(params->pathCode, typeList, data);
        if (data == NULL)
        {
            buffer->buffer = NULL;
            buffer->bufferMalloced = 0;
            IOBusy = false;
            return StatusCode::BUFFER_FULL;
        }
        //拷贝数据
        cur = 0;
        for (auto &mem : mallocedMemory)
        {
            memcpy(data + cur + startPos, mem.first, mem.second);
            delete[] mem.first;
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
    IOBusy = false;
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
int DB_QueryByFileID_Old(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;
    string pathToLine = params->pathToLine;
    string fileid = params->fileID;
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
    vector<string> dataFiles, packFiles;
    readDataFiles(params->pathToLine, dataFiles);
    readPakFilesList(params->pathToLine, packFiles);
    // if (paths.size() > 0)
    //     prefix = paths[paths.size() - 1];
    for (auto &pack : packFiles)
    {
        PackFileReader packReader(pack);
        if (packReader.packBuffer == NULL)
            continue;
        int fileNum;
        string templateName;
        packReader.ReadPackHead(fileNum, templateName);
        for (int i = 0; i < fileNum; i++)
        {
            string fileID;
            int readLength, zipType;
            long dataPos = packReader.Next(readLength, fileID, zipType);
            if (fileID == fileid)
            {
                if (TemplateManager::CheckTemplate(templateName) != 0)
                    return StatusCode::SCHEMA_FILE_NOT_FOUND;
                char *buff = new char[CurrentTemplate.totalBytes];
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
                    ReZipBuff(buff, readLength, params->pathToLine);
                    break;
                }
                default:
                    delete[] buff;
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
                    for (int i = 0; i < bytesList.size() && err == 0; i++)
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
                delete[] buff;
                buffer->buffer = data;
                buffer->length = copyBytes + startPos;
                return 0;
            }
        }
    }

    for (string &file : dataFiles)
    {
        if (file.find(fileid) != string::npos)
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
int DB_QueryByFileID(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    IOBusy = true;
    int check = CheckQueryParams(params);
    if (check != 0)
    {
        IOBusy = false;
        return check;
    }
    string pathToLine = params->pathToLine;
    string fileid = params->fileID;
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

    auto pack = packManager.GetPackByID(params->pathToLine, fileid);
    if (pack.first != NULL && pack.second != 0)
    {
        PackFileReader packReader(pack.first, pack.second);
        int fileNum;
        string templateName;
        packReader.ReadPackHead(fileNum, templateName);
        for (int i = 0; i < fileNum; i++)
        {
            string fileID;
            int readLength, zipType;
            long dataPos = packReader.Next(readLength, fileID, zipType);
            if (fileID == fileid)
            {
                if (TemplateManager::CheckTemplate(templateName) != 0)
                {
                    IOBusy = false;
                    return StatusCode::SCHEMA_FILE_NOT_FOUND;
                }
                char *buff = new char[CurrentTemplate.totalBytes];
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
                    ReZipBuff(buff, readLength, params->pathToLine);
                    break;
                }
                default:
                    delete[] buff;
                    IOBusy = false;
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
                    for (int i = 0; i < bytesList.size() && err == 0; i++)
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
                    IOBusy = false;
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
                    IOBusy = false;
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
                delete[] buff;
                buffer->buffer = data;
                buffer->length = copyBytes + startPos;
                IOBusy = false;
                return 0;
            }
        }
    }
    vector<string> dataFiles;
    readDataFiles(params->pathToLine, dataFiles);
    for (string &file : dataFiles)
    {
        if (file.find(fileid) != string::npos)
        {
            long len;
            DB_GetFileLengthByPath(const_cast<char *>(file.c_str()), &len);
            char buff[len];
            DB_OpenAndRead(const_cast<char *>(file.c_str()), buff);
            if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
            {
                IOBusy = false;
                return StatusCode::SCHEMA_FILE_NOT_FOUND;
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
                IOBusy = false;
                return err;
            }
            if (len < pos)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
                IOBusy = false;
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
                IOBusy = false;
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
            IOBusy = false;
            return 0;
        }
    }
    IOBusy = false;
    return StatusCode::DATAFILE_NOT_FOUND;
}

int main()
{
    DataTypeConverter converter;
    DB_QueryParams params;
    params.pathToLine = "JinfeiSixteen";
    params.fileID = "JinfeiSixteen15";
    char code[10];
    code[0] = (char)0;
    code[1] = (char)1;
    code[2] = (char)0;
    code[3] = (char)0;
    code[4] = 0;
    code[5] = (char)0;
    code[6] = 0;
    code[7] = (char)0;
    code[8] = (char)0;
    code[9] = (char)0;
    params.pathCode = code;
    params.valueName = "S2OFF";
    // params.valueName = NULL;
    params.start = 1650010750421;
    params.end = 1650169000000;
    params.order = ASCEND;
    params.compareType = LT;
    params.compareValue = "666";
    params.queryType = LAST;
    params.byPath = 1;
    params.queryNums = 43345;
    DB_DataBuffer buffer;
    buffer.savePath = "/";
    // cout << settings("Pack_Mode") << endl;
    // vector<pair<string, long>> files;
    // readDataFilesWithTimestamps("", files);
    // Packer::Pack("/",files);
    auto startTime = std::chrono::system_clock::now();
    // DB_QueryByTimespan_Single(&buffer, &params);

    auto endTime = std::chrono::system_clock::now();
    // free(buffer.buffer);

    std::cout << "首次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
    // free(buffer.buffer);
    //  startTime = std::chrono::system_clock::now();
    //  DB_QueryWholeFile_MultiThread(&buffer, &params);

    // endTime = std::chrono::system_clock::now();
    // std::cout << "第二次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
    // free(buffer.buffer);
    // startTime = std::chrono::system_clock::now();
    // // DB_QueryByTimespan_MultiThread(&buffer, &params);

    // endTime = std::chrono::system_clock::now();
    // std::cout << "第三次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
    // // free(buffer.buffer);
    // return 0;
    // // DB_QueryLastRecords_Using_Cache(&buffer, &params);
    // // DB_QueryByTimespan_Using_Cache(&buffer, &params);
    // // DB_QueryByTimespan(&buffer, &params);
    for (int i = 0; i < 10; i++)
    {
        startTime = std::chrono::system_clock::now();
        DB_QueryByTimespan_NonAtomic(&buffer, &params);

        endTime = std::chrono::system_clock::now();
        std::cout << "第" << i + 1 << "次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
        cout << buffer.length << endl;
        free(buffer.buffer);
        buffer.length = 0;
        buffer.bufferMalloced = 0;
    }
    for (int i = 0; i < 10; i++)
    {
        startTime = std::chrono::system_clock::now();
        DB_QueryByTimespan(&buffer, &params);

        endTime = std::chrono::system_clock::now();
        std::cout << "第" << i + 11 << "次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
        cout << buffer.length << endl;
        free(buffer.buffer);
        buffer.length = 0;
        buffer.bufferMalloced = 0;
    }
    if (buffer.bufferMalloced)
    {
        char buf[buffer.length];
        memcpy(buf, buffer.buffer, buffer.length);
        cout << buffer.length << endl;
        // for (int i = 0; i < buffer.length; i++)
        // {
        //     cout << (int)buf[i] << " ";
        //     if (i % 11 == 0)
        //         cout << endl;
        // }

        free(buffer.buffer);
    }
    // buffer.bufferMalloced = 0;
    // DB_QueryByFileID(&buffer, &params);

    // if (buffer.bufferMalloced)
    // {
    //     char buf[buffer.length];
    //     memcpy(buf, buffer.buffer, buffer.length);
    //     cout << buffer.length << endl;
    //     for (int i = 0; i < buffer.length; i++)
    //     {
    //         cout << (int)buf[i] << " ";
    //         if (i % 11 == 0)
    //             cout << endl;
    //     }

    //     free(buffer.buffer);
    // }
    // buffer.buffer = NULL;
    return 0;
}
