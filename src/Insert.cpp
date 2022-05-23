#include <utils.hpp>

using namespace std;

#ifndef WIN32
void *checkSettings(void *ptr);
pthread_t timer; //计时器，另开一个新的线程
int timerStarted = false;
bool IOBusy = false;
void *checkTime(void *ptr)
{
    timerStarted = true;
    long interval = atol(settings("Pack_Interval").c_str());
    while (1)
    {
        while (IOBusy)
        {
        }
        if (timerStarted == false)
            pthread_exit(NULL);
        long curTime = getMilliTime();
        if (curTime % interval < interval)
        {
            vector<string> dirs;
            readAllDirs(dirs, settings("Filename_Label"));
            cout << "start packing" << endl;
            for (auto &dir : dirs)
            {
                cout << "packing " << dir << endl;
                removeFilenameLabel(dir);
                DB_Pack(dir.c_str(), 0, 0);
            }
            cout << "Pack complete" << endl;
        }
        sleep(interval / 1000);
        // sleep_until
    }
}
void *checkSettings(void *ptr)
{

    while (1)
    {
        while (IOBusy)
        {
        }
        FILE *fp = fopen("settings.json", "r+");
        fseek(fp, 0, SEEK_END);
        int len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char buff[len];
        fread(buff, len, 1, fp);
        fclose(fp);
        string str = buff;
        string contents(str);
        neb::CJsonObject tmp(contents);
        settings = tmp;
        // cout << settings("Filename_Label") << endl;
        // cout << settings("FileOverFlowMode") << endl;
        // cout << settings("Pack_Cache_Size") << endl;
        sleep(3);
        // this_thread::sleep_for(chrono::seconds(3));
    }
}

/**
 * @brief 负责对打包、再打包的自动管理
 *
 */
void autoPacker()
{
}
// thread settingsWatcher;
pthread_t settingsWatcher;
int settingsWatcherStarted = false;
#endif

/**
 * @brief 检查新数据的新颖性
 *
 * @param buffer
 * @return 0:success, others:Status Code
 * @note 按照模版分别获取数据，转为python参数后传入，输入已训练好的模型
 */
