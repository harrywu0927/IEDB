#include "../include/utils.hpp"
using namespace std;

vector<ZipTemplate> ZipTemplates;
ZipTemplate CurrentZipTemplate;

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

//会重新创建一个.tem文件,根据已存在的tem文件数量追加命名
int DB_AddNodeToSchema_MultiTem(struct DB_TreeNodeParams *TreeParams)
{
    int err;
    if (TreeParams->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;
    //检查变量名输入是否合法
    string variableName = TreeParams->valueName;
    err = checkInputVaribaleName(variableName);
    if (err != 0)
        return StatusCode::VARIABLE_NAME_CHECK_ERROR;

    //检查路径编码输入是否合法
    char inputPathCode[10] = {0};
    memcpy(inputPathCode, TreeParams->pathCode, 10);
    err = checkInputPathcode(inputPathCode);
    if (err != 0)
        return StatusCode::PATHCODE_CHECK_ERROR;

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

    //检查数据类型是否合法
    if (TreeParams->valueType < 1 || TreeParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    //检查是否为数组是否合法
    if (TreeParams->isArrary != 0 && TreeParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    //检查是否为时间序列是否合法
    if (TreeParams->isTS != 0 && TreeParams->isTS != 1)
    {
        cout << "isTs只能为0或1" << endl;
        return StatusCode::ISTS_ERROR;
    }

    //检查是否带时间戳是否合法
    if (TreeParams->hasTime != 0 && TreeParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    char valueNmae[30], hasTime[1], pathCode[10], valueType[30]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(pathCode, 0, sizeof(pathCode));

    if (TreeParams->isTS == 1) //是Ts类型,拼接字符串
    {
        if (TreeParams->tsLen < 1)
        {
            cout << "Ts长度不能小于1" << endl;
            return StatusCode::TSLEN_ERROR;
        }
        if (TreeParams->isArrary)
        {
            if (TreeParams->arrayLen < 1)
            {
                cout << "数组长度不能小于１" << endl;
                return StatusCode::ARRAYLEN_ERROR;
            }
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", TreeParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "][0..");
            sprintf(s, "%d", TreeParams->arrayLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(TreeParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
        else
        {
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", TreeParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(TreeParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
    }
    else if (TreeParams->isArrary == 1) //是数组类型,拼接字符串
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
    char mode[2] = {'w', 'b'};
    err = DB_Open(const_cast<char *>(temPath.c_str()), mode, &fp);
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
 * @brief 向标准模板添加新节点,可以指定新文件夹存储修改后的模板
 *
 * @param TreeParams 标准模板参数
 * @return　0:success,
 *         others: StatusCode
 * @note   新节点参数必须齐全，pathToLine pathcode valueNmae hasTime valueType isArray arrayLen newPath
 *         新文件名称里不能包含数字
 */
int DB_AddNodeToSchema(struct DB_TreeNodeParams *TreeParams)
{
    int err;

    if (TreeParams->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;

    //检查变量名输入是否合法
    string variableName = TreeParams->valueName;
    err = checkInputVaribaleName(variableName);
    if (err != 0)
        return StatusCode::VARIABLE_NAME_CHECK_ERROR;

    //检查路径编码输入是否合法
    char inputPathCode[10] = {0};
    memcpy(inputPathCode, TreeParams->pathCode, 10);
    err = checkInputPathcode(inputPathCode);
    if (err != 0)
        return StatusCode::PATHCODE_CHECK_ERROR;

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

    //检查数据类型是否合法
    if (TreeParams->valueType < 1 || TreeParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    //检查是否为数组是否合法
    if (TreeParams->isArrary != 0 && TreeParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    //检查是否为时间序列是否合法
    if (TreeParams->isTS != 0 && TreeParams->isTS != 1)
    {
        cout << "isTs只能为0或1" << endl;
        return StatusCode::ISTS_ERROR;
    }

    //检查是否带时间戳是否合法
    if (TreeParams->hasTime != 0 && TreeParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    char valueNmae[30], hasTime[1], pathCode[10], valueType[30]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(pathCode, 0, sizeof(pathCode));

    if (TreeParams->isTS == 1) //是Ts类型,拼接字符串
    {
        if (TreeParams->tsLen < 1)
        {
            cout << "Ts长度不能小于1" << endl;
            return StatusCode::TSLEN_ERROR;
        }
        if (TreeParams->isArrary)
        {
            if (TreeParams->arrayLen < 1)
            {
                cout << "数组长度不能小于１" << endl;
                return StatusCode::ARRAYLEN_ERROR;
            }
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", TreeParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "][0..");
            sprintf(s, "%d", TreeParams->arrayLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(TreeParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
        else
        {
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", TreeParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(TreeParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
    }
    else if (TreeParams->isArrary == 1) //是数组类型,拼接字符串
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

    long fp;
    char mode[3] = {'w', 'b', '+'};
    if (TreeParams->newPath == NULL)
        TreeParams->newPath = TreeParams->pathToLine;
    if (strcmp(TreeParams->pathToLine, TreeParams->newPath) != 0)
    {
        //检查新文件夹名是否包含数字
        string checkPath = TreeParams->newPath;
        for (int i = 0; i < checkPath.length(); i++)
        {
            if (isdigit(checkPath[i]))
            {
                cout << "新文件夹名称包含数字！" << endl;
                return StatusCode::DIR_INCLUDE_NUMBER;
            }
        }

        //创建一个新的文件夹存放新的模板
        temPath = TreeParams->newPath;

        char finalPath[100];
        strcpy(finalPath, settings("Filename_Label").c_str());
        strcat(finalPath, "/");
        strcat(finalPath, const_cast<char *>(temPath.c_str()));
        mkdir(finalPath, 0777);

        temPath.append("/").append(TreeParams->newPath).append(".tem");
    }

    err = DB_Open(const_cast<char *>(temPath.c_str()), mode, &fp);
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

//创建新的.tem文件，根据目前已存在的模板数量进行编号
int DB_UpdateNodeToSchema_MultiTem(struct DB_TreeNodeParams *TreeParams, struct DB_TreeNodeParams *newTreeParams)
{
    int err;
    if (TreeParams->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;
    //如果更新节点的编码、新路径都为空，则保持原状态
    if (newTreeParams->pathCode == NULL)
        newTreeParams->pathCode = TreeParams->pathCode;
    if (newTreeParams->pathToLine == NULL)
        newTreeParams->pathToLine = TreeParams->pathToLine;

    //检查更新后路径编码输入是否合法
    char inputPathCode[10] = {0};
    memcpy(inputPathCode, newTreeParams->pathCode, 10);
    err = checkInputPathcode(inputPathCode);
    if (err != 0)
        return err;

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

    //检查数据类型是否合法
    if (newTreeParams->valueType < 1 || newTreeParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    //检查是否为数组是否合法
    if (newTreeParams->isArrary != 0 && newTreeParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    //检查是否为时间序列是否合法
    if (newTreeParams->isTS != 0 && newTreeParams->isTS != 1)
    {
        cout << "isTs只能为0或1" << endl;
        return StatusCode::ISTS_ERROR;
    }

    //检查是否带时间戳是否合法
    if (newTreeParams->hasTime != 0 && newTreeParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    //如果变量名为空，则保持原状态，否则使用新变量名，并检查变量名是否合法
    string variableName;
    if (newTreeParams->valueName == NULL)
    {
        newTreeParams->valueName = readBuf + pos * 71;
        // memcpy(newTreeParams->valueName,readBuf+pos*71,30);
        variableName = newTreeParams->valueName;
    }
    else
        variableName = newTreeParams->valueName;
    err = checkInputVaribaleName(variableName);
    if (err != 0)
        return StatusCode::VARIABLE_NAME_CHECK_ERROR;

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

    if (newTreeParams->isTS == 1) //是Ts类型,拼接字符串
    {
        if (newTreeParams->tsLen < 1)
        {
            cout << "Ts长度不能小于1" << endl;
            return StatusCode::TSLEN_ERROR;
        }
        if (newTreeParams->isArrary)
        {
            if (newTreeParams->arrayLen < 1)
            {
                cout << "数组长度不能小于１" << endl;
                return StatusCode::ARRAYLEN_ERROR;
            }
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", newTreeParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "][0..");
            sprintf(s, "%d", newTreeParams->arrayLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(newTreeParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
        else
        {
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", newTreeParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(newTreeParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
    }
    else if (newTreeParams->isArrary == 1) //是数组类型,拼接字符串
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
    char mode[3] = {'w', 'b', '+'};
    err = DB_Open(const_cast<char *>(temPath.c_str()), mode, &fp);
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
 * @brief 修改标准模板里的树节点,根据编码进行定位和修改，可以指定新的文件夹存储修改后的模板
 *
 * @param TreeParams 需要修改的节点
 * @param newTreeParams 需要修改的信息
 * @return　0:success,
 *         others: StatusCode
 * @note   更新节点参数必须齐全，pathToLine pathcode valueNmae hasTime valueType isArray arrayLen
 *         新文件夹名称里不能包含数字
 */
int DB_UpdateNodeToSchema(struct DB_TreeNodeParams *TreeParams, struct DB_TreeNodeParams *newTreeParams)
{
    int err;

    if (TreeParams->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;

    //如果更新节点的编码、新路径为空，则保持原状态
    if (newTreeParams->pathToLine == NULL)
        newTreeParams->pathToLine = TreeParams->pathToLine;
    if (newTreeParams->pathCode == NULL)
        newTreeParams->pathCode = TreeParams->pathCode;

    //检查更新后路径编码输入是否合法
    char inputPathCode[10] = {0};
    memcpy(inputPathCode, newTreeParams->pathCode, 10);
    err = checkInputPathcode(inputPathCode);
    if (err != 0)
        return StatusCode::PATHCODE_CHECK_ERROR;

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

    //检查数据类型是否合法
    if (newTreeParams->valueType < 1 || newTreeParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    //检查是否为数组是否合法
    if (newTreeParams->isArrary != 0 && newTreeParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    //检查是否为时间序列是否合法
    if (newTreeParams->isTS != 0 && newTreeParams->isTS != 1)
    {
        cout << "isTs只能为0或1" << endl;
        return StatusCode::ISTS_ERROR;
    }

    //检查是否带时间戳是否合法
    if (newTreeParams->hasTime != 0 && newTreeParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    //如果变量名为空，则保持原状态，否则使用新变量名，并检查变量名是否合法
    string variableName;
    if (newTreeParams->valueName == NULL)
    {
        newTreeParams->valueName = readBuf + pos * 71;
        variableName = newTreeParams->valueName;
    }
    else
        variableName = newTreeParams->valueName;
    err = checkInputVaribaleName(variableName);
    if (err != 0)
        return StatusCode::VARIABLE_NAME_CHECK_ERROR;

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

    if (newTreeParams->isTS == 1) //是Ts类型,拼接字符串
    {
        if (newTreeParams->tsLen < 1)
        {
            cout << "Ts长度不能小于1" << endl;
            return StatusCode::TSLEN_ERROR;
        }
        if (newTreeParams->isArrary)
        {
            if (newTreeParams->arrayLen < 1)
            {
                cout << "数组长度不能小于１" << endl;
                return StatusCode::ARRAYLEN_ERROR;
            }
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", newTreeParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "][0..");
            sprintf(s, "%d", newTreeParams->arrayLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(newTreeParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
        else
        {
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", newTreeParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(newTreeParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
    }
    else if (newTreeParams->isArrary == 1) //是数组类型,拼接字符串
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
    if (strcmp(TreeParams->pathToLine, newTreeParams->pathToLine) != 0)
    {
        //检查新文件夹名是否包含数字
        string checkPath = newTreeParams->pathToLine;
        for (int i = 0; i < checkPath.length(); i++)
        {
            if (isdigit(checkPath[i]))
            {
                cout << "新文件夹名称包含数字！" << endl;
                return StatusCode::DIR_INCLUDE_NUMBER;
            }
        }

        //会创建一个新的文件夹存放新的模板
        temPath = newTreeParams->pathToLine;

        char finalPath[100];
        strcpy(finalPath, settings("Filename_Label").c_str());
        strcat(finalPath, "/");
        strcat(finalPath, const_cast<char *>(temPath.c_str()));
        mkdir(finalPath, 0777);

        temPath.append("/").append(newTreeParams->pathToLine).append(".tem");
    }

    char mode[3] = {'w', 'b', '+'};
    err = DB_Open(const_cast<char *>(temPath.c_str()), mode, &fp);
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

//会创建一个新的.tem文件，根据已存在的模板数量进行编号
int DB_DeleteNodeToSchema_MultiTem(struct DB_TreeNodeParams *TreeParams)
{
    int err;
    if (TreeParams->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;
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
    char mode[3] = {'w', 'b', '+'};
    err = DB_Open(const_cast<char *>(temPath.c_str()), mode, &fp);
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
 * @brief 删除标准模板中已存在的节点,根据编码进行定位和删除，可以指定新文件夹存储修改后的模板
 *
 * @param TreeParams 标准模板参数
 * @return　0:success,
 *         others: StatusCode
 * @note 新文件夹名称里面不能包含数字
 */
int DB_DeleteNodeToSchema(struct DB_TreeNodeParams *TreeParams)
{
    int err;

    if (TreeParams->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;

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
    if (TreeParams->newPath == NULL)
        TreeParams->newPath = TreeParams->pathToLine;
    if (strcmp(TreeParams->pathToLine, TreeParams->newPath) != 0)
    {
        //检查新文件夹名是否包含数字
        string checkPath = TreeParams->newPath;
        for (int i = 0; i < checkPath.length(); i++)
        {
            if (isdigit(checkPath[i]))
            {
                cout << "新文件夹名称包含数字！" << endl;
                return StatusCode::DIR_INCLUDE_NUMBER;
            }
        }

        //创建一个新的文件夹存放新的模板
        temPath = TreeParams->newPath;

        char finalPath[100];
        strcpy(finalPath, settings("Filename_Label").c_str());
        strcat(finalPath, "/");
        strcat(finalPath, const_cast<char *>(temPath.c_str()));
        mkdir(finalPath, 0777);

        temPath.append("/").append(TreeParams->newPath).append(".tem");
    }
    char mode[3] = {'w', 'b', '+'};
    err = DB_Open(const_cast<char *>(temPath.c_str()), mode, &fp);
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

//会创建一个新的.ziptem文件，根据已存在的压缩模板文件数量进行编号
int DB_AddNodeToZipSchema_MultiZiptem(struct DB_ZipNodeParams *ZipParams)
{
    int err;
    if (ZipParams->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;

    if (ZipParams->valueType < 1 || ZipParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    //检查变量名输入是否合法
    string variableName = ZipParams->valueName;
    err = checkInputVaribaleName(variableName);
    if (err != 0)
        return StatusCode::VARIABLE_NAME_CHECK_ERROR;

    //检查是否为数组是否合法
    if (ZipParams->isArrary != 0 && ZipParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    //检查是否为TS是否合法
    if (ZipParams->isTS != 0 && ZipParams->isTS != 1)
    {
        cout << "isTS只能为0或1" << endl;
        return StatusCode::ISTS_ERROR;
    }

    //检查是否带时间戳是否合法
    if (ZipParams->hasTime != 0 && ZipParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    //如果不是数组类型则将数组长度置为0
    if (ZipParams->isArrary == 0)
        ZipParams->arrayLen = 0;

    //如果不是时序类型则将时序长度置为0
    if (ZipParams->isTS == 0)
        ZipParams->tsLen = 0;

    //检测标准值输入是否合法
    string stringStandardValue = ZipParams->standardValue;
    err = checkInputValue(DataType::JudgeValueTypeByNum(ZipParams->valueType), stringStandardValue);
    if (err != 0)
    {
        cout << "标准值输入不合法！" << endl;
        return StatusCode::STANDARD_CHECK_ERROR;
    }

    //检测最大值输入是否合法
    string stringMaxValue = ZipParams->maxValue;
    err = checkInputValue(DataType::JudgeValueTypeByNum(ZipParams->valueType), stringMaxValue);
    if (err != 0)
    {
        cout << "最大值输入不合法！" << endl;
        return StatusCode::MAX_CHECK_ERROR;
    }

    //检测最小值输入是否合法
    string stringMinValue = ZipParams->minValue;
    err = checkInputValue(DataType::JudgeValueTypeByNum(ZipParams->valueType), stringMinValue);
    if (err != 0)
    {
        cout << "最小值输入不合法！" << endl;
        return StatusCode::MIN_CHECK_ERROR;
    }

    //检测值范围是否合法
    err = checkValueRange(DataType::JudgeValueTypeByNum(ZipParams->valueType), stringStandardValue, stringMaxValue, stringMinValue);
    if (err != 0)
        return StatusCode::VALUE_RANGE_ERROR;

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

    err = DB_LoadZipSchema(ZipParams->pathToLine);
    if (err != 0)
        return err;

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len + 80 + 15 * ZipParams->arrayLen];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);
    DataTypeConverter converter;

    for (long i = 0; i < CurrentZipTemplate.schemas.size(); i++) //寻找是否有相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            cout << "存在相同的变量名" << endl;
            return StatusCode::VARIABLE_NAME_EXIST;
        }
        if (CurrentZipTemplate.schemas[i].second.isArray)
            readbuf_pos += 65 + CurrentZipTemplate.schemas[i].second.arrayLen * 5 * 3;
        else
            readbuf_pos += 80;
    }

    //先初始化为0
    char valueNmae[30], valueType[30], standardValue[5 * ZipParams->arrayLen + 5], maxValue[5 * ZipParams->arrayLen + 5], minValue[5 * ZipParams->arrayLen + 5], hasTime[1], timeseriesSpan[4];
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(standardValue, 0, sizeof(standardValue));
    memset(maxValue, 0, sizeof(maxValue));
    memset(minValue, 0, sizeof(minValue));
    memset(timeseriesSpan, 0, sizeof(timeseriesSpan));

    if (ZipParams->isTS == 1) //是Ts类型,拼接字符串
    {
        if (ZipParams->tsLen < 1)
        {
            cout << "Ts长度不能小于1" << endl;
            return StatusCode::TSLEN_ERROR;
        }
        if (ZipParams->isArrary)
        {
            if (ZipParams->arrayLen < 1)
            {
                cout << "数组长度不能小于１" << endl;
                return StatusCode::ARRAYLEN_ERROR;
            }
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", ZipParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "][0..");
            sprintf(s, "%d", ZipParams->arrayLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(ZipParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
        else
        {
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", ZipParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(ZipParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
    }
    else if (ZipParams->isArrary == 1) //是数组类型,拼接字符串
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

    //采样频率
    converter.ToUInt32Buff_m(ZipParams->tsSpan, timeseriesSpan);

    if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "BOOL")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[1];
                uint8_t sValue = (uint8_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                s[0] = sValue;
                memcpy(standardValue + valuePos, s, 1);

                //最大值
                char ma[1];
                uint8_t maValue = (uint8_t)atoi(const_cast<char *>(maxVec[t].c_str()));
                ma[0] = maValue;
                memcpy(maxValue + valuePos, ma, 1);

                //最小值
                char mi[1];
                uint8_t miValue = (uint8_t)atoi(const_cast<char *>(minVec[t].c_str()));
                mi[0] = miValue;
                memcpy(minValue + valuePos, mi, 1);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "USINT")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[1];
                uint8_t sValue = (uint8_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                s[0] = sValue;
                memcpy(standardValue + valuePos, s, 1);

                //最大值
                char ma[1];
                uint8_t maValue = (uint8_t)atoi(const_cast<char *>(maxVec[t].c_str()));
                ma[0] = maValue;
                memcpy(maxValue + valuePos, ma, 1);

                //最小值
                char mi[1];
                uint8_t miValue = (uint8_t)atoi(const_cast<char *>(minVec[t].c_str()));
                mi[0] = miValue;
                memcpy(minValue + valuePos, mi, 1);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "UINT")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[2];
                ushort sValue = (ushort)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToUInt16Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 2);

                //最大值
                char ma[2];
                ushort maValue = (ushort)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToUInt16Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 2);

                //最小值
                char mi[2];
                ushort miValue = (ushort)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToUInt16Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 2);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "UDINT")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[4];
                uint32_t sValue = (uint32_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToUInt32Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 4);

                //最大值
                char ma[4];
                ushort maValue = (ushort)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToUInt32Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 4);

                //最小值
                char mi[4];
                ushort miValue = (ushort)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToUInt32Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 4);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "SINT")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[1];
                int8_t sValue = (int8_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                s[0] = sValue;
                memcpy(standardValue + valuePos, s, 1);

                //最大值
                char ma[1];
                int8_t maValue = (int8_t)atoi(const_cast<char *>(maxVec[t].c_str()));
                ma[0] = maValue;
                memcpy(maxValue + valuePos, ma, 1);

                //最小值
                char mi[1];
                int8_t miValue = (int8_t)atoi(const_cast<char *>(minVec[t].c_str()));
                mi[0] = miValue;
                memcpy(minValue + valuePos, mi, 1);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "INT")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[2];
                short sValue = (short)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToInt16Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 2);

                //最大值
                char ma[2];
                short maValue = (short)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToInt16Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 2);

                //最小值
                char mi[2];
                short miValue = (short)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToInt16Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 2);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "DINT")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[4];
                int sValue = (int)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToInt32Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 4);

                //最大值
                char ma[4];
                int maValue = (int)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToInt32Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 4);

                //最小值
                char mi[4];
                int miValue = (int)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToInt32Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 4);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "REAL")
    {
        if (ZipParams->isArrary == 0)
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
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[4];
                float sValue = (float)atof(const_cast<char *>(standardVec[t].c_str()));
                converter.ToFloatBuff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 4);

                //最大值
                char ma[4];
                float maValue = (float)atof(const_cast<char *>(maxVec[t].c_str()));
                converter.ToFloatBuff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 4);

                //最小值
                char mi[4];
                float miValue = (float)atof(const_cast<char *>(minVec[t].c_str()));
                converter.ToFloatBuff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 4);

                valuePos += 5;
            }
        }
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

    char writeBuf[80 + 15 * ZipParams->arrayLen]; //将所有参数传入writeBuf中，已追加写的方式写入已有模板文件中
    if (ZipParams->isArrary == 0)
    {
        memcpy(writeBuf, valueNmae, 30);
        memcpy(writeBuf + 30, valueType, 30);
        memcpy(writeBuf + 60, standardValue, 5);
        memcpy(writeBuf + 65, maxValue, 5);
        memcpy(writeBuf + 70, minValue, 5);
        memcpy(writeBuf + 75, hasTime, 1);
        memcpy(writeBuf + 76, timeseriesSpan, 4);
        memcpy(readBuf + len, writeBuf, 80);
    }
    else
    {
        memcpy(writeBuf, valueNmae, 30);
        memcpy(writeBuf + 30, valueType, 30);
        memcpy(writeBuf + 60, standardValue, 5 * ZipParams->arrayLen);
        memcpy(writeBuf + 60 + 5 * ZipParams->arrayLen, maxValue, 5 * ZipParams->arrayLen);
        memcpy(writeBuf + 60 + 10 * ZipParams->arrayLen, minValue, 5 * ZipParams->arrayLen);
        memcpy(writeBuf + 60 + 15 * ZipParams->arrayLen, hasTime, 1);
        memcpy(writeBuf + 61 + 15 * ZipParams->arrayLen, timeseriesSpan, 4);
        memcpy(readBuf + len, writeBuf, 65 + 15 * ZipParams->arrayLen);
    }

    //创建一个新的.ziptem文件，根据当前已存在的压缩模板数量进行编号
    long fp;
    vector<string> ziptemFiles;
    readZIPTEMFilesList(ZipParams->pathToLine, ziptemFiles);
    int ziptemNum = ziptemFiles.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", ziptemNum);
    temPath = ZipParams->pathToLine;
    temPath.append("/").append(ZipParams->pathToLine).append(appendNum).append(".ziptem");
    char mode[3] = {'w', 'b'};
    err = DB_Open(const_cast<char *>(temPath.c_str()), mode, &fp);
    if (err == 0)
    {
        if (ZipParams->isArrary == 0)
            err = DB_Write(fp, readBuf, len + 80);
        else
            err = DB_Write(fp, readBuf, len + 65 + 15 * ZipParams->arrayLen);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    return err;
}

/**
 * @brief 向压缩模板添加新节点，暂定直接在尾部追加，可以指定新的文件夹存放修改后的压缩模板
 *
 * @param ZipParams 压缩模板参数
 * @return　0:success,
 *         others: StatusCode
 * @note   新节点参数必须齐全，pathToLine valueNmae hasTime valueType isArray arrayLen standardValue maxValue minValue newPath
 *         新文件夹名称里面不能包含数字
 */
int DB_AddNodeToZipSchema(struct DB_ZipNodeParams *ZipParams)
{
    int err;

    if (ZipParams->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;

    if (ZipParams->newPath == NULL)
        ZipParams->newPath = ZipParams->pathToLine;

    //检测数据类型是否合法
    if (ZipParams->valueType < 1 || ZipParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    //检查变量名输入是否合法
    string variableName = ZipParams->valueName;
    err = checkInputVaribaleName(variableName);
    if (err != 0)
        return StatusCode::VARIABLE_NAME_CHECK_ERROR;

    //检查是否为数组是否合法
    if (ZipParams->isArrary != 0 && ZipParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    //检查是否为TS是否合法
    if (ZipParams->isTS != 0 && ZipParams->isTS != 1)
    {
        cout << "isTS只能为0或1" << endl;
        return StatusCode::ISTS_ERROR;
    }

    //检测是否带时间戳是否合法
    if (ZipParams->hasTime != 0 && ZipParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

    //如果不是数组类型则将数组长度置为0
    if (ZipParams->isArrary == 0)
        ZipParams->arrayLen = 0;

    //如果不是时序类型则将时序长度置为0
    if (ZipParams->isTS == 0)
        ZipParams->tsLen = 0;

    //检测标准值输入是否合法
    string stringStandardValue = ZipParams->standardValue;
    // err = checkInputValue(DataType::JudgeValueTypeByNum(ZipParams->valueType), stringStandardValue);
    if (err != 0)
    {
        cout << "标准值输入不合法！" << endl;
        return StatusCode::STANDARD_CHECK_ERROR;
    }

    //检测最大值输入是否合法
    string stringMaxValue = ZipParams->maxValue;
    // err = checkInputValue(DataType::JudgeValueTypeByNum(ZipParams->valueType), stringMaxValue);
    if (err != 0)
    {
        cout << "最大值输入不合法！" << endl;
        return StatusCode::MAX_CHECK_ERROR;
    }

    //检测最小值输入是否合法
    string stringMinValue = ZipParams->minValue;
    // err = checkInputValue(DataType::JudgeValueTypeByNum(ZipParams->valueType), stringMinValue);
    if (err != 0)
    {
        cout << "最小值输入不合法！" << endl;
        return StatusCode::MIN_CHECK_ERROR;
    }

    //检测值范围是否合法
    // err = checkValueRange(DataType::JudgeValueTypeByNum(ZipParams->valueType), stringStandardValue, stringMaxValue, stringMinValue);
    if (err != 0)
        return StatusCode::VALUE_RANGE_ERROR;

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

    err = DB_LoadZipSchema(ZipParams->pathToLine);
    if (err != 0)
        return err;

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len + 80 + 15 * ZipParams->arrayLen];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);
    DataTypeConverter converter;

    for (long i = 0; i < CurrentZipTemplate.schemas.size(); i++) //寻找是否有相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            cout << "存在相同的变量名" << endl;
            return StatusCode::VARIABLE_NAME_EXIST;
        }
        if (CurrentZipTemplate.schemas[i].second.isArray)
            readbuf_pos += 65 + CurrentZipTemplate.schemas[i].second.arrayLen * 5 * 3;
        else
            readbuf_pos += 80;
    }

    //先初始化为0
    char valueNmae[30], valueType[30], standardValue[5 * ZipParams->arrayLen + 5], maxValue[5 * ZipParams->arrayLen + 5], minValue[5 * ZipParams->arrayLen + 5], hasTime[1], timeseriesSpan[4];
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(standardValue, 0, sizeof(standardValue));
    memset(maxValue, 0, sizeof(maxValue));
    memset(minValue, 0, sizeof(minValue));
    memset(timeseriesSpan, 0, sizeof(timeseriesSpan));

    if (ZipParams->isTS == 1) //是Ts类型,拼接字符串
    {
        if (ZipParams->tsLen < 1)
        {
            cout << "Ts长度不能小于1" << endl;
            return StatusCode::TSLEN_ERROR;
        }
        if (ZipParams->isArrary)
        {
            if (ZipParams->arrayLen < 1)
            {
                cout << "数组长度不能小于１" << endl;
                return StatusCode::ARRAYLEN_ERROR;
            }
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", ZipParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "][0..");
            sprintf(s, "%d", ZipParams->arrayLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(ZipParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
        else
        {
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", ZipParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(ZipParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
    }
    else if (ZipParams->isArrary == 1) //是数组类型,拼接字符串
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

    //采样频率
    converter.ToUInt32Buff_m(ZipParams->tsSpan, timeseriesSpan);

    if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "BOOL")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[1];
                uint8_t sValue = (uint8_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                s[0] = sValue;
                memcpy(standardValue + valuePos, s, 1);

                //最大值
                char ma[1];
                uint8_t maValue = (uint8_t)atoi(const_cast<char *>(maxVec[t].c_str()));
                ma[0] = maValue;
                memcpy(maxValue + valuePos, ma, 1);

                //最小值
                char mi[1];
                uint8_t miValue = (uint8_t)atoi(const_cast<char *>(minVec[t].c_str()));
                mi[0] = miValue;
                memcpy(minValue + valuePos, mi, 1);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "USINT")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[1];
                uint8_t sValue = (uint8_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                s[0] = sValue;
                memcpy(standardValue + valuePos, s, 1);

                //最大值
                char ma[1];
                uint8_t maValue = (uint8_t)atoi(const_cast<char *>(maxVec[t].c_str()));
                ma[0] = maValue;
                memcpy(maxValue + valuePos, ma, 1);

                //最小值
                char mi[1];
                uint8_t miValue = (uint8_t)atoi(const_cast<char *>(minVec[t].c_str()));
                mi[0] = miValue;
                memcpy(minValue + valuePos, mi, 1);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "UINT")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[2];
                ushort sValue = (ushort)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToUInt16Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 2);

                //最大值
                char ma[2];
                ushort maValue = (ushort)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToUInt16Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 2);

                //最小值
                char mi[2];
                ushort miValue = (ushort)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToUInt16Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 2);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "UDINT")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[4];
                uint32_t sValue = (uint32_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToUInt32Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 4);

                //最大值
                char ma[4];
                ushort maValue = (ushort)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToUInt32Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 4);

                //最小值
                char mi[4];
                ushort miValue = (ushort)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToUInt32Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 4);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "SINT")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[1];
                int8_t sValue = (int8_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                s[0] = sValue;
                memcpy(standardValue + valuePos, s, 1);

                //最大值
                char ma[1];
                int8_t maValue = (int8_t)atoi(const_cast<char *>(maxVec[t].c_str()));
                ma[0] = maValue;
                memcpy(maxValue + valuePos, ma, 1);

                //最小值
                char mi[1];
                int8_t miValue = (int8_t)atoi(const_cast<char *>(minVec[t].c_str()));
                mi[0] = miValue;
                memcpy(minValue + valuePos, mi, 1);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "INT")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[2];
                short sValue = (short)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToInt16Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 2);

                //最大值
                char ma[2];
                short maValue = (short)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToInt16Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 2);

                //最小值
                char mi[2];
                short miValue = (short)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToInt16Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 2);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "DINT")
    {
        if (ZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[4];
                int sValue = (int)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToInt32Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 4);

                //最大值
                char ma[4];
                int maValue = (int)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToInt32Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 4);

                //最小值
                char mi[4];
                int miValue = (int)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToInt32Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 4);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(ZipParams->valueType) == "REAL")
    {
        if (ZipParams->isArrary == 0)
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
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < ZipParams->arrayLen; t++)
            {
                //标准值
                char s[4];
                float sValue = (float)atof(const_cast<char *>(standardVec[t].c_str()));
                converter.ToFloatBuff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 4);

                //最大值
                char ma[4];
                float maValue = (float)atof(const_cast<char *>(maxVec[t].c_str()));
                converter.ToFloatBuff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 4);

                //最小值
                char mi[4];
                float miValue = (float)atof(const_cast<char *>(minVec[t].c_str()));
                converter.ToFloatBuff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 4);

                valuePos += 5;
            }
        }
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

    char writeBuf[80 + 15 * ZipParams->arrayLen]; //将所有参数传入writeBuf中，已追加写的方式写入已有模板文件中
    if (ZipParams->isArrary == 0)
    {
        memcpy(writeBuf, valueNmae, 30);
        memcpy(writeBuf + 30, valueType, 30);
        memcpy(writeBuf + 60, standardValue, 5);
        memcpy(writeBuf + 65, maxValue, 5);
        memcpy(writeBuf + 70, minValue, 5);
        memcpy(writeBuf + 75, hasTime, 1);
        memcpy(writeBuf + 76, timeseriesSpan, 4);
        memcpy(readBuf + len, writeBuf, 80);
    }
    else
    {
        memcpy(writeBuf, valueNmae, 30);
        memcpy(writeBuf + 30, valueType, 30);
        memcpy(writeBuf + 60, standardValue, 5 * ZipParams->arrayLen);
        memcpy(writeBuf + 60 + 5 * ZipParams->arrayLen, maxValue, 5 * ZipParams->arrayLen);
        memcpy(writeBuf + 60 + 10 * ZipParams->arrayLen, minValue, 5 * ZipParams->arrayLen);
        memcpy(writeBuf + 60 + 15 * ZipParams->arrayLen, hasTime, 1);
        memcpy(writeBuf + 61 + 15 * ZipParams->arrayLen, timeseriesSpan, 4);
        memcpy(readBuf + len, writeBuf, 65 + 15 * ZipParams->arrayLen);
    }

    //创建一个新的.ziptem文件，根据当前已存在的压缩模板数量进行编号
    long fp;
    char mode[3] = {'w', 'b', '+'};
    if (strcmp(ZipParams->pathToLine, ZipParams->newPath) != 0)
    {
        //检查新文件夹名是否包含数字
        string checkPath = ZipParams->newPath;
        for (int i = 0; i < checkPath.length(); i++)
        {
            if (isdigit(checkPath[i]))
            {
                cout << "新文件夹名称包含数字！" << endl;
                return StatusCode::DIR_INCLUDE_NUMBER;
            }
        }

        //创建一个新的文件夹存放新的模板
        temPath = ZipParams->newPath;

        char finalPath[100];
        strcpy(finalPath, settings("Filename_Label").c_str());
        strcat(finalPath, "/");
        strcat(finalPath, const_cast<char *>(temPath.c_str()));
        mkdir(finalPath, 0777);

        temPath.append("/").append(ZipParams->newPath).append(".ziptem");
    }

    //写文件
    err = DB_Open(const_cast<char *>(temPath.c_str()), mode, &fp);
    if (err == 0)
    {
        if (ZipParams->isArrary == 0)
            err = DB_Write(fp, readBuf, len + 80);
        else
            err = DB_Write(fp, readBuf, len + 65 + 15 * ZipParams->arrayLen);
        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    return err;
}

//会创建一个新的.ziptem文件，根据当前已存在的压缩模板数量进行编号
int DB_UpdateNodeToZipSchema_MultiZiptem(struct DB_ZipNodeParams *ZipParams, struct DB_ZipNodeParams *newZipParams)
{
    int err;

    if (ZipParams->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;
    //如果新节点的路径、变量名为空，则保持原状态
    if (newZipParams->pathToLine == NULL)
        newZipParams->pathToLine = ZipParams->pathToLine;
    if (newZipParams->valueName == NULL)
        newZipParams->valueName = ZipParams->valueName;

    //检查更新后数据类型是否合法
    if (newZipParams->valueType < 1 || newZipParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    //检查更新后变量名输入是否合法
    string variableName = newZipParams->valueName;
    err = checkInputVaribaleName(variableName);
    if (err != 0)
        return err;

    //检查更新后是否为数组是否合法
    if (newZipParams->isArrary != 0 && newZipParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    //检查是否为TS是否合法
    if (ZipParams->isTS != 0 && ZipParams->isTS != 1)
    {
        cout << "isTS只能为0或1" << endl;
        return StatusCode::ISTS_ERROR;
    }

    //检查更新后是否带时间戳是否合法
    if (newZipParams->hasTime != 0 && newZipParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

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

    err = DB_LoadZipSchema(ZipParams->pathToLine);
    if (err != 0)
        return err;

     long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    long writefront_len = 0; //用于记录被修改节点之前的节点信息的长度
    long writeafter_pos = 0; //用于记录被修改节点之后的节点信息的位置
    long writeafter_len = 0; //用于记录被修改节点之后的节点信息的长度
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    DataTypeConverter converter;

    long pos = -1;                                               //用于定位是修改模板的第几条
    for (long i = 0; i < CurrentZipTemplate.schemas.size(); i++) //寻找是否有相同变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            pos = i; //记录这条记录的位置
            break;
        }
        if (CurrentZipTemplate.schemas[i].second.isArray)
            readbuf_pos += 65 + 3 * 5 * CurrentZipTemplate.schemas[i].second.arrayLen;
        else
            readbuf_pos += 80;
    }
    if (pos == -1)
    {
        cout << "未找到变量名！" << endl;
        return StatusCode::UNKNOWN_VARIABLE_NAME;
    }

    writefront_len = readbuf_pos; //用于最后定位被更新模板结点的之前节点的位置
    //用于最后定位被更新模板结点之后的节点的位置
    if (CurrentZipTemplate.schemas[pos].second.isArray)
    {
        writeafter_pos = readbuf_pos + 65 + 15 * CurrentZipTemplate.schemas[pos].second.arrayLen;
        writeafter_len = len - readbuf_pos - 65 - 15 * CurrentZipTemplate.schemas[pos].second.arrayLen;
    }
    else
    {
        writeafter_pos = readbuf_pos + 80;
        writeafter_len = len - readbuf_pos - 80;
    }

    //如果不是数组类型则将数组长度置为0
    if (newZipParams->isArrary == 0)
        newZipParams->arrayLen = 0;

    //如果不是时序类型则将时序长度置为0
    if (newZipParams->isTS == 0)
        newZipParams->tsLen = 0;

    //是数组且数组长度为0则采用模板原数组长度
    if (newZipParams->isArrary == 1 && newZipParams->arrayLen == 0)
    {
        if (CurrentZipTemplate.schemas[pos].second.isArray)
            newZipParams->arrayLen = CurrentZipTemplate.schemas[pos].second.arrayLen;
        else
        {
            cout << "数组长度不能为0" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
    }

    //是时序且时序长度为0则采用模板原时序长度
    if (newZipParams->isTS == 1 && newZipParams->tsLen == 0)
    {
        if (CurrentZipTemplate.schemas[pos].second.isTimeseries)
            newZipParams->tsLen = CurrentZipTemplate.schemas[pos].second.tsLen;
        else
        {
            cout << "时序类型长度不能为0" << endl;
            return StatusCode::TSLEN_ERROR;
        }
    }

    int s_new = 0, max_new = 0, min_new = 0;
    if (newZipParams->standardValue == NULL)
    {
        s_new = 1;
        newZipParams->standardValue = new char[10];
        getValueStringByValueType(newZipParams->standardValue, CurrentZipTemplate.schemas[pos].second.valueType, pos, 0);
    }
    if (newZipParams->maxValue == NULL)
    {
        max_new = 1;
        newZipParams->maxValue = new char[10];
        getValueStringByValueType(newZipParams->maxValue, CurrentZipTemplate.schemas[pos].second.valueType, pos, 1);
    }
    if (newZipParams->minValue == NULL)
    {
        min_new = 1;
        newZipParams->minValue = new char[10];
        getValueStringByValueType(newZipParams->minValue, CurrentZipTemplate.schemas[pos].second.valueType, pos, 2);
    }

    //检测更新后标准值输入是否合法
    string stringStandardValue = newZipParams->standardValue;
    // err = checkInputValue(DataType::JudgeValueTypeByNum(newZipParams->valueType), stringStandardValue);
    if (err != 0)
    {
        cout << "标准值输入不合法！" << endl;
        return StatusCode::STANDARD_CHECK_ERROR;
    }

    //检测更新后最大值输入是否合法
    string stringMaxValue = newZipParams->maxValue;
    // err = checkInputValue(DataType::JudgeValueTypeByNum(newZipParams->valueType), stringMaxValue);
    if (err != 0)
    {
        cout << "最大值输入不合法！" << endl;
        return StatusCode::MAX_CHECK_ERROR;
    }

    //检测更新后最小值输入是否合法
    string stringMinValue = newZipParams->minValue;
    // err = checkInputValue(DataType::JudgeValueTypeByNum(newZipParams->valueType), stringMinValue);
    if (err != 0)
    {
        cout << "最小值输入不合法！" << endl;
        return StatusCode::MIN_CHECK_ERROR;
    }

    //检测值范围是否合法
    // err = checkValueRange(DataType::JudgeValueTypeByNum(newZipParams->valueType), stringStandardValue, stringMaxValue, stringMinValue);
    if (err != 0)
        return err;

    readbuf_pos = 0;
    for (long i = 0; i < CurrentZipTemplate.schemas.size(); i++) //寻找模板是否有与更新参数相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(newZipParams->valueName, existValueName) == 0 && i != pos)
        {
            cout << "更新参数存在模板已有的变量名" << endl;
            return StatusCode::VARIABLE_NAME_EXIST;
        }
        if (CurrentZipTemplate.schemas[i].second.isArray)
            readbuf_pos += 65 + 3 * 5 * CurrentZipTemplate.schemas[i].second.arrayLen;
        else
            readbuf_pos += 80;
    }

    char valueNmae[30], valueType[30], standardValue[5 * newZipParams->arrayLen + 5], maxValue[5 * newZipParams->arrayLen + 5], minValue[5 * newZipParams->arrayLen + 5], hasTime[1], timeseriesSpan[4];
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(standardValue, 0, sizeof(standardValue));
    memset(maxValue, 0, sizeof(maxValue));
    memset(minValue, 0, sizeof(minValue));
    memset(timeseriesSpan, 0, sizeof(timeseriesSpan));

    if (newZipParams->isTS == 1) //是Ts类型,拼接字符串
    {
        if (newZipParams->tsLen < 1)
        {
            cout << "Ts长度不能小于1" << endl;
            return StatusCode::TSLEN_ERROR;
        }
        if (newZipParams->isArrary)
        {
            if (newZipParams->arrayLen < 1)
            {
                cout << "数组长度不能小于１" << endl;
                return StatusCode::ARRAYLEN_ERROR;
            }
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", newZipParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "][0..");
            sprintf(s, "%d", newZipParams->arrayLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(newZipParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
        else
        {
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", newZipParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(newZipParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
    }
    else if (newZipParams->isArrary == 1) //是数组类型,拼接字符串
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

    //采样频率
    converter.ToUInt32Buff_m(newZipParams->tsSpan, timeseriesSpan);

    if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "BOOL")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[1];
                uint8_t sValue = (uint8_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                s[0] = sValue;
                memcpy(standardValue + valuePos, s, 1);

                //最大值
                char ma[1];
                uint8_t maValue = (uint8_t)atoi(const_cast<char *>(maxVec[t].c_str()));
                ma[0] = maValue;
                memcpy(maxValue + valuePos, ma, 1);

                //最小值
                char mi[1];
                uint8_t miValue = (uint8_t)atoi(const_cast<char *>(minVec[t].c_str()));
                mi[0] = miValue;
                memcpy(minValue + valuePos, mi, 1);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "USINT")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[1];
                uint8_t sValue = (uint8_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                s[0] = sValue;
                memcpy(standardValue + valuePos, s, 1);

                //最大值
                char ma[1];
                uint8_t maValue = (uint8_t)atoi(const_cast<char *>(maxVec[t].c_str()));
                ma[0] = maValue;
                memcpy(maxValue + valuePos, ma, 1);

                //最小值
                char mi[1];
                uint8_t miValue = (uint8_t)atoi(const_cast<char *>(minVec[t].c_str()));
                mi[0] = miValue;
                memcpy(minValue + valuePos, mi, 1);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "UINT")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[2];
                ushort sValue = (ushort)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToUInt16Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 2);

                //最大值
                char ma[2];
                ushort maValue = (ushort)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToUInt16Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 2);

                //最小值
                char mi[2];
                ushort miValue = (ushort)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToUInt16Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 2);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "UDINT")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[4];
                uint32_t sValue = (uint32_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToUInt32Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 4);

                //最大值
                char ma[4];
                ushort maValue = (ushort)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToUInt32Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 4);

                //最小值
                char mi[4];
                ushort miValue = (ushort)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToUInt32Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 4);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "SINT")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[1];
                int8_t sValue = (int8_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                s[0] = sValue;
                memcpy(standardValue + valuePos, s, 1);

                //最大值
                char ma[1];
                int8_t maValue = (int8_t)atoi(const_cast<char *>(maxVec[t].c_str()));
                ma[0] = maValue;
                memcpy(maxValue + valuePos, ma, 1);

                //最小值
                char mi[1];
                int8_t miValue = (int8_t)atoi(const_cast<char *>(minVec[t].c_str()));
                mi[0] = miValue;
                memcpy(minValue + valuePos, mi, 1);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "INT")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[2];
                short sValue = (short)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToInt16Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 2);

                //最大值
                char ma[2];
                short maValue = (short)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToInt16Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 2);

                //最小值
                char mi[2];
                short miValue = (short)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToInt16Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 2);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "DINT")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[4];
                int sValue = (int)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToInt32Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 4);

                //最大值
                char ma[4];
                int maValue = (int)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToInt32Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 4);

                //最小值
                char mi[4];
                int miValue = (int)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToInt32Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 4);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "REAL")
    {
        if (newZipParams->isArrary == 0)
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
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[4];
                float sValue = (float)atof(const_cast<char *>(standardVec[t].c_str()));
                converter.ToFloatBuff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 4);

                //最大值
                char ma[4];
                float maValue = (float)atof(const_cast<char *>(maxVec[t].c_str()));
                converter.ToFloatBuff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 4);

                //最小值
                char mi[4];
                float miValue = (float)atof(const_cast<char *>(minVec[t].c_str()));
                converter.ToFloatBuff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 4);

                valuePos += 5;
            }
        }
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

    char *writebuf = new char[len + newZipParams->arrayLen * 15];
    //将所有参数传入writeBuf中，已覆盖写的方式写入已有模板文件中
    memcpy(writebuf, readBuf, writefront_len);
    memcpy(writebuf + writefront_len, valueNmae, 30);
    memcpy(writebuf + writefront_len + 30, valueType, 30);
    if (newZipParams->isArrary == 0)
    {
        memcpy(writebuf + writefront_len + 60, standardValue, 5);
        memcpy(writebuf + writefront_len + 65, maxValue, 5);
        memcpy(writebuf + writefront_len + 70, minValue, 5);
        memcpy(writebuf + writefront_len + 75, hasTime, 1);
        memcpy(writebuf + writefront_len + 76, timeseriesSpan, 4);
        memcpy(writebuf + writefront_len + 80, readBuf + writeafter_pos, writeafter_len);
    }
    else
    {
        memcpy(writebuf + writefront_len + 60, standardValue, 5 * newZipParams->arrayLen);
        memcpy(writebuf + writefront_len + 60 + 5 * newZipParams->arrayLen, maxValue, 5 * newZipParams->arrayLen);
        memcpy(writebuf + writefront_len + 60 + 10 * newZipParams->arrayLen, minValue, 5 * newZipParams->arrayLen);
        memcpy(writebuf + writefront_len + 60 + 15 * newZipParams->arrayLen, hasTime, 1);
        memcpy(writebuf + writefront_len + 61 + 15 * newZipParams->arrayLen, timeseriesSpan, 4);
        memcpy(writebuf + writefront_len + 65 + 15 * newZipParams->arrayLen, readBuf + writeafter_pos, writeafter_len);
    }

    long finalLength = 0;
    if(newZipParams->isArrary==1)
        finalLength = writefront_len + writeafter_len + 65 + 15 * newZipParams->arrayLen;
    else
        finalLength = writefront_len + writeafter_len + 80;

    //创建一个新的.ziptem文件，根据当前已存在的压缩模板数量进行编号
    long fp;
    vector<string> ziptemFiles;
    readZIPTEMFilesList(ZipParams->pathToLine, ziptemFiles);
    int ziptemNum = ziptemFiles.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", ziptemNum);
    temPath = ZipParams->pathToLine;
    temPath.append("/").append(ZipParams->pathToLine).append(appendNum).append(".ziptem");
    char mode[3] = {'w', 'b', '+'};
    err = DB_Open(const_cast<char *>(temPath.c_str()), mode, &fp);
    if (err == 0)
    {
        err = DB_Write(fp, writebuf, finalLength);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    if (s_new == 1)
        delete[] newZipParams->standardValue;
    if (max_new == 1)
        delete[] newZipParams->maxValue;
    if (min_new == 1)
        delete[] newZipParams->minValue;
    return err;
}

