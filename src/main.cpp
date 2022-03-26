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

int EDVDB_QueryByFileID2(DataBuffer *buffer, QueryParams *params)
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
            long bytes = 1;
            CurrentTemplate.FindDatatypePos(pathCode,pos,bytes);
            
            char *data = (char *)malloc(bytes);
            if (data == NULL)
            {
                buffer->buffer = NULL;
                buffer->bufferMalloced = 0;
            }
            //内存分配成功，传入数据
            buffer->bufferMalloced = 1;
            buffer->length = bytes;
            memcpy(data,buff+pos,bytes);
            buffer->buffer = data;
            return 0;

            break;
        }
    }
    return StatusCode::DATAFILE_NOT_FOUND;
}

int EDVDB_QueryByFileID(DataBuffer *buffer, QueryParams *params)
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

int EDVDB_InsertRecord(DataBuffer *buffer, int addTime)
{
    long fp;
    time_t curtime;
    time(&curtime);
    string fileID = FileIDManager::GetFileID(buffer->savePath);
    string finalPath = "";
    string time = ctime(&curtime);
    time.pop_back();
    finalPath = finalPath.append(buffer->savePath).append("/").append(fileID).append(time).append(".idb");
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

int EDVDB_InsertRecords(DataBuffer buffer[], int recordsNum, int addTime)
{
    for (int i = 0; i < recordsNum; i++)
    {
        long fp;
        int err = EDVDB_Open(const_cast<char *>(buffer[i].savePath), "wb", &fp);
        if (err == 0)
        {
            err = EDVDB_Write(fp, buffer[i].buffer, buffer[i].length);
            if (err == 0)
            {
                return EDVDB_Close(fp);
            }
        }
        else
        {
            return err;
        }
    }
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
    EDVDB_ZipFile("./","XinFeng_0100.dat");
    //cout << EDVDB_LoadZipSchema("./");
    //cout << EDVDB_LoadSchema("./");
    return 0;
}