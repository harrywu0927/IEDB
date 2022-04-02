
#include "../include/utils.hpp"
// int DB_MAX(DB_DataBuffer *buffer, DB_QueryParams *params)
// {
//     TemplateManager::SetTemplate(params->pathToLine);
//     //检查是否有图片或数组
//     if(CurrentTemplate.checkHasArray(params->pathCode)) return StatusCode::QUERY_TYPE_NOT_SURPPORT;
//     cout<<EDVDB_ExecuteQuery(buffer, params)<<endl;
//     cout<<"buffer length"<<buffer->length<<endl;
//     int typeNum = buffer->buffer[0];
//     vector<DataType> typeList;
//     int recordLength = 0; //每行的长度
//     long bufPos = 0;
//     for (int i = 0; i < typeNum; i++)
//     {
//         DataType type;
//         int typeVal = buffer->buffer[i * 11 + 1];
//         switch ((typeVal - 1) / 10)
//         {
//         case 0:
//         {
//             type.isArray = false;
//             type.hasTime = false;
//             break;
//         }
//         case 1:
//         {
//             type.isArray = false;
//             type.hasTime = true;
//             break;
//         }
//         case 2:
//         {
//             type.isArray = true;
//             type.hasTime = false;
//             break;
//         }
//         case 3:
//         {
//             type.isArray = true;
//             type.hasTime = true;
//             break;
//         }
//         default:
//             break;
//         }
//         type.valueType = (ValueType::ValueType)((typeVal - 1) % 10 + 1);
//         type.valueBytes = DataType::GetValueBytes(type.valueType);
//         typeList.push_back(type);
//         recordLength += type.valueBytes + (type.hasTime ? 8 : 0);
//     }
//     long startPos = typeNum * 11 + 1;                       //数据区起始位置
//     long rows = (buffer->length - startPos) / recordLength; //获取行数
//     long cur = startPos;                                    //在buffer中的偏移量
//     char *newBuffer = (char *)malloc(recordLength + startPos);
//     buffer->length = startPos + recordLength;
//     memcpy(newBuffer, buffer->buffer, startPos);
//     long newBufCur = startPos; //在新缓冲区中的偏移量
//     for (int i = 0; i < typeNum; i++)
//     {
//         //cur = curs[i];
//         cout<<"no array"<<endl;
//         cur = startPos + getBufferDataPos(typeList, i);
//         int bytes = typeList[i].valueBytes; //此类型的值字节数
//         char column[bytes * rows];          //每列的所有值缓存
//         long colPos = 0;                    //在column中的偏移量
//         for (int j = 0; j < rows; j++)
//         {
//             memcpy(column + colPos, buffer->buffer + cur, bytes);
//             colPos += bytes;
//             cur += recordLength;
//         }
//         DataTypeConverter converter;
//         switch (typeList[i].valueType)
//         {
//         case ValueType::INT:
//         {
//             short max = INT16_MIN;
//             char val[2];
//             for (int k = 0; k < rows; k++)
//             {
//                 memcpy(val, column + k * 2, 2);
//                 short value = converter.ToInt16(val);
//                 if (max < value)
//                     max = value;
//             }
//             cout << "max:" << max << endl;
//             memcpy(newBuffer + newBufCur, &max, 2);
//             newBufCur += 2;
//             break;
//         }
//         case ValueType::UINT:
//         {
//             uint16_t max = 0;
//             char val[2];
//             for (int k = 0; k < rows; k++)
//             {
//                 memcpy(val, column + k * 2, 2);
//                 short value = converter.ToUInt16(val);
//                 if (max < value)
//                     max = value;
//             }
//             cout << "max:" << max << endl;
//             memcpy(newBuffer + newBufCur, val, 2);
//             newBufCur += 2;
//             break;
//         }
//         case ValueType::DINT:
//         {
//             int max = INT_MIN;
//             char val[4];
//             for (int k = 0; k < rows; k++)
//             {
//                 memcpy(val, column + k * 4, 4);
//                 short value = converter.ToInt32(val);
//                 if (max < value)
//                     max = value;
//             }
//             cout << "max:" << max << endl;
//             memcpy(newBuffer + newBufCur, val, 4);
//             newBufCur += 4;
//             break;
//         }
//         case ValueType::UDINT:
//         {
//             uint max = 0;
//             char val[4];
//             for (int k = 0; k < rows; k++)
//             {
//                 memcpy(val, column + k * 4, 4);
//                 short value = converter.ToUInt32(val);
//                 if (max < value)
//                     max = value;
//             }
//             cout << "max:" << max << endl;
//             memcpy(newBuffer + newBufCur, val, 4);
//             newBufCur += 4;
//             break;
//         }
//         case ValueType::REAL:
//         {
//             float max = __FLT_MIN__;
//             char val[4];
//             for (int k = 0; k < rows; k++)
//             {
//                 memcpy(val, column + k * 4, 4);
//                 short value = converter.ToFloat(val);
//                 if (max < value)
//                     max = value;
//             }
//             cout << "max:" << max << endl;
//             memcpy(newBuffer + newBufCur, val, 4);
//             newBufCur += 4;
//             break;
//         }
//         case ValueType::TIME:
//         {
//             int max = INT_MIN;
//             char val[4];
//             for (int k = 0; k < rows; k++)
//             {
//                 memcpy(val, column + k * 4, 4);
//                 short value = converter.ToInt32(val);
//                 if (max < value)
//                     max = value;
//             }
//             cout << "max:" << max << endl;
//             memcpy(newBuffer + newBufCur, val, 4);
//             newBufCur += 4;
//             break;
//         }
//         case ValueType::SINT:
//         {
//             char max = INT8_MIN;
//             char value;
//             for (int k = 0; k < rows; k++)
//             {
//                 value = column[k];
//                 if (max < value)
//                     max = value;
//             }
//             cout << "max:" << (int)max << endl;
//             newBuffer[newBufCur++] = value;
//             break;
//         }
//         default:
//             break;
//         }
//     }
//     free(buffer->buffer);
//     buffer->buffer = NULL;
//     buffer->buffer = newBuffer;
//     return 0;
// }
// int EDVDB_MIN(DB_DataBuffer *buffer, DB_QueryParams *params)
// {
// }
// int EDVDB_SUM(DB_DataBuffer *buffer, DB_QueryParams *params)
// {
// }
// int EDVDB_AVG(DB_DataBuffer *buffer, DB_QueryParams *params)
// {
// }
// int EDVDB_COUNT(DB_DataBuffer *buffer, DB_QueryParams *params)
// {
// }
// int EDVDB_STD(DB_DataBuffer *buffer, DB_QueryParams *params)
// {
// }
// int EDVDB_STDEV(DB_DataBuffer *buffer, DB_QueryParams *params)
// {
// }
// int main(){
//     DB_DataBuffer buffer;
//     DB_QueryParams params;
//     DB_MAX(&buffer, &params);
// }