int checkNovelty(DB_DataBuffer *buffer)
{
    // 指定py文件目录
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("if './' not in sys.path: sys.path.append('./')");

    PyObject *mymodule = PyImport_ImportModule("Novelty_Outlier");
    PyObject *pValue, *pArgs, *pFunc;
    PyObject *vals, *names, *dimensions;
    names = PyList_New(0);
    vals = PyList_New(0);
    dimensions = PyList_New(0);
    int cur = 0;
    DataTypeConverter converter;
    for (auto const &schema : CurrentTemplate.schemas)
    {
        PyObject *name = PyBytes_FromString(schema.first.name.c_str());
        PyList_Append(names, name);
        PyObject *val, *dim;
        if (schema.second.isArray)
        {
            PyList_Append(dimensions, PyLong_FromLong(schema.second.arrayLen));
            val = PyList_New(schema.second.arrayLen);
            if (schema.second.valueType == ValueType::REAL)
            {
                float v = 0;
                for (int i = 0; i < schema.second.arrayLen; i++)
                {
                    memcpy(&v, buffer->buffer + cur, 4);
                    PyList_SetItem(val, i, PyFloat_FromDouble(v));
                    cur += 4;
                }
                PyList_Append(vals, val);
                cur += schema.second.hasTime ? 8 : 0;
            }
            else if (schema.second.valueType != ValueType::IMAGE)
            {
                long v = 0;
                for (int i = 0; i < schema.second.arrayLen; i++)
                {
                    switch (schema.second.valueType)
                    {
                    case ValueType::INT:
                    {
                        char buf[2];
                        memcpy(buf, buffer->buffer + cur, 2);
                        v = converter.ToInt16(buf);
                        break;
                    }
                    case ValueType::UINT:
                    {
                        char buf[2];
                        memcpy(buf, buffer->buffer + cur, 2);
                        v = converter.ToUInt16(buf);
                        break;
                    }
                    case ValueType::DINT:
                    {
                        char buf[4];
                        memcpy(buf, buffer->buffer + cur, 4);
                        v = converter.ToInt32(buf);
                        break;
                    }
                    case ValueType::UDINT:
                    {
                        char buf[4];
                        memcpy(buf, buffer->buffer + cur, 4);
                        v = converter.ToUInt32(buf);
                        break;
                    }
                    case ValueType::SINT:
                    {
                        v = buffer->buffer[cur++];
                        break;
                    }
                    case ValueType::USINT:
                    {
                        v = (unsigned char)buffer->buffer[cur];
                        break;
                    }
                    case ValueType::TIME:
                    {
                        char buf[4];
                        memcpy(buf, buffer->buffer + cur, 4);
                        v = converter.ToInt32(buf);
                        break;
                    }
                    case ValueType::BOOL:
                    {
                        v = buffer->buffer[cur];
                        break;
                    }
                    default:
                        break;
                    }
                    PyList_SetItem(val, i, PyLong_FromLong(v));
                    cur += schema.second.valueBytes;
                }
                PyList_Append(vals, val);
                cur += schema.second.hasTime ? 8 : 0;
            }
            else //是图片,跳过
            {
                char length[2], width[2], channels[2];
                memcpy(length, buffer->buffer + cur, 2);
                cur += 2;
                memcpy(width, buffer->buffer + cur, 2);
                cur += 2;
                memcpy(channels, buffer->buffer + cur, 2);
                cur += 2;
                cur += (int)converter.ToUInt16(length) * (int)converter.ToUInt16(width) * (int)converter.ToUInt16(channels);
            }
        }
        else
        {
            PyList_Append(dimensions, PyLong_FromLong(1));
            if (schema.second.valueType == ValueType::REAL)
            {
                float v = 0;
                memcpy(&v, buffer->buffer + cur, 4);
                cur += 4;
                val = PyLong_FromLong(v);
                PyList_Append(vals, val);
                cur += schema.second.hasTime ? 8 : 0;
            }
            else if (schema.second.valueType != ValueType::IMAGE)
            {
                long v = 0;
                switch (schema.second.valueType)
                {
                case ValueType::INT:
                {
                    char buf[2];
                    memcpy(buf, buffer->buffer + cur, 2);
                    v = converter.ToInt16(buf);
                    break;
                }
                case ValueType::UINT:
                {
                    char buf[2];
                    memcpy(buf, buffer->buffer + cur, 2);
                    v = converter.ToUInt16(buf);
                    break;
                }
                case ValueType::DINT:
                {
                    char buf[4];
                    memcpy(buf, buffer->buffer + cur, 4);
                    v = converter.ToInt32(buf);
                    break;
                }
                case ValueType::UDINT:
                {
                    char buf[4];
                    memcpy(buf, buffer->buffer + cur, 4);
                    v = converter.ToUInt32(buf);
                    break;
                }
                case ValueType::SINT:
                {
                    v = buffer->buffer[cur++];
                    break;
                }
                case ValueType::USINT:
                {
                    v = (unsigned char)buffer->buffer[cur];
                    break;
                }
                case ValueType::TIME:
                {
                    char buf[4];
                    memcpy(buf, buffer->buffer + cur, 4);
                    v = converter.ToInt32(buf);
                    break;
                }
                case ValueType::BOOL:
                {
                    v = buffer->buffer[cur];
                    break;
                }
                default:
                    break;
                }
                cur += schema.second.valueBytes;
                val = PyLong_FromLong(v);
                PyList_Append(vals, val);
                cur += schema.second.hasTime ? 8 : 0;
            }
        }
    }

    if (mymodule != NULL)
    {
        // 从模块中获取函数
        pFunc = PyObject_GetAttrString(mymodule, "Novelty");

        if (pFunc && PyCallable_Check(pFunc))
        {
            // 创建参数元组
            pArgs = PyTuple_New(3);
            PyTuple_SetItem(pArgs, 0, vals);
            PyTuple_SetItem(pArgs, 1, dimensions); //暂定维度1
            PyTuple_SetItem(pArgs, 2, names);
            // 函数执行
            PyObject *ret = PyObject_CallObject(pFunc, pArgs);
            PyObject *item;
            long res = 0;
            int len = PyObject_Size(ret);
            for (int i = 0; i < len; i++)
            {
                item = PyList_GetItem(ret, i); //根据下标取出python列表中的元素
                res = PyLong_AsLong(item);     //转换为c类型的数据
                if (res < 0)
                {
                    return StatusCode::NOVEL_DATA_DETECTED;
                }
            }
            return 0;
        }
    }
    return StatusCode::PYTHON_SCRIPT_NOT_FOUND;
}
/**
 * @brief 将一个缓冲区中的一条数据(文件)存放在指定路径下，以文件ID+时间的方式命名
 * @param buffer    数据缓冲区
 * @param addTime    是否添加时间戳
 *
 * @return  0:success,
 *          others: StatusCode
 * @note 文件ID的暂定的命名方式为当前文件夹下的文件数量+1，
 *  时间戳格式为yyyy-mm-dd-hh-min-ss-ms
 */
