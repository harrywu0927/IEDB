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

/**
 * @brief 压缩已有的.idb文件,所有数据类型都支持
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idb文件路径
 * @return  0:success,
 *          other:StatusCode
 */
int DB_ZipFile(const char *ZipTemPath, const char *pathToLine)
{
    int err = 0;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;
    vector<string> files;
    readIDBFilesList(pathToLine, files);
    if (files.size() == 0)
    {
        cout << "没有文件！" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    for (size_t fileNum = 0; fileNum < files.size(); fileNum++) //循环压缩文件夹下的所有.idb文件
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(files[fileNum].c_str()), &len);
        char readbuff[len];                                                                    //文件内容
        char writebuff[CurrentZipTemplate.totalBytes + 2 * CurrentZipTemplate.schemas.size()]; //写入没有被压缩的数据
        if (DB_OpenAndRead(const_cast<char *>(files[fileNum].c_str()), readbuff))              //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        long readbuff_pos = 0;
        long writebuff_pos = 0;

        for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++) //循环比较
        {
            if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::BOOL) // BOOL变量
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                    {
                        memcpy(writebuff+writebuff_pos,readbuff+readbuff_pos,CurrentZipTemplate.schemas[i].second.arrayLen+8);
                        writebuff_pos +=  CurrentZipTemplate.schemas[i].second.arrayLen+8;
                        readbuff_pos +=  CurrentZipTemplate.schemas[i].second.arrayLen+8;
                    }
                    else
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos +=  CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos +=  CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
                }
                else
                {
                    char standardBool = CurrentZipTemplate.schemas[i].second.standardValue[0];
                    // 1个字节,暂定，根据后续情况可能进行更改
                    char value[1] = {0};
                    memcpy(value, readbuff + readbuff_pos, 1);
                    char currentUSintValue = value[0];
                    if (standardBool != readbuff[readbuff_pos])
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;
                        if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 9);
                            writebuff_pos+=9;
                        }
                        else
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                            writebuff_pos += 1;
                        }
                    }
                    CurrentZipTemplate.schemas[i].second.hasTime ?  readbuff_pos+=9:readbuff_pos+=1;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT变量
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos,  CurrentZipTemplate.schemas[i].second.arrayLen+8);
                        writebuff_pos +=  CurrentZipTemplate.schemas[i].second.arrayLen+8;
                        readbuff_pos +=  CurrentZipTemplate.schemas[i].second.arrayLen+8;
                    }
                    else
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos,  CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos +=  CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos +=  CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                    if (currentUSintValue != standardUSintValue && (currentUSintValue < maxUSintValue || currentUSintValue > minUSintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;
                        if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 9);
                            writebuff_pos += 9;
                        }
                        else
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                            writebuff_pos += 1;
                        }
                    }
                    CurrentZipTemplate.schemas[i].second.hasTime ? readbuff_pos+=9: readbuff_pos += 1;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT变量
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen+8);
                        writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen+8;
                        readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen+8;
                    }
                    else
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                    if (currentUintValue != standardUintValue && (currentUintValue < maxUintValue || currentUintValue > minUintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;
                        if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 10);
                            writebuff_pos += 10;
                        }
                        else
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                            writebuff_pos += 2;
                        }        
                    }
                    CurrentZipTemplate.schemas[i].second.hasTime ? readbuff_pos+=10 : readbuff_pos += 2;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT变量
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen+8);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen+8;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen+8;
                    }
                    else
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                    if (currentUDintValue != standardUDintValue && (currentUDintValue < maxUDintValue || currentUDintValue > minUDintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;
                        if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                            writebuff_pos += 12;
                        }
                        else
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                            writebuff_pos += 4;
                        }
                    }
                    CurrentZipTemplate.schemas[i].second.hasTime ? readbuff_pos+=12 : readbuff_pos += 4;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT变量
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen+8);
                        writebuff_pos +=  CurrentZipTemplate.schemas[i].second.arrayLen+8;
                        readbuff_pos +=  CurrentZipTemplate.schemas[i].second.arrayLen+8;
                    }
                    else
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos,  CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos +=  CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos +=  CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                    if (currentSintValue != standardSintValue && (currentSintValue < maxSintValue || currentSintValue > minSintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;
                        if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 9);
                            writebuff_pos += 9;
                        }
                        else
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                            writebuff_pos += 1;
                        }
                    }
                    CurrentZipTemplate.schemas[i].second.hasTime ? readbuff_pos+=9 : readbuff_pos += 1;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT变量
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen+8);
                        writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen+8;
                        readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen+8;
                    }
                    else
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                    if (currentIntValue != standardIntValue && (currentIntValue < maxIntValue || currentIntValue > minIntValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;
                        if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 10);
                            writebuff_pos += 10;
                        }
                        else
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                            writebuff_pos += 2;
                        }
                    }
                    CurrentZipTemplate.schemas[i].second.hasTime ? readbuff_pos+=10 : readbuff_pos += 2;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT变量
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen+8);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen+8;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen+8;
                    }
                    else
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                    if (currentDintValue != standardDintValue && (currentDintValue < maxDintValue || currentDintValue > minDintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;
                        if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                            writebuff_pos += 12;
                        }
                        else
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                            writebuff_pos += 4;
                        }
                    }
                    CurrentZipTemplate.schemas[i].second.hasTime ? readbuff_pos+=12 : readbuff_pos += 4;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL变量
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen+8);
                        writebuff_pos += 4* CurrentZipTemplate.schemas[i].second.arrayLen+8;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen+8;
                    }
                    else
                    {
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                        writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                        readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                    }
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
                    if (currentRealValue != standardFloatValue && (currentRealValue < maxFloatValue || currentRealValue > minFloatValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;
                        if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 12);
                            writebuff_pos += 12;
                        }
                        else
                        {
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                            writebuff_pos += 4;
                        }
                    }
                    CurrentZipTemplate.schemas[i].second.hasTime ? readbuff_pos+=12 : readbuff_pos += 4;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::IMAGE)
            {

                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;
                //暂定２个字节的图片长度
                char length[2] = {0};
                memcpy(length, readbuff + readbuff_pos, 2);
                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2); //图片长度也存
                writebuff_pos += 2;

                uint16_t imageLength = converter.ToUInt16(length);
                readbuff_pos += 2;
                if(CurrentZipTemplate.schemas[i].second.hasTime==true)//带有时间戳
                {
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, imageLength+8);
                    writebuff_pos += imageLength+8;
                    readbuff_pos += imageLength+8;
                }
                else
                {
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, imageLength);
                    writebuff_pos += imageLength;
                    readbuff_pos += imageLength;
                }
            }
        }

        if (writebuff_pos >= readbuff_pos) //表明数据没有被压缩,保持原文件
        {
            cout << files[fileNum] + "文件数据没有被压缩!" << endl;
            // return 1;//1表示数据没有被压缩
        }
        else
        {
            DB_DeleteFile(const_cast<char *>(files[fileNum].c_str())); //删除原文件
            long fp;
            string finalpath = files[fileNum].append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
            //创建新文件并写入
            err = DB_Open(const_cast<char *>(finalpath.c_str()), "wb", &fp);
            if (err == 0)
            {
                if (writebuff_pos != 0)
                    err = DB_Write(fp, writebuff, writebuff_pos);

                if (err == 0)
                {
                    DB_Close(fp);
                }
            }
        }
    }
    return err;
}

