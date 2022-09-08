#ifndef _INSERT_HPP
#define _INSERT_HPP
#endif
#include <list>
#include <CassFactoryDB.h>
#include <future>
#include <Logger.h>
using namespace std;
#define InsertBufferLimit 1024 * 1024 * 4

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
    list<pair<DB_DataBuffer, bool>> buffer[2];
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
    int write(DB_DataBuffer *buffer, bool zip);
};
extern InsertBuffer insertBuffer;

int InsertBuffer::write(DB_DataBuffer *data, bool zip)
{
    DB_DataBuffer temp;
    if (data->length != 0)
    {
        temp.buffer = new char[data->length];
        memcpy(temp.buffer, data->buffer, data->length);
    }
    temp.length = data->length;
    temp.savePath = data->savePath;
    buffer[switcher].push_back({temp, zip});
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
            string savepath = item.first.savePath;
            if (savepath == "")
            {
                throw iedb_err(StatusCode::EMPTY_SAVE_PATH);
            }
            long fp;
            long curtime = getMilliTime();
            time_t time = curtime / 1000;
            struct tm *dateTime = localtime(&time);
            string fileID = FileIDManager::GetFileID(item.first.savePath) + "_";
            string finalPath = "";
            finalPath = finalPath.append(item.first.savePath).append("/").append(fileID).append(to_string(1900 + dateTime->tm_year)).append("-").append(to_string(1 + dateTime->tm_mon)).append("-").append(to_string(dateTime->tm_mday)).append("-").append(to_string(dateTime->tm_hour)).append("-").append(to_string(dateTime->tm_min)).append("-").append(to_string(dateTime->tm_sec)).append("-").append(to_string(curtime % 1000));
            char mode[2] = {'w', 'b'};
            if (item.second == true)
            {
                finalPath.append(".idbzip");
                if (item.first.buffer[0] == 0) //数据未压缩
                {
                    int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
                    if (err == 0)
                    {
                        err = DB_Write(fp, item.first.buffer + 1, item.first.length - 1);
                        delete[] item.first.buffer;
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
                        delete[] item.first.buffer;
                        throw iedb_err(err);
                    }
                }
                else if (item.first.buffer[0] == 1) //数据完全压缩
                {

                    int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
                    if (err == 0)
                    {
                        DB_Close(fp);
                    }
                    else
                    {
                        throw iedb_err(err);
                    }
                }
                else if (item.first.buffer[0] == 2) //数据未完全压缩
                {
                    int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
                    if (err == 0)
                    {
                        err = DB_Write(fp, item.first.buffer + 1, item.first.length - 1);
                        delete[] item.first.buffer;
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
                        delete[] item.first.buffer;
                        throw iedb_err(err);
                    }
                }
            }
            else
            {
                finalPath.append(".idb");
                int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
                if (err == 0)
                {
                    err = DB_Write(fp, item.first.buffer, item.first.length);
                    delete[] item.first.buffer;
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
                    delete[] item.first.buffer;
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