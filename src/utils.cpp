#include "../include/utils.hpp"
//#include "STDFB_header.h"
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
    packMode = settings("Pack_Mode");
    packNum = atoi(settings("Pack_Num").c_str());
    packTimeInterval = atol(settings("Pack_Interval").c_str());
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
    cout << prefix << endl;
    //去除首尾的‘/’，使其符合命名规范
    if (path[0] == '/')
        path.erase(path.begin());
    if (path[path.size() - 1] == '/')
        path.pop_back();

    if (!filesListRead[path])
    {
        cout << "file list not read" << endl;
        vector<string> files;
        readFileList(path, files);
        filesListRead[path] = true;
        curNum[path] = files.size();
        cout << "now file num :" << curNum[path] << endl;
    }
    curNum[path]++;
    if (curNum[path] % 1 == 0)
    {
        Packer packer;
        vector<pair<string, long>> filesWithTime;
        readDataFilesWithTimestamps(path, filesWithTime);
        packer.Pack("/", filesWithTime);
    }
    return prefix + to_string(curNum[path] + 1) + "_";
}

/**
 * @brief 获取绝对时间(自1970/1/1至今),精确到毫秒
 *
 * @return  时间值
 * @note
 */
long getMilliTime()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * 1000 + time.tv_usec / 1000;
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
 * @brief 在缓冲区中写入变量名的缓冲区头
 * @param pathCode      路径编码
 * @param typeList      数据类型列表
 * @param buffer        缓冲区地址
 *
 * @return  缓冲区头的长度
 * @note
 */
int Template::writeBufferHead(char *pathCode, vector<DataType> &typeList, char *buffer)
{
    vector<PathCode> pathCodes;
    this->GetAllPathsByCode(pathCode, pathCodes);
    buffer[0] = (unsigned char)pathCodes.size();
    long cur = 1; //当前数据头写位置
    for (int i = 0; i < pathCodes.size(); i++)
    {
        buffer[cur] = (char)getBufferValueType(typeList[i]);
        memcpy(buffer + cur + 1, pathCodes[i].code, 10);
        cur += 11;
    }
    return cur;
}

/**
 * @brief 在缓冲区中写入变量名的缓冲区头
 * @param name          变量名
 * @param type          数据类型
 * @param buffer        缓冲区地址
 *
 * @return  缓冲区头的长度
 * @note
 */
int Template::writeBufferHead(string name, DataType &type, char *buffer)
{
    for (auto &schema : this->schemas)
    {
        if (name == schema.first.name)
        {
            buffer[0] = (char)1;
            buffer[1] = (char)getBufferValueType(type);
            memcpy(buffer + 2, schema.first.code, 10);
            break;
        }
    }
    return 12;
}

int Template::GetAllPathsByCode(char pathCode[], vector<PathCode> &pathCodes)
{
    int level = 5; //路径级数
    for (int i = 10 - 1; i >= 0 && pathCode[i] == 0; i -= 2)
    {
        level--;
    }
    for (auto const &schema : this->schemas)
    {
        bool codeEquals = true;
        for (size_t k = 0; k < level * 2; k++) //判断路径编码前缀是否相等
        {
            if (pathCode[k] != schema.first.code[k])
            {
                codeEquals = false;
            }
        }
        if (codeEquals)
        {
            pathCodes.push_back(schema.first);
        }
    }
    return 0;
}

bool Template::checkHasArray(char *pathCode)
{
    long pos = 0;
    int level = 5; //路径级数
    for (int i = 10 - 1; i >= 0 && pathCode[i] == 0; i -= 2)
    {
        level--;
    }
    for (auto const &schema : this->schemas)
    {
        bool codeEquals = true;
        for (size_t k = 0; k < level * 2; k++) //判断路径编码前缀是否相等
        {
            if (pathCode[k] != schema.first.code[k])
            {
                codeEquals = false;
            }
        }
        if (codeEquals)
        {
            int num = 1;
            if (schema.second.isArray)
            {
                return true;
            }
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
        }
        else
        {
            int num = 1;
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
        }
    }
    return false;
}

long Template::GetTotalBytes()
{
    long total = 0;
    for (auto const &schema : this->schemas)
    {
        if (schema.second.isArray)
        {
            total += schema.second.hasTime ? 8 + schema.second.valueBytes * schema.second.arrayLen : schema.second.valueBytes * schema.second.arrayLen;
        }
        else
        {
            total += schema.second.hasTime ? 8 + schema.second.valueBytes : schema.second.valueBytes;
        }
    }
    return total;
}

long Template::FindSortPosFromSelectedData(vector<long> &bytesList, string name, char *pathCode, vector<DataType> &typeList)
{
    vector<PathCode> pathCodes;
    this->GetAllPathsByCode(pathCode, pathCodes);
    long cur = 0;
    for (int i = 0; i < pathCodes.size(); i++)
    {
        if (name == pathCodes[i].name)
        {
            return cur;
        }
        cur += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
    }
    return 0;
}

