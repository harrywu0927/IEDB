#ifndef _UTILS_HPP
#define _UTILS_HPP

#include <Python.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <pthread.h>
#ifndef __linux__
#include <mach/mach.h>
#endif
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <memory>
#include <stack>
#include <mutex>
#include <cstdio>
#include <string.h>
#include <algorithm>
#include "DataTypeConvert.hpp"
#include "CassFactoryDB.h"
#include <sstream>
#include "CJsonObject.hpp"
#include <thread>
#include <future>
#include <chrono>
#include <stdarg.h>
#include <queue>
#include <LzmaLib.h>
#include <spdlog/spdlog.h>
#include "spdlog/sinks/rotating_file_sink.h"
#include <Logger.h>
#include <openssl/sha.h>

using namespace std;
#ifdef __linux__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#pragma once

#define PACK_MAX_SIZE 1024 * 1024 * 5

#define DEBUG

#define QRY_BUFFER_HEAD_ROW 19
#define PATH_CODE_LEVEL 5
#define PATH_CODE_LEVEL_SIZE 2
#define QRY_BUFFER_HEAD_DTYPE unsigned short
#define TYPE_NUM_SIZE 2
#define PATH_CODE_SIZE PATH_CODE_LEVEL *PATH_CODE_LEVEL_SIZE
// 包中FID大小
#define PACK_FID_SIZE 20
// 基本数据类型总数
#define DATA_TYPE_NUM 10

// 模版文件名的长度
#define TEMPLATE_NAME_SIZE 20
// 包中ID的长度
#define PACK_FID_SIZE 20
// 包头中文件数量的数据类型
#define PACK_FILE_NUM_DTYPE int
#define PACK_HEAD_SIZE TEMPLATE_NAME_SIZE + sizeof(PACK_FILE_NUM_DTYPE)

#define CUBICLE_LEVEL 2 // 编码中的工位层级(0-4)

