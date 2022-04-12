#include <CassFactoryDB.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <dirent.h>
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/statvfs.h>
#endif
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <queue>
#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <CJsonObject.hpp>
using namespace std;
#ifdef WIN32
typedef long long int long;
#endif
// extern int errno;
neb::CJsonObject ReadConfig();
static neb::CJsonObject config = ReadConfig();
long availableSpace = 1024 * 1024 * 10;
size_t totalSpace = 0;
char labelPath[100] = "./";

void SetBasePath(char path[])
{
    strcpy(labelPath, path);
}
neb::CJsonObject ReadConfig()
{
    ifstream t("./settings.json");
    stringstream buffer;
    buffer << t.rdbuf();
    string contents(buffer.str());
    neb::CJsonObject tmp(contents);
    strcpy(labelPath, config("Filename_Label").c_str());
    return tmp;
}
int get_file_size_time(const char *filename, pair<long, long> *s_t)
{
    struct stat statbuf;
    if (stat(filename, &statbuf) == -1)
    {
        perror("Error while getting file size and time");
        return (-1);
    }
    if (S_ISDIR(statbuf.st_mode)) //是文件夹
        return (1);
    if (S_ISREG(statbuf.st_mode)) //是常规文件
    {
        *s_t = make_pair(statbuf.st_size, statbuf.st_mtime);
    }

    return (0);
}

void getDiskSpaces()
{
#ifdef WIN32
DWORD64 qwFreeBytesToCaller;
bool bResult = GetDiskFreeSpaceEx(TEXT("C:"), 
         (PULARGE_INTEGER)&qwFreeBytesToCaller, 
         (PULARGE_INTEGER)&totalSpace, 
         (PULARGE_INTEGER)&availableSpace);
#else
    struct statvfs diskInfo;
    statvfs("./", &diskInfo);
    availableSpace = diskInfo.f_bavail * diskInfo.f_frsize - 1024 * 1024 * 5; //可用空间
    if (availableSpace < 0)
        availableSpace = 0;
    totalSpace = diskInfo.f_frsize * diskInfo.f_blocks; //总空间
#endif
}

int readFileList(char *basePath, vector<string> *files)
{
    DIR *dir;
    struct dirent *ptr;
    if ((dir = opendir(basePath)) == NULL)
    {
        perror("Error while opening directory");
        return 0;
    }

    while ((ptr = readdir(dir)) != NULL)
    {
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0) /// current dir OR parrent dir
            continue;
        else if (ptr->d_type == 8) /// file
        {
            string base = basePath;
            base += "/";
            base += ptr->d_name;
            files->push_back(base);
        }
        else if (ptr->d_type == 10) /// link file
            printf("link file:%s/%s\n", basePath, ptr->d_name);
        else if (ptr->d_type == 4) /// dir
        {
            string base = basePath;
            base += "/";
            base += ptr->d_name;
            files->push_back(base);
            readFileList(const_cast<char *>(base.c_str()), files);
        }
    }
    closedir(dir);
    return 1;
}

queue<string> InitFileQueue()
{
    vector<string> files;
    readFileList(labelPath, &files);
    vector<pair<string, pair<long, long>>> vec; //将键值对保存在数组中以便排序
    for (string &file : files)
    {
        pair<long, long> s_t;
        if (get_file_size_time(file.c_str(), &s_t) == 0)
        {
            vec.push_back(make_pair(file, make_pair(s_t.first, s_t.second)));
        }
    }

    //按照时间升序排序
    sort(vec.begin(), vec.end(),
         [](pair<string, pair<long, long>> iter1, pair<string, pair<long, long>> iter2) -> bool
         {
             return iter1.second.second < iter2.second.second;
         });
    queue<string> que;
    for (auto &file : files)
    {
        que.push(file);
    }
    return que;
}

queue<string> fileQueue = InitFileQueue();

