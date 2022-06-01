#include <utils.hpp>

/**
 * @brief 重载[]运算符，按下标找到行地址
 *
 * @param index 下标
 * @return char* 选定行的内存地址
 */
char *QueryBufferReader::operator[](const int &index)
{
    int offset = startPos;
    if (index >= 0 && index < rows)
    {
        for (int i = 0; i < index; i++)
        {
            offset += recordLength;
        }
    }
    return buffer + offset;
}

QueryBufferReader::QueryBufferReader(DB_DataBuffer *buffer)
{
    this->buffer = buffer->buffer;
    this->length = buffer->length;
    int typeNum = this->buffer[0];
    int pos = 1;
    recordLength = 0;
    rows = 0;
    for (int i = 0; i < typeNum; i++, pos += 10)
    {
        DataType type;
        int typeVal = this->buffer[pos++];
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
            type.arrayLen = *((int *)(this->buffer + pos));
            pos += 4;
            break;
        }
        case 3:
        {
            type.isArray = true;
            type.hasTime = true;
            type.isTimeseries = false;
            type.arrayLen = *((int *)(this->buffer + pos));
            pos += 4;
            break;
        }
        case 4:
        {
            type.isArray = false;
            type.hasTime = false;
            type.isTimeseries = true;
            type.tsLen = *((int *)(this->buffer + pos));
            pos += 4;
            break;
        }
        case 5:
        {
            type.isArray = true;
            type.hasTime = false;
            type.isTimeseries = true;
            type.tsLen = *((int *)(this->buffer + pos));
            pos += 4;
            type.arrayLen = *((int *)(this->buffer + pos));
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
    startPos = pos;
    rows = (length - pos) / recordLength;
    cur = startPos;
}

/**
 * @brief When you choose this to read the buffer, it means that you'll read every row in order.
 *  This function will return the memory address of the row you're going to read.
 *
 * @return char*
 */
char *QueryBufferReader::NextRow()
{
    char *res = buffer + cur;
    cur += recordLength;
    return res;
}

/**
 * @brief Get the memory address of row by the specified index
 *
 * @param index
 * @return char*
 */
char *QueryBufferReader::GetRow(const int &index)
{
    int offset = startPos;
    if (index >= 0 && index < rows)
    {
        for (int i = 0; i < index; i++)
        {
            offset += recordLength;
        }
    }
    return buffer + offset;
}

/**
 * @brief Get the element by row and col index. Note that this function is not suggested when you read a large number of data.
 *
 * @param row
 * @param col
 * @return void*
 */
void *QueryBufferReader::FindElement(const int &row, const int &col)
{
    int offset = startPos;
    if (row >= 0 && row < rows)
    {
        for (int i = 0; i < row; i++)
        {
            offset += recordLength;
        }
        for (int i = 0; i < col && i < typeList.size(); i++)
        {
            offset += DataType::GetTypeBytes(typeList[i]);
        }
    }
    return buffer + offset;
}

/**
 * @brief Get the element in specified index of the given memory address of the row.
 *
 * @param index Element index in the row
 * @param row Memory address of row
 * @return void*
 */
void *QueryBufferReader::FindElementInRow(const int &index, char *row)
{
    int offset = 0;
    for (int i = 0; i < index && i < typeList.size(); i++)
    {
        offset += DataType::GetTypeBytes(typeList[i]);
    }
    return row + offset;
}