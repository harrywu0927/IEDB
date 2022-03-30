#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <string.h>
#include "DataTypeConvert.hpp"
#include <sstream>
#include "../DB_FS/sources/CJsonObject.hpp"
#include "FS_header.h"
using namespace std;
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
static int errorCode; //错误码
//获取某一目录下的所有文件
//不递归子文件夹
void readFileList(string path, vector<string> &files);

void readIDBFilesList(string path, vector<string> &files);

void readIDBZIPFilesList(string path,vector<string> &files); 

void readIDBFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime);

long getMilliTime();

int getMemory(long size, char *mem);

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
    bool isArray;
    bool hasTime;
    int arrayLen;
    int valueBytes;
    ValueType::ValueType valueType;

    // string standardValue; //标准值
    // string maxValue;      //最大值
    // string minValue;      //最小值

    char maxValue[10];      //最大值
    char minValue[10];      //最小值
    char standardValue[10]; //标准值

    //实现分割字符串(源字符串也将改变)
    //@param srcstr     源字符串
    //@param str        划分字符
    static vector<string> StringSplit(char srcstr[], const char *str)
    {
        const char *d = str;
        char *p;
        p = strtok(srcstr, d);
        vector<string> res;
        while (p)
        {
            // printf("%s\n", p);
            res.push_back(p);
            p = strtok(NULL, d); // strtok内部记录了上次的位置
        }
        return res;
    }
    //判断值类型
    static ValueType::ValueType JudgeValueType(string vType)
    {
        if (vType == "INT")
            return ValueType::INT;
        else if (vType == "UINT")
            return ValueType::UINT;
        else if (vType == "DINT")
            return ValueType::DINT;
        else if (vType == "UDINT")
            return ValueType::UDINT;
        else if (vType == "REAL")
            return ValueType::REAL;
        else if (vType == "USINT")
            return ValueType::USINT;
        else if (vType == "TIME")
            return ValueType::TIME;
        else if (vType == "BOOL")
            return ValueType::BOOL;
        else if (vType == "SINT")
            return ValueType::SINT;
        else if (vType == "IMAGE")
            return ValueType::IMAGE;
        return ValueType::UNKNOWN;
    }
    //获取值类型所占字节数
    static int GetValueBytes(ValueType::ValueType &type)
    {
        switch (type)
        {
        case ValueType::INT: // int16
            return 2;
            break;
        case ValueType::UINT: // uint16
            return 2;
            break;
        case ValueType::SINT: // int8
            return 1;
            break;
        case ValueType::USINT: // uint8
            return 1;
            break;
        case ValueType::REAL: // float
            return 4;
            break;
        case ValueType::TIME: // int32
            return 4;
            break;
        case ValueType::DINT: // int32
            return 4;
            break;
        case ValueType::UDINT: // uint32
            return 4;
            break;
        case ValueType::BOOL: // int8
            return 1;
            break;
        case ValueType::IMAGE:
            return 1;
            break;
        default:
            return 0;
            break;
        }
    }
    //从字符串中获取数据类型
    static int GetDataTypeFromStr(char dataType[], DataType &type)
    {
        string dtype = dataType;
        if (dtype.find("ARRAY") != string::npos) //是数组
        {
            type.isArray = true;
            vector<string> arr = StringSplit(const_cast<char *>(dtype.c_str()), " ");
            type.valueType = JudgeValueType(arr[2]); //数组的值类型

            if (type.valueType == ValueType::USINT)
            {
                type.valueType = ValueType::IMAGE;
            }

            //获取数组长度
            arr[0][arr[0].length() - 1] = '\0';
            vector<string> vec = DataType::StringSplit(const_cast<char *>(arr[0].c_str()), "..");
            type.arrayLen = atoi(const_cast<char *>(vec[1].c_str()));
            type.valueBytes = GetValueBytes(type.valueType);
            return 0;
        }
        else
        {
            type.isArray = false;
            type.arrayLen = 0;
            type.valueType = JudgeValueType(dataType);
            type.valueBytes = GetValueBytes(type.valueType);
            return 0;
        }
        errorCode = StatusCode::UNKNOWN_TYPE;
        return StatusCode::UNKNOWN_TYPE;
    }

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
    static int CompareValue(DataType &type, char *compared, const char *toCompare)
    {
        if (type.isArray == 1)
        {
            return 0;
        }
        stringstream ss;
        ss << toCompare;
        int bytes = DataType::GetValueBytes(type.valueType);
        if (bytes == 0)
        {
            errorCode = StatusCode::UNKNOWN_TYPE;
            return StatusCode::UNKNOWN_TYPE;
        }
        char bufferV1[bytes];
        memcpy(bufferV1, compared, bytes);
        DataTypeConverter converter;
        switch (type.valueType)
        {
        case ValueType::INT:
        {
            short value1 = converter.ToInt16(compared);
            short value2 = 0;
            ss >> value2;
            return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
            break;
        }
        case ValueType::DINT:
        {
            int value1 = converter.ToInt32(bufferV1);
            int value2 = 0;
            ss >> value2;
            return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
            break;
        }
        case ValueType::UDINT:
        {
            uint value1 = converter.ToUInt32(compared);
            uint value2 = 0;
            ss >> value2;
            return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
            break;
        }
        case ValueType::UINT:
        {
            uint16_t value1 = converter.ToUInt16(compared);
            uint16_t value2 = 0;
            ss >> value2;
            return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
            break;
        }
        case ValueType::SINT:
        {
            int value1 = (int)compared[0];
            int value2 = 0;
            ss >> value2;
            return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
            break;
        }
        case ValueType::TIME:
        {
            int value1 = converter.ToInt32(compared);
            int value2 = 0;
            ss >> value2;
            return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
            break;
        }
        case ValueType::REAL:
        {
            float f1 = converter.ToFloat(compared);
            float f2 = 0;
            ss >> f2;
            return f1 == f2 ? 0 : (f1 > f2 ? 1 : -1);
            break;
        }

        default:
            return 0; //其他类型不作判断
            break;
        }
    }
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
    static int CompareValueInBytes(DataType &type, char *compared, const char *toCompare)
    {
        if (type.isArray == 1)
        {
            return 0;
        }
        int bytes = DataType::GetValueBytes(type.valueType);
        if (bytes == 0)
        {
            errorCode = StatusCode::UNKNOWN_TYPE;
            return StatusCode::UNKNOWN_TYPE;
        }
        char bufferV1[bytes];
        memcpy(bufferV1, compared, bytes);
        DataTypeConverter converter;
        switch (type.valueType)
        {
        case ValueType::INT:
        {
            short value1 = converter.ToInt16(compared);
            short value2 = converter.ToInt16(toCompare);
            return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
            break;
        }
        case ValueType::DINT:
        {
            int value1 = converter.ToInt32(bufferV1);
            int value2 = converter.ToInt32(toCompare);
            return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
            break;
        }
        case ValueType::UDINT:
        {
            uint value1 = converter.ToUInt32(compared);
            uint value2 = converter.ToUInt32(toCompare);
            return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
            break;
        }
        case ValueType::UINT:
        {
            uint16_t value1 = converter.ToUInt16(compared);
            uint16_t value2 = converter.ToUInt16(toCompare);
            return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
            break;
        }
        case ValueType::SINT:
        {
            int value1 = (int)compared[0];
            int value2 = (int)toCompare[0];
            return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
            break;
        }
        case ValueType::TIME:
        {
            int value1 = converter.ToInt32(compared);
            int value2 = converter.ToInt32(toCompare);
            return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
            break;
        }
        case ValueType::REAL:
        {
            float f1 = converter.ToFloat(compared);
            float f2 = converter.ToFloat(toCompare);
            return f1 == f2 ? 0 : (f1 > f2 ? 1 : -1);
            break;
        }

        default:
            return 0; //其他类型不作判断
            break;
        }
    }
};

