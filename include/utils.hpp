#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <dirent.h>
#ifdef WIN32
#include <windows.h>
#include <timeb.h>
#include <winsock2.h>
#else
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <pthread.h>
#endif
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <memory>
#include <stack>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include "DataTypeConvert.hpp"
#include "CassFactoryDB.h"
#include <sstream>
#include "CJsonObject.hpp"
#include <thread>
#include <future>
#include <chrono>
using namespace std;
#pragma once

#define PACK_MAX_SIZE 1024 * 1024 * 5

#ifdef WIN32
typedef long long int long;
typedef unsigned int uint;
typedef unsigned short ushort;
#endif

namespace StatusCode
{
    enum StatusCode
    {
        DATA_TYPE_MISMATCH_ERROR = 135,  //数据类型不匹配
        SCHEMA_FILE_NOT_FOUND = 136,     //未找到模版文件
        PATHCODE_INVALID = 137,          //路径编码不合法
        UNKNOWN_TYPE = 138,              //未知数据类型
        TEMPLATE_RESOLUTION_ERROR = 139, //模版文件解析错误
        DATAFILE_NOT_FOUND = 140,        //未找到数据文件
        UNKNOWN_PATHCODE = 141,          //未知的路径编码
        UNKNOWN_VARIABLE_NAME = 142,     //未知的变量名
        BUFFER_FULL = 143,               //缓冲区满，还有数据
        UNKNWON_DATAFILE = 144,          //未知的数据文件或数据文件不合法
        NO_QUERY_TYPE = 145,             //未指定主查询条件
        QUERY_TYPE_NOT_SURPPORT = 146,   //不支持的查询方式
        EMPTY_PATH_TO_LINE = 147,        //到产线的路径为空
        EMPTY_SAVE_PATH = 148,           //空的存储路径
        NO_DATA_QUERIED = 149,           //未找到数据
        VARIABLE_NAME_EXIST = 150,       //变量名已存在
        PATHCODE_EXIST = 151,            //编码已存在
        FILENAME_MODIFIED = 152,         //文件名被篡改
        INVALID_TIMESPAN = 153,          //时间段设置错误或未指定
        NO_PATHCODE_OR_NAME = 154,       //查询参数中路径编码和变量名均未找到
        NO_QUERY_NUM = 155,              //查询最新记录时未指定查询条数
        NO_FILEID = 156,                 //未指定文件ID
        VARIABLE_NOT_ASSIGNED = 157,     //比较某个值时未指定变量名
        NO_COMPARE_VALUE = 158,          //指定了比较类型却没有赋值
        ZIPTYPE_ERROR = 159,             //压缩数据类型出现问题
        NO_ZIP_TYPE = 160,               //未指定压缩/还原条件
        ISARRAY_ERROR = 161,             // isArray只能为0或者1
        HASTIME_ERROR = 162,             // hasTime只能为0或者1
        ARRAYLEN_ERROR = 163,            // arrayLen不能小于１
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

extern int errorCode; //错误码

extern int maxThreads; //此设备支持的最大线程数

extern neb::CJsonObject settings;
//获取某一目录下的所有文件
//不递归子文件夹
void readFileList(string path, vector<string> &files);

void readIDBFilesList(string path, vector<string> &files);

void readIDBZIPFilesList(string path, vector<string> &files);

void readTEMFilesList(string path,vector<string> &files);

void readZIPTEMFilesList(string path,vector<string> &files);

void readIDBFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime);

void readIDBZIPFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime);

void readDataFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime);

void readDataFiles(string path, vector<string> &files);

void readPakFilesList(string path, vector<string> &files);

void readAllDirs(vector<string> &dirs, string basePath);

long getMilliTime();

void removeFilenameLabel(string &path);

void sortByTime(vector<pair<string, long>> &selectedFiles, DB_Order order);

int getMemory(long size, char *mem);

int CheckQueryParams(DB_QueryParams *params);

int CheckZipParams(DB_ZipParams *params);

