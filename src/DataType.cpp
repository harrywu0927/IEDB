#include <utils.hpp>

vector<string> DataType::StringSplit(char srcstr[], const char *str)
{
    const char *d = str;
    char *p;
    p = strtok(srcstr, d);
    vector<string> res;
    while (p)
    {
        // printf("%s\n", p);
        res.push_back(p);
        p = strtok(NULL, d); // strtok内部记录了上次的位置
    }
    return res;
}

vector<string> DataType::splitWithStl(const string &str, const string &pattern)
{
    vector<string> resVec;

    if ("" == str)
    {
        return resVec;
    }
    //方便截取最后一段数据
    string strs = str + pattern;

    size_t pos = strs.find(pattern);
    size_t size = strs.size();

    while (pos != string::npos)
    {
        string x = strs.substr(0, pos);
        resVec.push_back(x);
        strs = strs.substr(pos + 1, size);
        pos = strs.find(pattern);
    }

    return resVec;
}

ValueType::ValueType DataType::JudgeValueType(string vType)
{
    if (vType == "INT")
        return ValueType::INT;
    else if (vType == "UINT")
        return ValueType::UINT;
    else if (vType == "DINT")
        return ValueType::DINT;
    else if (vType == "UDINT")
        return ValueType::UDINT;
    else if (vType == "REAL")
        return ValueType::REAL;
    else if (vType == "USINT")
        return ValueType::USINT;
    else if (vType == "TIME")
        return ValueType::TIME;
    else if (vType == "BOOL")
        return ValueType::BOOL;
    else if (vType == "SINT")
        return ValueType::SINT;
    else if (vType == "IMAGE")
        return ValueType::IMAGE;
    return ValueType::UNKNOWN;
}

/**
 * @brief 通过ValueType类型返回对应的枚举值
 *
 * @param vType ValueType::ValueType枚举类型
 * @return int 数据类型对应的枚举值
 */
int DataType::JudgeByValueType(ValueType::ValueType vType)
{
    switch (vType)
    {
    case ValueType::INT:
        return 4;
        break;
    case ValueType::UINT:
        return 1;
        break;
    case ValueType::SINT:
        return 6;
        break;
    case ValueType::USINT:
        return 2;
        break;
    case ValueType::REAL:
        return 8;
        break;
    case ValueType::TIME:
        return 9;
        break;
    case ValueType::DINT:
        return 7;
        break;
    case ValueType::UDINT:
        return 3;
        break;
    case ValueType::BOOL:
        return 5;
        break;
    case ValueType::IMAGE:
        return 10;
        break;
    default:
        return 0;
        break;
    }
}

/**
 * @brief 通过int类型返回对应的数据类型字符串
 *
 * @param vType ValueType::ValueType的枚举值
 * @return string 对应的数据类型字符串
 */
string DataType::JudgeValueTypeByNum(int vType)
{
    if (vType == 1)
        return "UINT";
    else if (vType == 2)
        return "USINT";
    else if (vType == 3)
        return "UDINT";
    else if (vType == 4)
        return "INT";
    else if (vType == 5)
        return "BOOL";
    else if (vType == 6)
        return "SINT";
    else if (vType == 7)
        return "DINT";
    else if (vType == 8)
        return "REAL";
    else if (vType == 9)
        return "TIME";
    else if (vType == 10)
        return "IMAGE";
    return "UNKNOWN";
}

int DataType::GetValueBytes(ValueType::ValueType &type)
{
    switch (type)
    {
    case ValueType::INT: // int16
        return 2;
        break;
    case ValueType::UINT: // uint16
        return 2;
        break;
    case ValueType::SINT: // int8
        return 1;
        break;
    case ValueType::USINT: // uint8
        return 1;
        break;
    case ValueType::REAL: // float
        return 4;
        break;
    case ValueType::TIME: // int32
        return 4;
        break;
    case ValueType::DINT: // int32
        return 4;
        break;
    case ValueType::UDINT: // uint32
        return 4;
        break;
    case ValueType::BOOL: // int8
        return 1;
        break;
    case ValueType::IMAGE:
        return 1;
        break;
    default:
        return 0;
        break;
    }
}