/**
 * @brief 修改压缩模板里的树节点，可以指定新的文件夹存储修改后的压缩模板
 *
 * @param Zipparams 需要修改的节点
 * @param newZipParams 需要修改的信息
 * @return　0:success,
 *         others: StatusCode
 * @note   更新节点参数必须齐全，pathToLine valueNmae hasTime valueType isArray arrayLen standardValue maxValue minValue
 *         新文件夹名称里不能包含数字
 */
int DB_UpdateNodeToZipSchema(struct DB_ZipNodeParams *ZipParams, struct DB_ZipNodeParams *newZipParams)
{
    int err;

    if (ZipParams->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;

    //如果新节点的路径、变量名为空，则保持原状态,newZipParams的pathToLine为指定的新文件夹
    if (newZipParams->pathToLine == NULL)
        newZipParams->pathToLine = ZipParams->pathToLine;
    if (newZipParams->valueName == NULL)
        newZipParams->valueName = ZipParams->valueName;

    //检查更新后数据类型是否合法
    if (newZipParams->valueType < 1 || newZipParams->valueType > 10)
    {
        cout << "数据类型选择错误" << endl;
        return StatusCode::UNKNOWN_TYPE;
    }

    //检查更新后变量名输入是否合法
    string variableName = newZipParams->valueName;
    err = checkInputVaribaleName(variableName);
    if (err != 0)
        return err;

    //检查更新后是否为数组是否合法
    if (newZipParams->isArrary != 0 && newZipParams->isArrary != 1)
    {
        cout << "isArray只能为0或1" << endl;
        return StatusCode::ISARRAY_ERROR;
    }

    //检查是否为TS是否合法
    if (ZipParams->isTS != 0 && ZipParams->isTS != 1)
    {
        cout << "isTS只能为0或1" << endl;
        return StatusCode::ISTS_ERROR;
    }

    //检查更新后是否带时间戳是否合法
    if (newZipParams->hasTime != 0 && newZipParams->hasTime != 1)
    {
        cout << "hasTime只能为0或1" << endl;
        return StatusCode::HASTIME_ERROR;
    }

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

    err = DB_LoadZipSchema(ZipParams->pathToLine);
    if (err != 0)
        return err;

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    long writefront_len = 0; //用于记录被修改节点之前的节点信息的长度
    long writeafter_pos = 0; //用于记录被修改节点之后的节点信息的位置
    long writeafter_len = 0; //用于记录被修改节点之后的节点信息的长度
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    DataTypeConverter converter;

    long pos = -1;                                               //用于定位是修改模板的第几条
    for (long i = 0; i < CurrentZipTemplate.schemas.size(); i++) //寻找是否有相同变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            pos = i; //记录这条记录的位置
            break;
        }
        if (CurrentZipTemplate.schemas[i].second.isArray)
            readbuf_pos += 65 + 3 * 5 * CurrentZipTemplate.schemas[i].second.arrayLen;
        else
            readbuf_pos += 80;
    }
    if (pos == -1)
    {
        cout << "未找到变量名！" << endl;
        return StatusCode::UNKNOWN_VARIABLE_NAME;
    }

    writefront_len = readbuf_pos; //用于最后定位被更新模板结点的之前节点的位置
    //用于最后定位被更新模板结点之后的节点的位置
    if (CurrentZipTemplate.schemas[pos].second.isArray)
    {
        writeafter_pos = readbuf_pos + 65 + 15 * CurrentZipTemplate.schemas[pos].second.arrayLen;
        writeafter_len = len - readbuf_pos - 65 - 15 * CurrentZipTemplate.schemas[pos].second.arrayLen;
    }
    else
    {
        writeafter_pos = readbuf_pos + 80;
        writeafter_len = len - readbuf_pos - 80;
    }

    //如果不是数组类型则将数组长度置为0
    if (newZipParams->isArrary == 0)
        newZipParams->arrayLen = 0;

    //如果不是时序类型则将时序长度置为0
    if (newZipParams->isTS == 0)
        newZipParams->tsLen = 0;

    //是数组且数组长度为0则采用模板原数组长度
    if (newZipParams->isArrary == 1 && newZipParams->arrayLen == 0)
    {
        if (CurrentZipTemplate.schemas[pos].second.isArray)
            newZipParams->arrayLen = CurrentZipTemplate.schemas[pos].second.arrayLen;
        else
        {
            cout << "数组长度不能为0" << endl;
            return StatusCode::ARRAYLEN_ERROR;
        }
    }

    //是时序且时序长度为0则采用模板原时序长度
    if (newZipParams->isTS == 1 && newZipParams->tsLen == 0)
    {
        if (CurrentZipTemplate.schemas[pos].second.isTimeseries)
            newZipParams->tsLen = CurrentZipTemplate.schemas[pos].second.tsLen;
        else
        {
            cout << "时序类型长度不能为0" << endl;
            return StatusCode::TSLEN_ERROR;
        }
    }

    int s_new = 0, max_new = 0, min_new = 0; //用于标志是否申请了空间
    if (newZipParams->standardValue == NULL)
    {
        s_new = 1;
        newZipParams->standardValue = new char[10];
        getValueStringByValueType(newZipParams->standardValue, CurrentZipTemplate.schemas[pos].second.valueType, pos, 0);
    }
    if (newZipParams->maxValue == NULL)
    {
        max_new = 1;
        newZipParams->maxValue = new char[10];
        getValueStringByValueType(newZipParams->maxValue, CurrentZipTemplate.schemas[pos].second.valueType, pos, 1);
    }
    if (newZipParams->minValue == NULL)
    {
        min_new = 1;
        newZipParams->minValue = new char[10];
        getValueStringByValueType(newZipParams->minValue, CurrentZipTemplate.schemas[pos].second.valueType, pos, 2);
    }

    //检测更新后标准值输入是否合法
    string stringStandardValue = newZipParams->standardValue;
    // err = checkInputValue(DataType::JudgeValueTypeByNum(newZipParams->valueType), stringStandardValue);
    if (err != 0)
    {
        cout << "标准值输入不合法！" << endl;
        return StatusCode::STANDARD_CHECK_ERROR;
    }

    //检测更新后最大值输入是否合法
    string stringMaxValue = newZipParams->maxValue;
    // err = checkInputValue(DataType::JudgeValueTypeByNum(newZipParams->valueType), stringMaxValue);
    if (err != 0)
    {
        cout << "最大值输入不合法！" << endl;
        return StatusCode::MAX_CHECK_ERROR;
    }

    //检测更新后最小值输入是否合法
    string stringMinValue = newZipParams->minValue;
    // err = checkInputValue(DataType::JudgeValueTypeByNum(newZipParams->valueType), stringMinValue);
    if (err != 0)
    {
        cout << "最小值输入不合法！" << endl;
        return StatusCode::MIN_CHECK_ERROR;
    }

    //检测值范围是否合法
    // err = checkValueRange(DataType::JudgeValueTypeByNum(newZipParams->valueType), stringStandardValue, stringMaxValue, stringMinValue);
    if (err != 0)
        return err;

    readbuf_pos = 0;
    for (long i = 0; i < CurrentZipTemplate.schemas.size(); i++) //寻找模板是否有与更新参数相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(newZipParams->valueName, existValueName) == 0 && i != pos)
        {
            cout << "更新参数存在模板已有的变量名" << endl;
            return StatusCode::VARIABLE_NAME_EXIST;
        }
        if (CurrentZipTemplate.schemas[i].second.isArray)
            readbuf_pos += 65 + 3 * 5 * CurrentZipTemplate.schemas[i].second.arrayLen;
        else
            readbuf_pos += 80;
    }

    char valueNmae[30], valueType[30], standardValue[5 * newZipParams->arrayLen + 5], maxValue[5 * newZipParams->arrayLen + 5], minValue[5 * newZipParams->arrayLen + 5], hasTime[1], timeseriesSpan[4];
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(standardValue, 0, sizeof(standardValue));
    memset(maxValue, 0, sizeof(maxValue));
    memset(minValue, 0, sizeof(minValue));
    memset(timeseriesSpan, 0, sizeof(timeseriesSpan));

    if (newZipParams->isTS == 1) //是Ts类型,拼接字符串
    {
        if (newZipParams->tsLen < 1)
        {
            cout << "Ts长度不能小于1" << endl;
            return StatusCode::TSLEN_ERROR;
        }
        if (newZipParams->isArrary)
        {
            if (newZipParams->arrayLen < 1)
            {
                cout << "数组长度不能小于１" << endl;
                return StatusCode::ARRAYLEN_ERROR;
            }
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", newZipParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "][0..");
            sprintf(s, "%d", newZipParams->arrayLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(newZipParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
        else
        {
            strcpy(valueType, "TS [0..");
            char s[10];
            sprintf(s, "%d", newZipParams->tsLen);
            strcat(valueType, s);
            strcat(valueType, "] OF ");
            string valueTypeStr = DataType::JudgeValueTypeByNum(newZipParams->valueType);
            strcat(valueType, const_cast<char *>(valueTypeStr.c_str()));
        }
    }
    else if (newZipParams->isArrary == 1) //是数组类型,拼接字符串
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

    //采样频率
    converter.ToUInt32Buff_m(newZipParams->tsSpan, timeseriesSpan);

    if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "BOOL")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[1];
                uint8_t sValue = (uint8_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                s[0] = sValue;
                memcpy(standardValue + valuePos, s, 1);

                //最大值
                char ma[1];
                uint8_t maValue = (uint8_t)atoi(const_cast<char *>(maxVec[t].c_str()));
                ma[0] = maValue;
                memcpy(maxValue + valuePos, ma, 1);

                //最小值
                char mi[1];
                uint8_t miValue = (uint8_t)atoi(const_cast<char *>(minVec[t].c_str()));
                mi[0] = miValue;
                memcpy(minValue + valuePos, mi, 1);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "USINT")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[1];
                uint8_t sValue = (uint8_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                s[0] = sValue;
                memcpy(standardValue + valuePos, s, 1);

                //最大值
                char ma[1];
                uint8_t maValue = (uint8_t)atoi(const_cast<char *>(maxVec[t].c_str()));
                ma[0] = maValue;
                memcpy(maxValue + valuePos, ma, 1);

                //最小值
                char mi[1];
                uint8_t miValue = (uint8_t)atoi(const_cast<char *>(minVec[t].c_str()));
                mi[0] = miValue;
                memcpy(minValue + valuePos, mi, 1);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "UINT")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[2];
                ushort sValue = (ushort)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToUInt16Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 2);

                //最大值
                char ma[2];
                ushort maValue = (ushort)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToUInt16Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 2);

                //最小值
                char mi[2];
                ushort miValue = (ushort)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToUInt16Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 2);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "UDINT")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[4];
                uint32_t sValue = (uint32_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToUInt32Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 4);

                //最大值
                char ma[4];
                ushort maValue = (ushort)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToUInt32Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 4);

                //最小值
                char mi[4];
                ushort miValue = (ushort)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToUInt32Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 4);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "SINT")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[1];
                int8_t sValue = (int8_t)atoi(const_cast<char *>(standardVec[t].c_str()));
                s[0] = sValue;
                memcpy(standardValue + valuePos, s, 1);

                //最大值
                char ma[1];
                int8_t maValue = (int8_t)atoi(const_cast<char *>(maxVec[t].c_str()));
                ma[0] = maValue;
                memcpy(maxValue + valuePos, ma, 1);

                //最小值
                char mi[1];
                int8_t miValue = (int8_t)atoi(const_cast<char *>(minVec[t].c_str()));
                mi[0] = miValue;
                memcpy(minValue + valuePos, mi, 1);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "INT")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[2];
                short sValue = (short)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToInt16Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 2);

                //最大值
                char ma[2];
                short maValue = (short)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToInt16Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 2);

                //最小值
                char mi[2];
                short miValue = (short)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToInt16Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 2);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "DINT")
    {
        if (newZipParams->isArrary == 0)
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
        else
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[4];
                int sValue = (int)atoi(const_cast<char *>(standardVec[t].c_str()));
                converter.ToInt32Buff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 4);

                //最大值
                char ma[4];
                int maValue = (int)atoi(const_cast<char *>(maxVec[t].c_str()));
                converter.ToInt32Buff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 4);

                //最小值
                char mi[4];
                int miValue = (int)atoi(const_cast<char *>(minVec[t].c_str()));
                converter.ToInt32Buff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 4);

                valuePos += 5;
            }
        }
    }
    else if (DataType::JudgeValueTypeByNum(newZipParams->valueType) == "REAL")
    {
        if (newZipParams->isArrary == 0)
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
        {
            //用空格分割字符串以获得每个数组元素的值
            vector<string> standardVec = DataType::StringSplit(const_cast<char *>(stringStandardValue.c_str()), " ");
            vector<string> maxVec = DataType::StringSplit(const_cast<char *>(stringMaxValue.c_str()), " ");
            vector<string> minVec = DataType::StringSplit(const_cast<char *>(stringMinValue.c_str()), " ");
            int valuePos = 0;
            for (int t = 0; t < newZipParams->arrayLen; t++)
            {
                //标准值
                char s[4];
                float sValue = (float)atof(const_cast<char *>(standardVec[t].c_str()));
                converter.ToFloatBuff_m(sValue, s);
                memcpy(standardValue + valuePos, s, 4);

                //最大值
                char ma[4];
                float maValue = (float)atof(const_cast<char *>(maxVec[t].c_str()));
                converter.ToFloatBuff_m(maValue, ma);
                memcpy(maxValue + valuePos, ma, 4);

                //最小值
                char mi[4];
                float miValue = (float)atof(const_cast<char *>(minVec[t].c_str()));
                converter.ToFloatBuff_m(miValue, mi);
                memcpy(minValue + valuePos, mi, 4);

                valuePos += 5;
            }
        }
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

    char *writebuf = new char[len + newZipParams->arrayLen * 15];
    //将所有参数传入writeBuf中，已覆盖写的方式写入已有模板文件中
    memcpy(writebuf, readBuf, writefront_len);
    memcpy(writebuf + writefront_len, valueNmae, 30);
    memcpy(writebuf + writefront_len + 30, valueType, 30);
    if (newZipParams->isArrary == 0)
    {
        memcpy(writebuf + writefront_len + 60, standardValue, 5);
        memcpy(writebuf + writefront_len + 65, maxValue, 5);
        memcpy(writebuf + writefront_len + 70, minValue, 5);
        memcpy(writebuf + writefront_len + 75, hasTime, 1);
        memcpy(writebuf + writefront_len + 76, timeseriesSpan, 4);
        memcpy(writebuf + writefront_len + 80, readBuf + writeafter_pos, writeafter_len);
    }
    else
    {
        memcpy(writebuf + writefront_len + 60, standardValue, 5 * newZipParams->arrayLen);
        memcpy(writebuf + writefront_len + 60 + 5 * newZipParams->arrayLen, maxValue, 5 * newZipParams->arrayLen);
        memcpy(writebuf + writefront_len + 60 + 10 * newZipParams->arrayLen, minValue, 5 * newZipParams->arrayLen);
        memcpy(writebuf + writefront_len + 60 + 15 * newZipParams->arrayLen, hasTime, 1);
        memcpy(writebuf + writefront_len + 61 + 15 * newZipParams->arrayLen, timeseriesSpan, 4);
        memcpy(writebuf + writefront_len + 65 + 15 * newZipParams->arrayLen, readBuf + writeafter_pos, writeafter_len);
    }

    long finalLength = 0;
    if(newZipParams->isArrary==1)
        finalLength = writefront_len + writeafter_len + 65 + 15 * newZipParams->arrayLen;
    else
        finalLength = writefront_len + writeafter_len + 80;

    //创建一个新的.ziptem文件，根据当前已存在的压缩模板数量进行编号
    long fp;
    if (strcmp(ZipParams->pathToLine, newZipParams->pathToLine) != 0)
    {
        //检查新文件夹名是否包含数字
        string checkPath = newZipParams->pathToLine;
        for (int i = 0; i < checkPath.length(); i++)
        {
            if (isdigit(checkPath[i]))
            {
                cout << "新文件夹名称包含数字！" << endl;
                return StatusCode::DIR_INCLUDE_NUMBER;
            }
        }

        //会创建一个新的文件夹存放新的模板
        temPath = newZipParams->pathToLine;

        char finalPath[100];
        strcpy(finalPath, settings("Filename_Label").c_str());
        strcat(finalPath, "/");
        strcat(finalPath, const_cast<char *>(temPath.c_str()));
        mkdir(finalPath, 0777);

        temPath.append("/").append(newZipParams->pathToLine).append(".ziptem");
    }

    char mode[3] = {'w', 'b', '+'};
    err = DB_Open(const_cast<char *>(temPath.c_str()), mode, &fp);
    if (err == 0)
    {
        err = DB_Write(fp, writebuf, finalLength);

        if (err == 0)
        {
            err = DB_Close(fp);
        }
    }
    if (s_new == 1)
        delete[] newZipParams->standardValue;
    if (max_new == 1)
        delete[] newZipParams->maxValue;
    if (min_new == 1)
        delete[] newZipParams->minValue;
    delete[] writebuf;
    return err;
}

