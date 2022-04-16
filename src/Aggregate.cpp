
#include <utils.hpp>

/**
 * @brief 根据自定义查询请求参数，获取单个或多个非数组变量各自的最大值
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_MAX(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::SetTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
    for (int i = 0; i < typeNum; i++)
    {
        DataType type;
        int typeVal = buffer->buffer[i * 11 + 1];
        switch ((typeVal - 1) / 10)
        {
        case 0:
        {
            type.isArray = false;
            type.hasTime = false;
            break;
        }
        case 1:
        {
            type.isArray = false;
            type.hasTime = true;
            break;
        }
        case 2:
        {
            type.isArray = true;
            type.hasTime = false;
            break;
        }
        case 3:
        {
            type.isArray = true;
            type.hasTime = true;
            break;
        }
        default:
            break;
        }
        type.valueType = (ValueType::ValueType)((typeVal - 1) % 10 + 1);
        type.valueBytes = DataType::GetValueBytes(type.valueType);
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            short max = INT16_MIN;
            char res[2];
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                short value = converter.ToInt16(val);
                if (max < value)
                {
                    max = value;
                    memcpy(res, val, 2);
                }
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, res, 2);
            newBufCur += 2;
            break;
        }
        case ValueType::UINT:
        {
            uint16_t max = 0;
            char val[2];
            char res[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                short value = converter.ToUInt16(val);
                if (max < value)
                {
                    max = value;
                    memcpy(res, val, 2);
                }
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 2);
            newBufCur += 2;
            break;
        }
        case ValueType::DINT:
        {
            int max = INT_MIN;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToInt32(val);
                if (max < value)
                {
                    max = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            uint max = 0;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToUInt32(val);
                cout << value << endl;
                if (max < value)
                {
                    max = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float max = __FLT_MIN__;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToFloat(val);
                if (max < value)
                {
                    max = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            int max = INT_MIN;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToInt32(val);
                if (max < value)
                {
                    max = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "max:" << max << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char max = INT8_MIN;
            char value;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                if (max < value)
                    max = value;
            }
            cout << "max:" << (int)max << endl;
            memcpy(newBuffer + newBufCur++, &value, 1);
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，获取单个或多个非数组变量各自的最小值
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_MIN(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::SetTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
    for (int i = 0; i < typeNum; i++)
    {
        DataType type;
        int typeVal = buffer->buffer[i * 11 + 1];
        switch ((typeVal - 1) / 10)
        {
        case 0:
        {
            type.isArray = false;
            type.hasTime = false;
            break;
        }
        case 1:
        {
            type.isArray = false;
            type.hasTime = true;
            break;
        }
        case 2:
        {
            type.isArray = true;
            type.hasTime = false;
            break;
        }
        case 3:
        {
            type.isArray = true;
            type.hasTime = true;
            break;
        }
        default:
            break;
        }
        type.valueType = (ValueType::ValueType)((typeVal - 1) % 10 + 1);
        type.valueBytes = DataType::GetValueBytes(type.valueType);
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            short min = INT16_MAX;
            char res[2];
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                short value = converter.ToInt16(val);
                if (min > value)
                {
                    min = value;
                    memcpy(res, val, 2);
                }
            }
            cout << "min:" << min << endl;
            memcpy(newBuffer + newBufCur, res, 2);
            newBufCur += 2;
            break;
        }
        case ValueType::UINT:
        {
            uint16_t min = UINT16_MAX;
            char val[2];
            char res[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                short value = converter.ToUInt16(val);
                if (min > value)
                {
                    min = value;
                    memcpy(res, val, 2);
                }
            }
            cout << "min:" << min << endl;
            memcpy(newBuffer + newBufCur, val, 2);
            newBufCur += 2;
            break;
        }
        case ValueType::DINT:
        {
            int min = INT_MAX;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToInt32(val);
                if (min > value)
                {
                    min = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "min:" << min << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            uint min = UINT32_MAX;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToUInt32(val);
                if (min > value)
                {
                    min = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "min:" << min << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float min = __FLT_MAX__;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToFloat(val);
                if (min > value)
                {
                    min = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "min:" << min << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            int min = INT_MAX;
            char val[4];
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                short value = converter.ToInt32(val);
                if (min > value)
                {
                    min = value;
                    memcpy(res, val, 4);
                }
            }
            cout << "min:" << min << endl;
            memcpy(newBuffer + newBufCur, val, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char min = INT8_MAX;
            char value;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                if (min > value)
                    min = value;
            }
            cout << "min:" << (int)min << endl;
            memcpy(newBuffer + newBufCur++, &value, 1);
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，对单个或多个非数组变量各自求和
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_SUM(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::SetTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 1; //每行的长度
    long bufPos = 0;
    for (int i = 0; i < typeNum; i++)
    {
        DataType type;
        int typeVal = buffer->buffer[i * 11 + 1];
        switch ((typeVal - 1) / 10)
        {
        case 0:
        {
            type.isArray = false;
            type.hasTime = false;
            break;
        }
        case 1:
        {
            type.isArray = false;
            type.hasTime = true;
            break;
        }
        case 2:
        {
            type.isArray = true;
            type.hasTime = false;
            break;
        }
        case 3:
        {
            type.isArray = true;
            type.hasTime = true;
            break;
        }
        default:
            break;
        }
        type.valueType = (ValueType::ValueType)((typeVal - 1) % 10 + 1);
        type.valueBytes = DataType::GetValueBytes(type.valueType);
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        int sum = 0;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            char res[4] = {0};
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                sum += converter.ToInt16(val);
            }
            for (int k = 0; k < 4; k++)
            {
                res[3 - k] |= sum;
                sum >>= 8;
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UINT:
        {
            char res[4] = {0};
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                sum += converter.ToUInt16(val);
            }
            for (int k = 0; k < 4; k++)
            {
                res[3 - k] |= sum;
                sum >>= 8;
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::DINT:
        {
            char res[4] = {0};
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                sum += converter.ToInt32(val);
            }
            for (int k = 0; k < 4; k++)
            {
                res[3 - k] |= sum;
                sum >>= 8;
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            char res[4] = {0};
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                sum += converter.ToUInt32(val);
            }
            for (int k = 0; k < 4; k++)
            {
                res[3 - k] |= sum;
                sum >>= 8;
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float floatSum = 0;
            char res[4] = {0};
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                floatSum += converter.ToFloat(val);
            }
            memcpy(newBuffer + newBufCur, &floatSum, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            char res[4] = {0};
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                sum += converter.ToUInt32(val);
            }
            for (int k = 0; k < 4; k++)
            {
                res[3 - k] |= sum;
                sum >>= 8;
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char res[4] = {0};
            char value;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                sum += value;
            }
            for (int k = 0; k < 4; k++)
            {
                res[3 - k] |= sum;
                sum >>= 8;
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    for (int i = 0; i < typeList.size(); i++)
    {
        if (typeList[i].valueType != ValueType::REAL)
            typeList[i].valueType = ValueType::DINT; //可能超出32位数字表示范围，不是浮点数暂时统一用DINT表示
    }
    // CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，对单个或多个非数组变量各自求平均值
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_AVG(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::SetTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
    for (int i = 0; i < typeNum; i++)
    {
        DataType type;
        int typeVal = buffer->buffer[i * 11 + 1];
        switch ((typeVal - 1) / 10)
        {
        case 0:
        {
            type.isArray = false;
            type.hasTime = false;
            break;
        }
        case 1:
        {
            type.isArray = false;
            type.hasTime = true;
            break;
        }
        case 2:
        {
            type.isArray = true;
            type.hasTime = false;
            break;
        }
        case 3:
        {
            type.isArray = true;
            type.hasTime = true;
            break;
        }
        default:
            break;
        }
        type.valueType = (ValueType::ValueType)((typeVal - 1) % 10 + 1);
        type.valueBytes = DataType::GetValueBytes(type.valueType);
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    // memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        float sum = 0;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                sum += converter.ToInt16(val);
            }
            float res = sum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UINT:
        {
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                sum += converter.ToUInt16(val);
            }
            float res = sum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::DINT:
        {
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                sum += converter.ToInt32(val);
            }
            float res = sum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                sum += converter.ToUInt32(val);
            }
            float res = sum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float floatSum = 0;
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                floatSum += converter.ToFloat(val);
            }
            float res = floatSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            char val[4];
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 4, 4);
                sum += converter.ToUInt32(val);
            }
            float res = sum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char value;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                sum += value;
            }
            float res = sum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    for (int i = 0; i < typeList.size(); i++)
    {
        if (typeList[i].valueType != ValueType::REAL)
            typeList[i].valueType = ValueType::REAL; //均值统一用浮点数表示
    }
    CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，对单个或多个非数组变量各自求计数
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_COUNT(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::SetTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
    for (int i = 0; i < typeNum; i++)
    {
        DataType type;
        int typeVal = buffer->buffer[i * 11 + 1];
        switch ((typeVal - 1) / 10)
        {
        case 0:
        {
            type.isArray = false;
            type.hasTime = false;
            break;
        }
        case 1:
        {
            type.isArray = false;
            type.hasTime = true;
            break;
        }
        case 2:
        {
            type.isArray = true;
            type.hasTime = false;
            break;
        }
        case 3:
        {
            type.isArray = true;
            type.hasTime = true;
            break;
        }
        default:
            break;
        }
        type.valueType = (ValueType::ValueType)((typeVal - 1) % 10 + 1);
        type.valueBytes = DataType::GetValueBytes(type.valueType);
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    // memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    char res[4] = {0};
    for (int k = 0; k < 4; k++)
    {
        res[3 - k] |= rows;
        rows >>= 8;
    }
    memcpy(newBuffer + newBufCur, res, 4);
    DataTypeConverter converter;
    cout << "count:" << converter.ToUInt32(res) << endl;
    free(buffer->buffer);
    buffer->buffer = NULL;
    for (int i = 0; i < typeList.size(); i++)
    {
        if (typeList[i].valueType != ValueType::UDINT)
            typeList[i].valueType = ValueType::UDINT; //计数统一用32位无符号数表示
    }
    CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，对单个或多个非数组变量各自求标准差
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_STD(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::SetTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
    for (int i = 0; i < typeNum; i++)
    {
        DataType type;
        int typeVal = buffer->buffer[i * 11 + 1];
        switch ((typeVal - 1) / 10)
        {
        case 0:
        {
            type.isArray = false;
            type.hasTime = false;
            break;
        }
        case 1:
        {
            type.isArray = false;
            type.hasTime = true;
            break;
        }
        case 2:
        {
            type.isArray = true;
            type.hasTime = false;
            break;
        }
        case 3:
        {
            type.isArray = true;
            type.hasTime = true;
            break;
        }
        default:
            break;
        }
        type.valueType = (ValueType::ValueType)((typeVal - 1) % 10 + 1);
        type.valueBytes = DataType::GetValueBytes(type.valueType);
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    // memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        float sum = 0;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            char val[2];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToInt16(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);

            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UINT:
        {
            char val[2];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt16(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::DINT:
        {
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToInt32(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt32(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);
            cout << "std:" << res << endl;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float floatSum = 0;
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt16(val);
                floatSum += value;
                vals.push_back(value);
            }
            float avg = floatSum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt32(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char value;
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrtf(sqrSum / (float)rows);
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    for (int i = 0; i < typeList.size(); i++)
    {
        if (typeList[i].valueType != ValueType::REAL)
            typeList[i].valueType = ValueType::REAL; //标准差统一用浮点数表示
    }
    CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 根据自定义查询请求参数，对单个或多个非数组变量各自求方差
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  暂不支持带有图片或数组的多变量聚合
 */