int ReZipBuff(char *buff, int &buffLength, const char *pathToLine);

bool IsNormalIDBFile(char *readbuff, const char *pathToLine);

int sysOpen(char path[]);

//根据时间升序或降序排序
void sortByTime(vector<pair<string, long>> &selectedFiles, DB_Order order);

class PathCode
{
public:
    vector<int> paths;
    string name;
    char code[10];
    //解码路径为整数数组
    vector<int> DecodePath(char pathEncode[], int &levels)
    {
        DataTypeConverter converter;
        vector<int> res;
        for (int i = 0; i < levels; i++)
        {
            string str = "  ";
            str[0] = pathEncode[i * 2];
            str[1] = pathEncode[i * 2 + 1];
            res.push_back((int)converter.ToUInt16(const_cast<char *>(str.c_str())));
        }
        return res;
    }
    PathCode(char pathEncode[], int levels, string name)
    {
        this->paths = DecodePath(pathEncode, levels);
        this->name = name;
        for (size_t i = 0; i < 10; i++)
        {
            this->code[i] = pathEncode[i];
        }
    }
};

class DataType
{
public:
    bool isArray = false;
    bool hasTime = false;
    int arrayLen;
    int valueBytes;
    ValueType::ValueType valueType;

    char maxValue[10];      //最大值
    char minValue[10];      //最小值
    char standardValue[10]; //标准值

    //实现分割字符串(源字符串也将改变)
    //@param srcstr     源字符串
    //@param str        划分字符
    static vector<string> StringSplit(char srcstr[], const char *str);

    static vector<string> splitWithStl(const string &str, const string &pattern);
    //判断值类型
    static ValueType::ValueType JudgeValueType(string vType);
    //判断值类型
    static string JudgeValueTypeByNum(int vType);
    //获取值类型所占字节数
    static int GetValueBytes(ValueType::ValueType &type);
    //从字符串中获取数据类型
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

int getBufferValueType(DataType &type);

class Template //标准模板
{
public:
    vector<pair<PathCode, DataType>> schemas;
    string path;         //挂载路径
    char *temFileBuffer; //模版文件缓存
    long fileLength;
    long totalBytes;
    Template() {}
    Template(vector<PathCode> &pathEncodes, vector<DataType> &dataTypes, const char *path)
    {
        for (int i = 0; i < pathEncodes.size(); i++)
        {
            this->schemas.push_back(make_pair(pathEncodes[i], dataTypes[i]));
        }
        this->path = path;
        this->totalBytes = GetTotalBytes();
    }

    int GetAllPathsByCode(char *pachCode, vector<PathCode> &pathCodes);

    bool checkHasArray(char *pathCode);

    int FindDatatypePosByCode(char pathCode[], char buff[], long &position, long &bytes);

    int FindDatatypePosByCode(char pathCode[], char buff[], long &position, long &bytes, DataType &type);

    int FindMultiDatatypePosByCode(char pathCode[], char buff[], vector<long> &positions, vector<long> &bytes, vector<DataType> &types);

    int FindDatatypePosByName(const char *name, char buff[], long &position, long &bytes);

    int FindDatatypePosByName(const char *name, char buff[], long &position, long &bytes, DataType &type);

    int writeBufferHead(vector<PathCode> &pathCodes, vector<DataType> &typeList, char *buffer);

    int writeBufferHead(char *pathCode, vector<DataType> &typeList, char *buffer);

    int writeBufferHead(string name, DataType &type, char *buffer);

    long FindSortPosFromSelectedData(vector<long> &bytesList, string name, char *pathCode, vector<DataType> &typeList);

