/***************************************
 * @file QueryBufferReader.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.9.3
 * @date Last Modified in 2022-11-18
 *
 * @copyright Copyright (c) 2022
 *
 ***************************************/

#include <utils.hpp>

/**
 * @brief 重载[]运算符，按下标找到行地址
 *
 * @param index 下标
 * @return char* 选定行的内存地址
 */
char *QueryBufferReader::operator[](const int &index)
{
    return GetRow(index);
    // int offset = startPos;
    // if (index >= 0 && index < rows)
    // {
    //     if (!hasIMG)
    //         offset += recordLength * index;
    //     else
    //     {
    //         for (int i = 0; i < index; i++)
    //         {
    //             for (int j = 0; j < typeList.size(); j++)
    //             {
    //                 if (typeList[j].valueType != ValueType::IMAGE)
    //                     offset += DataType::GetTypeBytes(typeList[j]);
    //                 else
    //                 {
    //                     if (!isBigEndian)
    //                     {
    //                         short length = __builtin_bswap16(*(short *)(buffer + offset));
    //                         offset += 2;
    //                         short width = __builtin_bswap16(*(short *)(buffer + offset));
    //                         offset += 2;
    //                         short channels = __builtin_bswap16(*(short *)(buffer + offset));
    //                         offset += 2 + length * width * channels;
    //                     }
    //                     else
    //                     {
    //                         short length = *(short *)(buffer + offset);
    //                         offset += 2;
    //                         short width = *(short *)(buffer + offset);
    //                         offset += 2;
    //                         short channels = *(short *)(buffer + offset);
    //                         offset += 2 + length * width * channels;
    //                     }
    //                 }
    //             }
    //         }
    //     }
    // }
    // return buffer + offset;
}

/**
 * @brief Construct a new Query Buffer Reader:: Query Buffer Reader object
 *
 * @param buffer
 * @param freeit Decide whether free or reserve the buffer when being destoryed.
 */
QueryBufferReader::QueryBufferReader(DB_DataBuffer *buffer, bool freeit)
{
    this->freeit = freeit;
    this->buffer = buffer->buffer;
    this->length = buffer->length;
    int typeNum = this->buffer[0];
    int pos = 1;
    recordLength = 0;
    rows = 0;
    // Reconstruct the info of each value
    for (int i = 0; i < typeNum; i++, pos += PATH_CODE_SIZE)
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
        if (type.valueType == ValueType::IMAGE)
            hasIMG = true;
        recordLength += DataType::GetTypeBytes(type);
    }
    startPos = pos;
    rows = (length - pos) / recordLength;
    cur = startPos;
    if (hasIMG)
    {
        rows = 0;
        while (NextRow() != NULL)
        {
            rows++;
        }
        Reset();
    }
    CheckBigEndian();
}

/**
 * @brief When you choose this to read the buffer, it means that you'll read every row in order.
 *  This function will return the memory address of the row you're going to read.
 *
 * @return char*
 */
char *QueryBufferReader::NextRow()
{
    if (cur == length)
        return NULL;
    char *res = buffer + cur;
    if (!hasIMG)
        cur += recordLength;
    else
    {
        for (int j = 0; j < typeList.size(); j++)
        {
            if (typeList[j].valueType != ValueType::IMAGE)
                cur += DataType::GetTypeBytes(typeList[j]);
            else
            {
                if (!isBigEndian)
                {
                    short length = __builtin_bswap16(*(short *)(buffer + cur));
                    cur += 2;
                    short width = __builtin_bswap16(*(short *)(buffer + cur));
                    cur += 2;
                    short channels = __builtin_bswap16(*(short *)(buffer + cur));
                    cur += 2 + length * width * channels;
                }
                else
                {
                    short length = *(short *)(buffer + cur);
                    cur += 2;
                    short width = *(short *)(buffer + cur);
                    cur += 2;
                    short channels = *(short *)(buffer + cur);
                    cur += 2 + length * width * channels;
                }
            }
        }
    }
    return res;
}