/**
 * @brief 根据当前模版寻找指定路径编码的数据在数据文件中的位置
 * @param pathCode        路径编码
 * @param position    数据起始位置
 * @param buff        文件数据
 * @param bytes       数据长度
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int Template::FindDatatypePosByCode(char pathCode[], char buff[], long &position, long &bytes)
{
    DataTypeConverter converter;
    long pos = 0;
    for (auto const &schema : this->schemas)
    {
        bool codeEquals = true;
        for (size_t k = 0; k < 10; k++) //判断路径编码是否相等
        {
            if (pathCode[k] != schema.first.code[k])
            {
                codeEquals = false;
            }
        }
        if (codeEquals)
        {
            position = pos;
            int num = 1;
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {
                    char imgLen[2];
                    imgLen[0] = buff[pos];
                    imgLen[1] = buff[pos + 1];
                    num = (int)converter.ToUInt16(imgLen);
                    position += 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            bytes = num * schema.second.valueBytes;
            return 0;
        }
        else
        {
            int num = 1;
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {
                    char imgLen[2];
                    imgLen[0] = buff[pos];
                    imgLen[1] = buff[pos + 1];
                    num = (int)converter.ToUInt16(imgLen) + 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
        }
    }
    return StatusCode::UNKNOWN_PATHCODE;
}

/**
 * @brief 根据当前模版寻找指定路径编码的数据在数据文件中的位置
 * @param pathCode        路径编码
 * @param position    数据起始位置
 * @param buff        文件数据
 * @param bytes       数据长度
 * @param type        数据类型
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int Template::FindDatatypePosByCode(char pathCode[], char buff[], long &position, long &bytes, DataType &type)
{
    DataTypeConverter converter;
    long pos = 0;
    for (auto const &schema : this->schemas)
    {
        bool codeEquals = true;
        for (size_t k = 0; k < 10; k++) //判断路径编码是否相等
        {
            if (pathCode[k] != schema.first.code[k])
            {
                codeEquals = false;
            }
        }
        if (codeEquals)
        {
            position = pos;
            int num = 1;
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {
                    char imgLen[2];
                    imgLen[0] = buff[pos];
                    imgLen[1] = buff[pos + 1];
                    num = (int)converter.ToUInt16(imgLen);
                    position += 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            bytes = num * schema.second.valueBytes;
            type = schema.second;
            return 0;
        }
        else
        {
            int num = 1;
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {

                    char imgLen[2];
                    imgLen[0] = buff[pos];
                    imgLen[1] = buff[pos + 1];
                    num = (int)converter.ToUInt16(imgLen) + 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
        }
    }
    return StatusCode::UNKNOWN_PATHCODE;
}

/**
 * @brief 根据当前模版模糊寻找指定路径编码的数据在数据文件中的位置，即获取模版中某一路径下所有孩子结点，包含自身
 * @param pathCode        路径编码
 * @param positions    数据起始位置
 * @param buff        文件数据
 * @param bytes       数据长度
 * @param types        数据类型
 *
 * @return  0:success,
 *          others: StatusCode
 * @note    图片数据目前暂时协定前2个字节为图片总长，不包括图片自身
 */