namespace StatusCode
{
    enum StatusCode
    {
        DATA_TYPE_MISMATCH_ERROR = 135,   // 数据类型不匹配
        SCHEMA_FILE_NOT_FOUND = 136,      // 未找到模版文件
        PATHCODE_INVALID = 137,           // 路径编码不合法
        UNKNOWN_TYPE = 138,               // 未知数据类型
        TEMPLATE_RESOLUTION_ERROR = 139,  // 模版文件解析错误
        DATAFILE_NOT_FOUND = 140,         // 未找到数据文件
        UNKNOWN_PATHCODE = 141,           // 未知的路径编码
        UNKNOWN_VARIABLE_NAME = 142,      // 未知的变量名
        BUFFER_FULL = 143,                // 缓冲区满，还有数据
        UNKNWON_DATAFILE = 144,           // 未知的数据文件或数据文件不合法
        NO_QUERY_TYPE = 145,              // 未指定主查询条件
        QUERY_TYPE_NOT_SURPPORT = 146,    // 不支持的查询方式
        EMPTY_PATH_TO_LINE = 147,         // 到产线的路径为空
        EMPTY_SAVE_PATH = 148,            // 空的存储路径
        NO_DATA_QUERIED = 149,            // 未找到数据
        VARIABLE_NAME_EXIST = 150,        // 变量名已存在
        PATHCODE_EXIST = 151,             // 编码已存在
        FILENAME_MODIFIED = 152,          // 文件名被篡改
        INVALID_TIMESPAN = 153,           // 时间段设置错误或未指定
        NO_PATHCODE_OR_NAME = 154,        // 查询参数中路径编码和变量名均未找到
        NO_QUERY_NUM = 155,               // 未指定查询条数
        NO_FILEID = 156,                  // 未指定文件ID
        VARIABLE_NOT_ASSIGNED = 157,      // 比较某个值时未指定变量名
        NO_COMPARE_VALUE = 158,           // 指定了比较类型却没有赋值
        ZIPTYPE_ERROR = 159,              // 压缩数据类型出现问题
        NO_ZIP_TYPE = 160,                // 未指定压缩/还原条件
        ISARRAY_ERROR = 161,              // isArray只能为0或者1
        HASTIME_ERROR = 162,              // hasTime只能为0或者1
        ARRAYLEN_ERROR = 163,             // arrayLen不能小于１
        ISTS_ERROR = 164,                 // isTS只能为0或者1
        TSLEN_ERROR = 165,                // TsLen不能小于１
        DIR_INCLUDE_NUMBER = 166,         // 文件夹名包含数字
        AMBIGUOUS_QUERY_PARAMS = 167,     // 查询条件有歧义
        ML_TYPE_NOT_SUPPORT = 168,        // 不支持的机器学习条件
        NOVEL_DATA_DETECTED = 169,        // 检测到奇异数据
        PYTHON_SCRIPT_NOT_FOUND = 170,    // 未找到python脚本文件
        METHOD_NOT_FOUND_IN_PYTHON = 171, // python脚本中未找到相应的方法
        VARIABLE_NAME_CHECK_ERROR = 172,  // 变量名不合法
        PATHCODE_CHECK_ERROR = 173,       // 路径编码不合法
        STANDARD_CHECK_ERROR = 174,       // 标准值不合法
        MAX_CHECK_ERROR = 175,            // 最大值不合法
        MIN_CHECK_ERROR = 176,            // 最小值不合法
        VALUE_CHECKE_ERROR = 177,         // 变量值不合法
        VALUE_RANGE_ERROR = 178,          // 变量值范围不合法，例如最小值大于最大值
        INVALID_QRY_PARAMS = 179,         // 查询条件不合法
        INVALID_QUERY_BUFFER = 180,       // 查询缓存不合法
        IMG_NOT_FOUND = 181,              // 未在文件中找到图片
        MAX_OR_MIN_ERROR = 182,           // maxOrmin只能为0,1,2
        ZIPNUM_ERROR = 183,               // 压缩数量错误
        ZIPNUM_AND_EID_ERROR = 184,       // zipNum和EID冲突，不能同时设置
        VARIABLE_NAME_NOT_INT_CODE = 185, // 编码中不包含指定变量名
        MEMORY_INSUFFICIENT = 186,        // 内存不足
        BACKUP_BROKEN = 187,              // 备份文件损坏
        ERROR_SID_EID_RANGE = 188,        // SID大于EID
        DATAFILE_MODIFIED = 189,          // 文件内容被篡改
        EMPTY_PAK = 190,
    };
}
namespace ValueType
{
    enum ValueType
    {
        UINT = 1,
        USINT = 2,
        UDINT = 3,
        INT = 4,
        BOOL = 5,
        SINT = 6,
        DINT = 7,
        REAL = 8,
        TIME = 9,
        IMAGE = 10,
        UNKNOWN = 0
    };
}

extern int errorCode; // 错误码

extern int maxThreads; // 此设备支持的最大线程数

extern bool IOBusy; // IO繁忙

extern int RepackThreshold; // 再打包的最大大小

extern neb::CJsonObject settings;

extern queue<string> fileQueue;

// 获取文件相关函数
void readFileList(string path, vector<string> &files);

void readIDBFilesList(string path, vector<string> &files);

int readIDBFilesListBySIDandNum(string path, string SID, uint32_t num, vector<pair<string, long>> &selectedFiles);

int readIDBFilesListBySIDandEID(string path, string SID, string EID, vector<pair<string, long>> &selectedFiles);

void readIDBZIPFilesList(string path, vector<string> &files);

int readIDBZIPFilesListBySIDandNum(string path, string SID, uint32_t num, vector<pair<string, long>> &selectedFiles);

int readIDBZIPFilesListBySIDandEID(string path, string SID, string EID, vector<pair<string, long>> &selectedFiles);

void readTEMFilesList(string path, vector<string> &files);

void readZIPTEMFilesList(string path, vector<string> &files);

void readIDBFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime);

void readIDBZIPFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime);

void readDataFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime);

void readDataFiles(string path, vector<string> &files);

void readPakFilesList(string path, vector<string> &files);

void readAllDirs(vector<string> &dirs, string basePath);

void readFiles(vector<string> &paths, string path, string extension, bool recursive = false);

// 其他函数
long getMilliTime();

void removeFilenameLabel(string &path);

void sortByTime(vector<pair<string, long>> &selectedFiles, DB_Order order);

int getMemory(long size, char *mem);

// void getMemoryUsage(long &total, long &available);

int CheckQueryParams(DB_QueryParams *params);

int ReZipBuff(char **buff, int &buffLength, const char *pathToLine = nullptr);

