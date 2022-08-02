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
    for (int i = 0; i < pathCodes.size(); i++, cur += 10)
    {
        buffer[cur++] = (char)getBufferValueType(typeList[i]);
        if (typeList[i].isTimeseries)
        {
            memcpy(buffer + cur, &typeList[i].tsLen, 4);
            cur += 4;
        }
        if (typeList[i].isArray)
        {
            memcpy(buffer + cur, &typeList[i].arrayLen, 4);
            cur += 4;
        }
        memcpy(buffer + cur, pathCodes[i].code, 10);
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
    int cur = 1;
    for (auto const &schema : this->schemas)
    {
        if (name == schema.first.name)
        {
            buffer[0] = 1;
            buffer[cur++] = (char)getBufferValueType(type);
            if (type.isTimeseries)
            {
                memcpy(buffer + cur, &type.tsLen, 4);
                cur += 4;
            }
            if (type.isArray)
            {
                memcpy(buffer + cur, &type.arrayLen, 4);
                cur += 4;
            }
            memcpy(buffer + cur, schema.first.code, 10);
            cur += 10;
            break;
        }
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
int Template::writeBufferHead(vector<string> &names, vector<DataType> &typeList, char *buffer)
{
    int cur = 1;
    buffer[0] = (unsigned char)names.size();
    for (int i = 0; i < names.size(); i++)
    {
        for (int j = 0; j < this->schemas.size(); j++)
        {
            if (names[i] == this->schemas[j].first.name)
            {
                buffer[cur++] = (char)getBufferValueType(typeList[i]);
                // cout << (int)getBufferValueType(typeList[i]) << endl;
                if (typeList[i].isTimeseries)
                {
                    memcpy(buffer + cur, &typeList[i].tsLen, 4);
                    cur += 4;
                }
                if (typeList[i].isArray)
                {
                    memcpy(buffer + cur, &typeList[i].arrayLen, 4);
                    cur += 4;
                }
                memcpy(buffer + cur, this->schemas[j].first.code, 10);
                cur += 10;
                break;
            }
        }
    }

    return cur;
}

/**
 * @brief 根据编码获取所有PathCode类型数据
 *
 * @param pathCode 编码
 * @param pathCodes 编码列表
 * @return int
 */
int Template::GetAllPathsByCode(char *pathCode, vector<PathCode> &pathCodes)
{
    pathCodes.clear();
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
    return pathCodes.size() > 0 ? 0 : StatusCode::UNKNOWN_PATHCODE;
}

/**
 * @brief 检查指定编码下是否包含数组类型数据
 *
 * @param pathCode
 * @return bool
 */
bool Template::checkHasArray(char *pathCode)
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
            if (schema.second.isArray)
            {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 检查指定编码下是否包含图片类型数据
 *
 * @param pathCode
 * @return bool
 */
bool Template::checkHasImage(char *pathCode)
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
            if (schema.second.valueType == ValueType::IMAGE)
            {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 检查指定编码下是否包含时间序列
 *
 * @param pathCode
 * @return bool
 */
bool Template::checkHasTimeseries(char *pathCode)
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
            if (schema.second.isTimeseries)
            {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 检查指定编码下是否包含数组类型数据
 *
 * @param pathCode
 * @return bool
 */
bool Template::checkIsArray(const char *name)
{
    for (auto const &schema : this->schemas)
    {
        string str = name;
        if (schema.first.name == str)
        {
            if (schema.second.isArray)
            {
                return true;
            }
        }
    }
    return false;
}
/**
 * @brief 检查指定编码下是否包含数组类型数据
 *
 * @param names
 * @return bool
 */
bool Template::checkHasArray(vector<string> &names)
{
    for (auto const &name : names)
    {
        for (auto const &schema : this->schemas)
        {
            string str = name;
            if (schema.first.name == str)
            {
                if (schema.second.isArray)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

/**
 * @brief 检查指定编码下是否包含图片类型数据
 *
 * @param pathCode
 * @return bool
 */
bool Template::checkIsImage(const char *name)
{
    for (auto const &schema : this->schemas)
    {
        string str = name;
        if (schema.first.name == str)
        {
            if (schema.second.valueType == ValueType::IMAGE)
            {
                return true;
            }
        }
    }
    return false;
}
/**
 * @brief 检查指定编码下是否包含数组类型数据
 *
 * @param names
 * @return bool
 */
bool Template::checkHasImage(vector<string> &names)
{
    for (auto const &name : names)
    {
        for (auto const &schema : this->schemas)
        {
            string str = name;
            if (schema.first.name == str)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

/**
 * @brief 检查指定编码下是否包含时间序列
 *
 * @param pathCode
 * @return bool
 */
bool Template::checkIsTimeseries(const char *name)
{
    for (auto const &schema : this->schemas)
    {
        string str = name;
        if (schema.first.name == str)
        {
            if (schema.second.isTimeseries)
            {
                return true;
            }
        }
    }
    return false;
}
/**
 * @brief 检查指定编码下是否包含时间序列
 *
 * @param names
 * @return bool
 */
bool Template::checkHasTimeseries(vector<string> &names)
{
    for (auto const &name : names)
    {
        for (auto const &schema : this->schemas)
        {
            string str = name;
            if (schema.first.name == str)
            {
                if (schema.second.isTimeseries)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

/**
 * @brief 获取模版中的数据总字节数
 *
 * @return long
 */
long Template::GetTotalBytes()
{
    long total = 0;
    for (auto const &schema : this->schemas)
    {
        if (schema.second.isTimeseries)
        {
            if (schema.second.isArray)
            {
                total += (schema.second.valueBytes * schema.second.arrayLen + 8) * schema.second.tsLen;
            }
            else
            {
                total += (schema.second.valueBytes + 8) * schema.second.tsLen;
            }
        }
        else if (schema.second.isArray)
        {
            // cout << "total=" << total << endl;
            if (schema.second.valueType != ValueType::IMAGE)
                total += schema.second.hasTime ? (8 + schema.second.valueBytes * schema.second.arrayLen) : (schema.second.valueBytes * schema.second.arrayLen);
        }
        else
        {
            total += schema.second.hasTime ? 8 + schema.second.valueBytes : schema.second.valueBytes;
        }
        // if (schema.second.valueType == ValueType::IMAGE)
        //     total += 5 * 1024 * 1024;
    }
    return total;
}

/**
 * @brief 从已选择的数据中获取要排序或比较多数据偏移
 *
 * @param bytesList 字节长度列表
 * @param name 变量名
 * @param pathCode 编码
 * @param typeList 数据类型列表
 * @return long
 */
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
    // long cur = 0;
    // for (int i = 0; i < this->schemas.size(); i++)
    // {
    //     if (name == this->schemas[i].first.name)
    //     {
    //         return cur;
    //     }
    //     cur += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
    // }

    return 0;
}

/**
 * @brief 从已选择的数据中获取要排序或比较多数据偏移
 *
 * @param bytesList 字节长度列表
 * @param name 变量名
 * @param pathCodes 编码列表
 * @param typeList 数据类型列表
 * @return long
 */
long Template::FindSortPosFromSelectedData(vector<long> &bytesList, string name, vector<PathCode> &pathCodes, vector<DataType> &typeList)
{
    long cur = 0;
    for (int i = 0; i < pathCodes.size(); i++)
    {
        if (name == pathCodes[i].name)
        {
            return cur;
        }
        cur += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
    }
    // long cur = 0;
    // for (int i = 0; i < this->schemas.size(); i++)
    // {
    //     if (name == this->schemas[i].first.name)
    //     {
    //         return cur;
    //     }
    //     cur += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
    // }

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
 * @note ! Deprecated
 */
int Template::FindDatatypePosByCode(char pathCode[], char *buff, long &position, long &bytes)
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
                    char length[2], width[2], channels[2];
                    memcpy(length, buff + pos, 2);
                    pos += 2;
                    memcpy(width, buff + pos, 2);
                    pos += 2;
                    memcpy(channels, buff + pos, 2);
                    num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels);
                    position += 6;
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
                    char length[2], width[2], channels[2];
                    memcpy(length, buff + pos, 2);
                    pos += 2;
                    memcpy(width, buff + pos, 2);
                    pos += 2;
                    memcpy(channels, buff + pos, 2);
                    num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels) + 2;
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
int Template::FindDatatypePosByCode(char pathCode[], char *buff, long &position, long &bytes, DataType &type)
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
            int num = 1; //元素个数
            if (schema.second.isTimeseries)
            {
                if (schema.second.isArray) //暂不支持图片的时间序列
                {
                    num = schema.second.arrayLen * schema.second.tsLen;
                }
                else
                {
                    num = schema.second.tsLen;
                }
            }
            else if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {
                    char length[2], width[2], channels[2];
                    memcpy(length, buff + pos, 2);
                    pos += 2;
                    memcpy(width, buff + pos, 2);
                    pos += 2;
                    memcpy(channels, buff + pos, 2);
                    num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels);
                    position += 6;
                }
                else
                    num = schema.second.arrayLen;
            }
            bytes = num * schema.second.valueBytes + (schema.second.tsLen * (schema.second.isTimeseries ? 8 : 0));
            type = schema.second;
            return 0;
        }
        else
        {
            int num = 1;
            if (schema.second.isTimeseries)
            {
                if (schema.second.isArray) //暂不支持图片的时间序列
                {
                    num = schema.second.arrayLen * schema.second.tsLen;
                }
                else
                {
                    num = schema.second.tsLen;
                }
            }
            else if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {
                    char length[2], width[2], channels[2];
                    memcpy(length, buff + pos, 2);
                    pos += 2;
                    memcpy(width, buff + pos, 2);
                    pos += 2;
                    memcpy(channels, buff + pos, 2);
                    num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels) + 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
            pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
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
int Template::FindMultiDatatypePosByCode(char pathCode[], char *buff, vector<long> &positions, vector<long> &bytes, vector<DataType> &types)
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
            if (schema.second.isTimeseries)
            {
                if (schema.second.isArray) //暂不支持图片的时间序列
                {
                    num = schema.second.arrayLen * schema.second.tsLen;
                }
                else
                {
                    num = schema.second.tsLen;
                }
            }
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {
                    char length[2], width[2], channels[2];
                    memcpy(length, buff + pos, 2);
                    pos += 2;
                    memcpy(width, buff + pos, 2);
                    pos += 2;
                    memcpy(channels, buff + pos, 2);
                    num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels);
                    pos += 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            positions.push_back(pos);
            bytes.push_back(num * schema.second.valueBytes + schema.second.tsLen * (schema.second.isTimeseries ? 8 : 0)); //时间序列整个获取
            types.push_back(schema.second);
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
            pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
        }
        else
        {
            int num = 1;
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {
                    char length[2], width[2], channels[2];
                    memcpy(length, buff + pos, 2);
                    pos += 2;
                    memcpy(width, buff + pos, 2);
                    pos += 2;
                    memcpy(channels, buff + pos, 2);
                    num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels) + 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
            pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
        }
    }
    return positions.size() == 0 ? StatusCode::UNKNOWN_PATHCODE : 0;
}

/**
 * @brief 根据当前模版模糊寻找指定路径编码的数据在数据文件中的位置，即获取模版中某一路径下所有孩子结点，包含自身,适用于不带图片的数据
 * @param pathCode        路径编码
 * @param positions    数据起始位置
 * @param buff        文件数据
 * @param bytes       数据长度
 * @param types        数据类型
 *
 * @return  0:success,
 *          others: StatusCode
 * @note    图片数据目前暂时协定前2个字节为图片总长，不包括图片自身,仅针对无图片数据
 */
int Template::FindMultiDatatypePosByCode(char pathCode[], vector<long> &positions, vector<long> &bytes, vector<DataType> &types)
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
            if (schema.second.isTimeseries)
            {
                if (schema.second.isArray)
                {
                    num = schema.second.arrayLen * schema.second.tsLen;
                }
                else
                {
                    num = schema.second.tsLen;
                }
            }
            else if (schema.second.isArray)
            {
                num = schema.second.arrayLen;
            }
            positions.push_back(pos);
            bytes.push_back(num * schema.second.valueBytes + schema.second.tsLen * (schema.second.isTimeseries ? 8 : 0)); //时间序列整个获取
            types.push_back(schema.second);
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
            pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
        }
        else
        {
            int num = 1;
            if (schema.second.isTimeseries)
            {
                if (schema.second.isArray)
                {
                    num = schema.second.arrayLen * schema.second.tsLen;
                }
                else
                {
                    num = schema.second.tsLen;
                }
            }
            else if (schema.second.isArray)
            {
                num = schema.second.arrayLen;
            }
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
            pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
        }
    }
    return positions.size() == 0 ? StatusCode::UNKNOWN_PATHCODE : 0;
}

/**
 * @brief 根据当前模版模糊寻找指定路径编码的数据在数据文件中的位置，即获取模版中某一路径下所有孩子结点，包含自身
 * @param pathCode        路径编码
 * @param positions    数据起始位置
 * @param buff        文件数据
 * @param bytes       数据长度
 *
 * @return  0:success,
 *          others: StatusCode
 * @note    图片数据目前暂时协定前2个字节为图片总长，不包括图片自身
 */
int Template::FindMultiDatatypePosByCode(char pathCode[], char *buff, vector<long> &positions, vector<long> &bytes)
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
                    char length[2], width[2], channels[2];
                    memcpy(length, buff + pos, 2);
                    pos += 2;
                    memcpy(width, buff + pos, 2);
                    pos += 2;
                    memcpy(channels, buff + pos, 2);
                    num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels);
                    pos += 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            positions.push_back(pos);
            bytes.push_back(num * schema.second.valueBytes + schema.second.tsLen * (schema.second.isTimeseries ? 8 : 0)); //时间序列整个获取
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
            pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
        }
        else
        {
            int num = 1;
            if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {
                    char length[2], width[2], channels[2];
                    memcpy(length, buff + pos, 2);
                    pos += 2;
                    memcpy(width, buff + pos, 2);
                    pos += 2;
                    memcpy(channels, buff + pos, 2);
                    num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels) + 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
            pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
        }
    }
    return positions.size() == 0 ? StatusCode::UNKNOWN_PATHCODE : 0;
}

/**
 * @brief 根据当前模版寻找指定变量名的数据在数据文件中的位置,适用于不带图片的数据
 *
 * @param name        变量名
 * @param position    数据起始位置
 * @param bytes       数据长度
 * @param type        数据类型
 * @return int
 */
int Template::FindDatatypePosByName(const char *name, long &position, long &bytes, DataType &type)
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
        //可能存在时间序列中的值为数组
        if (nameEquals)
        {
            position = pos;
            int num = 1;
            if (schema.second.isTimeseries)
            {
                if (schema.second.isArray)
                {
                    num = schema.second.arrayLen * schema.second.tsLen;
                }
                else
                {
                    num = schema.second.tsLen;
                }
            }
            else if (schema.second.isArray)
            {
                num = schema.second.arrayLen;
            }
            bytes = num * schema.second.valueBytes + (schema.second.tsLen * (schema.second.isTimeseries ? 8 : 0));
            type = schema.second;
            return 0;
        }
        else
        {
            int num = 1;
            if (schema.second.isTimeseries)
            {
                if (schema.second.isArray)
                {
                    num = schema.second.arrayLen * schema.second.tsLen;
                }
                else
                {
                    num = schema.second.tsLen;
                }
            }
            else if (schema.second.isArray)
            {
                num = schema.second.arrayLen;
            }
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
            pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
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
 *
 * @return  0:success,
 *          others: StatusCode
 * @note ! Deprecated
 */
int Template::FindDatatypePosByName(const char *name, char *buff, long &position, long &bytes)
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
                    char length[2], width[2], channels[2];
                    memcpy(length, buff + pos, 2);
                    pos += 2;
                    memcpy(width, buff + pos, 2);
                    pos += 2;
                    memcpy(channels, buff + pos, 2);
                    num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels);
                    position += 6;
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
                    char length[2], width[2], channels[2];
                    memcpy(length, buff + pos, 2);
                    pos += 2;
                    memcpy(width, buff + pos, 2);
                    pos += 2;
                    memcpy(channels, buff + pos, 2);
                    num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels) + 2;
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
int Template::FindDatatypePosByName(const char *name, char *buff, long &position, long &bytes, DataType &type)
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
            if (schema.second.isTimeseries)
            {
                if (schema.second.isArray) //暂不支持图片的时间序列
                {
                    num = schema.second.arrayLen * schema.second.tsLen;
                }
                else
                {
                    num = schema.second.tsLen;
                }
            }
            else if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {
                    char length[2], width[2], channels[2];
                    memcpy(length, buff + pos, 2);
                    pos += 2;
                    memcpy(width, buff + pos, 2);
                    pos += 2;
                    memcpy(channels, buff + pos, 2);
                    num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels);
                    position += 6;
                }
                else
                    num = schema.second.arrayLen;
            }
            bytes = num * schema.second.valueBytes + (schema.second.tsLen * (schema.second.isTimeseries ? 8 : 0));
            type = schema.second;
            return 0;
        }
        else
        {
            int num = 1;
            if (schema.second.isTimeseries)
            {
                if (schema.second.isArray) //暂不支持图片的时间序列
                {
                    num = schema.second.arrayLen * schema.second.tsLen;
                }
                else
                {
                    num = schema.second.tsLen;
                }
            }
            else if (schema.second.isArray)
            {
                if (schema.second.valueType == ValueType::IMAGE)
                {
                    char length[2], width[2], channels[2];
                    memcpy(length, buff + pos, 2);
                    pos += 2;
                    memcpy(width, buff + pos, 2);
                    pos += 2;
                    memcpy(channels, buff + pos, 2);
                    num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels) + 2;
                }
                else
                    num = schema.second.arrayLen;
            }
            pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
            pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
        }
    }
    return StatusCode::UNKNOWN_VARIABLE_NAME;
}