    long GetTotalBytes();
};

int getBufferDataPos(vector<DataType> &typeList, int num);

extern Template CurrentTemplate;
extern int maxTemplates;
extern vector<Template> templates;

//模版的管理
//内存中可同时存在若干数量的模版以提升存取效率，可按照LRU策略管理模版
class TemplateManager
{
private:
public:
    static void AddTemplate(Template &tem)
    {
    }
    static void ModifyMaxTemplates(int n)
    {
        maxTemplates = n;
    }
    //将模版设为当前模版
    static int SetTemplate(const char *path)
    {
        vector<string> files;
        readFileList(path, files);
        string temPath = "";
        for (string file : files) //找到带有后缀tem的文件
        {
            string s = file;
            vector<string> vec = DataType::StringSplit(const_cast<char *>(s.c_str()), ".");
            if (vec[vec.size() - 1].find("tem") != string::npos && vec[vec.size() - 1].find("ziptem") == string::npos)
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
            char timeFlag = buf[++i];
            memcpy(pathEncode, buf + i, 10);
            i += 10;
            vector<string> paths;
            PathCode pathCode(pathEncode, sizeof(pathEncode) / 2, variable);
            DataType type;
            if ((int)timeFlag == 1)
                type.hasTime = true;
            else
                type.hasTime = false;
            if (DataType::GetDataTypeFromStr(dataType, type) == 0)
            {
                dataTypes.push_back(type);
            }
            else
                return errorCode;
            pathEncodes.push_back(pathCode);
        }
        Template tem(pathEncodes, dataTypes, path);
        AddTemplate(tem);
        CurrentTemplate = tem;
        return 0;
    }

    //卸载指定路径下的模版
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

class ZipTemplate //压缩模板
{
public:
    vector<pair<string, DataType>> schemas;
    // unordered_map<vector<int>, string> pathNames;
    string path;         //挂载路径
    char *temFileBuffer; //模版文件缓存
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

static char Label[100] = "./";

//负责数据文件的打包，打包后的数据将存为一个文件，文件名为时间段.pak，
//格式为数据包头 + 每8字节时间戳，接20字节文件ID，接1字节表示是否为压缩文件(1:完全压缩，2:不完全压缩，0:非压缩），
//如果长度不为0，则接4字节文件长度，接文件内容；
//数据包头的格式暂定为：包中文件总数4字节 + 模版文件名20字节
class Packer
{
public:
    static int Pack(string pathToLine, vector<pair<string, long>> &filesWithTime);

    static int RePack(string pathToLine);
};

class PackFileReader
{
private:
    long curPos;     //当前读到的位置
    long packLength; // pak长度
    bool usingcache;

public:
    char *packBuffer = NULL; // pak缓存地址
    PackFileReader(string pathFilePath)
    {
        DB_DataBuffer buffer;
        buffer.savePath = pathFilePath.c_str();
        int err = DB_ReadFile(&buffer);
        if (buffer.bufferMalloced)
        {
            packBuffer = buffer.buffer;
            buffer.buffer = NULL;
            packLength = buffer.length;
            curPos = 24;
            usingcache = false;
        }
        else
        {
            packBuffer = NULL;
        }
        if (err != 0)
            errorCode = err;
    }
    PackFileReader(char *buffer, long length)
    {
        packBuffer = buffer;
        buffer = NULL;
        packLength = length;
        curPos = 24;
        usingcache = true;
    }
    ~PackFileReader()
    {
        if (packBuffer != NULL && !usingcache)
            free(packBuffer);
        // cout << "buffer freed" << endl;
    }
    long Next(int &readLength, long &timestamp, string &fileID, int &zipType);

    long Next(int &readLength, long &timestamp, int &zipType);

    long Next(int &readLength, string &fileID, int &zipType);

