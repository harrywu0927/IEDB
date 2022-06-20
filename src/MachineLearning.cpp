#include <utils.hpp>

/**
 * @brief Append value in buffer to PyList object
 *
 * @param item PyList object to set
 * @param buffer Data buffer
 * @param cur Offset in buffer
 * @param type Datatype
 */
void AppendValueToPyList(PyObject *item, char *buffer, int &cur, DataType &type)
{
    long value;
    double floatvalue;
    PyObject *obj;
    DataTypeConverter converter;
    switch (type.valueType)
    {
    case ValueType::INT:
    {
        // char val[2];
        // memcpy(val, buffer + cur, 2);
        value = __builtin_bswap16(*((short *)(buffer + cur)));
        cur += type.hasTime ? 10 : 2;
        // value = converter.ToInt16(val);
        obj = PyLong_FromLong(value);
        PyList_Append(item, obj);
        break;
    }
    case ValueType::UINT:
    {
        // char val[2];
        // memcpy(val, buffer + cur, 2);
        value = __builtin_bswap16(*((ushort *)(buffer + cur)));
        cur += type.hasTime ? 10 : 2;
        // value = converter.ToUInt16(val);
        obj = PyLong_FromLong(value);
        PyList_Append(item, obj);
        break;
    }
    case ValueType::DINT:
    {
        // char val[4];
        // memcpy(val, buffer + cur, 4);
        value = __builtin_bswap32(*((int *)(buffer + cur)));
        cur += type.hasTime ? 12 : 4;
        // value = converter.ToInt32(val);
        obj = PyLong_FromLong(value);
        PyList_Append(item, obj);
        break;
    }
    case ValueType::UDINT:
    {
        // char val[4];
        // memcpy(val, buffer + cur, 4);
        value = __builtin_bswap32(*((uint *)(buffer + cur)));
        cur += type.hasTime ? 12 : 4;
        // value = converter.ToUInt32(val);
        obj = PyLong_FromLong(value);
        PyList_Append(item, obj);
        break;
    }
    case ValueType::REAL:
    {
        // char val[4];
        // memcpy(val, buffer + cur, 4);
        floatvalue = __builtin_bswap32(*((float *)(buffer + cur)));
        cur += type.hasTime ? 12 : 4;
        // floatvalue = converter.ToFloat(val);
        obj = PyFloat_FromDouble(value);
        PyList_Append(item, obj);
        break;
    }
    case ValueType::TIME:
    {
        // char val[4];
        // memcpy(val, buffer + cur, 4);
        value = __builtin_bswap32(*((int *)(buffer + cur)));
        cur += type.hasTime ? 12 : 4;
        // value = converter.ToInt32(val);
        obj = PyLong_FromLong(value);
        PyList_Append(item, obj);
        break;
    }
    case ValueType::SINT:
    {
        char val;
        value = buffer[cur++];
        cur += type.hasTime ? 8 : 0;
        obj = PyLong_FromLong(value);
        PyList_Append(item, obj);
        break;
    }
    case ValueType::USINT:
    {
        unsigned char val;
        val = buffer[cur++];
        value = val;
        cur += type.hasTime ? 8 : 0;
        obj = PyLong_FromLong(value);
        PyList_Append(item, obj);
        break;
    }
    default:
        break;
    }
}

/**
 * @brief Set the Value To Py List object
 *
 * @param item PyList object to set
 * @param buffer Data buffer
 * @param cur Offset in buffer
 * @param type Datatype
 * @param index The index of PyList object to set
 */
