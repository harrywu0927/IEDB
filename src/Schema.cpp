#include "../include/utils.hpp"
using namespace std;

vector<ZipTemplate> ZipTemplates;
ZipTemplate CurrentZipTemplate;

/**
 * @brief 向标准模板添加新节点
 *
 * @param params 标准模板参数
 * @return　0:success,
 *         others: StatusCode
 * @note   新节点参数必须齐全，pathcode valueNmae hasTime valueType isArray arrayLen
 */
int DB_AddNodeToSchema(struct DB_TreeNodeParams *params)
{
    int err;
    vector<string> files;
    readFileList(params->pathToLine, files);
    string temPath = "";
    for (string file : files) //找到带有后缀tem的文件
    {
        string s = file;
        vector<string> vec = DataType::StringSplit(const_cast<char *>(s.c_str()), ".");
        if (vec[vec.size() - 1].find("tem") != string::npos && vec[vec.size() - 1].find("ziptem") == string::npos)
        {
            temPath = file;
            break;
        }
    }
    if (temPath == "")
        return StatusCode::SCHEMA_FILE_NOT_FOUND;

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    for (long i = 0; i < len / 71; i++) //寻找是否有相同的变量名或者编码
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(params->valueName, existValueName) == 0)
        {
            cout << "存在相同的变量名" << endl;
            return StatusCode::VARIABLE_NAME_EXIST;
        }
        readbuf_pos += 61;

        char existPathCode[10];
        memcpy(existPathCode, readBuf + readbuf_pos, 10);
        int j = 0;
        for (j = 0; j < 10; j++)
        {
            if (params->pathCode[j] != existPathCode[j])
                break;
        }
        if (j == 10)
        {
            cout << "存在相同的编码" << endl;
            return StatusCode::PATHCODE_EXIST;
        }
        readbuf_pos += 10;
    }
    char valueNmae[30], hasTime[1], pathCode[10], valueType[30]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(pathCode, 0, sizeof(pathCode));

    if (params->isArrary == 1) //是数组类型,拼接字符串
    {
        strcpy(valueType, "ARRAY [0..");
        char s[10];
        sprintf(s, "%d", params->arrayLen);
        strcat(valueType, s);
        strcat(valueType, "] OF ");
        string valueTypeStr = DataType::JudgeValueTypeByNum(params->valueType);
        // memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
        strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
    }
    else //不是数组类型，直接赋值字符串
    {
        string valueTypeStr = DataType::JudgeValueTypeByNum(params->valueType);
        memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
    }
    strcpy(valueNmae, params->valueName);
    hasTime[0] = (char)params->hasTime;
    memcpy(pathCode, params->pathCode, 10);

    char writeBuf[71]; //将所有参数传入writeBuf中，已追加写的方式写入已有模板文件中
    memcpy(writeBuf, valueNmae, 30);
    memcpy(writeBuf + 30, valueType, 30);
    memcpy(writeBuf + 60, hasTime, 1);
    memcpy(writeBuf + 61, pathCode, 10);

    //打开文件并追加写入
    long fp;
    err = DB_Open(const_cast<char *>(temPath.c_str()), "ab+", &fp);
    if (err == 0)
    {
        err = DB_Write(fp, writeBuf, 71);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    return err;
}

/**
 * @brief 修改标准模板中已存在的节点,根据编码进行定位和修改
 *
 * @param params 标准模板参数
 * @return　0:success,
 *         others: StatusCode
 * @note   更新节点参数必须齐全，pathcode valueNmae hasTime valueType isArray arrayLen
 */