class Template //标准模板
{
public:
    vector<pair<PathCode, DataType>> schemas;
    // unordered_map<vector<int>, string> pathNames;
    string path;         //挂载路径
    char *temFileBuffer; //模版文件缓存
    long fileLength;
    Template() {}
    Template(vector<PathCode> &pathEncodes, vector<DataType> &dataTypes, const char *path)
    {
        for (int i = 0; i < pathEncodes.size(); i++)
        {
            this->schemas.push_back(make_pair(pathEncodes[i], dataTypes[i]));
            // this->pathNames[pathEncodes[i].paths] = pathName[i];
        }
        this->path = path;
    }

    int GetAllPathsByCode(char *pachCode, vector<PathCode> &pathCodes);

    int FindDatatypePosByCode(char pathCode[], char buff[], long &position, long &bytes);

    int FindDatatypePosByCode(char pathCode[], char buff[], long &position, long &bytes, DataType &type);

    int FindMultiDatatypePosByCode(char pathCode[], char buff[], vector<long> &positions, vector<long> &bytes, vector<DataType> &types);

    int FindDatatypePosByName(const char *name, char buff[], long &position, long &bytes);

    int FindDatatypePosByName(const char *name, char buff[], long &position, long &bytes, DataType &type);

    int writeBufferHead(vector<PathCode> &pathCodes, vector<DataType> &typeList, char *buffer);