void SetValueToPyList(PyObject *item, char *buffer, int &cur, DataType &type, int index)
{
    long value;
    double floatvalue;
    PyObject *obj;
    DataTypeConverter converter;
    switch (type.valueType)
    {
    case ValueType::INT:
    {
        // char val[2];
        // memcpy(val, buffer + cur, 2);
        value = __builtin_bswap16(*((short *)(buffer + cur)));
        cur += type.hasTime ? 10 : 2;
        // value = converter.ToInt16(val);
        obj = PyLong_FromLong(value);
        PyList_SetItem(item, index, obj);
        break;
    }
    case ValueType::UINT:
    {
        // char val[2];
        // memcpy(val, buffer + cur, 2);
        value = __builtin_bswap16(*((ushort *)(buffer + cur)));
        cur += type.hasTime ? 10 : 2;
        // value = converter.ToInt16(val);
        obj = PyLong_FromLong(value);
        PyList_SetItem(item, index, obj);
        break;
    }
    case ValueType::DINT:
    {
        // char val[4];
        // memcpy(val, buffer + cur, 4);
        value = __builtin_bswap32(*((int *)(buffer + cur)));
        cur += type.hasTime ? 12 : 4;
        // value = converter.ToInt32(val);
        obj = PyLong_FromLong(value);
        PyList_SetItem(item, index, obj);
        break;
    }
    case ValueType::UDINT:
    {
        // char val[4];
        // memcpy(val, buffer + cur, 4);
        value = __builtin_bswap32(*((uint *)(buffer + cur)));
        cur += type.hasTime ? 12 : 4;
        // value = converter.ToInt32(val);
        obj = PyLong_FromLong(value);
        PyList_SetItem(item, index, obj);
        break;
    }
    case ValueType::REAL:
    {
        // char val[4];
        // memcpy(val, buffer + cur, 4);
        floatvalue = __builtin_bswap32(*((float *)(buffer + cur)));
        cur += type.hasTime ? 12 : 4;
        // value = converter.ToInt32(val);
        obj = PyFloat_FromDouble(value);
        PyList_SetItem(item, index, obj);
        break;
    }
    case ValueType::TIME:
    {
        // char val[4];
        // memcpy(val, buffer + cur, 4);
        value = __builtin_bswap32(*((int *)(buffer + cur)));
        cur += type.hasTime ? 12 : 4;
        // value = converter.ToInt32(val);
        obj = PyLong_FromLong(value);
        PyList_SetItem(item, index, obj);
        break;
    }
    case ValueType::SINT:
    {
        char val;
        value = buffer[cur++];
        cur += type.hasTime ? 8 : 0;
        obj = PyLong_FromLong(value);
        PyList_SetItem(item, index, obj);
        break;
    }
    case ValueType::USINT:
    {
        unsigned char val;
        val = buffer[cur++];
        value = val;
        cur += type.hasTime ? 8 : 0;
        obj = PyLong_FromLong(value);
        PyList_SetItem(item, index, obj);
        break;
    }
    default:
        break;
    }
}

/**
 * @brief 将查询得到的buffer中的数据提取到python列表中，允许数组(矢量)
 *
 * @param buffer
 * @return PyObject*
 */
PyObject *ConvertToPyList_ML_Old(struct DB_DataBuffer *buffer)
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
            if (typeList[j].valueType == ValueType::IMAGE)
                continue;
            if (!typeList[j].isArray) //标量
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
            else //数组类型（矢量）
            {
                PyObject *arr = PyList_New(typeList[j].arrayLen);
                long value;
                double floatvalue;
                for (int k = 0; k < typeList[j].arrayLen; k++)
                {
                    switch (typeList[j].valueType)
                    {
                    case ValueType::INT:
                    {
                        char val[2];
                        memcpy(val, buffer->buffer + cur, 2);
                        cur += 2;
                        value = converter.ToInt16(val);
                        PyObject *obj = PyLong_FromLong(value);
                        PyList_SetItem(arr, k, obj);
                        break;
                    }
                    case ValueType::UINT:
                    {
                        char val[2];
                        memcpy(val, buffer->buffer + cur, 2);
                        cur += 2;
                        value = converter.ToUInt16(val);
                        PyObject *obj = PyLong_FromLong(value);
                        PyList_SetItem(arr, k, obj);
                        break;
                    }
                    case ValueType::DINT:
                    {
                        char val[4];
                        memcpy(val, buffer->buffer + cur, 4);
                        cur += 4;
                        value = converter.ToInt32(val);
                        PyObject *obj = PyLong_FromLong(value);
                        PyList_SetItem(arr, k, obj);
                        break;
                    }
                    case ValueType::UDINT:
                    {
                        char val[4];
                        memcpy(val, buffer->buffer + cur, 4);
                        cur += 4;
                        value = converter.ToUInt32(val);
                        PyObject *obj = PyLong_FromLong(value);
                        PyList_SetItem(arr, k, obj);
                        break;
                    }
                    case ValueType::REAL:
                    {
                        char val[4];
                        memcpy(val, buffer->buffer + cur, 4);
                        cur += 4;
                        floatvalue = converter.ToFloat(val);
                        PyObject *obj = PyLong_FromLong(floatvalue);
                        PyList_SetItem(arr, k, obj);
                        break;
                    }
                    case ValueType::TIME:
                    {
                        char val[4];
                        memcpy(val, buffer->buffer + cur, 4);
                        cur += 4;
                        value = converter.ToUInt32(val);
                        PyObject *obj = PyLong_FromLong(value);
                        PyList_SetItem(arr, k, obj);
                        break;
                    }
                    case ValueType::SINT:
                    {
                        char val;
                        value = buffer->buffer[cur++];
                        PyObject *obj = PyLong_FromLong(value);
                        PyList_SetItem(arr, k, obj);
                        break;
                    }
                    case ValueType::USINT:
                    {
                        unsigned char val;
                        val = buffer->buffer[cur++];
                        value = val;
                        PyObject *obj = PyLong_FromLong(value);
                        PyList_SetItem(arr, k, obj);
                        break;
                    }
                    default:
                        break;
                    }
                }
                cur += typeList[j].hasTime ? 8 : 0;
                PyList_Append(row, arr);
            }
        }
        PyList_SetItem(res, i, row);
    }
    return res;
}

