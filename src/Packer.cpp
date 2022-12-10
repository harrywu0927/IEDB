/********************************************
 * @file Packer.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.9.3
 * @date Last modified in 2022-12-10
 *
 * @copyright Copyright (c) 2022
 *
 ********************************************/
#include <utils.hpp>

mutex packMutex;

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
    // cout << "size " << filesWithTime.size() << endl;
    return filesWithTime.size() == 0 ? 0 : Packer::Pack(pathToLine, filesWithTime);
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
    IOBusy = true;
    packMutex.lock();
    int err = 0;
    if ((err = TemplateManager::CheckTemplate(pathToLine)) != 0)
    {
        packMutex.unlock();
        return err;
    }
    if (filesWithTime.size() < 1)
    {
        packMutex.unlock();
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    // 升序排序
    sort(filesWithTime.begin(), filesWithTime.end(),
         [](pair<string, long> iter1, pair<string, long> iter2) -> bool
         {
             return iter1.second < iter2.second;
         });
    long start = filesWithTime[0].second;
    long end = filesWithTime[filesWithTime.size() - 1].second;
    string packageName = to_string(start) + "-" + to_string(end) + ".pak";
    string pakPath = pathToLine + "/" + packageName;
    long fp;
    char mode[2] = {'w', 'b'};
    DB_Open(const_cast<char *>(pakPath.c_str()), mode, &fp);
    PACK_FILE_NUM_DTYPE filesNum = filesWithTime.size();
    long temBytes = CurrentTemplate.GetTotalBytes();
    int MAXSIZE = 2048 * 10240; // 20MB
    char *packBuffer = (char *)malloc(MAXSIZE);
    if (packBuffer == NULL)
        throw bad_alloc();
    memset(packBuffer, 0, MAXSIZE);
    DB_DataBuffer buffer;
    // 写入包头
    memcpy(packBuffer, &filesNum, sizeof(PACK_FILE_NUM_DTYPE));
    memcpy(packBuffer + sizeof(PACK_FILE_NUM_DTYPE), CurrentTemplate.path.c_str(), CurrentTemplate.path.length() <= TEMPLATE_NAME_SIZE ? CurrentTemplate.path.length() : TEMPLATE_NAME_SIZE);
    long cur = PACK_HEAD_SIZE, packTotalSize = 0;
    for (auto &file : filesWithTime)
    {
        if (MAXSIZE - cur < temBytes + PACK_HEAD_SIZE + 1) // 缓冲区不足，Flush之
        {
            fwrite(packBuffer, cur, 1, (FILE *)fp);
            memset(packBuffer, 0, MAXSIZE);
            packTotalSize += cur;
            cur = 0;
        }
        memcpy(packBuffer + cur, &file.second, 8);
        cur += 8;
        char fileID[PACK_FID_SIZE] = {0};
        string str = DataType::splitWithStl(fs::path(file.first).stem(), "_")[0];
        memcpy(fileID, str.c_str(), str.length() <= PACK_FID_SIZE ? str.length() : PACK_FID_SIZE);
        memcpy(packBuffer + cur, fileID, PACK_FID_SIZE);
        cur += PACK_FID_SIZE;
        buffer.savePath = file.first.c_str();
        if (DB_ReadFile(&buffer) == 0)
        {
            DB_DeleteFile(const_cast<char *>(buffer.savePath));
            if (buffer.length != 0)
            {
                if (fs::path(file.first).extension() == ".idb") // 未压缩
                {
                    char type = 0;
                    memcpy(packBuffer + cur++, &type, 1);
                }
                else // 不完全压缩
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
                char type = 1;
                memcpy(packBuffer + cur++, &type, 1); // 完全压缩
            }
        }
        else
        {
            packMutex.unlock();
            RuntimeLogger.error("Error occured when reading file {} : {}", file.first, strerror(errno));
            IOBusy = false;
            return errno;
        }
    }
    fwrite(packBuffer, cur, 1, (FILE *)fp);
    free(packBuffer);
    DB_Close(fp);
    fileQueue.push(pakPath);
    packManager.allPacks[pathToLine].push_back({pakPath, make_tuple(start, end)});
    packMutex.unlock();
    RuntimeLogger.info("Package {} completed, {} files were packed", packageName, filesNum);
    IOBusy = false;
    return 0;
}

/**
 * @brief 对某一路径下体积较小的包文件再次合并
 *
 * @param pathToLine 存储路径
 * @param packThres 最大大小,默认0
 * @return int
 * @note 需要修改allPacks中的元素，pak文件名，删除原pak
 */
