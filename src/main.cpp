#include "FS_header.h"
#include "STDFB_header.h"
#include "DataTypeConvert.hpp"
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
namespace StatusCode
{
    enum StatusCode
    {
        DATA_TYPE_MISMATCH_ERROR = 135,  //数据类型不匹配
        SCHEMA_FILE_NOT_FOUND = 136,     //未找到模版文件
        PATHENCODE_INVALID = 137,        //路径编码不合法
        UNKNOWN_TYPE = 138,              //未知数据类型
        TEMPLATE_RESOLUTION_ERROR = 139, //模版文件解析错误
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
extern int errno;

class PathCode
{
public:
    vector<int> paths;
    string name;
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
            res.push_back((int)converter.ToUInt16(str));
        }
        return res;
    }
    PathCode(char pathEncode[], int levels, string name)
    {
        this->paths = DecodePath(pathEncode, levels);
        this->name = name;
    }
};

class DataType
{
public:
    bool isArray;
    int arrayLen;
    int valueBytes;
    ValueType::ValueType valueType;
    string standardValue; //标准值
    string maxValue;      //最大值
    string minValue;      //最小值
    //实现分割字符串
    static vector<string> StringSplit(char srcstr[], char str[])
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
            type.valueType = JudgeValueType(dataType);
            type.valueBytes = GetValueBytes(type.valueType);
            return 0;
        }
        return StatusCode::UNKNOWN_TYPE;
    }
};
class Template
{
public:
    vector<pair<PathCode, DataType> > schemas;
    // unordered_map<vector<int>, string> pathNames;
    string path; //挂载路径
    //挂载模版
    int SetTemplate(vector<PathCode> &pathEncodes, vector<DataType> &dataTypes)
    {
        if (pathEncodes.size() != dataTypes.size())
        {
            return StatusCode::TEMPLATE_RESOLUTION_ERROR;
        }
        for (int i = 0; i < pathEncodes.size(); i++)
        {
            this->schemas.push_back(make_pair(pathEncodes[i], dataTypes[i]));
            // this->pathNames[pathEncodes[i].paths] = pathName[i];
        }
        return 0;
    }

    //卸载当前模版
    int UnsetTemplate()
    {
        // this->pathNames.clear();
        this->schemas.clear();
        return 0;
    }

    //根据路径编码寻找数据在模版文件中的位置
    long FindDatatypePos(PathCode &pathCode)
    {
    }
} SchemaTemplate;

struct DataSet
{
    vector<string> nameList;
    vector<string> valueList;
    vector<DataType> typeList;
    vector<int> timeList;
};

int EDVDB_LoadSchema(char path[])
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        if (errno == 2)
            return StatusCode::SCHEMA_FILE_NOT_FOUND;
        else
        {
            cout << "Error while loading schema file, Code " << errno << ":" << strerror(errno);
            return errno;
        }
    }
    long length;
    cout << EDVDB_GetFileLengthByFilePtr((long)fp, &length) << endl;
    char buf[length];
    if (EDVDB_Read((long)fp, buf) != 0)
        return errno;
    int i = 0;
    // vector<string> variableNames;
    vector<PathCode> pathEncodes;
    vector<DataType> dataTypes;
    while (i < length)
    {
        char variable[30], dataType[30], pathEncode[6];
        memcpy(variable, buf + i, 30);
        i += 30;
        memcpy(dataType, buf + i, 30);
        i += 30;
        memcpy(pathEncode, buf + i, 6);
        i += 6;
        vector<string> paths;
        PathCode pathCode(pathEncode, sizeof(pathEncode) / 2, variable);
        DataType type;
        if (DataType::GetDataTypeFromStr(dataType, type) == 0)
        {
            dataTypes.push_back(type);
        }
        pathEncodes.push_back(pathCode);
    }
    return SchemaTemplate.SetTemplate(pathEncodes, dataTypes);
}
int EDVDB_UnloadSchema()
{
    return SchemaTemplate.UnsetTemplate();
}
int EDVDB_MAX(DataBuffer *buffer)
{
}
int EDVDB_MAX(DataBuffer *buffer, char pathcode[], int len)
{
}

// char* testQuery(long *len)
// {
//     char* buffer = new char[50];
//     for(int i=0;i<50;i++){
//         buffer[i] = 'a';
//     }
//     cout<<buffer<<endl;
//     *len = 50;
//     return buffer;
// }
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
    long length;
    // char* buff = testQuery(&length);
    // for (size_t i = 0; i < length; i++)
    // {
    //     cout<<buff[i];
    // }
    DataTypeConverter converter;
    converter.CheckBigEndian();
    cout << EDVDB_LoadSchema("/Users/harrywu/Documents/StdFunctions/exldata.tem");
    FILE *fp = fopen("exldata.tem", "rb");
    long len;
    EDVDB_GetFileLengthByFilePtr((long)fp, &len);
    char buf[len];
    EDVDB_Read((long)fp, buf);
    cout << sizeof(buf) << endl;
    return 0;
}