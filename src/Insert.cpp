#include <utils.hpp>
#include <Insert.hpp>

void *checkSettings(void *ptr);
pthread_t timer; //计时器，另开一个新的线程
thread autoPackManager;
int timerStarted = false;
bool IOBusy = false;
int RepackThreshold = 0;
InsertBuffer insertBuffer;
void *checkTime(void *ptr)
{
    timerStarted = true;
    long interval = atol(settings("Pack_Interval").c_str());
    while (1)
    {
        cout << "checking settings" << endl;
        cout << timerStarted << endl;
        // while (IOBusy)
        // {
        //     // cout << "iobusy" << endl;
        // }
        if (settings("Pack_Mode") == "auto")
            return NULL;
        if (timerStarted == false)
            pthread_exit(NULL);
        long curTime = getMilliTime();
        // cout << "checking settings" << endl;
        cout << curTime % interval << " " << interval << endl;

        if (curTime % interval < interval)
        {
            vector<string> dirs;
            cout << settings("Filename_Label") << endl;
            readAllDirs(dirs, settings("Filename_Label"));
            cout << dirs.size() << endl;
            cout << settings("Filename_Label") << endl;
            cout << "Start packing...\n";
            for (auto &dir : dirs)
            {
                cout << "Packing " << dir << '\n';
                removeFilenameLabel(dir);
                DB_Pack(dir.c_str(), 0, 0);
            }
            cout << "Pack complete\n";
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
    long total, available;
    getMemoryUsage(total, available);
    vector<string> dirs;
    readAllDirs(dirs, settings("Filename_Label"));
    vector<int> packNums; //每个文件夹的建议打包数量
    for (auto &dir : dirs)
    {
        if (TemplateManager::SetTemplate(dir.c_str()) != 0)
            continue;
        int rhythmSize = CurrentTemplate.totalBytes;       //除去图片的节拍大小
        packNums.push_back(1024 * 1024 * 10 / rhythmSize); //初次打包时设定为10MB大小
    }
    Packer packer;
    while (1)
    {
        while (IOBusy)
        {
        }
        if (settings("Pack_Mode") != "auto")
            return;
        IOBusy = true;

        dirs.clear();
        readAllDirs(dirs, settings("Filename_Label"));
        vector<pair<string, long>> files;
        for (int i = 0; i < dirs.size(); i++)
        {
            files.clear();
            readDataFilesWithTimestamps(dirs[i].c_str(), files);
            if (files.size() >= packNums[i])
            {
                packer.Pack(dirs[i], files);
            }
        }
        long curTime = getMilliTime();
        if (curTime % (3600 * 1000 * 24) == 0)
        {
            for (int i = 0; i < dirs.size(); i++)
            {
                packer.RePack(dirs[i]);
            }
        }
        IOBusy = false;
        sleep(5);
    }
}
// thread settingsWatcher;
pthread_t settingsWatcher;
int settingsWatcherStarted = false;

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
    PyObject *pArgs, *pFunc;
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
        PyObject *val;
        if (schema.second.isArray)
        {
            PyObject *dim = PyLong_FromLong(schema.second.arrayLen);
            PyList_Append(dimensions, dim);
            val = PyList_New(schema.second.arrayLen);
            if (schema.second.valueType == ValueType::REAL)
            {
                float v = 0;
                for (int i = 0; i < schema.second.arrayLen; i++)
                {
                    memcpy(&v, buffer->buffer + cur, 4);
                    PyObject *value = PyFloat_FromDouble(v);
                    PyList_SetItem(val, i, value);
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
            PyObject *dim = PyLong_FromLong(1);
            PyList_Append(dimensions, dim);
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
    PyObject_Free(vals);
    PyObject_Free(names);
    PyObject_Free(dimensions);
    Py_XDECREF(pFunc);
    Py_XDECREF(mymodule);
    return StatusCode::PYTHON_SCRIPT_NOT_FOUND;
}

/**
 * @brief 将一个缓冲区中的一条数据(文件)存放在指定路径下，以文件ID+时间的方式命名
 * @param buffer    数据缓冲区
 * @param zip    是否压缩
 *
 * @return  0:success,
 *          others: StatusCode
 * @note 文件ID的暂定的命名方式为当前文件夹下的文件数量+1，
 *  时间戳格式为yyyy-mm-dd-hh-min-ss-ms
 */
int DB_InsertRecord(DB_DataBuffer *buffer, int zip)
{
    if (!settingsWatcherStarted)
    {
        pthread_create(&settingsWatcher, NULL, checkSettings, NULL);
        settingsWatcherStarted = true;
    }
    if (!timerStarted && settings("Pack_Mode") == "timed")
    {
        int ret = pthread_create(&timer, NULL, checkTime, NULL);
        if (ret != 0)
        {
            cout << "pthread_create error: error_code=" << ret << endl;
        }
    }
    // else if (settings("Pack_Mode") == "auto")
    // {
    //     autoPackManager = thread(autoPacker);
    //     autoPackManager.detach();
    // }
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
                zip = 0;
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
    string fileID = FileIDManager::GetFileID(buffer->savePath) + "_";
    string finalPath = "";
    finalPath = finalPath.append(buffer->savePath).append("/").append(fileID).append(to_string(1900 + dateTime->tm_year)).append("-").append(to_string(1 + dateTime->tm_mon)).append("-").append(to_string(dateTime->tm_mday)).append("-").append(to_string(dateTime->tm_hour)).append("-").append(to_string(dateTime->tm_min)).append("-").append(to_string(dateTime->tm_sec)).append("-").append(to_string(curtime % 1000));

    // insertBuffer.write(buffer, zip, finalPath);
    char mode[2] = {'a', 'b'};
    if (zip == 0)
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

int main()
{
    DB_DataBuffer buffer;
    char data[] = {'1', 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, '2'};
    buffer.buffer = data;
    buffer.length = 20;
    buffer.savePath = "testdata";
    for (int i = 0; i < 40; i++)
    {
    }

    return 0;
}

// int main()
// {
//     DB_DataBuffer buffer;
//     uint32_t uintval = 200;
//     char uintarr[4] = {0};
//     DataTypeConverter converter;
//     converter.ToInt32Buff(uintval, uintarr);
//     buffer.buffer = new char[60];
//     memcpy(buffer.buffer, uintarr, 4);
//     memcpy(buffer.buffer + 4, uintarr, 4);
//     memcpy(buffer.buffer + 8, uintarr, 4);
//     uintval = 300;
//     converter.ToInt32Buff(uintval, uintarr);
//     memcpy(buffer.buffer + 12, uintarr, 4);
//     memcpy(buffer.buffer + 16, uintarr, 4);
//     memcpy(buffer.buffer + 20, uintarr, 4);
//     uintval = 200;
//     converter.ToInt32Buff(uintval, uintarr);
//     memcpy(buffer.buffer + 24, uintarr, 4);
//     memcpy(buffer.buffer + 28, uintarr, 4);
//     memcpy(buffer.buffer + 32, uintarr, 4);
//     uintval = 300;
//     converter.ToInt32Buff(uintval, uintarr);
//     memcpy(buffer.buffer + 36, uintarr, 4);
//     memcpy(buffer.buffer + 40, uintarr, 4);
//     memcpy(buffer.buffer + 44, uintarr, 4);
//     uintval = 200;
//     converter.ToInt32Buff(uintval, uintarr);
//     memcpy(buffer.buffer + 48, uintarr, 4);
//     memcpy(buffer.buffer + 52, uintarr, 4);
//     memcpy(buffer.buffer + 56, uintarr, 4);
//     buffer.length = 60;
//     buffer.savePath = "RobotTsTest";
//     DB_InsertRecord(&buffer, 0);
//     return 0;
// }