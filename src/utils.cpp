#include <utils.hpp>
#include <BackupHelper.h>
unordered_map<string, int> getDirCurrentFileIDIndex();
long getMilliTime();

int maxTemplates = 20;
vector<Template> templates;
int errorCode;
// int maxThreads = thread::hardware_concurrency();
Logger RuntimeLogger("runtime");
FileIDManager fileIDManager;
neb::CJsonObject settings = FileIDManager::GetSetting();
queue<string> fileQueue = InitFileQueue();
unordered_map<string, int> curNum = getDirCurrentFileIDIndex();
PackManager packManager(atoi(settings("Pack_Cache_Size").c_str()) * 1024);
BackupHelper backupHelper;

/**
 * @brief 调取在python脚本中定义的函数，并获取返回结果
 *
 * @param Args 传入参数，必须为元组类型
 * @param moduleName .py文件名
 * @param funcName 函数名
 * @param path 可选，python脚本所在文件夹的路径
 * @return PyObject*
 */
PyObject *PythonCall(PyObject *Args, const char *moduleName, const char *funcName, const char *path)
{
    if (!Py_IsInitialized())
        Py_Initialize();
    string pySentence = "";
    pySentence.append("if '").append(path).append("' not in sys.path: sys.path.append('").append(path).append("')");
    // 指定py文件目录
    PyRun_SimpleString("import sys");
    PyRun_SimpleString(pySentence.c_str());

    PyObject *mymodule = PyImport_ImportModule(moduleName);
    PyObject *pFunc, *ret;
    if (mymodule != NULL)
    {
        // 从模块中获取函数
        pFunc = PyObject_GetAttrString(mymodule, funcName);

        if (pFunc && PyCallable_Check(pFunc))
        {
            // 函数执行
            ret = PyObject_CallObject(pFunc, Args);
        }
        Py_XDECREF(pFunc);
    }

    Py_XDECREF(mymodule);
    return ret;
}

/**
 * @brief 调取在python脚本中定义的函数，并获取返回结果，传入的不定参数类型必须为PyObject*
 *
 * @param moduleName .py文件名
 * @param funcName 函数名
 * @param path python脚本所在文件夹的路径
 * @param num 参数个数
 * @param ...
 * @return PyObject*
 */
PyObject *PythonCall(const char *moduleName, const char *funcName, const char *path, int num, ...)
{
    if (!Py_IsInitialized())
        Py_Initialize();
    //定义指向参数列表的变量
    va_list arg_ptr;

    //把上面这个变量初始化.即让它指向参数列表
    va_start(arg_ptr, num);
    PyObject *args = PyTuple_New(num);
    for (int i = 0; i < num; i++)
    {
        //获取arg_ptr指向的当前参数.这个参数的类型由va_arg的第2个参数指定
        PyObject *item = va_arg(arg_ptr, PyObject *);
        PyTuple_SetItem(args, i, item);
    }

    string pySentence = "";
    pySentence.append("if '").append(path).append("' not in sys.path: sys.path.append('").append(path).append("')");
    // 指定py文件目录
    PyRun_SimpleString("import sys");
    PyRun_SimpleString(pySentence.c_str());

    PyObject *mymodule = PyImport_ImportModule(moduleName);
    PyObject *pFunc, *ret;
    if (mymodule != NULL)
    {
        // 从模块中获取函数
        pFunc = PyObject_GetAttrString(mymodule, funcName);

        if (pFunc && PyCallable_Check(pFunc))
        {
            // 函数执行
            ret = PyObject_CallObject(pFunc, args);
        }
        Py_XDECREF(pFunc);
    }
    Py_DECREF(args);
    Py_XDECREF(mymodule);
    return ret;
}

/**
 * @brief 根据包路径和节拍位序获取图片，图片数据将放在buff中，使用前需要加载模版
 *
 * @param buff 缓存地址的二级指针
 * @param length 数据长度
 * @param path 文件路径
 * @param index 包内节拍位序
 * @param pathCode 路径编码
 * @return int
 */
int FindImage(char **buff, long &length, string &path, int index, char *pathCode)
{
    DB_DataBuffer buffer;
    buffer.savePath = path.c_str();
    DB_ReadFile(&buffer);
    if (buffer.bufferMalloced)
    {
        if (fs::path(path).extension() == ".pak")
        {
            PackFileReader reader(buffer.buffer, buffer.length);
            reader.Skip(index);
            int zipType, readLength;
            long timestamp;
            long dataPos = reader.Next(readLength, timestamp, zipType);
            vector<long> bytes, poses;
            vector<DataType> types;
            int err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, reader.packBuffer + dataPos, poses, bytes, types);
            if (err != 0)
                return err;
            long len = 0;
            for (int i = 0; i < types.size(); i++)
            {
                if (types[i].valueType == ValueType::IMAGE)
                    len += bytes[i] + 6;
            }
            *buff = new char[len];
            long cur = 0;
            for (int i = 0; i < types.size(); i++)
            {
                if (types[i].valueType == ValueType::IMAGE)
                {
                    memcpy(*buff + cur, reader.packBuffer + dataPos + poses[i] - 6, bytes[i] + 6);
                    cur += bytes[i] + 6;
                }
            }
            length = cur;
            if (cur != 0)
                return 0;
            else
                return StatusCode::IMG_NOT_FOUND;
        }
    }

    return StatusCode::DATAFILE_NOT_FOUND;
}

/**
 * @brief 根据包路径和节拍位序获取图片，图片数据将放在buff中，使用前需要加载模版
 *
 * @param buff 缓存地址的二级指针
 * @param length 数据长度
 * @param path 文件路径
 * @param index 包内节拍位序
 * @param valueName 变量名
 * @return int
 */
int FindImage(char **buff, long &length, string &path, int index, const char *valueName)
{
    DB_DataBuffer buffer;
    buffer.savePath = path.c_str();
    DB_ReadFile(&buffer);
    if (buffer.bufferMalloced)
    {
        if (path.find(".pak") != string::npos)
        {
            PackFileReader reader(buffer.buffer, buffer.length, true);
            reader.Skip(index);
            int zipType, readLength;
            long timestamp;
            long dataPos = reader.Next(readLength, timestamp, zipType);
            long pos, bytes;
            DataType type;
            int err = CurrentTemplate.FindDatatypePosByName(valueName, reader.packBuffer + dataPos, pos, bytes, type);
            if (err != 0)
                return err;
            if (type.valueType == ValueType::IMAGE)
            {
                *buff = new char[bytes + 6];
                memcpy(*buff, reader.packBuffer + dataPos + pos - 6, bytes + 6); //保留图片前的6字节大小
                length = bytes + 6;
                return 0;
            }
            return StatusCode::IMG_NOT_FOUND;
        }
    }

    return StatusCode::DATAFILE_NOT_FOUND;
}

/**
 * @brief 根据包路径和节拍位序获取图片，图片数据将放在buff中，使用前需要加载模版
 *
 * @param buff 缓存地址的二级指针
 * @param length 数据长度
 * @param path 文件路径
 * @param index 包内节拍位序
 * @param valueName 变量名
 * @return int
 */
int FindImage(char **buff, long &length, string &path, int index, vector<string> &names)
{
    DB_DataBuffer buffer;
    buffer.savePath = path.c_str();
    DB_ReadFile(&buffer);
    if (buffer.bufferMalloced)
    {
        if (path.find(".pak") != string::npos)
        {
            PackFileReader reader(buffer.buffer, buffer.length, true);
            reader.Skip(index);
            int zipType, readLength;
            long timestamp;
            long dataPos = reader.Next(readLength, timestamp, zipType);
            vector<long> bytes, poses;
            vector<DataType> types;
            int err = CurrentTemplate.FindMultiDatatypePosByNames(names, reader.packBuffer + dataPos, poses, bytes, types);
            if (err != 0)
                return err;
            long len = 0;
            for (int i = 0; i < types.size(); i++)
            {
                if (types[i].valueType == ValueType::IMAGE)
                    len += bytes[i] + 6;
            }
            *buff = new char[len];
            long cur = 0;
            for (int i = 0; i < types.size(); i++)
            {
                if (types[i].valueType == ValueType::IMAGE)
                {
                    memcpy(*buff + cur, reader.packBuffer + dataPos + poses[i] - 6, bytes[i] + 6);
                    cur += bytes[i] + 6;
                }
            }
            length = cur;
            if (cur != 0)
                return 0;
            else
                return StatusCode::IMG_NOT_FOUND;
        }
    }

    return StatusCode::DATAFILE_NOT_FOUND;
}

