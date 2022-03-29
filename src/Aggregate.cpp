#include "../include/STDFB_header.h"
#include "../include/utils.hpp"
int EDVDB_MAX(DataBuffer *buffer, QueryParams *params)
{
    EDVDB_ExecuteQuery(buffer, params);
    int typeNum = buffer->buffer[0];
    DataType typeList[typeNum];
    int recordLength = 0;
    for (int i = 0; i < typeNum; i++)
    {
        DataType type;
        int typeVal = buffer->buffer[i + 1];
        if (typeVal > 10) //此变量带有时间戳
        {
            type.hasTime = true;
            typeVal -= 10;
        }
        type.valueType = (ValueType::ValueType)typeVal;
        type.valueBytes = DataType::GetValueBytes(type.valueType);
        typeList[i] = type;
        recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
    }
    int recordsNum = (buffer->length - typeNum - 1) / recordLength;
    char *data = (char *)malloc(recordLength + typeNum + 1);
    for (auto &type : typeList)
    {
        for (int i = 0; i < recordsNum; i++)
        {
        }
    }
}
int EDVDB_MIN(DataBuffer *buffer, QueryParams *params)
{
}
int EDVDB_SUM(DataBuffer *buffer, QueryParams *params)
{
}
int EDVDB_AVG(DataBuffer *buffer, QueryParams *params)
{
}
int EDVDB_COUNT(DataBuffer *buffer, QueryParams *params)
{
}
int EDVDB_STD(DataBuffer *buffer, QueryParams *params)
{
}
int EDVDB_STDEV(DataBuffer *buffer, QueryParams *params)
{
}
// int main(){
//     DataBuffer buffer;
//     QueryParams params;
//     EDVDB_MAX(&buffer, &params);
// }