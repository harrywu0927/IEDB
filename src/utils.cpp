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
    finalPath += path;
    // cout<<Label<<endl;
    cout << finalPath.c_str() << endl;
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

void readIDBFilesList(string path, vector<string> &files)
{
    // FileIDManager::GetSettings();
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
            if (datafile.find(".idb") != string::npos)
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
void readIDBFilesWithTimestamps(string path, vector<pair<string, long>> &filesWithTime)
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
            if (datafile.find(".idb") != string::npos)
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
    return tmp;
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
                num = schema.second.arrayLen;
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
                num = schema.second.arrayLen;
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
                num = schema.second.arrayLen;
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
                num = schema.second.arrayLen;
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
                num = schema.second.arrayLen;
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
                num = schema.second.arrayLen;
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
                num = schema.second.arrayLen;
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
                num = schema.second.arrayLen;
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
                num = schema.second.arrayLen;
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
                num = schema.second.arrayLen;
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