//获取当前工程根目录下所有产线-已有最大文件ID键值对
unordered_map<string, int> getDirCurrentFileIDIndex()
{
    string finalPath = settings("Filename_Label");
    unordered_map<string, int> map;
    if (DB_CreateDirectory(const_cast<char *>(finalPath.c_str())))
    {
        errorCode = errno;
        return map;
    }
    vector<string> dirs;
    dirs.push_back(settings("Filename_Label"));
    readAllDirs(dirs, finalPath);
    // cout<<dirs.size()<<endl;
    try
    {
        for (auto &d : dirs)
        {
            int max = 0;
            for (auto const &dir_entry : fs::directory_iterator{d})
            {
                if (!fs::is_regular_file(dir_entry))
                    continue;
                string fileName = dir_entry.path().filename();
                string dirWithoutPrefix = d + "/" + fileName;
                for (int i = 0; i <= settings("Filename_Label").length(); i++)
                {
                    dirWithoutPrefix.erase(dirWithoutPrefix.begin());
                }
                if (fs::path(fileName).extension() == ".pak")
                {
                    PackFileReader packReader(dirWithoutPrefix);
                    if (packReader.packBuffer == NULL)
                        continue;
                    int fileNum;
                    string templateName;
                    packReader.ReadPackHead(fileNum, templateName);
                    string fileID;
                    int readLength, zipType;
                    packReader.Next(readLength, fileID, zipType);
                    string startFID = fileID;
                    if (fileNum > 1)
                    {
                        cout << "skipping " << fileName << "\n";
                        packReader.Skip(fileNum - 2);
                        packReader.Next(readLength, fileID, zipType);
                    }
                    string endFID = fileID;
                    string startNum = "", endNum = "";
                    for (int i = 0; i < startFID.length(); i++)
                    {
                        if (isdigit(startFID[i]))
                        {
                            startNum += startFID[i];
                        }
                    }
                    for (int i = 0; i < endFID.length(); i++)
                    {
                        if (isdigit(endFID[i]))
                        {
                            endNum += endFID[i];
                        }
                    }
                    int minNum = atoi(startNum.c_str());
                    int maxNum = atoi(endNum.c_str());
                    fileIDManager.fidIndex[dirWithoutPrefix] = make_tuple(minNum, maxNum >= minNum ? maxNum : minNum);
                    if (max < maxNum)
                        max = maxNum;
                }
                else if (fs::path(fileName).extension() == ".idb")
                {
                    vector<string> vec = DataType::StringSplit(const_cast<char *>(fileName.c_str()), "_");
                    string num;
                    string fileID = vec[0];
                    for (int i = 0; i < fileID.length(); i++)
                    {
                        if (isdigit(fileID[i]))
                        {
                            num += fileID[i];
                        }
                    }
                    if (max < atoi(num.c_str()))
                        max = atoi(num.c_str());
                }
            }

            if (d == settings("Filename_Label"))
                map["/"] = max;
            else //去除前缀Label
            {
                for (int i = 0; i <= settings("Filename_Label").length(); i++)
                {
                    d.erase(d.begin());
                }
                map[d] = max;
            }
        }
    }
    catch (iedb_err &e)
    {
        // cerr << "Error code " << e.code << "\n";
        RuntimeLogger.critical("Error occured with code {}: {}", e.code, e.what());
    }
    catch (fs::filesystem_error &e)
    {
        RuntimeLogger.critical("File system error occured : {}", e.what());
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    return map;
}

/**
 * @brief 获取绝对时间(自1970/1/1至今),精确到毫秒
 *
 * @return  时间值
 * @note
 */
long getMilliTime()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * 1000 + time.tv_usec / 1000;
}

void removeFilenameLabel(string &path)
{
    for (int i = 0; i < settings("Filename_Label").length(); i++)
    {
        path.erase(path.begin());
    }
    while (path[0] == '/')
        path.erase(path.begin());
}

int getMemory(long size, char *mem)
{
    mem = (char *)malloc(size);
    if (mem == NULL)
    {
        errorCode = StatusCode::BUFFER_FULL;
        return errorCode;
    }
    return 0;
}
#ifndef __linux__
enum BYTE_UNITS
{
    BYTES = 0,
    KILOBYTES = 1,
    MEGABYTES = 2,
    GIGABYTES = 3
};

template <class T>
T convert_unit(T num, int to, int from = BYTES)
{
    for (; from < to; from++)
    {
        num /= 1024;
    }
    return num;
}
#endif
#ifdef __linux__
typedef struct MEMPACKED
{
    char name1[20];
    unsigned long MemTotal;
    char name2[20];
    unsigned long MemFree;
    char name3[20];
    unsigned long Buffers;
    char name4[20];
    unsigned long Cached;

} MEM_OCCUPY;

typedef struct os_line_data
{
    char *val;
    int len;
} os_line_data;

static char *os_getline(char *sin, os_line_data *line, char delim)
{
    char *out = sin;
    if (*out == '\0')
        return NULL;
    //	while (*out && (*out == delim)) { out++; }
    line->val = out;
    while (*out && (*out != delim))
    {
        out++;
    }
    line->len = out - line->val;
    //	while (*out && (*out == delim)) { out++; }
    if (*out && (*out == delim))
    {
        out++;
    }
    if (*out == '\0')
        return NULL;
    return out;
}
int Parser_EnvInfo(char *buffer, int size, MEM_OCCUPY *lpMemory)
{
    int state = 0;
    char *p = buffer;
    while (p)
    {
        os_line_data line = {0};
        p = os_getline(p, &line, ':');
        if (p == NULL || line.len <= 0)
            continue;

        if (line.len == 8 && strncmp(line.val, "MemTotal", 8) == 0)
        {
            char *point = strtok(p, " ");
            memcpy(lpMemory->name1, "MemTotal", 8);
            lpMemory->MemTotal = atol(point);
        }
        else if (line.len == 7 && strncmp(line.val, "MemFree", 7) == 0)
        {
            char *point = strtok(p, " ");
            memcpy(lpMemory->name2, "MemFree", 7);
            lpMemory->MemFree = atol(point);
        }
        else if (line.len == 7 && strncmp(line.val, "Buffers", 7) == 0)
        {
            char *point = strtok(p, " ");
            memcpy(lpMemory->name3, "Buffers", 7);
            lpMemory->Buffers = atol(point);
        }
        else if (line.len == 6 && strncmp(line.val, "Cached", 6) == 0)
        {
            char *point = strtok(p, " ");
            memcpy(lpMemory->name4, "Cached", 6);
            lpMemory->Cached = atol(point);
        }
    }
}

void get_procmeminfo(MEM_OCCUPY *lpMemory)
{
    FILE *fd;
    char buff[128] = {0};
    fd = fopen("/proc/meminfo", "r");
    if (fd < 0)
        return;
    fgets(buff, sizeof(buff), fd);
    Parser_EnvInfo(buff, sizeof(buff), lpMemory);

    fgets(buff, sizeof(buff), fd);
    Parser_EnvInfo(buff, sizeof(buff), lpMemory);

    fgets(buff, sizeof(buff), fd);
    Parser_EnvInfo(buff, sizeof(buff), lpMemory);

    fgets(buff, sizeof(buff), fd);
    Parser_EnvInfo(buff, sizeof(buff), lpMemory);

    fclose(fd);
}

#endif

/**
 * @brief Get the Memory Usage
 *
 * @param total
 * @param available
 */