int DB_GetFileLengthByPath(char path[], long *length)
{
    // ReadConfig();
    char finalPath[100];
    strcpy(finalPath, labelPath);
    strcat(finalPath, "/");
    strcat(finalPath, path);
    FILE *fp = fopen(finalPath, "r");
    if (fp == 0)
    {
        cout << finalPath << endl;
        perror("Error while getting file length");
        return errno;
    }
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        perror("Error while getting file length");
        return errno;
    }

    long len = ftell(fp);

    *length = len;
    return fclose(fp) == 0 ? 0 : errno;
}
int DB_GetFileLengthByFilePtr(long fileptr, long *length)
{
    FILE *fp = (FILE *)fileptr;
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        perror("Error while getting file length");
        return errno;
    }
    long len = ftell(fp);
    *length = len;
    return 0;
}
bool LoopMode(char buf[], long length)
{
    DIR *dir;
    struct dirent *ptr;
    vector<string> files;
    // char *path = "./";
    if (readFileList(labelPath, &files) == 1)
    {
        long needSpace = length + 1;
        if (needSpace > availableSpace) //空间不足
        {
            cout << "Need space:" << needSpace / 1024 << "KB Available:" << availableSpace / 1024 << "KB" << endl;
            vector<pair<string, pair<long, long>>> vec; //将键值对保存在数组中以便排序
            for (string file : files)
            {
                pair<long, long> s_t;
                if (get_file_size_time(file.c_str(), &s_t) == 0)
                {
                    vec.push_back(make_pair(file, make_pair(s_t.first, s_t.second)));
                }
            }

            //按照时间升序排序
            sort(vec.begin(), vec.end(),
                 [](pair<string, pair<long, long>> iter1, pair<string, pair<long, long>> iter2) -> bool
                 {
                     return iter1.second.second < iter2.second.second;
                 });
            for (int i = 0; i < vec.size(); i++) //删除文件直至可用容量大于需求容量
            {
                string file = vec[i].first;
                char filepath[file.length() + 1];
                int j;
                for (j = 0; j < file.length(); j++)
                {
                    filepath[j] = file[j];
                }
                filepath[j] = '\0';
                if (remove(filepath) == 0)
                {
                    getDiskSpaces();
                }
                if (availableSpace >= needSpace)
                    return true;
            }
        }
        else
        {
            return true;
        }
    }
    return false;
}
bool LimitMode(char buf[], long length)
{
    size_t dataSize = length + 1;
    size_t avail = availableSpace >> 20;
    size_t total = totalSpace >> 20;
    double usage = 1 - (double)avail / (double)total;
    // printf("Disk usage: %.2f%%\n", usage * 100);
    if (usage >= 0.7) //已使用70%以上空间
    {
        return false;
        printf("Disk usage:%.2f%%\n", usage * 100);
        if (dataSize > availableSpace)
        {
            return false;
        }
    }
    return true;
}

int DB_Write(long fp, char *buf, long length)
{
    FILE *file = (FILE *)fp;
    // struct statvfs diskInfo;
    // statvfs("./", &diskInfo);
    getDiskSpaces();
    // cout<<"available:"<<availableSpace/1024<<"KB\ntotal:"<<totalSpace/1024<<"KB"<<endl;
    bool allowWrite = true;
    // ReadConfig();   //读取配置

    if (config("FileOverFlowMode") == "loop")
    {
        allowWrite = LoopMode(buf, length);
    }
    else if (config("FileOverFlowMode") == "limit")
    {
        allowWrite = LimitMode(buf, length);
    }

    if (allowWrite)
    {
        if (fwrite(buf, length, 1, file) != 1)
        {
            perror("Error while writing");
            return errno;
        }
        return 0;
    }
    else
    {
        return errno;
    }
}

int DB_Open(char path[], char mode[], long *fptr)
{
    // ReadConfig();
    char finalPath[100];
    strcpy(finalPath, labelPath);
    strcat(finalPath, "/");
    strcat(finalPath, path);
    // fileQueue.push(finalPath);
    FILE *fp = fopen(finalPath, mode);
    if (fp == NULL)
    {
        if (errno == 2 && (mode[0] == 'w' || mode[0] == 'a'))
        {
            if (DB_CreateDirectory(finalPath) == 0)
            {
                fp = fopen(finalPath, mode);
                if (fp == NULL)
                {
                    return errno;
                }
                *fptr = (long)fp;
                return 0;
            }
            else
                return errno;
        }
        else
        {
            cout << finalPath << endl;
            perror("Error while opening file");
            return errno;
        }
    }
    *fptr = (long)fp;
    return 0;
}

int DB_Close(long fp)
{
    FILE *file = (FILE *)fp;
    // int fd = fileno(file);
    // char result[1024];
    // char path[1024];
    // /* Read out the link to our file descriptor. */
    // sprintf(path, "/proc/self/fd/%d", fd);
    // memset(result, 0, sizeof(result));
    // readlink(path, result, sizeof(result) - 1);
    return fclose(file) == 0 ? 0 : errno;
}