int DB_UpdateNodeToSchema(struct DB_TreeNodeParams *params, struct DB_TreeNodeParams *newTreeParams)
{
    int err;
    vector<string> files;
    readFileList(params->pathToLine, files);
    string temPath = "";
    for (string file : files) //找到带有后缀tem的文件
    {
        string s = file;
        vector<string> vec = DataType::StringSplit(const_cast<char *>(s.c_str()), ".");
        if (vec[vec.size() - 1].find("tem") != string::npos && vec[vec.size() - 1].find("ziptem") == string::npos)
        {
            temPath = file;
            break;
        }
    }
    if (temPath == "")
        return StatusCode::SCHEMA_FILE_NOT_FOUND;

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    long pos = -1;                      //用于定位是修改模板的第几条
    for (long i = 0; i < len / 71; i++) //寻找是否有相同编码
    {
        readbuf_pos += 61;
        char existPathCode[10];
        memcpy(existPathCode, readBuf + readbuf_pos, 10);
        int j = 0;
        for (j = 0; j < 10; j++)
        {
            if (params->pathCode[j] != existPathCode[j])
                break;
        }
        if (j == 10)
        {
            pos = i;
            break;
        }
        readbuf_pos += 10;
    }
    if (pos == -1)
    {
        cout << "未找到编码！" << endl;
        return StatusCode::UNKNOWN_PATHCODE;
    }

    readbuf_pos = 0;
    for (long i = 0; i < len / 71; i++) //寻找模板是否有与更新参数相同的变量名或者编码
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(newTreeParams->valueName, existValueName) == 0 && i != pos)
        {
            cout << "更新参数存在模板已有的变量名" << endl;
            return StatusCode::VARIABLE_NAME_EXIST;
        }
        readbuf_pos += 61;

        char existPathCode[10];
        memcpy(existPathCode, readBuf + readbuf_pos, 10);
        int j = 0;
        for (j = 0; j < 10; j++)
        {
            if (newTreeParams->pathCode[j] != existPathCode[j])
                break;
        }
        if (j == 10 && i != pos)
        {
            cout << "更新参数存在模板已有的编码" << endl;
            return StatusCode::PATHCODE_EXIST;
        }
        readbuf_pos += 10;
    }

    char valueNmae[30], hasTime[1], pathCode[10], valueType[30]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(pathCode, 0, sizeof(pathCode));

    if (newTreeParams->isArrary == 1) //是数组类型,拼接字符串
    {
        strcpy(valueType, "ARRAY [0..");
        char s[10];
        sprintf(s, "%d", newTreeParams->arrayLen);
        strcat(valueType, s);
        strcat(valueType, "] OF ");
        string valueTypeStr = DataType::JudgeValueTypeByNum(newTreeParams->valueType);
        strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
    }
    else //不是数组类型，直接赋值字符串
    {
        string valueTypeStr = DataType::JudgeValueTypeByNum(newTreeParams->valueType);
        memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
    }

    strcpy(valueNmae, newTreeParams->valueName);
    hasTime[0] = (char)newTreeParams->hasTime;
    memcpy(pathCode, newTreeParams->pathCode, 10);

    //将所有参数传入readBuf中，已覆盖写的方式写入已有模板文件中
    memcpy(readBuf + 71 * pos, valueNmae, 30);
    memcpy(readBuf + 71 * pos + 30, valueType, 30);
    memcpy(readBuf + 71 * pos + 60, hasTime, 1);
    memcpy(readBuf + 71 * pos + 61, pathCode, 10);

    //打开文件并覆盖写入
    long fp;
    err = DB_Open(const_cast<char *>(temPath.c_str()), "wb+", &fp);
    if (err == 0)
    {
        err = DB_Write(fp, readBuf, len);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    return err;
}

/**
 * @brief 删除标准模板中已存在的节点,根据编码进行定位和删除
 *
 * @param params 标准模板参数
 * @return　0:success,
 *         others: StatusCode
 */