//新创建一个.ziptem文件，根据已存在压缩模板的数量进行编号
int DB_DeleteNodeToZipSchema_MultiZiptem(struct DB_ZipNodeParams *ZipParams)
{
    int err;
    if (ZipParams->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;

    if (ZipParams->newPath == NULL)
        ZipParams->newPath = ZipParams->pathToLine;

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

    err = DB_LoadZipSchema(ZipParams->pathToLine);
    if (err != 0)
        return err;

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    long pos = -1;                                               //记录删除节点在第几条
    for (long i = 0; i < CurrentZipTemplate.schemas.size(); i++) //寻找是否有相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            pos = i; //记录这条记录的位置
            break;
        }
        if (CurrentZipTemplate.schemas[i].second.isArray)
            readbuf_pos += 65 + 3 * 5 * CurrentZipTemplate.schemas[i].second.arrayLen;
        else
            readbuf_pos += 80;
    }
    if (pos == -1)
    {
        cout << "未找到要删除的记录！" << endl;
        return StatusCode::UNKNOWN_VARIABLE_NAME;
    }

    char writeBuf[len - 80];
    //拷贝要被删除的记录之前的记录
    memcpy(writeBuf, readBuf, readbuf_pos);
    //拷贝要被删除的记录之后的记录
    if (CurrentZipTemplate.schemas[pos].second.isArray)
        memcpy(writeBuf + readbuf_pos, readBuf + readbuf_pos + 65 + 15 * CurrentZipTemplate.schemas[pos].second.arrayLen, len - readbuf_pos - 65 - 15 * CurrentZipTemplate.schemas[pos].second.arrayLen);
    else
        memcpy(writeBuf + readbuf_pos, readBuf + readbuf_pos + 80, len - readbuf_pos - 80);

    //创建新的.ziptem文件，根据当前已存在的压缩模板数量进行编号
    long fp;
    vector<string> ziptemFiles;
    readZIPTEMFilesList(ZipParams->pathToLine, ziptemFiles);
    int ziptemNum = ziptemFiles.size() + 1;
    char appendNum[4];
    sprintf(appendNum, "%d", ziptemNum);
    temPath = ZipParams->pathToLine;
    temPath.append("/").append(ZipParams->pathToLine).append(appendNum).append(".ziptem");
    char mode[3] = {'w', 'b', '+'};
    err = DB_Open(const_cast<char *>(temPath.c_str()), mode, &fp);
    if (err == 0)
    {
        if (CurrentZipTemplate.schemas[pos].second.isArray)
        {
            if ((len - 65 - CurrentZipTemplate.schemas[pos].second.arrayLen * 15) != 0)
                err = DB_Write(fp, writeBuf, len - 65 - CurrentZipTemplate.schemas[pos].second.arrayLen * 15);
        }
        else
        {
            if ((len - 80) != 0)
                err = DB_Write(fp, writeBuf, len - 80);
        }

        if (err == 0)
            err = DB_Close(fp);
    }
    return err;
}

