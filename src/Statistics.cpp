#include <utils.hpp>

enum STAT_Type
{
    MAX,
    MIN,
    SUM,
    PROD,
    STD,
    STDEV,
    AVG,
    MEDIAN
};

/**
 * @brief 将查询得到的buffer中的数据提取到python列表中
 *
 * @param buffer
 * @return PyObject*
 */
PyObject *ConvertToPyList_STAT(DB_DataBuffer *buffer)
{
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
    PyObject *res;
    for (int i = 0; i < typeNum; i++)
    {
        DataType type;
        char pathCode[10];
        memcpy(pathCode, buffer->buffer + i * 11 + 2, 10);
        int err = CurrentTemplate.GetDataTypeByCode(pathCode, type);
        if (err == 0)
        {
            if (type.valueType == ValueType::IMAGE)
            {
                res = PyList_New(0);
                return res;
            }
            typeList.push_back(type);
            recordLength += type.isArray ? type.valueBytes * type.arrayLen : type.valueBytes;
            recordLength += type.hasTime ? 8 : 0;
        }
    }
    int startPos = typeNum * 11 + 1;
    long rows = (buffer->length - startPos) / recordLength;
    int cur = startPos;
    res = PyList_New(rows);
    DataTypeConverter converter;
    for (int i = 0; i < rows; i++)
    {
        PyObject *row = PyList_New(0);
        for (int j = 0; j < typeList.size(); j++)
        {
            if (!typeList[j].isArray && typeList[j].valueType != ValueType::IMAGE)
            {
                long value;
                double floatvalue;
                PyObject *obj;
                switch (typeList[j].valueType)
                {
                case ValueType::INT:
                {
                    char val[2];
                    memcpy(val, buffer->buffer + cur, 2);
                    cur += typeList[j].hasTime ? 10 : 2;
                    value = converter.ToInt16(val);
                    obj = PyLong_FromLong(value);
                    PyList_Append(row, obj);
                    break;
                }
                case ValueType::UINT:
                {
                    char val[2];
                    memcpy(val, buffer->buffer + cur, 2);
                    cur += typeList[j].hasTime ? 10 : 2;
                    value = converter.ToUInt16(val);
                    obj = PyLong_FromLong(value);
                    PyList_Append(row, obj);
                    break;
                }
                case ValueType::DINT:
                {
                    char val[4];
                    memcpy(val, buffer->buffer + cur, 4);
                    cur += typeList[j].hasTime ? 12 : 4;
                    value = converter.ToInt32(val);
                    obj = PyLong_FromLong(value);
                    PyList_Append(row, obj);
                    break;
                }
                case ValueType::UDINT:
                {
                    char val[4];
                    memcpy(val, buffer->buffer + cur, 4);
                    cur += typeList[j].hasTime ? 12 : 4;
                    value = converter.ToUInt32(val);
                    obj = PyLong_FromLong(value);
                    PyList_Append(row, obj);
                    break;
                }
                case ValueType::REAL:
                {
                    char val[4];
                    memcpy(val, buffer->buffer + cur, 4);
                    cur += typeList[j].hasTime ? 12 : 4;
                    floatvalue = converter.ToFloat(val);
                    obj = PyFloat_FromDouble(value);
                    PyList_Append(row, obj);
                    break;
                }
                case ValueType::TIME:
                {
                    char val[4];
                    memcpy(val, buffer->buffer + cur, 4);
                    cur += typeList[j].hasTime ? 12 : 4;
                    value = converter.ToUInt32(val);
                    obj = PyLong_FromLong(value);
                    PyList_Append(row, obj);
                    break;
                }
                case ValueType::SINT:
                {
                    char val;
                    value = buffer->buffer[cur++];
                    cur += typeList[j].hasTime ? 8 : 0;
                    obj = PyLong_FromLong(value);
                    PyList_Append(row, obj);
                    break;
                }
                case ValueType::USINT:
                {
                    unsigned char val;
                    val = buffer->buffer[cur++];
                    value = val;
                    cur += typeList[j].hasTime ? 8 : 0;
                    obj = PyLong_FromLong(value);
                    PyList_Append(row, obj);
                    break;
                }
                default:
                    break;
                }
            }
            else
            {
                cur += typeList[j].hasTime ? typeList[j].valueBytes * typeList[j].arrayLen + 8 : typeList[j].valueBytes * typeList[j].arrayLen;
            }
        }
        PyList_SetItem(res, i, row);
    }
    return res;
}

/**
 * @brief 将查询得到的buffer中的数据提取到python列表中
 *
 * @param buffer
 * @return PyObject*
 */
