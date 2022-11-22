/***************************************
 * @file FileSystem.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.9.0
 * @date 于 2022-07-05 使用C++17 filesystem库重写部分函数
 *
 * @copyright Copyright (c) 2022
 *
 ***************************************/

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <queue>
#include <utils.hpp>

using namespace std;

long availableSpace = 1024 * 1024 * 100;
size_t totalSpace = 0;

bool fileOpenLocked = false;
/**
 * @brief 获取文件的大小和最后写入时间
 *
 * @param filename
 * @param s_t
 * @return int
 */
int get_file_size_time(const char *filename, pair<long, long> *s_t)
{
    fs::path file = filename;
    if (!fs::exists(file))
    {
        cout << "Error: file " << file << " does not exist\n";
        throw iedb_err(2);
    }
    if (fs::is_regular_file(file))
    {
        auto ftime = fs::last_write_time(file);
        *s_t = make_pair(fs::file_size(file), decltype(ftime)::clock::to_time_t(ftime));
    }

    return 0;
}
/**
 * @brief 获取磁盘空间
 *
 */
void getDiskSpaces()
{
    auto spaceInfo = fs::space("./");
    availableSpace = spaceInfo.available - 1024 * 1024 * 5;
    totalSpace = spaceInfo.capacity;
}
/**
 * @brief 递归读取文件列表
 *
 * @param basePath
 * @param files
 * @return int
 */
int readFileList_FS(const char *basePath, vector<string> *files)
{
    try
    {
        for (auto const &dir_entry : fs::recursive_directory_iterator{basePath})
        {
            if (fs::is_regular_file(dir_entry))
                files->push_back(dir_entry.path().string());
        }
    }
    catch (const std::exception &e)
    {
        // RuntimeLogger.error("Error when reading files: {}", e.what());
        throw iedb_err(errno);
    }
    return 0;
}
/**
 * @brief 初始化文件队列
 *
 * @return queue<string>
 */
queue<string> InitFileQueue()
{
    vector<string> files;
    try
    {
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
    }
    catch (iedb_err &e)
    {
        RuntimeLogger.critical("Error occured when initializing file list : {}", e.what());
        // exit(0);
    }
    catch (fs::filesystem_error &e)
    {
        RuntimeLogger.critical("File system error occured when initializing file list : {}", e.what());
        exit(0);
    }
    catch (const std::exception &e)
    {
        RuntimeLogger.critical("Error:{}", e.what());
        exit(0);
        // std::cerr << e.what() << '\n';
    }

    queue<string> que;
    for (auto &file : files)
    {
        que.push(file);
    }
    return que;
}

/**
 * @brief 通过文件名获得文件大小
 *
 * @param path 文件路径
 * @param length 文件大小
 * @return int
 */
int DB_GetFileLengthByPath(char path[], long *length)
{
    string filepath = settings("Filename_Label");
    if (filepath.back() != '/')
        filepath += "/";
    filepath.append(path);
    long len = 0;
    try
    {
        len = fs::file_size(filepath);
    }
    catch (fs::filesystem_error &e)
    {
        RuntimeLogger.critical("Error when getting file length: {}. File {}", e.what(), filepath);
    }
    catch (const std::exception &e)
    {
        RuntimeLogger.error("Error when getting file length: {}. File {}", e.what(), filepath);
        throw iedb_err(errno);
    }

    *length = len;
    return 0;
}

/**
 * @brief 通过文件句柄获得文件大小
 *
 * @param fileptr 文件句柄
 * @param length 文件大小
 * @return int
 */
int DB_GetFileLengthByFilePtr(long fileptr, long *length)
{
    FILE *fp = (FILE *)fileptr;
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        RuntimeLogger.error("Error when getting file length: {}", strerror(errno));
        throw iedb_err(errno);
        return errno;
    }
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    *length = len;
    return 0;
}
/**
 * @brief 循环写，当磁盘满时，删除文件队列中最早的文件直至有足够空间
 *
 * @param buf
 * @param length
 * @return true
 * @return false
 */