int Packer::RePack(string pathToLine, int packThres)
{
    IOBusy = true;
    vector<string> packs;
    vector<pair<string, tuple<long, long>>> packsWithTime, curPacks;
    packMutex.lock();
    while (pathToLine[0] == '/')
        pathToLine.erase(0);
    while (pathToLine[pathToLine.length() - 1] == '/')
        pathToLine.pop_back();

    packsWithTime = packManager.allPacks[pathToLine];
    // readPakFilesList(pathToLine, packs);
    struct stat fileInfo;
    // sort by time_asc
    sort(packsWithTime.begin(), packsWithTime.end(),
         [](pair<string, tuple<long, long>> iter1, pair<string, tuple<long, long>> iter2) -> bool
         {
             return std::get<0>(iter1) < std::get<0>(iter2);
         });
    int packThreshold;
    if (settings("Pack_Mode") != "auto")
        packThreshold = atol(settings("Pack_Max_Size").c_str());
    else if (packThres != 0)
        packThreshold = packThres;
    int cursize = 0;
    string lastTemName = "";
    for (auto &pack : packsWithTime)
    {
        string finalPath = settings("Filename_Label") + (settings("Filename_Label").back() == '/' ? pack.first : ("/" + pack.first));
        if (stat(finalPath.c_str(), &fileInfo) == -1)
        {
            RuntimeLogger.error("Filed to get the size of file {} : {}", finalPath, strerror(errno));
            continue;
        }

        cursize += fileInfo.st_size;
        curPacks.push_back(pack);
        if (cursize >= packThreshold)
        {
            char *buffer = new char[cursize];
            long pos = PACK_HEAD_SIZE;
            PACK_FILE_NUM_DTYPE newFileNum = 0;
            long newStart = getMilliTime(), newEnd = 0;
            for (auto pk = curPacks.begin(); pk != curPacks.end(); pk++)
            {
                auto packToRepack = packManager.GetPack(pk->first);
                string templateName;
                char temName[TEMPLATE_NAME_SIZE] = {0};
                memcpy(temName, packToRepack.first + sizeof(PACK_FILE_NUM_DTYPE), TEMPLATE_NAME_SIZE);
                templateName = temName;
                /**
                 * 由于不同的包所使用的模版可能不同，因此仅合并模版相同的包
                 * 若出现模版名不同的包，curPacks中依然会保留此包，待之后处理
                 */
                if (lastTemName == "" || templateName == lastTemName)
                {
                    memcpy(buffer + pos, packToRepack.first + PACK_HEAD_SIZE, packToRepack.second - PACK_HEAD_SIZE);
                    pos += packToRepack.second - PACK_HEAD_SIZE;
                    PACK_FILE_NUM_DTYPE fileNum;
                    memcpy(&fileNum, packToRepack.first, sizeof(PACK_FILE_NUM_DTYPE));
                    newFileNum += fileNum;
                    lastTemName = templateName;
                    packManager.DeletePack(pk->first);
                    // DB_DeleteFile(const_cast<char *>(pk->first.c_str()));
                    for (auto i = packManager.allPacks[pathToLine].begin(); i != packManager.allPacks[pathToLine].end(); i++)
                    {
                        if (i->first == pk->first)
                        {
                            packManager.allPacks[pathToLine].erase(i);
                            i--;
                            break;
                        }
                    }
                    if (newStart > std::get<0>(pk->second))
                        newStart = std::get<0>(pk->second);
                    if (newEnd < std::get<1>(pk->second))
                        newEnd = std::get<1>(pk->second);
                    curPacks.erase(pk);
                    cursize -= packToRepack.second;
                    pk--;
                }
            }
            if (pos == 24)
            {
                delete[] buffer;
                continue;
            }
            memcpy(buffer, &newFileNum, sizeof(PACK_FILE_NUM_DTYPE));
            lastTemName.resize(TEMPLATE_NAME_SIZE, 0);
            memcpy(buffer + sizeof(PACK_FILE_NUM_DTYPE), lastTemName.c_str(), TEMPLATE_NAME_SIZE);
            string newPackPath = pathToLine + "/" + to_string(newStart) + "-" + to_string(newEnd) + ".pak";
            long fp;
            char mode[2] = {'w', 'b'};
            DB_Open(const_cast<char *>(newPackPath.c_str()), mode, &fp);
            fwrite(buffer, pos, 1, (FILE *)fp);
            DB_Close(fp);
            fileQueue.push(newPackPath);
            packManager.AddPack(pathToLine, newPackPath, newStart, newEnd);
            delete[] buffer;
            lastTemName = "";
            RuntimeLogger.info("Repacked package {} completed", newPackPath);
            // cursize = 0;
            // curPacks.clear();
        }
    }

    packMutex.unlock();
    IOBusy = false;
    return 0;
}