long GetZipImgPos(char *buff);

bool IsNormalIDBFile(char *readbuff, const char *pathToLine);

int sysOpen(char path[]);

int FindImage(char **buff, long &length, string &path, int index, char *pathCode);

int FindImage(char **buff, long &length, string &path, int index, const char *valueName);

int FindImage(char **buff, long &length, string &path, int index, vector<string> &names);

queue<string> InitFileQueue();

int DB_QueryByTimespan_Single(DB_DataBuffer *buffer, DB_QueryParams *params);

// python相关
PyObject *ConvertToPyList_ML(DB_DataBuffer *buffer);

PyObject *ConvertToPyList_STAT(DB_DataBuffer *buffer);

PyObject *PythonCall(PyObject *Args, const char *moduleName, const char *funcName, const char *path = "./");

PyObject *PythonCall(const char *moduleName, const char *funcName, const char *path, int num, ...);

// 根据时间升序或降序排序
void sortByTime(vector<pair<string, long>> &selectedFiles, DB_Order order);

class iedb_err : exception
{
public:
    int code;
    string content;
    iedb_err(){};
    iedb_err(int code)
    {
        this->code = code;
    }
    iedb_err(int code, string content)
    {
        this->code = code;
        this->content = content;
    }
    ~iedb_err(){};
    const char *what() const noexcept;
    string paramInfo(DB_QueryParams *params);
};

class PathCode
{
public:
    vector<int> paths;
    string name;
    char code[PATH_CODE_SIZE];
    // 解码路径为整数数组
    vector<int> DecodePath(char pathEncode[], int &levels)
    {
        DataTypeConverter converter;
        vector<int> res;
        for (int i = 0; i < levels; i++)
        {
            string str = "  ";
            str[0] = pathEncode[i * PATH_CODE_LEVEL_SIZE];
            str[1] = pathEncode[i * PATH_CODE_LEVEL_SIZE + 1];
            res.push_back((int)converter.ToUInt16(const_cast<char *>(str.c_str())));
        }
        return res;
    }
    PathCode(){};
    PathCode(char pathEncode[], int levels, string name)
    {
        this->paths = DecodePath(pathEncode, levels);
        this->name = name;
        for (size_t i = 0; i < PATH_CODE_SIZE; i++)
        {
            this->code[i] = pathEncode[i];
        }
    }
    inline bool operator==(const PathCode &pathCode)
    {
        return memcmp(code, pathCode.code, PATH_CODE_SIZE) == 0 ? true : false;
    }
};

// 管理模板中的数据类型等信息
class DataType
{
public:
    bool isArray = false;
    bool hasTime = false;
    bool isTimeseries = false;      // 此值为true时，hasTime不可为true。(时间序列：<value,timestamp>键-值序列)
    int tsLen;                      // 时间序列长度
    int arrayLen;                   // 数组长度
    int valueBytes;                 // 值所占字节
    long timeseriesSpan = 0;        // 时间序列的采样时间
    ValueType::ValueType valueType; // 基本数据类型

    // char maxValue[10];      //最大值
    // char minValue[10];      //最小值
    // char standardValue[10]; //标准值
    char maxValue[5];      // 最大值
    char minValue[5];      // 最小值
    char standardValue[5]; // 标准值
    char (*max)[5];
    char (*min)[5];
    char (*standard)[5];

    // 实现分割字符串(源字符串也将改变) 线程不安全
    //@param srcstr     源字符串
    //@param str        划分字符
    static vector<string> StringSplit(char srcstr[], const char *str);

    // 使用STL的字符串分割
    static vector<string> splitWithStl(const string &str, const string &pattern);
    // 判断值类型
    static ValueType::ValueType JudgeValueType(string vType);
    // 判断值类型
    static string JudgeValueTypeByNum(int &vType);
    static int JudgeByValueType(ValueType::ValueType &vType);
    // 获取值类型所占字节数
    static int GetValueBytes(ValueType::ValueType &type);
    // 获取此数据类型所占的总字节数
    static int GetTypeBytes(DataType &type);
    // 从字符串中获取数据类型
    static int GetDataTypeFromStr(char dataType[], DataType &type);

