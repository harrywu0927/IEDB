#include "../include/utils.hpp"

/**
 * @brief 检查变量名是否合法
 *
 * @param variableName 变量名
 * @return int
 */
int checkInputVaribaleName(string variableName)
{
    if (variableName.empty())
        return StatusCode::UNKNOWN_VARIABLE_NAME;
    if ((variableName[0] >= 'a' && variableName[0] <= 'z') || (variableName[0] >= 'A' && variableName[0] <= 'Z') || variableName[0] == '_' || variableName[0] == '@')
        ;
    else
    {
        cout << "变量名输入不合法！" << endl;
        return StatusCode::VARIABLE_NAME_CHECK_ERROR;
    }

    for (int i = 1; i < variableName.length(); i++)
    {
        if ((variableName[i] >= 'a' && variableName[i] <= 'z') || (variableName[i] >= 'A' && variableName[i] <= 'Z') || variableName[i] == '_' || (variableName[i] >= '0' && variableName[i] <= '9'))
            continue;
        else
        {
            cout << "变量名输入不合法！" << endl;
            return StatusCode::VARIABLE_NAME_CHECK_ERROR;
        }
    }
    return 0;
}

/**
 * @brief 检查路径编码是否合法
 *
 * @param pathcode 路径编码
 * @return int
 */
int checkInputPathcode(char pathcode[])
{
    if (pathcode == NULL)
        return 1;
    for (int i = 0; i < 10; i++)
    {
        if ((pathcode[i] >= (char)0 && pathcode[i] <= (char)127) || (pathcode[i] >= 'a' && pathcode[i] <= 'z') || (pathcode[i] >= 'A' && pathcode[i] <= 'Z'))
            continue;
        else
        {
            cout << "路径编码输入不合法!" << endl;
            return StatusCode::PATHCODE_CHECK_ERROR;
        }
    }
    return 0;
}

