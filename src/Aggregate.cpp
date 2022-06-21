
#include <utils.hpp>

int maxThreads = thread::hardware_concurrency();

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
    TemplateManager::CheckTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    buffer->bufferMalloced = 0;
    if (DB_ExecuteQuery(buffer, params) != 0)
        return StatusCode::NO_DATA_QUERIED;
    if (!buffer->bufferMalloced)
        return StatusCode::NO_DATA_QUERIED;
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    int pos = 1;
    // Reconstruct the info of each value
    for (int i = 0; i < typeNum; i++, pos += 10)
    {
        DataType type;
        int typeVal = buffer->buffer[pos++];
        switch ((typeVal - 1) / 10)
        {
        case 0:
        {
            type.isArray = false;
            type.hasTime = false;
            type.isTimeseries = false;
            break;
        }
        case 1:
        {
            type.isArray = false;
            type.hasTime = true;
            type.isTimeseries = false;
            break;
        }
        case 2:
        {
            type.isArray = true;
            type.hasTime = false;
            type.isTimeseries = false;
            type.arrayLen = *((int *)(buffer->buffer + pos));
            pos += 4;
            break;
        }
        case 3:
        {
            type.isArray = true;
            type.hasTime = true;
            type.isTimeseries = false;
            type.arrayLen = *((int *)(buffer->buffer + pos));
            pos += 4;
            break;
        }
        case 4:
        {
            type.isArray = false;
            type.hasTime = false;
            type.isTimeseries = true;
            type.tsLen = *((int *)(buffer->buffer + pos));
            pos += 4;
            break;
        }
        case 5:
        {
            type.isArray = true;
            type.hasTime = false;
            type.isTimeseries = true;
            type.tsLen = *((int *)(buffer->buffer + pos));
            pos += 4;
            type.arrayLen = *((int *)(buffer->buffer + pos));
            pos += 4;
            break;
        }
        default:
            break;
        }
        type.valueType = (ValueType::ValueType)((typeVal - 1) % 10 + 1);
        type.valueBytes = DataType::GetValueBytes(type.valueType);
        typeList.push_back(type);
        recordLength += DataType::GetTypeBytes(type);
    }
    long startPos = pos;                                    //数据区起始位置
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
                // memcpy(val, column + k * 2, 2);
                // short value = converter.ToInt16(val);
                short value = __builtin_bswap16(*((short *)column + k));
                if (max < value)
                {
                    max = value;
                    memcpy(res, column + k * 2, 2);
                }
            }
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
                // memcpy(val, column + k * 2, 2);
                // unsigned short value = converter.ToUInt16(val);
                ushort value = __builtin_bswap16(*((ushort *)column + k));
                if (max < value)
                {
                    max = value;
                    memcpy(res, column + k * 2, 2);
                }
            }
            memcpy(newBuffer + newBufCur, res, 2);
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
                // memcpy(val, column + k * 4, 4);
                // int value = converter.ToInt32(val);
                int value = __builtin_bswap32(*((int *)column + k));
                if (max < value)
                {
                    max = value;
                    memcpy(res, column + k * 4, 4);
                }
            }
            memcpy(newBuffer + newBufCur, res, 4);
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
                // memcpy(val, column + k * 4, 4);
                // unsigned int value = converter.ToUInt32(val);
                uint value = __builtin_bswap32(*((uint *)column + k));
                if (max < value)
                {
                    max = value;
                    memcpy(res, column + k * 4, 4);
                }
            }
            memcpy(newBuffer + newBufCur, res, 4);
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
                // memcpy(val, column + k * 4, 4);
                // float value = converter.ToFloat(val);
                float value = __builtin_bswap32(*((float *)column + k));
                if (max < value)
                {
                    max = value;
                    memcpy(res, column + k * 4, 4);
                }
            }
            memcpy(newBuffer + newBufCur, res, 4);
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
                // memcpy(val, column + k * 4, 4);
                // int value = converter.ToInt32(val);
                int value = __builtin_bswap32(*((int *)column + k));
                if (max < value)
                {
                    max = value;
                    memcpy(res, column + k * 4, 4);
                }
            }
            memcpy(newBuffer + newBufCur, res, 4);
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
    TemplateManager::CheckTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    buffer->bufferMalloced = 0;
    if (DB_ExecuteQuery(buffer, params) != 0)
        return StatusCode::NO_DATA_QUERIED;
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
            for (int k = 0; k < rows; k++)
            {
                // memcpy(val, column + k * 2, 2);
                // short value = converter.ToInt16(val);
                short value = __builtin_bswap16(*((short *)column + k));
                if (min < value)
                {
                    min = value;
                    memcpy(res, column + k * 2, 2);
                }
            }
            memcpy(newBuffer + newBufCur, res, 2);
            newBufCur += 2;
            break;
        }
        case ValueType::UINT:
        {
            uint16_t min = UINT16_MAX;
            char res[2];
            for (int k = 0; k < rows; k++)
            {
                // memcpy(val, column + k * 2, 2);
                // unsigned short value = converter.ToUInt16(val);
                ushort value = __builtin_bswap16(*((ushort *)column + k));
                if (min < value)
                {
                    min = value;
                    memcpy(res, column + k * 2, 2);
                }
            }
            memcpy(newBuffer + newBufCur, res, 2);
            newBufCur += 2;
            break;
        }
        case ValueType::DINT:
        {
            int min = INT_MAX;
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                // memcpy(val, column + k * 4, 4);
                // int value = converter.ToInt32(val);
                int value = __builtin_bswap32(*((int *)column + k));
                if (min < value)
                {
                    min = value;
                    memcpy(res, column + k * 4, 4);
                }
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::UDINT:
        {
            uint min = UINT_MAX;
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                // memcpy(val, column + k * 4, 4);
                // unsigned int value = converter.ToUInt32(val);
                uint value = __builtin_bswap32(*((uint *)column + k));
                if (min < value)
                {
                    min = value;
                    memcpy(res, column + k * 4, 4);
                }
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::REAL:
        {
            float min = __FLT_MAX__;
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                // memcpy(val, column + k * 4, 4);
                // float value = converter.ToFloat(val);
                float value = __builtin_bswap32(*((float *)column + k));
                if (min > value)
                {
                    min = value;
                    memcpy(res, column + k * 4, 4);
                }
            }
            memcpy(newBuffer + newBufCur, res, 4);
            newBufCur += 4;
            break;
        }
        case ValueType::TIME:
        {
            int min = INT_MAX;
            char res[4];
            for (int k = 0; k < rows; k++)
            {
                // memcpy(val, column + k * 4, 4);
                // int value = converter.ToInt32(val);
                int value = __builtin_bswap32(*((int *)column + k));
                if (min > value)
                {
                    min = value;
                    memcpy(res, column + k * 4, 4);
                }
            }
            memcpy(newBuffer + newBufCur, res, 4);
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
    TemplateManager::CheckTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    buffer->bufferMalloced = 0;
    if (DB_ExecuteQuery(buffer, params) != 0)
        return StatusCode::NO_DATA_QUERIED;
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
        int sum = 0;
        switch (typeList[i].valueType)
        {
        case ValueType::INT:
        {
            char res[4] = {0};
            char val[2];
            for (int k = 0; k < rows; k++)
            {
                // memcpy(val, column + k * 2, 2);
                sum += __builtin_bswap16(*((short *)column + k));
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
                // memcpy(val, column + k * 2, 2);
                sum += __builtin_bswap16(*((ushort *)column + k));
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
                // memcpy(val, column + k * 4, 4);
                sum += __builtin_bswap32(*((int *)column + k));
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
                // memcpy(val, column + k * 4, 4);
                sum += __builtin_bswap32(*((uint *)column + k));
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
                // memcpy(val, column + k * 4, 4);
                floatSum += __builtin_bswap32(*((float *)column + k));
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
                // memcpy(val, column + k * 4, 4);
                sum += __builtin_bswap32(*((int *)column + k));
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
    if (params->byPath)
        CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    else
        CurrentTemplate.writeBufferHead(params->valueName, typeList[0], newBuffer);
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
    TemplateManager::CheckTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    buffer->bufferMalloced = 0;
    if (DB_ExecuteQuery(buffer, params) != 0)
        return StatusCode::NO_DATA_QUERIED;
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
                // memcpy(val, column + k * 2, 2);
                sum += __builtin_bswap16(*((short *)column + k));
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
                // memcpy(val, column + k * 2, 2);
                sum += __builtin_bswap16(*((ushort *)column + k));
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
                // memcpy(val, column + k * 4, 4);
                sum += __builtin_bswap32(*((int *)column + k));
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
                // memcpy(val, column + k * 4, 4);
                sum += __builtin_bswap32(*((uint *)column + k));
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
                // memcpy(val, column + k * 4, 4);
                sum += __builtin_bswap32(*((float *)column + k));
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
                // memcpy(val, column + k * 4, 4);
                sum += __builtin_bswap32(*((int *)column + k));
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
    if (params->byPath)
        CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    else
        CurrentTemplate.writeBufferHead(params->valueName, typeList[0], newBuffer);
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
    TemplateManager::CheckTemplate(params->pathToLine);
    if (params->byPath == 1 && CurrentTemplate.checkHasArray(params->pathCode))
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    buffer->bufferMalloced = 0;
    if (DB_ExecuteQuery(buffer, params) != 0)
        return StatusCode::NO_DATA_QUERIED;
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
    free(buffer->buffer);
    buffer->buffer = NULL;
    for (int i = 0; i < typeList.size(); i++)
    {
        if (typeList[i].valueType != ValueType::UDINT)
            typeList[i].valueType = ValueType::UDINT; //计数统一用32位无符号数表示
    }
    if (params->byPath)
        CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    else
        CurrentTemplate.writeBufferHead(params->valueName, typeList[0], newBuffer);
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
    buffer->bufferMalloced = 0;
    if (DB_ExecuteQuery(buffer, params) != 0)
        return StatusCode::NO_DATA_QUERIED;
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
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                // memcpy(val, column + k * 2, 2);
                float value = (float)*((short *)column + k);
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
                // memcpy(val, column + k * 2, 2);
                float value = (float)*((ushort *)column + k);
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
                // memcpy(val, column + k * 2, 2);
                float value = (float)*((int *)column + k);
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
                // memcpy(val, column + k * 2, 2);
                float value = (float)*((uint *)column + k);
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
        case ValueType::REAL:
        {
            float floatSum = 0;
            char val[4];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                // memcpy(val, column + k * 2, 2);
                float value = *((float *)column + k);
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
                // memcpy(val, column + k * 2, 2);
                float value = (float)*((int *)column + k);
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
    if (params->byPath)
        CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    else
        CurrentTemplate.writeBufferHead(params->valueName, typeList[0], newBuffer);
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
    buffer->bufferMalloced = 0;
    if (DB_ExecuteQuery(buffer, params) != 0)
        return StatusCode::NO_DATA_QUERIED;
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
                // memcpy(val, column + k * 2, 2);
                float value = (float)*((short *)column + k);
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
        case ValueType::UINT:
        {
            char val[2];
            vector<float> vals;
            for (int k = 0; k < rows; k++)
            {
                // memcpy(val, column + k * 2, 2);
                float value = (float)*((ushort *)column + k);
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
                // memcpy(val, column + k * 2, 2);
                float value = (float)*((int *)column + k);
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
                // memcpy(val, column + k * 2, 2);
                float value = (float)*((uint *)column + k);
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
                // memcpy(val, column + k * 2, 2);
                float value = *((float *)column + k);
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
                // memcpy(val, column + k * 2, 2);
                float value = (float)*((int *)column + k);
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
    if (params->byPath)
        CurrentTemplate.writeBufferHead(params->pathCode, typeList, newBuffer);
    else
        CurrentTemplate.writeBufferHead(params->valueName, typeList[0], newBuffer);
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
int DB_GetNormalDataCount_Single(DB_QueryParams *params, long *count)
{
    int err = 0;
    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
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
            string finalPath = settings("Filename_Label") + "/" + file.first;
            if (stat(finalPath.c_str(), &fileInfo) == -1)
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
        long i = 0;
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
                for (int j = fileNum - (params->queryNums - i); j < fileNum; j++, i++)
                {
                    if (i == params->queryNums)
                    {
                        *count = normal;
                        return 0;
                    }
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
 * @brief 线程任务
 *
 * @param param
 * @param pack
 * @return int
 */
int CountSinglePack_Normal(DB_QueryParams *param, pair<char *, long> pack)
{
    int normal = 0;
    switch (param->queryType)
    {
    case TIMESPAN:
    {
        PackFileReader packReader(pack.first, pack.second);
        int fileNum;
        string templateName;
        packReader.ReadPackHead(fileNum, templateName);
        for (int i = 0; i < fileNum; i++)
        {
            long timestamp;
            int zipType, readLength;
            long dataPos = packReader.Next(readLength, timestamp, zipType);
            if (timestamp >= param->start && timestamp <= param->end && zipType != 2)
            {
                if (zipType == 0)
                {
                    char buff[readLength];
                    memcpy(buff, packReader.packBuffer + dataPos, readLength);
                    if (IsNormalIDBFile(buff, param->pathToLine))
                    {
                        normal++;
                    }
                }
                else
                    normal++;
            }
        }
        break;
    }
    case LAST:
    {
        break;
    }
    case FILEID:
        break;
    default:
    {
        PackFileReader packReader(pack.first, pack.second);
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
                    if (IsNormalIDBFile(buff, param->pathToLine))
                    {
                        normal++;
                    }
                }
                else
                    normal++;
            }
        }
        break;
    }
    }
    return normal;
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
    int err = 0;
    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    long normal = 0;
    vector<string> packFiles, dataFiles;
    vector<pair<string, long>> selectedPacks, dataWithTime;
    switch (params->queryType)
    {
    case TIMESPAN:
    {
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        auto packFiles = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
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
        int index = 0;
        future_status status[maxThreads - 1];
        future<int> f[maxThreads - 1];
        for (int j = 0; j < maxThreads - 1 && index < packFiles.size(); j++)
        {
            auto pk = packManager.GetPack(packFiles[index].first);
            f[j] = async(std::launch::async, CountSinglePack_Normal, params, pk);
            status[j] = f[j].wait_for(chrono::milliseconds(1));
            index++;
        }
        while (index < packFiles.size())
        {
            for (int j = 0; j < maxThreads - 1; j++) //留一个线程循环遍历线程集，确认每个线程的运行状态
            {
                if (status[j] == future_status::ready)
                {
                    normal += f[j].get();
                    auto pk = packManager.GetPack(packFiles[index].first);
                    f[j] = async(std::launch::async, CountSinglePack_Normal, params, pk);
                    status[j] = f[j].wait_for(chrono::milliseconds(1));
                    index++;
                    if (index == packFiles.size())
                        break;
                }
                else
                {
                    status[j] = f[j].wait_for(chrono::milliseconds(1));
                }
            }
        }
        for (int j = 0; j < maxThreads - 1 && j < packFiles.size(); j++)
        {
            if (status[j] != future_status::ready)
            {
                f[j].wait();
                normal += f[j].get();
            }
            else
            {
                normal += f[j].get();
            }
        }

        *count = normal;
        break;
    }
    case LAST:
    {
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        sortByTime(dataWithTime, TIME_DSC);
        long i = 0;
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
                for (int j = fileNum - (params->queryNums - i); j < fileNum; j++, i++)
                {
                    if (i == params->queryNums)
                    {
                        *count = normal;
                        return 0;
                    }
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
        string pathToLine = params->pathToLine;
        string fileid = params->fileID;
        string fileidEnd = params->fileIDend == NULL ? "" : params->fileIDend;
        while (pathToLine[pathToLine.length() - 1] == '/')
        {
            pathToLine.pop_back();
        }

        vector<string> paths = DataType::splitWithStl(pathToLine, "/");
        if (paths.size() > 0)
        {
            if (fileid.find(paths[paths.size() - 1]) == string::npos)
                fileid = paths[paths.size() - 1] + fileid;
            if (params->fileIDend != NULL && fileidEnd.find(paths[paths.size() - 1]) == string::npos)
                fileidEnd = paths[paths.size() - 1] + fileidEnd;
        }
        else
        {
            if (fileid.find(paths[0]) == string::npos)
                fileid = paths[0] + fileid;
            if (params->fileIDend != NULL && fileidEnd.find(paths[0]) == string::npos)
                fileidEnd = paths[0] + fileidEnd;
        }
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        sortByTime(dataWithTime, TIME_ASC);
        bool firstIndexFound = false;
        string currentFileID;
        if ((params->queryNums == 1 || params->queryNums == 0) && params->fileIDend != NULL) //首尾ID方式
        {
            auto packs = packManager.GetPackByIDs(params->pathToLine, params->fileID, params->fileIDend);
            for (auto &pack : packs)
            {
                PackFileReader packReader(pack.first, pack.second);
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                for (int i = 0; i < fileNum; i++)
                {
                    int zipType, readLength;
                    long dataPos = packReader.Next(readLength, currentFileID, zipType);
                    if (fileid == currentFileID)
                        firstIndexFound = true;
                    if (firstIndexFound)
                    {
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
                    if (currentFileID == fileidEnd)
                        break;
                }
            }
            if (currentFileID != fileidEnd) //还未结束
            {
                for (auto &file : dataWithTime)
                {
                    if (!firstIndexFound)
                    {
                        vector<string> vec;
                        string tmp = fileid;
                        vec = DataType::splitWithStl(tmp, "/");
                        if (vec.size() == 0)
                            continue;
                        vec = DataType::splitWithStl(vec[vec.size() - 1], "_");
                        if (vec.size() == 0)
                            continue;
                        if (vec[0] == fileid)
                            firstIndexFound = true;
                    }
                    else
                    {
                        currentFileID = file.first;
                        if (file.first.back() != 'p') // is .idb
                        {
                            long fp, len;
                            char mode[2] = {'r', 'b'};
                            DB_Open(const_cast<char *>(file.first.c_str()), mode, &fp);
                            DB_GetFileLengthByFilePtr(fp, &len);
                            char *buff = new char[len];
                            DB_Read(fp, buff);
                            DB_Close(fp);
                            if (IsNormalIDBFile(buff, params->pathToLine))
                            {
                                normal++;
                            }
                            delete[] buff;
                        }
                        else
                        {
                            long len;
                            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                            if (len != 0)
                            {
                                normal++;
                            }
                        }
                        if (currentFileID.find(fileidEnd) != string::npos)
                            break;
                    }
                }
            }
        }
        else //首ID + 数量
        {
            auto packs = packManager.GetPackByIDs(params->pathToLine, fileid, params->queryNums);
            int scanNum = 0;
            for (auto &pack : packs)
            {
                if (pack.first != NULL && pack.second != 0)
                {
                    PackFileReader packReader(pack.first, pack.second);
                    int fileNum;
                    string templateName;
                    packReader.ReadPackHead(fileNum, templateName);
                    for (int i = 0; i < fileNum; i++)
                    {
                        if (scanNum == params->queryNums)
                            break;
                        int readLength, zipType;
                        long dataPos = packReader.Next(readLength, currentFileID, zipType);
                        if (fileid == currentFileID)
                            firstIndexFound = true;
                        if (firstIndexFound)
                        {
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
                            scanNum++;
                        }
                    }
                }
            }
            if (scanNum < params->queryNums)
            {
                for (auto &file : dataWithTime)
                {
                    if (scanNum == params->queryNums)
                        break;
                    if (!firstIndexFound)
                    {
                        vector<string> vec;
                        string tmp = fileid;
                        vec = DataType::splitWithStl(tmp, "/");
                        if (vec.size() == 0)
                            continue;
                        vec = DataType::splitWithStl(vec[vec.size() - 1], "_");
                        if (vec.size() == 0)
                            continue;
                        if (vec[0] == fileid)
                            firstIndexFound = true;
                    }
                    else
                    {
                        if (file.first.back() != 'p') // is .idb
                        {
                            long fp, len;
                            char mode[2] = {'r', 'b'};
                            DB_Open(const_cast<char *>(file.first.c_str()), mode, &fp);
                            DB_GetFileLengthByFilePtr(fp, &len);
                            char *buff = new char[len];
                            DB_Read(fp, buff);
                            DB_Close(fp);
                            if (!IsNormalIDBFile(buff, params->pathToLine))
                            {
                                normal++;
                            }
                            delete[] buff;
                        }
                        else
                        {
                            long len;
                            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                            if (len != 0)
                            {
                                normal++;
                            }
                        }
                        scanNum++;
                    }
                }
            }
        }
        *count = normal;
        break;
    }
    default:
    {
        readDataFiles(params->pathToLine, dataFiles);
        auto packFiles = packManager.allPacks[params->pathToLine];
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

        int index = 0;
        future_status status[maxThreads - 1];
        future<int> f[maxThreads - 1];
        for (int j = 0; j < maxThreads - 1 && index < packFiles.size(); j++)
        {
            auto pk = packManager.GetPack(packFiles[index].first);
            f[j] = async(std::launch::async, CountSinglePack_Normal, params, pk);
            status[j] = f[j].wait_for(chrono::milliseconds(1));
            index++;
        }
        while (index < packFiles.size())
        {
            for (int j = 0; j < maxThreads - 1; j++) //留一个线程循环遍历线程集，确认每个线程的运行状态
            {
                if (status[j] == future_status::ready)
                {
                    normal += f[j].get();
                    auto pk = packManager.GetPack(packFiles[index].first);
                    f[j] = async(std::launch::async, CountSinglePack_Normal, params, pk);
                    status[j] = f[j].wait_for(chrono::milliseconds(1));
                    index++;
                    if (index == packFiles.size())
                        break;
                }
                else
                {
                    status[j] = f[j].wait_for(chrono::milliseconds(1));
                }
            }
        }
        for (int j = 0; j < maxThreads - 1 && j < packFiles.size(); j++)
        {
            if (status[j] != future_status::ready)
            {
                f[j].wait();
                normal += f[j].get();
            }
            else
            {
                normal += f[j].get();
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
int DB_GetAbnormalDataCount_Single(DB_QueryParams *params, long *count)
{
    int err = 0;
    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
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
        long i = 0;
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
                for (int j = fileNum - (params->queryNums - i); j < fileNum; j++, i++)
                {
                    if (i == params->queryNums)
                    {
                        *count = abnormal;
                        return 0;
                    }
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

/**
 * @brief 线程任务
 *
 * @param param
 * @param pack
 * @return int
 */
int CountSinglePack_Abnormal(DB_QueryParams *param, pair<char *, long> pack)
{
    int abnormal = 0;
    switch (param->queryType)
    {
    case TIMESPAN:
    {
        PackFileReader packReader(pack.first, pack.second);
        int fileNum;
        string templateName;
        packReader.ReadPackHead(fileNum, templateName);
        for (int i = 0; i < fileNum; i++)
        {
            long timestamp;
            int zipType, readLength;
            long dataPos = packReader.Next(readLength, timestamp, zipType);
            if (timestamp >= param->start && timestamp <= param->end)
            {
                if (zipType != 1)
                {
                    if (zipType == 0)
                    {
                        char buff[readLength];
                        memcpy(buff, packReader.packBuffer + dataPos, readLength);
                        if (!IsNormalIDBFile(buff, templateName.c_str()))
                        {
                            abnormal++;
                        }
                    }
                    else
                        abnormal++;
                }
            }
        }
        break;
    }
    case LAST:
    {
        break;
    }
    case FILEID:
    {
        break;
    }
    default:
    {
        PackFileReader packReader(pack.first, pack.second);
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
                    if (!IsNormalIDBFile(buff, templateName.c_str()))
                    {
                        abnormal++;
                    }
                }
                else
                    abnormal++;
            }
        }
        break;
    }
    }
    return abnormal;
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
    int err = 0;
    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    long abnormal = 0;
    vector<string> packFiles, dataFiles;
    vector<pair<string, long>> selectedPacks, dataWithTime;
    switch (params->queryType)
    {
    case TIMESPAN:
    {
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        auto packFiles = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
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
        int index = 0;
        future_status status[maxThreads - 1];
        future<int> f[maxThreads - 1];
        for (int j = 0; j < maxThreads - 1 && index < packFiles.size(); j++)
        {
            auto pk = packManager.GetPack(packFiles[index].first);
            f[j] = async(std::launch::async, CountSinglePack_Abnormal, params, pk);
            status[j] = f[j].wait_for(chrono::milliseconds(1));
            index++;
        }
        while (index < packFiles.size())
        {
            for (int j = 0; j < maxThreads - 1; j++) //留一个线程循环遍历线程集，确认每个线程的运行状态
            {
                if (status[j] == future_status::ready)
                {
                    abnormal += f[j].get();
                    auto pk = packManager.GetPack(packFiles[index].first);
                    f[j] = async(std::launch::async, CountSinglePack_Abnormal, params, pk);
                    status[j] = f[j].wait_for(chrono::milliseconds(1));
                    index++;
                    if (index == packFiles.size())
                        break;
                }
                else
                {
                    status[j] = f[j].wait_for(chrono::milliseconds(1));
                }
            }
        }
        for (int j = 0; j < maxThreads - 1 && j < packFiles.size(); j++)
        {
            if (status[j] != future_status::ready)
            {
                f[j].wait();
                abnormal += f[j].get();
            }
            else
            {
                abnormal += f[j].get();
            }
        }

        *count = abnormal;
        break;
    }
    case LAST:
    {
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        sortByTime(dataWithTime, TIME_DSC);
        long i = 0;
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
                for (int j = fileNum - (params->queryNums - i); j < fileNum; j++, i++)
                {
                    if (i == params->queryNums)
                    {
                        *count = abnormal;
                        return 0;
                    }
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
        string pathToLine = params->pathToLine;
        string fileid = params->fileID;
        string fileidEnd = params->fileIDend == NULL ? "" : params->fileIDend;
        while (pathToLine[pathToLine.length() - 1] == '/')
        {
            pathToLine.pop_back();
        }

        vector<string> paths = DataType::splitWithStl(pathToLine, "/");
        if (paths.size() > 0)
        {
            if (fileid.find(paths[paths.size() - 1]) == string::npos)
                fileid = paths[paths.size() - 1] + fileid;
            if (params->fileIDend != NULL && fileidEnd.find(paths[paths.size() - 1]) == string::npos)
                fileidEnd = paths[paths.size() - 1] + fileidEnd;
        }
        else
        {
            if (fileid.find(paths[0]) == string::npos)
                fileid = paths[0] + fileid;
            if (params->fileIDend != NULL && fileidEnd.find(paths[0]) == string::npos)
                fileidEnd = paths[0] + fileidEnd;
        }
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        sortByTime(dataWithTime, TIME_ASC);
        bool firstIndexFound = false;
        string currentFileID;
        if ((params->queryNums == 1 || params->queryNums == 0) && params->fileIDend != NULL) //首尾ID方式
        {
            auto packs = packManager.GetPackByIDs(params->pathToLine, params->fileID, params->fileIDend);
            for (auto &pack : packs)
            {
                PackFileReader packReader(pack.first, pack.second);
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                for (int i = 0; i < fileNum; i++)
                {
                    int zipType, readLength;
                    long dataPos = packReader.Next(readLength, currentFileID, zipType);
                    if (fileid == currentFileID)
                        firstIndexFound = true;
                    if (firstIndexFound)
                    {
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
                    if (currentFileID == fileidEnd)
                        break;
                }
            }
            if (currentFileID != fileidEnd) //还未结束
            {
                for (auto &file : dataWithTime)
                {
                    if (!firstIndexFound)
                    {
                        vector<string> vec;
                        string tmp = fileid;
                        vec = DataType::splitWithStl(tmp, "/");
                        if (vec.size() == 0)
                            continue;
                        vec = DataType::splitWithStl(vec[vec.size() - 1], "_");
                        if (vec.size() == 0)
                            continue;
                        if (vec[0] == fileid)
                            firstIndexFound = true;
                    }
                    else
                    {
                        currentFileID = file.first;
                        if (file.first.back() != 'p') // is .idb
                        {
                            long fp, len;
                            char mode[2] = {'r', 'b'};
                            DB_Open(const_cast<char *>(file.first.c_str()), mode, &fp);
                            DB_GetFileLengthByFilePtr(fp, &len);
                            char *buff = new char[len];
                            DB_Read(fp, buff);
                            DB_Close(fp);
                            if (!IsNormalIDBFile(buff, params->pathToLine))
                            {
                                abnormal++;
                            }
                            delete[] buff;
                        }
                        else
                        {
                            long len;
                            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                            if (len != 0)
                            {
                                abnormal++;
                            }
                        }
                        if (currentFileID.find(fileidEnd) != string::npos)
                            break;
                    }
                }
            }
        }
        else //首ID + 数量
        {
            auto packs = packManager.GetPackByIDs(params->pathToLine, fileid, params->queryNums);
            int scanNum = 0;
            for (auto &pack : packs)
            {
                if (pack.first != NULL && pack.second != 0)
                {
                    PackFileReader packReader(pack.first, pack.second);
                    int fileNum;
                    string templateName;
                    packReader.ReadPackHead(fileNum, templateName);
                    for (int i = 0; i < fileNum; i++)
                    {
                        if (scanNum == params->queryNums)
                            break;
                        int readLength, zipType;
                        long dataPos = packReader.Next(readLength, currentFileID, zipType);
                        if (fileid == currentFileID)
                            firstIndexFound = true;
                        if (firstIndexFound)
                        {
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
                            scanNum++;
                        }
                    }
                }
            }
            if (scanNum < params->queryNums)
            {
                for (auto &file : dataWithTime)
                {
                    if (scanNum == params->queryNums)
                        break;
                    if (!firstIndexFound)
                    {
                        vector<string> vec;
                        string tmp = fileid;
                        vec = DataType::splitWithStl(tmp, "/");
                        if (vec.size() == 0)
                            continue;
                        vec = DataType::splitWithStl(vec[vec.size() - 1], "_");
                        if (vec.size() == 0)
                            continue;
                        if (vec[0] == fileid)
                            firstIndexFound = true;
                    }
                    else
                    {
                        if (file.first.back() != 'p') // is .idb
                        {
                            long fp, len;
                            char mode[2] = {'r', 'b'};
                            DB_Open(const_cast<char *>(file.first.c_str()), mode, &fp);
                            DB_GetFileLengthByFilePtr(fp, &len);
                            char *buff = new char[len];
                            DB_Read(fp, buff);
                            DB_Close(fp);
                            if (!IsNormalIDBFile(buff, params->pathToLine))
                            {
                                abnormal++;
                            }
                            delete[] buff;
                        }
                        else
                        {
                            long len;
                            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                            if (len != 0)
                            {
                                abnormal++;
                            }
                        }
                        scanNum++;
                    }
                }
            }
        }
        *count = abnormal;
        break;
    }
    default:
    {
        readDataFiles(params->pathToLine, dataFiles);
        auto packFiles = packManager.allPacks[params->pathToLine];
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
        int index = 0;
        future_status status[maxThreads - 1];
        future<int> f[maxThreads - 1];
        for (int j = 0; j < maxThreads - 1 && index < packFiles.size(); j++)
        {
            auto pk = packManager.GetPack(packFiles[index].first);
            f[j] = async(std::launch::async, CountSinglePack_Abnormal, params, pk);
            status[j] = f[j].wait_for(chrono::milliseconds(1));
            index++;
        }
        while (index < packFiles.size())
        {
            for (int j = 0; j < maxThreads - 1; j++) //留一个线程循环遍历线程集，确认每个线程的运行状态
            {
                if (status[j] == future_status::ready)
                {
                    abnormal += f[j].get();
                    auto pk = packManager.GetPack(packFiles[index].first);
                    f[j] = async(std::launch::async, CountSinglePack_Abnormal, params, pk);
                    status[j] = f[j].wait_for(chrono::milliseconds(1));
                    index++;
                    if (index == packFiles.size())
                        break;
                }
                else
                {
                    status[j] = f[j].wait_for(chrono::milliseconds(1));
                }
            }
        }
        for (int j = 0; j < maxThreads - 1 && j < packFiles.size(); j++)
        {
            if (status[j] != future_status::ready)
            {
                f[j].wait();
                abnormal += f[j].get();
            }
            else
            {
                abnormal += f[j].get();
            }
        }

        *count = abnormal;
    }
    }
    return 0;
}

/**
 * @brief 根据查询条件筛选后获取异常节拍的数据
 *
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 * @param mode 若为1，则使用LOF算法即时训练指定数据来判断数据是否异常；若为0，则根据已训练的模型检测指定数据的新颖性（异常性）；否则，根据压缩模版判断是否异常
 * @param no_query 是否组态
 * @return int
 */
int DB_GetAbnormalRhythm(DB_DataBuffer *buffer, DB_QueryParams *params, int mode, int no_query)
{
    //此处的pathcode和valuename仅仅为比较数值筛选时指定的变量，下方查询时将转为所有变量
    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    if (ZipTemplateManager::CheckZipTemplate(params->pathToLine) != 0)
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    vector<PathCode> pathCodes;
    if (!no_query)
    {
        if (params->byPath)
        {
            int err = CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
            if (err != 0)
                return err;
            if ((params->queryType != QRY_NONE || params->compareType != CMP_NONE) && pathCodes.size() > 1 && (params->valueName == NULL || strcmp(params->valueName, "") == 0)) //若此编码包含的数据类型大于1，而未指定变量名，又需要比较或排序，返回异常
                return StatusCode::INVALID_QRY_PARAMS;
            else
            {
                if ((params->valueName == NULL || strcmp(params->valueName, "") == 0) && (params->queryType != QRY_NONE || params->compareType != CMP_NONE)) //由于编码会变为全0，因此若需要排序或比较，需要添加变量名
                {
                    params->valueName = pathCodes[0].name.c_str();
                }
                char zeros[10] = {0};
                memcpy(params->pathCode, zeros, 10);
            }
        }
        else
        {
            // CurrentTemplate.GetCodeByName(params->valueName, pathCodes);
            params->byPath = 1;
            char zeros[10] = {0};
            memcpy(params->pathCode, zeros, 10);
        }
        int err = DB_ExecuteQuery(buffer, params);
        if (err != 0)
            return err;
    }
    else if (buffer->buffer == NULL)
        return StatusCode::INVALID_QUERY_BUFFER;
    QueryBufferReader reader(buffer);
    if (no_query)
    {
        reader.GetPathcodes(pathCodes);
    }
    if (mode == 1) // using machine learning
    {
        vector<int> typeIndexes;
        if (pathCodes.size() > 0)
        {
            for (int i = 0; i < pathCodes.size(); i++)
            {
                for (int j = 0; j < CurrentTemplate.schemas.size(); j++)
                {
                    if (CurrentTemplate.schemas[j].first == pathCodes[i])
                        typeIndexes.push_back(j);
                }
            }
        }
        else
        {
            for (int i = 0; i < CurrentTemplate.schemas.size(); i++)
            {
                if (strcmp(CurrentTemplate.schemas[i].first.name.c_str(), params->valueName) == 0)
                    typeIndexes.push_back(i);
            }
        }

        PyObject *table = ConvertToPyList_ML(buffer);
        char *set = new char[reader.rows]; //此数组中值为1的下标表示异常数据在查询结果中的行
        memset(set, 0, reader.rows);
        for (int i = 0; i < typeIndexes.size(); i++)
        {
            if (reader.typeList[i].valueType == ValueType::IMAGE) //图片不判断
                continue;
            PyObject *col = PyList_New(reader.rows); //逐列数据判断是否异常
            for (int j = 0; j < reader.rows; j++)
            {
                PyList_SetItem(col, j, PyList_GetItem(PyList_GetItem(table, j), typeIndexes[i]));
            }
            // PyObject *args = PyTuple_New(2);
            // PyTuple_SetItem(args, 0, col);
            PyObject *dim = PyLong_FromLong(reader.typeList[typeIndexes[i]].isArray ? reader.typeList[typeIndexes[i]].arrayLen : 1);
            // PyTuple_SetItem(args, 1, dim);
            // PyObject *ret = PythonCall(args, "Novelty_Outlier", "Outliers");
            PyObject *ret = PythonCall("Novelty_Outlier", "Outliers", "./", 2, col, dim);
            int len = PyObject_Size(ret);
            if (len == -1)
            {
                delete[] set;
                return StatusCode::NO_DATA_QUERIED;
            }
            for (int j = 0; j < len; j++)
            {
                if (PyLong_AsLong(PyList_GetItem(ret, j)) == -1)
                {
                    *(set + j) = 1;
                }
            }
            // Py_DECREF(args);
            Py_XDECREF(ret);
            // if (i == typeIndexes.size() - 1)
            //     PyObject_Free(dim);
            // int a = 1;
        }
        Py_DECREF(table);
        if (CurrentTemplate.hasImage)
        {
            vector<pair<char *, int>> abnormalData;
            long cur = reader.startPos;
            for (int i = 0; i < reader.rows; i++)
            {
                if (*(set + i) == 1)
                {
                    int rowlen;
                    char *memaddr = reader.GetRow(i, rowlen);
                    abnormalData.push_back({memaddr, rowlen});
                    cur += rowlen;
                }
            }
            char *newBuffer = (char *)malloc(cur);
            cur = reader.startPos;
            for (auto &mem : abnormalData)
            {
                memcpy(newBuffer + cur, mem.first, mem.second);
                cur += mem.second;
            }
            memcpy(newBuffer, reader.buffer, reader.startPos);
            buffer->buffer = newBuffer;
            buffer->length = cur;
        }
        else
        {
            vector<char *> abnormalData;
            long cur = reader.startPos;
            for (int i = 0; i < reader.rows; i++)
            {
                if (*(set + i) == 1)
                {
                    abnormalData.push_back(reader.GetRow(i));
                    cur += reader.recordLength;
                }
            }
            char *newBuffer = (char *)malloc(cur);
            cur = reader.startPos;
            memcpy(newBuffer, reader.buffer, reader.startPos);
            for (auto &mem : abnormalData)
            {
                memcpy(newBuffer + cur, mem, reader.recordLength);
                cur += reader.recordLength;
            }
            buffer->buffer = newBuffer;
            buffer->length = cur;
        }
        delete[] set;
    }
    else if (mode == 0)
    {
        vector<int> typeIndexes;
        if (pathCodes.size() > 0)
        {
            for (int i = 0; i < pathCodes.size(); i++)
            {
                for (int j = 0; j < CurrentTemplate.schemas.size(); j++)
                {
                    if (CurrentTemplate.schemas[j].first == pathCodes[i])
                        typeIndexes.push_back(j);
                }
            }
        }
        else
        {
            for (int i = 0; i < CurrentTemplate.schemas.size(); i++)
            {
                if (strcmp(CurrentTemplate.schemas[i].first.name.c_str(), params->valueName) == 0)
                {
                    typeIndexes.push_back(i);
                    break;
                }
            }
        }
        PyObject *table = ConvertToPyList_ML(buffer);
        char *set = new char[reader.rows]; //此数组中值为1的下标表示异常数据在查询结果中的行
        memset(set, 0, reader.rows);
        for (int i = 0; i < typeIndexes.size(); i++)
        {
            if (reader.typeList[i].valueType == ValueType::IMAGE) //图片不判断
                continue;
            PyObject *col = PyList_New(reader.rows); //逐列数据判断是否异常
            for (int j = 0; j < reader.rows; j++)
            {
                PyList_SetItem(col, j, PyList_GetItem(PyList_GetItem(table, j), typeIndexes[i]));
            }
            PyObject *args = PyTuple_New(3);
            PyTuple_SetItem(args, 0, col);
            PyObject *dim = PyLong_FromLong(reader.typeList[typeIndexes[i]].isArray ? reader.typeList[typeIndexes[i]].arrayLen : 1);
            PyTuple_SetItem(args, 1, dim);
            PyObject *name = PyBytes_FromString(CurrentTemplate.schemas[typeIndexes[i]].first.name.c_str());
            PyTuple_SetItem(args, 2, name);
            PyObject *ret = PythonCall(args, "Novelty_Outlier", "Novelty_Single_Column");

            int len = PyObject_Size(ret);
            if (len == -1)
            {
                delete[] set;
                return StatusCode::NO_DATA_QUERIED;
            }
            for (int j = 0; j < len; j++)
            {
                if (PyLong_AsLong(PyList_GetItem(ret, j)) == -1)
                {
                    *(set + j) = 1;
                }
            }
            Py_DECREF(args);
            Py_XDECREF(ret);
            // if (i == typeIndexes.size() - 1)
            //     PyObject_Free(dim);
        }
        Py_DECREF(table);
        if (CurrentTemplate.hasImage)
        {
            vector<pair<char *, int>> abnormalData;
            long cur = reader.startPos;
            for (int i = 0; i < reader.rows; i++)
            {
                if (*(set + i) == 1)
                {
                    int rowlen;
                    char *memaddr = reader.GetRow(i, rowlen);
                    abnormalData.push_back({memaddr, rowlen});
                    cur += rowlen;
                }
            }
            char *newBuffer = (char *)malloc(cur);
            cur = reader.startPos;
            for (auto &mem : abnormalData)
            {
                memcpy(newBuffer + cur, mem.first, mem.second);
                cur += mem.second;
            }
            memcpy(newBuffer, reader.buffer, reader.startPos);
            buffer->buffer = newBuffer;
            buffer->length = cur;
        }
        else
        {
            vector<char *> abnormalData;
            long cur = reader.startPos;
            for (int i = 0; i < reader.rows; i++)
            {
                if (*(set + i) == 1)
                {
                    abnormalData.push_back(reader.GetRow(i));
                    cur += reader.recordLength;
                }
            }
            char *newBuffer = (char *)malloc(cur);
            cur = reader.startPos;
            memcpy(newBuffer, reader.buffer, reader.startPos);
            for (auto &mem : abnormalData)
            {
                memcpy(newBuffer + cur, mem, reader.recordLength);
                cur += reader.recordLength;
            }
            buffer->buffer = newBuffer;
            buffer->length = cur;
        }
        delete[] set;
    }
    else // using ziptemplate
    {
        if (CurrentTemplate.hasImage)
        {
            vector<pair<char *, int>> abnormalData;
            long cur = reader.startPos;
            for (int i = 0; i < reader.rows; i++)
            {
                int rowlen = 0;
                char *row = reader.NextRow(rowlen);
                if (!IsNormalIDBFile(row, params->pathToLine))
                {
                    abnormalData.push_back({row, rowlen});
                    cur += rowlen;
                }
            }
            if (abnormalData.size() == 0)
                return StatusCode::NO_DATA_QUERIED;
            char *newBuffer = (char *)malloc(cur);
            memcpy(newBuffer, buffer->buffer, reader.startPos);
            cur = reader.startPos;
            for (auto &mem : abnormalData)
            {
                memcpy(newBuffer + cur, mem.first, mem.second);
                cur += mem.second;
            }
            memcpy(newBuffer, reader.buffer, reader.startPos);
            buffer->buffer = newBuffer;
            buffer->length = cur;
        }
        else
        {
            vector<char *> abnormalData;
            for (int i = 0; i < reader.rows; i++)
            {
                char *row = reader.NextRow();
                if (!IsNormalIDBFile(row, params->pathToLine))
                {
                    abnormalData.push_back(row);
                }
            }
            if (abnormalData.size() == 0)
                return StatusCode::NO_DATA_QUERIED;
            char *newBuffer = (char *)malloc(abnormalData.size() * reader.recordLength + reader.startPos);
            memcpy(newBuffer, buffer->buffer, reader.startPos);
            long cur = reader.startPos;
            for (auto &mem : abnormalData)
            {
                memcpy(newBuffer + cur, mem, reader.recordLength);
                cur += reader.recordLength;
            }
            memcpy(newBuffer, reader.buffer, reader.startPos);
            buffer->buffer = newBuffer;
            buffer->length = cur;
        }
    }
    return 0;
}
// int main()
// {
//     // DataTypeConverter converter;
//     DB_QueryParams params;
//     params.pathToLine = "JinfeiSeven";
//     params.fileID = "JinfeiSeven15269";
//     params.fileIDend = NULL;
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
//     params.pathCode = code;
//     params.valueName = "S1OFF";
//     // params.valueName = NULL;
//     params.start = 0;
//     params.end = 1751165600000;
//     params.order = ODR_NONE;
//     params.compareType = CMP_NONE;
//     params.compareValue = "666";
//     params.queryType = TIMESPAN;
//     params.byPath = 0;
//     params.queryNums = 40;
//     DB_DataBuffer buffer;
//     // DB_ExecuteQuery(&buffer, &params);
//     // DB_GetAbnormalRhythm(&buffer, &params, 1, 1);
//     // long count, count2;
//     // DB_GetAbnormalDataCount(&params, &count);
//     // DB_GetNormalDataCount(&params, &count2);
//     // DB_QueryByFileID(&buffer, &params);
//     // char *newbuf = (char *)malloc(212);
//     // memcpy(newbuf, buffer.buffer, 12);
//     // DataTypeConverter converter;
//     // for (int i = 0; i < 50; i++)
//     // {
//     //     uint v;
//     //     v = 95 + rand() % 5;
//     //     char buf[4];
//     //     converter.ToUInt32Buff(v, buf);
//     //     memcpy(newbuf + 12 + i * 4, buf, 4);
//     // }
//     DB_AVG(&buffer, &params);
//     if (buffer.bufferMalloced)
//     {
//         char buf[buffer.length];
//         memcpy(buf, buffer.buffer, buffer.length);
//         cout << buffer.length << endl;
//         for (int i = 0; i < buffer.length; i++)
//         {
//             cout << (int)buf[i] << " ";
//             if (i % 11 == 0)
//                 cout << endl;
//         }

//         free(buffer.buffer);
//     }
//     Py_Finalize();
//     return 0;
// }