bool LoopMode(char buf[], long length)
{
    long needSpace = length + 1;
    if (needSpace > availableSpace) //空间不足
    {
        // cout << "Need space:" << needSpace / 1024 << "KB Available:" << availableSpace / 1024 << "KB\n";

        while (availableSpace < needSpace && !fileQueue.empty()) //删除文件直至可用容量大于需求容量
        {
            string file = fileQueue.front();
            fileQueue.pop();
            if (remove(file.c_str()) == 0)
            {
                RuntimeLogger.warn("File {} was deleted.", file);
                getDiskSpaces();
            }
            else
            {
                RuntimeLogger.error("Failed to remove file {} : {}, skipping...", file, strerror(errno));
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
/**
 * @brief 使用容量大于95%，不允许写入
 *
 * @param buf
 * @param length
 * @return true
 * @return false
 */
bool LimitMode(char buf[], long length)
{
    // size_t dataSize = length + 1;
    // size_t avail = availableSpace >> 20;
    // size_t total = totalSpace >> 20;
    double usage = 1 - (double)availableSpace / (double)totalSpace;
    // printf("Disk usage: %.2f%%\n", usage * 100);
    if (usage >= 0.95) //已使用95%以上空间
    {
        // return false;
        printf("Disk usage:%.2f%%\n", usage * 100);
        fileOpenLocked = true;
        return false;
    }
    return true;
}

/**
 * @brief 通过文件句柄写文件
 *
 * @param fp 文件句柄
 * @param buf 写入的数据
 * @param length 写入的长度
 * @return int
 */
int DB_Write(long fp, char *buf, long length)
{
    FILE *file = (FILE *)fp;
    getDiskSpaces();
    bool allowWrite = true;

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
            RuntimeLogger.critical("Error when writing: {}", strerror(errno));
            throw iedb_err(errno);
            return errno;
        }
        return 0;
    }
    else
    {
        return errno;
    }
}

/**
 * @brief 打开文件
 *
 * @param path 文件路径及名称
 * @param mode 打开模式
 * @param fptr 文件句柄
 * @return int
 */
int DB_Open(char path[], char mode[], long *fptr)
{
    if (settings("FileOverFlowMode") == "limit" && fileOpenLocked)
    {
        getDiskSpaces();
        // size_t avail = availableSpace >> 20;
        // size_t total = totalSpace >> 20;
        double usage = 1 - (double)availableSpace / (double)totalSpace;
        // printf("Disk usage: %.2f%%\n", usage * 100);
        if (usage >= 0.95) //已使用70%以上空间
        {
            return -1;
        }
        fileOpenLocked = false; //空间足够，解锁
    }
    string filepath = settings("Filename_Label");
    if (filepath.back() != '/')
        filepath += "/";
    filepath.append(path);
    // fileQueue.push(finalPath);
    FILE *fp = fopen(filepath.c_str(), mode);
    if (fp == NULL)
    {
        if (errno == 2 && (mode[0] == 'w' || mode[0] == 'a'))
        {
            if (DB_CreateDirectory(const_cast<char *>(filepath.c_str())) == 0)
            {
                fp = fopen(filepath.c_str(), mode);
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
            RuntimeLogger.error("Error when opening {} : {}", filepath, strerror(errno));
            return errno;
        }
    }
    *fptr = (long)fp;
    return 0;
}

/**
 * @brief 关闭文件
 *
 * @param fp 文件句柄
 * @return int
 */
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

/**
 * @brief 删除文件
 *
 * @param path 文件路径
 * @return int
 */
int DB_DeleteFile(char path[])
{
    string filepath = settings("Filename_Label");
    if (filepath.back() != '/')
        filepath += "/";
    filepath.append(path);
    // availableSpace += GetFileLengthByPath(path);
    // cout<<"Delete file "<<path<<"size:"<<GetFileLengthByPath(path)<<endl;
    return remove(filepath.c_str()) == 0 ? 0 : errno;
}

/**
 * @brief 读文件
 *
 * @param fptr 文件句柄
 * @param buf 读出的数据
 * @return int
 */
int DB_Read(long fptr, char buf[])
{
    FILE *fp = (FILE *)fptr;
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    int readnum = fread(buf, len, 1, fp);
    if (readnum < 0)
    {
        RuntimeLogger.error("Error when reading : {}", strerror(errno));
        return errno;
    }
    return 0;
}

/**
 * @brief 打开文件并读取
 *
 * @param path
 * @param buf
 * @return int
 */
int DB_OpenAndRead(char path[], char buf[])
{
    string filepath = settings("Filename_Label");
    if (filepath.back() != '/')
        filepath += "/";
    filepath.append(path);
    FILE *fp = fopen(filepath.c_str(), "rb");
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    int readnum = fread(buf, len, 1, fp);
    fclose(fp); //读完后关闭文件
    if (readnum < 0)
    {
        RuntimeLogger.error("Error when reading {} : {}", path, strerror(errno));
        return errno;
    }

    return 0;
}

/**
 * @brief 读取文件内容到DB_DataBuffer中
 *
 * @param buffer
 * @return int
 */
int DB_ReadFile(DB_DataBuffer *buffer)
{
    string finalPath = settings("Filename_Label");

    string savepath = buffer->savePath;
    if (savepath[0] != '/')
        finalPath += "/";
    finalPath += savepath;
    // cout << finalPath << '\n';
    FILE *fp = fopen(finalPath.c_str(), "rb");
    if (fp == NULL)
    {
        buffer->bufferMalloced = 0;
        return errno;
    }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    if (len == 0)
    {
        buffer->length = 0;
        buffer->bufferMalloced = 0;
        // cout<<finalPath<<endl;
        fclose(fp);
        return StatusCode::EMPTY_PAK;
    }
    fseek(fp, 0, SEEK_SET);
    char *buf = (char *)malloc(len);
    if (buf == NULL)
    {
        fclose(fp);
        return StatusCode::MEMORY_INSUFFICIENT;
    }
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
    fs::path dirpath = path;
    if (dirpath.filename().has_extension())
    {
        dirpath = dirpath.remove_filename();
    }
    try
    {
        fs::create_directories(dirpath);
    }
    catch (fs::filesystem_error &e)
    {
        RuntimeLogger.error("Failed to create directory {} : {}", path, e.what());
        return errno;
    }
    return 0;
}
/**
 * @brief 删除文件夹
 *
 * @param path
 * @return int
 */
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
    string filepath = settings("Filename_Label");
    if (filepath.back() != '/')
        filepath += "/";
    filepath.append(path);

    int fd = open(filepath.c_str(), O_CREAT | O_RDWR, S_IRWXG | S_IRWXO | S_IRWXU);
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
