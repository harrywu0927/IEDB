#include <utils.hpp>

/**
 * @brief 根据时间段获取所有包含此时间区间的包文件
 *
 * @param pathToLine 到产线的路径
 * @param start 起始时间
 * @param end 结束时间
 * @return vector<pair<string, tuple<long, long, char*>>>
 */
vector<pair<string, pair<char *, long>>> PackManager::GetPacksByTime(string pathToLine, long start, long end)
{
    vector<pair<string, pair<char *, long>>> selectedPacks;
    string tmp = pathToLine;
    while (tmp[0] == '/')
        tmp.erase(tmp.begin());
    while (tmp[tmp.size() - 1] == '/')
        tmp.pop_back();
    for (auto &pack : allPacks[tmp])
    {
        long packStart = get<0>(pack.second);
        long packEnd = get<1>(pack.second);
        if ((packStart < start && packEnd >= start) || (packStart < end && packEnd >= start) || (packStart <= start && packEnd >= end) || (packStart >= start && packEnd <= end)) //落入或部分落入时间区间
        {
            selectedPacks.push_back(make_pair(pack.first, GetPack(pack.first)));
        }
    }
    return selectedPacks;
}

/**
 * @brief 获取磁盘上当前所有目录下的某一产线下的所有包中的最后index个包
 *
 * @param pathToLine 到产线的路径
 * @param index 最后index个
 * @return pair<string, tuple<long, long, char*>>
 */
pair<string, pair<char *, long>> PackManager::GetLastPack(string pathToLine, int index)
{
    string tmp = pathToLine;
    while (tmp[0] == '/')
        tmp.erase(tmp.begin());
    while (tmp[tmp.size() - 1] == '/')
        tmp.pop_back();
    string packPath = allPacks[tmp][allPacks[tmp].size() - index].first;
    return {packPath, GetPack(packPath)};
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
        hits++;
        return key2pos[path]->second;
    }
    //缺页中断
    hits--;
    ReadPack(path);
    return GetPack(path);
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

// int main()
// {
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