int checkInputSingleValue(int variableType, string value)
{
    switch (variableType)
    {
    case 5: // BOOL
    {
        if (value.length() != 1)
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        else if (value[0] != '0' && value[0] != '1')
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        break;
    }
    case 2: // USINT
    {
        for (int i = 0; i < value.length(); i++) //检查是否都为数字
        {
            if (value[i] < '0' || value[i] > '9')
            {
                return StatusCode::VALUE_CHECKE_ERROR;
            }
        }
        if ((value.length() != 1 && value[0] == '0') || value[0] == '-') //以0开头或 以-开头
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        uint16_t Val = (uint16_t)atoi(value.c_str());
        if (Val > 255 || Val < 0)
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        break;
    }
    case 1: // UINT
    {
        for (int i = 0; i < value.length(); i++) //检查是否都为数字
        {
            if (value[i] < '0' || value[i] > '9')
            {
                return StatusCode::VALUE_CHECKE_ERROR;
            }
        }
        if ((value.length() != 1 && value[0] == '0') || value[0] == '-') //以0开头或 以-开头
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        uint32_t Val = (uint32_t)atoi(value.c_str());
        if (Val > 65535 || Val < 0)
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        break;
    }
    case 3: // UDINT
    {
        for (int i = 0; i < value.length(); i++) //检查是否都为数字
        {
            if (value[i] < '0' || value[i] > '9')
            {
                return StatusCode::VALUE_CHECKE_ERROR;
            }
        }
        if ((value.length() != 1 && value[0] == '0') || value[0] == '-') //以0开头或 以-开头
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        uint64_t Val = (uint64_t)stoll(value);
        if (Val > 4294967295 || Val < 0)
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        break;
    }
    case 6: // SINT
    {
        if (value.length() == 1 && value[0] == '-') //只有一个 -
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }

        for (int i = 0; i < value.length(); i++) //检查是否都为数字或者以-开头
        {
            if (i == 0 && value[i] == '-') //以-开头
            {
                continue;
            }
            else if (value[i] < '0' || value[i] > '9')
            {
                return StatusCode::VALUE_CHECKE_ERROR;
            }
        }

        if ((value.length() != 1 && value[0] == '0') || (value.length() > 2 && value[0] == '-' && value[1] == '0')) //以0开头
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        int16_t Val = (int16_t)atoi(value.c_str());
        if (Val > 127 || Val < -128)
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        break;
    }
    case 4: // INT
    {
        if (value.length() == 1 && value[0] == '-') //只有一个 -
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }

        for (int i = 0; i < value.length(); i++) //检查是否都为数字或者以-开头
        {
            if (i == 0 && value[i] == '-') //以-开头
            {
                continue;
            }
            else if (value[i] < '0' || value[i] > '9')
            {
                return StatusCode::VALUE_CHECKE_ERROR;
            }
        }

        if ((value.length() != 1 && value[0] == '0') || (value.length() > 2 && value[0] == '-' && value[1] == '0')) //以0开头
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        int32_t Val = (int32_t)atoi(value.c_str());
        if (Val > 32767 || Val < -32768)
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        break;
    }
    case 7: // DINT
    {
        if (value.length() == 1 && value[0] == '-') //只有一个 -
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }

        for (int i = 0; i < value.length(); i++) //检查是否都为数字或者以-开头
        {
            if (i == 0 && value[i] == '-') //以-开头
            {
                continue;
            }
            else if (value[i] < '0' || value[i] > '9')
            {
                return StatusCode::VALUE_CHECKE_ERROR;
            }
        }

        if ((value.length() != 1 && value[0] == '0') || (value.length() > 2 && value[0] == '-' && value[1] == '0')) //以0开头
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        int64_t Val = (int64_t)stoll(value);
        if (Val > 2147483647 || Val < -2147483648)
        {
            return StatusCode::VALUE_CHECKE_ERROR;
        }
        break;
    }
    case 8: // REAL
    {
        if (value.find(".") != string::npos) //包含小数点
        {
            vector<string> valueSplit = DataType::splitWithStl(value, ".");
            if (valueSplit.size() != 2) //一般分割出两段
            {
                return StatusCode::VALUE_CHECKE_ERROR;
            }
            if (valueSplit[0].length() == 0 || valueSplit[1].length() == 0) //分割出来有空的情况
            {
                return StatusCode::VALUE_CHECKE_ERROR;
            }

            //检查前半段
            if (valueSplit[0].find("-") != string::npos) //如果包含 -
            {
                vector<string> temp = DataType::splitWithStl(valueSplit[0], "-");
                if (temp.size() != 2) //不只一个 -
                {
                    return StatusCode::VALUE_CHECKE_ERROR;
                }
                if (temp[0].length() != 0) //-分割出前半段肯定为空
                {
                    return StatusCode::VALUE_CHECKE_ERROR;
                }

                if (temp[1].length() == 0) //后半段为空，说明只有 -
                {
                    return StatusCode::VALUE_CHECKE_ERROR;
                }
                if (temp[1].length() > 1 && temp[1][0] == '0') //如果是0开头那么长度只能为1
                {
                    return StatusCode::VALUE_CHECKE_ERROR;
                }

                for (int j = 0; j < temp[1].length(); j++)
                {
                    if (temp[1][j] < '0' || temp[1][j] > '9') //-分割出后半段只能是纯数字
                    {
                        return StatusCode::VALUE_CHECKE_ERROR;
                    }
                }
            }
            else //不包含-
            {
                if (valueSplit[0].length() > 1 && valueSplit[0][0] == '0') //如果是0开头那么长度只能为1
                {
                    return StatusCode::VALUE_CHECKE_ERROR;
                }

                for (int i = 0; i < valueSplit[0].length(); i++)
                {
                    if (valueSplit[0][i] < '0' || valueSplit[0][i] > '9') //-分割出后半段只能是纯数字
                    {
                        return StatusCode::VALUE_CHECKE_ERROR;
                    }
                }
            }

            //检查后半段
            for (int i = 0; i < valueSplit[1].length(); i++)
            {
                if (valueSplit[1][i] < '0' || valueSplit[1][i] > '9') //后半段只能是纯数字
                {
                    return StatusCode::VALUE_CHECKE_ERROR;
                }
            }
        }
        else //不包含小数点
        {
            if (value.find("-") != string::npos) //包含 -
            {
                vector<string> temp = DataType::splitWithStl(value, "-");
                if (temp.size() != 2) //正常分割出两段
                {
                    return StatusCode::VALUE_CHECKE_ERROR;
                }
                if (temp[0].length() != 0) //前半段一定为空
                {
                    return StatusCode::VALUE_CHECKE_ERROR;
                }

                //检查后半段
                if (temp[1].length() == 0) //后半段为空
                {
                    return StatusCode::VALUE_CHECKE_ERROR;
                }
                if (temp[1].length() > 1 && temp[1][0] == '0') // 0开头
                {
                    return StatusCode::VALUE_CHECKE_ERROR;
                }

                for (int j = 0; j < temp[1].length(); j++)
                {
                    if (temp[1][j] < '0' || temp[1][j] > '9') //-分割出后半段只能是纯数字
                    {
                        return StatusCode::VALUE_CHECKE_ERROR;
                    }
                }
            }
            else // 不包含 -
            {
                if (value.length() > 1 && value[0] == '0') //连续0开头
                {
                    return StatusCode::VALUE_CHECKE_ERROR;
                }
                for (int j = 0; j < value.length(); j++)
                {
                    if (value[j] < '0' || value[j] > '9') //-分割出后半段只能是纯数字
                    {
                        return StatusCode::VALUE_CHECKE_ERROR;
                    }
                }
            }
        }
        break;
    }
    default:
        return StatusCode::DATA_TYPE_MISMATCH_ERROR;
        break;
    }
    return 0;
}

