#include <utils.hpp>

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
    //由于allPacks中的元素在初始化时已经为时间升序型，因此无需再排序
    for (auto &pack : allPacks[tmp])
    {
        long packStart = get<0>(pack.second);
        long packEnd = get<1>(pack.second);
        if ((packStart < start && packEnd >= start) || (packStart < end && packEnd >= start) || (packStart <= start && packEnd >= end) || (packStart >= start && packEnd <= end)) //落入或部分落入时间区间
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
 * @return pair<string, tuple<long, long, char*>>
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
 * @return pair<char *, long>
 */
pair<char *, long> PackManager::GetPackByID(string pathToLine, string fileID)
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
            return GetPack(pack.first);
        }
    }
    return {NULL, 0};
}

/**
 * @brief 根据文件ID范围获取包
 *
 * @param pathToLine
 * @param fileID
 * @return pair<char *, long>
 */
vector<pair<char *, long>> PackManager::GetPackByIDs(string pathToLine, string fileID, int num)
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
    int start = atoi(startNum.c_str());
    int end = start + num;
    vector<pair<char *, long>> res;
    for (auto &pack : allPacks[pathToLine])
    {
        if ((start >= std::get<0>(fileIDManager.fidIndex[pack.first]) && start <= std::get<1>(fileIDManager.fidIndex[pack.first])) || (end >= std::get<0>(fileIDManager.fidIndex[pack.first]) && end <= std::get<1>(fileIDManager.fidIndex[pack.first])) || (start <= std::get<0>(fileIDManager.fidIndex[pack.first]) && end >= std::get<1>(fileIDManager.fidIndex[pack.first])))
        {
            res.push_back(GetPack(pack.first));
        }
    }
    if (res.size() == 0)
        res.push_back({NULL, 0});
    return res;
}

/**
 * @brief 根据文件ID范围获取包
 *
 * @param pathToLine
 * @param fileIDStart
 * @param fileIDEnd
 * @return pair<char *, long>
 */
vector<pair<char *, long>> PackManager::GetPackByIDs(string pathToLine, string fileIDStart, string fileIDEnd)
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
            res.push_back(GetPack(pack.first));
        }
    }
    if (res.size() == 0)
        res.push_back({NULL, 0});
    return res;
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
        free(packsInMem.back().second.first); //置换时注意清空此内存
        packsInMem.pop_back();
    }
    packsInMem.push_front({path, pack});
    key2pos[path] = packsInMem.begin();
    memUsed += pack.second;
}
float hits = 0;
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
        // hits++;
        // cout << "hit" << endl;
        return key2pos[path]->second;
    }
    //缺页中断
    // hits--;
    ReadPack(path);
    search = key2pos.find(path);
    PutPack(path, key2pos[path]->second);
    return key2pos[path]->second;
}

/**
 * @brief 从磁盘读取包
 *
 * @param path 包的存放路径
 */
void PackManager::ReadPack(string path)
{
    DB_DataBuffer buffer;
    buffer.savePath = path.c_str();
    DB_ReadFile(&buffer);
    if (buffer.bufferMalloced)
    {
        PutPack(path, {buffer.buffer, buffer.length});
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
