#include <iostream>
#include "include/CassFactoryDB.h"
#include "include/utils.hpp"
#include <string>
#include <fstream>
#include <dirent.h>
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <tuple>
#include <time.h>
#include <thread>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <future>
#ifndef __linux__
#include <mach/mach.h>
#endif
#include <mutex>
#include <sys/sysctl.h>
#include <sys/types.h>
using namespace std;
// void *tick(void *ptr)
// {
//     int n = 5;
//     while (n--)
//     {
//         cout << "Hello World!" << endl;
//         sleep(1);
//     }
// }
mutex imute, countmute;
void tk(int n)
{
    while (1)
    {
        cout << "Hello World!" << endl;
        this_thread::sleep_for(chrono::seconds(1));
    }
}
void checkSettings()
{
    FILE *fp = fopen("settings.json", "r+");

    while (1)
    {
        fseek(fp, 0, SEEK_END);
        int len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char buff[len];
        fread(buff, len, 1, fp);
        string str = buff;
        string contents(str);
        neb::CJsonObject tmp(contents);

        cout << tmp("Filename_Label") << endl;

        this_thread::sleep_for(chrono::seconds(3));
    }
}
// int i = 0;
int tick(int *i, int *count)
{
    // while (*i < 100)
    //{
    imute.lock();
    if (*i < 100)
    {
        *i += 1;
        cout << *i << endl;
    }

    imute.unlock();

    //}
    countmute.lock();
    *count += 1;
    cout << "count:" << *count << endl;
    countmute.unlock();
    this_thread::sleep_for(chrono::milliseconds(rand() % 500));
    return 1;
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
void getMemUsePercentage()
{
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
    cout << totalMem << endl;
    cout << usedMem << endl;
    float value = float((usedMem * 100) / totalMem);
    cout << value << endl;
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

int get_procmeminfo(MEM_OCCUPY *lpMemory)
{
    FILE *fd;
    char buff[128] = {0};
    fd = fopen("/proc/meminfo", "r");
    if (fd < 0)
        return -1;
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
int main()
{
    // fd_set set;
    // thread th1(checkSettings);
    // th1.detach();

    // std::thread t1(tk,5);
    // t1.join();
    // pthread_t pid;
    // int ret = pthread_create(&pid, NULL, tick, NULL);
    // if (ret != 0)
    // {
    //     cout << "pthread_create error: error_code=" << ret << endl;
    // }
    // pthread_exit(NULL);
    float fl = 1.2345;
    char val[4] = {0};
    memcpy(val, &fl, 4);
    return 0;

    int maxThreads = thread::hardware_concurrency();
    int i = 0;
    future_status status[maxThreads - 1];
    future<int> f[maxThreads - 1];
    for (int j = 0; j < maxThreads - 1; j++)
    {
        status[j] = future_status::ready;
        //     f[j] = async(std::launch::async, tick, &i);
        //     status[j] = f[j].wait_for(chrono::milliseconds(0));
    }
    int count2 = 0;
    do
    {
        for (int j = 0; j < maxThreads - 1; j++)
        {
            // f[j].wait();
            if (status[j] == future_status::ready)
            {
                // try
                // {
                //     count2 += f[j].get();
                // }
                // catch (const std::exception &e)
                // {
                //     std::cerr << e.what() << '\n';
                // }

                f[j] = async(std::launch::async, tick, &i, &count2);
                status[j] = f[j].wait_for(chrono::milliseconds(0));
                // cout << "thread" << j << endl;
            }
            else
            {
                status[j] = f[j].wait_for(chrono::milliseconds(0));
            }
        }
    } while (i < 100);
    // for (int j = 0; j < maxThreads; j++)
    // {
    //     f[j].wait();
    //     count2 += f[j].get();
    // }
    cout << "count" << count2 << endl;
    return 0;

    DB_QueryParams params;
    params.pathToLine = "JinfeiThirtee";
    params.fileID = "JinfeiSixteen15";
    char code[10];
    code[0] = (char)0;
    code[1] = (char)1;
    code[2] = (char)0;
    code[3] = (char)0;
    code[4] = 0;
    code[5] = (char)0;
    code[6] = 0;
    code[7] = (char)0;
    code[8] = (char)0;
    code[9] = (char)0;
    params.pathCode = code;
    params.valueName = "S2OFF";
    // params.valueName = NULL;
    params.start = 1650095500000;
    params.end = 1650175600000;
    params.order = ODR_NONE;
    params.compareType = CMP_NONE;
    params.compareValue = "666";
    params.queryType = FILEID;
    params.byPath = 0;
    params.queryNums = 20000;
    DB_DataBuffer buffer;
    buffer.savePath = "/";
    // cout << settings("Pack_Mode") << endl;
    auto startTime = std::chrono::system_clock::now();
    DB_QueryByTimespan(&buffer, &params);

    auto endTime = std::chrono::system_clock::now();
    free(buffer.buffer);
    std::cout << "首次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;

    startTime = std::chrono::system_clock::now();
    DB_QueryLastRecords(&buffer, &params);

    endTime = std::chrono::system_clock::now();
    std::cout << "第二次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
    free(buffer.buffer);
    startTime = std::chrono::system_clock::now();
    DB_QueryLastRecords(&buffer, &params);

    endTime = std::chrono::system_clock::now();
    std::cout << "第二次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
    free(buffer.buffer);
    startTime = std::chrono::system_clock::now();
    DB_QueryByFileID(&buffer, &params);

    endTime = std::chrono::system_clock::now();
    std::cout << "第三次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
    // free(buffer.buffer);
    return 0;
    // long count = 0;
    // DB_GetNormalDataCount(&params, &count);
    // cout << count << endl;
    // DB_GetAbnormalDataCount(&params, &count);
    // cout << count << endl;
    // sleep(10);
    // DB_MAX(&buffer, &params);
    if (buffer.bufferMalloced)
    {
        char buf[buffer.length];
        memcpy(buf, buffer.buffer, buffer.length);
        cout << buffer.length << endl;
        for (int i = 0; i < buffer.length; i++)
        {
            cout << (int)buf[i] << " ";
            if (i % 11 == 0)
                cout << endl;
        }

        free(buffer.buffer);
    }
    // // DB_QueryByFileID(&buffer, &params);
    // return 0;

    // struct tm t;
    // t.tm_year = atoi("2022") - 1900;
    // t.tm_mon = atoi("4") - 1;
    // t.tm_mday = atoi("10");
    // t.tm_hour = atoi("0");
    // t.tm_min = atoi("0");
    // t.tm_sec = atoi("0");
    // t.tm_isdst = -1; //不设置夏令时
    // time_t seconds = mktime(&t);
    // int ms = atoi("0");
    // long start = seconds * 1000 + ms;
    // cout<<start<<endl;
    DB_ZipParams zipParam;
    zipParam.pathToLine = "jinfei/";
    zipParam.start = 1648742400000;
    zipParam.end = 1648828800000;
    // zipParam.start=1649520000000;
    // zipParam.end=1649692800000;
    zipParam.fileID = "99";
    // DB_ZipSwitchFileByTimeSpan(&zipParam);
    // DB_ReZipSwitchFileByTimeSpan(&zipParam);
    // DB_ZipSwitchFileByFileID(&zipParam);
    // DB_ReZipSwitchFileByFileID(&zipParam);
    // DB_ZipAnalogFileByTimeSpan(&zipParam);
    // DB_ReZipAnalogFileByTimeSpan(&zipParam);
    // DB_ZipAnalogFileByFileID(&zipParam);
    // DB_ReZipAnalogFileByFileID(&zipParam);
    // DB_ZipFileByTimeSpan(&zipParam);
    // DB_ReZipFileByTimeSpan(&zipParam);
    // DB_ZipFileByFileID(&zipParam);
    // DB_ReZipFileByFileID(&zipParam);
    // char test[zipParam.bufferLen];
    // memcpy(test,zipParam.buffer,zipParam.bufferLen);
    // cout<<zipParam.bufferLen<<endl;
    // free(zipParam.buffer);

    // long len;
    // DB_GetFileLengthByPath("/jinfei91_2022-4-1-19-28-49-807.idb",&len);
    // cout<<len<<endl;
    // char buff[len];
    // DB_OpenAndRead("/jinfei91_2022-4-1-19-28-49-807.idb",buff);
    // DB_ZipRecvAnalogFile("jinfei","jinfei",buff,&len);
    // DB_ZipRecvSwitchBuff("jinfei","jinfei",buff,&len);
    // DB_ZipRecvBuff("jinfei","jinfei",buff,&len);
    // cout<<len<<endl;

    // DB_ZipSwitchFile("jinfei","jinfei");
    // DB_ReZipSwitchFile("jinfei","jinfei");
    // DB_ZipAnalogFile("jinfei","jinfei");
    // DB_ReZipAnalogFile("jinfei","jinfei");
    // DB_ZipFile("jinfei", "jinfei");
    // DB_ReZipFile("jinfei","jinfei");

    // DB_DataBuffer buffer;
    // buffer.savePath = "jinfei";
    // buffer.length = 11;
    // char buf[11] = {'3'};
    // buffer.buffer = buf;
    // cout<<"test"<<endl;
    // // thread th(tk,1);
    // // th.join();
    // for (int i = 0; i < 20; i++)
    // {
    //     char type = 1;
    //     memcpy(buffer.buffer,&type,1);
    //     DB_InsertRecord(&buffer, 1);
    //     usleep(1000000);
    // }

    // DB_ReadFile(&buffer);

    // char value[4];
    // int num = 12000;
    // DataTypeConverter dc;
    // dc.ToUInt32Buff_m(num, value);
    // cout << num << endl;
    // char value1[2];
    // int num1 = 45536;
    // dc.ToUInt16Buff_m(num1, value1);
    // cout << num1 << endl;
    return 0;
}