    /**
     * @brief 根据指定的数据类型比较两个字节数据值的大小，暂不支持数组的比较。
     * @param type        数据类型
     * @param compared    被比较的值
     * @param toCompare   要比较的字符串字面值，如"123"，则值就为123
     *
     * @return 1:        compared > toCompare,
     *         0:        compared = toCompare,
     *         -1:       compared < toCompare
     * @note
     */
    static int CompareValue(DataType &type, char *compared, const char *toCompare);
    /**
     * @brief 根据指定的数据类型比较两个字节数据值的大小，暂不支持数组的比较。
     * @param type        数据类型
     * @param compared    被比较的值
     * @param toCompare   要比较的值
     *
     * @return 1:        compared > toCompare,
     *         0:        compared = toCompare,
     *         -1:       compared < toCompare
     * @note
     */
    static int CompareValueInBytes(DataType &type, char *compared, const char *toCompare);
};

void AppendValueToPyList(PyObject *item, char *buffer, int &cur, DataType &type);

void SetValueToPyList(PyObject *item, char *buffer, int &cur, DataType &type, int index);

int getBufferValueType(DataType &type);

class Template // 标准模板
{
public:
    vector<pair<PathCode, DataType>> schemas;
    string path; // 挂载路径
    // char *temFileBuffer; //模版文件缓存
    // long fileLength;
    long totalBytes;
    bool hasImage = false; // 若包含图片，则查询时需要对每一个节拍对照模版遍历；若不包含，则仅需获取数据的绝对偏移，大大节省查询时间
    Template() {}
    Template(vector<PathCode> &pathEncodes, vector<DataType> &dataTypes, const char *path)
    {
        for (int i = 0; i < pathEncodes.size(); i++)
        {
            this->schemas.push_back(make_pair(pathEncodes[i], dataTypes[i]));
            if (dataTypes[i].valueType == ValueType::IMAGE)
                this->hasImage = true;
        }
        this->path = path;
        this->totalBytes = GetTotalBytes();
    }

    int GetAllPathsByCode(char *pachCode, vector<PathCode> &pathCodes);

    bool checkHasArray(char *pathCode);
    bool checkHasArray(vector<string> &names);

    bool checkHasImage(char *pathCode);
    bool checkHasImage(vector<string> &names);

    bool checkHasTimeseries(char *pathCode);
    bool checkHasTimeseries(vector<string> &names);

    bool checkIsArray(const char *name);

    bool checkIsImage(const char *name);

    bool checkIsTimeseries(const char *name);

    int FindDatatypePosByCode(char pathCode[], char *buff, long &position, long &bytes);

    int FindDatatypePosByCode(char pathCode[], char *buff, long &position, long &bytes, DataType &type);

    int FindMultiDatatypePosByCode(char pathCode[], vector<long> &positions, vector<long> &bytes, vector<DataType> &types);

    int FindMultiDatatypePosByCode(char pathCode[], char *buff, vector<long> &positions, vector<long> &bytes, vector<DataType> &types);

    int FindMultiDatatypePosByCode(char pathCode[], char *buff, vector<long> &positions, vector<long> &bytes);

    int FindDatatypePosByName(const char *name, char *buff, long &position, long &bytes);

    int FindDatatypePosByName(const char *name, char *buff, long &position, long &bytes, DataType &type);

    int FindDatatypePosByName(const char *name, long &position, long &bytes, DataType &type);

    int FindMultiDatatypePosByNames(vector<string> &names, vector<long> &positions, vector<long> &bytes, vector<DataType> &types);

    int FindMultiDatatypePosByNames(vector<string> &names, char *buff, vector<long> &positions, vector<long> &bytes, vector<DataType> &types);

    int FindMultiDatatypePosByNames(vector<string> &names, char *buff, vector<long> &positions, vector<long> &bytes);

    int writeBufferHead(vector<string> &names, vector<DataType> &typeList, char *buffer);

    int writeBufferHead(char *pathCode, vector<DataType> &typeList, char *buffer);

    int writeBufferHead(string name, DataType &type, char *buffer);

    long FindSortPosFromSelectedData(vector<long> &bytesList, string name, char *pathCode, vector<DataType> &typeList);

    long FindSortPosFromSelectedData(vector<long> &bytesList, string name, vector<PathCode> &pathCodes, vector<DataType> &typeList);

    long GetTotalBytes();

    long GetBytesByCode(char *pathCode);

    int GetDataTypeByCode(char *pathCode, DataType &type);