/**
 * @brief 根据当前模版寻找多个变量名的数据在数据文件中的位置，适用于不带图片的数据,若包含未知的变量名将无视
 * @param names        路径编码
 * @param positions    数据起始位置
 * @param buff        文件数据
 * @param bytes       数据长度
 * @param types        数据类型
 *
 * @return  0:success,
 *          others: StatusCode
 * @note    图片数据目前暂时协定前2个字节为图片总长，不包括图片自身,仅针对无图片数据
 */
int Template::FindMultiDatatypePosByNames(vector<string> &names, vector<long> &positions, vector<long> &bytes, vector<DataType> &types)
{
    for (auto const &name : names)
    {
        long pos = 0;
        for (auto const &schema : this->schemas)
        {
            if (schema.first.name == name)
            {
                int num = 1;
                if (schema.second.isTimeseries)
                {
                    if (schema.second.isArray)
                    {
                        num = schema.second.arrayLen * schema.second.tsLen;
                    }
                    else
                    {
                        num = schema.second.tsLen;
                    }
                }
                else if (schema.second.isArray)
                {
                    num = schema.second.arrayLen;
                }
                positions.push_back(pos);
                bytes.push_back(num * schema.second.valueBytes + schema.second.tsLen * (schema.second.isTimeseries ? 8 : 0)); //时间序列整个获取
                types.push_back(schema.second);
                pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
                pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
            }
            else
            {
                int num = 1;
                if (schema.second.isTimeseries)
                {
                    if (schema.second.isArray)
                    {
                        num = schema.second.arrayLen * schema.second.tsLen;
                    }
                    else
                    {
                        num = schema.second.tsLen;
                    }
                }
                else if (schema.second.isArray)
                {
                    num = schema.second.arrayLen;
                }
                pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
                pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
            }
        }
    }

    return positions.size() == 0 ? StatusCode::UNKNOWN_VARIABLE_NAME : 0;
}