/**
 * @brief 还原压缩后的.idb文件为原状态
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idbzip压缩文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ReZipFile(const char *ZipTemPath, const char *pathToLine)
{
    int err = 0;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;
    vector<string> files;

    readIDBZIPFilesList(pathToLine, files);
    if (files.size() == 0)
    {
        cout << "没有.idbzip文件!" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    for (size_t fileNum = 0; fileNum < files.size(); fileNum++)
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(files[fileNum].c_str()), &len);
        char readbuff[len];                            //文件内容
        char writebuff[CurrentZipTemplate.totalBytes]; //写入没有被压缩的数据
        long readbuff_pos = 0;
        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(files[fileNum].c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                                writebuff_pos += 1;
                                readbuff_pos += 1;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                                writebuff_pos += 1;
                                readbuff_pos += 1;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                                writebuff_pos += 2;
                                readbuff_pos += 2;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                                writebuff_pos += 1;
                                readbuff_pos += 1;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                                writebuff_pos += 2;
                                readbuff_pos += 2;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
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
                memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                uint16_t posCmp = converter.ToUInt16(zipPosNum);
                if (posCmp == i) //是未压缩数据的编号
                {
                    readbuff_pos += 2;
                    //暂定２个字节的图片长度
                    char length[2] = {0};
                    memcpy(length, readbuff + readbuff_pos, 2);
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2); //图片长度也存
                    writebuff_pos += 2;
                    uint16_t imageLength = converter.ToUInt16(length);
                    readbuff_pos += 2;
                    //存储图片
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, imageLength);
                    writebuff_pos += imageLength;
                    readbuff_pos += imageLength;
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

        // DB_DeleteFile(const_cast<char *>(files[fileNum].c_str()));//删除原文件
        long fp;
        string finalpath = files[fileNum].substr(0, files[fileNum].length() - 3); //去掉后缀的zip
        //创建新文件并写入
        err = DB_Open(const_cast<char *>(finalpath.c_str()), "wb", &fp);
        if (err == 0)
        {
            err = DB_Write(fp, writebuff, writebuff_pos);

            if (err == 0)
            {
                DB_Close(fp);
            }
        }
    }
    return err;
}

/**
 * @brief 压缩收到的一整条数据
 *
 * @param ZipTemPath 压缩模板路径
 * @param filepath 存储文件路径
 * @param buff 接收到的数据
 * @param buffLength 接收到的数据长度
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipRecvBuff(const char *ZipTemPath, const char *filepath, char *buff, long *buffLength)
{
    int err = 0;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    long len = *buffLength;

    char writebuff[CurrentZipTemplate.totalBytes + 2 * CurrentZipTemplate.schemas.size()]; //写入没有被压缩的数据

    DataTypeConverter converter;
    long buff_pos = 0;
    long writebuff_pos = 0;

    for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++) //循环比较
    {
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::BOOL) // BOOL变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                char standardBool = CurrentZipTemplate.schemas[i].second.standardValue[0];

                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, buff + buff_pos, 1);
                char currentUSintValue = value[0];
                if (standardBool != buff[buff_pos])
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 1);
                    writebuff_pos += 1;
                }
                buff_pos += 1;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                char standardUSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char maxUSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                char minUSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];

                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, buff + buff_pos, 1);
                char currentUSintValue = value[0];
                if (currentUSintValue != standardUSintValue && (currentUSintValue < maxUSintValue || currentUSintValue > minUSintValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 1);
                    writebuff_pos += 1;
                }
                buff_pos += 1;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;
                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                ushort standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                ushort maxUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                ushort minUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.minValue);

                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, buff + buff_pos, 2);
                ushort currentUintValue = converter.ToUInt16(value);
                if (currentUintValue != standardUintValue && (currentUintValue < maxUintValue || currentUintValue > minUintValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 2);
                    writebuff_pos += 2;
                }
                buff_pos += 2;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                uint32 maxUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                uint32 minUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, buff + buff_pos, 4);
                uint32 currentUDintValue = converter.ToUInt32(value);
                if (currentUDintValue != standardUDintValue && (currentUDintValue < maxUDintValue || currentUDintValue > minUDintValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 4);
                    writebuff_pos += 4;
                }
                buff_pos += 4;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;
                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                char standardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char maxSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                char minSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];

                // 2个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, buff + buff_pos, 1);
                char currentSintValue = value[0];
                if (currentSintValue != standardSintValue && (currentSintValue < maxSintValue || currentSintValue > minSintValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 1);
                    writebuff_pos += 1;
                }
                buff_pos += 1;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;
                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                short maxIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                short minIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.minValue);

                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, buff + buff_pos, 2);
                short currentIntValue = converter.ToInt16(value);
                if (currentIntValue != standardIntValue && (currentIntValue < maxIntValue || currentIntValue > minIntValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 2);
                    writebuff_pos += 2;
                }
                buff_pos += 2;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;
                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                int maxDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                int minDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, buff + buff_pos, 4);
                int currentDintValue = converter.ToInt32(value);
                if (currentDintValue != standardDintValue && (currentDintValue < maxDintValue || currentDintValue > minDintValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 4);
                    writebuff_pos += 4;
                }
                buff_pos += 4;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;
                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                float standardFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                float maxFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.maxValue);
                float minFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.minValue);

                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, buff + buff_pos, 4);
                int currentRealValue = converter.ToFloat(value);
                if (currentRealValue != standardFloatValue && (currentRealValue < maxFloatValue || currentRealValue > minFloatValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 4);
                    writebuff_pos += 4;
                }
                buff_pos += 4;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::IMAGE)
        {

            //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
            uint16_t posNum = i;
            char zipPosNum[2] = {0};
            for (char j = 0; j < 2; j++)
            {
                zipPosNum[1 - j] |= posNum;
                posNum >>= 8;
            }
            memcpy(writebuff + writebuff_pos, zipPosNum, 2);
            writebuff_pos += 2;

            //暂定２个字节的图片长度
            char length[2] = {0};
            memcpy(length, buff + buff_pos, 2);
            memcpy(writebuff + writebuff_pos, buff + buff_pos, 2); //图片长度也存
            writebuff_pos += 2;

            uint16_t imageLength = converter.ToUInt16(length);
            buff_pos += 2;
            memcpy(writebuff + writebuff_pos, buff + buff_pos, imageLength);
            writebuff_pos += imageLength;
            buff_pos += imageLength;
        }
    }

    if (writebuff_pos >= buff_pos) //表明数据没有被压缩
    {
        char isZip[1] = {0};
        char finnalBuf[len + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, buff, *buffLength);
        memcpy(buff, finnalBuf, len + 1);
        *buffLength += 1;
    }
    else if (writebuff_pos == 0) //数据完全压缩
    {
        char isZip[1] = {1};
        char finnalBuf[writebuff_pos + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, writebuff, writebuff_pos);
        memcpy(buff, finnalBuf, writebuff_pos + 1);
        *buffLength = writebuff_pos + 1;
    }
    else //数据未完全压缩
    {
        char isZip[1] = {2};
        char finnalBuf[writebuff_pos + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, writebuff, writebuff_pos);
        memcpy(buff, finnalBuf, writebuff_pos + 1);
        *buffLength = writebuff_pos + 1;
    }

    DB_DataBuffer databuff;
    databuff.length = *buffLength;
    databuff.buffer = buff;
    databuff.savePath = filepath;
    DB_InsertRecord(&databuff, 1);
    return err;
}

/**
 * @brief 压缩只有开关量类型的已有.idb文件
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idb文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipSwitchFile(const char *ZipTemPath, const char *pathToLine)
{
    int err;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;
    vector<string> files;
    readIDBFilesList(pathToLine, files);
    if (files.size() == 0)
    {
        cout << "没有文件！" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    for (size_t fileNum = 0; fileNum < files.size(); fileNum++) //循环以给每个.idb文件进行压缩处理
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(files[fileNum].c_str()), &len);
        char readbuff[len];                                                                    //文件内容
        char writebuff[CurrentZipTemplate.totalBytes + 2 * CurrentZipTemplate.schemas.size()]; //写入没有被压缩的数据
        long readbuff_pos = 0;
        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(files[fileNum].c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        for (int i = 0; i < CurrentZipTemplate.schemas.size(); i++)
        {
            if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) //开关量的持续时长
            {

                uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                uint32 maxBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                uint32 minBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

                // 4个字节的持续时长,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                uint32 currentBoolTime = converter.ToUInt32(value);
                if (currentBoolTime != standardBoolTime && (currentBoolTime < minBoolTime || currentBoolTime > maxBoolTime))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                    writebuff_pos += 4;
                }
                readbuff_pos += 4;
            }
            else
            {
                cout << "存在开关量以外的类型，请检查模板或者更换功能块" << endl;
                return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            }
        }
        if (writebuff_pos >= readbuff_pos) //表明数据没有被压缩,不做处理
        {
            cout << files[fileNum] + "文件数据没有被压缩!" << endl;
            // return 1;//1表示数据没有被压缩
        }
        else
        {
            DB_DeleteFile(const_cast<char *>(files[fileNum].c_str())); //删除原文件
            long fp;
            string finalpath = files[fileNum].append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
            //创建新文件并写入
            err = DB_Open(const_cast<char *>(finalpath.c_str()), "wb", &fp);
            if (err == 0)
            {
                if (writebuff_pos != 0)
                    err = DB_Write(fp, writebuff, writebuff_pos);

                if (err == 0)
                {
                    DB_Close(fp);
                }
            }
        }
    }
    return err;
}

/**
 * @brief 还原压缩后的开关量文件为原状态
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idbzip压缩文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ReZipSwitchFile(const char *ZipTemPath, const char *pathToLine)
{
    int err = 0;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;
    vector<string> files;

    readIDBZIPFilesList(pathToLine, files);
    if (files.size() == 0)
    {
        cout << "没有.idbzip文件!" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    for (size_t fileNum = 0; fileNum < files.size(); fileNum++)
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(files[fileNum].c_str()), &len);
        char readbuff[len];                            //文件内容
        char writebuff[CurrentZipTemplate.totalBytes]; //写入没有被压缩的数据
        long readbuff_pos = 0;
        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(files[fileNum].c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++)
        {
            if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) //开关量持续时长
            {
                if (len == 0) //表示文件完全压缩
                {
                    uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char boolTime[4] = {0};
                    for (int j = 0; j < 4; j++)
                    {
                        boolTime[3 - j] |= standardBoolTime;
                        standardBoolTime >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, boolTime, 4); //持续时长
                    writebuff_pos += 4;
                }
                else //文件未完全压缩
                {
                    if (readbuff_pos < len) //还有未压缩的数据
                    {
                        //对比编号是否等于当前模板所在条数
                        char zipPosNum[2] = {0};
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);

                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                            writebuff_pos += 4;
                            readbuff_pos += 4;
                        }
                        else //不是未压缩的编号
                        {
                            uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                            char boolTime[4] = {0};
                            for (int j = 0; j < 4; j++)
                            {
                                boolTime[3 - j] |= standardBoolTime;
                                standardBoolTime >>= 8;
                            }
                            memcpy(writebuff + writebuff_pos, boolTime, 4); //持续时长
                            writebuff_pos += 4;
                        }
                    }
                    else //没有未压缩的数据了
                    {
                        uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char boolTime[4] = {0};
                        for (int j = 0; j < 4; j++)
                        {
                            boolTime[3 - j] |= standardBoolTime;
                            standardBoolTime >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, boolTime, 4); //持续时长
                        writebuff_pos += 4;
                    }
                }
            }
            else
            {
                cout << "存在开关量以外的类型，请检查模板或者更换功能块" << endl;
                return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            }
        }

        // DB_DeleteFile(const_cast<char *>(files[fileNum].c_str()));//删除原文件
        long fp;
        string finalpath = files[fileNum].substr(0, files[fileNum].length() - 3); //去掉后缀的zip
        //创建新文件并写入
        err = DB_Open(const_cast<char *>(finalpath.c_str()), "wb", &fp);
        if (err == 0)
        {
            err = DB_Write(fp, writebuff, writebuff_pos);

            if (err == 0)
            {
                DB_Close(fp);
            }
        }
    }
    return err;
}

/**
 * @brief 压缩接收到只有开关量类型的整条数据
 *
 * @param ZipTemPath 压缩模板路径
 * @param filepath 存储文件路径
 * @param buff 接收到的数据
 * @param buffLength 接收的数据长度
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipRecvSwitchBuff(const char *ZipTemPath, const char *filepath, char *buff, long *buffLength)
{
    int err = 0;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    long len = *buffLength;

    char writebuff[CurrentZipTemplate.totalBytes + 2 * CurrentZipTemplate.schemas.size()]; //写入没有被压缩的数据

    DataTypeConverter converter;
    long buff_pos = 0;
    long writebuff_pos = 0;

    for (int i = 0; i < CurrentZipTemplate.schemas.size(); i++)
    {
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // BOOL变量
        {

            uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
            uint32 maxBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
            uint32 minBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

            // 4个字节的持续时长,暂定，根据后续情况可能进行更改
            char value[4] = {0};
            memcpy(value, buff + buff_pos, 4);
            uint32 currentBoolTime = converter.ToUInt32(value);
            if (currentBoolTime != standardBoolTime && (currentBoolTime < minBoolTime || currentBoolTime > maxBoolTime))
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                memcpy(writebuff + writebuff_pos, buff + buff_pos, 4);
                writebuff_pos += 4;
            }
            buff_pos += 4;
        }
        else
        {
            cout << "存在开关量以外的类型，请检查模板或者更换功能块" << endl;
            return StatusCode::DATA_TYPE_MISMATCH_ERROR;
        }
    }

    if (writebuff_pos >= buff_pos) //表明数据没有被压缩
    {
        char isZip[1] = {0};
        char finnalBuf[len + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, buff, *buffLength);
        memcpy(buff, finnalBuf, len + 1);
        *buffLength += 1;
    }
    else if (writebuff_pos == 0) //数据完全压缩
    {
        char isZip[1] = {1};
        char finnalBuf[writebuff_pos + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, writebuff, writebuff_pos);
        memcpy(buff, finnalBuf, writebuff_pos + 1);
        *buffLength = writebuff_pos + 1;
    }
    else //数据未完全压缩
    {
        char isZip[1] = {2};
        char finnalBuf[writebuff_pos + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, writebuff, writebuff_pos);
        memcpy(buff, finnalBuf, writebuff_pos + 1);
        *buffLength = writebuff_pos + 1;
    }

    DB_DataBuffer databuff;
    databuff.length = *buffLength;
    databuff.buffer = buff;
    databuff.savePath = filepath;
    DB_InsertRecord(&databuff, 1);
    return err;
}

/**
 * @brief 压缩接收到只有模拟量类型的.idb文件
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine 存储文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipAnalogFile(const char *ZipTemPath, const char *pathToLine)
{
    int err;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;
    vector<string> files;
    readIDBFilesList(pathToLine, files);
    if (files.size() == 0)
    {
        cout << "没有文件！" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    for (size_t fileNum = 0; fileNum < files.size(); fileNum++) //循环以给每个.idb文件进行压缩处理
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(files[fileNum].c_str()), &len);
        char readbuff[len];                                                                    //文件内容
        char writebuff[CurrentZipTemplate.totalBytes + 2 * CurrentZipTemplate.schemas.size()]; //写入没有被压缩的数据
        long readbuff_pos = 0;
        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(files[fileNum].c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        for (int i = 0; i < CurrentZipTemplate.schemas.size(); i++)
        {
            if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
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
                    if (currentUDintValue != standardUDintValue && (currentUDintValue < maxUDintValue || currentUDintValue > minUDintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                        writebuff_pos += 4;
                    }
                    readbuff_pos += 4;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
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
                    if (currentUSintValue != standardUSintValue && (currentUSintValue < maxUSintValue || currentUSintValue > minUSintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                        writebuff_pos += 1;
                    }
                    readbuff_pos += 1;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
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
                    if (currentUintValue != standardUintValue && (currentUintValue < maxUintValue || currentUintValue > minUintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                        writebuff_pos += 2;
                    }
                    readbuff_pos += 2;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // UINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
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
                    if (currentSintValue != standardSintValue && (currentSintValue < maxSintValue || currentSintValue > minSintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                        writebuff_pos += 1;
                    }
                    readbuff_pos += 1;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
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
                    if (currentIntValue != standardIntValue && (currentIntValue < maxIntValue || currentIntValue > minIntValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                        writebuff_pos += 2;
                    }
                    readbuff_pos += 2;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
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
                    if (currentDintValue != standardDintValue && (currentDintValue < maxDintValue || currentDintValue > minDintValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                        writebuff_pos += 4;
                    }
                    readbuff_pos += 4;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL类型
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;
                    memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                    writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
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
                    if (currentRealValue != standardFloatValue && (currentRealValue < maxFloatValue || currentRealValue > minFloatValue))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        uint16_t posNum = i;
                        char zipPosNum[2] = {0};
                        for (char j = 0; j < 2; j++)
                        {
                            zipPosNum[1 - j] |= posNum;
                            posNum >>= 8;
                        }
                        memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                        writebuff_pos += 2;

                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                        writebuff_pos += 4;
                    }
                    readbuff_pos += 4;
                }
            }
            else
            {
                cout << "存在模拟量以外的类型，请检查模板或者更换功能块" << endl;
                return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            }
        }
        if (writebuff_pos >= readbuff_pos) //表明数据没有被压缩,不做处理
        {
            cout << files[fileNum] + "文件数据没有被压缩!" << endl;
            // return 1;//1表示数据没有被压缩
        }
        else
        {
            DB_DeleteFile(const_cast<char *>(files[fileNum].c_str())); //删除原文件
            long fp;
            string finalpath = files[fileNum].append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
            //创建新文件并写入
            err = DB_Open(const_cast<char *>(finalpath.c_str()), "wb", &fp);
            if (err == 0)
            {
                if (writebuff_pos != 0)
                    err = DB_Write(fp, writebuff, writebuff_pos);

                if (err == 0)
                {
                    DB_Close(fp);
                }
            }
        }
    }
    return err;
}

/**
 * @brief 还原被压缩的模拟量文件返回原状态
 *
 * @param ZipTemPath 压缩模板所在目录
 * @param pathToLine 压缩文件所在路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ReZipAnalogFile(const char *ZipTemPath, const char *pathToLine)
{
    int err = 0;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;
    vector<string> files;

    readIDBZIPFilesList(pathToLine, files);
    if (files.size() == 0)
    {
        cout << "没有.idbzip文件!" << endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    for (size_t fileNum = 0; fileNum < files.size(); fileNum++)
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(files[fileNum].c_str()), &len);
        char readbuff[len];                            //文件内容
        char writebuff[CurrentZipTemplate.totalBytes]; //写入没有被压缩的数据
        long readbuff_pos = 0;
        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(files[fileNum].c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++)
        {
            if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT类型
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                                writebuff_pos += 1;
                                readbuff_pos += 1;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                                writebuff_pos += 2;
                                readbuff_pos += 2;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 1);
                                writebuff_pos += 1;
                                readbuff_pos += 1;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 2);
                                writebuff_pos += 2;
                                readbuff_pos += 2;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
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
                        memcpy(zipPosNum, readbuff + readbuff_pos, 2);
                        uint16_t posCmp = converter.ToUInt16(zipPosNum);
                        if (posCmp == i) //是未压缩数据的编号
                        {
                            readbuff_pos += 2;
                            if (CurrentZipTemplate.schemas[i].second.isArray == true)
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
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
            else
            {
                cout << "存在模拟量以外的类型，请检查模板或者更换功能块" << endl;
                return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            }
        }
        // DB_DeleteFile(const_cast<char *>(files[fileNum].c_str()));//删除原文件
        long fp;
        string finalpath = files[fileNum].substr(0, files[fileNum].length() - 3); //去掉后缀的zip
        //创建新文件并写入
        err = DB_Open(const_cast<char *>(finalpath.c_str()), "wb", &fp);
        if (err == 0)
        {
            err = DB_Write(fp, writebuff, writebuff_pos);

            if (err == 0)
            {
                DB_Close(fp);
            }
        }
    }
    return err;
}

/**
 * @brief 压缩接收到的只有模拟量类型的整条数据
 *
 * @param ZipTempPath 压缩模板路径
 * @param filepath 存储文件路径
 * @param buff 接收到的要压缩的数据
 * @param buffLength 接收到数据的长度
 * @return 0:success,
 *          others:StatusCode
 */