int DB_InsertRecord(DB_DataBuffer *buffer, int addTime)
{
#ifndef WIN32
    if (!settingsWatcherStarted)
    {
        pthread_create(&settingsWatcher, NULL, checkSettings, NULL);
        settingsWatcherStarted = true;
    }
    // if (!timerStarted && settings("Pack_Mode") == "timed")
    // {
    //     int ret = pthread_create(&timer, NULL, checkTime, NULL);
    //     if (ret != 0)
    //     {
    //         cout << "pthread_create error: error_code=" << ret << endl;
    //     }
    // }
    else if (settings("Pack_Mode") == "auto")
    {
    }
    return 0;
#endif
    int errCode = 0;
    IOBusy = 1;
    if (!Py_IsInitialized())
        Py_Initialize();
    if (settings("Check_Novelty") == "true")
    {
        if (TemplateManager::CheckTemplate(buffer->savePath) != 0)
            errCode = StatusCode::SCHEMA_FILE_NOT_FOUND;
        if (errCode == 0) // errCode非0时，依然会插入数据，但不会检测奇异性，且会返回错误码
        {
            errCode = checkNovelty(buffer); //此处若检测奇异性过程中出现异常，不会中止插入数据
            if (errCode == 0)
                addTime = 0;
        }
    }

    string savepath = buffer->savePath;
    if (savepath == "")
    {
        IOBusy = 0;
        return StatusCode::EMPTY_SAVE_PATH;
    }
    long fp;
    long curtime = getMilliTime();
    time_t time = curtime / 1000;
    struct tm *dateTime = localtime(&time);
    string fileID = FileIDManager::GetFileID(buffer->savePath);
    string finalPath = "";
    finalPath = finalPath.append(buffer->savePath).append("/").append(fileID).append(to_string(1900 + dateTime->tm_year)).append("-").append(to_string(1 + dateTime->tm_mon)).append("-").append(to_string(dateTime->tm_mday)).append("-").append(to_string(dateTime->tm_hour)).append("-").append(to_string(dateTime->tm_min)).append("-").append(to_string(dateTime->tm_sec)).append("-").append(to_string(curtime % 1000));
    char mode[2] = {'a', 'b'};
    if (addTime == 0)
    {
        finalPath.append(".idb");
        int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
        if (err == 0)
        {
            err = DB_Write(fp, buffer->buffer, buffer->length);
            if (err == 0)
            {
                IOBusy = 0;
                return DB_Close(fp);
            }
        }
        IOBusy = 0;
        return err;
    }
    else
    {
        finalPath.append(".idbzip");
        if (buffer->buffer[0] == 0) //数据未压缩
        {
            int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
            if (err == 0)
            {
                err = DB_Write(fp, buffer->buffer + 1, buffer->length - 1);
                if (err == 0)
                {
                    IOBusy = 0;
                    return DB_Close(fp);
                }
            }
            IOBusy = 0;
            return err;
        }
        else if (buffer->buffer[0] == 1) //数据完全压缩
        {

            int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
            err = DB_Close(fp);
            IOBusy = 0;
            return err;
        }
        else if (buffer->buffer[0] == 2) //数据未完全压缩
        {
            int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
            if (err == 0)
            {
                err = DB_Write(fp, buffer->buffer + 1, buffer->length - 1);
                if (err == 0)
                {
                    IOBusy = 0;
                    return DB_Close(fp);
                }
            }
            IOBusy = 0;
            return err;
        }
    }
    IOBusy = 0;
    return errCode;
}

/**
 * @brief 将多条数据(文件)存放在指定路径下，以文件ID+时间的方式命名
 * @param buffer[]    数据缓冲区
 * @param recordsNum  数据(文件)条数
 * @param addTime    是否添加时间戳
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  文件ID的暂定的命名方式为当前文件夹下的文件数量+1，
 *  时间戳格式为yyyy-mm-dd-hh-min-ss-ms
 */
int DB_InsertRecords(DB_DataBuffer buffer[], int recordsNum, int addTime)
{
    string savepath = buffer->savePath;
    if (savepath == "")
        return StatusCode::EMPTY_SAVE_PATH;
    long curtime = getMilliTime();
    time_t time = curtime / 1000;
    struct tm *dateTime = localtime(&time);
    string timestr = "";
    timestr.append(to_string(1900 + dateTime->tm_year)).append("-").append(to_string(1 + dateTime->tm_mon)).append("-").append(to_string(dateTime->tm_mday)).append("-").append(to_string(dateTime->tm_hour)).append("-").append(to_string(dateTime->tm_min)).append("-").append(to_string(dateTime->tm_sec)).append("-").append(to_string(curtime % 1000)).append(".idb");
    char mode[2] = {'a', 'b'};
    for (int i = 0; i < recordsNum; i++)
    {
        long fp;

        string fileID = FileIDManager::GetFileID(buffer->savePath);
        string finalPath = "";
        finalPath = finalPath.append(buffer->savePath).append("/").append(fileID).append(timestr);
        int err = DB_Open(const_cast<char *>(buffer[i].savePath), mode, &fp);
        if (err == 0)
        {
            err = DB_Write(fp, buffer[i].buffer, buffer[i].length);
            if (err != 0)
            {
                return err;
            }
            DB_Close(fp);
        }
        else
        {
            return err;
        }
    }
    return 0;
}

int main()
{
    DB_DataBuffer buffer;
    buffer.savePath = "JinfeiSeven/JinfeiSeven1527050_2022-5-12-18-10-35-620.idb";
    DB_ReadFile(&buffer);
    buffer.savePath = "JinfeiSeven";
    DB_InsertRecord(&buffer, 0);
    return 0;
}