/**
 * @brief 根据当前模版寻找多个变量名的数据在数据文件中的位置，若包含未知的变量名将无视
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
int Template::FindMultiDatatypePosByNames(vector<string> &names, char *buff, vector<long> &positions, vector<long> &bytes, vector<DataType> &types)
{
    DataTypeConverter converter;
    for (auto const &name : names)
    {
        long pos = 0;
        for (auto const &schema : this->schemas)
        {
            if (schema.first.name == name)
            {
                int num = 1;
                if (schema.second.isTimeseries)
                {
                    if (schema.second.isArray) //暂不支持图片的时间序列
                    {
                        num = schema.second.arrayLen * schema.second.tsLen;
                    }
                    else
                    {
                        num = schema.second.tsLen;
                    }
                }
                else if (schema.second.isArray)
                {
                    if (schema.second.valueType == ValueType::IMAGE)
                    {
                        char length[2], width[2], channels[2];
                        memcpy(length, buff + pos, 2);
                        pos += 2;
                        memcpy(width, buff + pos, 2);
                        pos += 2;
                        memcpy(channels, buff + pos, 2);
                        num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels);
                        pos += 2;
                    }
                    else
                        num = schema.second.arrayLen;
                }
                positions.push_back(pos);
                bytes.push_back(num * schema.second.valueBytes + schema.second.tsLen * (schema.second.isTimeseries ? 8 : 0)); //时间序列整个获取
                types.push_back(schema.second);
                pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
                pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
            }
            else
            {
                int num = 1;
                if (schema.second.isTimeseries)
                {
                    if (schema.second.isArray)
                    {
                        num = schema.second.arrayLen * schema.second.tsLen;
                    }
                    else
                    {
                        num = schema.second.tsLen;
                    }
                }
                else if (schema.second.isArray)
                {
                    num = schema.second.arrayLen;
                }
                pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
                pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
            }
        }
    }

    return positions.size() == 0 ? StatusCode::UNKNOWN_VARIABLE_NAME : 0;
}

/**
 * @brief 根据当前模版模糊寻找指定路径编码的数据在数据文件中的位置，即获取模版中某一路径下所有孩子结点，包含自身
 * @param pathCode        路径编码
 * @param positions    数据起始位置
 * @param buff        文件数据
 * @param bytes       数据长度
 *
 * @return  0:success,
 *          others: StatusCode
 * @note    图片数据目前暂时协定前2个字节为图片总长，不包括图片自身
 */
