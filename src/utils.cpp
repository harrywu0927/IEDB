#include "../include/utils.hpp"

unordered_map<string, int> getDirCurrentFileIDIndex();
long getMilliTime();

int maxTemplates = 20;
vector<Template> templates;
int errorCode;
neb::CJsonObject settings = FileIDManager::GetSetting();
unordered_map<string, int> curNum = getDirCurrentFileIDIndex();
//unordered_map<string, bool> filesListRead;

//  string packMode;       //定时打包或存储一定数量后打包
//  int packNum;           //一次打包的文件数量
//  long packTimeInterval; //定时打包时间间隔

//递归获取所有子文件夹
void readAllDirs(vector<string> &dirs, string basePath)
{

    DIR *dir;
    struct dirent *ptr;
    if ((dir = opendir(basePath.c_str())) == NULL)
    {
        perror("Error while opening directory");
        return;
    }

    while ((ptr = readdir(dir)) != NULL)
    {
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0) /// current dir OR parrent dir
            continue;
        else if (ptr->d_type == 4) /// dir
        {
            string base = basePath;
            base += "/";
            base += ptr->d_name;
            dirs.push_back(base);
            readAllDirs(dirs, base);
        }
    }
    closedir(dir);
}

//获取当前工程根目录下所有产线-已有最大文件ID键值对
unordered_map<string, int> getDirCurrentFileIDIndex()
{
    struct dirent *ptr;
    DIR *dir;
    string finalPath = settings("Filename_Label");
    unordered_map<string, int> map;
    if (DB_CreateDirectory(const_cast<char *>(finalPath.c_str())))
    {
        errorCode = errno;
        return map;
    }
    vector<string> dirs;
    dirs.push_back(settings("Filename_Label"));
    readAllDirs(dirs, finalPath);
    // cout<<dirs.size()<<endl;
    for (auto &d : dirs)
    {
        dir = opendir(d.c_str());
        if (dir == NULL)
            return map;
        long max = 0;
        while ((ptr = readdir(dir)) != NULL)
        {
            if (ptr->d_name[0] == '.')
                continue;
            if (ptr->d_type == 8)
            {
                string fileName = ptr->d_name;
                string dirWithoutPrefix = d + "/" + fileName;
                for (int i = 0; i <= settings("Filename_Label").length(); i++)
                {
                    dirWithoutPrefix.erase(dirWithoutPrefix.begin());
                }

                if (fileName.find(".pak") != string::npos)
                {
                    PackFileReader packReader(dirWithoutPrefix);
                    if (packReader.packBuffer == NULL)
                        continue;
                    int fileNum;
                    string templateName;
                    packReader.ReadPackHead(fileNum, templateName);
                    string fileID;
                    int readLength, zipType;
                    for (int i = 0; i < fileNum; i++)
                    {
                        packReader.Next(readLength, fileID, zipType);
                    }
                    string num;
                    for (int i = 0; i < fileID.length(); i++)
                    {
                        if (isdigit(fileID[i]))
                        {
                            num += fileID[i];
                        }
                    }
                    if (max < atol(num.c_str()))
                        max = atol(num.c_str());
                }
                else if (fileName.find(".idb") != string::npos)
                {
                    vector<string> vec = DataType::StringSplit(const_cast<char *>(fileName.c_str()), "_");
                    string num;
                    string fileID = vec[0];
                    for (int i = 0; i < fileID.length(); i++)
                    {
                        if (isdigit(fileID[i]))
                        {
                            num += fileID[i];
                        }
                    }
                    if (max < atol(num.c_str()))
                        max = atol(num.c_str());
                }
            }
        }
        closedir(dir);
        if (d == settings("Filename_Label"))
            map["/"] = max;
        else //去除前缀Label
        {
            for (int i = 0; i <= settings("Filename_Label").length(); i++)
            {
                d.erase(d.begin());
            }
            map[d] = max;
        }
    }
    return map;
}

