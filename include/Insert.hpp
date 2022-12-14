#ifndef _INSERT_HPP
#define _INSERT_HPP

#include <list>
#include <CassFactoryDB.h>
#include <future>
#include <Logger.h>
using namespace std;
#define InsertBufferLimit 100

struct Rhythm
{
    char *data;
    int length;
    string savePath;
    bool zip;
};

/**
 * @brief 插入缓冲
 * 使用双缓冲，一个缓冲区满后立即开始flush，同时开始使用另一个缓冲区
 * 插入链表时记录size总大小，没有硬性限制大小，超过既定阈值后即开始刷新
 * 通过switcher控制当前使用的缓冲区
 */
class InsertBuffer
{
private:
    int size;
    list<Rhythm> buffer[2];
    int switcher;
    int error;

public:
    InsertBuffer()
    {
        size = 0;
        switcher = 0;
        error = 0;
    }
    ~InsertBuffer() {}
    int flush(int switcher);
    int write(DB_DataBuffer *buffer, bool zip, string savePath);
};
extern InsertBuffer insertBuffer;

int InsertBuffer::write(DB_DataBuffer *data, bool zip, string savePath)
{
    Rhythm rhythm;
    if (data->length != 0)
    {
        rhythm.data = new char[data->length];
        memcpy(rhythm.data, data->buffer, data->length);
    }
    rhythm.length = data->length;
    rhythm.savePath = savePath;
    buffer[switcher].push_back(rhythm);
    size += data->length;
    if (size >= InsertBufferLimit)
    {
        std::async(std::launch::async, &InsertBuffer::flush, this, switcher);
        switcher = switcher == 0 ? 1 : 0;
    }
    return error;
}

int InsertBuffer::flush(int switcher)
{
    try
    {
        while (!buffer[switcher].empty())
        {
            auto item = buffer[switcher].front();
            // string savepath = item.savePath;
            // if (savepath == "")
            // {
            //     throw iedb_err(StatusCode::EMPTY_SAVE_PATH);
            // }
            long fp;
            // long curtime = getMilliTime();
            // time_t time = curtime / 1000;
            // struct tm *dateTime = localtime(&time);
            // string fileID = FileIDManager::GetFileID(item.savePath) + "_";
            // string finalPath = "";
            // finalPath = finalPath.append(item.savePath).append("/").append(fileID).append(to_string(1900 + dateTime->tm_year)).append("-").append(to_string(1 + dateTime->tm_mon)).append("-").append(to_string(dateTime->tm_mday)).append("-").append(to_string(dateTime->tm_hour)).append("-").append(to_string(dateTime->tm_min)).append("-").append(to_string(dateTime->tm_sec)).append("-").append(to_string(curtime % 1000));
            char mode[2] = {'w', 'b'};
            if (item.zip == true)
            {
                item.savePath.append(".idbzip");
                if (item.data[0] == 0) // 数据未压缩
                {
                    int err = DB_Open(const_cast<char *>(item.savePath.c_str()), mode, &fp);
                    if (err == 0)
                    {
                        err = DB_Write(fp, item.data + 1, item.length - 1);
                        delete[] item.data;
                        if (err == 0)
                        {
                            DB_Close(fp);
                        }
                        else
                        {
                            throw iedb_err(err);
                        }
                    }
                    else
                    {
                        delete[] item.data;
                        throw iedb_err(err);
                    }
                }
                else if (item.data[0] == 1) // 数据完全压缩
                {

                    int err = DB_Open(const_cast<char *>(item.savePath.c_str()), mode, &fp);
                    if (err == 0)
                    {
                        DB_Close(fp);
                    }
                    else
                    {
                        throw iedb_err(err);
                    }
                }
                else if (item.data[0] == 2) // 数据未完全压缩
                {
                    int err = DB_Open(const_cast<char *>(item.savePath.c_str()), mode, &fp);
                    if (err == 0)
                    {
                        err = DB_Write(fp, item.data + 1, item.length - 1);
                        delete[] item.data;
                        if (err == 0)
                        {
                            DB_Close(fp);
                        }
                        else
                        {
                            throw iedb_err(err);
                        }
                    }
                    else
                    {
                        delete[] item.data;
                        throw iedb_err(err);
                    }
                }
            }
            else
            {
                item.savePath.append(".idb");
                int err = DB_Open(const_cast<char *>(item.savePath.c_str()), mode, &fp);
                if (err == 0)
                {
                    err = DB_Write(fp, item.data, item.length);
                    delete[] item.data;
                    if (err == 0)
                    {
                        DB_Close(fp);
                    }
                    else
                    {
                        throw iedb_err(err);
                    }
                }
                else
                {
                    delete[] item.data;
                    throw iedb_err(err);
                }
            }
            error = 0;
        }
    }
    catch (iedb_err &e)
    {
        error = e.code;
        RuntimeLogger.critical(e.what());
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    return 0;
}
#endif