int DB_DeleteNodeToSchema(struct DB_TreeNodeParams *params)
{
    int err;
    vector<string> files;
    readFileList(params->pathToLine, files);
    string temPath = "";
    for (string file : files) //找到带有后缀tem的文件
    {
        string s = file;
        vector<string> vec = DataType::StringSplit(const_cast<char *>(s.c_str()), ".");
        if (vec[vec.size() - 1].find("tem") != string::npos && vec[vec.size() - 1].find("ziptem") == string::npos)
        {
            temPath = file;
            break;
        }
    }
    if (temPath == "")
        return StatusCode::SCHEMA_FILE_NOT_FOUND;

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    long pos = -1;                      //记录删除节点在第几条
    for (long i = 0; i < len / 71; i++) //寻找是否有相同的编码
    {
        readbuf_pos += 61;
        char existPathCode[10];
        memcpy(existPathCode, readBuf + readbuf_pos, 10);
        int j = 0;
        for (j = 0; j < 10; j++)
        {
            if (params->pathCode[j] != existPathCode[j])
                break;
        }
        if (j == 10)
        {
            pos = i;
            break;
        }
        readbuf_pos += 10;
    }
    if (pos == -1)
    {
        cout << "未找到编码！" << endl;
        return StatusCode::UNKNOWN_PATHCODE;
    }

    char writeBuf[len - 71];
    memcpy(writeBuf, readBuf, pos * 71);                                       //拷贝要被删除的记录之前的记录
    memcpy(writeBuf + pos * 71, readBuf + pos * 71 + 71, len - pos * 71 - 71); //拷贝要被删除的记录之后的记录

    //打开文件并追加写入
    long fp;
    err = DB_Open(const_cast<char *>(temPath.c_str()), "wb+", &fp);
    if (err == 0)
    {
        if (len - 71 != 0)
            err = DB_Write(fp, writeBuf, len - 71);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    return err;
}

/**
 * @brief 加载压缩模板
 *
 * @param path 压缩模板路径
 * @return 0:success,
 *         others: StatusCode
 */
int DB_LoadZipSchema(const char *path)
{
    vector<string> files;
    readFileList(path, files);
    string temPath = "";
    for (string file : files) //找到带有后缀ziptem的文件
    {
        string s = file;
        vector<string> vec = DataType::StringSplit(const_cast<char *>(s.c_str()), ".");
        if (vec[vec.size() - 1].find("ziptem") != string::npos)
        {
            temPath = file;
            break;
        }
    }
    if (temPath == "")
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    long length;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &length);
    char buf[length];
    int err = DB_OpenAndRead(const_cast<char *>(temPath.c_str()), buf);
    if (err != 0)
        return err;
    int i = 0;

    DataTypeConverter dtc;
    vector<string> dataName;
    vector<DataType> dataTypes;
    while (i < length)
    {
        char variable[30], dataType[30], standardValue[10], maxValue[10], minValue[10];
        memcpy(variable, buf + i, 30);
        i += 30;
        memcpy(dataType, buf + i, 30);
        i += 30;
        memcpy(standardValue, buf + i, 10);
        i += 10;
        memcpy(maxValue, buf + i, 10);
        i += 10;
        memcpy(minValue, buf + i, 10);
        i += 10;
        vector<string> paths;

        dataName.push_back(variable);
        DataType type;

        memcpy(type.standardValue, standardValue, 10);
        memcpy(type.maxValue, maxValue, 10);
        memcpy(type.minValue, minValue, 10);
        // strcpy(type.standardValue,standardValue);
        // strcpy(type.maxValue,maxValue);
        // strcpy(type.minValue,minValue);
        if (DataType::GetDataTypeFromStr(dataType, type) == 0)
        {
            dataTypes.push_back(type);
        }
    }

    ZipTemplate tem(dataName, dataTypes, path);
    return ZipTemplateManager::SetTemplate(tem);
}

/**
 * @brief 卸载当前压缩模板
 *
 * @param pathToUnset 压缩模板路径
 * @return  0:success,
 *          other:StatusCode
 */
int DB_UnloadZipSchema(const char *pathToUnset)
{
    return ZipTemplateManager::UnsetZipTemplate(pathToUnset);
}

int main()
{
    // char test[len];
    // memcpy(test,readbuf,len);
    // cout<<test<<endl;
    // DB_TreeNodeParams params;
    // params.pathToLine = "/";
    // char code[10];
    // code[0] = (char)0;
    // code[1] = (char)1;
    // code[2] = (char)0;
    // code[3] = (char)4;
    // code[4] = 'R';
    // code[5] = (char)1;
    // code[6] = 0;
    // code[7] = (char)0;
    // code[8] = (char)0;
    // code[9] = (char)0;
    // params.pathCode = code;
    // params.valueType = 3;
    // params.hasTime = 0;
    // params.isArrary = 0;
    // params.arrayLen = 100;
    // params.valueName = "S4ON";

    // DB_TreeNodeParams newTreeParams;
    // newTreeParams.pathToLine = "/";
    // char newcode[10];
    // newcode[0] = (char)0;
    // newcode[1] = (char)1;
    // newcode[2] = (char)0;
    // newcode[3] = (char)4;
    // newcode[4] = 'R';
    // newcode[5] = (char)1;
    // newcode[6] = 0;
    // newcode[7] = (char)0;
    // newcode[8] = (char)0;
    // newcode[9] = (char)0;
    // newTreeParams.pathCode = newcode;
    // newTreeParams.valueType = 3;
    // newTreeParams.hasTime = 0;
    // newTreeParams.isArrary = 0;
    // newTreeParams.arrayLen = 100;
    // newTreeParams.valueName = "S4ON";
    // DB_UpdateNodeToSchema(&params,&newTreeParams);
    // DB_AddNodeToSchema(&params);
    // DB_DeleteNodeToSchema(&params);
    return 0;
}