/**
 * @brief 删除压缩模板节点，根据变量名进行定位，可以指定新的文件夹存放修改后的模板
 *
 * @param ZipParams 压缩模板参数
 * @return 0:success,　
 *         others: StatusCode
 * @note 新文件夹名称里面不能包含数字
 */
int DB_DeleteNodeToZipSchema(struct DB_ZipNodeParams *ZipParams)
{
    int err;

    if (ZipParams->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;

    if (ZipParams->newPath == NULL)
        ZipParams->newPath = ZipParams->pathToLine;

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

    err = DB_LoadZipSchema(ZipParams->pathToLine);
    if (err != 0)
        return err;

    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    long pos = -1;                                               //记录删除节点在第几条
    for (long i = 0; i < CurrentZipTemplate.schemas.size(); i++) //寻找是否有相同的变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(ZipParams->valueName, existValueName) == 0)
        {
            pos = i; //记录这条记录的位置
            break;
        }
        if (CurrentZipTemplate.schemas[i].second.isArray)
            readbuf_pos += 65 + 3 * 5 * CurrentZipTemplate.schemas[i].second.arrayLen;
        else
            readbuf_pos += 80;
    }
    if (pos == -1)
    {
        cout << "未找到要删除的记录！" << endl;
        return StatusCode::UNKNOWN_VARIABLE_NAME;
    }

    char writeBuf[len - 80];
    //拷贝要被删除的记录之前的记录
    memcpy(writeBuf, readBuf, readbuf_pos);
    //拷贝要被删除的记录之后的记录
    if (CurrentZipTemplate.schemas[pos].second.isArray)
        memcpy(writeBuf + readbuf_pos, readBuf + readbuf_pos + 65 + 15 * CurrentZipTemplate.schemas[pos].second.arrayLen, len - readbuf_pos - 65 - 15 * CurrentZipTemplate.schemas[pos].second.arrayLen);
    else
        memcpy(writeBuf + readbuf_pos, readBuf + readbuf_pos + 80, len - readbuf_pos - 80);

    //创建新的文件夹存放修改后的压缩模板
    long fp;
    if (strcmp(ZipParams->pathToLine, ZipParams->newPath) != 0)
    {
        //检查新文件夹名是否包含数字
        string checkPath = ZipParams->newPath;
        for (int i = 0; i < checkPath.length(); i++)
        {
            if (isdigit(checkPath[i]))
            {
                cout << "新文件夹名称包含数字！" << endl;
                return StatusCode::DIR_INCLUDE_NUMBER;
            }
        }

        //创建一个新的文件夹存放新的模板
        temPath = ZipParams->newPath;

        char finalPath[100];
        strcpy(finalPath, settings("Filename_Label").c_str());
        strcat(finalPath, "/");
        strcat(finalPath, const_cast<char *>(temPath.c_str()));
        mkdir(finalPath, 0777);

        temPath.append("/").append(ZipParams->newPath).append(".ziptem");
    }
    char mode[3] = {'w', 'b', '+'};
    err = DB_Open(const_cast<char *>(temPath.c_str()), mode, &fp);
    if (err == 0)
    {
        if (CurrentZipTemplate.schemas[pos].second.isArray)
        {
            if ((len - 65 - CurrentZipTemplate.schemas[pos].second.arrayLen * 15) != 0)
                err = DB_Write(fp, writeBuf, len - 65 - CurrentZipTemplate.schemas[pos].second.arrayLen * 15);
        }
        else
        {
            if ((len - 80) != 0)
                err = DB_Write(fp, writeBuf, len - 80);
        }
        if (err == 0)
            err = DB_Close(fp);
    }
    return err;
}