void getMemoryUsage(long &total, long &available)
{
#ifdef __linux__
    MEM_OCCUPY mem;
    get_procmeminfo(&mem);
    total = mem.MemTotal;
    available = mem.MemFree;
#else
    u_int64_t total_mem = 0;
    float used_mem = 0;

    vm_size_t page_size;
    vm_statistics_data_t vm_stats;

    // Get total physical memory
    int mib[] = {CTL_HW, HW_MEMSIZE};
    size_t length = sizeof(total_mem);
    sysctl(mib, 2, &total_mem, &length, NULL, 0);

    mach_port_t mach_port = mach_host_self();
    mach_msg_type_number_t count = sizeof(vm_stats) / sizeof(natural_t);
    if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
        KERN_SUCCESS == host_statistics(mach_port, HOST_VM_INFO,
                                        (host_info_t)&vm_stats, &count))
    {
        used_mem = static_cast<float>(
            (vm_stats.active_count + vm_stats.wire_count) * page_size);
    }

    uint usedMem = convert_unit(static_cast<float>(used_mem), MEGABYTES);
    uint totalMem = convert_unit(static_cast<float>(total_mem), MEGABYTES);
    total = totalMem;
    available = totalMem - usedMem;

#endif
}

/**
 * @brief 获取数据类型代号
 *
 * @param type
 * @return int
 */
int getBufferValueType(DataType &type)
{
    int baseType = (int)type.valueType;
    if (type.hasTime && !type.isArray && !type.isTimeseries)
        return baseType + 10;
    if (!type.hasTime && type.isArray && !type.isTimeseries)
        return baseType + 20;
    if (type.isArray && type.hasTime && !type.isTimeseries)
        return baseType + 30;
    if (type.isTimeseries && !type.isArray)
        return baseType + 40;
    if (type.isTimeseries && type.isArray)
        return baseType + 50;
    return baseType;
}

/**
 * @brief 获取数据区中第N个变量在每行中的偏移量
 * @param typeList      数据类型列表
 * @param num           第num个
 *
 * @return  偏移量
 * @note
 */
int getBufferDataPos(vector<DataType> &typeList, int num)
{
    long pos = 0;
    for (int i = 0; i < num; i++)
    {
        pos += typeList[i].hasTime ? typeList[i].valueBytes + 8 : typeList[i].valueBytes;
    }

    return pos;
}

/**
 * @brief 在进行压缩操作前，检查输入的压缩参数是否合法
 * @param params        压缩请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int CheckZipParams(DB_ZipParams *params)
{
    if (params->pathToLine == NULL)
    {
        return StatusCode::EMPTY_PATH_TO_LINE;
    }
    switch (params->ZipType)
    {
    case TIME_SPAN:
    {
        if ((params->start == 0 && params->end == 0) || params->start > params->end)
        {
            return StatusCode::INVALID_TIMESPAN;
        }
        else if (params->end == 0)
        {
            params->end = getMilliTime();
        }
        break;
    }
    case PATH_TO_LINE:
    {
        if (params->pathToLine == 0)
        {
            return StatusCode::EMPTY_PATH_TO_LINE;
        }
        break;
    }
    case FILE_ID:
    {
        if (params->fileID == NULL)
        {
            return StatusCode::NO_FILEID;
        }
        else if (params->zipNums < 0 && params->EID == NULL)
            return StatusCode::ZIPNUM_ERROR;
        else if (params->zipNums > 1 && params->EID != NULL)
            return StatusCode::ZIPNUM_AND_EID_ERROR;
        break;
    }
    default:
        return StatusCode::NO_ZIP_TYPE;
        break;
    }
    return 0;
}

/**
 * @brief 根据压缩模版判断数据是否正常
 *
 * @param readbuff 数据缓存
 * @param pathToLine 数据所在路径
 * @return true
 * @return false
 */