    int writeBufferHead(char *pathCode, vector<DataType> &typeList, char *buffer);

    int writeBufferHead(string name, DataType &type, char *buffer);
};

class ZipTemplate //压缩模板
{
public:
    vector<pair<string, DataType>> schemas;
    // unordered_map<vector<int>, string> pathNames;
    string path;         //挂载路径
    char *temFileBuffer; //模版文件缓存
    long fileLength;
    ZipTemplate() {}
    ZipTemplate(vector<string> &dataName, vector<DataType> &dataTypes, const char *path)
    {
        for (int i = 0; i < dataName.size(); i++)
        {
            this->schemas.push_back(make_pair(dataName[i], dataTypes[i]));
        }
        this->path = path;
    }
};

static char Label[100] = "./";

//文件ID管理
//根据总体目录结构发放文件ID
class FileIDManager
{
private:
public:
    //派发文件ID
    static string GetFileID(string path)
    {
        /*
            临时使用，今后需修改
            获取此路径下文件数量，以文件数量+1作为ID
        */
        vector<string> files;
        readFileList(path, files);
        return "XinFeng" + to_string(files.size() + 1) + "_";
    }
    static void GetSettings();
    static neb::CJsonObject GetSetting();
};
static neb::CJsonObject settings = FileIDManager::GetSetting();

static int maxTemplates = 20;
static vector<Template> templates;
static Template CurrentTemplate;
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
            type.hasTime = true;
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
    static int UnsetTemplate(string path)
    {
        for (int i = 0; i < templates.size(); i++)
        {
            if (templates[i].path == path)
            {
                templates.erase(templates.begin() + i);
            }
        }
        CurrentTemplate.path = "";
        CurrentTemplate.schemas.clear();
        CurrentTemplate.temFileBuffer = NULL;
        return 0;
    }
};

//负责数据文件的打包，打包后的数据将存为一个文件，文件名为文件ID+时间段.pak，
//格式为每8字节时间戳，接20字节文件ID，接8字节文件长度(0表示为压缩文件)，接文件内容；
//为方便查询，需要定义读取pak文件的辅助函数，将其转化为若干个独立文件列表暂存在内存中。
static int packMode;          //定时打包或存储一定数量后打包
static int packNum;           //一次打包的文件数量
static long packTimeInterval; //定时打包时间间隔
class Packer
{
public:
    static int Pack(string path)
    {
    }
};

static vector<ZipTemplate> ZipTemplates;
static ZipTemplate CurrentZipTemplate;
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
