#include "../include/ZipUtils.h"
using namespace std;

int ZipSwitchBuf(char *readbuff, char *writebuff, long &writebuff_pos);
int ReZipSwitchBuf(char *readbuff, const long len, char *writebuff, long &writebuff_pos);
int DB_ZipSwitchFile_thread(vector<pair<string, long>> selectedFiles, uint16_t begin, uint16_t num, const char *pathToLine);
int DB_ReZipSwitchFile_thread(vector<pair<string, long>> selectedFiles, uint16_t begin, uint16_t num, const char *pathToLine);

mutex openSwitchMutex;

/**
 * @brief 对readbuff里的数据进行压缩，压缩后数据保存在writebuff里，长度为writebuff_pos
 *
 * @param readbuff 需要进行压缩的数据
 * @param writebuff 压缩后的数据
 * @param writebuff_pos 压缩后数据的长度
 * @return 0:success,
 *          others: StatusCode
 */
int ZipSwitchBuf(char *readbuff, char *writebuff, long &writebuff_pos)
{
    long readbuff_pos = 0;
    DataTypeConverter converter;

    for (int i = 0; i < CurrentZipTemplate.schemas.size(); i++)
    {
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) //开关量的持续时长
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true) //是时间序列类型
            {
                if (ZipUtils::IsTSZip(i, writebuff, writebuff_pos, readbuff, readbuff_pos, ValueType::UDINT))
                {
                    cout << "存在开关量以外的类型(本功能针对开关量的持续时间,而不是bool值,如果是请替换为ZipFile功能)，请检查模板或者更换功能块" << endl;
                    return StatusCode::DATA_TYPE_MISMATCH_ERROR;
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型则不压缩
            {
                // ZipUtils::IsArrayZip(i, writebuff, writebuff_pos, readbuff, readbuff_pos);
                if (ZipUtils::IsArrayZip(i, writebuff, writebuff_pos, readbuff, readbuff_pos, ValueType::UDINT))
                {
                    cout << "存在开关量以外的类型(本功能针对开关量的持续时间,而不是bool值,如果是请替换为ZipFile功能)，请检查模板或者更换功能块" << endl;
                    return StatusCode::DATA_TYPE_MISMATCH_ERROR;
                }
            }
            else
            {
                uint32 standardBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                uint32 maxBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                uint32 minBoolTime = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节的持续时长,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                uint32 currentBoolTime = converter.ToUInt32(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime) //带时间戳
                {
                    //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                    ZipUtils::addZipPos(i, writebuff, writebuff_pos);

                    if (currentBoolTime != standardBoolTime && (currentBoolTime < minBoolTime || currentBoolTime > maxBoolTime))
                    {
                        //既有数据又有时间
                        ZipUtils::addZipType(DATA_AND_TIME, writebuff, writebuff_pos);
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes + ZIP_TIMESTAMP_SIZE);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + ZIP_TIMESTAMP_SIZE;
                    }
                    else
                    {
                        //只有时间
                        ZipUtils::addZipType(ONLY_TIME, writebuff, writebuff_pos);
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos + CurrentZipTemplate.schemas[i].second.valueBytes, ZIP_TIMESTAMP_SIZE);
                        writebuff_pos += ZIP_TIMESTAMP_SIZE;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + ZIP_TIMESTAMP_SIZE;
                }
                else //不带时间戳
                {
                    if (currentBoolTime != standardBoolTime && (currentBoolTime < minBoolTime || currentBoolTime > maxBoolTime))
                    {
                        //添加编号方便知道未压缩的变量是哪个，按照模板的顺序，从0开始，2个字节
                        ZipUtils::addZipPos(i, writebuff, writebuff_pos);
                        //只有数据
                        ZipUtils::addZipType(ONLY_DATA, writebuff, writebuff_pos);
                        memcpy(writebuff + writebuff_pos, readbuff + readbuff_pos, CurrentZipTemplate.schemas[i].second.valueBytes);
                        writebuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                    }
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
                }
            }
        }
        else
        {
            cout << "存在开关量以外的类型(本功能针对开关量的持续时间,而不是bool值,如果是请替换为ZipFile功能)，请检查模板或者更换功能块" << endl;
            return StatusCode::DATA_TYPE_MISMATCH_ERROR;
        }
    }
    return 0;
}

/**
 * @brief 对readbuff里的数据进行还原，还原后数据保存在writebuff里，长度为writebuff_pos
 *
 * @param readbuff 需要进行还原的数据
 * @param len 还原数据的长度
 * @param writebuff 还原后的数据
 * @param writebuff_pos 还原后数据的长度
 * @return 0:success,
 *          others: StatusCode
 */
int ReZipSwitchBuf(char *readbuff, const long len, char *writebuff, long &writebuff_pos)
{
    long readbuff_pos = 0;
    DataTypeConverter converter;

    for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++)
    {
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) //开关量持续时长
        {
            if (len == 0) //表示文件完全压缩
            {
                //添加上标准值到writebuff
                // ZipUtils::addStandardValue(i, writebuff, writebuff_pos, ValueType::UDINT);
                if (CurrentZipTemplate.schemas[i].second.isArray)
                    ZipUtils::addArrayStandardValue(i, writebuff, writebuff_pos, ValueType::UDINT);
                else
                    ZipUtils::addStandardValue(i, writebuff, writebuff_pos, ValueType::UDINT);
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[ZIP_ZIPPOS_SIZE] = {0};
                    memcpy(zipPosNum, readbuff + readbuff_pos, ZIP_ZIPPOS_SIZE);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);

                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += ZIP_ZIPPOS_SIZE + ZIP_ZIPTYPE_SIZE;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (ZipUtils::IsTSReZip(i, writebuff, writebuff_pos, readbuff, readbuff_pos, ValueType::UDINT))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            // if (ZipUtils::IsArrayReZip(i, writebuff, writebuff_pos, readbuff, readbuff_pos))
                            if (ZipUtils::IsArrayReZip(i, writebuff, writebuff_pos, readbuff, readbuff_pos, ValueType::UDINT))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                        else
                        {
                            if (ZipUtils::IsNotArrayAndTSReZip(i, writebuff, writebuff_pos, readbuff, readbuff_pos, ValueType::UDINT))
                                return StatusCode::ZIPTYPE_ERROR;
                        }
                    }
                    else //不是未压缩的编号
                    {
                        //添加上标准值到writebuff
                        // ZipUtils::addStandardValue(i, writebuff, writebuff_pos, ValueType::UDINT);
                        if (CurrentZipTemplate.schemas[i].second.isArray)
                            ZipUtils::addArrayStandardValue(i, writebuff, writebuff_pos, ValueType::UDINT);
                        else
                            ZipUtils::addStandardValue(i, writebuff, writebuff_pos, ValueType::UDINT);
                    }
                }
                else //没有未压缩的数据了
                {
                    //添加上标准值到writebuff
                    // ZipUtils::addStandardValue(i, writebuff, writebuff_pos, ValueType::UDINT);
                    if (CurrentZipTemplate.schemas[i].second.isArray)
                        ZipUtils::addArrayStandardValue(i, writebuff, writebuff_pos, ValueType::UDINT);
                    else
                        ZipUtils::addStandardValue(i, writebuff, writebuff_pos, ValueType::UDINT);
                }
            }
        }
        else
        {
            cout << "存在开关量以外的类型，请检查模板或者更换功能块" << endl;
            return StatusCode::DATA_TYPE_MISMATCH_ERROR;
        }
    }
    return 0;
}