/**
 * @brief 查询模板参数
 *
 * @param QueryParams 查询参数
 * @return int
 */
int DB_QueryNode(struct DB_QueryNodeParams *QueryParams)
{
    //检查产线路径和变量名是否合法
    int err = checkQueryNodeParam(QueryParams);
    if (err != 0)
        return err;

    //寻找模板文件
    vector<string> files;
    readFileList(QueryParams->pathToLine, files);
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

    //读取模板文件
    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    err = DB_LoadSchema(QueryParams->pathToLine);
    if (err != 0)
        return err;

    long pos = -1;                      //用于定位是修改模板的第几条
    for (long i = 0; i < len / 71; i++) //寻找是否有相同变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(QueryParams->valueName, existValueName) == 0)
        {
            pos = i; //记录这条记录的位置
            break;
        }
        readbuf_pos += 71;
    }
    if (pos == -1)
    {
        cout << "未找到变量名！" << endl;
        return StatusCode::UNKNOWN_VARIABLE_NAME;
    }

    //将节点信息赋值到查询参数中
    QueryParams->isArrary = CurrentTemplate.schemas[pos].second.isArray;
    if (CurrentTemplate.schemas[pos].second.isArray)
        QueryParams->arrayLen = CurrentTemplate.schemas[pos].second.arrayLen;
    else
        QueryParams->arrayLen = 0;
    QueryParams->isTS = CurrentTemplate.schemas[pos].second.isTimeseries;
    if (CurrentTemplate.schemas[pos].second.isTimeseries)
        QueryParams->arrayLen = CurrentTemplate.schemas[pos].second.tsLen;
    else
        QueryParams->tsLen = 0;
    QueryParams->hasTime = CurrentTemplate.schemas[pos].second.hasTime;
    QueryParams->valueType = CurrentTemplate.schemas[pos].second.valueType;
    char *pathcode = (char *)malloc(sizeof(char) * 10);
    memcpy(pathcode, readBuf + pos * 71 + 61, 10);
    QueryParams->pathcode = pathcode;
    return 0;
}