/**
 * @brief When you choose this to read the buffer, it means that you'll read every row in order.
 *  This function is suggested to be called when there is some data with uncertain length like image. It will return the memory address of the row you're going to read and tell you the length. Yet it may cause some speed loss accordingly.
 *
 * @param rowLength The length of the entire row read this time
 * @return char*
 */
char *QueryBufferReader::NextRow(int &rowLength)
{
    if (cur == length)
        return NULL;
    char *res = buffer + cur;
    if (!hasIMG)
        cur += recordLength;
    else
    {
        int rowlen = 0;
        for (int j = 0; j < typeList.size(); j++)
        {
            if (typeList[j].valueType != ValueType::IMAGE)
            {
                rowlen += DataType::GetTypeBytes(typeList[j]);
            }
            else
            {
                if (!isBigEndian)
                {
                    short length = __builtin_bswap16(*(short *)(buffer + cur + rowlen));
                    rowlen += 2;
                    short width = __builtin_bswap16(*(short *)(buffer + cur + rowlen));
                    rowlen += 2;
                    short channels = __builtin_bswap16(*(short *)(buffer + cur + rowlen));
                    rowlen += 2 + length * width * channels;
                }
                else
                {
                    short length = *(short *)(buffer + cur + rowlen);
                    rowlen += 2;
                    short width = *(short *)(buffer + cur + rowlen);
                    rowlen += 2;
                    short channels = *(short *)(buffer + cur + rowlen);
                    rowlen += 2 + length * width * channels;
                }
            }
        }
        rowLength = rowlen;
        cur += rowlen;
    }
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
        if (!hasIMG)
            offset += recordLength * index;
        else
        {
            for (int i = 0; i < index; i++)
            {
                for (int j = 0; j < typeList.size(); j++)
                {
                    if (typeList[j].valueType != ValueType::IMAGE)
                        offset += DataType::GetTypeBytes(typeList[j]);
                    else
                    {
                        if (!isBigEndian)
                        {
                            short length = __builtin_bswap16(*(short *)(buffer + offset));
                            offset += 2;
                            short width = __builtin_bswap16(*(short *)(buffer + offset));
                            offset += 2;
                            short channels = __builtin_bswap16(*(short *)(buffer + offset));
                            offset += 2 + length * width * channels;
                        }
                        else
                        {
                            short length = *(short *)(buffer + offset);
                            offset += 2;
                            short width = *(short *)(buffer + offset);
                            offset += 2;
                            short channels = *(short *)(buffer + offset);
                            offset += 2 + length * width * channels;
                        }
                    }
                }
            }
        }
    }
    return buffer + offset;
}

/**
 * @brief Get the memory address of row by the specified index.This function is called when there is some data with uncertain length like image. It will return the memory address of the row you're going to read and tell you the length. Yet it may cause some speed loss accordingly.
 *
 * @param index
 * @return char*
 */