    int GetDataTypesByCode(char *pathCode, vector<DataType> &types);

    int GetDataTypeByName(const char *name, DataType &type);

    int GetCodeByName(const char *name, vector<PathCode> &pathCode);

    int GetCodeByName(const char *name, PathCode &pathCode);

    vector<DataType> GetAllTypes(char *pathCode);
};

int getBufferDataPos(vector<DataType> &typeList, int num);

extern Template CurrentTemplate;
extern int maxTemplates;
extern vector<Template> templates;

// 模版的管理
class TemplateManager
{
private:
public:
    // 将模版设为当前模版
    static int SetTemplate(const char *path)
    {
        vector<string> files;
        string pathToLine = path;
        readFileList(pathToLine, files);
        string temPath = "";
        for (string &file : files) // 找到带有后缀tem的文件
        {
            if (fs::path(file).extension() == ".tem")
            {
                temPath = file;
                break;
            }
        }
        if (temPath == "")
            return StatusCode::SCHEMA_FILE_NOT_FOUND;
        long length;
        DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &length);
        char buf[length];
        int err = DB_OpenAndRead(const_cast<char *>(temPath.c_str()), buf);
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
            char timeFlag;
            memcpy(&timeFlag, buf + i++, 1);
            memcpy(pathEncode, buf + i, PATH_CODE_SIZE);
            i += PATH_CODE_SIZE;
            vector<string> paths;
            PathCode pathCode(pathEncode, PATH_CODE_LEVEL, variable);
            DataType type;

            if (DataType::GetDataTypeFromStr(dataType, type) == 0)
            {
                if ((int)timeFlag == 1 && !type.isTimeseries) // 如果是时间序列，即使timeFlag为1，依然不额外携带时间
                    type.hasTime = true;
                else
                    type.hasTime = false;
                dataTypes.push_back(type);
            }
            else
                return errorCode;
            pathEncodes.push_back(pathCode);
        }
        Template tem(pathEncodes, dataTypes, path);
        CurrentTemplate = tem;
        return 0;
    }

    // 卸载指定路径下的模版
    static int UnsetTemplate(string path);

    static int CheckTemplate(string path)
    {
        if ((path != CurrentTemplate.path && path != "") || CurrentTemplate.path == "")
        {
            return SetTemplate(path.c_str());
        }
        return 0;
    }
};

class ZipTemplate // 压缩模板
{
public:
    vector<pair<string, DataType>> schemas;
    // unordered_map<vector<int>, string> pathNames;
    string path; // 挂载路径
    // char *temFileBuffer; //模版文件缓存
    long fileLength;
    long totalBytes;
    ZipTemplate() {}
    ZipTemplate(vector<string> &dataName, vector<DataType> &dataTypes, const char *path)
    {
        for (int i = 0; i < dataName.size(); i++)
        {
            this->schemas.push_back(make_pair(dataName[i], dataTypes[i]));
        }
        this->path = path;
        this->totalBytes = GetTotalBytes();
    }

    long GetTotalBytes();
};

extern vector<ZipTemplate> ZipTemplates;
extern ZipTemplate CurrentZipTemplate;
class ZipTemplateManager
{
private:
public:
    // 将模版设为当前模版
    static int SetZipTemplate(const char *path)
    {
        vector<string> files;
        readFileList(path, files);
        string temPath = "";
        for (string file : files) // 找到带有后缀ziptem的文件
        {
            string s = file;
            vector<string> vec = DataType::StringSplit(const_cast<char *>(s.c_str()), ".");
            if (vec[vec.size() - 1].find("ziptem") != string::npos)
            {
                temPath = file;
                break;
            }
        }
        if (temPath == "")
            return StatusCode::SCHEMA_FILE_NOT_FOUND;
        long length;
        DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &length);
        char buf[length];
        int err = DB_OpenAndRead(const_cast<char *>(temPath.c_str()), buf);
        if (err != 0)
            return err;
        int i = 0;