bool IsNormalIDBFile(char *readbuff, const char *pathToLine)
{
    // int err = 0;
    // err = DB_LoadZipSchema(pathToLine); //加载压缩模板
    // if (err)
    // {
    //     cout << "未加载模板" << endl;
    //     return StatusCode::SCHEMA_FILE_NOT_FOUND;
    // }
    DataTypeConverter converter;
    long readbuff_pos = 0;

    for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++) //循环比较
    {
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::BOOL) // BOOL变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //既是时间序列又是数组
                {
                    continue;
                }
                else //只是时间序列类型
                {
                    char standardBool = CurrentZipTemplate.schemas[i].second.standardValue[0];

                    for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                    {
                        if (standardBool != readbuff[readbuff_pos])
                            return false;
                        else
                            readbuff_pos += 9;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型
            {
                continue;
            }
            else
            {
                char standardBool = CurrentZipTemplate.schemas[i].second.standardValue[0];

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                {
                    if (standardBool != readbuff[readbuff_pos])
                        return false;
                    else
                        readbuff_pos += 9;
                }
                else //不带时间戳
                {
                    if (standardBool != readbuff[readbuff_pos])
                        return false;
                    else
                        readbuff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //既是时间序列又是数组
                {
                    continue;
                }
                else //只是时间序列类型
                {
                    char standardUSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                    char maxUSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                    char minUSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];

                    for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                    {
                        // 1个字节,暂定，根据后续情况可能进行更改
                        char value[1] = {0};
                        memcpy(value, readbuff + readbuff_pos, 1);
                        char currentUSintValue = value[0];
                        if (currentUSintValue != standardUSintValue && (currentUSintValue < minUSintValue || currentUSintValue > maxUSintValue))
                            return false;
                        else
                            readbuff_pos += 9;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型
            {
                continue;
            }
            else
            {
                char standardUSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char maxUSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                char minUSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];
                // 1个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readbuff_pos, 1);
                char currentUSintValue = value[0];

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                {
                    if (currentUSintValue != standardUSintValue && (currentUSintValue < minUSintValue || currentUSintValue > maxUSintValue))
                        return false;
                    else
                        readbuff_pos += 9;
                }
                else //不带时间戳
                {
                    if (currentUSintValue != standardUSintValue && (currentUSintValue < minUSintValue || currentUSintValue > maxUSintValue))
                        return false;
                    else
                        readbuff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //既是时间序列又是数组
                {
                    continue;
                }
                else //只是时间序列类型
                {
                    ushort standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    ushort maxUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                    ushort minUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.minValue);

                    for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                    {
                        // 2个字节,暂定，根据后续情况可能进行更改
                        char value[2] = {0};
                        memcpy(value, readbuff + readbuff_pos, 2);
                        ushort currentUintValue = converter.ToUInt16(value);
                        if (currentUintValue != standardUintValue && (currentUintValue < minUintValue || currentUintValue > maxUintValue))
                            return false;
                        else
                            readbuff_pos += 10;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型
            {
                continue;
            }
            else
            {
                ushort standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                ushort maxUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                ushort minUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, readbuff + readbuff_pos, 2);
                ushort currentUintValue = converter.ToUInt16(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                {
                    if (currentUintValue != standardUintValue && (currentUintValue < minUintValue || currentUintValue > maxUintValue))
                        return false;
                    else
                        readbuff_pos += 10;
                }
                else //不带时间戳
                {
                    if (currentUintValue != standardUintValue && (currentUintValue < minUintValue || currentUintValue > maxUintValue))
                        return false;
                    else
                        readbuff_pos += 2;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //既是时间序列又是数组
                {
                    continue;
                }
                else //只是时间序列类型
                {
                    uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    uint32 maxUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                    uint32 minUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

                    for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                    {
                        // 4个字节,暂定，根据后续情况可能进行更改
                        char value[4] = {0};
                        memcpy(value, readbuff + readbuff_pos, 4);
                        uint32 currentUDintValue = converter.ToUInt32(value);

                        if (currentUDintValue != standardUDintValue && (currentUDintValue < minUDintValue || currentUDintValue > maxUDintValue))
                            return false;
                        else
                            readbuff_pos += 12;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型
            {
                continue;
            }
            else
            {
                uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                uint32 maxUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                uint32 minUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                uint32 currentUDintValue = converter.ToUInt32(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime) //带时间戳
                {
                    if (currentUDintValue != standardUDintValue && (currentUDintValue < minUDintValue || currentUDintValue > maxUDintValue))
                        return false;
                    else
                        readbuff_pos += 12;
                }
                else //不带时间戳
                {
                    if (currentUDintValue != standardUDintValue && (currentUDintValue < minUDintValue || currentUDintValue > maxUDintValue))
                        return false;
                    else
                        readbuff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //既是时间序列又是数组
                {
                    continue;
                }
                else //只是时间序列类型
                {
                    char standardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                    char maxSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                    char minSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];

                    for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                    {
                        // 2个字节,暂定，根据后续情况可能进行更改
                        char value[1] = {0};
                        memcpy(value, readbuff + readbuff_pos, 1);
                        char currentSintValue = value[0];
                        if (currentSintValue != standardSintValue && (currentSintValue < minSintValue || currentSintValue > maxSintValue))
                            return false;
                        else
                            readbuff_pos += 9;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型
            {
                continue;
            }
            else
            {
                char standardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char maxSintValue = CurrentZipTemplate.schemas[i].second.maxValue[0];
                char minSintValue = CurrentZipTemplate.schemas[i].second.minValue[0];
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[1] = {0};
                memcpy(value, readbuff + readbuff_pos, 1);
                char currentSintValue = value[0];

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带时间戳
                {
                    if (currentSintValue != standardSintValue && (currentSintValue < minSintValue || currentSintValue > maxSintValue))
                        return false;
                    else
                        readbuff_pos += 9;
                }
                else //不带时间戳
                {
                    if (currentSintValue != standardSintValue && (currentSintValue < minSintValue || currentSintValue > maxSintValue))
                        return false;
                    else
                        readbuff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //既是时间序列又是数组
                {
                    continue;
                }
                else //只是时间序列类型
                {
                    short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    short maxIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                    short minIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.minValue);

                    for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                    {
                        // 2个字节,暂定，根据后续情况可能进行更改
                        char value[2] = {0};
                        memcpy(value, readbuff + readbuff_pos, 2);
                        short currentIntValue = converter.ToInt16(value);
                        if (currentIntValue != standardIntValue && (currentIntValue < minIntValue || currentIntValue > maxIntValue))
                            return false;
                        else
                            readbuff_pos += 10;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型
            {
                continue;
            }
            else
            {
                short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                short maxIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
                short minIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 2个字节,暂定，根据后续情况可能进行更改
                char value[2] = {0};
                memcpy(value, readbuff + readbuff_pos, 2);
                short currentIntValue = converter.ToInt16(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    if (currentIntValue != standardIntValue && (currentIntValue < minIntValue || currentIntValue > maxIntValue))
                        return false;
                    else
                        readbuff_pos += 10;
                }
                else //不带时间戳
                {
                    if (currentIntValue != standardIntValue && (currentIntValue < minIntValue || currentIntValue > maxIntValue))
                        return false;
                    else
                        readbuff_pos += 2;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //既是时间序列又是数组
                {
                    continue;
                }
                else //只是时间序列类型
                {
                    int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    int maxDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                    int minDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

                    for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                    {
                        // 4个字节,暂定，根据后续情况可能进行更改
                        char value[4] = {0};
                        memcpy(value, readbuff + readbuff_pos, 4);
                        int currentDintValue = converter.ToInt32(value);
                        if (currentDintValue != standardDintValue && (currentDintValue < minDintValue || currentDintValue > maxDintValue))
                            return false;
                        else
                            readbuff_pos += 12;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型
            {
                continue;
            }
            else
            {
                int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                int maxDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                int minDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                int currentDintValue = converter.ToInt32(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    if (currentDintValue != standardDintValue && (currentDintValue < minDintValue || currentDintValue > maxDintValue))
                        return false;
                    else
                        readbuff_pos += 12;
                }
                else //不带时间戳
                {
                    if (currentDintValue != standardDintValue && (currentDintValue < minDintValue || currentDintValue > maxDintValue))
                        return false;
                    else
                        readbuff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL变量
        {
            if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
            {
                if (CurrentZipTemplate.schemas[i].second.isArray == true) //既是时间序列又是数组
                {
                    continue;
                }
                else //只是时间序列类型
                {
                    float standardFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    float maxFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.maxValue);
                    float minFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.minValue);

                    for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                    {
                        // 4个字节,暂定，根据后续情况可能进行更改
                        char value[4] = {0};
                        memcpy(value, readbuff + readbuff_pos, 4);
                        float currentRealValue = converter.ToFloat(value);
                        if (currentRealValue != standardFloatValue && (currentRealValue < minFloatValue || currentRealValue > maxFloatValue))
                            return false;
                        else
                            readbuff_pos += 12;
                    }
                }
            }
            else if (CurrentZipTemplate.schemas[i].second.isArray == true) //是数组类型
            {
                continue;
            }
            else
            {
                float standardFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                float maxFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.maxValue);
                float minFloatValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.minValue);
                // 4个字节,暂定，根据后续情况可能进行更改
                char value[4] = {0};
                memcpy(value, readbuff + readbuff_pos, 4);
                int currentRealValue = converter.ToFloat(value);

                if (CurrentZipTemplate.schemas[i].second.hasTime == true) //带有时间戳
                {
                    if (currentRealValue != standardFloatValue && (currentRealValue < minFloatValue || currentRealValue > maxFloatValue))
                        return false;
                    else
                        readbuff_pos += 12;
                }
                else //不带时间戳
                {
                    if (currentRealValue != standardFloatValue && (currentRealValue < minFloatValue || currentRealValue > maxFloatValue))
                        return false;
                    else
                        readbuff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::IMAGE)
        {
            //暂定图片前面有2字节长度，2字节宽度和2字节通道
            char length[2] = {0};
            memcpy(length, readbuff + readbuff_pos, 2);
            uint16_t imageLength = converter.ToUInt16(length);
            char width[2] = {0};
            memcpy(width, readbuff + readbuff_pos + 2, 2);
            uint16_t imageWidth = converter.ToUInt16(width);
            char channel[2] = {0};
            memcpy(channel, readbuff + readbuff_pos + 4, 2);
            uint16_t imageChannel = converter.ToUInt16(channel);
            uint32_t imageSize = imageChannel * imageLength * imageWidth;
            readbuff_pos += imageSize + 6;
            continue;
        }
    }
    return true;
}

/**
 * @brief 获取解压后的数据
 *
 * @param buff 已压缩数据缓存的二级指针
 * @param buffLength 解压后长度
 * @param pathToLine 路径
 * @return int
 */
int ReZipBuff(char **buff, int &buffLength, const char *pathToLine)
{
    int err;
    //确认当前模版
    // if (ZipTemplateManager::CheckZipTemplate(pathToLine) != 0)
    //     return StatusCode::SCHEMA_FILE_NOT_FOUND;
    DataTypeConverter converter;

    long len = buffLength;
    char *writebuff = new char[CurrentZipTemplate.totalBytes]; //写入没有被压缩的数据
    long readbuff_pos = 0;
    long writebuff_pos = 0;

    for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++)
    {
        if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::BOOL) // BOOL类型
        {
            if (len == 0) //表示文件完全压缩
            {
                char standardBoolValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char BoolValue[1] = {0};
                BoolValue[0] = standardBoolValue;
                memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
                writebuff_pos += 1;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, *buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)3) //既是时间序列又是数组
                            {
                                //直接拷贝
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen);
                                writebuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                                readbuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)4) //只是时间序列
                            {
                                //先获得第一次采样的时间
                                char time[8];
                                memcpy(time, *buff + readbuff_pos, 8);
                                uint64_t startTime = converter.ToLong64(time);
                                readbuff_pos += 8;

                                for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                                {
                                    if (*(*buff + readbuff_pos - 1) == (char)-1) //说明没有未压缩的时间序列了
                                    {
                                        //将标准值数据拷贝到writebuff
                                        char standardBoolValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                        char BoolValue[1] = {0};
                                        BoolValue[0] = standardBoolValue;
                                        memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
                                        writebuff_pos += 1;

                                        //添加上时间戳
                                        uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                        char zipTimeBuff[8] = {0};
                                        converter.ToLong64Buff(zipTime, zipTimeBuff);
                                        memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                        writebuff_pos += 8;
                                    }
                                    else
                                    {
                                        //对比编号是否等于未压缩的时间序列编号
                                        char zipTsPosNum[2] = {0};
                                        memcpy(zipTsPosNum, *buff + readbuff_pos, 2);
                                        uint16_t tsPosCmp = converter.ToUInt16(zipTsPosNum);

                                        if (tsPosCmp == j) //是未压缩时间序列的编号
                                        {
                                            //将未压缩的数据拷贝到writebuff
                                            readbuff_pos += 2;
                                            memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4);
                                            readbuff_pos += 4;
                                            writebuff_pos += 4;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                        else //不是未压缩时间序列的编号
                                        {
                                            //将标准值数据拷贝到writebuff
                                            char standardBoolValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                            char BoolValue[1] = {0};
                                            BoolValue[0] = standardBoolValue;
                                            memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
                                            writebuff_pos += 1;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                    }
                                    if (j == CurrentZipTemplate.schemas[i].second.tsLen - 1) //时间序列还原结束，readbuff_pos+1跳过0xFF标志
                                        readbuff_pos += 1;
                                }
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 9);
                                writebuff_pos += 9;
                                readbuff_pos += 9;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)1) //只有时间
                            {
                                char standardBoolValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                char BoolValue[1] = {0};
                                BoolValue[0] = standardBoolValue;
                                memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
                                writebuff_pos += 1;

                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 1);
                                writebuff_pos += 1;
                                readbuff_pos += 1;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        char standardBoolValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                        char BoolValue[1] = {0};
                        BoolValue[0] = standardBoolValue;
                        memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
                        writebuff_pos += 1;
                    }
                }
                else //没有未压缩的数据了
                {
                    char standardBoolValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                    char BoolValue[1] = {0};
                    BoolValue[0] = standardBoolValue;
                    memcpy(writebuff + writebuff_pos, BoolValue, 1); // Bool标准值
                    writebuff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UDINT) // UDINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                char UDintValue[4] = {0};
                converter.ToUInt32Buff(standardUDintValue, UDintValue);
                memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                writebuff_pos += 4;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, *buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)3) //既是时间序列又是数组
                            {
                                //直接拷贝
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen);
                                writebuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                                readbuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)4) //只是时间序列
                            {
                                //先获得第一次采样的时间
                                char time[8];
                                memcpy(time, *buff + readbuff_pos, 8);
                                uint64_t startTime = converter.ToLong64(time);
                                readbuff_pos += 8;

                                for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                                {
                                    if (*(*buff + readbuff_pos - 1) == (char)-1) //说明没有未压缩的时间序列了
                                    {
                                        //将标准值数据拷贝到writebuff
                                        uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                        char UDintValue[4] = {0};
                                        converter.ToUInt32Buff(standardUDintValue, UDintValue);
                                        memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                                        writebuff_pos += 4;

                                        //添加上时间戳
                                        uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                        char zipTimeBuff[8] = {0};
                                        converter.ToLong64Buff(zipTime, zipTimeBuff);
                                        memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                        writebuff_pos += 8;
                                    }
                                    else
                                    {
                                        //对比编号是否等于未压缩的时间序列编号
                                        char zipTsPosNum[2] = {0};
                                        memcpy(zipTsPosNum, *buff + readbuff_pos, 2);
                                        uint16_t tsPosCmp = converter.ToUInt16(zipTsPosNum);

                                        if (tsPosCmp == j) //是未压缩时间序列的编号
                                        {
                                            //将未压缩的数据拷贝到writebuff
                                            readbuff_pos += 2;
                                            memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4);
                                            readbuff_pos += 4;
                                            writebuff_pos += 4;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                        else //不是未压缩时间序列的编号
                                        {
                                            //将标准值数据拷贝到writebuff
                                            uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                            char UDintValue[4] = {0};
                                            converter.ToUInt32Buff(standardUDintValue, UDintValue);
                                            memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                                            writebuff_pos += 4;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                    }
                                    if (j == CurrentZipTemplate.schemas[i].second.tsLen - 1) //时间序列还原结束，readbuff_pos+1跳过0xFF标志
                                        readbuff_pos += 1;
                                }
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 12);
                                writebuff_pos += 12;
                                readbuff_pos += 12;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)1) //只有时间
                            {
                                uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                char UDintValue[4] = {0};
                                converter.ToUInt32Buff(standardUDintValue, UDintValue);
                                memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                                writebuff_pos += 4;

                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char UDintValue[4] = {0};
                        converter.ToUInt32Buff(standardUDintValue, UDintValue);
                        memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                        writebuff_pos += 4;
                    }
                }
                else //没有未压缩的数据了
                {
                    uint32 standardUDintValue = converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char UDintValue[4] = {0};
                    converter.ToUInt32Buff(standardUDintValue, UDintValue);
                    memcpy(writebuff + writebuff_pos, UDintValue, 4); // UDINT标准值
                    writebuff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::USINT) // USINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char UsintValue[1] = {0};
                UsintValue[0] = StandardUsintValue;
                memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                writebuff_pos += 1;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, *buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)3) //既是时间序列又是数组
                            {
                                //直接拷贝
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen);
                                writebuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                                readbuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)4) //只是时间序列
                            {
                                //先获得第一次采样的时间
                                char time[8];
                                memcpy(time, *buff + readbuff_pos, 8);
                                uint64_t startTime = converter.ToLong64(time);
                                readbuff_pos += 8;

                                for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                                {
                                    if (*buff[readbuff_pos] == (char)-1) //说明没有未压缩的时间序列了
                                    {
                                        //将标准值数据拷贝到writebuff
                                        char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                        char UsintValue[1] = {0};
                                        UsintValue[0] = StandardUsintValue;
                                        memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                                        writebuff_pos += 1;

                                        //添加上时间戳
                                        uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                        char zipTimeBuff[8] = {0};
                                        converter.ToLong64Buff(zipTime, zipTimeBuff);
                                        memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                        writebuff_pos += 8;
                                    }
                                    else
                                    {
                                        //对比编号是否等于未压缩的时间序列编号
                                        char zipTsPosNum[2] = {0};
                                        memcpy(zipTsPosNum, *buff + readbuff_pos, 2);
                                        uint16_t tsPosCmp = converter.ToUInt16(zipTsPosNum);

                                        if (tsPosCmp == j) //是未压缩时间序列的编号
                                        {
                                            //将未压缩的数据拷贝到writebuff
                                            readbuff_pos += 2;
                                            memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4);
                                            readbuff_pos += 4;
                                            writebuff_pos += 4;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                        else //不是未压缩时间序列的编号
                                        {
                                            //将标准值数据拷贝到writebuff
                                            char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                            char UsintValue[1] = {0};
                                            UsintValue[0] = StandardUsintValue;
                                            memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                                            writebuff_pos += 1;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                    }
                                    if (j == CurrentZipTemplate.schemas[i].second.tsLen - 1) //时间序列还原结束，readbuff_pos+1跳过0xFF标志
                                        readbuff_pos += 1;
                                }
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 9);
                                writebuff_pos += 9;
                                readbuff_pos += 9;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)1) //只有时间
                            {
                                char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                char UsintValue[1] = {0};
                                UsintValue[0] = StandardUsintValue;
                                memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                                writebuff_pos += 1;

                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 1);
                                writebuff_pos += 1;
                                readbuff_pos += 1;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                        char UsintValue[1] = {0};
                        UsintValue[0] = StandardUsintValue;
                        memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                        writebuff_pos += 1;
                    }
                }
                else //没有未压缩的数据了
                {
                    char StandardUsintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                    char UsintValue[1] = {0};
                    UsintValue[0] = StandardUsintValue;
                    memcpy(writebuff + writebuff_pos, UsintValue, 1); // USINT标准值
                    writebuff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::UINT) // UINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                char UintValue[2] = {0};
                converter.ToUInt16Buff(standardUintValue, UintValue);
                memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                writebuff_pos += 2;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, *buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)3) //既是时间序列又是数组
                            {
                                //直接拷贝
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen);
                                writebuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                                readbuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)4) //只是时间序列
                            {
                                //先获得第一次采样的时间
                                char time[8];
                                memcpy(time, *buff + readbuff_pos, 8);
                                uint64_t startTime = converter.ToLong64(time);
                                readbuff_pos += 8;

                                for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                                {
                                    if (*(*buff + readbuff_pos - 1) == (char)-1) //说明没有未压缩的时间序列了
                                    {
                                        //将标准值数据拷贝到writebuff
                                        uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                        char UintValue[2] = {0};
                                        converter.ToUInt16Buff(standardUintValue, UintValue);
                                        memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                                        writebuff_pos += 2;

                                        //添加上时间戳
                                        uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                        char zipTimeBuff[8] = {0};
                                        converter.ToLong64Buff(zipTime, zipTimeBuff);
                                        memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                        writebuff_pos += 8;
                                    }
                                    else
                                    {
                                        //对比编号是否等于未压缩的时间序列编号
                                        char zipTsPosNum[2] = {0};
                                        memcpy(zipTsPosNum, *buff + readbuff_pos, 2);
                                        uint16_t tsPosCmp = converter.ToUInt16(zipTsPosNum);

                                        if (tsPosCmp == j) //是未压缩时间序列的编号
                                        {
                                            //将未压缩的数据拷贝到writebuff
                                            readbuff_pos += 2;
                                            memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4);
                                            readbuff_pos += 4;
                                            writebuff_pos += 4;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                        else //不是未压缩时间序列的编号
                                        {
                                            //将标准值数据拷贝到writebuff
                                            uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                            char UintValue[2] = {0};
                                            converter.ToUInt16Buff(standardUintValue, UintValue);
                                            memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                                            writebuff_pos += 2;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                    }
                                    if (j == CurrentZipTemplate.schemas[i].second.tsLen - 1) //时间序列还原结束，readbuff_pos+1跳过0xFF标志
                                        readbuff_pos += 1;
                                }
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 10);
                                writebuff_pos += 10;
                                readbuff_pos += 10;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)1) //只有时间
                            {
                                uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                char UintValue[2] = {0};
                                converter.ToUInt16Buff(standardUintValue, UintValue);
                                memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                                writebuff_pos += 2;

                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 2);
                                writebuff_pos += 2;
                                readbuff_pos += 2;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char UintValue[2] = {0};
                        converter.ToUInt16Buff(standardUintValue, UintValue);
                        memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                        writebuff_pos += 2;
                    }
                }
                else //没有未压缩的数据了
                {
                    uint16_t standardUintValue = converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char UintValue[2] = {0};
                    converter.ToUInt16Buff(standardUintValue, UintValue);
                    memcpy(writebuff + writebuff_pos, UintValue, 2); // UINT标准值
                    writebuff_pos += 2;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::SINT) // SINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                char SintValue[1] = {0};
                SintValue[0] = StandardSintValue;
                memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                writebuff_pos += 1;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, *buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)3) //既是时间序列又是数组
                            {
                                //直接拷贝
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen);
                                writebuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                                readbuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)4) //只是时间序列
                            {
                                //先获得第一次采样的时间
                                char time[8];
                                memcpy(time, *buff + readbuff_pos, 8);
                                uint64_t startTime = converter.ToLong64(time);
                                readbuff_pos += 8;

                                for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                                {
                                    if (*(*buff + readbuff_pos - 1) == (char)-1) //说明没有未压缩的时间序列了
                                    {
                                        //将标准值数据拷贝到writebuff
                                        char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                        char SintValue[1] = {0};
                                        SintValue[0] = StandardSintValue;
                                        memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                                        writebuff_pos += 1;

                                        //添加上时间戳
                                        uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                        char zipTimeBuff[8] = {0};
                                        converter.ToLong64Buff(zipTime, zipTimeBuff);
                                        memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                        writebuff_pos += 8;
                                    }
                                    else
                                    {
                                        //对比编号是否等于未压缩的时间序列编号
                                        char zipTsPosNum[2] = {0};
                                        memcpy(zipTsPosNum, *buff + readbuff_pos, 2);
                                        uint16_t tsPosCmp = converter.ToUInt16(zipTsPosNum);

                                        if (tsPosCmp == j) //是未压缩时间序列的编号
                                        {
                                            //将未压缩的数据拷贝到writebuff
                                            readbuff_pos += 2;
                                            memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4);
                                            readbuff_pos += 4;
                                            writebuff_pos += 4;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                        else //不是未压缩时间序列的编号
                                        {
                                            //将标准值数据拷贝到writebuff
                                            char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                            char SintValue[1] = {0};
                                            SintValue[0] = StandardSintValue;
                                            memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                                            writebuff_pos += 1;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                    }
                                    if (j == CurrentZipTemplate.schemas[i].second.tsLen - 1) //时间序列还原结束，readbuff_pos+1跳过0xFF标志
                                        readbuff_pos += 1;
                                }
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 9);
                                writebuff_pos += 9;
                                readbuff_pos += 9;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)1) //只有时间
                            {
                                char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                                char SintValue[1] = {0};
                                SintValue[0] = StandardSintValue;
                                memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                                writebuff_pos += 1;

                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 1);
                                writebuff_pos += 1;
                                readbuff_pos += 1;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                        char SintValue[1] = {0};
                        SintValue[0] = StandardSintValue;
                        memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                        writebuff_pos += 1;
                    }
                }
                else //没有未压缩的数据了
                {
                    char StandardSintValue = CurrentZipTemplate.schemas[i].second.standardValue[0];
                    char SintValue[1] = {0};
                    SintValue[0] = StandardSintValue;
                    memcpy(writebuff + writebuff_pos, SintValue, 1); // SINT标准值
                    writebuff_pos += 1;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::INT) // INT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                char IntValue[2] = {0};
                converter.ToInt16Buff(standardIntValue, IntValue);
                memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                writebuff_pos += 2;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, *buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)3) //既是时间序列又是数组
                            {
                                //直接拷贝
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen);
                                writebuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                                readbuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)4) //只是时间序列
                            {
                                //先获得第一次采样的时间
                                char time[8];
                                memcpy(time, *buff + readbuff_pos, 8);
                                uint64_t startTime = converter.ToLong64(time);
                                readbuff_pos += 8;

                                for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                                {
                                    if (*(*buff + readbuff_pos - 1) == (char)-1) //说明没有未压缩的时间序列了
                                    {
                                        //将标准值数据拷贝到writebuff
                                        short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                        char IntValue[2] = {0};
                                        converter.ToInt16Buff(standardIntValue, IntValue);
                                        memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                                        writebuff_pos += 2;

                                        //添加上时间戳
                                        uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                        char zipTimeBuff[8] = {0};
                                        converter.ToLong64Buff(zipTime, zipTimeBuff);
                                        memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                        writebuff_pos += 8;
                                    }
                                    else
                                    {
                                        //对比编号是否等于未压缩的时间序列编号
                                        char zipTsPosNum[2] = {0};
                                        memcpy(zipTsPosNum, *buff + readbuff_pos, 2);
                                        uint16_t tsPosCmp = converter.ToUInt16(zipTsPosNum);

                                        if (tsPosCmp == j) //是未压缩时间序列的编号
                                        {
                                            //将未压缩的数据拷贝到writebuff
                                            readbuff_pos += 2;
                                            memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4);
                                            readbuff_pos += 4;
                                            writebuff_pos += 4;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                        else //不是未压缩时间序列的编号
                                        {
                                            //将标准值数据拷贝到writebuff
                                            short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                            char IntValue[2] = {0};
                                            converter.ToInt16Buff(standardIntValue, IntValue);
                                            memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                                            writebuff_pos += 2;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                    }
                                    if (j == CurrentZipTemplate.schemas[i].second.tsLen - 1) //时间序列还原结束，readbuff_pos+1跳过0xFF标志
                                        readbuff_pos += 1;
                                }
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 2 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 2 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 10);
                                writebuff_pos += 10;
                                readbuff_pos += 10;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)1) //只有时间
                            {
                                short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                char IntValue[2] = {0};
                                converter.ToInt16Buff(standardIntValue, IntValue);
                                memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                                writebuff_pos += 2;

                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 2);
                                writebuff_pos += 2;
                                readbuff_pos += 2;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char IntValue[2] = {0};
                        converter.ToInt16Buff(standardIntValue, IntValue);
                        memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                        writebuff_pos += 2;
                    }
                }
                else //没有未压缩的数据了
                {
                    short standardIntValue = converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char IntValue[2] = {0};
                    converter.ToInt16Buff(standardIntValue, IntValue);
                    memcpy(writebuff + writebuff_pos, IntValue, 2); // INT标准值
                    writebuff_pos += 2;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::DINT) // DINT类型
        {
            if (len == 0) //表示文件完全压缩
            {
                int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                char DintValue[4] = {0};
                converter.ToInt32Buff(standardDintValue, DintValue);
                memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                writebuff_pos += 4;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, *buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)3) //既是时间序列又是数组
                            {
                                //直接拷贝
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen);
                                writebuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                                readbuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)4) //只是时间序列
                            {
                                //先获得第一次采样的时间
                                char time[8];
                                memcpy(time, *buff + readbuff_pos, 8);
                                uint64_t startTime = converter.ToLong64(time);
                                readbuff_pos += 8;

                                for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                                {
                                    if (*(*buff + readbuff_pos - 1) == (char)-1) //说明没有未压缩的时间序列了
                                    {
                                        //将标准值数据拷贝到writebuff
                                        int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                        char DintValue[4] = {0};
                                        converter.ToInt32Buff(standardDintValue, DintValue);
                                        memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                                        writebuff_pos += 4;

                                        //添加上时间戳
                                        uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                        char zipTimeBuff[8] = {0};
                                        converter.ToLong64Buff(zipTime, zipTimeBuff);
                                        memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                        writebuff_pos += 8;
                                    }
                                    else
                                    {
                                        //对比编号是否等于未压缩的时间序列编号
                                        char zipTsPosNum[2] = {0};
                                        memcpy(zipTsPosNum, buff + readbuff_pos, 2);
                                        uint16_t tsPosCmp = converter.ToUInt16(zipTsPosNum);

                                        if (tsPosCmp == j) //是未压缩时间序列的编号
                                        {
                                            //将未压缩的数据拷贝到writebuff
                                            readbuff_pos += 2;
                                            memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4);
                                            readbuff_pos += 4;
                                            writebuff_pos += 4;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                        else //不是未压缩时间序列的编号
                                        {
                                            //将标准值数据拷贝到writebuff
                                            int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                            char DintValue[4] = {0};
                                            converter.ToInt32Buff(standardDintValue, DintValue);
                                            memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                                            writebuff_pos += 4;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                    }
                                    if (j == CurrentZipTemplate.schemas[i].second.tsLen - 1) //时间序列还原结束，readbuff_pos+1跳过0xFF标志
                                        readbuff_pos += 1;
                                }
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 12);
                                writebuff_pos += 12;
                                readbuff_pos += 12;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)1) //只有时间
                            {
                                int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                char DintValue[4] = {0};
                                converter.ToInt32Buff(standardDintValue, DintValue);
                                memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                                writebuff_pos += 4;

                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char DintValue[4] = {0};
                        converter.ToInt32Buff(standardDintValue, DintValue);
                        memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                        writebuff_pos += 4;
                    }
                }
                else //没有未压缩的数据了
                {
                    int standardDintValue = converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char DintValue[4] = {0};
                    converter.ToInt32Buff(standardDintValue, DintValue);
                    memcpy(writebuff + writebuff_pos, DintValue, 4); // DINT标准值
                    writebuff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::REAL) // REAL类型
        {
            if (len == 0) //表示文件完全压缩
            {
                float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                char RealValue[4] = {0};
                converter.ToFloatBuff(standardRealValue, RealValue);
                memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                writebuff_pos += 4;
            }
            else //文件未完全压缩
            {
                if (readbuff_pos < len) //还有未压缩的数据
                {
                    //对比编号是否等于当前模板所在条数
                    char zipPosNum[2] = {0};
                    memcpy(zipPosNum, *buff + readbuff_pos, 2);
                    uint16_t posCmp = converter.ToUInt16(zipPosNum);
                    if (posCmp == i) //是未压缩数据的编号
                    {
                        readbuff_pos += 3;
                        if (CurrentZipTemplate.schemas[i].second.isTimeseries == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)3) //既是时间序列又是数组
                            {
                                //直接拷贝
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen);
                                writebuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                                readbuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * 4 + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)4) //只是时间序列
                            {
                                //先获得第一次采样的时间
                                char time[8];
                                memcpy(time, *buff + readbuff_pos, 8);
                                uint64_t startTime = converter.ToLong64(time);
                                readbuff_pos += 8;

                                for (auto j = 0; j < CurrentZipTemplate.schemas[i].second.tsLen; j++)
                                {
                                    if (*(*buff + readbuff_pos - 1) == (char)-1) //说明没有未压缩的时间序列了
                                    {
                                        //将标准值数据拷贝到writebuff
                                        float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                        char RealValue[4] = {0};
                                        converter.ToFloatBuff(standardRealValue, RealValue);
                                        memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                                        writebuff_pos += 4;

                                        //添加上时间戳
                                        uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                        char zipTimeBuff[8] = {0};
                                        converter.ToLong64Buff(zipTime, zipTimeBuff);
                                        memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                        writebuff_pos += 8;
                                    }
                                    else
                                    {
                                        //对比编号是否等于未压缩的时间序列编号
                                        char zipTsPosNum[2] = {0};
                                        memcpy(zipTsPosNum, *buff + readbuff_pos, 2);
                                        uint16_t tsPosCmp = converter.ToUInt16(zipTsPosNum);

                                        if (tsPosCmp == j) //是未压缩时间序列的编号
                                        {
                                            //将未压缩的数据拷贝到writebuff
                                            readbuff_pos += 2;
                                            memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4);
                                            readbuff_pos += 4;
                                            writebuff_pos += 4;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                        else //不是未压缩时间序列的编号
                                        {
                                            //将标准值数据拷贝到writebuff
                                            float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                            char RealValue[4] = {0};
                                            converter.ToFloatBuff(standardRealValue, RealValue);
                                            memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                                            writebuff_pos += 4;

                                            //添加上时间戳
                                            uint64_t zipTime = startTime + CurrentZipTemplate.schemas[i].second.timeseriesSpan * j;
                                            char zipTimeBuff[8] = {0};
                                            converter.ToLong64Buff(zipTime, zipTimeBuff);
                                            memcpy(writebuff + writebuff_pos, zipTimeBuff, 8);
                                            writebuff_pos += 8;
                                        }
                                    }
                                    if (j == CurrentZipTemplate.schemas[i].second.tsLen - 1) //时间序列还原结束，readbuff_pos+1跳过0xFF标志
                                        readbuff_pos += 1;
                                }
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else if (CurrentZipTemplate.schemas[i].second.isArray == true)
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4 * CurrentZipTemplate.schemas[i].second.arrayLen);
                                writebuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                                readbuff_pos += 4 * CurrentZipTemplate.schemas[i].second.arrayLen;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                        else
                        {
                            if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 12);
                                writebuff_pos += 12;
                                readbuff_pos += 12;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)1) //只有时间
                            {
                                float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                                char RealValue[4] = {0};
                                converter.ToFloatBuff(standardRealValue, RealValue);
                                memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                                writebuff_pos += 4;

                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 8);
                                writebuff_pos += 8;
                                readbuff_pos += 8;
                            }
                            else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                            {
                                memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 4);
                                writebuff_pos += 4;
                                readbuff_pos += 4;
                            }
                            else
                            {
                                cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                                return StatusCode::ZIPTYPE_ERROR;
                            }
                        }
                    }
                    else //不是未压缩的编号
                    {
                        float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char RealValue[4] = {0};
                        converter.ToFloatBuff(standardRealValue, RealValue);
                        memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                        writebuff_pos += 4;
                    }
                }
                else //没有未压缩的数据了
                {
                    float standardRealValue = converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char RealValue[4] = {0};
                    converter.ToFloatBuff(standardRealValue, RealValue);
                    memcpy(writebuff + writebuff_pos, RealValue, 4); // REAL标准值
                    writebuff_pos += 4;
                }
            }
        }
        else if (CurrentZipTemplate.schemas[i].second.valueType == ValueType::IMAGE) // IMAGE类型
        {
            continue;
            //对比编号是否等于当前模板所在条数
            char zipPosNum[2] = {0};
            memcpy(zipPosNum, *buff + readbuff_pos, 2);
            uint16_t posCmp = converter.ToUInt16(zipPosNum);
            if (posCmp == i) //是未压缩数据的编号
            {
                readbuff_pos += 2;
                //暂定图片之前有2字节的长度，2字节的宽度和2字节的通道
                char length[2] = {0};
                memcpy(length, *buff + readbuff_pos + 1, 2);
                uint16_t imageLength = converter.ToUInt16(length);
                char width[2] = {0};
                memcpy(width, *buff + readbuff_pos + 3, 2);
                uint16_t imageWidth = converter.ToUInt16(width);
                char channel[2] = {0};
                memcpy(channel, *buff + readbuff_pos + 5, 2);
                //算出图片大小
                uint16_t imageChannel = converter.ToUInt16(channel);
                uint32_t imageSize = imageChannel * imageLength * imageWidth;
                readbuff_pos += 1;

                //重新分配内存
                char *writebuff_new = new char[CurrentZipTemplate.totalBytes + imageSize + 6];
                memcpy(writebuff_new, writebuff, writebuff_pos);
                delete[] writebuff;
                writebuff = writebuff_new;

                if (*(*buff + readbuff_pos - 1) == (char)2) //既有时间又有数据
                {
                    memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 6); //存储图片的长度、宽度、通道
                    readbuff_pos += 6;
                    writebuff_pos += 6;
                    //存储图片
                    memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, imageLength + 8);
                    writebuff_pos += imageLength + 8;
                    readbuff_pos += imageLength + 8;
                }
                else if (*(*buff + readbuff_pos - 1) == (char)0) //只有数据
                {
                    memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, 6); //存储图片的长度、宽度、通道
                    readbuff_pos += 6;
                    writebuff_pos += 6;
                    //存储图片
                    memcpy(writebuff + writebuff_pos, *buff + readbuff_pos, imageLength);
                    writebuff_pos += imageLength;
                    readbuff_pos += imageLength;
                }
                else
                {
                    cout << "压缩类型出错！请检查压缩功能是否有误" << endl;
                    return StatusCode::ZIPTYPE_ERROR;
                }
            }
            else //不是未压缩的编号
            {
                cout << "图片还原出现问题！" << endl;
                return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            }
        }
        else
        {
            cout << "存在未知数据类型，请检查模板" << endl;
            return StatusCode::UNKNOWN_TYPE;
        }
    }
    // delete[] * buff;
    // char *buff_new = new char[writebuff_pos];
    // *buff = buff_new;
    memcpy(*buff, writebuff, writebuff_pos);
    delete[] writebuff;
    buffLength = (int)writebuff_pos;
    return 0;
}