/**
 * @brief 查询压缩模板信息
 *
 * @param QueryParams 查询参数
 * @return int
 */
int DB_QueryZipNode(struct DB_QueryNodeParams *QueryParams)
{
    //检查产线路径和变量名是否合法
    int err = checkQueryNodeParam(QueryParams);
    if (err != 0)
        return err;

    //寻找模板文件
    vector<string> files;
    readFileList(QueryParams->pathToLine, files);
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

    err = DB_LoadZipSchema(QueryParams->pathToLine);
    if (err != 0)
        return err;

    //读取模板文件
    long len;
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &len);
    char readBuf[len];
    long readbuf_pos = 0;
    DB_OpenAndRead(const_cast<char *>(temPath.c_str()), readBuf);

    long pos = -1;                                               //用于定位是修改模板的第几条
    for (long i = 0; i < CurrentZipTemplate.schemas.size(); i++) //寻找是否有相同变量名
    {
        char existValueName[30];
        memcpy(existValueName, readBuf + readbuf_pos, 30);
        if (strcmp(QueryParams->valueName, existValueName) == 0)
        {
            pos = i; //记录这条记录的位置
            break;
        }

        if (CurrentZipTemplate.schemas[i].second.isArray)
            readbuf_pos += 65 + 3 * 5 * CurrentZipTemplate.schemas[i].second.arrayLen;
        else
            readbuf_pos += 80;
    }
    if (pos == -1)
    {
        cout << "未找到变量名！" << endl;
        return StatusCode::UNKNOWN_VARIABLE_NAME;
    }

    //将节点信息赋值到查询参数中
    QueryParams->isArrary = CurrentZipTemplate.schemas[pos].second.isArray;
    if (CurrentZipTemplate.schemas[pos].second.isArray)
        QueryParams->arrayLen = CurrentZipTemplate.schemas[pos].second.arrayLen;
    else
        QueryParams->arrayLen = 0;
    QueryParams->isTS = CurrentZipTemplate.schemas[pos].second.isTimeseries;
    if (CurrentZipTemplate.schemas[pos].second.isTimeseries)
        QueryParams->tsLen = CurrentZipTemplate.schemas[pos].second.tsLen;
    else
        QueryParams->tsLen = 0;
    QueryParams->tsSpan = CurrentZipTemplate.schemas[pos].second.timeseriesSpan;
    QueryParams->hasTime = CurrentZipTemplate.schemas[pos].second.hasTime;
    QueryParams->valueType = CurrentZipTemplate.schemas[pos].second.valueType;

    if (CurrentZipTemplate.schemas[pos].second.isArray)
    {
        char *standValue = (char *)malloc(sizeof(char) * 5 * CurrentZipTemplate.schemas[pos].second.arrayLen);
        memcpy(standValue, readBuf + readbuf_pos + 60, 5 * CurrentZipTemplate.schemas[pos].second.arrayLen);
        QueryParams->standardValue = standValue;
        char *maxValue = (char *)malloc(sizeof(char) * 5 * CurrentZipTemplate.schemas[pos].second.arrayLen);
        memcpy(maxValue, readBuf + readbuf_pos + 60 + 5 * CurrentZipTemplate.schemas[pos].second.arrayLen, 5 * CurrentZipTemplate.schemas[pos].second.arrayLen);
        QueryParams->maxValue = maxValue;
        char *minValue = (char *)malloc(sizeof(char) * 5 * CurrentZipTemplate.schemas[pos].second.arrayLen);
        memcpy(minValue, readBuf + readbuf_pos + 60 + 10 * CurrentZipTemplate.schemas[pos].second.arrayLen, 5 * CurrentZipTemplate.schemas[pos].second.arrayLen);
        QueryParams->minValue = minValue;
    }
    else
    {
        char *standValue = (char *)malloc(sizeof(char) * 5);
        memcpy(standValue, readBuf + readbuf_pos + 60, 5);
        QueryParams->standardValue = standValue;
        char *maxValue = (char *)malloc(sizeof(char) * 5);
        memcpy(maxValue, readBuf + readbuf_pos + 65, 5);
        QueryParams->maxValue = maxValue;
        char *minValue = (char *)malloc(sizeof(char) * 5);
        memcpy(minValue, readBuf + readbuf_pos + 70, 5);
        QueryParams->minValue = minValue;
    }
    return 0;
}