        DataTypeConverter dtc;
        vector<string> dataName;
        vector<DataType> dataTypes;
        while (i < length)
        {
            DataType type;
            char variable[30], dataType[30], standardValue[5], maxValue[5], minValue[5], hasTime[1], timeseriesSpan[4];
            // 变量名
            memcpy(variable, buf + i, 30);
            i += 30;
            dataName.push_back(variable);

            // 数据类型
            memcpy(dataType, buf + i, 30);
            i += 30;
            if (DataType::GetDataTypeFromStr(dataType, type) == 0)
            {
                if ((bool)hasTime[0] == true && !type.isTimeseries) // 如果是时间序列，即使hasTime为1，依然不额外携带时间
                    type.hasTime = true;
                else
                    type.hasTime = false;
            }
            else
                return errorCode;

            // 标准值、最大值、最小值
            if (type.isArray)
            {
                memset(type.maxValue, 0, sizeof(type.maxValue));
                memset(type.minValue, 0, sizeof(type.minValue));
                memset(type.standardValue, 0, sizeof(type.standardValue));
                type.standard = new char[type.arrayLen][5];
                for (int t = 0; t < type.arrayLen; t++)
                {
                    memcpy(type.standard[t], buf + i, 5);
                    i += 5;
                }
                type.max = new char[type.arrayLen][5];
                for (int t = 0; t < type.arrayLen; t++)
                {
                    memcpy(type.max[t], buf + i, 5);
                    i += 5;
                }
                type.min = new char[type.arrayLen][5];
                for (int t = 0; t < type.arrayLen; t++)
                {
                    memcpy(type.min[t], buf + i, 5);
                    i += 5;
                }
            }
            else
            {
                memcpy(standardValue, buf + i, 5);
                i += 5;
                memcpy(maxValue, buf + i, 5);
                i += 5;
                memcpy(minValue, buf + i, 5);
                i += 5;
                memcpy(type.standardValue, standardValue, 5);
                memcpy(type.maxValue, maxValue, 5);
                memcpy(type.minValue, minValue, 5);
            }

            memcpy(hasTime, buf + i, 1);
            i += 1;
            memcpy(timeseriesSpan, buf + i, 4);
            i += 4;
            type.hasTime = (bool)hasTime[0];
            type.timeseriesSpan = dtc.ToUInt32_m(timeseriesSpan);
            dataTypes.push_back(type);
        }
        ZipTemplate tem(dataName, dataTypes, path);
        CurrentZipTemplate = tem;
        return 0;
    }
    // static int SetZipTemplate(const char *path)
    // {
    //     vector<string> files;
    //     readFileList(path, files);
    //     string temPath = "";
    //     for (string file : files) //找到带有后缀ziptem的文件
    //     {
    //         string s = file;
    //         vector<string> vec = DataType::StringSplit(const_cast<char *>(s.c_str()), ".");
    //         if (vec[vec.size() - 1].find("ziptem") != string::npos)
    //         {
    //             temPath = file;
    //             break;
    //         }
    //     }
    //     if (temPath == "")
    //         return StatusCode::SCHEMA_FILE_NOT_FOUND;
    //     long length;
    //     DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &length);
    //     char buf[length];
    //     int err = DB_OpenAndRead(const_cast<char *>(temPath.c_str()), buf);
    //     if (err != 0)
    //         return err;
    //     int i = 0;

    //     DataTypeConverter dtc;
    //     vector<string> dataName;
    //     vector<DataType> dataTypes;
    //     while (i < length)
    //     {
    //         char variable[30], dataType[30], standardValue[10], maxValue[10], minValue[10], hasTime[1], timeseriesSpan[4];
    //         memcpy(variable, buf + i, 30);
    //         i += 30;
    //         memcpy(dataType, buf + i, 30);
    //         i += 30;
    //         memcpy(standardValue, buf + i, 10);
    //         i += 10;
    //         memcpy(maxValue, buf + i, 10);
    //         i += 10;
    //         memcpy(minValue, buf + i, 10);
    //         i += 10;
    //         memcpy(hasTime, buf + i, 1);
    //         i += 1;
    //         memcpy(timeseriesSpan, buf + i, 4);
    //         i += 4;
    //         vector<string> paths;

    //         dataName.push_back(variable);
    //         DataType type;

    //         memcpy(type.standardValue, standardValue, 10);
    //         memcpy(type.maxValue, maxValue, 10);
    //         memcpy(type.minValue, minValue, 10);
    //         type.hasTime = (bool)hasTime[0];
    //         // strcpy(type.standardValue,standardValue);
    //         // strcpy(type.maxValue,maxValue);
    //         // strcpy(type.minValue,minValue);
    //         type.timeseriesSpan = dtc.ToUInt32_m(timeseriesSpan);
    //         if (DataType::GetDataTypeFromStr(dataType, type) == 0)
    //         {
    //             if ((bool)hasTime[0] == true && !type.isTimeseries) //如果是时间序列，即使hasTime为1，依然不额外携带时间
    //                 type.hasTime = true;
    //             else
    //                 type.hasTime = false;
    //             dataTypes.push_back(type);
    //         }
    //         else
    //             return errorCode;
    //     }
    //     ZipTemplate tem(dataName, dataTypes, path);
    //     AddZipTemplate(tem);
    //     CurrentZipTemplate = tem;
    //     return 0;
    // }
    // 卸载指定路径下的模版
    static int UnsetZipTemplate(string path)
    {
        for (int i = 0; i < ZipTemplates.size(); i++)
        {
            if (ZipTemplates[i].path == path)
            {
                ZipTemplates.erase(ZipTemplates.begin() + i);
            }
        }
        CurrentZipTemplate.path = "";
        CurrentZipTemplate.schemas.clear();
        // CurrentZipTemplate.temFileBuffer = NULL;
        return 0;
    }
    // 检查是否已加载了模板
    static int CheckZipTemplate(string path)
    {
        if ((path != CurrentZipTemplate.path && path != "") || CurrentZipTemplate.path == "")
        {
            return SetZipTemplate(path.c_str());
        }
        return 0;
    }
};