/**
 * @brief Construct a new Pack File Reader:: Pack File Reader object
 *
 * @param pathFilePath
 */
PackFileReader::PackFileReader(string pathFilePath)
{
    DB_DataBuffer buffer;
    buffer.buffer = NULL;
    buffer.savePath = pathFilePath.c_str();
    int err = DB_ReadFile(&buffer);
    if (err != 0)
    {
        errorCode = err;
        if (errorCode == StatusCode::EMPTY_PAK)
        {
            throw iedb_err(err, pathFilePath);
        }
        throw iedb_err(err);
    }
    if (buffer.bufferMalloced)
    {
        packBuffer = buffer.buffer;
        buffer.buffer = NULL;
        packLength = buffer.length;
        curPos = PACK_HEAD_SIZE;
        usingcache = false;
    }
    else
    {
        packBuffer = NULL;
        // cerr<<err<<endl;
        throw iedb_err(err);
    }
}

/**
 * @brief Construct a new Pack File Reader:: Pack File Reader object
 *
 * @param buffer
 * @param length
 */
PackFileReader::PackFileReader(char *buffer, long length, bool freeit)
{
    if (buffer == nullptr || buffer == NULL)
        // throw iedb_err(StatusCode::EMPTY_PAK);
        return;
    // char tempName[TEMPLATE_NAME_SIZE] = {0};
    // memcpy(tempName, buffer + 4, 20);
    // TemplateManager::CheckTemplate(tempName);
    // if (CurrentTemplate.hasImage)
    //     hasImg = true;
    // else
    //     hasImg = false;
    packBuffer = buffer;
    // buffer = NULL;
    packLength = length;
    curPos = PACK_HEAD_SIZE;
    if (freeit)
        usingcache = false;
    else
        usingcache = true;
}

/**
 * @brief 获取pak文件中本次应读取的文件长度、文件ID和时间戳
 * @param readLength      本次应读取的文件长度
 * @param timestamp       本文件的时间戳
 * @param fileID          本文件的ID
 * @param zipType         压缩类型
 * @param buffer          可选，读取非图片数据到缓冲区
 * @param completeZiped   可选，完全压缩的数据还原后的数据
 *
 * @return  数据区的起始位置
 * @note    为避免频繁的动态分配和释放内存，提高性能，此处采用指定偏移量和长度的方式，调用方自行执行内存拷贝，curPos记录当前已读到的位置
 */
long PackFileReader::Next(int &readLength, long &timestamp, string &fileID, int &zipType, char **buffer, char *completeZiped)
{
    memcpy(&timestamp, packBuffer + curPos, 8);
    curPos += 8;
    char fid[PACK_FID_SIZE + 1] = {0}; // 多留一个0作为结束位
    memcpy(fid, packBuffer + curPos, PACK_FID_SIZE);
    curPos += PACK_FID_SIZE;
    fileID = fid;
    zipType = (int)packBuffer[curPos++]; // 压缩情况
    long dataPos = curPos;
    switch (zipType)
    {
    case 0: // 非压缩
    {
        readLength = *(int *)(packBuffer + curPos);
        dataPos = curPos + 4;
        curPos += 4 + readLength;
        break;
    }
    case 1: // 完全压缩
    {
        readLength = 0;
        break;
    }
    case 2: // 不完全压缩
    {
        readLength = *(int *)(packBuffer + curPos);
        dataPos = curPos + 4;
        curPos += 4 + readLength;
        break;
    }
    default:
        throw iedb_err(StatusCode::UNKNWON_DATAFILE);
    }
    if (buffer != nullptr)
    {
        switch (zipType)
        {
        case 0:
            *buffer = packBuffer + dataPos;
            break;
        case 1:
            *buffer = completeZiped;
            break;
        case 2:
            *buffer = new char[CurrentTemplate.totalBytes];
            memcpy(*buffer, packBuffer + dataPos, readLength);
            ReZipBuff(buffer, readLength);
            break;
        default:
            break;
        }
    }
    return dataPos;
}