char *QueryBufferReader::GetRow(const int &index, int &rowLength)
{
    int offset = startPos;
    if (index >= 0 && index < rows)
    {
        if (!hasIMG)
            offset += recordLength * index;
        else
        {
            int rowlen = 0;
            for (int i = 0; i < index; i++)
            {
                rowlen = 0;
                for (int j = 0; j < typeList.size(); j++)
                {
                    if (typeList[j].valueType != ValueType::IMAGE)
                    {
                        rowlen += DataType::GetTypeBytes(typeList[j]);
                    }
                    else
                    {
                        if (!isBigEndian)
                        {
                            short length = __builtin_bswap16(*(short *)(buffer + offset + rowlen));
                            rowlen += 2;
                            short width = __builtin_bswap16(*(short *)(buffer + offset + rowlen));
                            rowlen += 2;
                            short channels = __builtin_bswap16(*(short *)(buffer + offset + rowlen));
                            rowlen += 2 + length * width * channels;
                        }
                        else
                        {
                            short length = *(short *)(buffer + offset + rowlen);
                            rowlen += 2;
                            short width = *(short *)(buffer + offset + rowlen);
                            rowlen += 2;
                            short channels = *(short *)(buffer + offset + rowlen);
                            rowlen += 2 + length * width * channels;
                        }
                    }
                }
                offset += rowlen;
            }
            rowLength = rowlen;
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
        if (!hasIMG)
            offset += recordLength * row;
        else
        {
            for (int i = 0; i < row; i++)
            {
                for (int j = 0; j < typeList.size(); j++)
                {
                    if (typeList[j].valueType != ValueType::IMAGE)
                        offset += DataType::GetTypeBytes(typeList[j]);
                    else
                    {
                        if (!isBigEndian)
                        {
                            short length = __builtin_bswap16(*(short *)(buffer + offset));
                            offset += 2;
                            short width = __builtin_bswap16(*(short *)(buffer + offset));
                            offset += 2;
                            short channels = __builtin_bswap16(*(short *)(buffer + offset));
                            offset += 2 + length * width * channels;
                        }
                        else
                        {
                            short length = *(short *)(buffer + offset);
                            offset += 2;
                            short width = *(short *)(buffer + offset);
                            offset += 2;
                            short channels = *(short *)(buffer + offset);
                            offset += 2 + length * width * channels;
                        }
                    }
                }
            }
        }

        for (int i = 0; i < col && i < typeList.size(); i++)
        {
            if (typeList[i].valueType != ValueType::IMAGE)
                offset += DataType::GetTypeBytes(typeList[i]);
            else
            {
                if (!isBigEndian)
                {
                    short length = __builtin_bswap16(*(short *)(buffer + offset));
                    offset += 2;
                    short width = __builtin_bswap16(*(short *)(buffer + offset));
                    offset += 2;
                    short channels = __builtin_bswap16(*(short *)(buffer + offset));
                    offset += 2 + length * width * channels;
                }
                else
                {
                    short length = *(short *)(buffer + offset);
                    offset += 2;
                    short width = *(short *)(buffer + offset);
                    offset += 2;
                    short channels = *(short *)(buffer + offset);
                    offset += 2 + length * width * channels;
                }
            }
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
        if (typeList[i].valueType != ValueType::IMAGE)
            offset += DataType::GetTypeBytes(typeList[i]);
        else
        {
            if (!isBigEndian)
            {
                short length = __builtin_bswap16(*(short *)(buffer + offset));
                offset += 2;
                short width = __builtin_bswap16(*(short *)(buffer + offset));
                offset += 2;
                short channels = __builtin_bswap16(*(short *)(buffer + offset));
                offset += 2 + length * width * channels;
            }
            else
            {
                short length = *(short *)(buffer + offset);
                offset += 2;
                short width = *(short *)(buffer + offset);
                offset += 2;
                short channels = *(short *)(buffer + offset);
                offset += 2 + length * width * channels;
            }
        }
    }
    return row + offset;
}

/**
 * @brief Get the Pathcodes object from buffer head
 *
 * @return vector<PathCode>
 */
void QueryBufferReader::GetPathcodes(vector<PathCode> &pathCodes)
{
    int pos = 1;
    vector<PathCode> res;
    for (size_t i = 0; i < typeList.size(); i++)
    {
        int typeVal = this->buffer[pos++];
        switch ((typeVal - 1) / 10)
        {
        case 0:
        {
            break;
        }
        case 1:
        {
            break;
        }
        case 2:
        {
            pos += 4;
            break;
        }
        case 3:
        {
            pos += 4;
            break;
        }
        case 4:
        {
            pos += 4;
            break;
        }
        case 5:
        {
            pos += 8;
            break;
        }
        default:
            break;
        }
        char code[PATH_CODE_SIZE];
        memcpy(code, buffer + pos, PATH_CODE_SIZE);
        pos += PATH_CODE_SIZE;
        for (size_t j = 0; j < CurrentTemplate.schemas.size(); j++)
        {
            if (memcmp(CurrentTemplate.schemas[j].first.code, code, PATH_CODE_SIZE) == 0)
            {
                pathCodes.push_back(CurrentTemplate.schemas[j].first);
                break;
            }
        }
    }
}