/**
 * @brief 检查数据值是否合法
 *
 * @param variableType 数据类型
 * @param value 数据值
 * @return int
 */
int checkInputValue(string variableType, string value, int isArray, int arrayLen)
{
    int err = 0;
    int type = DataType::JudgeValueType(variableType);

    if (isArray == 0)
    {
        err = checkInputSingleValue(type, value);
    }
    else
    {
        if (arrayLen < 1)
            return StatusCode::ARRAYLEN_ERROR;
        vector<string> valueArr = DataType::splitWithStl(value, " ");
        if (valueArr.size() != arrayLen)
            return StatusCode::VALUE_CHECKE_ERROR;
        else
        {
            for (int i = 0; i < arrayLen; i++)
            {
                err = checkInputSingleValue(type, valueArr[i]);
                if (err != 0)
                    break;
            }
        }
    }
    return err;
}

int checkSingleValueRange(int variableType, string standardValue, string maxValue, string minValue)
{
    switch (variableType)
    {
    case 5: // BOOL
    {
        if (standardValue != maxValue || standardValue != minValue || minValue != maxValue)
        {
            cout << "BOOL类型标准值、最大值、最小值必须保持一致!" << endl;
            return StatusCode::VALUE_RANGE_ERROR;
        }
        break;
    }
    case 2: // USINT
    {
        uint8_t standard = (uint8_t)atoi(standardValue.c_str());
        uint8_t min = (uint8_t)atoi(minValue.c_str());
        uint8_t max = (uint8_t)atoi(maxValue.c_str());
        if (standard < min || standard > max)
        {
            cout << "请检查标准值、最大值、最小值的范围！" << endl;
            return StatusCode::VALUE_RANGE_ERROR;
        }
        break;
    }
    case 1: // UINT
    {
        uint16_t standard = (uint16_t)atoi(standardValue.c_str());
        uint16_t min = (uint16_t)atoi(minValue.c_str());
        uint16_t max = (uint16_t)atoi(maxValue.c_str());
        if (standard < min || standard > max)
        {
            cout << "请检查标准值、最大值、最小值的范围！" << endl;
            return StatusCode::VALUE_RANGE_ERROR;
        }
        break;
    }
    case 3: // UDINT
    {
        uint32_t standard = (uint32_t)atoi(standardValue.c_str());
        uint32_t min = (uint32_t)atoi(minValue.c_str());
        uint32_t max = (uint32_t)atoi(maxValue.c_str());
        if (standard < min || standard > max)
        {
            cout << "请检查标准值、最大值、最小值的范围！" << endl;
            return StatusCode::VALUE_RANGE_ERROR;
        }
        break;
    }
    case 6: // SINT
    {
        int8_t standard = (int8_t)atoi(standardValue.c_str());
        int8_t min = (int8_t)atoi(minValue.c_str());
        int8_t max = (int8_t)atoi(maxValue.c_str());
        if (standard < min || standard > max)
        {
            cout << "请检查标准值、最大值、最小值的范围！" << endl;
            return StatusCode::VALUE_RANGE_ERROR;
        }
        break;
    }
    case 4: // INT
    {
        int16_t standard = (int16_t)atoi(standardValue.c_str());
        int16_t min = (int16_t)atoi(minValue.c_str());
        int16_t max = (int16_t)atoi(maxValue.c_str());
        if (standard < min || standard > max)
        {
            cout << "请检查标准值、最大值、最小值的范围！" << endl;
            return StatusCode::VALUE_RANGE_ERROR;
        }
        break;
    }
    case 7: // DINT
    {
        int32_t standard = (int32_t)atoi(standardValue.c_str());
        int32_t min = (int32_t)atoi(minValue.c_str());
        int32_t max = (int32_t)atoi(maxValue.c_str());
        if (standard < min || standard > max)
        {
            cout << "请检查标准值、最大值、最小值的范围！" << endl;
            return StatusCode::VALUE_RANGE_ERROR;
        }
        break;
    }
    case 8: // REAL
    {
        double standard = atof(standardValue.c_str());
        double min = atof(minValue.c_str());
        double max = atof(maxValue.c_str());
        if (standard < min || standard > max)
        {
            cout << "请检查标准值、最大值、最小值的范围！" << endl;
            return StatusCode::VALUE_RANGE_ERROR;
        }
        break;
    }
    default:
        return StatusCode::DATA_TYPE_MISMATCH_ERROR;
        break;
    }
    return 0;
}