/**
 * @brief 将查询得到的buffer中的数据提取到python列表中，允许数组(矢量)
 *
 * @param buffer
 * @return PyObject*
 * @note support timeseries
 */
PyObject *ConvertToPyList_ML(DB_DataBuffer *buffer)
{
    QueryBufferReader reader(buffer, false);
    int cur = reader.startPos;
    PyObject *res;
    if (!Py_IsInitialized())
        Py_Initialize();
    res = PyList_New(reader.rows);
    for (int i = 0; i < reader.rows; i++)
    {
        PyObject *row = PyList_New(0);
        for (auto &type : reader.typeList)
        {
            if (type.valueType == ValueType::IMAGE)
                continue;
            if (!type.isArray) //标量,转到python中为一维数组
            {
                if (type.isTimeseries) //如果是时间序列，直接全部转为PyLong或PyFloat，在python内部reshape
                {
                    PyObject *ts = PyList_New(type.tsLen * 2);
                    for (int k = 0; k < type.tsLen; k++)
                    {
                        SetValueToPyList(ts, buffer->buffer, cur, type, k * 2);
                        long timestamp = 0;
                        memcpy(&timestamp, buffer->buffer + cur, 8);
                        PyObject *time = PyLong_FromLong(timestamp);
                        PyList_SetItem(ts, k * 2 + 1, time); // insert timestamp to timeseries
                    }
                    PyList_Append(row, ts);
                }
                else
                {
                    AppendValueToPyList(row, buffer->buffer, cur, type);
                }
            }
            else //数组类型（矢量），转到python中为二维数组
            {
                if (type.isTimeseries)
                {
                    PyObject *ts = PyList_New(type.tsLen * (type.arrayLen + 1));
                    for (int k = 0; k < type.tsLen; k++)
                    {
                        PyObject *arr = PyList_New(type.arrayLen + 1); //增加一个时间戳位
                        for (int l = 0; l < type.arrayLen; l++)
                        {
                            SetValueToPyList(arr, buffer->buffer, cur, type, l);
                        }
                        long timestamp = 0;
                        memcpy(&timestamp, buffer->buffer + cur, 8);
                        PyObject *time = PyLong_FromLong(timestamp);
                        PyList_SetItem(arr, type.arrayLen + 1, time); // insert timestamp to timeseries
                        PyList_SetItem(ts, k, arr);
                    }
                    PyList_Append(row, ts);
                }
                else
                {
                    PyObject *arr = PyList_New(type.arrayLen);
                    for (int k = 0; k < type.arrayLen; k++)
                    {
                        SetValueToPyList(arr, buffer->buffer, cur, type, k);
                    }
                    cur += type.hasTime ? 8 : 0;
                    PyList_Append(row, arr);
                }
            }
        }
        PyList_SetItem(res, i, row);
    }
    return res;
}