// 负责数据文件的打包，打包后的数据将存为一个文件，文件名为时间段.pak，
// 格式为数据包头 + 每8字节时间戳，接20字节文件ID，接1字节表示是否为压缩文件(1:完全压缩，2:不完全压缩，0:非压缩），
// 如果长度不为0，则接4字节文件长度，接文件内容；
// 数据包头的格式暂定为：包中文件总数4字节 + 模版文件名20字节
class Packer
{
public:
    static int Pack(string pathToLine, vector<pair<string, long>> &filesWithTime);

    static int RePack(string pathToLine, int packThres = 0);
};

class PackFileReader
{
private:
    long curPos;     // 当前读到的位置
    long packLength; // pak长度
    bool usingcache; // 不使用缓存的阅读模式时，将回收此内存
    bool hasImg;

public:
    char *packBuffer = NULL; // pak缓存地址
    PackFileReader(){};
    PackFileReader(string pathFilePath);
    PackFileReader(char *buffer, long length, bool freeit = false);
    ~PackFileReader()
    {
        if (packBuffer != NULL && !usingcache)
            free(packBuffer);
        // cout << "buffer freed" << endl;
    }
    long Next(int &readLength, long &timestamp, string &fileID, int &zipType, char **buffer = nullptr, char *completeZiped = nullptr);

    long Next(int &readLength, long &timestamp, int &zipType, char **buffer = nullptr, char *completeZiped = nullptr);

    long Next(int &readLength, string &fileID, int &zipType, char **buffer = nullptr, char *completeZiped = nullptr);

    int Skip(int num);

    void ReadPackHead(int &fileNum, string &templateName);

    long GetPackLength()
    {
        return packLength;
    }

    void SetCurPos(long pos)
    {
        curPos = pos;
    }
};

/**
 * @brief 包文件的管理，将一部分包文件放于内存中，提高查询效率，按照LRU策略管理
 *
 */
class PackManager
{
private:
    long memUsed; // 已占用内存大小
    long memCapacity;
    list<pair<string, pair<char *, long>>> packsInMem;                               // 当前内存中的pack文件路径(key)-内存地址、长度(value)
    unordered_map<string, list<pair<string, pair<char *, long>>>::iterator> key2pos; // key到双向链表packsInMem中对应value的映射

public:
    PackManager(long memcap); // 初始化allPacks
    ~PackManager()
    {
        for (auto &pack : packsInMem)
        {
            free(pack.second.first);
        }
    }
    unordered_map<string, vector<pair<string, tuple<long, long>>>> allPacks; // 磁盘中当前所有目录下的所有包文件的路径、时间段,按照时间升序存放
    void PutPack(string path, pair<char *, long> pack);
    pair<char *, long> GetPack(string path);
    void ReadPack(string path);
    void DeletePack(string path);
    void UpdatePack(string path, string newPath, long start, long end);
    vector<pair<string, tuple<long, long>>> GetPacksByTime(string pathToLine, long start, long end);
    pair<string, pair<char *, long>> GetLastPack(string pathToLine, int index);
    pair<char *, long> GetPackByID(string pathToLine, string fileID, bool getpath = 0);
    vector<pair<char *, long>> GetPackByIDs(string pathToLine, string fileID, int num, bool getpath = 0);
    vector<pair<char *, long>> GetPackByIDs(string pathToLine, string fileIDStart, string fileIDEnd, bool getpath = 0);
    vector<pair<string, tuple<long, long>>> GetFilesInPacksByTime(string pathToLine, long start);
    void AddPack(string &pathToLine, string &path, long &start, long &end);
    void ModifyCacheCapacity(int memcap)
    {
        memCapacity = memcap;
    }
};