int Template::FindMultiDatatypePosByNames(vector<string> &names, char *buff, vector<long> &positions, vector<long> &bytes)
{
    DataTypeConverter converter;

    for (auto const &name : names)
    {
        long pos = 0;
        for (auto const &schema : this->schemas)
        {
            if (schema.first.name == name)
            {
                int num = 1;
                if (schema.second.isArray)
                {
                    if (schema.second.valueType == ValueType::IMAGE)
                    {
                        char length[2], width[2], channels[2];
                        memcpy(length, buff + pos, 2);
                        pos += 2;
                        memcpy(width, buff + pos, 2);
                        pos += 2;
                        memcpy(channels, buff + pos, 2);
                        num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels);
                        pos += 2;
                    }
                    else
                        num = schema.second.arrayLen;
                }
                positions.push_back(pos);
                bytes.push_back(num * schema.second.valueBytes + schema.second.tsLen * (schema.second.isTimeseries ? 8 : 0)); //时间序列整个获取
                pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
                pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
            }
            else
            {
                int num = 1;
                if (schema.second.isArray)
                {
                    if (schema.second.valueType == ValueType::IMAGE)
                    {
                        char length[2], width[2], channels[2];
                        memcpy(length, buff + pos, 2);
                        pos += 2;
                        memcpy(width, buff + pos, 2);
                        pos += 2;
                        memcpy(channels, buff + pos, 2);
                        num = (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels) + 2;
                    }
                    else
                        num = schema.second.arrayLen;
                }
                pos += schema.second.hasTime ? num * schema.second.valueBytes + 8 : num * schema.second.valueBytes;
                pos += schema.second.isTimeseries ? 8 * schema.second.tsLen : 0;
            }
        }
    }
    return positions.size() == 0 ? StatusCode::UNKNOWN_PATHCODE : 0;
}