/**
 * @brief 按文件夹压缩.idb文件，线程函数
 *
 * @param filesWithTime 带时间戳的.idb文件
 * @param begin 文件的开始下标
 * @param num 文件的数量
 * @param pathToLine 文件夹路径
 * @return 0:success,
 *          others:StatusCode
 */
int DB_ZipSwitchFile_thread(vector<pair<string, long>> selectedFiles, uint16_t begin, uint16_t num, const char *pathToLine)
{
    int err;

    //循环以给每个.idb文件进行压缩处理
    for (auto i = begin; i < num; i++)
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[i].first.c_str()), &len);
        char *readbuff = new char[len];                                                                    //文件内容
        char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据

        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(selectedFiles[i].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            IOBusy = false;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        err = ZipSwitchBuf(readbuff, writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后数据在writebuff里
        if (err)
            continue;

        if (writebuff_pos >= len) //表明数据没有被压缩,不做处理
        {
            cout << selectedFiles[i].first + "文件数据没有被压缩!" << endl;
        }
        else
        {
            DB_DeleteFile(const_cast<char *>(selectedFiles[i].first.c_str())); //删除原文件
            long fp;
            string finalpath = selectedFiles[i].first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
            //创建新文件并写入
            char mode[2] = {'w', 'b'};

            openSwitchMutex.lock();
            err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
            if (err == 0)
            {
                if (writebuff_pos != 0)
                    err = fwrite(writebuff, writebuff_pos, 1, (FILE *)fp);
                err = DB_Close(fp);
            }
            openSwitchMutex.unlock();
        }
        delete[] readbuff;
        delete[] writebuff;
    }
    return err;
}

/**
 * @brief 按文件夹还原.idbzip文件，线程函数
 *
 * @param filesWithTime 带时间戳的.idbzip文件
 * @param begin 文件的开始下标
 * @param num 文件的数量
 * @param pathToLine 文件夹路径
 * @return 0:success,
 *          others:StatusCode
 */
int DB_ReZipSwitchFile_thread(vector<pair<string, long>> selectedFiles, uint16_t begin, uint16_t num, const char *pathToLine)
{
    int err = 0;

    //循环以给每个.idbzip文件进行还原处理
    for (auto i = begin; i < num; i++)
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[i].first.c_str()), &len);
        char *readbuff = new char[len];                            //文件内容
        char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入被还原的数据

        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(selectedFiles[i].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            IOBusy = false;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        err = ReZipSwitchBuf(readbuff, len, writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
        if (err)
            continue;
        ;

        DB_DeleteFile(const_cast<char *>(selectedFiles[i].first.c_str())); //删除原文件
        long fp;
        string finalpath = selectedFiles[i].first.substr(0, selectedFiles[i].first.length() - 3); //去掉后缀的zip
        //创建新文件并写入
        char mode[2] = {'w', 'b'};
        openSwitchMutex.lock();
        err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
        if (err == 0)
        {
            err = fwrite(writebuff, writebuff_pos, 1, (FILE *)fp);
            err = DB_Close(fp);
        }
        openSwitchMutex.unlock();
        delete[] readbuff;
        delete[] writebuff;
    }
    return err;
}

