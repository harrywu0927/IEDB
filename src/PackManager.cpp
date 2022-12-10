/*******************************************
 * @file PackManager.cpp
 * @author your name (you@domain.com)
 * @brief 对内存中pak的LRU管理和pak获取
 * @version 0.9.3
 * @date Last Modification in 2022-12-10
 *
 * @copyright Copyright (c) 2022
 *
 *******************************************/

#include <utils.hpp>

/**
 * @brief Construct a new Pack Manager:: Pack Manager object
 *
 * @param memcap Memory amount pack manager can use.
 */
PackManager::PackManager(long memcap) // 初始化allPacks
{
    vector<string> dirs;

    readAllDirs(dirs, settings("Filename_Label"));

    for (auto &dir : dirs)
    {
        try
        {
            for (auto const &dir_entry : fs::directory_iterator{dir})
            {
                if (fs::is_regular_file(dir_entry))
                {
                    string dirWithoutLabel = dir_entry.path().string();
                    removeFilenameLabel(dirWithoutLabel);
                    fs::path file = dirWithoutLabel;
                    fs::path pathToLine = file.parent_path(); // Not only absolute path here
                    if (file.extension() == ".pak")
                    {
                        auto timespan = DataType::splitWithStl(file.stem(), "-");
                        if (timespan.size() > 1)
                        {
                            long start = atol(timespan[0].c_str());
                            long end = atol(timespan[1].c_str());
                            if (end < start)
                            {
                                RuntimeLogger.warn("Package {} has invalid time range. It will be ignored.", dir_entry.path().string());
                                continue;
                            }
                            allPacks[pathToLine].push_back(make_pair(dirWithoutLabel, make_tuple(start, end)));
                        }
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            // std::cerr << e.what() << '\n';
            RuntimeLogger.error("Error occured when initializing pack index : code {} , {}", errno, strerror(errno));
        }
    }

    for (auto &iter : allPacks)
    {
        sort(iter.second.begin(), iter.second.end(),
             [](pair<string, tuple<long, long>> iter1, pair<string, tuple<long, long>> iter2) -> bool
             {
                 return get<0>(iter1.second) < get<0>(iter2.second);
             });
    }

    memCapacity = memcap;
}

/**
 * @brief 根据时间段获取所有包含此时间区间的包文件,由于缓存容量有限，故只返回文件名和时间范围，不读取包
 *
 * @param pathToLine 到产线的路径
 * @param start 起始时间
 * @param end 结束时间
 * @return vector<pair<string, tuple<long, long, char*>>>
 */
vector<pair<string, tuple<long, long>>> PackManager::GetPacksByTime(string pathToLine, long start, long end)
{
    vector<pair<string, tuple<long, long>>> selectedPacks;
    string tmp = pathToLine;
    while (tmp[0] == '/')
        tmp.erase(tmp.begin());
    while (tmp[tmp.size() - 1] == '/')
        tmp.pop_back();
    // 由于allPacks中的元素在初始化时已经为时间升序型，因此无需再排序
    for (auto &pack : allPacks[tmp])
    {
        long packStart = get<0>(pack.second);
        long packEnd = get<1>(pack.second);
        if ((packStart < start && packEnd >= start) || (packStart < end && packEnd >= start) || (packStart <= start && packEnd >= end) || (packStart >= start && packEnd <= end)) // 落入或部分落入时间区间
        {
            selectedPacks.push_back(pack);
        }
    }
    return selectedPacks;
}

/**
 * @brief 获取磁盘上当前所有目录下的某一产线下的所有包中的最后第index个包
 *
 * @param pathToLine 到产线的路径
 * @param index 最后第index个
 * @return pair<string, pair<char*,long>>
 */
pair<string, pair<char *, long>> PackManager::GetLastPack(string pathToLine, int index)
{
    if (index == 0)
        index = 1;
    string tmp = pathToLine;
    while (tmp[0] == '/')
        tmp.erase(tmp.begin());
    while (tmp[tmp.size() - 1] == '/')
        tmp.pop_back();
    string packPath = allPacks[tmp][allPacks[tmp].size() - index].first;
    return {packPath, GetPack(packPath)};
}

/**
 * @brief 根据文件ID获取包含此ID的包
 *
 * @param pathToLine
 * @param fileID
 * @param getpath 是否获取路径，默认为0
 * @return pair<char *, long> getpath为0时，返回内存地址-长度；设为1时，为路径-时间戳
 */
pair<char *, long> PackManager::GetPackByID(string pathToLine, string fileID, bool getpath)
{
    while (pathToLine[0] == '/')
        pathToLine.erase(0);
    while (pathToLine.back() == '/')
        pathToLine.pop_back();
    string startNum = "", endNum = "";
    for (int i = 0; i < fileID.length(); i++)
    {
        if (isdigit(fileID[i]))
        {
            startNum += fileID[i];
        }
    }
    int num = atoi(startNum.c_str());
    for (auto &pack : allPacks[pathToLine])
    {
        if (num >= std::get<0>(fileIDManager.fidIndex[pack.first]) && num <= std::get<1>(fileIDManager.fidIndex[pack.first]))
        {
            return getpath ? make_pair(const_cast<char *>(pack.first.c_str()), std::get<0>(pack.second)) : GetPack(pack.first);
        }
    }
    return {NULL, 0};
}

/**
 * @brief 根据文件ID范围获取包
 *
 * @param pathToLine 路径
 * @param fileID 起始ID
 * @param num 数量
 * @param getpath 是否获取路径，默认为0
 * @return pair<char *, long> getpath为0时，返回内存地址-长度；设为1时，为路径-时间戳
 */
vector<pair<char *, long>> PackManager::GetPackByIDs(string pathToLine, string fileID, int num, bool getpath)
{
    while (pathToLine[0] == '/')
        pathToLine.erase(0);
    while (pathToLine.back() == '/')
        pathToLine.pop_back();
    string startNum = "";
    for (int i = 0; i < fileID.length(); i++)
    {
        if (isdigit(fileID[i]))
        {
            startNum += fileID[i];
        }
    }
    /* 计算起始和结束ID */
    int start = atoi(startNum.c_str());
    int end = start + num;
    vector<pair<char *, long>> res;
    for (auto &pack : allPacks[pathToLine])
    {
        if ((start >= std::get<0>(fileIDManager.fidIndex[pack.first]) && start <= std::get<1>(fileIDManager.fidIndex[pack.first])) || (end >= std::get<0>(fileIDManager.fidIndex[pack.first]) && end <= std::get<1>(fileIDManager.fidIndex[pack.first])) || (start <= std::get<0>(fileIDManager.fidIndex[pack.first]) && end >= std::get<1>(fileIDManager.fidIndex[pack.first])))
        {
            if (!getpath)
                res.push_back(GetPack(pack.first));
            else
                res.push_back({const_cast<char *>(pack.first.c_str()), std::get<0>(pack.second)});
        }
    }
    if (res.size() == 0)
        res.push_back({NULL, 0});
    return res;
}

/**
 * @brief 根据文件ID范围获取包
 *
 * @param pathToLine 路径
 * @param fileIDStart 起始ID
 * @param fileIDEnd 结束ID
 * @param getpath 是否获取路径，默认为0
 * @return pair<char *, long> getpath为0时，返回内存地址-长度；设为1时，为路径-时间戳
 */
vector<pair<char *, long>> PackManager::GetPackByIDs(string pathToLine, string fileIDStart, string fileIDEnd, bool getpath)
{
    while (pathToLine[0] == '/')
        pathToLine.erase(0);
    while (pathToLine.back() == '/')
        pathToLine.pop_back();
    string startNum = "", endNum = "";
    for (int i = 0; i < fileIDStart.length(); i++)
    {
        if (isdigit(fileIDStart[i]))
        {
            startNum += fileIDStart[i];
        }
    }
    for (int i = 0; i < fileIDEnd.length(); i++)
    {
        if (isdigit(fileIDEnd[i]))
        {
            endNum += fileIDEnd[i];
        }
    }
    int start = atoi(startNum.c_str());
    int end = atoi(endNum.c_str());
    vector<pair<char *, long>> res;
    for (auto &pack : allPacks[pathToLine])
    {
        if ((start >= std::get<0>(fileIDManager.fidIndex[pack.first]) && start <= std::get<1>(fileIDManager.fidIndex[pack.first])) || (end >= std::get<0>(fileIDManager.fidIndex[pack.first]) && end <= std::get<1>(fileIDManager.fidIndex[pack.first])) || (start <= std::get<0>(fileIDManager.fidIndex[pack.first]) && end >= std::get<1>(fileIDManager.fidIndex[pack.first])))
        {
            if (!getpath)
                res.push_back(GetPack(pack.first));
            else
                res.push_back({const_cast<char *>(pack.first.c_str()), std::get<0>(pack.second)});
        }
    }
    if (res.size() == 0)
        res.push_back({NULL, 0});
    return res;
}

/**
 * @brief 根据起始时间，获取包含第一个大于起始时间文件的包文件
 *
 * @param pathToLine 到产线的路径
 * @param start 起始时间
 *
 * @author xss
 */
vector<pair<string, tuple<long, long>>> PackManager::GetFilesInPacksByTime(string pathToLine, long start)
{
    vector<pair<string, tuple<long, long>>> selectedPacks;
    string tmp = pathToLine;
    while (tmp[0] == '/')
        tmp.erase(tmp.begin());
    while (tmp[tmp.size() - 1] == '/')
        tmp.pop_back();
    // 由于allPacks中的元素在初始化时已经为时间升序型，因此无需再排序
    for (auto &pack : allPacks[tmp])
    {
        long packStart = get<0>(pack.second);
        long packEnd = get<1>(pack.second);
        if ((packStart < start && packEnd >= start) || (packStart > start)) // 大于指定时间的包
        {
            selectedPacks.push_back(pack);
            // cout << "start=" << start << ",packstart=" << packStart << ",packend=" << packEnd << endl;
            break;
        }
    }
    return selectedPacks;
}

/**
 * @brief put
 *
 * @param path 包的路径
 * @param pack 内存地址-长度对
 */
void PackManager::PutPack(string path, pair<char *, long> pack)
{
    if (key2pos.find(path) != key2pos.end())
    {
        memUsed -= key2pos[path]->second.second;
        packsInMem.erase(key2pos[path]);
    }
    else if (memUsed >= memCapacity)
    {
        memUsed -= packsInMem.back().second.second;
        key2pos.erase(packsInMem.back().first);
        if (packsInMem.back().second.first != NULL)
            free(packsInMem.back().second.first); // 置换时注意清空此内存
        packsInMem.pop_back();
    }
    packsInMem.push_front({path, pack});
    key2pos[path] = packsInMem.begin();
    memUsed += pack.second;
}
/**
 * @brief get
 *
 * @param path 包的路径
 * @return pair<char *, long>
 */
pair<char *, long> PackManager::GetPack(string path)
{
    auto search = key2pos.find(path);
    if (search != key2pos.end())
    {
        PutPack(path, key2pos[path]->second);
        return key2pos[path]->second;
    }
    // 缺页中断
    try
    {
        ReadPack(path);
    }
    catch (bad_alloc &e)
    {
        // RuntimeLogger.critical("Memory ");
        throw e;
    }
    catch (iedb_err &e)
    {
        RuntimeLogger.critical("Error occured when reading pack {} : {}", path, e.what());
        if (e.code == StatusCode::EMPTY_PAK)
            return {NULL, 0};
        throw e;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    search = key2pos.find(path);
    PutPack(path, key2pos[path]->second);
    return key2pos[path]->second;
}

/**
 * @brief 从磁盘读取包，当检测到包中含有图片时，仅在内存存储图片以外的数据
 * 图片索引方式为包的完整路径+包内节拍位序
 *
 * @param path 包的存放路径
 */
void PackManager::ReadPack(string path)
{
    DB_DataBuffer buffer;
    buffer.savePath = path.c_str();
#ifdef DEBUG
    spdlog::info("Read pack:{}", buffer.savePath);
#endif
    int err = DB_ReadFile(&buffer);
    if (err != 0)
        throw iedb_err(err);
    if (buffer.bufferMalloced)
    {
        PackFileReader reader(buffer.buffer, buffer.length);
        string templateName;
        int fileNum;
        reader.ReadPackHead(fileNum, templateName);
        if (TemplateManager::CheckTemplate(templateName) != 0 &&
            ZipTemplateManager::CheckZipTemplate(templateName) != 0)
            throw iedb_err(StatusCode::SCHEMA_FILE_NOT_FOUND);
        if (!CurrentTemplate.hasImage)
            PutPack(path, {buffer.buffer, buffer.length});
        else
        {
            char *newBuffer = (char *)malloc(24 + fileNum * (CurrentTemplate.totalBytes + 29));
            if (newBuffer == NULL)
                throw bad_alloc();
            memcpy(newBuffer, reader.packBuffer, 24);
            long posInPack = 24;
            int imgPos = 0;
            /* 假定图片数据均在模版的最后,获取第一张图片的位置 */
            for (auto &schema : CurrentTemplate.schemas)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                    break;
                if (schema.second.isTimeseries)
                {
                    if (schema.second.isArray)
                    {
                        imgPos += (schema.second.valueBytes * schema.second.arrayLen + 8) * schema.second.tsLen;
                    }
                    else
                    {
                        imgPos += (schema.second.valueBytes + 8) * schema.second.tsLen;
                    }
                }
                else if (schema.second.isArray)
                {
                    imgPos += schema.second.hasTime ? (8 + schema.second.valueBytes * schema.second.arrayLen) : (schema.second.valueBytes * schema.second.arrayLen);
                }
                else
                {
                    imgPos += schema.second.hasTime ? 8 + schema.second.valueBytes : schema.second.valueBytes;
                }
            }
            long cur = 24;
            for (int i = 0; i < fileNum; i++)
            {
                int readLength;
                memcpy(newBuffer + cur, reader.packBuffer + posInPack, 29);
                posInPack += 28;
                int zipType = (int)reader.packBuffer[posInPack++];
                memcpy(&readLength, reader.packBuffer + posInPack, 4);
                cur += 29;
                if (zipType == 0)
                {
                    /* 此处，imgPos即为非图片部分的数据长度 */
                    memcpy(newBuffer + cur, &imgPos, 4);
                    cur += 4;
                    memcpy(newBuffer + cur, reader.packBuffer + posInPack + 4, imgPos);
                    cur += imgPos;
                }
                /* 带有图片的数据不可能完全压缩,ziptype = 2 */
                else if (zipType == 2)
                {
                    int offset = GetZipImgPos(reader.packBuffer + posInPack + 4);
                    if (offset >= readLength)
                        throw iedb_err(StatusCode::DATAFILE_MODIFIED);
                    memcpy(newBuffer + cur, &offset, 4);
                    cur += 4;
                    memcpy(newBuffer + cur, reader.packBuffer + posInPack + 4, offset);
                    cur += offset;
                }
                /* 包内数据异常 */
                else
                {
                    throw iedb_err(StatusCode::DATAFILE_MODIFIED);
                }
                posInPack += 4 + readLength;
            }
            PutPack(path, {newBuffer, cur});
        }
    }
    else
    {
        RuntimeLogger.critical("Failed to read pack : {}, Error code {}", path, err);
        throw iedb_err(err);
    }
}

/**
 * @brief 删除包
 *
 * @param path 路径
 * @note 从allPacks中删除此包的索引
 */
void PackManager::DeletePack(string path)
{
    string tmp = path;
    string pathToLine = DataType::splitWithStl(tmp, "/")[0];
    DB_DeleteFile(const_cast<char *>(path.c_str()));
    for (auto it = allPacks[pathToLine].begin(); it != allPacks[pathToLine].end(); it++)
    {
        if (it->first == path)
        {
            allPacks[pathToLine].erase(it);
            break;
        }
    }
}

/**
 * @brief 修改包信息
 *
 * @param path 磁盘中的路径
 * @param newPath 新的路径
 * @param start 包的起始时间
 * @param end 包的结束时间
 * @note 无需关注已在内存中的包，它们会自然地被置换
 */
void PackManager::UpdatePack(string path, string newPath, long start, long end)
{
    string tmp = path;
    string pathToLine = DataType::splitWithStl(tmp, "/")[0];
    for (auto it = allPacks[pathToLine].begin(); it != allPacks[pathToLine].end(); it++)
    {
        if (it->first == path)
        {
            it->first = newPath;
            it->second = make_tuple(start, end);
            sort(allPacks[pathToLine].begin(), allPacks[pathToLine].end(), [](pair<string, tuple<long, long>> iter1, pair<string, tuple<long, long>> iter2) -> bool
                 { return get<0>(iter1.second) < get<0>(iter2.second); });
            return;
        }
    }
}

/**
 * @brief 增加包索引信息
 *
 * @param pathToLine 二级路径
 * @param path  包路径
 * @param start 开始时间
 * @param end 结束时间
 */
void PackManager::AddPack(string &pathToLine, string &path, long &start, long &end)
{
    allPacks[pathToLine].push_back({path, make_tuple(start, end)});
    sort(allPacks[pathToLine].begin(), allPacks[pathToLine].end(), [](pair<string, tuple<long, long>> iter1, pair<string, tuple<long, long>> iter2) -> bool
         { return get<0>(iter1.second) < get<0>(iter2.second); });
}

// int main()
// {
//     packManager.GetPackByID("JinfeiSixteen", "JinfeiSixteen1234");
//     return 0;
//     vector<string> files;
//     readPakFilesList("JinfeiSixteen", files);
//     for (int i = 0; i < 100; i++)
//     {
//         int index = rand() % files.size();
//         packManager.GetPack(files[index]);
//     }
//     cout << "hit percenge:" << hits << endl;
//     return 0;
// }