int Template::FindMultiDatatypePosByCode(char pathCode[], char buff[], vector<long> &positions, vector<long> &bytes, vector<DataType> &types)
{
    DataTypeConverter converter;
    long pos = 0;
    int level = 5; //路径级数
    for (int i = 10 - 1; i >= 0 && pathCode[i] == 0; i -= 2)
    {
        level--;
    }
    for (auto const &schema : this->schemas)
    {
        bool codeEquals = true;
        for (size_t k = 0; k < level * 2; k++) //判断路径编码前缀是否相等
        {
            if (pathCode[k] != schema.first.code[k])
            {
                codeEquals = false;
            }
        }
        if (codeEquals)
        {
            int num = 1;
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {

                    char imgLen[2];
                    imgLen[0] = buff[pos];
                    imgLen[1] = buff[pos + 1];
                    num = (int)converter.ToUInt16(imgLen);
                    pos += 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            positions.push_back(pos);
            bytes.push_back(num * schema.second.valueBytes);
            types.push_back(schema.second);
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
        }
        else
        {
            int num = 1;
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {

                    char imgLen[2];
                    imgLen[0] = buff[pos];
                    imgLen[1] = buff[pos + 1];
                    num = (int)converter.ToUInt16(imgLen) + 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
        }
    }
    return positions.size() == 0 ? StatusCode::UNKNOWN_PATHCODE : 0;
}

/**
 * @brief 根据当前模版寻找指定变量名的数据在数据文件中的位置
 * @param name        变量名
 * @param position    数据起始位置
 * @param buff        文件数据
 * @param bytes       数据长度
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int Template::FindDatatypePosByName(const char *name, char buff[], long &position, long &bytes)
{
    DataTypeConverter converter;
    long pos = 0;
    for (auto const &schema : this->schemas)
    {
        bool nameEquals = true;
        string str = name;
        if (str != schema.first.name)
        {
            nameEquals = false;
        }
        if (nameEquals)
        {
            position = pos;
            int num = 1;
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {

                    char imgLen[2];
                    imgLen[0] = buff[pos];
                    imgLen[1] = buff[pos + 1];
                    num = (int)converter.ToUInt16(imgLen);
                    position += 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            bytes = num * schema.second.valueBytes;
            return 0;
        }
        else
        {
            int num = 1;
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {

                    char imgLen[2];
                    imgLen[0] = buff[pos];
                    imgLen[1] = buff[pos + 1];
                    num = (int)converter.ToUInt16(imgLen) + 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
        }
    }
    return StatusCode::UNKNOWN_VARIABLE_NAME;
}

/**
 * @brief 根据当前模版寻找指定变量名的数据在数据文件中的位置
 * @param name        变量名
 * @param position    数据起始位置
 * @param buff        文件数据
 * @param bytes       数据长度
 * @param type        数据类型
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int Template::FindDatatypePosByName(const char *name, char buff[], long &position, long &bytes, DataType &type)
{
    DataTypeConverter converter;
    long pos = 0;
    for (auto const &schema : this->schemas)
    {
        bool nameEquals = true;
        string str = name;
        if (str != schema.first.name)
        {
            nameEquals = false;
        }
        if (nameEquals)
        {
            position = pos;
            int num = 1;
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {

                    char imgLen[2];
                    imgLen[0] = buff[pos];
                    imgLen[1] = buff[pos + 1];
                    num = (int)converter.ToUInt16(imgLen);
                    position += 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            bytes = num * schema.second.valueBytes;
            type = schema.second;
            return 0;
        }
        else
        {
            int num = 1;
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {

                    char imgLen[2];
                    imgLen[0] = buff[pos];
                    imgLen[1] = buff[pos + 1];
                    num = (int)converter.ToUInt16(imgLen) + 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
        }
    }
    return StatusCode::UNKNOWN_VARIABLE_NAME;
}

/**
 * @brief 将一个文件夹下的压缩或未压缩的数据文件打包为一个文件(.pak)
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
    long fp;
    char mode[2] = {'w', 'b'};
    DB_Open(const_cast<char *>(packageName.c_str()), mode, &fp);
    int filesNum = filesWithTime.size();
    long temBytes = CurrentTemplate.GetTotalBytes();
    int MAXSIZE = 2048 * 1024; // 2MB
    char *packBuffer = (char *)malloc(MAXSIZE);
    memset(packBuffer,0,MAXSIZE);
    DB_DataBuffer buffer;
    //写入包头
    memcpy(packBuffer, &filesNum, 4);
    memcpy(packBuffer + 4, CurrentTemplate.path.c_str(), CurrentTemplate.path.length() <= 20 ? CurrentTemplate.path.length() : 20);
    long cur = 24, packTotalSize = 0;
    for (auto &file : filesWithTime)
    {
        if (MAXSIZE - cur < temBytes) //缓冲区不足，Flush
        {
            fwrite(packBuffer, cur, 1, (FILE *)fp);
            memset(packBuffer,0,MAXSIZE);
            packTotalSize += cur;
            cur = 0;
        }
        memcpy(packBuffer + cur, &file.second, 8);
        cur += 8;
        char fileID[20] = {0};
        string tmp = file.first;
        string str = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "_")[0];
        while(str[0] == '/') //去除‘/’
            str.erase(str.begin());
        memcpy(fileID, str.c_str(), str.length() <= 20 ? str.length() : 20);
        memcpy(packBuffer + cur, fileID, 20);
        cur += 20;
        buffer.savePath = file.first.c_str();
        if (DB_ReadFile(&buffer) == 0)
        {
            if (buffer.length != 0)
            {
                if(file.first.find(".idbzip") == string::npos){
                    char type = 0;
                    memcpy(packBuffer + cur++,&type,1);
                }
                else{
                    char type = 2;
                    memcpy(packBuffer + cur++,&type,1);
                }
                int res = file.first.find(".idbzip") == string::npos ? 0 : 2;
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
 * @brief 读取缓存中的pak文件，curPos记录当前已读到的位置
 * @param buffer      本次读取到的单个文件数据
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int PackFileReader::Read(char *buffer)
{
    
}

// int main()
// {
//     // FileIDManager::GetFileID("/");
//     // FileIDManager::GetFileID("/");
//     // FileIDManager::GetFileID("/Jinfei3");
//     // FileIDManager::GetFileID("/Jinfei3");
//     // FileIDManager::GetFileID("/Jinfei4/line1");
//     // FileIDManager::GetFileID("/Jinfei4/line1/");
//     char buf[4];
//     char a = 2;
//     buf[2] = a;
//     Packer packer;
//     vector<pair<string, long>> files;
//     readDataFilesWithTimestamps("", files);
//     packer.Pack("", files);
//     return 0;
// }