vector<DataType> Template::GetAllTypes(char *pathCode)
{
    vector<DataType> res;
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
            res.push_back(schema.second);
        }
    }
    return res;
}

/**
 * @brief 根据编码获取数据类型
 *
 * @param pathCode
 * @param types
 * @return int
 */
int Template::GetDataTypeByCode(char *pathCode, DataType &type)
{
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
            type = schema.second;
            return 0;
        }
    }
    return StatusCode::UNKNOWN_PATHCODE;
}

/**
 * @brief 根据编码获取数据类型
 *
 * @param pathCode
 * @param types
 * @return int
 */
int Template::GetDataTypesByCode(char *pathCode, vector<DataType> &types)
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
            types.push_back(schema.second);
        }
    }
    return types.size() == 0 ? StatusCode::UNKNOWN_PATHCODE : 0;
}

/**
 * @brief Get the datatype object by variable name.
 *
 * @param name
 * @param type
 * @return int
 */
int Template::GetDataTypeByName(const char *name, DataType &type)
{
    string str = name;
    for (auto const &schema : this->schemas)
    {
        if (schema.first.name == str)
        {
            type = schema.second;
            return 0;
        }
    }
    return StatusCode::UNKNOWN_VARIABLE_NAME;
}

/**
 * @brief 根据编码计算所选数据的总字节数
 *
 * @param pathCode
 * @return long
 */
