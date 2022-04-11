#include <utils.hpp>

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

long ZipTemplate::GetTotalBytes()
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