int DataType::GetDataTypeFromStr(char dataType[], DataType &type)
{
    string dtype = dataType;
    if (dtype.find("ARRAY") != string::npos) //是数组
    {
        type.isArray = true;
        vector<string> arr = StringSplit(const_cast<char *>(dtype.c_str()), " ");
        type.valueType = JudgeValueType(arr[2]); //数组的值类型

        //获取数组长度
        arr[0][arr[0].length() - 1] = '\0';
        vector<string> vec = DataType::StringSplit(const_cast<char *>(arr[0].c_str()), "..");
        type.arrayLen = atoi(const_cast<char *>(vec[1].c_str()));
        type.valueBytes = GetValueBytes(type.valueType);
        return 0;
    }
    else if (dtype.find("Timeseries") != string::npos)
    {
        type.isTimeseries = true;
        vector<string> arr = StringSplit(const_cast<char *>(dtype.c_str()), " ");
        type.valueType = JudgeValueType(arr[2]); //值类型

        //获取时间序列长度
        arr[0][arr[0].length() - 1] = '\0';
        vector<string> vec = DataType::StringSplit(const_cast<char *>(arr[0].c_str()), "..");
        type.arrayLen = atoi(const_cast<char *>(vec[1].c_str()));
        type.valueBytes = GetValueBytes(type.valueType);
    }
    else if (dtype.find("IMAGE") != string::npos)
    {
        type.isArray = true;
        type.valueType = ValueType::IMAGE;
        type.valueBytes = 1;
        return 0;
    }
    else
    {
        type.isArray = false;
        type.arrayLen = 0;
        type.valueType = JudgeValueType(dataType);
        type.valueBytes = GetValueBytes(type.valueType);
        return 0;
    }
    errorCode = StatusCode::UNKNOWN_TYPE;
    return StatusCode::UNKNOWN_TYPE;
}

int DataType::CompareValue(DataType &type, char *compared, const char *toCompare)
{
    if (type.isArray == 1)
    {
        return 0;
    }
    stringstream ss;
    ss << toCompare;
    int bytes = DataType::GetValueBytes(type.valueType);
    if (bytes == 0)
    {
        errorCode = StatusCode::UNKNOWN_TYPE;
        return StatusCode::UNKNOWN_TYPE;
    }
    char bufferV1[bytes];
    memcpy(bufferV1, compared, bytes);
    DataTypeConverter converter;
    switch (type.valueType)
    {
    case ValueType::INT:
    {
        short value1 = converter.ToInt16(compared);
        short value2 = 0;
        ss >> value2;
        return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
        break;
    }
    case ValueType::DINT:
    {
        int value1 = converter.ToInt32(compared);
        int value2 = 0;
        ss >> value2;
        return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
        break;
    }
    case ValueType::UDINT:
    {
        uint value1 = converter.ToUInt32(compared);
        uint value2 = 0;
        ss >> value2;
        return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
        break;
    }
    case ValueType::UINT:
    {
        uint16_t value1 = converter.ToUInt16(compared);
        uint16_t value2 = 0;
        ss >> value2;
        return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
        break;
    }
    case ValueType::SINT:
    {
        int value1 = (int)compared[0];
        int value2 = 0;
        ss >> value2;
        return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
        break;
    }
    case ValueType::TIME:
    {
        int value1 = converter.ToInt32(compared);
        int value2 = 0;
        ss >> value2;
        return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
        break;
    }
    case ValueType::REAL:
    {
        float f1 = converter.ToFloat(compared);
        float f2 = 0;
        ss >> f2;
        return f1 == f2 ? 0 : (f1 > f2 ? 1 : -1);
        break;
    }

    default:
        return 0; //其他类型不作判断
        break;
    }
}

int DataType::CompareValueInBytes(DataType &type, char *compared, const char *toCompare)
{
    if (type.isArray == 1)
    {
        return 0;
    }
    int bytes = DataType::GetValueBytes(type.valueType);
    if (bytes == 0)
    {
        errorCode = StatusCode::UNKNOWN_TYPE;
        return StatusCode::UNKNOWN_TYPE;
    }
    char bufferV1[bytes];
    memcpy(bufferV1, compared, bytes);
    DataTypeConverter converter;
    switch (type.valueType)
    {
    case ValueType::INT:
    {
        short value1 = converter.ToInt16(compared);
        short value2 = converter.ToInt16(toCompare);
        return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
        break;
    }
    case ValueType::DINT:
    {
        int value1 = converter.ToInt32(compared);
        int value2 = converter.ToInt32(toCompare);
        return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
        break;
    }
    case ValueType::UDINT:
    {
        uint value1 = converter.ToUInt32(compared);
        uint value2 = converter.ToUInt32(toCompare);
        return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
        break;
    }
    case ValueType::UINT:
    {
        uint16_t value1 = converter.ToUInt16(compared);
        uint16_t value2 = converter.ToUInt16(toCompare);
        return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
        break;
    }
    case ValueType::SINT:
    {
        int value1 = (int)compared[0];
        int value2 = (int)toCompare[0];
        return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
        break;
    }
    case ValueType::TIME:
    {
        int value1 = converter.ToInt32(compared);
        int value2 = converter.ToInt32(toCompare);
        return value1 == value2 ? 0 : (value1 > value2 ? 1 : -1);
        break;
    }
    case ValueType::REAL:
    {
        float f1 = converter.ToFloat(compared);
        float f2 = converter.ToFloat(toCompare);
        return f1 == f2 ? 0 : (f1 > f2 ? 1 : -1);
        break;
    }

    default:
        return 0; //其他类型不作判断
        break;
    }
}