//获取某一目录下的所有文件
//不递归子文件夹
void readFileList(string path, vector<string> &files)
{
    // FileIDManager::GetSettings();
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    if (DB_CreateDirectory(const_cast<char *>(finalPath.c_str())))
    {
        errorCode = errno;
        return;
    }
    dir = opendir(finalPath.c_str());
    // cout<<finalPath<<endl;
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            files.push_back(p.append(path).append("/").append(ptr->d_name));
        }
    }
    closedir(dir);
}

//获取.idb文件路径
void readIDBFilesList(string path, vector<string> &files)
{
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    if (DB_CreateDirectory(const_cast<char *>(finalPath.c_str())))
    {
        errorCode = errno;
        return;
    }
    dir = opendir(finalPath.c_str());
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;
            if ((datafile.find(".idb") != string::npos) && (datafile.find(".idbzip") == string::npos))
                files.push_back(p.append(path).append("/").append(ptr->d_name));
        }
    }
    closedir(dir);
}

//获取.idbzip文件路径
void readIDBZIPFilesList(string path, vector<string> &files)
{
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    if (DB_CreateDirectory(const_cast<char *>(finalPath.c_str())))
    {
        errorCode = errno;
        return;
    }
    dir = opendir(finalPath.c_str());
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;

            if (datafile.find(".idbzip") != string::npos)
                files.push_back(p.append(path).append("/").append(ptr->d_name));
        }
    }
    closedir(dir);
}

/**
 * @brief 从指定文件夹中获取所有数据文件，并从文件名中取得时间戳
 * @param path             路径
 * @param filesWithTime    带有时间戳的文件名列表
 *
 * @return
 * @note
 */
void readDataFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime)
{
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    dir = opendir(finalPath.c_str());
    if (dir == NULL)
        return;
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;
            if (datafile.find(".idbzip") != string::npos || datafile.find(".idb") != string::npos)
            {
                string tmp = datafile;
                vector<string> time = DataType::StringSplit(const_cast<char *>(DataType::StringSplit(const_cast<char *>(tmp.c_str()), "_")[1].c_str()), "-");
                if (time.size() == 0)
                {
                    continue;
                }
                time[time.size() - 1] = DataType::StringSplit(const_cast<char *>(time[time.size() - 1].c_str()), ".")[0];
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
                filesWithTime.push_back(make_pair(p.append(path).append("/").append(ptr->d_name), millis));
            }
        }
    }
    closedir(dir);
}

/**
 * @brief 从指定文件夹中获取所有数据文件
 * @param path             路径
 * @param files            文件名列表
 *
 * @return
 * @note
 */
void readDataFiles(string path, vector<string> &files)
{
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    dir = opendir(finalPath.c_str());
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;
            if (datafile.find(".idbzip") != string::npos || datafile.find(".idb") != string::npos)
            {
                files.push_back(p.append(path).append("/").append(ptr->d_name));
            }
        }
    }
    closedir(dir);
}

/**
 * @brief 从指定文件夹中获取所有.idb文件，并从文件名中取得时间戳
 * @param path             路径
 * @param filesWithTime    带有时间戳的文件名列表
 *
 * @return
 * @note
 */
void readIDBFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime)
{
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    dir = opendir(finalPath.c_str());
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;
            if (datafile.find(".idbzip") == string::npos && datafile.find(".idb") != string::npos)
            {
                string tmp = datafile;
                vector<string> time = DataType::StringSplit(const_cast<char *>(DataType::StringSplit(const_cast<char *>(tmp.c_str()), "_")[1].c_str()), "-");
                if (time.size() == 0)
                {
                    continue;
                }
                time[time.size() - 1] = DataType::StringSplit(const_cast<char *>(time[time.size() - 1].c_str()), ".")[0];
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
                filesWithTime.push_back(make_pair(p.append(path).append("/").append(ptr->d_name), millis));
            }
        }
    }
    closedir(dir);
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
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    dir = opendir(finalPath.c_str());
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;
            if (datafile.find(".idbzip") != string::npos)
            {
                string tmp = datafile;
                vector<string> time = DataType::StringSplit(const_cast<char *>(DataType::StringSplit(const_cast<char *>(tmp.c_str()), "_")[1].c_str()), "-");
                if (time.size() == 0)
                {
                    continue;
                }
                time[time.size() - 1] = DataType::StringSplit(const_cast<char *>(time[time.size() - 1].c_str()), ".")[0];
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
                filesWithTime.push_back(make_pair(p.append(path).append("/").append(ptr->d_name), millis));
            }
        }
    }
    closedir(dir);
}

