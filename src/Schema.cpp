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
 * @note   新节点参数必须齐全，pathToLine pathcode valueNmae hasTime valueType isArray arrayLen
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
 * @brief 修改标准模板里的树节点
 *
 * @param params 需要修改的节点
 * @param newTreeParams 需要修改的信息
 * @return　0:success,
 *         others: StatusCode
 * @note   更新节点参数必须齐全，pathToLine pathcode valueNmae hasTime valueType isArray arrayLen
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
 * @brief 向压缩模板添加新节点
 *
 * @param params 压缩模板参数
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

    char valueNmae[30], valueType[30], standardValue[10], maxValue[10], minValue[10], hasTime[1]; //先初始化为0
    memset(valueNmae, 0, sizeof(valueNmae));
    memset(valueType, 0, sizeof(valueType));
    memset(standardValue, 0, sizeof(standardValue));
    memset(maxValue, 0, sizeof(maxValue));
    memset(minValue, 0, sizeof(minValue));

    if (ZipParams->isArrary == 1) //是数组类型,拼接字符串
    {
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

/**
 * @brief 修改压缩模板里的树节点
 *
 * @param params 需要修改的节点
 * @param newTreeParams 需要修改的信息
 * @return　0:success,
 *         others: StatusCode
 * @note   更新节点参数必须齐全，pathToLine valueNmae hasTime valueType isArray arrayLen standardValue maxValue minValue
 */
int DB_UpdateNodeToZipSchema(struct DB_ZipNodeParams *ZipParams, struct DB_ZipNodeParams *newZipParams)
{
    return 0;
}

/**
 * @brief 删除压缩模板节点，根据变量名进行定位
 *
 * @param TreeParams 压缩模板参数
 * @return 0:success,　
 *         others: StatusCode
 */
int DB_DeleteNodeToSchema(struct DB_TreeNodeParams *TreeParams)
{
    return 0;
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
        char variable[30], dataType[30], standardValue[10], maxValue[10], minValue[10], hasTime[1];
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
        memcpy(hasTime, buf + i, 1);
        i += 1;
        vector<string> paths;

        dataName.push_back(variable);
        DataType type;

        memcpy(type.standardValue, standardValue, 10);
        memcpy(type.maxValue, maxValue, 10);
        memcpy(type.minValue, minValue, 10);
        type.hasTime = (bool)hasTime[0];
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
    //     DB_LoadZipSchema("jinfei/");

    //     // DB_TreeNodeParams params;
    //     // params.pathToLine = "/";
    //     // char code[10];
    //     // code[0] = (char)0;
    //     // code[1] = (char)1;
    //     // code[2] = (char)0;
    //     // code[3] = (char)4;
    //     // code[4] = 'R';
    //     // code[5] = (char)1;
    //     // code[6] = 0;
    //     // code[7] = (char)0;
    //     // code[8] = (char)0;
    //     // code[9] = (char)0;
    //     // params.pathCode = code;
    //     // params.valueType = 3;
    //     // params.hasTime = 0;
    //     // params.isArrary = 0;
    //     // params.arrayLen = 100;
    //     // params.valueName = "S4ON";

    //     // DB_TreeNodeParams newTreeParams;
    //     // newTreeParams.pathToLine = "/";
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
    //     // newTreeParams.hasTime = 0;
    //     // newTreeParams.isArrary = 0;
    //     // newTreeParams.arrayLen = 100;
    //     // newTreeParams.valueName = "S4ON";
    //     // DB_UpdateNodeToSchema(&params,&newTreeParams);
    //     // DB_AddNodeToSchema(&params);
    //     // DB_DeleteNodeToSchema(&params);

    DB_ZipNodeParams params;
    params.pathToLine = "/";
    params.valueType = 2;
    params.hasTime = 1;
    params.isArrary = 0;
    params.arrayLen = 100;
    params.valueName = "S4OFF";
    params.standardValue = "210";
    params.maxValue = "230";
    params.minValue = "190";
    DB_AddNodeToZipSchema(&params);
    return 0;
}
