#include "../include/utils.hpp"
//获取某一目录下的所有文件
//不递归子文件夹
void readFileList(string path, vector<string> &files)
{
    //FileIDManager::GetSettings();
    struct dirent *ptr;
    DIR *dir;
    string finalPath = Label;
    finalPath += path;
    //cout<<Label<<endl;
    cout<<finalPath.c_str()<<endl;
    dir = opendir(finalPath.c_str());
    //cout<<finalPath<<endl;
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
    //FileIDManager::GetSettings();
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

//获取绝对时间(自1970/1/1至今)
//精确到毫秒
long getMilliTime()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * 1000 + time.tv_usec / 1000;
}


    /**
     * @brief 根据当前模版寻找指定路径编码的数据在数据文件中的位置
     * @param name        变量名
     * @param position    数据起始位置
     * @param bytes       数据长度
     *
     * @return  0:success,
     *          others: StatusCode
     * @note
     */
int Template::FindDatatypePosByCode(char pathCode[], long &position, long &bytes)
{
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
            }
            pos += num * schema.second.valueBytes;
        }
    }
    return StatusCode::UNKNOWN_PATHCODE;
}

 /**
     * @brief 根据当前模版寻找指定路径编码的数据在数据文件中的位置
     * @param name        变量名
     * @param position    数据起始位置
     * @param bytes       数据长度
     * @param type        数据类型
     *
     * @return  0:success,
     *          others: StatusCode
     * @note
     */
int Template::FindDatatypePosByCode(char pathCode[], long &position, long &bytes, DataType &type)
{
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
            }
            pos += num * schema.second.valueBytes;
        }
    }
    return StatusCode::UNKNOWN_PATHCODE;
}

/**
     * @brief 根据当前模版寻找指定变量名的数据在数据文件中的位置
     * @param name        变量名
     * @param position    数据起始位置
     * @param bytes       数据长度
     *
     * @return  0:success,
     *          others: StatusCode
     * @note
     */
int Template::FindDatatypePosByName(const char *name, long &position, long &bytes)
{
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
            }
            pos += num * schema.second.valueBytes;
        }
    }
    return StatusCode::UNKNOWN_VARIABLE_NAME;
}

/**
     * @brief 根据当前模版寻找指定变量名的数据在数据文件中的位置
     * @param name        变量名
     * @param position    数据起始位置
     * @param bytes       数据长度
     * @param type        数据类型
     *
     * @return  0:success,
     *          others: StatusCode
     * @note
     */
int Template::FindDatatypePosByName(const char *name, long &position, long &bytes, DataType &type)
{
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
            }
            pos += num * schema.second.valueBytes;
        }
    }
    return StatusCode::UNKNOWN_VARIABLE_NAME;
}