int DB_STDEV(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    //检查是否有图片或数组
    TemplateManager::CheckTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    DB_ExecuteQuery(buffer, params);
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
    for (int i = 0; i < typeNum; i++)
    {
        DataType type;
        int typeVal = buffer->buffer[i * 11 + 1];
        switch ((typeVal - 1) / 10)
        {
        case 0:
        {
            type.isArray = false;
            type.hasTime = false;
            break;
        }
        case 1:
        {
            type.isArray = false;
            type.hasTime = true;
            break;
        }
        case 2:
        {
            type.isArray = true;
            type.hasTime = false;
            break;
        }
        case 3:
        {
            type.isArray = true;
            type.hasTime = true;
            break;
        }
        default:
            break;
        }
        type.valueType = (ValueType::ValueType)((typeVal - 1) % 10 + 1);
        type.valueBytes = DataType::GetValueBytes(type.valueType);
        typeList.push_back(type);
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    long startPos = typeNum * 11 + 1;                       //数据区起始位置
    long rows = (buffer->length - startPos) / recordLength; //获取行数
    long cur = startPos;                                    //在buffer中的偏移量
    char *newBuffer = (char *)malloc(recordLength + startPos);
    buffer->length = startPos + recordLength;
    // memcpy(newBuffer, buffer->buffer, startPos);
    long newBufCur = startPos; //在新缓冲区中的偏移量
    for (int i = 0; i < typeNum; i++)
    {
        cur = startPos + getBufferDataPos(typeList, i);
        int bytes = typeList[i].valueBytes; //此类型的值字节数
        char column[bytes * rows];          //每列的所有值缓存
        long colPos = 0;                    //在column中的偏移量
        for (int j = 0; j < rows; j++)
        {
            memcpy(column + colPos, buffer->buffer + cur, bytes);
            colPos += bytes;
            cur += recordLength;
        }
        DataTypeConverter converter;
        float sum = 0;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            char val[2];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToInt16(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            cout << "stdev:" << res << endl;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UINT:
        {
            char val[2];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt16(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::DINT:
        {
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToInt32(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt32(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float floatSum = 0;
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt16(val);
                floatSum += value;
                vals.push_back(value);
            }
            float avg = floatSum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                memcpy(val, column + k * 2, 2);
                float value = (float)converter.ToUInt32(val);
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::SINT:
        {
            char value;
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                value = column[k];
                sum += value;
                vals.push_back(value);
            }
            float avg = sum / (float)rows;
            float sqrSum = 0;
            for (auto &v : vals)
            {
                sqrSum += powf(v - avg, 2);
            }
            float res = sqrSum / (float)rows;
            memcpy(newBuffer + newBufCur, &res, 4);
            newBufCur += 4;
            break;
        }
        default:
            break;
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    for (int i = 0; i < typeList.size(); i++)
    {
        if (typeList[i].valueType != ValueType::REAL)
            typeList[i].valueType = ValueType::REAL; //方差统一用浮点数表示
    }
    CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    buffer->buffer = newBuffer;
    return 0;
}

/**
 * @brief 按条件统计正常文件条数
 *
 * @param params  查询请求参数
 * @param count  计数值
 * @return statuscode
 */
int DB_GetNormalDataCount(DB_QueryParams *params, long *count)
{
    long normal = 0;
    vector<string> packFiles, dataFiles;
    vector<pair<string, long>> selectedPacks, dataWithTime;
    switch (params->queryType)
    {
    case TIMESPAN:
    {
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &file : dataWithTime)
        {
            struct stat fileInfo;
            if (stat(file.first.c_str(), &fileInfo) == -1)
            {
                continue;
            }
            if (S_ISREG(fileInfo.st_mode)) //是常规文件
            {
                if (file.second >= params->start && file.second <= params->end)
                {
                    if (fileInfo.st_size == 0)
                    {
                        normal++;
                        continue;
                    }
                    if (file.first.back() != 'p')
                    {
                        DB_DataBuffer buffer;
                        buffer.savePath = file.first.c_str();
                        if (DB_ReadFile(&buffer) == 0)
                        {
                            if (IsNormalIDBFile(buffer.buffer, params->pathToLine))
                                normal++;
                            free(buffer.buffer);
                        }
                    }
                }
            }
        }
        for (auto &file : packFiles)
        {
            string tmp = file;
            while (tmp.back() == '/')
                tmp.pop_back();
            vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
            string packName = vec[vec.size() - 1];
            vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
            if (timespan.size() > 0)
            {
                long start = atol(timespan[0].c_str());
                long end = atol(timespan[1].c_str());
                if ((start < params->start && end >= params->start) || (start < params->end && end >= params->start) || (start <= params->start && end >= params->end) || (start >= params->start && end <= params->end)) //落入或部分落入时间区间
                {
                    selectedPacks.push_back(make_pair(file, start));
                }
            }
        }
        for (auto &pack : selectedPacks)
        {
            PackFileReader packReader(pack.first);
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            for (int i = 0; i < fileNum; i++)
            {
                long timestamp;
                int zipType, readLength;
                long dataPos = packReader.Next(readLength, timestamp, zipType);
                if (timestamp >= params->start && timestamp <= params->end && zipType != 2)
                {
                    if (zipType == 0)
                    {
                        char buff[readLength];
                        memcpy(buff, packReader.packBuffer + dataPos, readLength);
                        if (IsNormalIDBFile(buff, params->pathToLine))
                        {
                            normal++;
                        }
                    }
                    else
                        normal++;
                }
            }
        }
        *count = normal;
        break;
    }
    case LAST:
    {
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        sortByTime(dataWithTime, TIME_DSC);
        long i;
        for (i = 0; i < params->queryNums && i < dataWithTime.size(); i++)
        {
            struct stat fileInfo;
            if (stat(dataWithTime[i].first.c_str(), &fileInfo) == -1)
            {
                continue;
            }
            if (S_ISREG(fileInfo.st_mode)) //是常规文件
            {
                if (fileInfo.st_size == 0)
                {
                    normal++;
                    continue;
                }
                if (dataWithTime[i].first.back() != 'p')
                {
                    DB_DataBuffer buffer;
                    buffer.savePath = dataWithTime[i].first.c_str();
                    if (DB_ReadFile(&buffer) == 0)
                    {
                        if (IsNormalIDBFile(buffer.buffer, params->pathToLine))
                            normal++;
                        free(buffer.buffer);
                    }
                }
            }
        }
        if (i == params->queryNums)
        {
            *count = normal;
            return 0;
        }
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &&file : packFiles)
        {
            string tmp = file;
            while (tmp.back() == '/')
                tmp.pop_back();
            vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
            string packName = vec[vec.size() - 1];
            vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
            if (timespan.size() > 0)
            {
                long start = atol(timespan[0].c_str());
                selectedPacks.push_back(make_pair(file, start));
            }
        }
        sortByTime(selectedPacks, TIME_DSC);
        for (auto &&pack : selectedPacks)
        {
            PackFileReader packReader(pack.first);
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            if (i + fileNum < params->queryNums)
            {
                i += fileNum;
                for (int j = 0; j < fileNum; j++)
                {
                    long timestamp;
                    int zipType, readLength;
                    long dataPos = packReader.Next(readLength, timestamp, zipType);
                    if (zipType != 2)
                    {
                        if (zipType == 0)
                        {
                            char buff[readLength];
                            memcpy(buff, packReader.packBuffer + dataPos, readLength);
                            if (IsNormalIDBFile(buff, params->pathToLine))
                            {
                                normal++;
                            }
                        }
                        else
                            normal++;
                    }
                }
            }
            else
            {
                long timestamp;
                int zipType, readLength;
                packReader.Skip(fileNum - (params->queryNums - i));
                for (int j = fileNum - (params->queryNums - i); j < fileNum; j++)
                {
                    long dataPos = packReader.Next(readLength, timestamp, zipType);
                    if (zipType != 2)
                    {
                        if (zipType == 0)
                        {
                            char buff[readLength];
                            memcpy(buff, packReader.packBuffer + dataPos, readLength);
                            if (IsNormalIDBFile(buff, params->pathToLine))
                            {
                                normal++;
                            }
                        }
                        else
                            normal++;
                    }
                }
            }
        }
        *count = normal;
        break;
    }
    case FILEID:
    {
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
        break;
    }
    default:
    {
        readDataFiles(params->pathToLine, dataFiles);
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &file : dataFiles)
        {
            struct stat fileInfo;
            string finalPath = settings("Filename_Label") + "/" + file;
            if (stat(finalPath.c_str(), &fileInfo) == -1)
            {
                continue;
            }
            if (S_ISREG(fileInfo.st_mode)) //是常规文件
            {
                if (fileInfo.st_size == 0)
                {
                    normal++;
                    continue;
                }
                if (file.back() != 'p')
                {
                    DB_DataBuffer buffer;
                    buffer.savePath = file.c_str();
                    if (DB_ReadFile(&buffer) == 0)
                    {
                        if (IsNormalIDBFile(buffer.buffer, params->pathToLine))
                            normal++;
                        free(buffer.buffer);
                    }
                }
            }
        }
        for (auto &pack : packFiles)
        {
            PackFileReader packReader(pack);
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            for (int i = 0; i < fileNum; i++)
            {
                long timestamp;
                int zipType, readLength;
                long dataPos = packReader.Next(readLength, timestamp, zipType);
                if (zipType != 2)
                {
                    if (zipType == 0)
                    {
                        char buff[readLength];
                        memcpy(buff, packReader.packBuffer + dataPos, readLength);
                        if (IsNormalIDBFile(buff, params->pathToLine))
                        {
                            normal++;
                        }
                    }
                    else
                        normal++;
                }
            }
        }
        *count = normal;
    }
    }
    return 0;
}

/**
 * @brief 按条件统计非正常文件条数
 *
 * @param params  查询请求参数
 * @param count  计数值
 * @return statuscode
 */
int DB_GetAbnormalDataCount(DB_QueryParams *params, long *count)
{
    long abnormal = 0;
    vector<string> packFiles, dataFiles;
    vector<pair<string, long>> selectedPacks, dataWithTime;
    switch (params->queryType)
    {
    case TIMESPAN:
    {
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &file : dataWithTime)
        {
            struct stat fileInfo;
            string finalPath = settings("Filename_Label") + "/" + file.first;
            if (stat(finalPath.c_str(), &fileInfo) == -1)
            {
                continue;
            }
            if (S_ISREG(fileInfo.st_mode)) //是常规文件
            {
                if (file.second >= params->start && file.second <= params->end)
                {
                    if (file.first.back() != 'p') // is .idb
                    {
                        DB_DataBuffer buffer;
                        buffer.savePath = file.first.c_str();
                        if (DB_ReadFile(&buffer) == 0)
                        {
                            if (!IsNormalIDBFile(buffer.buffer, params->pathToLine))
                                abnormal++;
                            free(buffer.buffer);
                        }
                    }
                    else
                    {
                        if (fileInfo.st_size != 0)
                            abnormal++;
                    }
                }
            }
        }
        for (auto &file : packFiles)
        {
            string tmp = file;
            while (tmp.back() == '/')
                tmp.pop_back();
            vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
            string packName = vec[vec.size() - 1];
            vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
            if (timespan.size() > 0)
            {
                long start = atol(timespan[0].c_str());
                long end = atol(timespan[1].c_str());
                if ((start < params->start && end >= params->start) || (start < params->end && end >= params->start) || (start <= params->start && end >= params->end) || (start >= params->start && end <= params->end)) //落入或部分落入时间区间
                {
                    selectedPacks.push_back(make_pair(file, start));
                }
            }
        }
        for (auto &pack : selectedPacks)
        {
            PackFileReader packReader(pack.first);
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            for (int i = 0; i < fileNum; i++)
            {
                long timestamp;
                int zipType, readLength;
                long dataPos = packReader.Next(readLength, timestamp, zipType);
                if (timestamp >= params->start && timestamp <= params->end && zipType != 1)
                {
                    if (zipType == 0)
                    {
                        char buff[readLength];
                        memcpy(buff, packReader.packBuffer + dataPos, readLength);
                        if (!IsNormalIDBFile(buff, params->pathToLine))
                        {
                            abnormal++;
                        }
                    }
                    else
                        abnormal++;
                }
            }
        }
        *count = abnormal;
        break;
    }
    case LAST:
    {
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        sortByTime(dataWithTime, TIME_DSC);
        long i;
        for (i = 0; i < params->queryNums && i < dataWithTime.size(); i++)
        {
            struct stat fileInfo;
            string finalPath = settings("Filename_Label") + "/" + dataWithTime[i].first;
            if (stat(finalPath.c_str(), &fileInfo) == -1)
            {
                continue;
            }
            if (S_ISREG(fileInfo.st_mode)) //是常规文件
            {
                if (dataWithTime[i].first.back() != 'p') // is .idb
                {
                    DB_DataBuffer buffer;
                    buffer.savePath = dataWithTime[i].first.c_str();
                    if (DB_ReadFile(&buffer) == 0)
                    {
                        if (!IsNormalIDBFile(buffer.buffer, params->pathToLine))
                            abnormal++;
                        free(buffer.buffer);
                    }
                }
                else
                {
                    if (fileInfo.st_size != 0)
                        abnormal++;
                }
            }
        }
        if (i == params->queryNums)
        {
            *count = abnormal;
            return 0;
        }
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &&file : packFiles)
        {
            string tmp = file;
            while (tmp.back() == '/')
                tmp.pop_back();
            vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
            string packName = vec[vec.size() - 1];
            vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
            if (timespan.size() > 0)
            {
                long start = atol(timespan[0].c_str());
                selectedPacks.push_back(make_pair(file, start));
            }
        }
        sortByTime(selectedPacks, TIME_DSC);
        for (auto &&pack : selectedPacks)
        {
            PackFileReader packReader(pack.first);
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            if (i + fileNum < params->queryNums)
            {
                i += fileNum;
                for (int j = 0; j < fileNum; j++)
                {
                    long timestamp;
                    int zipType, readLength;
                    long dataPos = packReader.Next(readLength, timestamp, zipType);
                    if (zipType != 1)
                    {
                        if (zipType == 0)
                        {
                            char buff[readLength];
                            memcpy(buff, packReader.packBuffer + dataPos, readLength);
                            if (!IsNormalIDBFile(buff, params->pathToLine))
                            {
                                abnormal++;
                            }
                        }
                        else
                            abnormal++;
                    }
                }
            }
            else
            {
                long timestamp;
                int zipType, readLength;
                packReader.Skip(fileNum - (params->queryNums - i));
                for (int j = fileNum - (params->queryNums - i); j < fileNum; j++)
                {
                    long dataPos = packReader.Next(readLength, timestamp, zipType);
                    if (zipType != 1)
                    {
                        if (zipType == 0)
                        {
                            char buff[readLength];
                            memcpy(buff, packReader.packBuffer + dataPos, readLength);
                            if (!IsNormalIDBFile(buff, params->pathToLine))
                            {
                                abnormal++;
                            }
                        }
                        else
                            abnormal++;
                    }
                }
            }
        }
        *count = abnormal;
        break;
    }
    case FILEID:
    {
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
        break;
    }
    default:
    {
        readDataFiles(params->pathToLine, dataFiles);
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &file : dataFiles)
        {
            struct stat fileInfo;
            string finalPath = settings("Filename_Label") + "/" + file;
            if (stat(finalPath.c_str(), &fileInfo) == -1)
            {
                continue;
            }
            if (S_ISREG(fileInfo.st_mode)) //是常规文件
            {
                if (file.back() != 'p') // is .idb
                {
                    DB_DataBuffer buffer;
                    buffer.savePath = file.c_str();
                    if (DB_ReadFile(&buffer) == 0)
                    {
                        if (!IsNormalIDBFile(buffer.buffer, params->pathToLine))
                            abnormal++;
                        free(buffer.buffer);
                    }
                }
                else
                {
                    if (fileInfo.st_size != 0)
                        abnormal++;
                }
            }
        }
        for (auto &pack : packFiles)
        {
            PackFileReader packReader(pack);
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            for (int i = 0; i < fileNum; i++)
            {
                long timestamp;
                int zipType, readLength;
                long dataPos = packReader.Next(readLength, timestamp, zipType);
                if (zipType != 1)
                {
                    if (zipType == 0)
                    {
                        char buff[readLength];
                        memcpy(buff, packReader.packBuffer + dataPos, readLength);
                        if (!IsNormalIDBFile(buff, params->pathToLine))
                        {
                            abnormal++;
                        }
                    }
                    else
                        abnormal++;
                }
            }
        }
        *count = abnormal;
    }
    }
    return 0;
}
int main()
{
    DB_QueryParams params;
    params.pathToLine = "JinfeiSixteen";
    params.start = 1649897531555;
    params.end = 1649901032603;
    params.queryType = QRY_NONE;
    params.queryNums = 10;
    long count;
    DB_GetAbnormalDataCount(&params, &count);
    count=0;
    DB_GetNormalDataCount(&params, &count);
    return 0;
}