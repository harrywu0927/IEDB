#include "../include/utils.hpp"
//#include "STDFB_header.h"
//获取某一目录下的所有文件
//不递归子文件夹
void readFileList(string path, vector<string> &files)
{
    // FileIDManager::GetSettings();
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    if (DB_CreateDirectory(const_cast<char *>(finalPath.c_str())))
    {
        errorCode = errno;
        return;
    }
    dir = opendir(finalPath.c_str());
    // cout<<finalPath<<endl;
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            files.push_back(p.append(path).append("/").append(ptr->d_name));
        }
    }
    closedir(dir);
}

//获取.idb文件路径
void readIDBFilesList(string path, vector<string> &files)
{
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    if (DB_CreateDirectory(const_cast<char *>(finalPath.c_str())))
    {
        errorCode = errno;
        return;
    }
    dir = opendir(finalPath.c_str());
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;
            if ((datafile.find(".idb") != string::npos) && (datafile.find(".idbzip") == string::npos))
                files.push_back(p.append(path).append("/").append(ptr->d_name));
        }
    }
    closedir(dir);
}

//获取.idbzip文件路径
void readIDBZIPFilesList(string path, vector<string> &files)
{
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    finalPath += path;
    dir = opendir(finalPath.c_str());
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;

            if (datafile.find(".idbzip") != string::npos)
                files.push_back(p.append(path).append("/").append(ptr->d_name));
        }
    }
    closedir(dir);
}

/**
 * @brief 从指定文件夹中获取所有数据文件，并从文件名中取得时间戳
 * @param path             路径
 * @param filesWithTime    带有时间戳的文件名列表
 *
 * @return
 * @note
 */
void readDataFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime)
{
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    dir = opendir(finalPath.c_str());
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;
            if (datafile.find(".idbzip") != string::npos || datafile.find(".idb") != string::npos)
            {
                string tmp = datafile;
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
                t.tm_isdst = -1; //不设置夏令时
                time_t seconds = mktime(&t);
                int ms = atoi(time[6].c_str());
                long millis = seconds * 1000 + ms;
                filesWithTime.push_back(make_pair(p.append(path).append("/").append(ptr->d_name), millis));
            }
        }
    }
    closedir(dir);
}

/**
 * @brief 从指定文件夹中获取所有.idb文件，并从文件名中取得时间戳
 * @param path             路径
 * @param filesWithTime    带有时间戳的文件名列表
 *
 * @return
 * @note
 */
void readIDBFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime)
{
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    dir = opendir(finalPath.c_str());
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;
            if (datafile.find(".idbzip") == string::npos && datafile.find(".idb") != string::npos)
            {
                string tmp = datafile;
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
                t.tm_isdst = -1; //不设置夏令时
                time_t seconds = mktime(&t);
                int ms = atoi(time[6].c_str());
                long millis = seconds * 1000 + ms;
                filesWithTime.push_back(make_pair(p.append(path).append("/").append(ptr->d_name), millis));
            }
        }
    }
    closedir(dir);
}

void readPakFilesList(string path, vector<string> &files)
{
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    if (DB_CreateDirectory(const_cast<char *>(finalPath.c_str())))
    {
        errorCode = errno;
        return;
    }
    dir = opendir(finalPath.c_str());
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;
            if (datafile.find(".pak") != string::npos)
                files.push_back(p.append(path).append("/").append(ptr->d_name));
        }
    }
    closedir(dir);
}

void FileIDManager::GetSettings()
{
    ifstream t("./settings.json");
    stringstream buffer;
    buffer << t.rdbuf();
    string contents(buffer.str());
    neb::CJsonObject tmp(contents);
    settings = tmp;
    strcpy(Label, settings("Filename_Label").c_str());
}
neb::CJsonObject FileIDManager::GetSetting()
{
    ifstream t("./settings.json");
    stringstream buffer;
    buffer << t.rdbuf();
    string contents(buffer.str());
    neb::CJsonObject tmp(contents);
    strcpy(Label, settings("Filename_Label").c_str());
    packMode = settings("Pack_Mode");
    packNum = atoi(settings("Pack_Num").c_str());
    packTimeInterval = atol(settings("Pack_Interval").c_str());
    return tmp;
}

string FileIDManager::GetFileID(string path)
{
    /*
        临时使用，今后需修改
        获取此路径下文件数量，以文件数量+1作为ID
    */
    string tmp = path;
    vector<string> paths = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
    string prefix = "Default";
    if (paths.size() > 0)
        prefix = paths[paths.size() - 1];
    cout << prefix << endl;
    //去除首尾的‘/’，使其符合命名规范
    if (path[0] == '/')
        path.erase(path.begin());
    if (path[path.size() - 1] == '/')
        path.pop_back();

    if (!filesListRead[path])
    {
        cout << "file list not read" << endl;
        vector<string> files;
        readFileList(path, files);
        filesListRead[path] = true;
        curNum[path] = files.size();
        cout << "now file num :" << curNum[path] << endl;
    }
    curNum[path]++;
    if (curNum[path] % 1 == 0)
    {
        Packer packer;
        vector<pair<string, long>> filesWithTime;
        readDataFilesWithTimestamps(path, filesWithTime);
        packer.Pack("/", filesWithTime);
    }
    return prefix + to_string(curNum[path] + 1) + "_";
}

/**
 * @brief 获取绝对时间(自1970/1/1至今),精确到毫秒
 *
 * @return  时间值
 * @note
 */
long getMilliTime()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * 1000 + time.tv_usec / 1000;
}

int getMemory(long size, char *mem)
{
    mem = (char *)malloc(size);
    if (mem == NULL)
    {
        errorCode = StatusCode::BUFFER_FULL;
        return errorCode;
    }
    return 0;
}

int getBufferValueType(DataType &type)
{
    int baseType = (int)type.valueType;
    if (type.hasTime && !type.isArray)
        return baseType + 10;
    if (!type.hasTime && type.isArray)
        return baseType + 20;
    if (type.isArray && type.hasTime)
        return baseType + 30;
    return baseType;
}

/**
 * @brief 获取数据区中第N个变量在每行中的偏移量
 * @param typeList      数据类型列表
 * @param num           第num个
 *
 * @return  偏移量
 * @note
 */
int getBufferDataPos(vector<DataType> &typeList, int num)
{
    long pos = 0;
    for (int i = 0; i < num; i++)
    {
        pos += typeList[i].hasTime ? typeList[i].valueBytes + 8 : typeList[i].valueBytes;
    }

    return pos;
}

int main()
{
    // FileIDManager::GetFileID("/");
    // FileIDManager::GetFileID("/");
    // FileIDManager::GetFileID("/Jinfei3");
    // FileIDManager::GetFileID("/Jinfei3");
    // FileIDManager::GetFileID("/Jinfei4/line1");
    // FileIDManager::GetFileID("/Jinfei4/line1/");
    PackFileReader packReader("1648812529807-1648812625883.pak");
    int fileNum;
    string templateName;
    packReader.ReadPackHead(fileNum, templateName);
    // TemplateManager::CheckTemplate(temp)
    for (size_t i = 0; i < fileNum; i++)
    {
        long timestamp;
        int readLength;
        string fileID;
        long datapos = packReader.Next(readLength, timestamp, fileID);
    }

    return 0;

    Packer packer;
    vector<pair<string, long>> files;
    readDataFilesWithTimestamps("", files);
    packer.Pack("", files);
    return 0;
}