/**
 * @brief 获得压缩后文件图片所在的偏移量（默认图片在模板最后）
 *
 * @param buff 数据缓冲
 * @return long 图片所在偏移量
 */
long GetZipImgPos(char *buff)
{
    DataTypeConverter converter;
    long readbuff_pos = 0;

    for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++)
    {
        //对比编号是否等于当前模板所在条数
        char zipPosNum[2] = {0};
        memcpy(zipPosNum, buff + readbuff_pos, 2);
        uint16_t posCmp = converter.ToUInt16(zipPosNum);
        if (posCmp == i && CurrentZipTemplate.schemas[i].second.valueType != ValueType::IMAGE) //是未压缩数据的编号
        {
            readbuff_pos += 3;
            if (buff[readbuff_pos - 1] == (char)0) //只有数据
            {
                if (CurrentZipTemplate.schemas[i].second.isArray)
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen;
                else
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes;
            }
            else if (buff[readbuff_pos - 1] == (char)2) //既有数据又有时间
            {
                if (CurrentZipTemplate.schemas[i].second.isArray)
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes * CurrentZipTemplate.schemas[i].second.arrayLen + 8;
                else
                    readbuff_pos += CurrentZipTemplate.schemas[i].second.valueBytes + 8;
            }
            else if (buff[readbuff_pos - 1] == (char)1) //只有时间
            {
                readbuff_pos += 8;
            }
            else if (buff[readbuff_pos - 1] == (char)3) //既是数组又是时间序列
            {
                readbuff_pos += (CurrentZipTemplate.schemas[i].second.arrayLen * CurrentZipTemplate.schemas[i].second.valueBytes + 8) * CurrentZipTemplate.schemas[i].second.tsLen;
            }
            else if (buff[readbuff_pos - 1] == (char)4) //只是时间序列
            {
                readbuff_pos += 8; //时间戳
                while (buff[readbuff_pos] != (char)-1)
                {
                    readbuff_pos += 2 + CurrentZipTemplate.schemas[i].second.valueBytes;
                }
                readbuff_pos += 1;
            }
        }
        else if (posCmp == i && CurrentZipTemplate.schemas[i].second.valueType == ValueType::IMAGE)
            return readbuff_pos;
        else
            continue;
    }
    return readbuff_pos;
}