/**
 * @brief 压缩只有开关量类型的已有.idb文件，单线程压缩
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idb文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipSwitchFile_Single(const char *ZipTemPath, const char *pathToLine)
{
    IOBusy = true;
    int err;
    if (ZipTemPath == NULL || pathToLine == NULL)
        return StatusCode::EMPTY_SAVE_PATH;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    vector<pair<string, long>> filesWithTime;
    readIDBFilesWithTimestamps(pathToLine, filesWithTime); //获取所有.idb文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(filesWithTime, TIME_ASC); //将文件按时间升序

    for (size_t fileNum = 0; fileNum < filesWithTime.size(); fileNum++) //循环以给每个.idb文件进行压缩处理
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(filesWithTime[fileNum].first.c_str()), &len);
        char *readbuff = new char[len];                                                                    //文件内容
        char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据

        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(filesWithTime[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            IOBusy = false;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        err = ZipSwitchBuf(readbuff, writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后数据在writebuff里
        if (err)
            continue;

        if (writebuff_pos >= len) //表明数据没有被压缩,不做处理
        {
            cout << filesWithTime[fileNum].first + "文件数据没有被压缩!" << endl;
            // return 1;//1表示数据没有被压缩
        }
        else
        {
            DB_DeleteFile(const_cast<char *>(filesWithTime[fileNum].first.c_str())); //删除原文件
            long fp;
            string finalpath = filesWithTime[fileNum].first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
            //创建新文件并写入
            char mode[2] = {'w', 'b'};
            err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
            if (err == 0)
            {
                if (writebuff_pos != 0)
                    // err = DB_Write(fp, writebuff, writebuff_pos);
                    err = fwrite(writebuff, writebuff_pos, 1, (FILE *)fp);
                err = DB_Close(fp);
            }
            delete[] readbuff;
            delete[] writebuff;
            // int fd = sysOpen(const_cast<char *>(finalpath.c_str()));
            // err = write(fd, writebuff, writebuff_pos);
            // if (err == -1)
            //     return errno;
            // err = close(fd);
        }
    }
    IOBusy = false;
    return err;
}

/**
 * @brief 多线程按文件夹压缩.idb文件函数，内核数>２时才有效
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idb文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipSwitchFile(const char *ZipTemPath, const char *pathToLine)
{
    int err;
    if (ZipTemPath == NULL || pathToLine == NULL)
        return StatusCode::EMPTY_SAVE_PATH;
    maxThreads = thread::hardware_concurrency();
    if (maxThreads <= 2) //如果内核数小于等于2则不如直接使用单线程
    {
        err = DB_ZipSwitchFile_Single(ZipTemPath, pathToLine);
        return err;
    }

    IOBusy = true;

    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    vector<pair<string, long>> filesWithTime;
    readIDBFilesWithTimestamps(pathToLine, filesWithTime); //获取所有.idb文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    //文件数小于1000则采用单线程压缩
    if (filesWithTime.size() < 1000)
    {
        IOBusy = false;
        err = DB_ZipSwitchFile_Single(ZipTemPath, pathToLine);
        return err;
    }
    sortByTime(filesWithTime, TIME_ASC); //将文件按时间升序

    uint16_t singleThreadFileNum = filesWithTime.size() / (maxThreads - 1);
    future_status status[maxThreads - 1];
    future<int> f[maxThreads - 1];
    uint16_t begin = 0;
    for (int j = 0; j < maxThreads - 2; j++) //循环开启多线程
    {
        f[j] = async(std::launch::async, DB_ZipSwitchFile_thread, filesWithTime, begin, singleThreadFileNum * (j + 1), pathToLine);
        begin += singleThreadFileNum;
        status[j] = f[j].wait_for(chrono::milliseconds(1));
    }
    f[maxThreads - 2] = async(std::launch::async, DB_ZipSwitchFile_thread, filesWithTime, begin, filesWithTime.size(), pathToLine);
    status[maxThreads - 2] = f[maxThreads - 2].wait_for(chrono::milliseconds(1));
    for (int j = 0; j < maxThreads - 1; j++) //等待所有线程结束
    {
        if (status[j] != future_status::ready)
            f[j].wait();
    }

    IOBusy = false;
    return err;
}

/**
 * @brief 还原压缩后的开关量文件为原状态，单线程
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idbzip压缩文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ReZipSwitchFile_Single(const char *ZipTemPath, const char *pathToLine)
{
    IOBusy = true;
    int err = 0;
    if (ZipTemPath == NULL || pathToLine == NULL)
        return StatusCode::EMPTY_SAVE_PATH;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    vector<pair<string, long>> filesWithTime;
    readIDBZIPFilesWithTimestamps(pathToLine, filesWithTime); //获取所有.idbzip文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(filesWithTime, TIME_ASC); //将文件按时间升序

    for (size_t fileNum = 0; fileNum < filesWithTime.size(); fileNum++) //循环以给每个.idbzip文件进行还原处理
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(filesWithTime[fileNum].first.c_str()), &len);
        char *readbuff = new char[len];                            //文件内容
        char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入被还原的数据

        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(filesWithTime[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            IOBusy = false;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        err = ReZipSwitchBuf(readbuff, len, writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
        if (err)
            continue;

        DB_DeleteFile(const_cast<char *>(filesWithTime[fileNum].first.c_str())); //删除原文件
        long fp;
        string finalpath = filesWithTime[fileNum].first.substr(0, filesWithTime[fileNum].first.length() - 3); //去掉后缀的zip
        //创建新文件并写入
        char mode[2] = {'w', 'b'};
        err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
        if (err == 0)
        {
            err = fwrite(writebuff, writebuff_pos, 1, (FILE *)fp);
            err = DB_Close(fp);
        }
        delete[] readbuff;
        delete[] writebuff;
        // int fd = sysOpen(const_cast<char *>(finalpath.c_str()));
        // err = write(fd, writebuff, writebuff_pos);
        // if (err == -1)
        //     return errno;
        // err = close(fd);
    }
    IOBusy = false;
    return err;
}

/**
 * @brief 多线程按文件夹还原.idbzip文件函数，内核数>２时才有效
 *
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idbzip文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ReZipSwitchFile(const char *ZipTemPath, const char *pathToLine)
{
    int err = 0;
    if (ZipTemPath == NULL || pathToLine == NULL)
        return StatusCode::EMPTY_SAVE_PATH;
    maxThreads = thread::hardware_concurrency();
    if (maxThreads <= 2) //如果内核数小于等于2则不如使用单线程
    {
        err = DB_ReZipSwitchFile_Single(ZipTemPath, pathToLine);
        return err;
    }

    IOBusy = true;

    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    vector<pair<string, long>> filesWithTime;
    readIDBZIPFilesWithTimestamps(pathToLine, filesWithTime); //获取所有.idbzip文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(filesWithTime, TIME_ASC); //将文件按时间升序

    //如果文件数量小于1000则采用单线程还原
    if (filesWithTime.size() < 1000)
    {
        IOBusy = false;
        err = DB_ReZipSwitchFile_Single(ZipTemPath, pathToLine);
        return err;
    }

    uint16_t singleThreadFileNum = filesWithTime.size() / (maxThreads - 1);
    future_status status[maxThreads - 1];
    future<int> f[maxThreads - 1];
    uint16_t begin = 0;
    for (int j = 0; j < maxThreads - 2; j++) //循环开启多线程
    {
        f[j] = async(std::launch::async, DB_ReZipSwitchFile_thread, filesWithTime, begin, singleThreadFileNum * (j + 1), pathToLine);
        begin += singleThreadFileNum;
        status[j] = f[j].wait_for(chrono::milliseconds(1));
    }
    f[maxThreads - 2] = async(std::launch::async, DB_ReZipSwitchFile_thread, filesWithTime, begin, filesWithTime.size(), pathToLine);
    status[maxThreads - 2] = f[maxThreads - 2].wait_for(chrono::milliseconds(1));
    for (int j = 0; j < maxThreads - 1; j++) //等待所有线程结束
    {
        if (status[j] != future_status::ready)
            f[j].wait();
    }

    IOBusy = false;
    return err;
}

/**
 * @brief 压缩接收到只有开关量类型的整条数据
 *
 * @param ZipTemPath 压缩模板路径
 * @param filepath 存储文件路径
 * @param buff 接收到的数据
 * @param buffLength 接收的数据长度
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipRecvSwitchBuff(const char *ZipTemPath, const char *filepath, char *buff, long *buffLength)
{
    int err = 0;
    if (ZipTemPath == NULL)
        return StatusCode::EMPTY_SAVE_PATH;
    err = DB_LoadZipSchema(ZipTemPath); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    long len = *buffLength;

    char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据

    long writebuff_pos = 0;

    err = ZipSwitchBuf(buff, writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后数据在writebuff里
    if (err)
        return StatusCode::ZIPTYPE_ERROR;

    if (writebuff_pos >= len) //表明数据没有被压缩
    {
        char isZip[1] = {0};
        char finnalBuf[len + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, buff, *buffLength);
        memcpy(buff, finnalBuf, len + 1);
        *buffLength += 1;
    }
    else if (writebuff_pos == 0) //数据完全压缩
    {
        char isZip[1] = {1};
        char finnalBuf[writebuff_pos + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, writebuff, writebuff_pos);
        memcpy(buff, finnalBuf, writebuff_pos + 1);
        *buffLength = writebuff_pos + 1;
    }
    else //数据未完全压缩
    {
        char isZip[1] = {2};
        char finnalBuf[writebuff_pos + 1];

        memcpy(finnalBuf, isZip, 1);
        memcpy(finnalBuf + 1, writebuff, writebuff_pos);
        memcpy(buff, finnalBuf, writebuff_pos + 1);
        *buffLength = writebuff_pos + 1;
    }

    DB_DataBuffer databuff;
    databuff.length = *buffLength;
    databuff.buffer = buff;
    databuff.savePath = filepath;
    DB_InsertRecord(&databuff, 1);
    delete[] writebuff;
    return err;
}

/**
 * @brief 根据时间段压缩开关量类型.idb文件，单线程
 *
 * @param params 压缩请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ZipSwitchFileByTimeSpan_Single(struct DB_ZipParams *params)
{
    IOBusy = true;
    params->ZipType = TIME_SPAN;
    int err = CheckZipParams(params);
    if (err != 0)
        return err;

    vector<pair<string, long>> filesWithTime, selectedFiles;
    readIDBFilesWithTimestamps(params->pathToLine, filesWithTime); //获取所有.idb文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }
    if (selectedFiles.size() == 0)
    {
        cout << "没有这个时间段的文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(selectedFiles, TIME_ASC);

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    for (size_t fileNum = 0; fileNum < selectedFiles.size(); fileNum++) //循环以给每个.idb文件进行压缩处理
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
        char *readbuff = new char[len];                                                                    //文件内容
        char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据

        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            IOBusy = false;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        err = ZipSwitchBuf(readbuff, writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后数据在writebuff里
        if (err)
            continue;

        if (writebuff_pos >= len) //表明数据没有被压缩,不做处理
        {
            cout << selectedFiles[fileNum].first + "文件数据没有被压缩!" << endl;
            // return 1;//1表示数据没有被压缩
        }
        else
        {
            DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
            long fp;
            string finalpath = selectedFiles[fileNum].first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
            //创建新文件并写入
            char mode[2] = {'w', 'b'};
            err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
            if (err == 0)
            {
                if (writebuff_pos != 0)
                    err = DB_Write(fp, writebuff, writebuff_pos);
                DB_Close(fp);
            }
        }
        delete[] readbuff;
        delete[] writebuff;
    }
    IOBusy = false;
    return err;
}

/**
 * @brief 多线程按时间段压缩.idb文件函数，内核数>２时才有效
 *
 * @param params 压缩请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ZipSwitchFileByTimeSpan(struct DB_ZipParams *params)
{
    params->ZipType = TIME_SPAN;
    int err = CheckZipParams(params);
    if (err != 0)
        return err;

    maxThreads = thread::hardware_concurrency();
    if (maxThreads <= 2) //如果内核数小于等于2则不如使用单线程
    {
        err = DB_ZipSwitchFileByTimeSpan_Single(params);
        return err;
    }

    IOBusy = true;

    vector<pair<string, long>> filesWithTime, selectedFiles;
    readIDBFilesWithTimestamps(params->pathToLine, filesWithTime); //获取所有.idb文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }
    if (selectedFiles.size() == 0)
    {
        cout << "没有这个时间段的文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    //文件数小于1000则采用单线程压缩
    if (selectedFiles.size() < 1000)
    {
        IOBusy = false;
        err = DB_ZipSwitchFileByTimeSpan_Single(params);
        return err;
    }
    sortByTime(selectedFiles, TIME_ASC);

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    uint16_t singleThreadFileNum = selectedFiles.size() / maxThreads;
    future_status status[maxThreads];
    future<int> f[maxThreads];
    uint16_t begin = 0;
    for (int j = 0; j < maxThreads - 1; j++) //循环开启线程
    {
        f[j] = async(std::launch::async, DB_ZipSwitchFile_thread, selectedFiles, begin, singleThreadFileNum * (j + 1), params->pathToLine);
        begin += singleThreadFileNum;
        status[j] = f[j].wait_for(chrono::milliseconds(1));
    }
    f[maxThreads - 1] = async(std::launch::async, DB_ZipSwitchFile_thread, selectedFiles, begin, selectedFiles.size(), params->pathToLine);
    status[maxThreads - 1] = f[maxThreads - 1].wait_for(chrono::milliseconds(1));
    for (int j = 0; j < maxThreads - 1; j++) //等待线程结束
    {
        if (status[j] != future_status::ready)
            f[j].wait();
    }

    IOBusy = false;
    return err;
}

/**
 * @brief 根据时间段还原开关量类型.idbzip文件，单线程
 *
 * @param params 还原请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ReZipSwitchFileByTimeSpan_Single(struct DB_ZipParams *params)
{
    IOBusy = true;
    params->ZipType = TIME_SPAN;
    int err = CheckZipParams(params);
    if (err != 0)
        return err;

    vector<pair<string, long>> filesWithTime, selectedFiles;
    readIDBZIPFilesWithTimestamps(params->pathToLine, filesWithTime); //获取所有.idbzip文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }
    if (selectedFiles.size() == 0)
    {
        cout << "没有这个时间段的文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    sortByTime(selectedFiles, TIME_ASC);

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;

    for (size_t fileNum = 0; fileNum < selectedFiles.size(); fileNum++) //循环以给每个.idbzip文件进行压缩处理
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
        char *readbuff = new char[len];                            //文件内容
        char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入被还原的数据

        long writebuff_pos = 0;

        if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
        {
            cout << "未找到文件" << endl;
            IOBusy = false;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        err = ReZipSwitchBuf(readbuff, len, writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
        if (err)
            continue;

        DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
        long fp;
        string finalpath = selectedFiles[fileNum].first.substr(0, selectedFiles[fileNum].first.length() - 3); //去掉后缀的zip
        //创建新文件并写入
        char mode[2] = {'w', 'b'};
        err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
        if (err == 0)
        {
            err = DB_Write(fp, writebuff, writebuff_pos);
            DB_Close(fp);
        }
        delete[] readbuff;
        delete[] writebuff;
    }
    IOBusy = false;
    return err;
}

/**
 * @brief 多线程按时间段还原.idbzip文件函数，内核数>２时才有效
 *
 * @param params 压缩请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ReZipSwitchFileByTimeSpan(struct DB_ZipParams *params)
{
    params->ZipType = TIME_SPAN;
    int err = CheckZipParams(params);
    if (err != 0)
        return err;

    maxThreads = thread::hardware_concurrency();
    if (maxThreads <= 2) //如果内核数小于等于2则不如使用单线程
    {
        err = DB_ReZipSwitchFileByTimeSpan_Single(params);
        return err;
    }

    IOBusy = true;

    vector<pair<string, long>> filesWithTime, selectedFiles;
    readIDBZIPFilesWithTimestamps(params->pathToLine, filesWithTime); //获取所有.idbzip文件，并带有时间戳
    if (filesWithTime.size() == 0)
    {
        cout << "没有文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    //筛选落入时间区间内的文件
    for (auto &file : filesWithTime)
    {
        if (file.second >= params->start && file.second <= params->end)
        {
            selectedFiles.push_back(make_pair(file.first, file.second));
        }
    }
    if (selectedFiles.size() == 0)
    {
        cout << "没有这个时间段的文件！" << endl;
        IOBusy = false;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    //文件数小于1000则使用单线程还原
    if (selectedFiles.size() < 1000)
    {
        IOBusy = false;
        err = DB_ReZipSwitchFileByTimeSpan_Single(params);
        return err;
    }
    sortByTime(selectedFiles, TIME_ASC);

    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        IOBusy = false;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    uint16_t singleThreadFileNum = selectedFiles.size() / maxThreads;
    future_status status[maxThreads];
    future<int> f[maxThreads];
    uint16_t begin = 0;
    for (int j = 0; j < maxThreads - 1; j++) //循环开启线程
    {
        f[j] = async(std::launch::async, DB_ReZipSwitchFile_thread, selectedFiles, begin, singleThreadFileNum * (j + 1), params->pathToLine);
        begin += singleThreadFileNum;
        status[j] = f[j].wait_for(chrono::milliseconds(1));
    }
    f[maxThreads - 1] = async(std::launch::async, DB_ReZipSwitchFile_thread, selectedFiles, begin, selectedFiles.size(), params->pathToLine);
    status[maxThreads - 1] = f[maxThreads - 1].wait_for(chrono::milliseconds(1));
    for (int j = 0; j < maxThreads - 1; j++) //等待线程结束
    {
        if (status[j] != future_status::ready)
            f[j].wait();
    }

    IOBusy = false;
    return err;
}

/**
 * @brief 根据文件ID压缩开关量类型.idb文件,单线程
 *
 * @param params 压缩请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ZipSwitchFileByFileID_Single(struct DB_ZipParams *params)
{
    params->ZipType = FILE_ID;
    int err = CheckZipParams(params); //检查压缩参数是否合法
    if (err != 0)
        return err;

    string pathToLine = params->pathToLine;
    string fileid = params->fileID; //用于记录fileID，前面会带产线名称
    while (pathToLine[pathToLine.length() - 1] == '/')
    {
        pathToLine.pop_back();
    }

    vector<string> paths = DataType::splitWithStl(pathToLine, "/");
    if (paths.size() > 0)
    {
        if (fileid.find(paths[paths.size() - 1]) == string::npos)
            fileid = paths[paths.size() - 1] + fileid;
    }
    else
    {
        if (fileid.find(paths[0]) == string::npos)
            fileid = paths[0] + fileid;
    }

    //加载压缩模板
    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    if (params->zipNums > 1) //根据SID以及zipNums压缩
    {
        vector<pair<string, long>> selectedFiles;
        readIDBFilesListBySIDandNum(params->pathToLine, params->fileID, params->zipNums, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        for (auto fileNum = 0; fileNum < selectedFiles.size(); fileNum++) //循环以给每个.idb文件进行压缩处理
        {
            long len;
            DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
            char *readbuff = new char[len];                                                                    //文件内容
            char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据
            long writebuff_pos = 0;

            if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
            {
                cout << "未找到文件" << endl;
                return StatusCode::DATAFILE_NOT_FOUND;
            }

            err = ZipSwitchBuf(readbuff, writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后的数据存在writebuff中
            if (err)
                continue;

            if (writebuff_pos >= len) //表明数据没有被压缩,不做处理
            {
                cout << selectedFiles[fileNum].first + "文件数据没有被压缩!" << endl;
                // return 1;//1表示数据没有被压缩
            }
            else
            {
                DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
                long fp;
                string finalpath = selectedFiles[fileNum].first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
                //创建新文件并写入
                char mode[2] = {'w', 'b'};

                err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
                if (err == 0)
                {
                    if (writebuff_pos != 0)
                        err = DB_Write(fp, writebuff, writebuff_pos);
                    err = DB_Close(fp);
                }
            }
            delete[] readbuff;
            delete[] writebuff;
        }
    }
    else if (params->EID != NULL) //根据SID以及EID压缩
    {
        vector<pair<string, long>> selectedFiles;
        readIDBFilesListBySIDandEID(params->pathToLine, params->fileID, params->EID, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        for (auto fileNum = 0; fileNum < selectedFiles.size(); fileNum++) //循环以给每个.idb文件进行压缩处理
        {
            long len;
            DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
            char *readbuff = new char[len];                                                                    //文件内容
            char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据
            long writebuff_pos = 0;

            if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
            {
                cout << "未找到文件" << endl;
                IOBusy = false;
                return StatusCode::DATAFILE_NOT_FOUND;
            }

            err = ZipSwitchBuf(readbuff, writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后数据在writebuff里
            if (err)
                continue;

            if (writebuff_pos >= len) //表明数据没有被压缩,不做处理
            {
                cout << selectedFiles[fileNum].first + "文件数据没有被压缩!" << endl;
                // return 1;//1表示数据没有被压缩
            }
            else
            {
                DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
                long fp;
                string finalpath = selectedFiles[fileNum].first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
                //创建新文件并写入
                char mode[2] = {'w', 'b'};

                err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
                if (err == 0)
                {
                    if (writebuff_pos != 0)
                        err = DB_Write(fp, writebuff, writebuff_pos);
                    err = DB_Close(fp);
                }
            }
            delete[] readbuff;
            delete[] writebuff;
        }
    }
    else //只压缩SID一个文件
    {
        vector<pair<string, long>> Files;
        readIDBFilesWithTimestamps(params->pathToLine, Files);
        if (Files.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        sortByTime(Files, TIME_ASC);
        int flag = 0; //用于标志是否找到文件

        for (auto &file : Files) //遍历寻找ID
        {
            if (file.first.find(fileid) != string::npos) //找到了
            {
                flag = 1;
                long len;
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char *readbuff = new char[len];                                                                    //文件内容
                char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据
                long writebuff_pos = 0;

                if (DB_OpenAndRead(const_cast<char *>(file.first.c_str()), readbuff)) //将文件内容读取到readbuff
                {
                    cout << "未找到文件" << endl;
                    return StatusCode::DATAFILE_NOT_FOUND;
                }

                err = ZipSwitchBuf(readbuff, writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后的数据存在writebuff中
                if (err)
                    break;

                if (writebuff_pos >= len) //表明数据没有被压缩,不做处理
                {
                    cout << file.first + "文件数据没有被压缩!" << endl;
                    // return 1;//1表示数据没有被压缩
                    char *data = (char *)malloc(len);
                    memcpy(data, readbuff, len);
                    params->buffer = data; //将数据记录在params->buffer中
                    params->bufferLen = len;
                }
                else
                {
                    char *data = (char *)malloc(writebuff_pos);
                    memcpy(data, writebuff, writebuff_pos);
                    params->buffer = data; //将压缩后数据记录在params->buffer中
                    params->bufferLen = writebuff_pos;

                    DB_DeleteFile(const_cast<char *>(file.first.c_str())); //删除原文件
                    long fp;
                    string finalpath = file.first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
                    //创建新文件并写入
                    char mode[2] = {'w', 'b'};
                    err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
                    if (err == 0)
                    {
                        if (writebuff_pos != 0)
                            err = DB_Write(fp, writebuff, writebuff_pos);
                        DB_Close(fp);
                    }
                }
                delete[] readbuff;
                delete[] writebuff;
                break;
            }
        }
        if (flag == 0)
            return StatusCode::DATAFILE_NOT_FOUND;
    }
    return err;
}

/**
 * @brief 多线程按时间段压缩.idb文件函数，内核数>２时才有效
 *
 * @param params 压缩请求参数
 * @return int
 */
