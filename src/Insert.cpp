#include "../include/utils.hpp"
#include "../include/Packer.hpp"

using namespace std;

/**
 * @brief 将一个缓冲区中的一条数据(文件)存放在指定路径下，以文件ID+时间的方式命名
 * @param buffer    数据缓冲区
 * @param addTime    是否添加时间戳
 *
 * @return  0:success,
 *          others: StatusCode
 * @note 文件ID的暂定的命名方式为当前文件夹下的文件数量+1，
 *  时间戳格式为yyyy-mm-dd-hh-min-ss-ms
 */
int DB_InsertRecord(DB_DataBuffer *buffer, int addTime)
{
    string savepath = buffer->savePath;
    if (savepath == "")
        return StatusCode::EMPTY_SAVE_PATH;
    long fp;
    long curtime = getMilliTime();
    time_t time = curtime / 1000;
    struct tm *dateTime = localtime(&time);
    string fileID = FileIDManager::GetFileID(buffer->savePath);
    string finalPath = "";
    finalPath = finalPath.append(buffer->savePath).append("/").append(fileID).append(to_string(1900 + dateTime->tm_year)).append("-").append(to_string(1 + dateTime->tm_mon)).append("-").append(to_string(dateTime->tm_mday)).append("-").append(to_string(dateTime->tm_hour)).append("-").append(to_string(dateTime->tm_min)).append("-").append(to_string(dateTime->tm_sec)).append("-").append(to_string(curtime % 1000));
    char mode[2] = {'a','b'};
    if (addTime == 0)
    {
        finalPath.append(".idb");
        int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
        if (err == 0)
        {
            err = DB_Write(fp, buffer->buffer, buffer->length);
            if (err == 0)
            {
                return DB_Close(fp);
            }
        }
        return err;
    }
    else
    {
        finalPath.append(".idbzip");
        if (buffer->buffer[0] == 0) //数据未压缩
        {
            int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
            if (err == 0)
            {
                err = DB_Write(fp, buffer->buffer + 1, buffer->length - 1);
                if (err == 0)
                {
                    return DB_Close(fp);
                }
            }
            return err;
        }
        else if (buffer->buffer[0] == 1) //数据完全压缩
        {

            int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
            err = DB_Close(fp);
            return err;
        }
        else if (buffer->buffer[0] == 2) //数据未完全压缩
        {
            int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
            if (err == 0)
            {
                err = DB_Write(fp, buffer->buffer + 1, buffer->length - 1);
                if (err == 0)
                {
                    return DB_Close(fp);
                }
            }
            return err;
        }
    }
    return 0;
}

/**
 * @brief 将多条数据(文件)存放在指定路径下，以文件ID+时间的方式命名
 * @param buffer[]    数据缓冲区
 * @param recordsNum  数据(文件)条数
 * @param addTime    是否添加时间戳
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  文件ID的暂定的命名方式为当前文件夹下的文件数量+1，
 *  时间戳格式为yyyy-mm-dd-hh-min-ss-ms
 */
int DB_InsertRecords(DB_DataBuffer buffer[], int recordsNum, int addTime)
{
    string savepath = buffer->savePath;
    if (savepath == "")
        return StatusCode::EMPTY_SAVE_PATH;
    long curtime = getMilliTime();
    time_t time = curtime / 1000;
    struct tm *dateTime = localtime(&time);
    string timestr = "";
    timestr.append(to_string(1900 + dateTime->tm_year)).append("-").append(to_string(1 + dateTime->tm_mon)).append("-").append(to_string(dateTime->tm_mday)).append("-").append(to_string(dateTime->tm_hour)).append("-").append(to_string(dateTime->tm_min)).append("-").append(to_string(dateTime->tm_sec)).append("-").append(to_string(curtime % 1000)).append(".idb");
    char mode[2] = {'a','b'};
    for (int i = 0; i < recordsNum; i++)
    {
        long fp;

        string fileID = FileIDManager::GetFileID(buffer->savePath);
        string finalPath = "";
        finalPath = finalPath.append(buffer->savePath).append("/").append(fileID).append(timestr);
        int err = DB_Open(const_cast<char *>(buffer[i].savePath), mode, &fp);
        if (err == 0)
        {
            err = DB_Write(fp, buffer[i].buffer, buffer[i].length);
            if (err != 0)
            {
                return err;
            }
            DB_Close(fp);
        }
        else
        {
            return err;
        }
    }
    return 0;
}