// int main()
// {
//     DB_TreeNodeParams treeParam;
//     treeParam.pathToLine = "RbTsImgTest";
//     // treeParam.valueName = "TestOO";
//     // treeParam.valueType = 3;
//     char code[10];
//     code[0] = (char)0;
//     code[1] = (char)1;
//     code[2] = (char)0;
//     code[3] = (char)1;
//     code[4] = 0;
//     code[5] = (char)0;
//     code[6] = 0;
//     code[7] = (char)0;
//     code[8] = (char)0;
//     code[9] = (char)0;
//     treeParam.pathCode = code;
//     // treeParam.isArrary = 0;
//     // treeParam.arrayLen = 6;
//     // treeParam.isTS = 1;
//     // treeParam.tsLen = 5;
//     // treeParam.hasTime = 1;
//     // treeParam.newPath = "RbTsImgTest";
//     DB_TreeNodeParams newParam;
//     newParam.pathToLine = NULL;
//     newParam.valueName = NULL;
//     newParam.valueType = 3;
//     code[0] = (char)0;
//     code[1] = (char)1;
//     code[2] = (char)0;
//     code[3] = (char)1;
//     code[4] = 0;
//     code[5] = (char)0;
//     code[6] = 0;
//     code[7] = (char)0;
//     code[8] = (char)0;
//     code[9] = (char)0;
//     newParam.pathCode = NULL;
//     newParam.isArrary = 1;
//     newParam.arrayLen = 6;
//     newParam.isTS = 0;
//     newParam.tsLen = 5;
//     newParam.hasTime = 1;
//     newParam.newPath = NULL;
//     // DB_AddNodeToSchema(&treeParam);
//     DB_UpdateNodeToSchema(&treeParam, &newParam);
//     // DB_DeleteNodeToSchema(&treeParam);
//     return 0;
// }

