#include <utils.hpp>

/**
 * @brief
 *
 * @param packs
 * @return long
 */
long FileIDManager::GetPacksRhythmNum(vector<pair<string, tuple<long, long>>> &packs)
{
    long res = 0;
    for (auto &pack : packs)
    {
        auto range = fidIndex[pack.first];
        res += std::get<1>(range) - std::get<0>(range);
    }
    return res;
}

/**
 * @brief
 *
 * @param packs
 * @return long
 */
long FileIDManager::GetPacksRhythmNum(vector<pair<char *, long>> &packs)
{
    long res = 0;
    for (auto &pack : packs)
    {
        string path = pack.first;
        auto range = fidIndex[path];
        res += std::get<1>(range) - std::get<0>(range);
    }
    return res;
}

void FileIDManager::GetSettings()
{
    ifstream t("./settings.json");
    stringstream buffer;
    buffer << t.rdbuf();
    string contents(buffer.str());
    neb::CJsonObject tmp(contents);
    settings = tmp;
    strcpy(Label, settings("Filename_Label").c_str());
}
neb::CJsonObject FileIDManager::GetSetting()
{
    ifstream t("settings.json");
    stringstream buffer;
    buffer << t.rdbuf();
    string contents(buffer.str());
    neb::CJsonObject tmp(contents);
    strcpy(Label, settings("Filename_Label").c_str());
    // pthread_create(&settingsWatcher, NULL, checkSettings, NULL);
    // settingsWatcher = thread(checkSettings);
    // settingsWatcher.detach();
    return tmp;
}

string FileIDManager::GetFileID(string path)
{
    string tmp = path;
    vector<string> paths = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
    string prefix = "Default";
    if (paths.size() > 0)
        prefix = paths[paths.size() - 1];
    // cout << prefix << endl;
    //去除首尾的‘/’，使其符合命名规范
    if (path[0] == '/')
        path.erase(path.begin());
    if (path[path.size() - 1] == '/')
        path.pop_back();

    // if (curNum[path] == 0)
    // {
    //     cout << "file list not read" << endl;
    //     vector<string> files;
    //     readFileList(path, files);

    //     filesListRead[path] = true;
    //     curNum[path] = files.size();
    //     cout << "now file num :" << curNum[path] << endl;
    // }
    curNum[path]++;
    if ((settings("Pack_Mode") == "quantitative") && (curNum[path] % atoi(settings("Pack_Num").c_str()) == 0))
    {
        Packer packer;
        vector<pair<string, long>> filesWithTime;
        readDataFilesWithTimestamps(path, filesWithTime);
        packer.Pack(path, filesWithTime);
    }
    return prefix + to_string(curNum[path]) + "_";
}