int DB_ZipRecvAnalogFile(const char *ZipTempPath, const char *filepath, char *buff, long *buffLength)
{
    int err = 0;
    err = DB_LoadZipSchema(ZipTempPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    long len = *buffLength;

    char writebuff[CurrentZipTemplate.totalBytes + 2 * CurrentZipTemplate.schemas.size()]; //写入没有被压缩的数据

    DataTypeConverter converter;
    long buff_pos = 0;
    long writebuff_pos = 0;

    for (int i = 0; i < CurrentZipTemplate.schemas.size(); i++)
    {
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                char standardUSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char maxUSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                char minUSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];

                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, buff + buff_pos, 1);
                char currentUSintValue = value[0];
                if (currentUSintValue != standardUSintValue && (currentUSintValue < maxUSintValue || currentUSintValue > minUSintValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 1);
                    writebuff_pos += 1;
                }
                buff_pos += 1;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;
                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                ushort standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                ushort maxUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                ushort minUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.minValue);

                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, buff + buff_pos, 2);
                ushort currentUintValue = converter.ToUInt16(value);
                if (currentUintValue != standardUintValue && (currentUintValue < maxUintValue || currentUintValue > minUintValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 2);
                    writebuff_pos += 2;
                }
                buff_pos += 2;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;

                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                uint32 maxUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                uint32 minUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, buff + buff_pos, 4);
                uint32 currentUDintValue = converter.ToUInt32(value);
                if (currentUDintValue != standardUDintValue && (currentUDintValue < maxUDintValue || currentUDintValue > minUDintValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 4);
                    writebuff_pos += 4;
                }
                buff_pos += 4;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;
                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                char standardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char maxSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                char minSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];

                // 2个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, buff + buff_pos, 1);
                char currentSintValue = value[0];
                if (currentSintValue != standardSintValue && (currentSintValue < maxSintValue || currentSintValue > minSintValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 1);
                    writebuff_pos += 1;
                }
                buff_pos += 1;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;
                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                short maxIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                short minIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.minValue);

                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, buff + buff_pos, 2);
                short currentIntValue = converter.ToInt16(value);
                if (currentIntValue != standardIntValue && (currentIntValue < maxIntValue || currentIntValue > minIntValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 2);
                    writebuff_pos += 2;
                }
                buff_pos += 2;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;
                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                int maxDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                int minDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, buff + buff_pos, 4);
                int currentDintValue = converter.ToInt32(value);
                if (currentDintValue != standardDintValue && (currentDintValue < maxDintValue || currentDintValue > minDintValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 4);
                    writebuff_pos += 4;
                }
                buff_pos += 4;
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL变量
        {
            if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                uint16_t posNum = i;
                char zipPosNum[2] = {0};
                for (char j = 0; j < 2; j++)
                {
                    zipPosNum[1 - j] |= posNum;
                    posNum >>= 8;
                }
                memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                writebuff_pos += 2;
                memcpy(writebuff + writebuff_pos, buff + buff_pos, CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen);
                writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                buff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
            }
            else
            {
                float standardFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                float maxFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.maxValue);
                float minFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.minValue);

                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, buff + buff_pos, 4);
                int currentRealValue = converter.ToFloat(value);
                if (currentRealValue != standardFloatValue && (currentRealValue < maxFloatValue || currentRealValue > minFloatValue))
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    uint16_t posNum = i;
                    char zipPosNum[2] = {0};
                    for (char j = 0; j < 2; j++)
                    {
                        zipPosNum[1 - j] |= posNum;
                        posNum >>= 8;
                    }
                    memcpy(writebuff + writebuff_pos, zipPosNum, 2);
                    writebuff_pos += 2;

                    memcpy(writebuff + writebuff_pos, buff + buff_pos, 4);
                    writebuff_pos += 4;
                }
                buff_pos += 4;
            }
        }
        else
        {
            cout << "存在模拟量以外的类型，请检查模板或者更换功能块" << endl;
            return StatusCode::DATA_TYPE_MISMATCH_ERROR;
        }
    }

    if (writebuff_pos >= buff_pos) //表明数据没有被压缩
    {
        char isZip[1] = {0};
        char finnalBuf[len + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, buff, *buffLength);
        memcpy(buff, finnalBuf, len + 1);
        *buffLength += 1;
    }
    else if (writebuff_pos == 0) //数据完全压缩
    {
        char isZip[1] = {1};
        char finnalBuf[writebuff_pos + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, writebuff, writebuff_pos);
        memcpy(buff, finnalBuf, writebuff_pos + 1);
        *buffLength = writebuff_pos + 1;
    }
    else //数据未完全压缩
    {
        char isZip[1] = {2};
        char finnalBuf[writebuff_pos + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, writebuff, writebuff_pos);
        memcpy(buff, finnalBuf, writebuff_pos + 1);
        *buffLength = writebuff_pos + 1;
    }

    DB_DataBuffer databuff;
    databuff.length = *buffLength;
    databuff.buffer = buff;
    databuff.savePath = filepath;
    DB_InsertRecord(&databuff, 1);
    return err;
}

// int main()
// {
//     // EDVDB_LoadZipSchema("/");
//     long len;
//     DB_GetFileLengthByPath("Jinfei91_2022-4-1-19-28-49-807.idb", &len);
//     cout << len << endl;
//     char readbuf[len];
//     DB_OpenAndRead("Jinfei91_2022-4-1-19-28-49-807.idb", readbuf);
//     // DB_ZipRecvSwitchBuff("/jinfei","/jinfei",readbuf,&len);
//     // DB_ZipRecvAnalogFile("/jinfei","/jinfei",readbuf,&len);
//     DB_ZipRecvBuff("/jinfei", "/jinfei", readbuf, &len);
//     cout << len << endl;

//     // DB_ZipSwitchFile("/jinfei/","/jinfei/");
//     // DB_ReZipSwitchFile("/jinfei/","/jinfei/");
//     // DB_ZipAnalogFile("/jinfei/","/jinfei/");
//     // DB_ReZipAnalogFile("jinfei/","jinfei/");
//     // DB_ZipFile("jinfei/","jinfei/");
//     // DB_ReZipFile("jinfei/","jinfei/");

//     // char test[len];
//     // memcpy(test,readbuf,len);
//     // cout<<test<<endl;
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
//     return 0;
// }