// int main()
// {
//     DB_ZipNodeParams param;
//     param.pathToLine = "arrTest";
//     param.valueName = "DB1";
//     param.valueType = 8;
//     param.newPath = "newArrTest";
//     param.hasTime = 1;
//     param.isTS = 1;
//     param.tsLen = 10;
//     param.tsSpan = 1000;
//     param.isArrary = 1;
//     param.arrayLen = 5;
//     param.standardValue = "-100.5 -200.5 -300.5 -400.5 -500.5";
//     param.maxValue = "-600.5 -700.5 -800.5 -900.5 -1000.5";
//     param.minValue = "-1100.5 -1200.5 -1300.5 -1400.5 -1500.5";
//     // DB_AddNodeToZipSchema(&param);
//     // DB_DeleteNodeToZipSchema(&param);
//     DB_ZipNodeParams newParam;
//     newParam.newPath = "newTest";
//     newParam.pathToLine = "newArrTest";
//     newParam.valueName = NULL;
//     newParam.valueType = 3;
//     newParam.isTS = 0;
//     newParam.tsLen = 6;
//     newParam.isArrary = 0;
//     newParam.arrayLen = 10;
//     newParam.maxValue = "150";
//     newParam.minValue = "90";
//     newParam.standardValue = "110";
//     newParam.tsSpan = 20000;
//     newParam.hasTime = 0;
//     DB_UpdateNodeToZipSchema(&param, &newParam);
//     return 0;
// }
// int main()
// {
//     DB_QueryNodeParams params;
//     params.valueName = "DB2";
//     params.pathToLine = "arrTest";
//     DB_QueryZipNode(&params);
//     cout << params.valueName << endl;
//     // char pathcode[10];
//     // memcpy(pathcode, params.pathcode, 10);
//     // for (int i = 0; i < 10; i++)
//     //     cout << pathcode[i] << " ";
//     // cout << endl;
//     char stand[30];
//     memcpy(stand, params.standardValue, 30);
//     for (int i = 0; i < 30; i++)
//         cout << stand[i] << " ";
//     cout << endl;
//     char max[30];
//     memcpy(max, params.maxValue, 30);
//     for (int i = 0; i < 30; i++)
//         cout << max[i] << " ";
//     cout << endl;
//     char min[30];
//     memcpy(min, params.minValue, 30);
//     for (int i = 0; i < 30; i++)
//         cout << min[i] << " ";
//     cout << endl;
//     cout << params.valueType << endl;
//     cout << params.isArrary << endl;
//     cout << params.arrayLen << endl;
//     cout << params.isTS << endl;
//     cout << params.tsLen << endl;
//     cout << params.hasTime << endl;
//     cout << params.tsSpan << endl;
//     //free(params.pathcode);
//     free(params.maxValue);
//     free(params.minValue);
//     free(params.standardValue);
//     return 0;
// }