// int main()
// {
//     // FileIDManager::GetFileID("/");
//     // FileIDManager::GetFileID("/");
//     // FileIDManager::GetFileID("/Jinfei3");
//     // FileIDManager::GetFileID("/Jinfei3");
//     // FileIDManager::GetFileID("/Jinfei4/line1");
//     // FileIDManager::GetFileID("/Jinfei4/line1/");
//     // cout << settings("Pack_Mode") << " " << settings("Pack_Num") << " " << settings("Pack_Interval") << endl;
//     // char *buff=NULL;
//     // int length=0;
//     // DB_ZipSwitchFile("/","/");

//     // ReZipBuff(buff, length, "/");
//     // cout<<length<<endl;
//     // return 0;
//     int err = checkInputValue("REAL","-9.000001");
//     cout<<err<<endl;
//     return 0;
//     vector<pair<string, long>> selectIDBFiles;
//     vector<pair<string, long>> selectIDBFIles2;
//     string SID = "JinfeiEleven4534";
//     string EID = "JinfeiEleven4564";
//     readIDBFilesListBySIDandNum("JinfeiEleven", SID, 12, selectIDBFiles);
//     cout << selectIDBFiles.size() << endl;
//     readIDBFilesListBySIDandEID("JinfeiEleven", SID, EID, selectIDBFIles2);
//     cout << selectIDBFIles2.size() << endl;
//     vector<pair<string, long>> selectIDBZIPFiles;
//     vector<pair<string, long>> selectIDBZIPFiles2;
//     SID = "JinfeiSeven1526000";
//     EID = "JinfeiSeven1540000";
//     readIDBZIPFilesListBySIDandNum("JinfeiSeven", SID, 1000, selectIDBZIPFiles);
//     cout << selectIDBZIPFiles.size() << endl;
//     readIDBZIPFilesListBySIDandEID("JinfeiSeven", SID, EID, selectIDBZIPFiles2);
//     cout << selectIDBZIPFiles2.size() << endl;
//     return 0;
//     vector<string> vec = DataType::splitWithStl("jinfei/", "/");
//     // curNum = getDirCurrentFileIDIndex();
//     return 0;
//     char *buff = (char *)malloc(24);
//     buff[0] = 0;
//     buff[1] = 0;
//     buff[2] = 0;
//     buff[3] = 0;
//     buff[4] = 0;
//     buff[5] = 103;
//     int len = 6;
//     ReZipBuff(buff, len, "/");
//     Packer packer;
//     vector<pair<string, long>> files;
//     readDataFilesWithTimestamps("", files);
//     packer.Pack("", files);
//     return 0;
// }
// int main()
// {
//     vector<pair<string, long>> selected;
//     // readIDBFilesListBySIDandNum("RbTsImageEle", "RbTsImageEle11", 10, selected);
//     readIDBFilesListBySIDandNum("JinfeiSeven", "JinfeiSeven1810003", 800, selected);
//     cout << selected.size() << endl;
//     return 0;
// }
