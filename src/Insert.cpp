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

bool checkNovelty(DB_DataBuffer *buffer)
{
    // 指定py文件目录
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("if './' not in sys.path: sys.path.append('./')");

    PyObject *mymodule = PyImport_ImportModule("Novelty_Outlier");
    PyObject *pValue, *pArgs, *pFunc;
    PyObject *arr;
    long res = 0;
    if (mymodule != NULL)
    {
        // 从模块中获取函数
        pFunc = PyObject_GetAttrString(mymodule, "Novelty");

        if (pFunc && PyCallable_Check(pFunc))
        {
            // 创建参数元组
            pArgs = PyTuple_New(1);
            PyTuple_SetItem(pArgs, 0, arr);
            PyTuple_SetItem(pArgs, 1, PyLong_FromLong(1)); //暂定维度1
            // 函数执行
            PyObject *ret = PyObject_CallObject(pFunc, pArgs);
        }
    }
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
    if (!timerStarted && settings("Pack_Mode") == "timed")
    {
        int ret = pthread_create(&timer, NULL, checkTime, NULL);
        if (ret != 0)
        {
            cout << "pthread_create error: error_code=" << ret << endl;
        }
    }
    else if (settings("Pack_Mode") == "auto")
    {
    }
#endif
    if (!Py_IsInitialized())
        Py_Initialize();
    if (settings("Check_Novelty") == "true")
    {
        if (checkNovelty(buffer))
        {
        }
    }
    string savepath = buffer->savePath;
    if (savepath == "")
        return StatusCode::EMPTY_SAVE_PATH;
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
                return DB_Close(fp);
            }
        }
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
                    return DB_Close(fp);
                }
            }
            return err;
        }
        else if (buffer->buffer[0] == 1) //数据完全压缩
        {

            int err = DB_Open(const_cast<char *>(finalPath.c_str()), mode, &fp);
            err = DB_Close(fp);
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
                    return DB_Close(fp);
                }
            }
            return err;
        }
    }
    return 0;
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