void readPakFilesList(string path, vector<string> &files)
{
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    if (path[0] != '/')
        finalPath += "/" + path;
    else
        finalPath += path;
    if (DB_CreateDirectory(const_cast<char *>(finalPath.c_str())))
    {
        errorCode = errno;
        return;
    }
    dir = opendir(finalPath.c_str());
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_name[0] == '.')
            continue;

        if (ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;
            if (datafile.find(".pak") != string::npos)
                files.push_back(p.append(path).append("/").append(ptr->d_name));
        }
    }
    closedir(dir);
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
    ifstream t("./settings.json");
    stringstream buffer;
    buffer << t.rdbuf();
    string contents(buffer.str());
    neb::CJsonObject tmp(contents);
    strcpy(Label, settings("Filename_Label").c_str());
    // packMode = settings("Pack_Mode");
    // packNum = atoi(settings("Pack_Num").c_str());
    // cout<<packNum<<endl;
    // packTimeInterval = atol(settings("Pack_Interval").c_str());
    // cout<<packTimeInterval<<endl;
    return tmp;
}

string FileIDManager::GetFileID(string path)
{
    /*
        临时使用，今后需修改
        获取此路径下文件数量，以文件数量+1作为ID
    */
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

/**
 * @brief 获取绝对时间(自1970/1/1至今),精确到毫秒
 *
 * @return  时间值
 * @note
 */
long getMilliTime()
{
#ifdef WIN32
    struct timeval tv;
    time_t clock;
    struct tm tm;
    SYSTEMTIME wtm;

    GetLocalTime(&wtm);
    tm.tm_year = wtm.wYear - 1900;
    tm.tm_mon = wtm.wMonth - 1;
    tm.tm_mday = wtm.wDay;
    tm.tm_hour = wtm.wHour;
    tm.tm_min = wtm.wMinute;
    tm.tm_sec = wtm.wSecond;
    tm.tm_isdst = -1;
    clock = mktime(&tm);
    tv.tv_sec = clock;
    tv.tv_usec = wtm.wMilliseconds * 1000;
    return ((unsigned long long)tv.tv_sec * 1000 + (unsigned long long)tv.tv_usec / 1000);
#else
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * 1000 + time.tv_usec / 1000;
#endif
}

void removeFilenameLabel(string &path)
{
    for (int i = 0; i < settings("Filename_Label").length(); i++)
    {
        path.erase(path.begin());
    }
    while (path[0] == '/')
        path.erase(path.begin());
}

int getMemory(long size, char *mem)
{
    mem = (char *)malloc(size);
    if (mem == NULL)
    {
        errorCode = StatusCode::BUFFER_FULL;
        return errorCode;
    }
    return 0;
}

int getBufferValueType(DataType &type)
{
    int baseType = (int)type.valueType;
    if (type.hasTime && !type.isArray)
        return baseType + 10;
    if (!type.hasTime && type.isArray)
        return baseType + 20;
    if (type.isArray && type.hasTime)
        return baseType + 30;
    return baseType;
}

/**
 * @brief 获取数据区中第N个变量在每行中的偏移量
 * @param typeList      数据类型列表
 * @param num           第num个
 *
 * @return  偏移量
 * @note
 */
int getBufferDataPos(vector<DataType> &typeList, int num)
{
    long pos = 0;
    for (int i = 0; i < num; i++)
    {
        pos += typeList[i].hasTime ? typeList[i].valueBytes + 8 : typeList[i].valueBytes;
    }

    return pos;
}

/**
 * @brief 在进行所有的查询操作前，检查输入的查询参数是否合法，若可能引发严重错误，则不允许进入查询
 * @param params        查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int CheckQueryParams(DB_QueryParams *params)
{
    if (params->pathToLine == NULL)
    {
        return StatusCode::EMPTY_PATH_TO_LINE;
    }
    if (params->pathCode == NULL && params->valueName == NULL)
    {
        return StatusCode::NO_PATHCODE_OR_NAME;
    }
    if (params->compareType != CMP_NONE && params->valueName == NULL)
    {
        return StatusCode::VARIABLE_NOT_ASSIGNED;
    }
    if (params->compareType != CMP_NONE && params->compareValue == NULL)
    {
        return StatusCode::NO_COMPARE_VALUE;
    }
    switch (params->queryType)
    {
    case TIMESPAN:
    {
        if (params->start == 0 || params->end == 0)
        {
            return StatusCode::INVALID_TIMESPAN;
        }
        break;
    }
    case LAST:
    {
        if (params->queryNums == 0)
        {
            return StatusCode::NO_QUERY_NUM;
        }
        break;
    }
    case FILEID:
    {
        if (params->fileID == NULL)
        {
            return StatusCode::NO_FILEID;
        }
        break;
    }
    default:
        return StatusCode::NO_QUERY_TYPE;
        break;
    }
    return 0;
}

/**
 * @brief 在进行压缩操作前，检查输入的压缩参数是否合法
 * @param params        压缩请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int CheckZipParams(DB_ZipParams *params)
{
    if (params->pathToLine == NULL)
    {
        return StatusCode::EMPTY_PATH_TO_LINE;
    }
    switch (params->ZipType)
    {
    case TIME_SPAN:
    {
        if (params->start == 0 || params->end == 0)
        {
            return StatusCode::INVALID_TIMESPAN;
        }
        break;
    }
    case PATH_TO_LINE:
    {
        if (params->pathToLine == 0)
        {
            return StatusCode::EMPTY_PATH_TO_LINE;
        }
        break;
    }
    case FILE_ID:
    {
        if (params->fileID == NULL)
        {
            return StatusCode::NO_FILEID;
        }
        break;
    }
    default:
        return StatusCode::NO_ZIP_TYPE;
        break;
    }
    return 0;
}

bool IsNormalIDBFile(char *readbuff, const char *pathToLine)
{
    int err = 0;
    err = DB_LoadZipSchema(pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;
    long readbuff_pos = 0;

    for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++) //循环比较
    {
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::BOOL) // BOOL变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                return false;
            }
            else
            {
                char standardBool = CurrentZipTemplate.schemas[i].second.standardValue[0];
                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readbuff_pos, 1);
                char currentUSintValue = value[0];

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                {
                    if (standardBool != readbuff[readbuff_pos])
                        return false;
                    else
                        readbuff_pos += 9;
                }
                else //不带时间戳
                {
                    if (standardBool != readbuff[readbuff_pos])
                        return false;
                    else
                        readbuff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                return false;
            }
            else
            {
                char standardUSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char maxUSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                char minUSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];
                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readbuff_pos, 1);
                char currentUSintValue = value[0];

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                {
                    if (currentUSintValue != standardUSintValue && (currentUSintValue < minUSintValue || currentUSintValue > maxUSintValue))
                        return false;
                    else
                        readbuff_pos += 9;
                }
                else //不带时间戳
                {
                    if (currentUSintValue != standardUSintValue && (currentUSintValue < minUSintValue || currentUSintValue > maxUSintValue))
                        return false;
                    else
                        readbuff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                return false;
            }
            else
            {
                ushort standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                ushort maxUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                ushort minUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, readbuff + readbuff_pos, 2);
                ushort currentUintValue = converter.ToUInt16(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                {
                    if (currentUintValue != standardUintValue && (currentUintValue < minUintValue || currentUintValue > maxUintValue))
                        return false;
                    else
                        readbuff_pos += 10;
                }
                else //不带时间戳
                {
                    if (currentUintValue != standardUintValue && (currentUintValue < minUintValue || currentUintValue > maxUintValue))
                        return false;
                    else
                        readbuff_pos += 2;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                return false;
            }
            else
            {
                uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                uint32 maxUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                uint32 minUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                uint32 currentUDintValue = converter.ToUInt32(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime) //带时间戳
                {
                    if (currentUDintValue != standardUDintValue && (currentUDintValue < minUDintValue || currentUDintValue > maxUDintValue))
                        return false;
                    else
                        readbuff_pos += 12;
                }
                else //不带时间戳
                {
                    if (currentUDintValue != standardUDintValue && (currentUDintValue < minUDintValue || currentUDintValue > maxUDintValue))
                        return false;
                    else
                        readbuff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                return false;
            }
            else
            {
                char standardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char maxSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                char minSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readbuff_pos, 1);
                char currentSintValue = value[0];

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                {
                    if (currentSintValue != standardSintValue && (currentSintValue < minSintValue || currentSintValue > maxSintValue))
                        return false;
                    else
                        readbuff_pos += 9;
                }
                else //不带时间戳
                {
                    if (currentSintValue != standardSintValue && (currentSintValue < minSintValue || currentSintValue > maxSintValue))
                        return false;
                    else
                        readbuff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                return false;
            }
            else
            {
                short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                short maxIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                short minIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, readbuff + readbuff_pos, 2);
                short currentIntValue = converter.ToInt16(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    if (currentIntValue != standardIntValue && (currentIntValue < minIntValue || currentIntValue > maxIntValue))
                        return false;
                    else
                        readbuff_pos += 10;
                }
                else //不带时间戳
                {
                    if (currentIntValue != standardIntValue && (currentIntValue < minIntValue || currentIntValue > maxIntValue))
                        return false;
                    else
                        readbuff_pos += 2;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                return false;
            }
            else
            {
                int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                int maxDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                int minDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                int currentDintValue = converter.ToInt32(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    if (currentDintValue != standardDintValue && (currentDintValue < minDintValue || currentDintValue > maxDintValue))
                        return false;
                    else
                        readbuff_pos += 12;
                }
                else //不带时间戳
                {
                    if (currentDintValue != standardDintValue && (currentDintValue < minDintValue || currentDintValue > maxDintValue))
                        return false;
                    else
                        readbuff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                return false;
            }
            else
            {
                float standardFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                float maxFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.maxValue);
                float minFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                int currentRealValue = converter.ToFloat(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    if (currentRealValue != standardFloatValue && (currentRealValue < minFloatValue || currentRealValue > maxFloatValue))
                        return false;
                    else
                        readbuff_pos += 12;
                }
                else //不带时间戳
                {
                    if (currentRealValue != standardFloatValue && (currentRealValue < minFloatValue || currentRealValue > maxFloatValue))
                        return false;
                    else
                        readbuff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::IMAGE)
        {
            return false;
        }
    }
    return true;
}

int ReZipBuff(char *buff, int &buffLength, const char *pathToLine)
{
    int err = 0;
    err = DB_LoadZipSchema(pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;

    long len = buffLength;
    char writebuff[CurrentZipTemplate.totalBytes]; //写入没有被压缩的数据
    long readbuff_pos = 0;
    long writebuff_pos = 0;

    for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++)
    {
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::BOOL) // BOOL类型
        {
            if (len == 0) //表示文件完全压缩
            {
                char standardBoolValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char BoolValue[1] = {0};
                BoolValue[0] = standardBoolValue;
                memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
                writebuff_pos += 1;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 9);
                                writebuff_pos += 9;
                                readbuff_pos += 9;
                            }
                            else if (buff[readbuff_pos - 1] == (char)1) //只有时间
                            {
                                char standardBoolValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                char BoolValue[1] = {0};
                                BoolValue[0] = standardBoolValue;
                                memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
                                writebuff_pos += 1;

                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 1);
                                writebuff_pos += 1;
                                readbuff_pos += 1;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        char standardBoolValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                        char BoolValue[1] = {0};
                        BoolValue[0] = standardBoolValue;
                        memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
                        writebuff_pos += 1;
                    }
                }
                else //没有未压缩的数据了
                {
                    char standardBoolValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                    char BoolValue[1] = {0};
                    BoolValue[0] = standardBoolValue;
                    memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
                    writebuff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                char UDintValue[4] = {0};
                for (int j = 0; j < 4; j++)
                {
                    UDintValue[3 - j] |= standardUDintValue;
                    standardUDintValue >>= 8;
                }
                memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                writebuff_pos += 4;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 12);
                                writebuff_pos += 12;
                                readbuff_pos += 12;
                            }
                            else if (buff[readbuff_pos - 1] == (char)1) //只有时间
                            {
                                uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                char UDintValue[4] = {0};
                                for (int j = 0; j < 4; j++)
                                {
                                    UDintValue[3 - j] |= standardUDintValue;
                                    standardUDintValue >>= 8;
                                }
                                memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                                writebuff_pos += 4;

                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char UDintValue[4] = {0};
                        for (int j = 0; j < 4; j++)
                        {
                            UDintValue[3 - j] |= standardUDintValue;
                            standardUDintValue >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                        writebuff_pos += 4;
                    }
                }
                else //没有未压缩的数据了
                {
                    uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char UDintValue[4] = {0};
                    for (int j = 0; j < 4; j++)
                    {
                        UDintValue[3 - j] |= standardUDintValue;
                        standardUDintValue >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                    writebuff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char UsintValue[1] = {0};
                UsintValue[0] = StandardUsintValue;
                memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                writebuff_pos += 1;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 9);
                                writebuff_pos += 9;
                                readbuff_pos += 9;
                            }
                            else if (buff[readbuff_pos - 1] == (char)1) //只有时间
                            {
                                char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                char UsintValue[1] = {0};
                                UsintValue[0] = StandardUsintValue;
                                memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                                writebuff_pos += 1;

                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 1);
                                writebuff_pos += 1;
                                readbuff_pos += 1;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                        char UsintValue[1] = {0};
                        UsintValue[0] = StandardUsintValue;
                        memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                        writebuff_pos += 1;
                    }
                }
                else //没有未压缩的数据了
                {
                    char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                    char UsintValue[1] = {0};
                    UsintValue[0] = StandardUsintValue;
                    memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                    writebuff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                char UintValue[2] = {0};
                for (int j = 0; j < 2; j++)
                {
                    UintValue[1 - j] |= standardUintValue;
                    standardUintValue >>= 8;
                }
                memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                writebuff_pos += 2;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 10);
                                writebuff_pos += 10;
                                readbuff_pos += 10;
                            }
                            else if (buff[readbuff_pos - 1] == (char)1) //只有时间
                            {
                                uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                char UintValue[2] = {0};
                                for (int j = 0; j < 2; j++)
                                {
                                    UintValue[1 - j] |= standardUintValue;
                                    standardUintValue >>= 8;
                                }
                                memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                                writebuff_pos += 2;

                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 2);
                                writebuff_pos += 2;
                                readbuff_pos += 2;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char UintValue[2] = {0};
                        for (int j = 0; j < 2; j++)
                        {
                            UintValue[1 - j] |= standardUintValue;
                            standardUintValue >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                        writebuff_pos += 2;
                    }
                }
                else //没有未压缩的数据了
                {
                    uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char UintValue[2] = {0};
                    for (int j = 0; j < 2; j++)
                    {
                        UintValue[1 - j] |= standardUintValue;
                        standardUintValue >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                    writebuff_pos += 2;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char SintValue[1] = {0};
                SintValue[0] = StandardSintValue;
                memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                writebuff_pos += 1;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 9);
                                writebuff_pos += 9;
                                readbuff_pos += 9;
                            }
                            else if (buff[readbuff_pos - 1] == (char)1) //只有时间
                            {
                                char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                char SintValue[1] = {0};
                                SintValue[0] = StandardSintValue;
                                memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                                writebuff_pos += 1;

                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 1);
                                writebuff_pos += 1;
                                readbuff_pos += 1;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                        char SintValue[1] = {0};
                        SintValue[0] = StandardSintValue;
                        memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                        writebuff_pos += 1;
                    }
                }
                else //没有未压缩的数据了
                {
                    char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                    char SintValue[1] = {0};
                    SintValue[0] = StandardSintValue;
                    memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                    writebuff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                char IntValue[2] = {0};
                for (int j = 0; j < 2; j++)
                {
                    IntValue[1 - j] |= standardIntValue;
                    standardIntValue >>= 8;
                }
                memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                writebuff_pos += 2;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 10);
                                writebuff_pos += 10;
                                readbuff_pos += 10;
                            }
                            else if (buff[readbuff_pos - 1] == (char)1) //只有时间
                            {
                                short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                char IntValue[2] = {0};
                                for (int j = 0; j < 2; j++)
                                {
                                    IntValue[1 - j] |= standardIntValue;
                                    standardIntValue >>= 8;
                                }
                                memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                                writebuff_pos += 2;

                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 2);
                                writebuff_pos += 2;
                                readbuff_pos += 2;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char IntValue[2] = {0};
                        for (int j = 0; j < 2; j++)
                        {
                            IntValue[1 - j] |= standardIntValue;
                            standardIntValue >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                        writebuff_pos += 2;
                    }
                }
                else //没有未压缩的数据了
                {
                    short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char IntValue[2] = {0};
                    for (int j = 0; j < 2; j++)
                    {
                        IntValue[1 - j] |= standardIntValue;
                        standardIntValue >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                    writebuff_pos += 2;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                char DintValue[4] = {0};
                for (int j = 0; j < 4; j++)
                {
                    DintValue[3 - j] |= standardDintValue;
                    standardDintValue >>= 8;
                }
                memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                writebuff_pos += 4;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 12);
                                writebuff_pos += 12;
                                readbuff_pos += 12;
                            }
                            else if (buff[readbuff_pos - 1] == (char)1) //只有时间
                            {
                                int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                char DintValue[4] = {0};
                                for (int j = 0; j < 4; j++)
                                {
                                    DintValue[3 - j] |= standardDintValue;
                                    standardDintValue >>= 8;
                                }
                                memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                                writebuff_pos += 4;

                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char DintValue[4] = {0};
                        for (int j = 0; j < 4; j++)
                        {
                            DintValue[3 - j] |= standardDintValue;
                            standardDintValue >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                        writebuff_pos += 4;
                    }
                }
                else //没有未压缩的数据了
                {
                    int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char DintValue[4] = {0};
                    for (int j = 0; j < 4; j++)
                    {
                        DintValue[3 - j] |= standardDintValue;
                        standardDintValue >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                    writebuff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL类型
        {
            if (len == 0) //表示文件完全压缩
            {
                float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                char RealValue[4] = {0};
                void *pf;
                pf = &standardRealValue;
                for (int j = 0; j < 4; j++)
                {
                    *((unsigned char *)pf + j) = RealValue[j];
                }
                memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                writebuff_pos += 4;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (buff[readbuff_pos - 1] == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 12);
                                writebuff_pos += 12;
                                readbuff_pos += 12;
                            }
                            else if (buff[readbuff_pos - 1] == (char)1) //只有时间
                            {
                                float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                char RealValue[4] = {0};
                                void *pf;
                                pf = &standardRealValue;
                                for (int j = 0; j < 4; j++)
                                {
                                    *((unsigned char *)pf + j) = RealValue[j];
                                }
                                memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                                writebuff_pos += 4;

                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char RealValue[4] = {0};
                        void *pf;
                        pf = &standardRealValue;
                        for (int j = 0; j < 4; j++)
                        {
                            *((unsigned char *)pf + j) = RealValue[j];
                        }
                        memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                        writebuff_pos += 4;
                    }
                }
                else //没有未压缩的数据了
                {
                    float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char RealValue[4] = {0};
                    void *pf;
                    pf = &standardRealValue;
                    for (int j = 0; j < 4; j++)
                    {
                        *((unsigned char *)pf + j) = RealValue[j];
                    }
                    memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                    writebuff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::IMAGE) // IMAGE类型
        {
            //对比编号是否等于当前模板所在条数
            char zipPosNum[2] = {0};
            memcpy(zipPosNum, buff + readbuff_pos, 2);
            uint16_t posCmp = converter.ToUInt16(zipPosNum);
            if (posCmp == i) //是未压缩数据的编号
            {
                readbuff_pos += 2;
                //暂定２个字节的图片长度
                char length[2] = {0};
                memcpy(length, buff + readbuff_pos, 2);
                uint16_t imageLength = converter.ToUInt16(length);

                memcpy(writebuff + writebuff_pos, buff + readbuff_pos, 2); //图片长度也存
                writebuff_pos += 2;
                readbuff_pos += 3;

                if (buff[readbuff_pos - 1] == (char)2) //既有数据又有数据
                {
                    //存储图片
                    memcpy(writebuff + writebuff_pos, buff + readbuff_pos, imageLength + 8);
                    writebuff_pos += imageLength + 8;
                    readbuff_pos += imageLength + 8;
                }
                else if (buff[readbuff_pos - 1] == (char)0) //只有数据
                {
                    //存储图片
                    memcpy(writebuff + writebuff_pos, buff + readbuff_pos, imageLength);
                    writebuff_pos += imageLength;
                    readbuff_pos += imageLength;
                }
                else
                {
                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                    return StatusCode::ZIPTYPE_ERROR;
                }
            }
            else //不是未压缩的编号
            {
                cout << "图片还原出现问题！" << endl;
                return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            }
        }
        else
        {
            cout << "存在未知数据类型，请检查模板" << endl;
            return StatusCode::UNKNOWN_TYPE;
        }
    }
    memcpy(buff, writebuff, writebuff_pos);
    buffLength = (int)writebuff_pos;
    return err;
}
// int main()
// {
//     // FileIDManager::GetFileID("/");
//     // FileIDManager::GetFileID("/");
//     // FileIDManager::GetFileID("/Jinfei3");
//     // FileIDManager::GetFileID("/Jinfei3");
//     // FileIDManager::GetFileID("/Jinfei4/line1");
//     // FileIDManager::GetFileID("/Jinfei4/line1/");
//     // cout << settings("Pack_Mode") << " " << settings("Pack_Num") << " " << settings("Pack_Interval") << endl;
//     // char *buff=NULL;
//     // int length=0;
//     // DB_ZipSwitchFile("/","/");

//     // ReZipBuff(buff, length, "/");
//     // cout<<length<<endl;
//     // return 0;
//     vector<string> vec = DataType::splitWithStl("jinfei/","/");
//     //curNum = getDirCurrentFileIDIndex();
//     return 0;
//     char *buff = (char *)malloc(24);
//     buff[0] = 0;
//     buff[1] = 0;
//     buff[2] = 0;
//     buff[3] = 0;
//     buff[4] = 0;
//     buff[5] = 103;
//     int len = 6;
//     ReZipBuff(buff, len, "/");
//     Packer packer;
//     vector<pair<string, long>> files;
//     readDataFilesWithTimestamps("", files);
//     packer.Pack("", files);
//     return 0;
// }