int DB_OutlierDetection(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    int dim = 1;
    if (params->byPath)
    {
        auto allTypes = CurrentTemplate.GetAllTypes(params->pathCode);
        if (allTypes.size() > 1)
            return StatusCode::ML_TYPE_NOT_SUPPORT;
        dim = allTypes[0].isArray ? allTypes[0].arrayLen : 1;
    }

    int err = DB_ExecuteQuery(buffer, params);
    if (err != 0)
        return err;
    if (!Py_IsInitialized())
        Py_Initialize();
    PyObject *arr = ConvertToPyList_ML(buffer);
    if (PyObject_Size(arr) == 0)
        return StatusCode::INVALID_QUERY_BUFFER;
    // 指定py文件目录
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("if './' not in sys.path: sys.path.append('./')");

    PyObject *mymodule = PyImport_ImportModule("Novelty_Outlier");
    PyObject *pArgs, *pFunc;
    if (mymodule != NULL)
    {
        // 从模块中获取函数
        pFunc = PyObject_GetAttrString(mymodule, "Outliers");

        if (pFunc && PyCallable_Check(pFunc))
        {
            // 创建参数元组
            pArgs = PyTuple_New(2);
            PyTuple_SetItem(pArgs, 0, arr);
            PyTuple_SetItem(pArgs, 1, PyLong_FromLong(dim));
            // 函数执行
            PyObject *ret = PyObject_CallObject(pFunc, pArgs);
            PyObject *item;
            long val;
            int len = PyObject_Size(ret);
            char *res = (char *)malloc(len);
            for (int i = 0; i < len; i++)
            {
                item = PyList_GetItem(ret, i); //根据下标取出python列表中的元素
                val = PyLong_AsLong(item);     //转换为c类型的数据
                // cout << val << " ";
                res[i] = (char)val;
            }
            free(buffer->buffer);
            buffer->length = len;
            buffer->buffer = res;
            buffer->bufferMalloced = 1;
        }
        Py_XDECREF(pFunc);
    }
    Py_DECREF(arr);

    Py_XDECREF(mymodule);
    return 0;
}

int DB_NoveltyFit(DB_QueryParams *params, double *maxLine, double *minLine)
{
    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    int dim = 1;
    if (params->byPath) // ristrict type num to 1
    {
        auto allTypes = CurrentTemplate.GetAllTypes(params->pathCode);
        if (allTypes.size() > 1)
            return StatusCode::ML_TYPE_NOT_SUPPORT;
        dim = allTypes[0].isArray ? allTypes[0].arrayLen : 1;
        if (allTypes[0].isTimeseries)
            dim++;
    }
    DB_DataBuffer buffer;
    int err = DB_ExecuteQuery(&buffer, params);
    if (err != 0)
        return err;
    if (!Py_IsInitialized())
        Py_Initialize();
    PyObject *arr = ConvertToPyList_ML(&buffer);

    // 指定py文件目录
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("if './' not in sys.path: sys.path.append('./')");

    PyObject *mymodule = PyImport_ImportModule("Novelty_Outlier");
    PyObject *pArgs, *pFunc;
    long res = 0;
    if (mymodule != NULL)
    {
        // 从模块中获取函数
        pFunc = PyObject_GetAttrString(mymodule, "NoveltyFit");

        if (pFunc && PyCallable_Check(pFunc))
        {
            // 创建参数元组
            pArgs = PyTuple_New(2);
            PyTuple_SetItem(pArgs, 0, arr);
            // PyTuple_SetItem(pArgs, 1, PyLong_FromLong(dim));
            // 函数执行
            PyObject *ret = PyObject_CallObject(pFunc, pArgs);
            *maxLine = PyFloat_AsDouble(PyTuple_GetItem(ret, 0)); //转换为c类型的数据
            *minLine = PyFloat_AsDouble(PyTuple_GetItem(ret, 1));
        }
        Py_XDECREF(pFunc);
    }
    Py_DECREF(arr);

    Py_XDECREF(mymodule);
    return 0;
}

