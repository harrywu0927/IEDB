#include "../include/utils.hpp"

/**
 * @brief 将一个文件夹下的压缩或未压缩的数据文件打包为一个文件(.pak)，pak文件内的数据均为时间升序型
 * @param pathToLine        到产线的路径
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int DB_Pack(const char *pathToLine, int num, int packAll)
{
    vector<pair<string, long>> filesWithTime;
    readDataFilesWithTimestamps(pathToLine, filesWithTime);
    return Packer::Pack(pathToLine, filesWithTime);
}

/**
 * @brief 将一个文件夹下的压缩或未压缩的数据文件打包为一个文件(.pak)，pak文件内的数据均为时间升序型
 * @param pathToLine        到产线的路径
 * @param filesWithTime     带有时间戳的文件列表
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int Packer::Pack(string pathToLine, vector<pair<string, long>> &filesWithTime)
{
    TemplateManager::CheckTemplate(pathToLine);
    if (filesWithTime.size() < 1)
        return -1;
    //升序排序
    sort(filesWithTime.begin(), filesWithTime.end(),
         [](pair<string, long> iter1, pair<string, long> iter2) -> bool
         {
             return iter1.second < iter2.second;
         });
    long start = filesWithTime[0].second;
    long end = filesWithTime[filesWithTime.size() - 1].second;
    string packageName = to_string(start) + "-" + to_string(end) + ".pak";
    string pakPath = pathToLine + 
    long fp;
    char mode[2] = {'w', 'b'};
    DB_Open(const_cast<char *>(packageName.c_str()), mode, &fp);
    int filesNum = filesWithTime.size();
    long temBytes = CurrentTemplate.GetTotalBytes();
    int MAXSIZE = 2048 * 1024; // 2MB
    char *packBuffer = (char *)malloc(MAXSIZE);
    memset(packBuffer, 0, MAXSIZE);
    DB_DataBuffer buffer;
    //写入包头
    memcpy(packBuffer, &filesNum, 4);
    memcpy(packBuffer + 4, CurrentTemplate.path.c_str(), CurrentTemplate.path.length() <= 20 ? CurrentTemplate.path.length() : 20);
    long cur = 24, packTotalSize = 0;
    for (auto &file : filesWithTime)
    {
        if (MAXSIZE - cur < temBytes + 25) //缓冲区不足，Flush之
        {
            fwrite(packBuffer, cur, 1, (FILE *)fp);
            memset(packBuffer, 0, MAXSIZE);
            packTotalSize += cur;
            cur = 0;
        }
        memcpy(packBuffer + cur, &file.second, 8);
        cur += 8;
        char fileID[20] = {0};
        string tmp = file.first;
        string str = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "_")[0];
        while (str[0] == '/') //去除‘/’
            str.erase(str.begin());
        memcpy(fileID, str.c_str(), str.length() <= 20 ? str.length() : 20);
        memcpy(packBuffer + cur, fileID, 20);
        cur += 20;
        buffer.savePath = file.first.c_str();
        if (DB_ReadFile(&buffer) == 0)
        {
            if (buffer.length != 0)
            {
                if (file.first.find(".idbzip") == string::npos) //未压缩
                {
                    char type = 0;
                    memcpy(packBuffer + cur++, &type, 1);
                }
                else //不完全压缩
                {
                    char type = 2;
                    memcpy(packBuffer + cur++, &type, 1);
                }
                memcpy(packBuffer + cur, (int *)&buffer.length, 4);
                cur += 4;
                memcpy(packBuffer + cur, buffer.buffer, buffer.length);
                cur += buffer.length;
                free(buffer.buffer);
                buffer.buffer = NULL;
            }
            else
            {
                packBuffer[++cur] = (char)1; //完全压缩
            }
        }
        else
        {
            return errno;
        }
    }
    fwrite(packBuffer, cur, 1, (FILE *)fp);
    free(packBuffer);
    fclose((FILE *)fp);
    return 0;
}

/**
 * @brief 获取pak文件中本次应读取的文件长度、文件ID和时间戳
 * @param readLength      本次应读取的文件长度
 * @param timestamp       本文件的时间戳
 * @param fileID          本文件的ID
 *
 * @return  数据区的起始位置
 * @note    为避免频繁的动态分配和释放内存，提高性能，此处采用指定偏移量和长度的方式，调用方自行执行内存拷贝，curPos记录当前已读到的位置
 */
long PackFileReader::Next(int &readLength, long &timestamp, string &fileID, int &zipType)
{
    memcpy(&timestamp, packBuffer + curPos, 8);
    curPos += 8;
    char fid[20] = {0};
    memcpy(fid, packBuffer + curPos, 20);
    curPos += 20;
    fileID = fid;
    zipType = (int)packBuffer[curPos++]; //压缩情况
    long dataPos = curPos;
    switch (zipType)
    {
    case 0: //非压缩
    {
        memcpy(&readLength, packBuffer + curPos, 4);
        dataPos = curPos + 4;
        curPos += 4 + readLength;
        break;
    }
    case 1: //完全压缩
    {
        readLength = 0;
        break;
    }
    case 2: //不完全压缩
    {
        memcpy(&readLength, packBuffer + curPos, 4);
        dataPos = curPos + 4;
        curPos += 4 + readLength;
        break;
    }
    default:
        break;
    }
    return dataPos;
}

long PackFileReader::Next(int &readLength, long &timestamp, int &zipType)
{
    memcpy(&timestamp, packBuffer + curPos, 8);
    curPos += 28;
    zipType = (int)packBuffer[curPos++]; //压缩情况
    long dataPos = curPos;
    switch (zipType)
    {
    case 0: //非压缩
    {
        memcpy(&readLength, packBuffer + curPos, 4);
        dataPos = curPos + 4;
        curPos += 4 + readLength;
        break;
    }
    case 1: //完全压缩
    {
        readLength = 0;
        break;
    }
    case 2: //不完全压缩
    {
        memcpy(&readLength, packBuffer + curPos, 4);
        dataPos = curPos + 4;
        curPos += 4 + readLength;
        break;
    }
    default:
        break;
    }
    return dataPos;
}

long PackFileReader::Next(int &readLength, string &fileID, int &zipType)
{
    curPos += 8;
    char fid[20] = {0};
    memcpy(fid, packBuffer + curPos, 20);
    curPos += 20;
    fileID = fid;
    zipType = (int)packBuffer[curPos++]; //压缩情况
    long dataPos = curPos;
    switch (zipType)
    {
    case 0: //非压缩
    {
        memcpy(&readLength, packBuffer + curPos, 4);
        dataPos = curPos + 4;
        curPos += 4 + readLength;
        break;
    }
    case 1: //完全压缩
    {
        readLength = 0;
        break;
    }
    case 2: //不完全压缩
    {
        memcpy(&readLength, packBuffer + curPos, 4);
        dataPos = curPos + 4;
        curPos += 4 + readLength;
        break;
    }
    default:
        break;
    }
    return dataPos;
}

/**
 * @brief 读取pak文件包头
 * @param fileNum            包中文件总数
 * @param templateName       包中文件对应的模版名称
 *
 * @return
 * @note
 */
void PackFileReader::ReadPackHead(int &fileNum, string &templateName)
{
    memcpy(&fileNum, packBuffer, 4);
    char temName[20] = {0};
    memcpy(temName, packBuffer + 4, 20);
    templateName = temName;
}