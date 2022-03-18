#include "../include/FS_header.h"
#include "../include/STDFB_header.h"
#include "../include/QueryRequest.hpp"
#include "../include/utils.hpp"
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


int EDVDB_LoadSchema(char path[])
{
    vector<string> files;
    readFileList(path,files);
    string temPath = "";
    for(string file : files)    //找到带有后缀tem的文件
    {
        string s = file;
        vector<string> vec = DataType::StringSplit(const_cast<char*>(s.c_str()),".");
        if(vec[vec.size()-1].find("tem")!=string::npos){
            temPath = file;
            break;
        }
    }
    if(temPath == "") return StatusCode::SCHEMA_FILE_NOT_FOUND;
    long length;
    EDVDB_GetFileLengthByPath(const_cast<char*>(temPath.c_str()), &length);
    char buf[length];
    int err = EDVDB_OpenAndRead(const_cast<char*>(temPath.c_str()), buf);
    if (err != 0)
        return err;
    int i = 0;
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
    Template tem(pathEncodes,dataTypes,path);
    return TemplateManager::SetTemplate(tem);
}
int EDVDB_UnloadSchema(char pathToUnset[])
{
    //return CurrentSchemaTemplate.UnsetTemplate();
}

int EDVDB_ExecuteQuery(DataBuffer *buffer, QueryParams *params)
{

}

int EDVDB_InsertRecord(DataBuffer *buffer, int addTime)
{
    long fp;
    
    int err = EDVDB_Open(buffer->savePath, "wb", &fp);
    if (err == 0)
    {
        err = EDVDB_Write(fp, buffer->buffer, buffer->length);
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

int EDVDB_InsertRecords(DataBuffer buffer[], int recordsNum, int addTime)
{
    for (int i = 0; i < recordsNum; i++)
    {
        long fp;
        int err = EDVDB_Open(buffer[i].savePath, "wb", &fp);
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
    long length;
    DataTypeConverter converter;
    converter.CheckBigEndian();
    cout << EDVDB_LoadSchema("./");
    //FILE *fp = fopen("exldata.tem", "rb");
    //long len;
    //EDVDB_GetFileLengthByFilePtr((long)fp, &len);
   //char buf[len];
    //EDVDB_Read((long)fp, buf);
    //cout << sizeof(buf) << endl;
    return 0;
}