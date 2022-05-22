#include <utils.hpp>

/**
 * @brief 将查询得到的buffer中的数据提取到python列表中，允许数组(矢量)
 *
 * @param buffer
 * @return PyObject*
 */
PyObject *ConvertToPyList_ML(DB_DataBuffer *buffer)
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

    // 指定py文件目录
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("if './' not in sys.path: sys.path.append('./')");

    PyObject *mymodule = PyImport_ImportModule("Novelty_Outlier");
    PyObject *pValue, *pArgs, *pFunc;
    long res = 0;
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
            buffer->length = len;
            buffer->buffer = res;
            buffer->bufferMalloced = 1;
        }
    }
    return 0;
}

int DB_NoveltyFit(DB_QueryParams *params, double *maxLine, double *minLine)
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
    PyObject *pValue, *pArgs, *pFunc;
    long res = 0;
    if (mymodule != NULL)
    {
        // 从模块中获取函数
        pFunc = PyObject_GetAttrString(mymodule, "NoveltyFit");

        if (pFunc && PyCallable_Check(pFunc))
        {
            // 创建参数元组
            pArgs = PyTuple_New(1);
            PyTuple_SetItem(pArgs, 0, arr);
            // PyTuple_SetItem(pArgs, 1, PyLong_FromLong(dim));
            // 函数执行
            PyObject *ret = PyObject_CallObject(pFunc, pArgs);
            *maxLine = PyFloat_AsDouble(PyTuple_GetItem(ret, 0)); //转换为c类型的数据
            *minLine = PyFloat_AsDouble(PyTuple_GetItem(ret, 1));
        }
    }
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
        }
    }
    return 0;
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
//     params.valueName = "S1OFF";
//     // params.valueName = NULL;
//     params.start = 0;
//     params.end = 1650099030250;
//     // params.start = 1650093562902;
//     // params.end = 1650163562902;
//     params.order = ODR_NONE;
//     params.compareType = CMP_NONE;
//     params.compareValue = "666";
//     params.queryType = FILEID;
//     params.byPath = 0;
//     params.queryNums = 50;
//     DB_DataBuffer buffer;
//     double maxline, minline;
//     DB_NoveltyTraining("JinfeiSixteen");
//     Py_Finalize();
//     return 0;
// }