int DB_ZipSwitchFileByFileID(struct DB_ZipParams *params)
{
    params->ZipType = FILE_ID;
    int err = CheckZipParams(params); //检查压缩参数是否合法
    if (err != 0)
        return err;

    maxThreads = thread::hardware_concurrency();
    if (maxThreads <= 2) //如果内核数小于等于2则不如使用单线程
    {
        err = DB_ZipSwitchFileByFileID_Single(params);
        return err;
    }

    string pathToLine = params->pathToLine;
    string fileid = params->fileID; //用于记录fileID，前面会带产线名称
    while (pathToLine[pathToLine.length() - 1] == '/')
    {
        pathToLine.pop_back();
    }

    vector<string> paths = DataType::splitWithStl(pathToLine, "/");
    if (paths.size() > 0)
    {
        if (fileid.find(paths[paths.size() - 1]) == string::npos)
            fileid = paths[paths.size() - 1] + fileid;
    }
    else
    {
        if (fileid.find(paths[0]) == string::npos)
            fileid = paths[0] + fileid;
    }

    //加载压缩模板
    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    IOBusy = true;

    if (params->zipNums > 1) //根据SID以及zipNums压缩
    {
        vector<pair<string, long>> selectedFiles;
        readIDBFilesListBySIDandNum(params->pathToLine, params->fileID, params->zipNums, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            IOBusy = false;
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        //当文件数小于1000时，采用单线程压缩
        if (selectedFiles.size() < 1000)
        {
            IOBusy = false;
            err = DB_ZipSwitchFileByFileID_Single(params);
            return err;
        }

        uint16_t singleThreadFileNum = selectedFiles.size() / maxThreads;
        future_status status[maxThreads];
        future<int> f[maxThreads];
        uint16_t begin = 0;
        for (int j = 0; j < maxThreads - 1; j++) //循环开启线程
        {
            f[j] = async(std::launch::async, DB_ZipSwitchFile_thread, selectedFiles, begin, singleThreadFileNum * (j + 1), params->pathToLine);
            begin += singleThreadFileNum;
            status[j] = f[j].wait_for(chrono::milliseconds(1));
        }
        f[maxThreads - 1] = async(std::launch::async, DB_ZipSwitchFile_thread, selectedFiles, begin, selectedFiles.size(), params->pathToLine);
        status[maxThreads - 1] = f[maxThreads - 1].wait_for(chrono::milliseconds(1));
        for (int j = 0; j < maxThreads - 1; j++) //等待线程结束
        {
            if (status[j] != future_status::ready)
                f[j].wait();
        }

        IOBusy = false;
        return err;
    }
    else if (params->EID != NULL) //根据SID以及EID压缩
    {
        vector<pair<string, long>> selectedFiles;
        readIDBFilesListBySIDandEID(params->pathToLine, params->fileID, params->EID, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            IOBusy = false;
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        //当文件数小于1000时采用单线程压缩
        if (selectedFiles.size() < 1000)
        {
            IOBusy = false;
            err = DB_ZipSwitchFileByFileID_Single(params);
            return err;
        }

        uint16_t singleThreadFileNum = selectedFiles.size() / maxThreads;
        future_status status[maxThreads];
        future<int> f[maxThreads];
        uint16_t begin = 0;
        for (int j = 0; j < maxThreads - 1; j++) //循环开启线程
        {
            f[j] = async(std::launch::async, DB_ZipSwitchFile_thread, selectedFiles, begin, singleThreadFileNum * (j + 1), params->pathToLine);
            begin += singleThreadFileNum;
            status[j] = f[j].wait_for(chrono::milliseconds(1));
        }
        f[maxThreads - 1] = async(std::launch::async, DB_ZipSwitchFile_thread, selectedFiles, begin, selectedFiles.size(), params->pathToLine);
        status[maxThreads - 1] = f[maxThreads - 1].wait_for(chrono::milliseconds(1));
        for (int j = 0; j < maxThreads - 1; j++) //等待线程结束
        {
            if (status[j] != future_status::ready)
                f[j].wait();
        }

        IOBusy = false;
        return err;
    }
    else //只压缩SID一个文件
    {
        IOBusy = false;
        vector<pair<string, long>> Files;
        readIDBFilesWithTimestamps(params->pathToLine, Files);
        if (Files.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        sortByTime(Files, TIME_ASC);
        int flag = 0; //用于标志是否找到文件

        for (auto &file : Files) //遍历寻找ID
        {
            if (file.first.find(fileid) != string::npos) //找到了
            {
                flag = 1;
                long len;
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char *readbuff = new char[len];                                                                    //文件内容
                char *writebuff = new char[CurrentZipTemplate.totalBytes + 3 * CurrentZipTemplate.schemas.size()]; //写入被压缩的数据
                long writebuff_pos = 0;

                if (DB_OpenAndRead(const_cast<char *>(file.first.c_str()), readbuff)) //将文件内容读取到readbuff
                {
                    cout << "未找到文件" << endl;
                    return StatusCode::DATAFILE_NOT_FOUND;
                }

                err = ZipSwitchBuf(readbuff, writebuff, writebuff_pos); //调用函数对readbuff进行压缩，压缩后的数据存在writebuff中
                if (err)
                    break;

                if (writebuff_pos >= len) //表明数据没有被压缩,不做处理
                {
                    cout << file.first + "文件数据没有被压缩!" << endl;
                    // return 1;//1表示数据没有被压缩
                    char *data = (char *)malloc(len);
                    memcpy(data, readbuff, len);
                    params->buffer = data; //将数据记录在params->buffer中
                    params->bufferLen = len;
                }
                else
                {
                    char *data = (char *)malloc(writebuff_pos);
                    memcpy(data, writebuff, writebuff_pos);
                    params->buffer = data; //将压缩后数据记录在params->buffer中
                    params->bufferLen = writebuff_pos;

                    DB_DeleteFile(const_cast<char *>(file.first.c_str())); //删除原文件
                    long fp;
                    string finalpath = file.first.append("zip"); //给压缩文件后缀添加zip，暂定，根据后续要求更改
                    //创建新文件并写入
                    char mode[2] = {'w', 'b'};
                    err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
                    if (err == 0)
                    {
                        if (writebuff_pos != 0)
                            err = DB_Write(fp, writebuff, writebuff_pos);
                        DB_Close(fp);
                    }
                }
                delete[] readbuff;
                delete[] writebuff;
                break;
            }
        }
        if (flag == 0)
            return StatusCode::DATAFILE_NOT_FOUND;
        return 0;
    }
}

/**
 * @brief 根据文件ID还原开关量类型.idbzip文件，单线程
 *
 * @param params 还原请求参数
 * @return 0:success,
 *          others: StatusCode
 */
int DB_ReZipSwitchFileByFileID_Single(struct DB_ZipParams *params)
{
    params->ZipType = FILE_ID;
    int err = CheckZipParams(params); //检查还原参数是否合法
    if (err != 0)
        return err;

    string pathToLine = params->pathToLine;
    string fileid = params->fileID; //用于记录fileID，前面会带产线名称
    while (pathToLine[pathToLine.length() - 1] == '/')
    {
        pathToLine.pop_back();
    }

    vector<string> paths = DataType::splitWithStl(pathToLine, "/");
    if (paths.size() > 0)
    {
        if (fileid.find(paths[paths.size() - 1]) == string::npos)
            fileid = paths[paths.size() - 1] + fileid;
    }
    else
    {
        if (fileid.find(paths[0]) == string::npos)
            fileid = paths[0] + fileid;
    }

    //加载压缩模板
    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    if (params->zipNums > 1) //根据SID以及zipNums还原
    {
        vector<pair<string, long>> selectedFiles;
        readIDBZIPFilesListBySIDandNum(params->pathToLine, params->fileID, params->zipNums, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        for (auto fileNum = 0; fileNum < selectedFiles.size(); fileNum++) //循环以给每个.idbzip文件进行压缩处理
        {
            long len;
            DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
            char *readbuff = new char[len];                            //文件内容
            char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入被还原的数据
            long writebuff_pos = 0;

            if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
            {
                cout << "未找到文件" << endl;
                IOBusy = false;
                return StatusCode::DATAFILE_NOT_FOUND;
            }

            err = ReZipSwitchBuf(readbuff, len, writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
            if (err)
                continue;

            DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
            long fp;
            string finalpath = selectedFiles[fileNum].first.substr(0, selectedFiles[fileNum].first.length() - 3); //去掉后缀的zip
            //创建新文件并写入
            char mode[2] = {'w', 'b'};

            err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
            if (err == 0)
            {
                err = DB_Write(fp, writebuff, writebuff_pos);
                err = DB_Close(fp);
            }
            delete[] readbuff;
            delete[] writebuff;
        }
    }
    else if (params->EID != NULL) //根据SID以及EID还原
    {
        vector<pair<string, long>> selectedFiles;
        readIDBZIPFilesListBySIDandEID(params->pathToLine, params->fileID, params->EID, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        for (auto fileNum = 0; fileNum < selectedFiles.size(); fileNum++) //循环以给每个.idbzip文件进行压缩处理
        {
            long len;
            DB_GetFileLengthByPath(const_cast<char *>(selectedFiles[fileNum].first.c_str()), &len);
            char *readbuff = new char[len];                            //文件内容
            char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入被还原的数据
            long writebuff_pos = 0;

            if (DB_OpenAndRead(const_cast<char *>(selectedFiles[fileNum].first.c_str()), readbuff)) //将文件内容读取到readbuff
            {
                cout << "未找到文件" << endl;
                IOBusy = false;
                return StatusCode::DATAFILE_NOT_FOUND;
            }

            err = ReZipSwitchBuf(readbuff, len, writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
            if (err)
                continue;

            DB_DeleteFile(const_cast<char *>(selectedFiles[fileNum].first.c_str())); //删除原文件
            long fp;
            string finalpath = selectedFiles[fileNum].first.substr(0, selectedFiles[fileNum].first.length() - 3); //去掉后缀的zip
            //创建新文件并写入
            char mode[2] = {'w', 'b'};

            err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
            if (err == 0)
            {
                err = DB_Write(fp, writebuff, writebuff_pos);
                DB_Close(fp);
            }
            delete[] readbuff;
            delete[] writebuff;
        }
    }
    else //还原单个文件
    {
        vector<pair<string, long>> Files;
        readIDBZIPFilesWithTimestamps(params->pathToLine, Files);
        if (Files.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        sortByTime(Files, TIME_ASC);
        int flag; //用于标志文件是否找到

        for (auto &file : Files) //遍历寻找ID
        {
            if (file.first.find(fileid) != string::npos) //找到了
            {
                flag = 1;
                long len;
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char *readbuff = new char[len];                            //文件内容
                char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入被还原的数据
                long writebuff_pos = 0;

                if (DB_OpenAndRead(const_cast<char *>(file.first.c_str()), readbuff)) //将文件内容读取到readbuff
                {
                    cout << "未找到文件" << endl;
                    return StatusCode::DATAFILE_NOT_FOUND;
                }

                err = ReZipSwitchBuf(readbuff, len, writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
                if (err)
                    break;

                char *data = (char *)malloc(writebuff_pos);
                memcpy(data, writebuff, writebuff_pos);
                params->buffer = data; //将还原后的数据记录在params->buffer中
                params->bufferLen = writebuff_pos;

                DB_DeleteFile(const_cast<char *>(file.first.c_str())); //删除原文件
                long fp;
                string finalpath = file.first.substr(0, file.first.length() - 3); //去掉后缀的zip
                //创建新文件并写入
                char mode[2] = {'w', 'b'};
                err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
                if (err == 0)
                {
                    err = DB_Write(fp, writebuff, writebuff_pos);
                    DB_Close(fp);
                }
                delete[] readbuff;
                delete[] writebuff;
                break;
            }
        }
        if (flag == 0)
            return StatusCode::DATAFILE_NOT_FOUND;
    }
    return err;
}

/**
 * @brief 多线程按时间段还原.idbzip文件函数，内核数>２时才有效
 *
 * @param params 还原请求参数
 * @return int
 */
int DB_ReZipSwitchFileByFileID(struct DB_ZipParams *params)
{
    params->ZipType = FILE_ID;
    int err = CheckZipParams(params); //检查还原参数是否合法
    if (err != 0)
        return err;

    maxThreads = thread::hardware_concurrency();
    if (maxThreads <= 2) //如果内核数小于等于2则不如使用单线程
    {
        err = DB_ReZipSwitchFileByFileID_Single(params);
        return err;
    }

    string pathToLine = params->pathToLine;
    string fileid = params->fileID; //用于记录fileID，前面会带产线名称
    while (pathToLine[pathToLine.length() - 1] == '/')
    {
        pathToLine.pop_back();
    }

    vector<string> paths = DataType::splitWithStl(pathToLine, "/");
    if (paths.size() > 0)
    {
        if (fileid.find(paths[paths.size() - 1]) == string::npos)
            fileid = paths[paths.size() - 1] + fileid;
    }
    else
    {
        if (fileid.find(paths[0]) == string::npos)
            fileid = paths[0] + fileid;
    }

    //加载压缩模板
    err = DB_LoadZipSchema(params->pathToLine); //加载压缩模板
    if (err)
    {
        cout << "未加载模板" << endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }

    IOBusy = true;

    if (params->zipNums > 1) //根据SID以及zipNums还原
    {
        vector<pair<string, long>> selectedFiles;
        readIDBZIPFilesListBySIDandNum(params->pathToLine, params->fileID, params->zipNums, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            IOBusy = false;
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        //如果文件数量小于1000,采用单线程还原
        if (selectedFiles.size() < 1000)
        {
            IOBusy = false;
            err = DB_ReZipSwitchFileByFileID_Single(params);
            return err;
        }

        uint16_t singleThreadFileNum = selectedFiles.size() / maxThreads;
        future_status status[maxThreads];
        future<int> f[maxThreads];
        uint16_t begin = 0;
        for (int j = 0; j < maxThreads - 1; j++) //循环开启线程
        {
            f[j] = async(std::launch::async, DB_ReZipSwitchFile_thread, selectedFiles, begin, singleThreadFileNum * (j + 1), params->pathToLine);
            begin += singleThreadFileNum;
            status[j] = f[j].wait_for(chrono::milliseconds(1));
        }
        f[maxThreads - 1] = async(std::launch::async, DB_ReZipSwitchFile_thread, selectedFiles, begin, selectedFiles.size(), params->pathToLine);
        status[maxThreads - 1] = f[maxThreads - 1].wait_for(chrono::milliseconds(1));
        for (int j = 0; j < maxThreads - 1; j++) //等待线程结束
        {
            if (status[j] != future_status::ready)
                f[j].wait();
        }

        IOBusy = false;
        return err;
    }
    else if (params->EID != NULL) //根据SID以及EID还原
    {
        vector<pair<string, long>> selectedFiles;
        readIDBZIPFilesListBySIDandEID(params->pathToLine, params->fileID, params->EID, selectedFiles);
        if (selectedFiles.size() == 0)
        {
            IOBusy = false;
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }

        //如果文件数量小于1000,采用单线程还原
        if (selectedFiles.size() < 1000)
        {
            IOBusy = false;
            err = DB_ReZipSwitchFileByFileID_Single(params);
            return err;
        }

        uint16_t singleThreadFileNum = selectedFiles.size() / maxThreads;
        future_status status[maxThreads];
        future<int> f[maxThreads];
        uint16_t begin = 0;
        for (int j = 0; j < maxThreads - 1; j++) //循环开启线程
        {
            f[j] = async(std::launch::async, DB_ReZipSwitchFile_thread, selectedFiles, begin, singleThreadFileNum * (j + 1), params->pathToLine);
            begin += singleThreadFileNum;
            status[j] = f[j].wait_for(chrono::milliseconds(1));
        }
        f[maxThreads - 1] = async(std::launch::async, DB_ReZipSwitchFile_thread, selectedFiles, begin, selectedFiles.size(), params->pathToLine);
        status[maxThreads - 1] = f[maxThreads - 1].wait_for(chrono::milliseconds(1));
        for (int j = 0; j < maxThreads - 1; j++) //等待线程结束
        {
            if (status[j] != future_status::ready)
                f[j].wait();
        }

        IOBusy = false;
        return err;
    }
    else //还原单个文件
    {
        IOBusy = false;
        vector<pair<string, long>> Files;
        readIDBZIPFilesWithTimestamps(params->pathToLine, Files);
        if (Files.size() == 0)
        {
            cout << "没有文件！" << endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        sortByTime(Files, TIME_ASC);
        int flag; //用于标志文件是否找到

        for (auto &file : Files) //遍历寻找ID
        {
            if (file.first.find(fileid) != string::npos) //找到了
            {
                flag = 1;
                long len;
                DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                char *readbuff = new char[len];                            //文件内容
                char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入被还原的数据
                long writebuff_pos = 0;

                if (DB_OpenAndRead(const_cast<char *>(file.first.c_str()), readbuff)) //将文件内容读取到readbuff
                {
                    cout << "未找到文件" << endl;
                    return StatusCode::DATAFILE_NOT_FOUND;
                }

                err = ReZipSwitchBuf(readbuff, len, writebuff, writebuff_pos); //调用函数对readbuff进行还原，还原后的数据存在writebuff中
                if (err)
                    break;

                char *data = (char *)malloc(writebuff_pos);
                memcpy(data, writebuff, writebuff_pos);
                params->buffer = data; //将还原后的数据记录在params->buffer中
                params->bufferLen = writebuff_pos;

                DB_DeleteFile(const_cast<char *>(file.first.c_str())); //删除原文件
                long fp;
                string finalpath = file.first.substr(0, file.first.length() - 3); //去掉后缀的zip
                //创建新文件并写入
                char mode[2] = {'w', 'b'};
                err = DB_Open(const_cast<char *>(finalpath.c_str()), mode, &fp);
                if (err == 0)
                {
                    err = DB_Write(fp, writebuff, writebuff_pos);
                    DB_Close(fp);
                }
                delete[] readbuff;
                delete[] writebuff;
                break;
            }
        }
        if (flag == 0)
            return StatusCode::DATAFILE_NOT_FOUND;
    }
    return err;
}

// int main()
// {
//     // long len;
//     // DB_GetFileLengthByPath("/jinfei91_2022-4-1-19-28-49-807.idb",&len);
//     // cout<<len<<endl;
//     // char buff[len];
//     // DB_OpenAndRead("/jinfei91_2022-4-1-19-28-49-807.idb",buff);
//     // DB_DataBuffer buffer;
//     // buffer.buffer=buff;
//     // buffer.length=len;
//     // buffer.savePath="jinfei";
//     // for(int i=0;i<5000;i++)
//     // {
//     //     DB_InsertRecord(&buffer,0);
//     // }

//     sleep(1);
//     std::cout << "start 开关量文件 单线程压缩" << std::endl;
//     auto startTime = std::chrono::system_clock::now();
//     DB_ZipSwitchFile_Single("jinfei", "jinfei");
//     auto endTime = std::chrono::system_clock::now();
//     std::cout << "开关量文件 单线程压缩耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     std::cout << "end 开关量文件 单线程压缩" << std::endl
//               << std::endl;

//     sleep(1);
//     std::cout << "start 开关量文件 单线程还原" << std::endl;
//     startTime = std::chrono::system_clock::now();
//     DB_ReZipSwitchFile_Single("jinfei", "jinfei");
//     endTime = std::chrono::system_clock::now();
//     std::cout << "开关量文件 单线程还原耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     std::cout << "end 开关量文件 单线程还原" << std::endl
//               << std::endl;

//     // sleep(1);
//     // std::cout<<"start 开关量文件 多线程压缩"<<std::endl;
//     // startTime = std::chrono::system_clock::now();
//     // DB_ZipSwitchFile("JinfeiEleven", "JinfeiEleven");
//     // endTime = std::chrono::system_clock::now();
//     // std::cout << "开关量文件 多线程压缩耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     // std::cout<<"end 开关量文件 多线程压缩"<<std::endl<<std::endl;

//     // sleep(1);
//     // std::cout<<"start 开关量文件 多线程还原"<<std::endl;
//     // startTime = std::chrono::system_clock::now();
//     // //DB_ReZipSwitchFile("JinfeiEleven", "JinfeiEleven");
//     // endTime = std::chrono::system_clock::now();
//     // std::cout << "开关量文件 多线程还原耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     // std::cout<<"end 开关量文件 多线程还原"<<std::endl<<std::endl;

//     // struct tm t;
//     // t.tm_year = atoi("2022") - 1900;
//     // t.tm_mon = atoi("5") - 1;
//     // t.tm_mday = atoi("7");
//     // t.tm_hour = atoi("15");
//     // t.tm_min = atoi("0");
//     // t.tm_sec = atoi("0");
//     // t.tm_isdst = -1; //不设置夏令时
//     // time_t seconds = mktime(&t);
//     // int ms = atoi("0");
//     // long start = seconds * 1000 + ms;
//     // cout<<start<<endl;
//     DB_ZipParams zipParam;
//     zipParam.pathToLine = "jinfei";
//     zipParam.start = 1651903200000;
//     zipParam.end = 1651906800000;

//     // sleep(3);
//     // std::cout<<"start 开关量文件按时间段 单线程压缩"<<std::endl;
//     // auto startTime = std::chrono::system_clock::now();
//     // DB_ZipSwitchFileByTimeSpan_Single(&zipParam);
//     // auto endTime = std::chrono::system_clock::now();
//     // std::cout << "开关量文件按时间段 单线程压缩耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     // std::cout<<"end 开关量文件按时间段 单线程压缩"<<std::endl<<std::endl;

//     // sleep(3);
//     // std::cout<<"start 开关量文件按时间段 单线程还原"<<std::endl;
//     // startTime = std::chrono::system_clock::now();
//     // DB_ReZipSwitchFileByTimeSpan_Single(&zipParam);
//     // endTime = std::chrono::system_clock::now();
//     // std::cout << "开关量文件按时间段 单线程还原耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     // std::cout<<"end 开关量文件按时间段 单线程还原"<<std::endl<<std::endl;

//     // sleep(3);
//     // std::cout<<"start 开关量文件按时间段 多线程压缩"<<std::endl;
//     // startTime = std::chrono::system_clock::now();
//     // DB_ZipSwitchFileByTimeSpan(&zipParam);
//     // endTime = std::chrono::system_clock::now();
//     // std::cout << "开关量文件按时间段 多线程时间段压缩耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     // std::cout<<"end 开关量文件按时间段 多线程压缩"<<std::endl<<std::endl;

//     // sleep(3);
//     // std::cout<<"start 开关量文件按时间段 多线程还原"<<std::endl;
//     // startTime = std::chrono::system_clock::now();
//     // DB_ReZipSwitchFileByTimeSpan(&zipParam);
//     // endTime = std::chrono::system_clock::now();
//     // std::cout << "开关量文件按时间段 多线程时间段还原耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
//     // std::cout<<"end 开关量文件按时间段 多线程还原"<<std::endl<<std::endl;

//     return 0;
// }
// int main()
// {
//     DB_ZipSwitchFile("JinfeiSeven","JinfeiSeven");
//     return 0;
//     //     DB_ZipParams param;
//     //     param.ZipType = FILE_ID;
//     //     param.pathToLine = "JinfeiSeven";
//     //     param.fileID = "JinfeiSeven1810030";
//     //     param.zipNums = 40;
//     //     param.EID = NULL;
//     //     cout << DB_ZipSwitchFileByFileID(&param) << endl;
//     //     cout << DB_ReZipSwitchFileByFileID(&param) << endl;
//     //     // DB_ZipSwitchFile("RobotTS","RobotTS");
//     //     // DB_ReZipSwitchFile("RobotTsTest", "RobotTsTest");
//     //     return 0;
// }