int DB_DeleteFile(char path[])
{
    // ReadConfig();
    char finalPath[100];
    strcpy(finalPath, labelPath);
    strcat(finalPath, "/");
    strcat(finalPath, path);
    // availableSpace += GetFileLengthByPath(path);
    // cout<<"Delete file "<<path<<"size:"<<GetFileLengthByPath(path)<<endl;
    return remove(finalPath) == 0 ? 0 : errno;
}
int DB_Read(long fptr, char buf[])
{
    FILE *fp = (FILE *)fptr;
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    int readnum = fread(buf, len, 1, fp);
    if (readnum < 0)
    {
        perror("Error while reading:");
        return errno;
    }
    return 0;
}
int DB_OpenAndRead(char path[], char buf[])
{
    char finalPath[100];
    strcpy(finalPath, labelPath);
    strcat(finalPath, "/");
    strcat(finalPath, path);
    FILE *fp = fopen(finalPath, "rb");
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    int readnum = fread(buf, len, 1, fp);
    fclose(fp); //读完后关闭文件
    if (readnum < 0)
    {
        perror("Error while reading:");
        return errno;
    }

    return 0;
}

int DB_ReadFile(DB_DataBuffer *buffer)
{
    string finalPath = labelPath;

    string savepath = buffer->savePath;
    if (savepath[0] != '/')
        finalPath += "/";
    finalPath += savepath;
    cout << finalPath << endl;
    FILE *fp = fopen(finalPath.c_str(), "rb");
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    if (len == 0)
    {
        buffer->length = 0;
        fclose(fp);
        return 0;
    }
    fseek(fp, 0, SEEK_SET);
    char *buf = (char *)malloc(len);
    int readnum = fread(buf, len, 1, fp);
    fclose(fp); //读完后关闭文件
    if (readnum < 0)
    {
        free(buf);
        buffer->bufferMalloced = 0;
        return errno;
    }
    buffer->buffer = buf;
    buffer->length = len;
    buffer->bufferMalloced = 1;
    return 0;
}

int DB_CreateDirectory(char path[])
{
    char str[100];
    strcpy(str, path);
    const char *d = "/";
    char *p;
    p = strtok(str, d);
    vector<string> res;
    while (p)
    {
        // printf("%s\n", p);
        res.push_back(p);
        p = strtok(NULL, d); // strtok内部记录了上次的位置
    }
    string dirs = "./";
    for (int i = 0; i < res.size(); i++)
    {
        if (res[i].find("idb") != string::npos)
            break;
        if (res[i] == ".." || res[i] == ".")
        {
            dirs += res[i] + "/";
        }
        else
        {
            dirs += res[i] + "/";
            if (mkdir(dirs.c_str(), 0777) != 0)
            {
                if (errno != 17)
                {
                    perror("Error while Creating directory");
                    return errno;
                }
            }
        }
    }
    return 0;
    // cout << path << endl;

    // char *p = strtok(str, "/");
    char dir[] = "./";
    while (p != NULL)
    {
        string s = p;
        if (s.find("idb") != string::npos)
            break;
        if (s == "..")
        {
            p = strtok(NULL, "/");
            strcat(dir, "../");
            continue;
        }
        strcat(dir, p);
        char dirpath[strlen(dir)];
        strcpy(dirpath, dir);
        strcat(dir, "/");
        cout << dirpath << endl;

        if (mkdir(dirpath, 0777) != 0)
        {
            perror("Error while Creating directory");
            return errno;
        }

        p = strtok(NULL, "/");
    }

    return 0;
}
int DB_DeleteDirectory(char path[])
{
    /*
        ReadConfig();
        char finalPath[100];
        strcpy(finalPath, labelPath);
        strcat(finalPath, "/");
        strcat(finalPath, path);
        */
    struct stat statbuf;
    if (stat(path, &statbuf) == -1)
    {
        perror("Error while deleting directory");
        return errno;
    }
    if (S_ISDIR(statbuf.st_mode)) //是文件夹
    {
        if (remove(path) == 0)
            return 0;
        else
            return errno;
    }

    if (S_ISREG(statbuf.st_mode)) //是常规文件
    {
        return 2;
    }
    return 0;
}
// int main()
// {
//     long fp;
//     DB_Open("test.db", "wb", &fp);
//     DB_Close(fp);
//     return 0;
// }
