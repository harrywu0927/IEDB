#include "../include/utils.hpp"
using namespace std;

vector<ZipTemplate> ZipTemplates;
ZipTemplate CurrentZipTemplate;

/**
 * @brief 向标准模板添加新节点,暂定直接在尾部追加
 *
 * @param TreeParams 标准模板参数
 * @return　0:success,
 *         others: StatusCode
 * @note   新节点参数必须齐全，pathToLine pathcode valueNmae hasTime valueType isArray arrayLen
 */
int DB_AddNodeToSchema(struct DB_TreeNodeParams *TreeParams)
{
    int err;
    vector<string> files;
    readFileList(TreeParams->pathToLine, files);
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
        if (strcmp(TreeParams->valueName, existValueName) == 0)
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
            if (TreeParams->pathCode[j] != existPathCode[j])
                break;
        }
        if (j == 10)
        {
            cout << "存在相同的编码" << endl;
            return StatusCode::PATHCODE_EXIST;
        }
        readbuf_pos += 10;
    }

    if (TreeParams->valueType < 1 || TreeParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    if (TreeParams->isArrary != 0 && TreeParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    if (TreeParams->hasTime != 0 && TreeParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    char valueNmae[30], hasTime[1], pathCode[10], valueType[30]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(pathCode, 0, sizeof(pathCode));

    if (TreeParams->isArrary == 1) //是数组类型,拼接字符串
    {
        if (TreeParams->arrayLen < 1)
        {
            cout << "数组长度不能小于１" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
        strcpy(valueType, "ARRAY [0..");
        char s[10];
        sprintf(s, "%d", TreeParams->arrayLen);
        strcat(valueType, s);
        strcat(valueType, "] OF ");
        string valueTypeStr = DataType::JudgeValueTypeByNum(TreeParams->valueType);
        // memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
        strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
    }
    else //不是数组类型，直接赋值字符串
    {
        string valueTypeStr = DataType::JudgeValueTypeByNum(TreeParams->valueType);
        memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
    }

    strcpy(valueNmae, TreeParams->valueName);
    hasTime[0] = (char)TreeParams->hasTime;
    memcpy(pathCode, TreeParams->pathCode, 10);

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

//会重新创建一个.tem文件,根据已存在的tem文件数量追加命名
int DB_AddNodeToSchema_new(struct DB_TreeNodeParams *TreeParams)
{
    int err;
    vector<string> files;
    readTEMFilesList(TreeParams->pathToLine, files);
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
    char readBuf[len + 71];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    for (long i = 0; i < len / 71; i++) //寻找是否有相同的变量名或者编码
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(TreeParams->valueName, existValueName) == 0)
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
            if (TreeParams->pathCode[j] != existPathCode[j])
                break;
        }
        if (j == 10)
        {
            cout << "存在相同的编码" << endl;
            return StatusCode::PATHCODE_EXIST;
        }
        readbuf_pos += 10;
    }

    if (TreeParams->valueType < 1 || TreeParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    if (TreeParams->isArrary != 0 && TreeParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    if (TreeParams->hasTime != 0 && TreeParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    char valueNmae[30], hasTime[1], pathCode[10], valueType[30]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(pathCode, 0, sizeof(pathCode));

    if (TreeParams->isArrary == 1) //是数组类型,拼接字符串
    {
        if (TreeParams->arrayLen < 1)
        {
            cout << "数组长度不能小于１" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
        strcpy(valueType, "ARRAY [0..");
        char s[10];
        sprintf(s, "%d", TreeParams->arrayLen);
        strcat(valueType, s);
        strcat(valueType, "] OF ");
        string valueTypeStr = DataType::JudgeValueTypeByNum(TreeParams->valueType);
        // memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
        strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
    }
    else //不是数组类型，直接赋值字符串
    {
        string valueTypeStr = DataType::JudgeValueTypeByNum(TreeParams->valueType);
        memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
    }

    strcpy(valueNmae, TreeParams->valueName);
    hasTime[0] = (char)TreeParams->hasTime;
    memcpy(pathCode, TreeParams->pathCode, 10);

    char writeBuf[71]; //将所有参数传入writeBuf中，已追加写的方式写入已有模板文件中
    memcpy(writeBuf, valueNmae, 30);
    memcpy(writeBuf + 30, valueType, 30);
    memcpy(writeBuf + 60, hasTime, 1);
    memcpy(writeBuf + 61, pathCode, 10);
    memcpy(readBuf + len, writeBuf, 71);

    //创建一个新的模板文件,根据当前已存在模板数量进行编号
    long fp;
    vector<string> temFiles;
    readTEMFilesList(TreeParams->pathToLine, temFiles);
    int temNum = temFiles.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", temNum);
    temPath = TreeParams->pathToLine;
    temPath.append("/").append(TreeParams->pathToLine).append(appendNum).append(".tem");

    err = DB_Open(const_cast<char *>(temPath.c_str()), "wb", &fp);
    if (err == 0)
    {
        err = DB_Write(fp, readBuf, len + 71);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    return err;
}

//会重新创建一个新的文件夹存放添加新节点后的模板
int DB_AddNodeToSchema_mkdir(struct DB_TreeNodeParams *TreeParams)
{
    int err;
    vector<string> files;
    readTEMFilesList(TreeParams->pathToLine, files);
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
    char readBuf[len + 71];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    for (long i = 0; i < len / 71; i++) //寻找是否有相同的变量名或者编码
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(TreeParams->valueName, existValueName) == 0)
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
            if (TreeParams->pathCode[j] != existPathCode[j])
                break;
        }
        if (j == 10)
        {
            cout << "存在相同的编码" << endl;
            return StatusCode::PATHCODE_EXIST;
        }
        readbuf_pos += 10;
    }

    if (TreeParams->valueType < 1 || TreeParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    if (TreeParams->isArrary != 0 && TreeParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    if (TreeParams->hasTime != 0 && TreeParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    char valueNmae[30], hasTime[1], pathCode[10], valueType[30]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(pathCode, 0, sizeof(pathCode));

    if (TreeParams->isArrary == 1) //是数组类型,拼接字符串
    {
        if (TreeParams->arrayLen < 1)
        {
            cout << "数组长度不能小于１" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
        strcpy(valueType, "ARRAY [0..");
        char s[10];
        sprintf(s, "%d", TreeParams->arrayLen);
        strcat(valueType, s);
        strcat(valueType, "] OF ");
        string valueTypeStr = DataType::JudgeValueTypeByNum(TreeParams->valueType);
        // memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
        strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
    }
    else //不是数组类型，直接赋值字符串
    {
        string valueTypeStr = DataType::JudgeValueTypeByNum(TreeParams->valueType);
        memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
    }

    strcpy(valueNmae, TreeParams->valueName);
    hasTime[0] = (char)TreeParams->hasTime;
    memcpy(pathCode, TreeParams->pathCode, 10);

    char writeBuf[71]; //将所有参数传入writeBuf中，已追加写的方式写入已有模板文件中
    memcpy(writeBuf, valueNmae, 30);
    memcpy(writeBuf + 30, valueType, 30);
    memcpy(writeBuf + 60, hasTime, 1);
    memcpy(writeBuf + 61, pathCode, 10);
    memcpy(readBuf + len, writeBuf, 71);

    //创建一个新的文件夹存放新的模板
    long fp;
    temPath = TreeParams->pathToLine;
    vector<string> dirs;
    readAllDirs(dirs,settings("Filename_Label").c_str());
    int dirNum = dirs.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", dirNum);
    temPath.append(appendNum);

    char finalPath[100];
    strcpy(finalPath, settings("Filename_Label").c_str());
    strcat(finalPath, "/");
    strcat(finalPath, const_cast<char *>(temPath.c_str()));
    mkdir(finalPath,0777);

    temPath.append("/").append(TreeParams->pathToLine).append(appendNum).append(".tem");
    err = DB_Open(const_cast<char *>(temPath.c_str()), "wb", &fp);
    if (err == 0)
    {
        err = DB_Write(fp, readBuf, len + 71);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    return err;
}

/**
 * @brief 修改标准模板里的树节点,根据编码进行定位和修改
 *
 * @param TreeParams 需要修改的节点
 * @param newTreeParams 需要修改的信息
 * @return　0:success,
 *         others: StatusCode
 * @note   更新节点参数必须齐全，pathToLine pathcode valueNmae hasTime valueType isArray arrayLen
 */
int DB_UpdateNodeToSchema(struct DB_TreeNodeParams *TreeParams, struct DB_TreeNodeParams *newTreeParams)
{
    int err;
    vector<string> files;
    readFileList(TreeParams->pathToLine, files);
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
            if (TreeParams->pathCode[j] != existPathCode[j])
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

    if (newTreeParams->valueType < 1 || newTreeParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    if (newTreeParams->isArrary != 0 && newTreeParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    if (newTreeParams->hasTime != 0 && newTreeParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
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
        if (newTreeParams->arrayLen < 1)
        {
            cout << "数组长度不能小于１" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
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
    if (newTreeParams->hasTime == -1)
        hasTime[0] = readBuf[71 * pos + 60];
    else
        hasTime[0] = (char)newTreeParams->hasTime;
    memcpy(pathCode, newTreeParams->pathCode, 10);

    //将所有参数传入readBuf中，以覆盖写的方式写入已有模板文件中
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

//创建新的.tem文件，根据目前已存在的模板数量进行编号
int DB_UpdateNodeToSchema_new(struct DB_TreeNodeParams *TreeParams, struct DB_TreeNodeParams *newTreeParams)
{
    int err;
    vector<string> files;
    readFileList(TreeParams->pathToLine, files);
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
            if (TreeParams->pathCode[j] != existPathCode[j])
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

    if (newTreeParams->valueType < 1 || newTreeParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    if (newTreeParams->isArrary != 0 && newTreeParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    if (newTreeParams->hasTime != 0 && newTreeParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
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
        if (newTreeParams->arrayLen < 1)
        {
            cout << "数组长度不能小于１" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
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
    if (newTreeParams->hasTime == -1)
        hasTime[0] = readBuf[71 * pos + 60];
    else
        hasTime[0] = (char)newTreeParams->hasTime;
    memcpy(pathCode, newTreeParams->pathCode, 10);

    //将所有参数传入readBuf中，以覆盖写的方式写入已有模板文件中
    memcpy(readBuf + 71 * pos, valueNmae, 30);
    memcpy(readBuf + 71 * pos + 30, valueType, 30);
    memcpy(readBuf + 71 * pos + 60, hasTime, 1);
    memcpy(readBuf + 71 * pos + 61, pathCode, 10);

    //创建一个新的.tem文件，根据当前已存在的模板数量进行编号
    long fp;
    vector<string> temFiles;
    readTEMFilesList(TreeParams->pathToLine, temFiles);
    int temNum = temFiles.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", temNum);
    temPath = TreeParams->pathToLine;
    temPath.append("/").append(TreeParams->pathToLine).append(appendNum).append(".tem");
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

//创建新文件夹存放修改后的.tem文件
int DB_UpdateNodeToSchema_mkdir(struct DB_TreeNodeParams *TreeParams, struct DB_TreeNodeParams *newTreeParams)
{
    int err;
    vector<string> files;
    readFileList(TreeParams->pathToLine, files);
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
            if (TreeParams->pathCode[j] != existPathCode[j])
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

    if (newTreeParams->valueType < 1 || newTreeParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    if (newTreeParams->isArrary != 0 && newTreeParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    if (newTreeParams->hasTime != 0 && newTreeParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
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
        if (newTreeParams->arrayLen < 1)
        {
            cout << "数组长度不能小于１" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
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
    if (newTreeParams->hasTime == -1)
        hasTime[0] = readBuf[71 * pos + 60];
    else
        hasTime[0] = (char)newTreeParams->hasTime;
    memcpy(pathCode, newTreeParams->pathCode, 10);

    //将所有参数传入readBuf中，以覆盖写的方式写入已有模板文件中
    memcpy(readBuf + 71 * pos, valueNmae, 30);
    memcpy(readBuf + 71 * pos + 30, valueType, 30);
    memcpy(readBuf + 71 * pos + 60, hasTime, 1);
    memcpy(readBuf + 71 * pos + 61, pathCode, 10);

    //创建一个新的文件夹来存放修改后的模板
    long fp;
    temPath = newTreeParams->pathToLine;

    char finalPath[100];
    strcpy(finalPath, settings("Filename_Label").c_str());
    strcat(finalPath, "/");
    strcat(finalPath, const_cast<char *>(temPath.c_str()));
    mkdir(finalPath,0777);

    temPath.append("/").append(newTreeParams->pathToLine).append(".tem");
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
 * @param TreeParams 标准模板参数
 * @return　0:success,
 *         others: StatusCode
 */
int DB_DeleteNodeToSchema(struct DB_TreeNodeParams *TreeParams)
{
    int err;
    vector<string> files;
    readFileList(TreeParams->pathToLine, files);
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
            if (TreeParams->pathCode[j] != existPathCode[j])
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

    //打开文件并覆盖写入删除节点后的信息
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

//会创建一个新的.tem文件，根据已存在的模板数量进行编号
int DB_DeleteNodeToSchema_new(struct DB_TreeNodeParams *TreeParams)
{
    int err;
    vector<string> files;
    readFileList(TreeParams->pathToLine, files);
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
            if (TreeParams->pathCode[j] != existPathCode[j])
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

    //创建新的.tem文件，根据当前已存在的模板文件进行编号
    long fp;
    vector<string> temFiles;
    readTEMFilesList(TreeParams->pathToLine, temFiles);
    int temNum = temFiles.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", temNum);
    temPath = TreeParams->pathToLine;
    temPath.append("/").append(TreeParams->pathToLine).append(appendNum).append(".tem");
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

//会创建一个新的文件夹存放删除模板节点后的模板
int DB_DeleteNodeToSchema_mkdir(struct DB_TreeNodeParams *TreeParams)
{
    int err;
    vector<string> files;
    readFileList(TreeParams->pathToLine, files);
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
            if (TreeParams->pathCode[j] != existPathCode[j])
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

    //创建新的.tem文件，根据当前已存在的模板文件进行编号
    long fp;
    temPath = TreeParams->pathToLine;
    vector<string> dirs;
    readAllDirs(dirs,settings("Filename_Label").c_str());
    int dirNum = dirs.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", dirNum);
    temPath.append(appendNum);

    char finalPath[100];
    strcpy(finalPath, settings("Filename_Label").c_str());
    strcat(finalPath, "/");
    strcat(finalPath, const_cast<char *>(temPath.c_str()));
    mkdir(finalPath,0777);

    temPath.append("/").append(TreeParams->pathToLine).append(appendNum).append(".tem");
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
 * @brief 向压缩模板添加新节点，暂定直接在尾部追加
 *
 * @param ZipParams 压缩模板参数
 * @return　0:success,
 *         others: StatusCode
 * @note   新节点参数必须齐全，pathToLine valueNmae hasTime valueType isArray arrayLen standardValue maxValue minValue
 */
int DB_AddNodeToZipSchema(struct DB_ZipNodeParams *ZipParams)
{
    int err;
    vector<string> files;
    readFileList(ZipParams->pathToLine, files);
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

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    DataTypeConverter converter;

    for (long i = 0; i < len / 91; i++) //寻找是否有相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            cout << "存在相同的变量名" << endl;
            return StatusCode::VARIABLE_NAME_EXIST;
        }
        readbuf_pos += 91;
    }

    if (ZipParams->valueType < 1 || ZipParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    if (ZipParams->isArrary != 0 && ZipParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    if (ZipParams->hasTime != 0 && ZipParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    char valueNmae[30], valueType[30], standardValue[10], maxValue[10], minValue[10], hasTime[1]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(standardValue, 0, sizeof(standardValue));
    memset(maxValue, 0, sizeof(maxValue));
    memset(minValue, 0, sizeof(minValue));

    if (ZipParams->isArrary == 1) //是数组类型,拼接字符串
    {
        if (ZipParams->arrayLen < 1)
        {
            cout << "数组长度不能小于１" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
        strcpy(valueType, "ARRAY [0..");
        char s[10];
        sprintf(s, "%d", ZipParams->arrayLen);
        strcat(valueType, s);
        strcat(valueType, "] OF ");
        string valueTypeStr = DataType::JudgeValueTypeByNum(ZipParams->valueType);
        // memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
        strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
    }
    else //不是数组类型，直接赋值字符串
    {
        string valueTypeStr = DataType::JudgeValueTypeByNum(ZipParams->valueType);
        memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
    }

    strcpy(valueNmae, ZipParams->valueName);
    hasTime[0] = (char)ZipParams->hasTime;

    if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "BOOL")
    {
        memcpy(standardValue, ZipParams->standardValue, 1);
        memcpy(maxValue, ZipParams->maxValue, 1);
        memcpy(minValue, ZipParams->minValue, 1);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "USINT")
    {
        //标准值
        char s[1];
        uint8_t sValue = (uint8_t)atoi(ZipParams->standardValue);
        s[0] = sValue;
        memcpy(standardValue, s, 1);

        //最大值
        char ma[1];
        uint8_t maValue = (uint8_t)atoi(ZipParams->maxValue);
        ma[0] = maValue;
        memcpy(maxValue, ma, 1);

        //最小值
        char mi[1];
        uint8_t miValue = (uint8_t)atoi(ZipParams->minValue);
        mi[0] = miValue;
        memcpy(minValue, mi, 1);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "UINT")
    {
        //标准值
        char s[2];
        ushort sValue = (ushort)atoi(ZipParams->standardValue);
        converter.ToUInt16Buff_m(sValue, s);
        memcpy(standardValue, s, 2);

        //最大值
        char ma[2];
        ushort maValue = (ushort)atoi(ZipParams->maxValue);
        converter.ToUInt16Buff_m(maValue, ma);
        memcpy(maxValue, ma, 2);

        //最小值
        char mi[2];
        ushort miValue = (ushort)atoi(ZipParams->minValue);
        converter.ToUInt16Buff_m(miValue, mi);
        memcpy(minValue, mi, 2);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "UDINT")
    {
        //标准值
        char s[4];
        uint32_t sValue = (uint32_t)atoi(ZipParams->standardValue);
        converter.ToUInt32Buff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        ushort maValue = (ushort)atoi(ZipParams->maxValue);
        converter.ToUInt32Buff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        ushort miValue = (ushort)atoi(ZipParams->minValue);
        converter.ToUInt32Buff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "SINT")
    {
        //标准值
        char s[1];
        int8_t sValue = (int8_t)atoi(ZipParams->standardValue);
        s[0] = sValue;
        memcpy(standardValue, s, 1);

        //最大值
        char ma[1];
        int8_t maValue = (int8_t)atoi(ZipParams->maxValue);
        ma[0] = maValue;
        memcpy(maxValue, ma, 1);

        //最小值
        char mi[1];
        int8_t miValue = (int8_t)atoi(ZipParams->minValue);
        mi[0] = miValue;
        memcpy(minValue, mi, 1);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "INT")
    {
        //标准值
        char s[2];
        short sValue = (short)atoi(ZipParams->standardValue);
        converter.ToInt16Buff_m(sValue, s);
        memcpy(standardValue, s, 2);

        //最大值
        char ma[2];
        short maValue = (short)atoi(ZipParams->maxValue);
        converter.ToInt16Buff_m(maValue, ma);
        memcpy(maxValue, ma, 2);

        //最小值
        char mi[2];
        short miValue = (short)atoi(ZipParams->minValue);
        converter.ToInt16Buff_m(miValue, mi);
        memcpy(minValue, mi, 2);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "DINT")
    {
        //标准值
        char s[4];
        int sValue = (int)atoi(ZipParams->standardValue);
        converter.ToInt32Buff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        int maValue = (int)atoi(ZipParams->maxValue);
        converter.ToInt32Buff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        int miValue = (int)atoi(ZipParams->minValue);
        converter.ToInt32Buff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "REAL")
    {
        //标准值
        char s[4];
        float sValue = (float)atof(ZipParams->standardValue);
        converter.ToFloatBuff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        float maValue = (float)atof(ZipParams->maxValue);
        converter.ToFloatBuff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        float miValue = (float)atof(ZipParams->minValue);
        converter.ToFloatBuff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "IMAGE")
    {
        //标准值
        char s[4];
        float sValue = (float)atof(ZipParams->standardValue);
        converter.ToFloatBuff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        float maValue = (float)atof(ZipParams->maxValue);
        converter.ToFloatBuff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        float miValue = (float)atof(ZipParams->minValue);
        converter.ToFloatBuff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else
        return StatusCode::UNKNOWN_TYPE;

    char writeBuf[91]; //将所有参数传入writeBuf中，已追加写的方式写入已有模板文件中
    memcpy(writeBuf, valueNmae, 30);
    memcpy(writeBuf + 30, valueType, 30);
    memcpy(writeBuf + 60, standardValue, 10);
    memcpy(writeBuf + 70, maxValue, 10);
    memcpy(writeBuf + 80, minValue, 10);
    memcpy(writeBuf + 90, hasTime, 1);

    //打开文件并追加写入
    long fp;
    err = DB_Open(const_cast<char *>(temPath.c_str()), "ab+", &fp);
    if (err == 0)
    {
        err = DB_Write(fp, writeBuf, 91);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    return err;
}

//会创建一个新的.ziptem文件，根据已存在的压缩模板文件数量进行编号
int DB_AddNodeToZipSchema_new(struct DB_ZipNodeParams *ZipParams)
{
    int err;
    vector<string> files;
    readFileList(ZipParams->pathToLine, files);
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

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len + 91];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    DataTypeConverter converter;

    for (long i = 0; i < len / 91; i++) //寻找是否有相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            cout << "存在相同的变量名" << endl;
            return StatusCode::VARIABLE_NAME_EXIST;
        }
        readbuf_pos += 91;
    }

    if (ZipParams->valueType < 1 || ZipParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    if (ZipParams->isArrary != 0 && ZipParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    if (ZipParams->hasTime != 0 && ZipParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    char valueNmae[30], valueType[30], standardValue[10], maxValue[10], minValue[10], hasTime[1]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(standardValue, 0, sizeof(standardValue));
    memset(maxValue, 0, sizeof(maxValue));
    memset(minValue, 0, sizeof(minValue));

    if (ZipParams->isArrary == 1) //是数组类型,拼接字符串
    {
        if (ZipParams->arrayLen < 1)
        {
            cout << "数组长度不能小于１" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
        strcpy(valueType, "ARRAY [0..");
        char s[10];
        sprintf(s, "%d", ZipParams->arrayLen);
        strcat(valueType, s);
        strcat(valueType, "] OF ");
        string valueTypeStr = DataType::JudgeValueTypeByNum(ZipParams->valueType);
        // memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
        strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
    }
    else //不是数组类型，直接赋值字符串
    {
        string valueTypeStr = DataType::JudgeValueTypeByNum(ZipParams->valueType);
        memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
    }

    strcpy(valueNmae, ZipParams->valueName);
    hasTime[0] = (char)ZipParams->hasTime;

    if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "BOOL")
    {
        memcpy(standardValue, ZipParams->standardValue, 1);
        memcpy(maxValue, ZipParams->maxValue, 1);
        memcpy(minValue, ZipParams->minValue, 1);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "USINT")
    {
        //标准值
        char s[1];
        uint8_t sValue = (uint8_t)atoi(ZipParams->standardValue);
        s[0] = sValue;
        memcpy(standardValue, s, 1);

        //最大值
        char ma[1];
        uint8_t maValue = (uint8_t)atoi(ZipParams->maxValue);
        ma[0] = maValue;
        memcpy(maxValue, ma, 1);

        //最小值
        char mi[1];
        uint8_t miValue = (uint8_t)atoi(ZipParams->minValue);
        mi[0] = miValue;
        memcpy(minValue, mi, 1);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "UINT")
    {
        //标准值
        char s[2];
        ushort sValue = (ushort)atoi(ZipParams->standardValue);
        converter.ToUInt16Buff_m(sValue, s);
        memcpy(standardValue, s, 2);

        //最大值
        char ma[2];
        ushort maValue = (ushort)atoi(ZipParams->maxValue);
        converter.ToUInt16Buff_m(maValue, ma);
        memcpy(maxValue, ma, 2);

        //最小值
        char mi[2];
        ushort miValue = (ushort)atoi(ZipParams->minValue);
        converter.ToUInt16Buff_m(miValue, mi);
        memcpy(minValue, mi, 2);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "UDINT")
    {
        //标准值
        char s[4];
        uint32_t sValue = (uint32_t)atoi(ZipParams->standardValue);
        converter.ToUInt32Buff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        ushort maValue = (ushort)atoi(ZipParams->maxValue);
        converter.ToUInt32Buff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        ushort miValue = (ushort)atoi(ZipParams->minValue);
        converter.ToUInt32Buff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "SINT")
    {
        //标准值
        char s[1];
        int8_t sValue = (int8_t)atoi(ZipParams->standardValue);
        s[0] = sValue;
        memcpy(standardValue, s, 1);

        //最大值
        char ma[1];
        int8_t maValue = (int8_t)atoi(ZipParams->maxValue);
        ma[0] = maValue;
        memcpy(maxValue, ma, 1);

        //最小值
        char mi[1];
        int8_t miValue = (int8_t)atoi(ZipParams->minValue);
        mi[0] = miValue;
        memcpy(minValue, mi, 1);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "INT")
    {
        //标准值
        char s[2];
        short sValue = (short)atoi(ZipParams->standardValue);
        converter.ToInt16Buff_m(sValue, s);
        memcpy(standardValue, s, 2);

        //最大值
        char ma[2];
        short maValue = (short)atoi(ZipParams->maxValue);
        converter.ToInt16Buff_m(maValue, ma);
        memcpy(maxValue, ma, 2);

        //最小值
        char mi[2];
        short miValue = (short)atoi(ZipParams->minValue);
        converter.ToInt16Buff_m(miValue, mi);
        memcpy(minValue, mi, 2);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "DINT")
    {
        //标准值
        char s[4];
        int sValue = (int)atoi(ZipParams->standardValue);
        converter.ToInt32Buff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        int maValue = (int)atoi(ZipParams->maxValue);
        converter.ToInt32Buff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        int miValue = (int)atoi(ZipParams->minValue);
        converter.ToInt32Buff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "REAL")
    {
        //标准值
        char s[4];
        float sValue = (float)atof(ZipParams->standardValue);
        converter.ToFloatBuff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        float maValue = (float)atof(ZipParams->maxValue);
        converter.ToFloatBuff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        float miValue = (float)atof(ZipParams->minValue);
        converter.ToFloatBuff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "IMAGE")
    {
        //标准值
        char s[4];
        float sValue = (float)atof(ZipParams->standardValue);
        converter.ToFloatBuff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        float maValue = (float)atof(ZipParams->maxValue);
        converter.ToFloatBuff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        float miValue = (float)atof(ZipParams->minValue);
        converter.ToFloatBuff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else
        return StatusCode::UNKNOWN_TYPE;

    char writeBuf[91]; //将所有参数传入writeBuf中，已追加写的方式写入已有模板文件中
    memcpy(writeBuf, valueNmae, 30);
    memcpy(writeBuf + 30, valueType, 30);
    memcpy(writeBuf + 60, standardValue, 10);
    memcpy(writeBuf + 70, maxValue, 10);
    memcpy(writeBuf + 80, minValue, 10);
    memcpy(writeBuf + 90, hasTime, 1);
    memcpy(readBuf + len, writeBuf, 91);

    //创建一个新的.ziptem文件，根据当前已存在的压缩模板数量进行编号
    long fp;
    vector<string> ziptemFiles;
    readZIPTEMFilesList(ZipParams->pathToLine, ziptemFiles);
    int ziptemNum = ziptemFiles.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", ziptemNum);
    temPath = ZipParams->pathToLine;
    temPath.append("/").append(ZipParams->pathToLine).append(appendNum).append(".ziptem");
    err = DB_Open(const_cast<char *>(temPath.c_str()), "wb", &fp);
    if (err == 0)
    {
        err = DB_Write(fp, writeBuf, 91);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    return err;
}

//会创建一个新的文件夹存放新增节点后的压缩模板
int DB_AddNodeToZipSchema_mkdir(struct DB_ZipNodeParams *ZipParams)
{
    int err;
    vector<string> files;
    readFileList(ZipParams->pathToLine, files);
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

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len + 91];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    DataTypeConverter converter;

    for (long i = 0; i < len / 91; i++) //寻找是否有相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            cout << "存在相同的变量名" << endl;
            return StatusCode::VARIABLE_NAME_EXIST;
        }
        readbuf_pos += 91;
    }

    if (ZipParams->valueType < 1 || ZipParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    if (ZipParams->isArrary != 0 && ZipParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    if (ZipParams->hasTime != 0 && ZipParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    char valueNmae[30], valueType[30], standardValue[10], maxValue[10], minValue[10], hasTime[1]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(standardValue, 0, sizeof(standardValue));
    memset(maxValue, 0, sizeof(maxValue));
    memset(minValue, 0, sizeof(minValue));

    if (ZipParams->isArrary == 1) //是数组类型,拼接字符串
    {
        if (ZipParams->arrayLen < 1)
        {
            cout << "数组长度不能小于１" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
        strcpy(valueType, "ARRAY [0..");
        char s[10];
        sprintf(s, "%d", ZipParams->arrayLen);
        strcat(valueType, s);
        strcat(valueType, "] OF ");
        string valueTypeStr = DataType::JudgeValueTypeByNum(ZipParams->valueType);
        // memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
        strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
    }
    else //不是数组类型，直接赋值字符串
    {
        string valueTypeStr = DataType::JudgeValueTypeByNum(ZipParams->valueType);
        memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
    }

    strcpy(valueNmae, ZipParams->valueName);
    hasTime[0] = (char)ZipParams->hasTime;

    if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "BOOL")
    {
        memcpy(standardValue, ZipParams->standardValue, 1);
        memcpy(maxValue, ZipParams->maxValue, 1);
        memcpy(minValue, ZipParams->minValue, 1);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "USINT")
    {
        //标准值
        char s[1];
        uint8_t sValue = (uint8_t)atoi(ZipParams->standardValue);
        s[0] = sValue;
        memcpy(standardValue, s, 1);

        //最大值
        char ma[1];
        uint8_t maValue = (uint8_t)atoi(ZipParams->maxValue);
        ma[0] = maValue;
        memcpy(maxValue, ma, 1);

        //最小值
        char mi[1];
        uint8_t miValue = (uint8_t)atoi(ZipParams->minValue);
        mi[0] = miValue;
        memcpy(minValue, mi, 1);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "UINT")
    {
        //标准值
        char s[2];
        ushort sValue = (ushort)atoi(ZipParams->standardValue);
        converter.ToUInt16Buff_m(sValue, s);
        memcpy(standardValue, s, 2);

        //最大值
        char ma[2];
        ushort maValue = (ushort)atoi(ZipParams->maxValue);
        converter.ToUInt16Buff_m(maValue, ma);
        memcpy(maxValue, ma, 2);

        //最小值
        char mi[2];
        ushort miValue = (ushort)atoi(ZipParams->minValue);
        converter.ToUInt16Buff_m(miValue, mi);
        memcpy(minValue, mi, 2);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "UDINT")
    {
        //标准值
        char s[4];
        uint32_t sValue = (uint32_t)atoi(ZipParams->standardValue);
        converter.ToUInt32Buff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        ushort maValue = (ushort)atoi(ZipParams->maxValue);
        converter.ToUInt32Buff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        ushort miValue = (ushort)atoi(ZipParams->minValue);
        converter.ToUInt32Buff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "SINT")
    {
        //标准值
        char s[1];
        int8_t sValue = (int8_t)atoi(ZipParams->standardValue);
        s[0] = sValue;
        memcpy(standardValue, s, 1);

        //最大值
        char ma[1];
        int8_t maValue = (int8_t)atoi(ZipParams->maxValue);
        ma[0] = maValue;
        memcpy(maxValue, ma, 1);

        //最小值
        char mi[1];
        int8_t miValue = (int8_t)atoi(ZipParams->minValue);
        mi[0] = miValue;
        memcpy(minValue, mi, 1);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "INT")
    {
        //标准值
        char s[2];
        short sValue = (short)atoi(ZipParams->standardValue);
        converter.ToInt16Buff_m(sValue, s);
        memcpy(standardValue, s, 2);

        //最大值
        char ma[2];
        short maValue = (short)atoi(ZipParams->maxValue);
        converter.ToInt16Buff_m(maValue, ma);
        memcpy(maxValue, ma, 2);

        //最小值
        char mi[2];
        short miValue = (short)atoi(ZipParams->minValue);
        converter.ToInt16Buff_m(miValue, mi);
        memcpy(minValue, mi, 2);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "DINT")
    {
        //标准值
        char s[4];
        int sValue = (int)atoi(ZipParams->standardValue);
        converter.ToInt32Buff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        int maValue = (int)atoi(ZipParams->maxValue);
        converter.ToInt32Buff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        int miValue = (int)atoi(ZipParams->minValue);
        converter.ToInt32Buff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "REAL")
    {
        //标准值
        char s[4];
        float sValue = (float)atof(ZipParams->standardValue);
        converter.ToFloatBuff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        float maValue = (float)atof(ZipParams->maxValue);
        converter.ToFloatBuff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        float miValue = (float)atof(ZipParams->minValue);
        converter.ToFloatBuff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "IMAGE")
    {
        //标准值
        char s[4];
        float sValue = (float)atof(ZipParams->standardValue);
        converter.ToFloatBuff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        float maValue = (float)atof(ZipParams->maxValue);
        converter.ToFloatBuff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        float miValue = (float)atof(ZipParams->minValue);
        converter.ToFloatBuff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else
        return StatusCode::UNKNOWN_TYPE;

    char writeBuf[91]; //将所有参数传入writeBuf中，已追加写的方式写入已有模板文件中
    memcpy(writeBuf, valueNmae, 30);
    memcpy(writeBuf + 30, valueType, 30);
    memcpy(writeBuf + 60, standardValue, 10);
    memcpy(writeBuf + 70, maxValue, 10);
    memcpy(writeBuf + 80, minValue, 10);
    memcpy(writeBuf + 90, hasTime, 1);
    memcpy(readBuf + len, writeBuf, 91);

    //创建一个新的.ziptem文件，根据当前已存在的压缩模板数量进行编号
    long fp;
    vector<string> dirs;
    readAllDirs(dirs,settings("Filename_Label").c_str());
    int dirNum = dirs.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", dirNum);
    temPath = ZipParams->pathToLine;
    temPath.append(appendNum);

    char finalPath[100];
    strcpy(finalPath, settings("Filename_Label").c_str());
    strcat(finalPath, "/");
    strcat(finalPath, const_cast<char *>(temPath.c_str()));
    mkdir(finalPath,0777);

    temPath.append("/").append(ZipParams->pathToLine).append(appendNum).append(".ziptem");
    err = DB_Open(const_cast<char *>(temPath.c_str()), "wb", &fp);
    if (err == 0)
    {
        err = DB_Write(fp, writeBuf, 91);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    return err;
}

/**
 * @brief 修改压缩模板里的树节点
 *
 * @param Zipparams 需要修改的节点
 * @param newZipParams 需要修改的信息
 * @return　0:success,
 *         others: StatusCode
 * @note   更新节点参数必须齐全，pathToLine valueNmae hasTime valueType isArray arrayLen standardValue maxValue minValue
 */
int DB_UpdateNodeToZipSchema(struct DB_ZipNodeParams *ZipParams, struct DB_ZipNodeParams *newZipParams)
{
    int err;
    vector<string> files;
    readFileList(ZipParams->pathToLine, files);
    string temPath = "";
    for (string file : files) //找到带有后缀tem的文件
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

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    DataTypeConverter converter;

    long pos = -1;                      //用于定位是修改模板的第几条
    for (long i = 0; i < len / 91; i++) //寻找是否有相同变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            pos = i; //记录这条记录的位置
        }
        readbuf_pos += 91;
    }
    if (pos == -1)
    {
        cout << "未找到变量名！" << endl;
        return StatusCode::UNKNOWN_PATHCODE;
    }

    readbuf_pos = 0;
    for (long i = 0; i < len / 71; i++) //寻找模板是否有与更新参数相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(newZipParams->valueName, existValueName) == 0 && i != pos)
        {
            cout << "更新参数存在模板已有的变量名" << endl;
            return StatusCode::VARIABLE_NAME_EXIST;
        }
        readbuf_pos += 91;
    }

    if (newZipParams->valueType < 1 || newZipParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    if (newZipParams->isArrary != 0 && newZipParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    if (newZipParams->hasTime != 0 && newZipParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    char valueNmae[30], valueType[30], standardValue[10], maxValue[10], minValue[10], hasTime[1]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(standardValue, 0, sizeof(standardValue));
    memset(maxValue, 0, sizeof(maxValue));
    memset(minValue, 0, sizeof(minValue));

    if (newZipParams->isArrary == 1) //是数组类型,拼接字符串
    {
        if (newZipParams->arrayLen < 1)
        {
            cout << "数组长度不能小于１" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
        strcpy(valueType, "ARRAY [0..");
        char s[10];
        sprintf(s, "%d", newZipParams->arrayLen);
        strcat(valueType, s);
        strcat(valueType, "] OF ");
        string valueTypeStr = DataType::JudgeValueTypeByNum(newZipParams->valueType);
        // memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
        strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
    }
    else //不是数组类型，直接赋值字符串
    {
        string valueTypeStr = DataType::JudgeValueTypeByNum(newZipParams->valueType);
        memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
    }

    strcpy(valueNmae, newZipParams->valueName);
    hasTime[0] = (char)newZipParams->hasTime;

    if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "BOOL")
    {
        memcpy(standardValue, newZipParams->standardValue, 1);
        memcpy(maxValue, newZipParams->maxValue, 1);
        memcpy(minValue, newZipParams->minValue, 1);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "USINT")
    {
        //标准值
        char s[1];
        uint8_t sValue = (uint8_t)atoi(newZipParams->standardValue);
        s[0] = sValue;
        memcpy(standardValue, s, 1);

        //最大值
        char ma[1];
        uint8_t maValue = (uint8_t)atoi(newZipParams->maxValue);
        ma[0] = maValue;
        memcpy(maxValue, ma, 1);

        //最小值
        char mi[1];
        uint8_t miValue = (uint8_t)atoi(newZipParams->minValue);
        mi[0] = miValue;
        memcpy(minValue, mi, 1);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "UINT")
    {
        //标准值
        char s[2];
        ushort sValue = (ushort)atoi(newZipParams->standardValue);
        converter.ToUInt16Buff_m(sValue, s);
        memcpy(standardValue, s, 2);

        //最大值
        char ma[2];
        ushort maValue = (ushort)atoi(newZipParams->maxValue);
        converter.ToUInt16Buff_m(maValue, ma);
        memcpy(maxValue, ma, 2);

        //最小值
        char mi[2];
        ushort miValue = (ushort)atoi(newZipParams->minValue);
        converter.ToUInt16Buff_m(miValue, mi);
        memcpy(minValue, mi, 2);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "UDINT")
    {
        //标准值
        char s[4];
        uint32_t sValue = (uint32_t)atoi(newZipParams->standardValue);
        converter.ToUInt32Buff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        ushort maValue = (ushort)atoi(newZipParams->maxValue);
        converter.ToUInt32Buff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        ushort miValue = (ushort)atoi(newZipParams->minValue);
        converter.ToUInt32Buff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "SINT")
    {
        //标准值
        char s[1];
        int8_t sValue = (int8_t)atoi(newZipParams->standardValue);
        s[0] = sValue;
        memcpy(standardValue, s, 1);

        //最大值
        char ma[1];
        int8_t maValue = (int8_t)atoi(newZipParams->maxValue);
        ma[0] = maValue;
        memcpy(maxValue, ma, 1);

        //最小值
        char mi[1];
        int8_t miValue = (int8_t)atoi(newZipParams->minValue);
        mi[0] = miValue;
        memcpy(minValue, mi, 1);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "INT")
    {
        //标准值
        char s[2];
        short sValue = (short)atoi(newZipParams->standardValue);
        converter.ToInt16Buff_m(sValue, s);
        memcpy(standardValue, s, 2);

        //最大值
        char ma[2];
        short maValue = (short)atoi(newZipParams->maxValue);
        converter.ToInt16Buff_m(maValue, ma);
        memcpy(maxValue, ma, 2);

        //最小值
        char mi[2];
        short miValue = (short)atoi(newZipParams->minValue);
        converter.ToInt16Buff_m(miValue, mi);
        memcpy(minValue, mi, 2);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "DINT")
    {
        //标准值
        char s[4];
        int sValue = (int)atoi(newZipParams->standardValue);
        converter.ToInt32Buff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        int maValue = (int)atoi(newZipParams->maxValue);
        converter.ToInt32Buff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        int miValue = (int)atoi(newZipParams->minValue);
        converter.ToInt32Buff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "REAL")
    {
        //标准值
        char s[4];
        float sValue = (float)atof(newZipParams->standardValue);
        converter.ToFloatBuff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        float maValue = (float)atof(newZipParams->maxValue);
        converter.ToFloatBuff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        float miValue = (float)atof(newZipParams->minValue);
        converter.ToFloatBuff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "IMAGE")
    {
        //标准值
        char s[4];
        float sValue = (float)atof(newZipParams->standardValue);
        converter.ToFloatBuff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        float maValue = (float)atof(newZipParams->maxValue);
        converter.ToFloatBuff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        float miValue = (float)atof(newZipParams->minValue);
        converter.ToFloatBuff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else
        return StatusCode::UNKNOWN_TYPE;

    //将所有参数传入readBuf中，已覆盖写的方式写入已有模板文件中
    memcpy(readBuf + 91 * pos, valueNmae, 30);
    memcpy(readBuf + 91 * pos + 30, valueType, 30);
    memcpy(readBuf + 91 * pos + 60, standardValue, 10);
    memcpy(readBuf + 91 * pos + 70, maxValue, 10);
    memcpy(readBuf + 91 * pos + 80, minValue, 10);
    memcpy(readBuf + 91 * pos + 90, hasTime, 1);

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

//会创建一个新的.ziptem文件，根据当前已存在的压缩模板数量进行编号
int DB_UpdateNodeToZipSchema_new(struct DB_ZipNodeParams *ZipParams, struct DB_ZipNodeParams *newZipParams)
{
    int err;
    vector<string> files;
    readFileList(ZipParams->pathToLine, files);
    string temPath = "";
    for (string file : files) //找到带有后缀tem的文件
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

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    DataTypeConverter converter;

    long pos = -1;                      //用于定位是修改模板的第几条
    for (long i = 0; i < len / 91; i++) //寻找是否有相同变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            pos = i; //记录这条记录的位置
        }
        readbuf_pos += 91;
    }
    if (pos == -1)
    {
        cout << "未找到变量名！" << endl;
        return StatusCode::UNKNOWN_PATHCODE;
    }

    readbuf_pos = 0;
    for (long i = 0; i < len / 71; i++) //寻找模板是否有与更新参数相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(newZipParams->valueName, existValueName) == 0 && i != pos)
        {
            cout << "更新参数存在模板已有的变量名" << endl;
            return StatusCode::VARIABLE_NAME_EXIST;
        }
        readbuf_pos += 91;
    }

    if (newZipParams->valueType < 1 || newZipParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    if (newZipParams->isArrary != 0 && newZipParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    if (newZipParams->hasTime != 0 && newZipParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    char valueNmae[30], valueType[30], standardValue[10], maxValue[10], minValue[10], hasTime[1]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(standardValue, 0, sizeof(standardValue));
    memset(maxValue, 0, sizeof(maxValue));
    memset(minValue, 0, sizeof(minValue));

    if (newZipParams->isArrary == 1) //是数组类型,拼接字符串
    {
        if (newZipParams->arrayLen < 1)
        {
            cout << "数组长度不能小于１" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
        strcpy(valueType, "ARRAY [0..");
        char s[10];
        sprintf(s, "%d", newZipParams->arrayLen);
        strcat(valueType, s);
        strcat(valueType, "] OF ");
        string valueTypeStr = DataType::JudgeValueTypeByNum(newZipParams->valueType);
        // memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
        strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
    }
    else //不是数组类型，直接赋值字符串
    {
        string valueTypeStr = DataType::JudgeValueTypeByNum(newZipParams->valueType);
        memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
    }

    strcpy(valueNmae, newZipParams->valueName);
    hasTime[0] = (char)newZipParams->hasTime;

    if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "BOOL")
    {
        memcpy(standardValue, newZipParams->standardValue, 1);
        memcpy(maxValue, newZipParams->maxValue, 1);
        memcpy(minValue, newZipParams->minValue, 1);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "USINT")
    {
        //标准值
        char s[1];
        uint8_t sValue = (uint8_t)atoi(newZipParams->standardValue);
        s[0] = sValue;
        memcpy(standardValue, s, 1);

        //最大值
        char ma[1];
        uint8_t maValue = (uint8_t)atoi(newZipParams->maxValue);
        ma[0] = maValue;
        memcpy(maxValue, ma, 1);

        //最小值
        char mi[1];
        uint8_t miValue = (uint8_t)atoi(newZipParams->minValue);
        mi[0] = miValue;
        memcpy(minValue, mi, 1);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "UINT")
    {
        //标准值
        char s[2];
        ushort sValue = (ushort)atoi(newZipParams->standardValue);
        converter.ToUInt16Buff_m(sValue, s);
        memcpy(standardValue, s, 2);

        //最大值
        char ma[2];
        ushort maValue = (ushort)atoi(newZipParams->maxValue);
        converter.ToUInt16Buff_m(maValue, ma);
        memcpy(maxValue, ma, 2);

        //最小值
        char mi[2];
        ushort miValue = (ushort)atoi(newZipParams->minValue);
        converter.ToUInt16Buff_m(miValue, mi);
        memcpy(minValue, mi, 2);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "UDINT")
    {
        //标准值
        char s[4];
        uint32_t sValue = (uint32_t)atoi(newZipParams->standardValue);
        converter.ToUInt32Buff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        ushort maValue = (ushort)atoi(newZipParams->maxValue);
        converter.ToUInt32Buff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        ushort miValue = (ushort)atoi(newZipParams->minValue);
        converter.ToUInt32Buff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "SINT")
    {
        //标准值
        char s[1];
        int8_t sValue = (int8_t)atoi(newZipParams->standardValue);
        s[0] = sValue;
        memcpy(standardValue, s, 1);

        //最大值
        char ma[1];
        int8_t maValue = (int8_t)atoi(newZipParams->maxValue);
        ma[0] = maValue;
        memcpy(maxValue, ma, 1);

        //最小值
        char mi[1];
        int8_t miValue = (int8_t)atoi(newZipParams->minValue);
        mi[0] = miValue;
        memcpy(minValue, mi, 1);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "INT")
    {
        //标准值
        char s[2];
        short sValue = (short)atoi(newZipParams->standardValue);
        converter.ToInt16Buff_m(sValue, s);
        memcpy(standardValue, s, 2);

        //最大值
        char ma[2];
        short maValue = (short)atoi(newZipParams->maxValue);
        converter.ToInt16Buff_m(maValue, ma);
        memcpy(maxValue, ma, 2);

        //最小值
        char mi[2];
        short miValue = (short)atoi(newZipParams->minValue);
        converter.ToInt16Buff_m(miValue, mi);
        memcpy(minValue, mi, 2);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "DINT")
    {
        //标准值
        char s[4];
        int sValue = (int)atoi(newZipParams->standardValue);
        converter.ToInt32Buff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        int maValue = (int)atoi(newZipParams->maxValue);
        converter.ToInt32Buff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        int miValue = (int)atoi(newZipParams->minValue);
        converter.ToInt32Buff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "REAL")
    {
        //标准值
        char s[4];
        float sValue = (float)atof(newZipParams->standardValue);
        converter.ToFloatBuff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        float maValue = (float)atof(newZipParams->maxValue);
        converter.ToFloatBuff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        float miValue = (float)atof(newZipParams->minValue);
        converter.ToFloatBuff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "IMAGE")
    {
        //标准值
        char s[4];
        float sValue = (float)atof(newZipParams->standardValue);
        converter.ToFloatBuff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        float maValue = (float)atof(newZipParams->maxValue);
        converter.ToFloatBuff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        float miValue = (float)atof(newZipParams->minValue);
        converter.ToFloatBuff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else
        return StatusCode::UNKNOWN_TYPE;

    //将所有参数传入readBuf中，已覆盖写的方式写入已有模板文件中
    memcpy(readBuf + 91 * pos, valueNmae, 30);
    memcpy(readBuf + 91 * pos + 30, valueType, 30);
    memcpy(readBuf + 91 * pos + 60, standardValue, 10);
    memcpy(readBuf + 91 * pos + 70, maxValue, 10);
    memcpy(readBuf + 91 * pos + 80, minValue, 10);
    memcpy(readBuf + 91 * pos + 90, hasTime, 1);

    //创建一个新的.ziptem文件，根据当前已存在的压缩模板数量进行编号
    long fp;
    vector<string> ziptemFiles;
    readZIPTEMFilesList(ZipParams->pathToLine, ziptemFiles);
    int ziptemNum = ziptemFiles.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", ziptemNum);
    temPath = ZipParams->pathToLine;
    temPath.append("/").append(ZipParams->pathToLine).append(appendNum).append(".ziptem");
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

//会创建一个新的文件夹来存放修改后的压缩模板
int DB_UpdateNodeToZipSchema_mkdir(struct DB_ZipNodeParams *ZipParams, struct DB_ZipNodeParams *newZipParams)
{
    int err;
    vector<string> files;
    readFileList(ZipParams->pathToLine, files);
    string temPath = "";
    for (string file : files) //找到带有后缀tem的文件
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

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    DataTypeConverter converter;

    long pos = -1;                      //用于定位是修改模板的第几条
    for (long i = 0; i < len / 91; i++) //寻找是否有相同变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            pos = i; //记录这条记录的位置
        }
        readbuf_pos += 91;
    }
    if (pos == -1)
    {
        cout << "未找到变量名！" << endl;
        return StatusCode::UNKNOWN_PATHCODE;
    }

    readbuf_pos = 0;
    for (long i = 0; i < len / 71; i++) //寻找模板是否有与更新参数相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(newZipParams->valueName, existValueName) == 0 && i != pos)
        {
            cout << "更新参数存在模板已有的变量名" << endl;
            return StatusCode::VARIABLE_NAME_EXIST;
        }
        readbuf_pos += 91;
    }

    if (newZipParams->valueType < 1 || newZipParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    if (newZipParams->isArrary != 0 && newZipParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    if (newZipParams->hasTime != 0 && newZipParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    char valueNmae[30], valueType[30], standardValue[10], maxValue[10], minValue[10], hasTime[1]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(standardValue, 0, sizeof(standardValue));
    memset(maxValue, 0, sizeof(maxValue));
    memset(minValue, 0, sizeof(minValue));

    if (newZipParams->isArrary == 1) //是数组类型,拼接字符串
    {
        if (newZipParams->arrayLen < 1)
        {
            cout << "数组长度不能小于１" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
        strcpy(valueType, "ARRAY [0..");
        char s[10];
        sprintf(s, "%d", newZipParams->arrayLen);
        strcat(valueType, s);
        strcat(valueType, "] OF ");
        string valueTypeStr = DataType::JudgeValueTypeByNum(newZipParams->valueType);
        // memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
        strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
    }
    else //不是数组类型，直接赋值字符串
    {
        string valueTypeStr = DataType::JudgeValueTypeByNum(newZipParams->valueType);
        memcpy(valueType, const_cast<char *>(valueTypeStr.c_str()), valueTypeStr.length());
    }

    strcpy(valueNmae, newZipParams->valueName);
    hasTime[0] = (char)newZipParams->hasTime;

    if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "BOOL")
    {
        memcpy(standardValue, newZipParams->standardValue, 1);
        memcpy(maxValue, newZipParams->maxValue, 1);
        memcpy(minValue, newZipParams->minValue, 1);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "USINT")
    {
        //标准值
        char s[1];
        uint8_t sValue = (uint8_t)atoi(newZipParams->standardValue);
        s[0] = sValue;
        memcpy(standardValue, s, 1);

        //最大值
        char ma[1];
        uint8_t maValue = (uint8_t)atoi(newZipParams->maxValue);
        ma[0] = maValue;
        memcpy(maxValue, ma, 1);

        //最小值
        char mi[1];
        uint8_t miValue = (uint8_t)atoi(newZipParams->minValue);
        mi[0] = miValue;
        memcpy(minValue, mi, 1);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "UINT")
    {
        //标准值
        char s[2];
        ushort sValue = (ushort)atoi(newZipParams->standardValue);
        converter.ToUInt16Buff_m(sValue, s);
        memcpy(standardValue, s, 2);

        //最大值
        char ma[2];
        ushort maValue = (ushort)atoi(newZipParams->maxValue);
        converter.ToUInt16Buff_m(maValue, ma);
        memcpy(maxValue, ma, 2);

        //最小值
        char mi[2];
        ushort miValue = (ushort)atoi(newZipParams->minValue);
        converter.ToUInt16Buff_m(miValue, mi);
        memcpy(minValue, mi, 2);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "UDINT")
    {
        //标准值
        char s[4];
        uint32_t sValue = (uint32_t)atoi(newZipParams->standardValue);
        converter.ToUInt32Buff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        ushort maValue = (ushort)atoi(newZipParams->maxValue);
        converter.ToUInt32Buff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        ushort miValue = (ushort)atoi(newZipParams->minValue);
        converter.ToUInt32Buff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "SINT")
    {
        //标准值
        char s[1];
        int8_t sValue = (int8_t)atoi(newZipParams->standardValue);
        s[0] = sValue;
        memcpy(standardValue, s, 1);

        //最大值
        char ma[1];
        int8_t maValue = (int8_t)atoi(newZipParams->maxValue);
        ma[0] = maValue;
        memcpy(maxValue, ma, 1);

        //最小值
        char mi[1];
        int8_t miValue = (int8_t)atoi(newZipParams->minValue);
        mi[0] = miValue;
        memcpy(minValue, mi, 1);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "INT")
    {
        //标准值
        char s[2];
        short sValue = (short)atoi(newZipParams->standardValue);
        converter.ToInt16Buff_m(sValue, s);
        memcpy(standardValue, s, 2);

        //最大值
        char ma[2];
        short maValue = (short)atoi(newZipParams->maxValue);
        converter.ToInt16Buff_m(maValue, ma);
        memcpy(maxValue, ma, 2);

        //最小值
        char mi[2];
        short miValue = (short)atoi(newZipParams->minValue);
        converter.ToInt16Buff_m(miValue, mi);
        memcpy(minValue, mi, 2);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "DINT")
    {
        //标准值
        char s[4];
        int sValue = (int)atoi(newZipParams->standardValue);
        converter.ToInt32Buff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        int maValue = (int)atoi(newZipParams->maxValue);
        converter.ToInt32Buff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        int miValue = (int)atoi(newZipParams->minValue);
        converter.ToInt32Buff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "REAL")
    {
        //标准值
        char s[4];
        float sValue = (float)atof(newZipParams->standardValue);
        converter.ToFloatBuff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        float maValue = (float)atof(newZipParams->maxValue);
        converter.ToFloatBuff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        float miValue = (float)atof(newZipParams->minValue);
        converter.ToFloatBuff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "IMAGE")
    {
        //标准值
        char s[4];
        float sValue = (float)atof(newZipParams->standardValue);
        converter.ToFloatBuff_m(sValue, s);
        memcpy(standardValue, s, 4);

        //最大值
        char ma[4];
        float maValue = (float)atof(newZipParams->maxValue);
        converter.ToFloatBuff_m(maValue, ma);
        memcpy(maxValue, ma, 4);

        //最小值
        char mi[4];
        float miValue = (float)atof(newZipParams->minValue);
        converter.ToFloatBuff_m(miValue, mi);
        memcpy(minValue, mi, 4);
    }
    else
        return StatusCode::UNKNOWN_TYPE;

    //将所有参数传入readBuf中，已覆盖写的方式写入已有模板文件中
    memcpy(readBuf + 91 * pos, valueNmae, 30);
    memcpy(readBuf + 91 * pos + 30, valueType, 30);
    memcpy(readBuf + 91 * pos + 60, standardValue, 10);
    memcpy(readBuf + 91 * pos + 70, maxValue, 10);
    memcpy(readBuf + 91 * pos + 80, minValue, 10);
    memcpy(readBuf + 91 * pos + 90, hasTime, 1);

    //创建一个新的.ziptem文件，根据当前已存在的压缩模板数量进行编号
    long fp;
    temPath = newZipParams->pathToLine;
    char finalPath[100];
    strcpy(finalPath, settings("Filename_Label").c_str());
    strcat(finalPath, "/");
    strcat(finalPath, const_cast<char *>(temPath.c_str()));
    mkdir(finalPath,0777);

    temPath.append("/").append(newZipParams->pathToLine).append(".ziptem");
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
 * @brief 删除压缩模板节点，根据变量名进行定位
 *
 * @param ZipParams 压缩模板参数
 * @return 0:success,　
 *         others: StatusCode
 */
int DB_DeleteNodeToZipSchema(struct DB_ZipNodeParams *ZipParams)
{
    int err;
    vector<string> files;
    readFileList(ZipParams->pathToLine, files);
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

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    long pos = -1;                      //记录删除节点在第几条
    for (long i = 0; i < len / 91; i++) //寻找是否有相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            pos = i; //记录这条记录的位置
        }
        readbuf_pos += 91;
    }
    if (pos == -1)
    {
        cout << "未找到要删除的记录！" << endl;
        return StatusCode::UNKNOWN_PATHCODE;
    }

    char writeBuf[len - 91];
    memcpy(writeBuf, readBuf, pos * 91);                                       //拷贝要被删除的记录之前的记录
    memcpy(writeBuf + pos * 91, readBuf + pos * 91 + 91, len - pos * 91 - 91); //拷贝要被删除的记录之后的记录

    //打开文件并覆盖写入
    long fp;
    err = DB_Open(const_cast<char *>(temPath.c_str()), "wb+", &fp);
    if (err == 0)
    {
        if (len - 91 != 0)
            err = DB_Write(fp, writeBuf, len - 91);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    return err;
}

//新创建一个.ziptem文件，根据已存在压缩模板的数量进行编号
int DB_DeleteNodeToZipSchema_new(struct DB_ZipNodeParams *ZipParams)
{
    int err;
    vector<string> files;
    readFileList(ZipParams->pathToLine, files);
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

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    long pos = -1;                      //记录删除节点在第几条
    for (long i = 0; i < len / 91; i++) //寻找是否有相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            pos = i; //记录这条记录的位置
        }
        readbuf_pos += 91;
    }
    if (pos == -1)
    {
        cout << "未找到要删除的记录！" << endl;
        return StatusCode::UNKNOWN_PATHCODE;
    }

    char writeBuf[len - 91];
    memcpy(writeBuf, readBuf, pos * 91);                                       //拷贝要被删除的记录之前的记录
    memcpy(writeBuf + pos * 91, readBuf + pos * 91 + 91, len - pos * 91 - 91); //拷贝要被删除的记录之后的记录

    //创建新的.ziptem文件，根据当前已存在的压缩模板数量进行编号
    long fp;
    vector<string> ziptemFiles;
    readZIPTEMFilesList(ZipParams->pathToLine, ziptemFiles);
    int ziptemNum = ziptemFiles.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", ziptemNum);
    temPath = ZipParams->pathToLine;
    temPath.append("/").append(ZipParams->pathToLine).append(appendNum).append(".ziptem");
    err = DB_Open(const_cast<char *>(temPath.c_str()), "wb+", &fp);
    if (err == 0)
    {
        if (len - 91 != 0)
            err = DB_Write(fp, writeBuf, len - 91);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    return err;
}

//新创建一个文件夹存放修改后的压缩模板
int DB_DeleteNodeToZipSchema_mkdir(struct DB_ZipNodeParams *ZipParams)
{
    int err;
    vector<string> files;
    readFileList(ZipParams->pathToLine, files);
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

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    long pos = -1;                      //记录删除节点在第几条
    for (long i = 0; i < len / 91; i++) //寻找是否有相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            pos = i; //记录这条记录的位置
        }
        readbuf_pos += 91;
    }
    if (pos == -1)
    {
        cout << "未找到要删除的记录！" << endl;
        return StatusCode::UNKNOWN_PATHCODE;
    }

    char writeBuf[len - 91];
    memcpy(writeBuf, readBuf, pos * 91);                                       //拷贝要被删除的记录之前的记录
    memcpy(writeBuf + pos * 91, readBuf + pos * 91 + 91, len - pos * 91 - 91); //拷贝要被删除的记录之后的记录

    //创建新的文件夹存放修改后的压缩模板
    long fp;
    vector<string> dirs;
    readAllDirs(dirs,settings("Filename_Label").c_str());
    int dirNum = dirs.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", dirNum);
    temPath = ZipParams->pathToLine;
    temPath.append(appendNum);

    char finalPath[100];
    strcpy(finalPath, settings("Filename_Label").c_str());
    strcat(finalPath, "/");
    strcat(finalPath, const_cast<char *>(temPath.c_str()));
    mkdir(finalPath,0777);

    temPath.append("/").append(ZipParams->pathToLine).append(appendNum).append(".ziptem");
    err = DB_Open(const_cast<char *>(temPath.c_str()), "wb+", &fp);
    if (err == 0)
    {
        if (len - 91 != 0)
            err = DB_Write(fp, writeBuf, len - 91);

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
    return ZipTemplateManager::SetZipTemplate(path);
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

// int main()
// {
//     // DB_LoadZipSchema("jinfei/");

//     DB_TreeNodeParams params;
//     params.pathToLine = "jinfei";
//     char code[10];
//     code[0] = (char)0;
//     code[1] = (char)1;
//     code[2] = (char)0;
//     code[3] = (char)4;
//     code[4] = 'R';
//     code[5] = (char)1;
//     code[6] = 0;
//     code[7] = (char)0;
//     code[8] = (char)0;
//     code[9] = (char)0;
//     params.pathCode = code;
//     params.valueType = 3;
//     params.hasTime = 0;
//     params.isArrary = 0;
//     params.arrayLen = 100;
//     params.valueName = "S4ON";

//     // DB_TreeNodeParams newTreeParams;
//     // newTreeParams.pathToLine = "jinfeiTwo";
//     // char newcode[10];
//     // newcode[0] = (char)0;
//     // newcode[1] = (char)1;
//     // newcode[2] = (char)0;
//     // newcode[3] = (char)4;
//     // newcode[4] = 'R';
//     // newcode[5] = (char)1;
//     // newcode[6] = 0;
//     // newcode[7] = (char)0;
//     // newcode[8] = (char)0;
//     // newcode[9] = (char)0;
//     // newTreeParams.pathCode = newcode;
//     // newTreeParams.valueType = 3;
//     // newTreeParams.hasTime = 1;
//     // newTreeParams.isArrary = 1;
//     // newTreeParams.arrayLen = 100;
//     // newTreeParams.valueName = "S4ON";
//     //DB_UpdateNodeToSchema(&params,&newTreeParams);
//     //DB_UpdateNodeToSchema_new(&params,&newTreeParams);
//     //DB_UpdateNodeToSchema_mkdir(&params,&newTreeParams);
//     //DB_AddNodeToSchema(&params);
//     //DB_AddNodeToSchema_new(&params);
//     DB_AddNodeToSchema_mkdir(&params);
//     //DB_DeleteNodeToSchema(&params);

//     // DB_ZipNodeParams params;
//     // params.pathToLine = "/jinfei";
//     // params.valueType = 3;
//     // params.hasTime = 1;
//     // params.isArrary = 1;
//     // params.arrayLen = 100;
//     // params.valueName = "S4ON";
//     // params.standardValue = "210";
//     // params.maxValue = "230";
//     // params.minValue = "190";
//     // DB_AddNodeToZipSchema(&params);
//     // DB_DeleteNodeToZipSchema(&params);
//     DB_ZipNodeParams newparams;
//     newparams.pathToLine = "/jinfeiTwo";
//     newparams.valueType = 3;
//     newparams.hasTime = 0;
//     newparams.isArrary = 0;
//     newparams.arrayLen = 100;
//     newparams.valueName = "S4ON";
//     newparams.standardValue = "12000";
//     newparams.maxValue = "13000";
//     newparams.minValue = "11000";
//     //DB_UpdateNodeToZipSchema(&params,&newparams);
//     //DB_UpdateNodeToZipSchema_mkdir(&params,&newparams);
//     return 0;
// }