long Template::GetBytesByCode(char *pathCode)
{
    vector<DataType> types;
    long total = 0;
    this->GetDataTypesByCode(pathCode, types);
    for (auto const &type : types)
    {
        if (type.isTimeseries)
        {
            if (type.isArray)
            {
                total += (type.valueBytes * type.arrayLen + 8) * type.tsLen;
            }
            else
            {
                total += (type.valueBytes + 8) * type.tsLen;
            }
        }
        else if (type.isArray)
        {
            // cout << type.valueBytes << " " << type.arrayLen << endl;
            total += type.hasTime ? (8 + type.valueBytes * type.arrayLen) : (type.valueBytes * type.arrayLen);
        }
        else
        {
            total += type.hasTime ? (8 + type.valueBytes) : type.valueBytes;
        }
    }
    return total;
}

int Template::GetCodeByName(const char *name, vector<PathCode> &pathCode)
{
    for (auto &schema : this->schemas)
    {
        if (strcmp(schema.first.name.c_str(), name) == 0)
        {
            pathCode.push_back(schema.first);
            return 0;
        }
    }
    return StatusCode::UNKNOWN_VARIABLE_NAME;
}

int Template::GetCodeByName(const char *name, PathCode &pathCode)
{
    for (auto &schema : this->schemas)
    {
        if (strcmp(schema.first.name.c_str(), name) == 0)
        {
            pathCode = schema.first;
            return 0;
        }
    }
    return StatusCode::UNKNOWN_VARIABLE_NAME;
}