/**
 * @brief 检查数据范围是否合法
 *
 * @param variableType 数据类型
 * @param standardValue 标准值
 * @param maxValue 最大值
 * @param minValue 最小值
 * @return int
 */
int checkValueRange(string variableType, string standardValue, string maxValue, string minValue, int isArray, int arrayLen)
{
    int err = 0;
    int type = DataType::JudgeValueType(variableType);

    if (isArray == 0)
    {
        err = checkSingleValueRange(type, standardValue, maxValue, minValue);
    }
    else
    {
        if (arrayLen < 1)
            return StatusCode::ARRAYLEN_ERROR;
        vector<string> standardValueArr = DataType::splitWithStl(standardValue, " ");
        vector<string> MaxValueArr = DataType::splitWithStl(maxValue, " ");
        vector<string> MinValueArr = DataType::splitWithStl(minValue, " ");
        if (standardValueArr.size() != arrayLen)
            return StatusCode::VALUE_CHECKE_ERROR;
        if (MaxValueArr.size() != arrayLen)
            return StatusCode::VALUE_CHECKE_ERROR;
        if (MinValueArr.size() != arrayLen)
            return StatusCode::VALUE_CHECKE_ERROR;
        else
        {
            for (int i = 0; i < arrayLen; i++)
            {
                err = checkSingleValueRange(type, standardValueArr[i], MaxValueArr[i], MinValueArr[i]);
                if (err != 0)
                    break;
            }
        }
    }
    return err;
}

/**
 * @brief 根据数据类型获得数值字符串
 *
 * @param value 字符串
 * @param type 数据类型
 * @param schemaPos 模板所在位置
 * @param MaxOrMin 最大值or最小值or标准值
 * @return int
 */
