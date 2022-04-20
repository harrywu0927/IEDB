#include <utils.hpp>
PackManager packManager(1024 * 1024);
/**
 * @brief 根据时间段获取所有包含此时间区间的包文件
 *
 * @param pathToLine
 * @param start
 * @param end
 * @return vector<pair<string, tuple<long, long, char*>>>
 */
vector<pair<string, pair<char *, long>>> PackManager::GetPacksByTime(string pathToLine, long start, long end)
{
    vector<pair<string, pair<char *, long>>> selectedPacks;
    for (auto &pack : allPacks)
    {
        string tmp = pathToLine;
        while (tmp[0] == '/')
            tmp.erase(tmp.begin());
        while (tmp[tmp.size() - 1] == '/')
            tmp.pop_back();
        if (pack.first.substr(tmp.length()) != tmp)
            continue;
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
 * @brief 获取磁盘上当前所有包中的最后index个包
 *
 * @param pathToLine
 * @param index
 * @return pair<string, tuple<long, long, char*>>
 */
pair<string, tuple<long, long, char *>> PackManager::GetLastPack(string pathToLine, int index)
{
}

/**
 * @brief put
 *
 * @param path
 * @param pack
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
        free(packsInMem.back().second.first);
        packsInMem.pop_back();
    }
    packsInMem.push_front({path, pack});
    key2pos[path] = packsInMem.begin();
    memUsed += pack.second;
}

/**
 * @brief get
 *
 * @param path
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
    //缺页中断
    ReadPack(path);
    return GetPack(path);
}

/**
 * @brief 从磁盘读取包
 *
 * @param path 路径
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

int main()
{
}