// int TemplateManager::SetTemplate(const char *path)
// {
//     // UnsetTemplate(path);
//     vector<string> files;
//     readFileList(path, files);
//     string temPath = "";
//     for (string file : files) //找到带有后缀tem的文件
//     {
//         string s = file;
//         vector<string> vec = DataType::StringSplit(const_cast<char *>(s.c_str()), ".");
//         if (vec[vec.size() - 1].find("tem") != string::npos && vec[vec.size() - 1].find("ziptem") == string::npos)
//         {
//             temPath = file;
//             break;
//         }
//     }
//     if (temPath == "")
//         return StatusCode::SCHEMA_FILE_NOT_FOUND;
//     long length;
//     DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &length);
//     char buf[length];
//     int err = DB_OpenAndRead(const_cast<char *>(temPath.c_str()), buf);
//     if (err != 0)
//         return err;
//     int i = 0;
//     vector<PathCode> pathEncodes;
//     vector<DataType> dataTypes;
//     while (i < length)
//     {
//         char variable[30], dataType[30], pathEncode[10];
//         memcpy(variable, buf + i, 30);
//         i += 30;
//         memcpy(dataType, buf + i, 30);
//         i += 30;
//         char timeFlag = buf[++i];
//         memcpy(pathEncode, buf + i, 10);
//         i += 10;
//         vector<string> paths;
//         PathCode pathCode(pathEncode, sizeof(pathEncode) / 2, variable);
//         DataType type;
//         if ((int)timeFlag == 1)
//             type.hasTime = true;
//         else
//             type.hasTime = false;
//         if (DataType::GetDataTypeFromStr(dataType, type) == 0)
//         {
//             dataTypes.push_back(type);
//         }
//         else
//             return errorCode;
//         pathEncodes.push_back(pathCode);
//     }
//     Template tem(pathEncodes, dataTypes, path);
//     AddTemplate(tem);
//     CurrentTemplate = tem;
//     return 0;
// }

int TemplateManager::UnsetTemplate(string path)
{
    for (int i = 0; i < templates.size(); i++)
    {
        if (templates[i].path == path)
        {
            templates.erase(templates.begin() + i);
        }
    }
    CurrentTemplate.path = "";
    CurrentTemplate.schemas.clear();
    // CurrentTemplate.temFileBuffer = NULL;
    return 0;
}

long ZipTemplate::GetTotalBytes()
{
    long total = 0;
    for (auto const &schema : this->schemas)
    {
        if (schema.second.isTimeseries)
        {
            if (schema.second.isArray)
            {
                total += (schema.second.valueBytes * schema.second.arrayLen + 8) * schema.second.tsLen;
            }
            else
            {
                total += (schema.second.valueBytes + 8) * schema.second.tsLen;
            }
        }
        else if (schema.second.isArray)
        {
            total += schema.second.hasTime ? 8 + schema.second.valueBytes * schema.second.arrayLen : schema.second.valueBytes * schema.second.arrayLen;
        }
        else
        {
            total += schema.second.hasTime ? 8 + schema.second.valueBytes : schema.second.valueBytes;
        }
        // if (schema.second.valueType == ValueType::IMAGE)
        //     total += 5 * 1024 * 1024;
    }
    return total;
}