int getValueStringByValueType(char *value, ValueType::ValueType type, int schemaPos, int MaxOrMin, int isArray, int arrayLen)
{
    DataTypeConverter converter;
    char *s = new char[32];
    if (type == ValueType::BOOL)
    {
        if (isArray == 0)
        {
            if (MaxOrMin == 0)
            {
                //标准值
                uint8_t sValue = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
                sprintf(s, "%u", sValue);
                memcpy(value, s, 1);
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                uint8_t maValue = CurrentZipTemplate.schemas[schemaPos].second.maxValue[0];
                sprintf(s, "%u", maValue);
                memcpy(value, s, 1);
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                uint8_t miValue = CurrentZipTemplate.schemas[schemaPos].second.minValue[0];
                sprintf(s, "%u", miValue);
                memcpy(value, s, 1);
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
        else
        {
            if (arrayLen != CurrentZipTemplate.schemas[schemaPos].second.arrayLen)
                return StatusCode::ARRAYLEN_ERROR;
            if (MaxOrMin == 0)
            {
                //标准值
                for (int i = 0; i < arrayLen; i++)
                {
                    uint8_t sValue = CurrentZipTemplate.schemas[schemaPos].second.standard[i][0];
                    sprintf(s, "%u", sValue);
                    memcpy(value + 2 * i, s, 1);
                    if (i != arrayLen - 1)
                        value[2 * i + 1] = ' ';
                }
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                for (int i = 0; i < arrayLen; i++)
                {
                    uint8_t maValue = CurrentZipTemplate.schemas[schemaPos].second.max[i][0];
                    sprintf(s, "%u", maValue);
                    memcpy(value + 2 * i, s, 1);
                    if (i != arrayLen - 1)
                        value[2 * i + 1] = ' ';
                }
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                for (int i = 0; i < arrayLen; i++)
                {
                    uint8_t miValue = CurrentZipTemplate.schemas[schemaPos].second.min[i][0];
                    sprintf(s, "%u", miValue);
                    memcpy(value + 2 * i, s, 1);
                    if (i != arrayLen - 1)
                        value[2 * i + 1] = ' ';
                }
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
    }
    else if (type == ValueType::USINT)
    {
        if (isArray == 0)
        {
            if (MaxOrMin == 0)
            {
                //标准值
                uint8_t sValue = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
                sprintf(s, "%u", sValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                uint8_t maValue = CurrentZipTemplate.schemas[schemaPos].second.maxValue[0];
                sprintf(s, "%u", maValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                uint8_t miValue = CurrentZipTemplate.schemas[schemaPos].second.minValue[0];
                sprintf(s, "%u", miValue);
                memcpy(value, s, 10);
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
        else
        {
            if (arrayLen != CurrentZipTemplate.schemas[schemaPos].second.arrayLen)
                return StatusCode::ARRAYLEN_ERROR;
            if (MaxOrMin == 0)
            {
                //标准值
                for (int i = 0; i < arrayLen; i++)
                {
                    uint8_t sValue = CurrentZipTemplate.schemas[schemaPos].second.standard[i][0];
                    sprintf(s, "%u", sValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                for (int i = 0; i < arrayLen; i++)
                {
                    uint8_t maValue = CurrentZipTemplate.schemas[schemaPos].second.max[i][0];
                    sprintf(s, "%u", maValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                for (int i = 0; i < arrayLen; i++)
                {
                    uint8_t miValue = CurrentZipTemplate.schemas[schemaPos].second.min[i][0];
                    sprintf(s, "%u", miValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
    }
    else if (type == ValueType::UINT)
    {
        if (isArray == 0)
        {
            if (MaxOrMin == 0)
            {
                //标准值
                uint16_t sValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
                sprintf(s, "%u", sValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                uint16_t maValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.maxValue);
                sprintf(s, "%u", maValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                uint16_t miValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.minValue);
                sprintf(s, "%u", miValue);
                memcpy(value, s, 10);
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
        else
        {
            if (arrayLen != CurrentZipTemplate.schemas[schemaPos].second.arrayLen)
                return StatusCode::ARRAYLEN_ERROR;
            if (MaxOrMin == 0)
            {
                //标准值
                for (int i = 0; i < arrayLen; i++)
                {
                    uint16_t sValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
                    sprintf(s, "%u", sValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                for (int i = 0; i < arrayLen; i++)
                {
                    uint16_t maValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.max[i]);
                    sprintf(s, "%u", maValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                for (int i = 0; i < arrayLen; i++)
                {
                    uint16_t miValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[schemaPos].second.min[i]);
                    sprintf(s, "%u", miValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
    }
    else if (type == ValueType::UDINT)
    {
        if (isArray == 0)
        {
            if (MaxOrMin == 0)
            {
                //标准值
                uint32_t sValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
                sprintf(s, "%u", sValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                uint32_t maValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.maxValue);
                sprintf(s, "%u", maValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                uint32_t miValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.minValue);
                sprintf(s, "%u", miValue);
                memcpy(value, s, 10);
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
        else
        {
            if (arrayLen != CurrentZipTemplate.schemas[schemaPos].second.arrayLen)
                return StatusCode::ARRAYLEN_ERROR;
            if (MaxOrMin == 0)
            {
                //标准值
                for (int i = 0; i < arrayLen; i++)
                {
                    uint32_t sValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
                    sprintf(s, "%u", sValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                for (int i = 0; i < arrayLen; i++)
                {
                    uint32_t maValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.max[i]);
                    sprintf(s, "%u", maValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                for (int i = 0; i < arrayLen; i++)
                {
                    uint32_t miValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[schemaPos].second.min[i]);
                    sprintf(s, "%u", miValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
    }
    else if (type == ValueType::SINT)
    {
        if (isArray == 0)
        {
            if (MaxOrMin == 0)
            {
                //标准值
                int8_t sValue = CurrentZipTemplate.schemas[schemaPos].second.standardValue[0];
                sprintf(s, "%d", sValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                int8_t maValue = CurrentZipTemplate.schemas[schemaPos].second.maxValue[0];
                sprintf(s, "%d", maValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                int8_t miValue = CurrentZipTemplate.schemas[schemaPos].second.minValue[0];
                sprintf(s, "%d", miValue);
                memcpy(value, s, 10);
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
        else
        {
            if (arrayLen != CurrentZipTemplate.schemas[schemaPos].second.arrayLen)
                return StatusCode::ARRAYLEN_ERROR;
            if (MaxOrMin == 0)
            {
                //标准值
                for (int i = 0; i < arrayLen; i++)
                {
                    int8_t sValue = CurrentZipTemplate.schemas[schemaPos].second.standard[i][0];
                    sprintf(s, "%d", sValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                for (int i = 0; i < arrayLen; i++)
                {
                    int8_t maValue = CurrentZipTemplate.schemas[schemaPos].second.max[i][0];
                    sprintf(s, "%d", maValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                for (int i = 0; i < arrayLen; i++)
                {
                    int8_t miValue = CurrentZipTemplate.schemas[schemaPos].second.min[i][0];
                    sprintf(s, "%d", miValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
    }
    else if (type == ValueType::INT)
    {
        if (isArray == 0)
        {
            if (MaxOrMin == 0)
            {
                //标准值
                int16_t sValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
                sprintf(s, "%d", sValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                int16_t maValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.maxValue);
                sprintf(s, "%d", maValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                int16_t miValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.minValue);
                sprintf(s, "%d", miValue);
                memcpy(value, s, 10);
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
        else
        {
            if (arrayLen != CurrentZipTemplate.schemas[schemaPos].second.arrayLen)
                return StatusCode::ARRAYLEN_ERROR;
            if (MaxOrMin == 0)
            {
                //标准值
                for (int i = 0; i < arrayLen; i++)
                {
                    int16_t sValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
                    sprintf(s, "%d", sValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                for (int i = 0; i < arrayLen; i++)
                {
                    int16_t maValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.max[i]);
                    sprintf(s, "%d", maValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                for (int i = 0; i < arrayLen; i++)
                {
                    int16_t miValue = converter.ToInt16_m(CurrentZipTemplate.schemas[schemaPos].second.min[i]);
                    sprintf(s, "%d", miValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
    }
    else if (type == ValueType::DINT)
    {
        if (isArray == 0)
        {
            if (MaxOrMin == 0)
            {
                //标准值
                int32_t sValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
                sprintf(s, "%d", sValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                int32_t maValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.maxValue);
                sprintf(s, "%d", maValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                int32_t miValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.minValue);
                sprintf(s, "%d", miValue);
                memcpy(value, s, 10);
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
        else
        {
            if (arrayLen != CurrentZipTemplate.schemas[schemaPos].second.arrayLen)
                return StatusCode::ARRAYLEN_ERROR;
            if (MaxOrMin == 0)
            {
                //标准值
                for (int i = 0; i < arrayLen; i++)
                {
                    int32_t sValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
                    sprintf(s, "%d", sValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                for (int i = 0; i < arrayLen; i++)
                {
                    int32_t maValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.max[i]);
                    sprintf(s, "%d", maValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                for (int i = 0; i < arrayLen; i++)
                {
                    int32_t miValue = converter.ToInt32_m(CurrentZipTemplate.schemas[schemaPos].second.min[i]);
                    sprintf(s, "%d", miValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
    }
    else if (type == ValueType::REAL)
    {
        if (isArray == 0)
        {
            if (MaxOrMin == 0)
            {
                //标准值
                float sValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
                sprintf(s, "%f", sValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                float maValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.maxValue);
                sprintf(s, "%f", maValue);
                memcpy(value, s, 10);
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                float miValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.minValue);
                sprintf(s, "%f", miValue);
                memcpy(value, s, 10);
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
        else
        {
            if (arrayLen != CurrentZipTemplate.schemas[schemaPos].second.arrayLen)
                return StatusCode::ARRAYLEN_ERROR;
            if (MaxOrMin == 0)
            {
                //标准值
                for (int i = 0; i < arrayLen; i++)
                {
                    float sValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.standard[i]);
                    sprintf(s, "%f", sValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 1)
            {
                //最大值
                for (int i = 0; i < arrayLen; i++)
                {
                    float maValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.max[i]);
                    sprintf(s, "%f", maValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else if (MaxOrMin == 2)
            {
                //最小值
                for (int i = 0; i < arrayLen; i++)
                {
                    float miValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.min[i]);
                    sprintf(s, "%f", miValue);
                    strcat(value, s);
                    if (i != arrayLen - 1)
                        strcat(value, " ");
                }
            }
            else
                return StatusCode::MAX_OR_MIN_ERROR;
        }
    }
    else if (type == ValueType::IMAGE)
    {
        if (MaxOrMin == 0)
        {
            char s[10];
            float sValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.standardValue);
            sprintf(s, "%f", sValue);
            memcpy(value, s, 10);
        }
        else if (MaxOrMin == 1)
        {
            //最大值
            char ma[10];
            float maValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.maxValue);
            sprintf(ma, "%f", maValue);
            memcpy(value, ma, 10);
        }
        else if (MaxOrMin == 2)
        {
            //最小值
            char mi[10];
            float miValue = converter.ToFloat_m(CurrentZipTemplate.schemas[schemaPos].second.minValue);
            sprintf(mi, "%f", miValue);
            memcpy(value, mi, 10);
        }
        else
            return StatusCode::MAX_OR_MIN_ERROR;
    }
    else
        return StatusCode::UNKNOWN_TYPE;
    delete[] s;
    return 0;
}

/**
 * @brief 检查查询模板条件
 *
 * @param params
 * @return int
 */
int checkQueryNodeParam(struct DB_QueryNodeParams *params)
{
    if (params->valueName == NULL)
        return StatusCode::UNKNOWN_VARIABLE_NAME;
    if (params->pathToLine == NULL)
        return StatusCode::EMPTY_PATH_TO_LINE;
    return 0;
}

// int main()
// {
//     string standard = "100 200 300 400 500";
//     string max = "110 210 310 410 510";
//     string min = "90 190 290 390 490";
//     checkValueRange("UDINT",standard,max,min,1,5);
//     // cout<< checkInputValue("BOOL", "1 0 1 0 1 2",1,6) <<endl;
//     // DB_LoadZipSchema("arrTest");
//     // int t;
//     // for (int i = 0; i < CurrentZipTemplate.schemas[1].second.arrayLen; i++)
//     // {
//     //     char s[10];
//     //     uint8_t sValue = CurrentZipTemplate.schemas[1].second.standard[i][0];
//     //     sprintf(s, "%d", sValue);
//     //     cout << s[0] << endl;
//     // }
//     // char *ss = new char[100];
//     // getValueStringByValueType(ss, ValueType::UDINT, 0, 0, 1, 5);
//     // for (int i = 0; i < 12; i++)
//     //     cout << ss[i];
//     // cout << endl;
//     // string test = ss;
//     // cout << test << endl;
//     // delete[] ss;
//     return 0;
// }