    void Skip(int index);

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
    long memUsed; //已占用内存大小
    long memCapacity;
    list<pair<string, pair<char *, long>>> packsInMem; //当前内存中的pack文件路径-内存地址、长度
    unordered_map<string, list<pair<string, pair<char *, long>>>::iterator> key2pos;

public:
    PackManager(long memcap) //初始化allPacks
    {
        vector<string> packList, dirs;

        readAllDirs(dirs, settings("Filename_Label"));
        for (auto &d : dirs)
        {
            DIR *dir = opendir(d.c_str());
            struct dirent *ptr;
            if (dir == NULL)
                continue;
            while ((ptr = readdir(dir)) != NULL)
            {
                if (ptr->d_name[0] == '.')
                    continue;
                if (ptr->d_type == 8)
                {
                    string fileName = ptr->d_name;
                    string dirWithoutPrefix = d + "/" + fileName;
                    string fileLabel = settings("Filename_Label");
                    while (fileLabel[fileLabel.length() - 1] == '/')
                    {
                        fileLabel.pop_back();
                    }
                    for (int i = 0; i <= fileLabel.length(); i++)
                    {
                        dirWithoutPrefix.erase(dirWithoutPrefix.begin());
                    }
                    string pathToLine = DataType::splitWithStl(dirWithoutPrefix, "/")[0];
                    if (fileName.find(".pak") != string::npos)
                    {
                        string str = d;
                        for (int i = 0; i <= settings("Filename_Label").length(); i++)
                        {
                            str.erase(str.begin());
                        }
                        string tmp = fileName;
                        while (tmp.back() == '/')
                            tmp.pop_back();
                        vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
                        string packName = vec[vec.size() - 1];
                        vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
                        if (timespan.size() > 0)
                        {
                            long start = atol(timespan[0].c_str());
                            long end = atol(timespan[1].c_str());
                            allPacks[pathToLine].push_back(make_pair(str + "/" + fileName, make_tuple(start, end)));
                        }
                    }
                }
            }
            closedir(dir);
        }
        for (auto &iter : allPacks)
        {
            sort(iter.second.begin(), iter.second.end(),
                 [](pair<string, tuple<long, long>> iter1, pair<string, tuple<long, long>> iter2) -> bool
                 {
                     return get<0>(iter1.second) < get<0>(iter2.second);
                 });
        }

        memCapacity = memcap;
    }
    ~PackManager()
    {
        for (auto &pack : packsInMem)
        {
            free(pack.second.first);
        }
    }
    unordered_map<string, vector<pair<string, tuple<long, long>>>> allPacks; //磁盘中当前所有目录下的所有包文件的路径、时间段,按照时间升序存放
    void PutPack(string path, pair<char *, long> pack);

    pair<char *, long> GetPack(string path);

    void ReadPack(string path);

    vector<pair<string, pair<char *, long>>> GetPacksByTime(string pathToLine, long start, long end);

    pair<string, pair<char *, long>> GetLastPack(string pathToLine, int index);

    pair<char *, long> GetPackByID(string pathToLine, string fileID);
};

extern PackManager packManager;

//产线文件夹命名规范统一为 xxxx/yyy
extern unordered_map<string, int> curNum; //记录每个产线文件夹当前已有idb文件数
// extern unordered_map<string, bool> filesListRead; //记录每个产线文件夹是否已读取过文件列表
//文件ID管理
//根据总体目录结构发放文件ID
class FileIDManager
{
private:
public:
    //建立从包到文件ID范围的索引
    unordered_map<string, tuple<int, int>> fidIndex;
    //派发文件ID
    static string GetFileID(string path);
    static void GetSettings();
    static neb::CJsonObject GetSetting();
};
extern FileIDManager fileIDManager;

extern vector<ZipTemplate> ZipTemplates;
extern ZipTemplate CurrentZipTemplate;
class ZipTemplateManager
{
private:
public:
    static void AddTemplate(ZipTemplate &tem)
    {
    }
    static void ModifyMaxTemplates(int n)
    {
        maxTemplates = n;
    }
    //将模版设为当前模版
    static int SetTemplate(ZipTemplate &tem)
    {
        CurrentZipTemplate = tem;
        return 0;
    }
    //卸载指定路径下的模版
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
        CurrentZipTemplate.temFileBuffer = NULL;
        return 0;
    }
};

//用于程序内部交换的数据集
struct DataSet
{
    vector<string> nameList;
    vector<string> valueList;
    vector<DataType> typeList;
    vector<int> timeList;
};