PyObject *ConvertToPyList_STAT_New(DB_DataBuffer *buffer)
{
    int typeNum = buffer->buffer[0];
    vector<DataType> typeList;
    int recordLength = 0; //每行的长度
    long bufPos = 0;
    PyObject *res;
    for (int i = 0; i < typeNum; i++)
    {
        DataType type;
        char pathCode[10];
        memcpy(pathCode, buffer->buffer + i * 11 + 2, 10);
        int err = CurrentTemplate.GetDataTypeByCode(pathCode, type);
        if (err == 0)
        {
            if (type.valueType == ValueType::IMAGE)
            {
                res = PyList_New(0);
                return res;
            }
            typeList.push_back(type);
            recordLength += type.isArray ? type.valueBytes * type.arrayLen : type.valueBytes;
            recordLength += type.hasTime ? 8 : 0;
        }
    }
    int startPos = typeNum * 11 + 1;
    long rows = (buffer->length - startPos) / recordLength;
    int cur = startPos;
    res = PyList_New(rows);
    DataTypeConverter converter;
    for (int i = 0; i < rows; i++)
    {
        PyObject *row = PyList_New(0);
        for (int j = 0; j < typeList.size(); j++)
        {
            if (typeList[j].isTimeseries)
            {
                if (!typeList[j].isArray && typeList[j].valueType != ValueType::IMAGE)
                {
                    PyObject *ts = PyList_New(typeList[j].tsLen * 2);
                    for (int k = 0; k < typeList[j].tsLen; k++)
                    {
                        SetValueToPyList(ts, buffer->buffer, cur, typeList[j], k * 2);
                        long timestamp = 0;
                        memcpy(&timestamp, buffer->buffer + cur, 8);
                        PyList_SetItem(ts, k * 2 + 1, PyLong_FromLong(timestamp)); // insert timestamp to timeseries
                    }
                    PyList_Append(row, ts);
                }
                else
                {
                    cur += typeList[j].arrayLen * typeList[j].tsLen * (typeList[j].valueBytes + 8);
                }
            }
            else
            {
                if (!typeList[j].isArray && typeList[j].valueType != ValueType::IMAGE)
                {
                    AppendValueToPyList(row, buffer->buffer, cur, typeList[j]);
                }
                else
                {
                    cur += typeList[j].hasTime ? typeList[j].valueBytes * typeList[j].arrayLen + 8 : typeList[j].valueBytes * typeList[j].arrayLen;
                }
            }
        }
        PyList_SetItem(res, i, row);
    }
    return res;
}

/**
 * @brief 写入统计函数返回数组的头，为避免超过原始数据类型表示范围，统一返回double类型的数据
 *
 * @param typeNum 变量个数
 * @param newBuffer 新缓冲区
 * @param buffer 旧缓冲区
 */
void writeStatHead(int typeNum, char *newBuffer, char *buffer)
{
    memcpy(newBuffer, buffer, typeNum * 11 + 1);
    char type = 8;
    for (int i = 0; i < typeNum; i++)
    {
        memcpy(newBuffer + 1 + i * 11, &type, 1);
    }
}

/**
 * @brief 处理统计函数，调用Python程序
 *
 * @param buffer
 * @param type
 * @return int
 */
int STAT_Process(DB_DataBuffer *buffer, STAT_Type type)
{
    if (!Py_IsInitialized())
        Py_Initialize();
    if (buffer->length == 0)
        return StatusCode::NO_DATA_QUERIED;
    PyObject *arr = ConvertToPyList_STAT(buffer);
    if (PyObject_Size(arr) == 0)
    {
        free(buffer->buffer);
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
    }
    int typeNum = buffer->buffer[0];
    char *newBuffer = (char *)malloc(typeNum * 19 + 1);
    writeStatHead(typeNum, newBuffer, buffer->buffer);
    int startPos = typeNum * 11 + 1;
    // 指定py文件目录
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("if './' not in sys.path: sys.path.append('./')");
    PyObject *statistics = PyImport_ImportModule("Statistics");
    PyObject *pFunc, *pArgs, *pValue;
    if (statistics != NULL)
    {
        // 从模块中获取函数
        switch (type)
        {
        case STAT_Type::MAX:
            pFunc = PyObject_GetAttrString(statistics, "MAX");
            break;
        case STAT_Type::MIN:
            pFunc = PyObject_GetAttrString(statistics, "MIN");
            break;
        case STAT_Type::SUM:
            pFunc = PyObject_GetAttrString(statistics, "SUM");
            break;
        case STAT_Type::PROD:
            pFunc = PyObject_GetAttrString(statistics, "PROD");
            break;
        case STAT_Type::STD:
            pFunc = PyObject_GetAttrString(statistics, "STD");
            break;
        case STAT_Type::STDEV:
            pFunc = PyObject_GetAttrString(statistics, "STDEV");
            break;
        case STAT_Type::AVG:
            pFunc = PyObject_GetAttrString(statistics, "AVG");
            break;
        case STAT_Type::MEDIAN:
            pFunc = PyObject_GetAttrString(statistics, "MEDIAN");
            break;
        default:
            break;
        }
        if (pFunc && PyCallable_Check(pFunc))
        {
            // 创建参数元组
            pArgs = PyTuple_New(1);
            PyTuple_SetItem(pArgs, 0, arr);
            // 函数执行
            PyObject *ret = PyObject_CallObject(pFunc, pArgs);
            PyObject *item;
            double val;
            int len = PyObject_Size(ret);
            for (int i = 0; i < len; i++)
            {
                item = PyList_GetItem(ret, i); //根据下标取出python列表中的元素
                val = PyFloat_AsDouble(item);  //转换为c类型的数据
                cout << val << " ";
                memcpy(newBuffer + startPos + i * 8, &val, 8);
            }
        }
    }
    free(buffer->buffer);
    buffer->buffer = NULL;
    buffer->buffer = newBuffer;
    buffer->length = typeNum * 19 + 1;
    return 0;
}