extern PackManager packManager;

// 产线文件夹命名规范统一为 xxxx/yyy
extern unordered_map<string, int> curNum; // 记录每个产线文件夹当前已有idb文件数
// 文件ID管理
// 根据总体目录结构发放文件ID
class FileIDManager
{
private:
public:
    // 建立从包到文件ID范围的索引
    unordered_map<string, tuple<int, int>> fidIndex;
    // 派发文件ID
    static string GetFileID(string path);
    static void GetSettings();
    static neb::CJsonObject GetSetting();

    long GetPacksRhythmNum(vector<pair<string, tuple<long, long>>> &packs);

    long GetPacksRhythmNum(vector<pair<char *, long>> &packs);
};
extern FileIDManager fileIDManager;

class QueryBufferReader
{
private:
    int cur;
    bool freeit;
    bool hasIMG;

public:
    char *buffer;
    int startPos;
    int rows;
    long length;
    int recordLength;
    vector<DataType> typeList;
    bool isBigEndian;
    void CheckBigEndian()
    {
        static int chk = 0x0201; // used to distinguish CPU's type (BigEndian or LittleEndian)
        isBigEndian = (0x01 != *(char *)(&chk));
    }
    QueryBufferReader()
    {
        buffer = NULL;
        CheckBigEndian();
    };
    QueryBufferReader(DB_DataBuffer *buffer, bool freeit = true); // 当freeit设置为false时，不会自动清空buffer
    ~QueryBufferReader()
    {
        if (freeit && buffer != NULL)
        {
            free(buffer);
            buffer = NULL;
        }
    }
    /**
     * @brief Equals to GetRow(). You could use [] operator to get the specified row.
     *
     * @param index
     * @return char*
     */
    char *operator[](const int &index);
    /**
     * @brief Get the element in specified index of the given memory address of the row.
     *
     * @param index Element index in the row
     * @param row Memory address of row
     * @return void*
     */
    void *FindElementInRow(const int &index, char *row);
    /**
     * @brief Get the element by row and col index. Note that this function is not suggested when you read a large number of data.
     *
     * @param row
     * @param col
     * @return void*
     */
    void *FindElement(const int &row, const int &col);
    /**
     * @brief Get the memory address of row by the specified index
     *
     * @param index
     * @return char*
     */
    char *GetRow(const int &index);
    /**
     * @brief Get the memory address of row by the specified index.This function is called when there is some data with uncertain length like image. It will return the memory address of the row you're going to read and tell you the length. Yet it may cause some speed loss accordingly.
     *
     * @param index
     * @return char*
     */
    char *GetRow(const int &index, int &rowLength);
    /**
     * @brief When you choose this to read the buffer, it means that you'll read every row in order.
     *  This function will return the memory address of the row you're going to read.
     *
     * @return char*
     */
    char *NextRow();
    /**
     * @brief When you choose this to read the buffer, it means that you'll read every row in order.
     *  This function is suggested to be called when there is some data with uncertain length like image. It will return the memory address of the row you're going to read and tell you the length. Yet it may cause some speed loss accordingly.
     *
     * @return char*
     */
    char *NextRow(int &rowLength);

    /**
     * @brief 重置当前读的位置
     *
     */
    void Reset() { cur = startPos; }

    /**
     * @brief Get the Pathcodes object from buffer head
     *
     * @return vector<PathCode>
     */
    void GetPathcodes(vector<PathCode> &pathCodes);
};
#endif