long PackFileReader::Next(int &readLength, long &timestamp, int &zipType, char **buffer, char *completeZiped)
{
    memcpy(&timestamp, packBuffer + curPos, 8);
    curPos += 8 + PACK_FID_SIZE;
    zipType = (int)packBuffer[curPos++]; // 压缩情况
    long dataPos = curPos;
    switch (zipType)
    {
    case 0: // 非压缩
    {
        readLength = *(int *)(packBuffer + curPos);
        dataPos = curPos + 4;
        curPos += 4 + readLength;
        break;
    }
    case 1: // 完全压缩
    {
        readLength = 0;
        break;
    }
    case 2: // 不完全压缩
    {
        readLength = *(int *)(packBuffer + curPos);
        dataPos = curPos + 4;
        curPos += 4 + readLength;
        break;
    }
    default:
        throw iedb_err(StatusCode::UNKNWON_DATAFILE);
    }
    if (buffer != nullptr)
    {
        switch (zipType)
        {
        case 0:
            *buffer = packBuffer + dataPos;
            break;
        case 1:
            *buffer = completeZiped;
            break;
        case 2:
            *buffer = new char[CurrentTemplate.totalBytes];
            memcpy(*buffer, packBuffer + dataPos, readLength);
            ReZipBuff(buffer, readLength);
            break;
        default:
            break;
        }
    }
    return dataPos;
}

long PackFileReader::Next(int &readLength, string &fileID, int &zipType, char **buffer, char *completeZiped)
{
    curPos += 8;
    char fid[PACK_FID_SIZE + 1] = {0};
    memcpy(fid, packBuffer + curPos, PACK_FID_SIZE);
    curPos += PACK_FID_SIZE;
    fileID = fid;
    zipType = (int)packBuffer[curPos++]; // 压缩情况
    long dataPos = curPos;
    switch (zipType)
    {
    case 0: // 非压缩
    {
        readLength = *(int *)(packBuffer + curPos);
        dataPos = curPos + 4;
        curPos += 4 + readLength;
        break;
    }
    case 1: // 完全压缩
    {
        readLength = 0;
        break;
    }
    case 2: // 不完全压缩
    {
        readLength = *(int *)(packBuffer + curPos);
        dataPos = curPos + 4;
        curPos += 4 + readLength;
        break;
    }
    default:
        // cerr << "datapos:" << dataPos << " ziptype :" << zipType << " readLength:" << readLength << " fileid:" << fid << "\n";
        throw iedb_err(StatusCode::UNKNWON_DATAFILE);
    }
    if (buffer != nullptr)
    {
        switch (zipType)
        {
        case 0:
            *buffer = packBuffer + dataPos;
            break;
        case 1:
            *buffer = completeZiped;
            break;
        case 2:
            *buffer = new char[CurrentTemplate.totalBytes];
            memcpy(*buffer, packBuffer + dataPos, readLength);
            ReZipBuff(buffer, readLength);
            break;
        default:
            break;
        }
    }
    return dataPos;
}

/**
 * @brief 略过包中的若干个文件
 *
 * @param num 略过个数
 */
int PackFileReader::Skip(int num)
{
    for (int i = 0; i < num; i++)
    {
        curPos += PACK_FID_SIZE + 8;
        int ztype = (int)packBuffer[curPos++]; // 压缩情况
        if (ztype != 1)
        {
            curPos += 4 + *(int *)(packBuffer + curPos);
        }
        else if (ztype != 2 && ztype != 0)
        {
            // cerr << "skip " << num << "files\n";

            return -1;
            throw iedb_err(StatusCode::DATAFILE_MODIFIED);
        }
    }
    return 0;
}

/**
 * @brief 读取pak文件包头
 * @param fileNum            包中文件总数
 * @param templateName       包中文件对应的模版名称
 *
 * @return
 * @note
 */
void PackFileReader::ReadPackHead(PACK_FILE_NUM_DTYPE &fileNum, string &templateName)
{
    memcpy(&fileNum, packBuffer, sizeof(PACK_FILE_NUM_DTYPE));
    char temName[TEMPLATE_NAME_SIZE] = {0};
    memcpy(temName, packBuffer + sizeof(PACK_FILE_NUM_DTYPE), TEMPLATE_NAME_SIZE);
    templateName = temName;
}
// int main()
// {
//     vector<pair<string, long>> files;
//     readDataFilesWithTimestamps("Robot_UDP", files);
//     Packer::Pack("Robot_UDP", files);
//     return 0;
// }