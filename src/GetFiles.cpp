#include "../include/utils.hpp"

/**
 * @brief 获取所有子文件夹
 *
 * @param dirs 获得的子文件夹
 * @param basePath 根目录
 */
void readAllDirs(vector<string> &dirs, string basePath)
{
    try
    {
        for (auto const &dir_entry : fs::recursive_directory_iterator{basePath})
        {
            if (fs::is_directory(dir_entry))
            {
                dirs.push_back(fs::path(basePath) / dir_entry.path().filename().string());
                // cout << dir_entry.path().filename() << endl;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 通过文件后缀获得相应的文件
 *
 * @param paths 获得的文件
 * @param path 路径
 * @param extension 后缀名
 * @param recursive 是否递归文件夹
 */
void readFiles(vector<string> &paths, string path, string extension, bool recursive)
{
    try
    {
        if (!recursive)
        {
            for (auto const &dir_entry : fs::directory_iterator{path})
            {
                if (fs::is_regular_file(dir_entry))
                {
                    fs::path file = dir_entry.path();
                    if (file.extension() == extension)
                    {
                        paths.push_back(fs::path(path) / file.filename().string());
                    }
                }
            }
        }
        else
        {
            for (auto const &dir_entry : fs::recursive_directory_iterator{path})
            {
                if (fs::is_regular_file(dir_entry))
                {
                    fs::path file = dir_entry.path();
                    if (file.extension() == extension)
                    {
                        paths.push_back(fs::path(path) / file.filename().string());
                    }
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 获取某一目录下的所有文件 不递归子文件夹
 *
 * @param path 文件路径
 * @param files 获得的文件
 */
void readFileList(string path, vector<string> &files)
{
    try
    {
        string finalPath = settings("Filename_Label");
        if (path[0] != '/')
            finalPath += "/" + path;
        else
            finalPath += path;
        for (auto const &dir_entry : fs::directory_iterator{finalPath})
        {
            if (fs::is_regular_file(dir_entry))
                files.push_back(fs::path(path) / dir_entry.path().filename().string());
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 获取.idb文件
 *
 * @param path 文件路径
 * @param files 获得的.idb文件
 */
void readIDBFilesList(string path, vector<string> &files)
{
    try
    {
        string finalPath = settings("Filename_Label");
        if (path[0] != '/')
            finalPath += "/" + path;
        else
            finalPath += path;
        for (auto const &dir_entry : fs::directory_iterator{finalPath})
        {
            if (fs::is_regular_file(dir_entry))
            {
                fs::path file = dir_entry.path();
                if (file.extension() == ".idb")
                {
                    files.push_back(fs::path(path) / file.filename().string());
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 获取.idbzip文件
 *
 * @param path 文件路径
 * @param files 获得的.idbzip文件
 */
void readIDBZIPFilesList(string path, vector<string> &files)
{
    try
    {
        string finalPath = settings("Filename_Label");
        if (path[0] != '/')
            finalPath += "/" + path;
        else
            finalPath += path;
        for (auto const &dir_entry : fs::directory_iterator{finalPath})
        {
            if (fs::is_regular_file(dir_entry))
            {
                fs::path file = dir_entry.path();
                if (file.extension() == ".idbzip")
                {
                    files.push_back(fs::path(path) / file.filename().string());
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 获取.tem文件
 *
 * @param path 文件路径
 * @param files 获得的.tem文件
 */
void readTEMFilesList(string path, vector<string> &files)
{
    string finalPath = settings("Filename_Label");
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    try
    {
        for (auto const &dir_entry : fs::directory_iterator(finalPath))
        {
            if (fs::is_regular_file(dir_entry) && dir_entry.path().extension() == ".tem")
            {
                files.push_back(path + "/" + dir_entry.path().filename().string());
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 获取.ziptem文件
 *
 * @param path 文件路径
 * @param files 获得的.ziptem文件
 */
void readZIPTEMFilesList(string path, vector<string> &files)
{
    try
    {
        string finalPath = settings("Filename_Label");
        if (path[0] != '/')
            finalPath += "/" + path;
        else
            finalPath += path;
        for (auto const &dir_entry : fs::directory_iterator{finalPath})
        {
            if (fs::is_regular_file(dir_entry))
            {
                fs::path file = dir_entry.path();
                if (file.extension() == ".ziptem")
                {
                    files.push_back(fs::path(path) / file.filename().string());
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 从指定文件夹中获取所有数据文件，并从文件名中取得时间戳
 * @param path             路径
 * @param filesWithTime    带有时间戳的文件名列表
 * @return
 * @note
 */
void readDataFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime)
{
    try
    {
        string finalPath = settings("Filename_Label");
        if (path[0] != '/')
            finalPath += "/" + path;
        else
            finalPath += path;
        for (auto const &dir_entry : fs::directory_iterator{finalPath})
        {
            if (fs::is_regular_file(dir_entry))
            {
                fs::path file = dir_entry.path();
                if (file.extension() == ".idbzip" || file.extension() == ".idb")
                {
                    string tmp = file.stem();
                    vector<string> time = DataType::StringSplit(const_cast<char *>(DataType::StringSplit(const_cast<char *>(tmp.c_str()), "_")[1].c_str()), "-");
                    if (time.size() == 0)
                    {
                        continue;
                    }
                    struct tm t;
                    t.tm_year = atoi(time[0].c_str()) - 1900;
                    t.tm_mon = atoi(time[1].c_str()) - 1;
                    t.tm_mday = atoi(time[2].c_str());
                    t.tm_hour = atoi(time[3].c_str());
                    t.tm_min = atoi(time[4].c_str());
                    t.tm_sec = atoi(time[5].c_str());
                    t.tm_isdst = -1; //不设置夏令时
                    time_t seconds = mktime(&t);
                    int ms = atoi(time[6].c_str());
                    long millis = seconds * 1000 + ms;
                    filesWithTime.push_back(make_pair(fs::path(path) / file.filename(), millis));
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 从指定文件夹中获取所有数据文件
 * @param path             路径
 * @param files            文件名列表
 * @return
 * @note
 */
void readDataFiles(string path, vector<string> &files)
{
    try
    {
        string finalPath = settings("Filename_Label");
        if (path[0] != '/')
            finalPath += "/" + path;
        else
            finalPath += path;
        for (auto const &dir_entry : fs::directory_iterator{finalPath})
        {
            if (fs::is_regular_file(dir_entry))
            {
                fs::path file = dir_entry.path();
                if (file.extension() == ".idbzip" || file.extension() == ".idb")
                    files.push_back(fs::path(path) / file.filename());
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 从指定文件夹中获取所有.idb文件，并从文件名中取得时间戳
 * @param path             路径
 * @param filesWithTime    带有时间戳的文件名列表
 * @return
 * @note
 */
void readIDBFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime)
{
    try
    {
        string finalPath = settings("Filename_Label");
        if (path[0] != '/')
            finalPath += "/" + path;
        else
            finalPath += path;
        for (auto const &dir_entry : fs::directory_iterator{finalPath})
        {
            if (fs::is_regular_file(dir_entry))
            {
                fs::path file = dir_entry.path();
                if (file.extension() == ".idb")
                {
                    string tmp = file.stem();
                    vector<string> time = DataType::StringSplit(const_cast<char *>(DataType::StringSplit(const_cast<char *>(tmp.c_str()), "_")[1].c_str()), "-");
                    if (time.size() == 0)
                    {
                        continue;
                    }
                    struct tm t;
                    t.tm_year = atoi(time[0].c_str()) - 1900;
                    t.tm_mon = atoi(time[1].c_str()) - 1;
                    t.tm_mday = atoi(time[2].c_str());
                    t.tm_hour = atoi(time[3].c_str());
                    t.tm_min = atoi(time[4].c_str());
                    t.tm_sec = atoi(time[5].c_str());
                    t.tm_isdst = -1; //不设置夏令时
                    time_t seconds = mktime(&t);
                    int ms = atoi(time[6].c_str());
                    long millis = seconds * 1000 + ms;
                    filesWithTime.push_back(make_pair(fs::path(path) / file.filename(), millis));
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 从指定文件夹中获取所有.idbzip文件，并从文件名中取得时间戳
 * @param path             路径
 * @param filesWithTime    带有时间戳的文件名列表
 *
 * @return
 * @note
 */
void readIDBZIPFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime)
{
    try
    {
        string finalPath = settings("Filename_Label");
        if (path[0] != '/')
            finalPath += "/" + path;
        else
            finalPath += path;
        for (auto const &dir_entry : fs::directory_iterator{finalPath})
        {
            if (fs::is_regular_file(dir_entry))
            {
                fs::path file = dir_entry.path();
                if (file.extension() == ".idbzip")
                {
                    string tmp = file.stem();
                    vector<string> time = DataType::StringSplit(const_cast<char *>(DataType::StringSplit(const_cast<char *>(tmp.c_str()), "_")[1].c_str()), "-");
                    if (time.size() == 0)
                    {
                        continue;
                    }
                    struct tm t;
                    t.tm_year = atoi(time[0].c_str()) - 1900;
                    t.tm_mon = atoi(time[1].c_str()) - 1;
                    t.tm_mday = atoi(time[2].c_str());
                    t.tm_hour = atoi(time[3].c_str());
                    t.tm_min = atoi(time[4].c_str());
                    t.tm_sec = atoi(time[5].c_str());
                    t.tm_isdst = -1; //不设置夏令时
                    time_t seconds = mktime(&t);
                    int ms = atoi(time[6].c_str());
                    long millis = seconds * 1000 + ms;
                    filesWithTime.push_back(make_pair(fs::path(path) / file.filename(), millis));
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 读取包文件列表
 *
 * @param path 文件夹路径
 * @param files 包文件列表
 */
void readPakFilesList(string path, vector<string> &files)
{
    try
    {
        string finalPath = settings("Filename_Label");
        if (path[0] != '/')
            finalPath += "/" + path;
        else
            finalPath += path;
        for (auto const &dir_entry : fs::directory_iterator{finalPath})
        {
            if (fs::is_regular_file(dir_entry))
            {
                fs::path file = dir_entry.path();
                if (file.extension() == ".pak")
                {
                    files.push_back(fs::path(path) / file.filename());
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 根据文件首ID及文件数量筛选出符合条件的.idb文件
 *
 * @param path 文件所在路径
 * @param SID 首ID
 * @param num 帅选数量
 * @param selectedFiles 筛选后符合条件的.idb文件
 * @return int
 */
int readIDBFilesListBySIDandNum(string path, string SID, uint32_t num, vector<pair<string, long>> &selectedFiles)
{
    int err = 0;
    vector<pair<string, long>> files;
    readIDBFilesWithTimestamps(path, files);
    if (files.size() < 1)
    {
        cout << "没有找到文件" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(files, TIME_ASC); //升序

    uint32_t count = 0; //用于记录个数

    //提取出第一个文件的ID与SID对比
    vector<string> firstFile = DataType::splitWithStl(files[0].first, "_");
    char firstFileID[10];
    memset(firstFileID, 0, sizeof(firstFileID));
    int pos = 0;
    for (int i = 0; i < firstFile[0].length(); i++)
    {
        if (isdigit(firstFile[0][i]))
        {
            firstFileID[pos] = firstFile[0][i];
            pos++;
        }
    }
    int firstID = atoi(firstFileID);

    //提取出SID与第一个文件的ID做对比
    char SIDnum[10];
    memset(SIDnum, 0, sizeof(SIDnum));
    pos = 0;
    for (int i = 0; i < SID.length(); i++)
    {
        if (isdigit(SID[i]))
        {
            SIDnum[pos] = SID[i];
            pos++;
        }
    }
    int S_ID = atoi(SIDnum);

    //提取出最后一个文件的ID与SID对比
    vector<string> lastFile = DataType::splitWithStl(files[files.size() - 1].first, "_");
    char lastFileID[10];
    memset(lastFileID, 0, sizeof(lastFileID));
    pos = 0;
    for (int i = 0; i < lastFile[0].length(); i++)
    {
        if (isdigit(lastFile[0][i]))
        {
            lastFileID[pos] = lastFile[0][i];
            pos++;
        }
    }
    int lastID = atoi(lastFileID);

    //如果SID比最后一个文件的ID大
    if (S_ID > lastID)
        return StatusCode::DATAFILE_NOT_FOUND;

    lastID = S_ID + (int)num;

    //如果SID比第一个文件的ID小
    if (S_ID < firstID)
    {
        if (S_ID + (int)num - firstID <= 0)
            return StatusCode::DATAFILE_NOT_FOUND;
        else
        {
            SID = files[0].first;   // SID从第一个文件开始
            count = firstID - S_ID; // count从两者的差开始
            S_ID = firstID;
        }
    }

    //开始筛选符合条件的.idb文件
    bool SIDFind = false;
    for (auto i = 0; i < files.size(); i++)
    {
        if (SIDFind == false) //还未找到SID
        {
            if (files[i].first.find(SID) != string::npos)
            {
                SIDFind = true;
                selectedFiles.push_back(files[i]);
                count++;
            }
            else
            {
                //提取出当前文件ID与SID对比
                vector<string> nowFile = DataType::splitWithStl(files[i].first, "_");
                char nowFileID[10];
                memset(nowFileID, 0, sizeof(nowFileID));
                pos = 0;
                for (int i = 0; i < nowFile[0].length(); i++)
                {
                    if (isdigit(nowFile[0][i]))
                    {
                        nowFileID[pos] = nowFile[0][i];
                        pos++;
                    }
                }
                int nowID = atoi(nowFileID);

                //如果当前ID大于文件SID+num,则结束
                if (nowID > lastID)
                    break;
                //当当前文件的ID大于SID时,说明SID文件不存在,从当前文件开始
                if (nowID >= S_ID)
                {
                    SIDFind = true;
                    selectedFiles.push_back(files[i]);
                    count += nowID - S_ID + 1;
                }
            }
        }
        else //已找到SID
        {
            //提取出当前文件ID与lastID对比
            vector<string> nowFile = DataType::splitWithStl(files[i].first, "_");
            char nowFileID[10];
            memset(nowFileID, 0, sizeof(nowFileID));
            pos = 0;
            for (int i = 0; i < nowFile[0].length(); i++)
            {
                if (isdigit(nowFile[0][i]))
                {
                    nowFileID[pos] = nowFile[0][i];
                    pos++;
                }
            }
            int nowID = atoi(nowFileID);

            //如果当前ID大于文件SID+num,则结束
            if (nowID > lastID)
                break;
            if (count < num)
            {
                selectedFiles.push_back(files[i]);
                count++;
            }
            else
                break;
        }
    }
    if (SIDFind == false)
        return StatusCode::DATAFILE_NOT_FOUND;
    return err;
}

/**
 * @brief 根据文件首ID及EID筛选出符合条件的.idb文件
 *
 * @param path 文件所在路径
 * @param SID 起始ID
 * @param EID 结束ID
 * @param selectedFiles 筛选出的符合条件的.idb文件
 * @return int
 */
int readIDBFilesListBySIDandEID(string path, string SID, string EID, vector<pair<string, long>> &selectedFiles)
{
    int err = 0;
    vector<pair<string, long>> files;
    readIDBFilesWithTimestamps(path, files);
    if (files.size() < 1)
    {
        cout << "没有找到文件" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(files, TIME_ASC); //升序

    //提取出第一个文件的ID与EID对比
    vector<string> firstFile = DataType::splitWithStl(files[0].first, "_");
    char firstFileID[10];
    memset(firstFileID, 0, sizeof(firstFileID));
    int pos = 0;
    for (int i = 0; i < firstFile[0].length(); i++)
    {
        if (isdigit(firstFile[0][i]))
        {
            firstFileID[pos] = firstFile[0][i];
            pos++;
        }
    }
    int firstID = atoi(firstFileID);

    //提取出最后一个文件的ID与SID对比
    vector<string> lastFile = DataType::splitWithStl(files[files.size() - 1].first, "_");
    char lastFileID[10];
    memset(lastFileID, 0, sizeof(lastFileID));
    pos = 0;
    for (int i = 0; i < lastFile[0].length(); i++)
    {
        if (isdigit(lastFile[0][i]))
        {
            lastFileID[pos] = lastFile[0][i];
            pos++;
        }
    }
    int lastID = atoi(lastFileID);

    //提取出SID与最后一个文件的ID做对比
    char SIDnum[10];
    memset(SIDnum, 0, sizeof(SIDnum));
    pos = 0;
    for (int i = 0; i < SID.length(); i++)
    {
        if (isdigit(SID[i]))
        {
            SIDnum[pos] = SID[i];
            pos++;
        }
    }
    int S_ID = atoi(SIDnum);

    //提取出EID与第一个文件的ID做对比
    char EIDnum[10];
    memset(EIDnum, 0, sizeof(EIDnum));
    pos = 0;
    for (int i = 0; i < EID.length(); i++)
    {
        if (isdigit(EID[i]))
        {
            SIDnum[pos] = EID[i];
            pos++;
        }
    }
    int E_ID = atoi(SIDnum);

    //如果SID比EID大
    if (S_ID > E_ID)
        return StatusCode::ERROR_SID_EID_RANGE;

    //如果SID比最后一个文件ID大 或者 EID比第一个文件的ID小
    if (S_ID > lastID || E_ID < firstID)
        return StatusCode::DATAFILE_NOT_FOUND;

    //如果SID比第一个文件ID小
    if (S_ID < firstID)
    {
        SID = files[0].first;
        S_ID = firstID;
    }

    //如果EID比最后一个文件ID大
    if (E_ID > lastID)
    {
        EID = files[files.size() - 1].first;
        E_ID = lastID;
    }

    //筛选出符合条件的文件
    bool SIDFind = false;
    bool EIDFind = false;
    for (auto i = 0; i < files.size(); i++)
    {
        if (SIDFind == false) //未找到SID
        {
            if (files[i].first.find(SID) != string::npos)
            {
                SIDFind = true;
                selectedFiles.push_back(files[i]);
            }
            else
            {
                //提取出当前文件ID与SID对比
                vector<string> nowFile = DataType::splitWithStl(files[i].first, "_");
                char nowFileID[10];
                memset(nowFileID, 0, sizeof(nowFileID));
                pos = 0;
                for (int i = 0; i < nowFile[0].length(); i++)
                {
                    if (isdigit(nowFile[0][i]))
                    {
                        nowFileID[pos] = nowFile[0][i];
                        pos++;
                    }
                }
                int nowID = atoi(nowFileID);
                //当当前文件的ID大于SID时,说明SID文件不存在,从当前文件开始
                if (nowID >= S_ID)
                {
                    SIDFind = true;
                    selectedFiles.push_back(files[i]);
                }
            }
        }
        else //已找到SID
        {
            if (EIDFind == false) //未找到EID
            {
                if (files[i].first.find(EID) != string::npos)
                {
                    EIDFind = true;
                    selectedFiles.push_back(files[i]);
                }
                else
                {
                    //提取出当前与EID对比
                    vector<string> nowFile = DataType::splitWithStl(files[i].first, "_");
                    char nowFileID[10];
                    memset(nowFileID, 0, sizeof(nowFileID));
                    pos = 0;
                    for (int i = 0; i < nowFile[0].length(); i++)
                    {
                        if (isdigit(nowFile[0][i]))
                        {
                            nowFileID[pos] = nowFile[0][i];
                            pos++;
                        }
                    }
                    int nowID = atoi(nowFileID);
                    //当当前文件的ID大于EID时,说明EID文件不存在,从当前文件结束
                    if (nowID >= E_ID)
                        EIDFind = true;
                    else
                        selectedFiles.push_back(files[i]);
                }
            }
            else //已找到EID
                break;
        }
    }
    if (SIDFind == false)
        return StatusCode::DATAFILE_NOT_FOUND;
    return err;
}

/**
 * @brief 根据文件首ID及文件数量筛选出符合条件的.idbzip文件
 *
 * @param path 文件所在路径
 * @param SID 首ID
 * @param num 筛选数量
 * @param selectedFiles 筛选出符合条件的.idbzip文件
 * @return int
 */
int readIDBZIPFilesListBySIDandNum(string path, string SID, uint32_t num, vector<pair<string, long>> &selectedFiles)
{
    int err = 0;
    vector<pair<string, long>> files;
    readIDBZIPFilesWithTimestamps(path, files);
    if (files.size() < 1)
    {
        cout << "没有找到文件" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(files, TIME_ASC); //升序

    uint32_t count = 0; //用于记录个数

    //提取出第一个文件的ID与SID对比
    vector<string> firstFile = DataType::splitWithStl(files[0].first, "_");
    char firstFileID[10];
    memset(firstFileID, 0, sizeof(firstFileID));
    int pos = 0;
    for (int i = 0; i < firstFile[0].length(); i++)
    {
        if (isdigit(firstFile[0][i]))
        {
            firstFileID[pos] = firstFile[0][i];
            pos++;
        }
    }
    int firstID = atoi(firstFileID);

    //提取出SID与第一个文件的ID做对比
    char SIDnum[10];
    memset(SIDnum, 0, sizeof(SIDnum));
    pos = 0;
    for (int i = 0; i < SID.length(); i++)
    {
        if (isdigit(SID[i]))
        {
            SIDnum[pos] = SID[i];
            pos++;
        }
    }
    int S_ID = atoi(SIDnum);

    //提取出最后一个文件的ID与SID对比
    vector<string> lastFile = DataType::splitWithStl(files[files.size() - 1].first, "_");
    char lastFileID[10];
    memset(lastFileID, 0, sizeof(lastFileID));
    pos = 0;
    for (int i = 0; i < lastFile[0].length(); i++)
    {
        if (isdigit(lastFile[0][i]))
        {
            lastFileID[pos] = lastFile[0][i];
            pos++;
        }
    }
    int lastID = atoi(lastFileID);

    //如果SID比最后一个文件的ID大
    if (S_ID > lastID)
        return StatusCode::DATAFILE_NOT_FOUND;

    lastID = firstID + (int)num;

    //如果SID比第一个文件的ID小
    if (S_ID < firstID)
    {
        if ((S_ID + (int)num - firstID) <= 0)
            return StatusCode::DATAFILE_NOT_FOUND;
        else
        {
            SID = files[0].first;   // SID从第一个文件开始
            count = firstID - S_ID; // count从两者的差开始
            S_ID = firstID;
        }
    }

    //帅选出符合条件的文件
    bool SIDFind = false;
    for (auto i = 0; i < files.size(); i++)
    {
        if (SIDFind == false) //未找到SID
        {
            if (files[i].first.find(SID) != string::npos)
            {
                SIDFind = true;
                selectedFiles.push_back(files[i]);
                count++;
            }
            else
            {
                //提取出当前文件ID与SID对比
                vector<string> nowFile = DataType::splitWithStl(files[i].first, "_");
                char nowFileID[10];
                memset(nowFileID, 0, sizeof(nowFileID));
                pos = 0;
                for (int i = 0; i < nowFile[0].length(); i++)
                {
                    if (isdigit(nowFile[0][i]))
                    {
                        nowFileID[pos] = nowFile[0][i];
                        pos++;
                    }
                }
                int nowID = atoi(nowFileID);

                //如果当前ID大于文件SID+num,则结束
                if (nowID > lastID)
                    break;
                //当当前文件的ID大于SID时,说明SID文件不存在,从当前文件开始
                if (nowID >= S_ID)
                {
                    SIDFind = true;
                    selectedFiles.push_back(files[i]);
                }
            }
        }
        else //已找到SID
        {
            //提取出当前文件ID与lastID对比
            vector<string> nowFile = DataType::splitWithStl(files[i].first, "_");
            char nowFileID[10];
            memset(nowFileID, 0, sizeof(nowFileID));
            pos = 0;
            for (int i = 0; i < nowFile[0].length(); i++)
            {
                if (isdigit(nowFile[0][i]))
                {
                    nowFileID[pos] = nowFile[0][i];
                    pos++;
                }
            }
            int nowID = atoi(nowFileID);

            //如果当前ID大于文件SID+num,则结束
            if (nowID > lastID)
                break;
            if (count < num)
            {
                selectedFiles.push_back(files[i]);
                count++;
            }
            else
                break;
        }
    }
    if (SIDFind == false)
        return StatusCode::DATAFILE_NOT_FOUND;
    return err;
}

/**
 * @brief 根据SID和EID筛选出符合条件的.idbzip文件
 *
 * @param path 存储路径
 * @param SID 起始ID
 * @param EID 结束ID
 * @param selectedFiles 帅选后结果
 * @return int
 */
int readIDBZIPFilesListBySIDandEID(string path, string SID, string EID, vector<pair<string, long>> &selectedFiles)
{
    int err = 0;
    vector<pair<string, long>> files;
    readIDBZIPFilesWithTimestamps(path, files);
    if (files.size() < 1)
    {
        cout << "没有找到文件" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(files, TIME_ASC); //升序

    //提取出第一个文件的ID与EID对比
    vector<string> firstFile = DataType::splitWithStl(files[0].first, "_");
    char firstFileID[10];
    memset(firstFileID, 0, sizeof(firstFileID));
    int pos = 0;
    for (int i = 0; i < firstFile[0].length(); i++)
    {
        if (isdigit(firstFile[0][i]))
        {
            firstFileID[pos] = firstFile[0][i];
            pos++;
        }
    }
    int firstID = atoi(firstFileID);

    //提取出最后一个文件的ID与SID对比
    vector<string> lastFile = DataType::splitWithStl(files[files.size() - 1].first, "_");
    char lastFileID[10];
    memset(lastFileID, 0, sizeof(lastFileID));
    pos = 0;
    for (int i = 0; i < lastFile[0].length(); i++)
    {
        if (isdigit(lastFile[0][i]))
        {
            lastFileID[pos] = lastFile[0][i];
            pos++;
        }
    }
    int lastID = atoi(lastFileID);

    //提取出SID与最后一个文件的ID做对比
    char SIDnum[10];
    memset(SIDnum, 0, sizeof(SIDnum));
    pos = 0;
    for (int i = 0; i < SID.length(); i++)
    {
        if (isdigit(SID[i]))
        {
            SIDnum[pos] = SID[i];
            pos++;
        }
    }
    int S_ID = atoi(SIDnum);

    //提取出EID与第一个文件的ID做对比
    char EIDnum[10];
    memset(EIDnum, 0, sizeof(EIDnum));
    pos = 0;
    for (int i = 0; i < EID.length(); i++)
    {
        if (isdigit(EID[i]))
        {
            SIDnum[pos] = EID[i];
            pos++;
        }
    }
    int E_ID = atoi(SIDnum);

    //如果SID比EID大
    if (S_ID > E_ID)
        return StatusCode::ERROR_SID_EID_RANGE;

    //如果SID比最后一个文件ID大 或者 EID比第一个文件的ID小
    if (S_ID > lastID || E_ID < firstID)
        return StatusCode::DATAFILE_NOT_FOUND;

    //如果SID比第一个文件ID小
    if (S_ID < firstID)
    {
        SID = files[0].first;
        S_ID = firstID;
    }

    //如果EID比最后一个文件ID大
    if (E_ID > lastID)
    {
        EID = files[files.size() - 1].first;
        E_ID = lastID;
    }

    //开始筛选符合条件的.idbzip文件
    bool SIDFind = false;
    bool EIDFind = false;
    for (auto i = 0; i < files.size(); i++)
    {
        if (SIDFind == false) //未找到SID
        {
            if (files[i].first.find(SID) != string::npos)
            {
                SIDFind = true;
                selectedFiles.push_back(files[i]);
            }
            else
            {
                //提取出当前文件ID与SID对比
                vector<string> nowFile = DataType::splitWithStl(files[i].first, "_");
                char nowFileID[10];
                memset(nowFileID, 0, sizeof(nowFileID));
                pos = 0;
                for (int i = 0; i < nowFile[0].length(); i++)
                {
                    if (isdigit(nowFile[0][i]))
                    {
                        nowFileID[pos] = nowFile[0][i];
                        pos++;
                    }
                }
                int nowID = atoi(nowFileID);
                //当当前文件的ID大于SID时,说明SID文件不存在,从当前文件开始
                if (nowID >= S_ID)
                {
                    SIDFind = true;
                    selectedFiles.push_back(files[i]);
                }
            }
        }
        else //已找到SID
        {
            if (EIDFind == false) //为找到EID
            {
                if (files[i].first.find(EID) != string::npos)
                {
                    EIDFind = true;
                    selectedFiles.push_back(files[i]);
                }
                else
                {
                    //提取出当前与EID对比
                    vector<string> nowFile = DataType::splitWithStl(files[i].first, "_");
                    char nowFileID[10];
                    memset(nowFileID, 0, sizeof(nowFileID));
                    pos = 0;
                    for (int i = 0; i < nowFile[0].length(); i++)
                    {
                        if (isdigit(nowFile[0][i]))
                        {
                            nowFileID[pos] = nowFile[0][i];
                            pos++;
                        }
                    }
                    int nowID = atoi(nowFileID);
                    //当当前文件的ID大于EID时,说明EID文件不存在,从当前文件结束
                    if (nowID >= E_ID)
                        EIDFind = true;
                    else
                        selectedFiles.push_back(files[i]);
                }
            }
            else //已找到EID
                break;
        }
    }
    if (SIDFind == false)
        return StatusCode::DATAFILE_NOT_FOUND;
    return err;
}