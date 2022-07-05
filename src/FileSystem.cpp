#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <queue>
#include <utils.hpp>
#include <filesystem>
using namespace std;
#ifdef WIN32
typedef long long int long;
#endif
long availableSpace = 1024 * 1024 * 10;
size_t totalSpace = 0;
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

int readFileList_FS(const char *basePath, vector<string> *files)
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
        // else if (ptr->d_type == 10) /// link file
        //     printf("link file:%s/%s\n", basePath, ptr->d_name);
        else if (ptr->d_type == 4) /// dir
        {
            string base = basePath;
            base += "/";
            base += ptr->d_name;
            files->push_back(base);
            readFileList_FS(base.c_str(), files);
        }
    }
    closedir(dir);
    return 1;
}

queue<string> InitFileQueue()
{
    vector<string> files;
    readFileList_FS(settings("Filename_Label").c_str(), &files);
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
    char finalPath[100];
    strcpy(finalPath, settings("Filename_Label").c_str());
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
    fseek(fp, 0, SEEK_SET);
    *length = len;
    return 0;
}
bool LoopMode(char buf[], long length)
{
    DIR *dir;
    struct dirent *ptr;
    vector<string> files;
    long needSpace = length + 1;
    if (needSpace > availableSpace) //空间不足
    {
        cout << "Need space:" << needSpace / 1024 << "KB Available:" << availableSpace / 1024 << "KB" << endl;

        while (availableSpace < needSpace && !fileQueue.empty()) //删除文件直至可用容量大于需求容量
        {
            string file = fileQueue.front();
            fileQueue.pop();
            if (remove(file.c_str()) == 0)
            {
                getDiskSpaces();
            }
        }
        if (availableSpace >= needSpace)
            return true;
    }
    else
    {
        return true;
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
        // return false;
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

    if (settings("FileOverFlowMode") == "loop")
    {
        allowWrite = LoopMode(buf, length);
    }
    else if (settings("FileOverFlowMode") == "limit")
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
    strcpy(finalPath, settings("Filename_Label").c_str());
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
    strcpy(finalPath, settings("Filename_Label").c_str());
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
    strcpy(finalPath, settings("Filename_Label").c_str());
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
    string finalPath = settings("Filename_Label");

    string savepath = buffer->savePath;
    if (savepath[0] != '/')
        finalPath += "/";
    finalPath += savepath;
    cout << finalPath << endl;
    FILE *fp = fopen(finalPath.c_str(), "rb");
    if (fp == NULL)
        return errno;
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

/**
 * @brief 创建文件夹
 *
 * @param path
 * @return int
 */
int DB_CreateDirectory(char path[])
{
    std::filesystem::path dirpath = path;
    if (dirpath.filename().has_extension())
    {
        dirpath = dirpath.remove_filename();
    }
    try
    {
        std::filesystem::create_directories(dirpath);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return errno;
    }
    return 0;
    // char str[100];
    // strcpy(str, path);
    // const char *d = "/";
    // char *p;
    // p = strtok(str, d);
    // vector<string> res;
    // while (p)
    // {
    //     res.push_back(p);
    //     p = strtok(NULL, d); // strtok内部记录了上次的位置
    // }
    // string dirs = "./";
    // for (int i = 0; i < res.size(); i++)
    // {
    //     if (res[i].find("idb") != string::npos)
    //         break;
    //     if (res[i] == ".." || res[i] == ".")
    //     {
    //         dirs += res[i] + "/";
    //     }
    //     else
    //     {
    //         dirs += res[i] + "/";
    //         if (mkdir(dirs.c_str(), 0777) != 0)
    //         {
    //             if (errno != 17)
    //             {
    //                 perror("Error while Creating directory");
    //                 return errno;
    //             }
    //         }
    //     }
    // }
    // return 0;
}
int DB_DeleteDirectory(char path[])
{
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

int sysOpen(char path[])
{
    char finalPath[100];
    strcpy(finalPath, settings("Filename_Label").c_str());
    strcat(finalPath, "/");
    strcat(finalPath, path);

    int fd = open(finalPath, O_CREAT | O_RDWR, S_IRWXG | S_IRWXO | S_IRWXU);
    if (fd == -1)
        return errno;
    return fd;
}

// int main()
// {
//     long fp;
//     DB_Open("test.db", "wb", &fp);
//     DB_Close(fp);
//     return 0;
// }