/**
 * @brief 根据查询得到的缓冲区中的数据获取每列的最大值
 *
 * @param buffer
 * @return status code
 * @note 将buffer中的数据处理为python数据结构后作为参数传入相对应的函数中，获取返回值即可
 */
int DB_MAX(DB_DataBuffer *buffer)
{
    return STAT_Process(buffer, STAT_Type::MAX);
}

/**
 * @brief 根据查询得到的缓冲区中的数据获取每列的最小值
 *
 * @param buffer
 * @return status code
 * @note 将buffer中的数据处理为python数据结构后作为参数传入相对应的函数中，获取返回值即可
 */
int DB_MIN(DB_DataBuffer *buffer)
{
    return STAT_Process(buffer, STAT_Type::MIN);
}

/**
 * @brief 根据查询得到的缓冲区中的数据获取每列的标准差
 *
 * @param buffer
 * @return status code
 * @note 将buffer中的数据处理为python数据结构后作为参数传入相对应的函数中，获取返回值即可
 */
int DB_STD(DB_DataBuffer *buffer)
{
    return STAT_Process(buffer, STAT_Type::STD);
}

/**
 * @brief 根据查询得到的缓冲区中的数据获取每列的方差
 *
 * @param buffer
 * @return status code
 * @note 将buffer中的数据处理为python数据结构后作为参数传入相对应的函数中，获取返回值即可
 */
int DB_STDEV(DB_DataBuffer *buffer)
{
    return STAT_Process(buffer, STAT_Type::STDEV);
}

/**
 * @brief 根据查询得到的缓冲区中的数据获取每列的平均值
 *
 * @param buffer
 * @return status code
 * @note 将buffer中的数据处理为python数据结构后作为参数传入相对应的函数中，获取返回值即可
 */
int DB_AVG(DB_DataBuffer *buffer)
{
    return STAT_Process(buffer, STAT_Type::AVG);
}

/**
 * @brief 根据查询得到的缓冲区中的数据获取每列的中位数
 *
 * @param buffer
 * @return status code
 * @note 将buffer中的数据处理为python数据结构后作为参数传入相对应的函数中，获取返回值即可
 */
int DB_MEDIAN(DB_DataBuffer *buffer)
{
    return STAT_Process(buffer, STAT_Type::MEDIAN);
}

/**
 * @brief 根据查询得到的缓冲区中的数据对每一列求积
 *
 * @param buffer
 * @return status code
 * @note 将buffer中的数据处理为python数据结构后作为参数传入相对应的函数中，获取返回值即可
 */
int DB_PROD(DB_DataBuffer *buffer)
{
    return STAT_Process(buffer, STAT_Type::PROD);
}
/**
 * @brief 根据查询得到的缓冲区中的数据对每一列求和
 *
 * @param buffer
 * @return status code
 * @note 将buffer中的数据处理为python数据结构后作为参数传入相对应的函数中，获取返回值即可
 */
int DB_SUM(DB_DataBuffer *buffer)
{
    return STAT_Process(buffer, STAT_Type::SUM);
}
// int main()
// {
//     // Py_Initialize();
//     DB_QueryParams params;
//     params.pathToLine = "JinfeiSeven";
//     params.fileID = "JinfeiSeven1135073";
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
//     params.valueName = "S1ON";
//     // params.valueName = NULL;
//     params.start = 0;
//     params.end = 1650099030250;
//     // params.start = 1650093562902;
//     // params.end = 1650163562902;
//     params.order = ODR_NONE;
//     params.compareType = CMP_NONE;
//     params.compareValue = "666";
//     params.queryType = FILEID;
//     params.byPath = 1;
//     params.queryNums = 10;
//     DB_DataBuffer buffer;
//     DB_QueryByFileID(&buffer, &params);
//     DB_MAX(&buffer);
//     if (buffer.bufferMalloced)
//         free(buffer.buffer);
//     DB_QueryByFileID(&buffer, &params);
//     DB_MAX(&buffer);
//     Py_Finalize();
//     return 0;
// }