int DB_NoveltyFit_JF(DB_QueryParams *params, double *maxLine, double *minLine)
{
    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    int dim = 1;
    if (params->byPath)
    {
        auto allTypes = CurrentTemplate.GetAllTypes(params->pathCode);
        if (allTypes.size() > 1)
            return StatusCode::ML_TYPE_NOT_SUPPORT;
        dim = allTypes[0].isArray ? allTypes[0].arrayLen : 1;
        if (allTypes[0].isTimeseries)
            dim++;
    }
    DB_DataBuffer buffer;
    int err = DB_ExecuteQuery(&buffer, params);
    if (err != 0)
        return err;
    if (!Py_IsInitialized())
        Py_Initialize();
    PyObject *arr = ConvertToPyList_ML(&buffer);

    // 指定py文件目录
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("if './' not in sys.path: sys.path.append('./')");

    PyObject *mymodule = PyImport_ImportModule("Novelty_Outlier");
    PyObject *pArgs, *pFunc;
    long res = 0;
    if (mymodule != NULL)
    {
        // 从模块中获取函数
        pFunc = PyObject_GetAttrString(mymodule, "NoveltyFit_JF");

        if (pFunc && PyCallable_Check(pFunc))
        {
            // 创建参数元组
            pArgs = PyTuple_New(2);
            PyTuple_SetItem(pArgs, 0, arr);
            // PyTuple_SetItem(pArgs, 1, PyLong_FromLong(dim));
            // 函数执行
            PyObject *ret = PyObject_CallObject(pFunc, pArgs);
            *maxLine = PyFloat_AsDouble(PyTuple_GetItem(ret, 0)); //转换为c类型的数据
            *minLine = PyFloat_AsDouble(PyTuple_GetItem(ret, 1));
        }
        Py_XDECREF(pFunc);
    }
    Py_DECREF(arr);

    Py_XDECREF(mymodule);
    return 0;
}

/**
 * @brief 指定路径，对此路径下模版中所有变量的数据执行局部离群因子（LOF）训练，模型存为 变量名_novelty_model.pkl
 *
 * @param pathToLine
 * @return 0:success,others:StatusCode
 */
int DB_NoveltyTraining(const char *pathToLine)
{
    if (TemplateManager::CheckTemplate(pathToLine) != 0)
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    if (!Py_IsInitialized())
        Py_Initialize();
    // 指定py文件目录
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("if './' not in sys.path: sys.path.append('./')");
    for (auto const &schema : CurrentTemplate.schemas)
    {
        DB_DataBuffer buffer;
        DB_QueryParams params;
        params.byPath = 0;
        params.valueName = schema.first.name.c_str();
        params.queryType = TIMESPAN;
        params.start = 0;
        params.end = getMilliTime();
        params.pathToLine = pathToLine;
        params.compareType = CMP_NONE;
        params.order = ODR_NONE;
        int err = DB_ExecuteQuery(&buffer, &params);
        if (err != 0 || !buffer.bufferMalloced)
            return err;

        PyObject *arr = ConvertToPyList_ML(&buffer);

        PyObject *mymodule = PyImport_ImportModule("Novelty_Outlier");
        PyObject *pValue, *pArgs, *pFunc;
        long res = 0;
        if (mymodule != NULL)
        {
            // 从模块中获取函数
            pFunc = PyObject_GetAttrString(mymodule, "NoveltyModelTrain");
            int dim = schema.second.isArray ? schema.second.arrayLen : 1;
            if (pFunc && PyCallable_Check(pFunc))
            {
                // 创建参数元组
                pArgs = PyTuple_New(3);
                PyTuple_SetItem(pArgs, 0, arr);
                PyTuple_SetItem(pArgs, 1, PyLong_FromLong(dim));
                PyTuple_SetItem(pArgs, 2, PyBytes_FromString(params.valueName));
                // 函数执行
                PyObject *ret = PyObject_CallObject(pFunc, pArgs);
                if (PyLong_AsLong(ret) != 0)
                    return StatusCode::UNKNWON_DATAFILE;
            }
            Py_XDECREF(pFunc);
        }
        Py_DECREF(arr);

        Py_XDECREF(mymodule);
    }
    return 0;
}
// int main()
// {
//     // Py_Initialize();
//     DB_QueryParams params;
//     params.pathToLine = "JinfeiTem";
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
//     params.valueName = "S1OFF";
//     // params.valueName = NULL;
//     params.start = 0;
//     params.end = 1750099030250;
//     // params.start = 1650093562902;
//     // params.end = 1650163562902;
//     params.order = ODR_NONE;
//     params.compareType = CMP_NONE;
//     params.compareValue = "666";
//     params.queryType = TIMESPAN;
//     params.byPath = 0;
//     params.queryNums = 50;
//     DB_DataBuffer buffer;
//     double maxline, minline;
//     DB_NoveltyFit_JF(&params, &maxline, &minline);
//     // DB_NoveltyFit(&params, &maxline, &minline);
//     Py_Finalize();
//     return 0;
// }
