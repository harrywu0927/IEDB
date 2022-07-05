#include <iostream>
#include "include/CassFactoryDB.h"
#include "include/utils.hpp"
// #include <numpy/arrayobject.h>
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
#include <experimental/filesystem>
using namespace std;
#ifdef __linux__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
mutex imute, countmute;
// namespace fs = std::filesystem;
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

int main()
{
    fs::path mypath = "./testIEDB/testdir/test2/test3";
    fs::create_directories(mypath);
    // cout << mypath.filename() / "sdf.fe" << endl;
    return 0;
    // int (*M)[10] = new int[10][10];
    // char (*maxValue)[10];
    // maxValue = new char[10][10];
    // // char** maxValue;
    // // maxValue = new char*[1];
    // // for(int i=0;i<1;i++)
    // //     maxValue[i] = new char[10];
    // char max[10] = {'0','1','2','3','4','5','6','7','8','9'};
    // memcpy(maxValue[0],max,10);
    // for(int i=0;i<10;i++)
    //     cout<<maxValue[0][i]<<endl;
    // delete[] maxValue;
    // return 0;

    // DB_DataBuffer dbf;
    // dbf.savePath = "JinfeiSeven/JinfeiSeven1527050_2022-5-12-18-10-35-620.idb";
    // DB_ReadFile(&dbf);
    // dbf.savePath = "TEST";
    // auto start = std::chrono::system_clock::now();
    // for (int i = 0; i < 100; i++)
    // {

    // }
    // auto end = std::chrono::system_clock::now();
    // std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << std::endl;
    // // sleep(100);
    // return 0;
    // Py_Initialize();
    // // 指定py文件目录
    // PyRun_SimpleString("import sys");
    // PyRun_SimpleString("if './' not in sys.path: sys.path.append('./')");
    // PyObject *mymodule = PyImport_ImportModule("Novelty_Outlier");
    // PyObject *mymodule2 = PyImport_ImportModule("Statistics");
    // for (int a = 0; a < 5; a++)
    // {
    //     PyObject *arr = PyList_New(100);
    //     PyObject *lstitem;
    //     for (int i = 0; i < 100; i++)
    //     {
    //         lstitem = PyLong_FromLong(i % 20 == 0 ? rand() % 100 : rand() % 10);
    //         PyList_SetItem(arr, i, lstitem);
    //         // PyList_Append(test, lstitem);
    //     }

    //     PyObject *obj;

    //     // PyObject *pname = Py_BuildValue("s", "testpy");

    //     // PyObject *numpy = PyImport_ImportModule("numpy");
    //     // PyObject *std = PyObject_GetAttrString(numpy, "fft");
    //     PyObject *pValue, *pArgs, *pFunc;
    //     long res = 0;
    //     if (mymodule != NULL)
    //     {
    //         // 从模块中获取函数
    //         pFunc = PyObject_GetAttrString(mymodule, "Outliers");

    //         if (pFunc && PyCallable_Check(pFunc))
    //         {
    //             // 创建参数元组
    //             pArgs = PyTuple_New(2);
    //             PyTuple_SetItem(pArgs, 0, arr);
    //             PyTuple_SetItem(pArgs, 1, PyLong_FromLong(1));
    //             // 函数执行
    //             PyObject *ret = PyObject_CallObject(pFunc, pArgs);
    //             PyObject *item;
    //             long val;
    //             int len = PyObject_Size(ret);
    //             for (int i = 0; i < len; i++)
    //             {
    //                 item = PyList_GetItem(ret, i); //根据下标取出python列表中的元素
    //                 val = PyLong_AsLong(item);     //转换为c类型的数据
    //                 cout << val << " ";
    //             }
    //             // res = PyLong_AsLong(PyList_GetItem(pValue, 1));
    //             // cout << pValue->ob_type->tp_name << endl;
    //         }
    //     }
    // }
    // Py_Finalize();
    // return 0;
    // long timestart = 1763728603642;
    // DataTypeConverter dt;
    // char time[8] = {0};
    // dt.ToLong64Buff(timestart, time);
    // float f = 31.5;
    // char ff[4] = {0};
    // dt.ToFloatBuff(f, ff);
    // float fff = dt.ToFloat(ff);
    // char ffff[4] = {0};
    // dt.ToFloatBuff_m(f, ffff);
    // float fffff = dt.ToFloat_m(ffff);
    // return 0;

    DB_QueryParams params;
    params.pathToLine = "JinfeiSeven";
    params.fileID = "RobotDataTwenty50";
    params.fileIDend = NULL;
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
    params.valueName = "S1ON";
    // params.valueName = NULL;
    // params.start = 0;
    // params.end = 1652099030250;
    params.start = 1553728593562;
    params.end = 1651897393777;
    params.order = ODR_NONE;
    params.compareType = CMP_NONE;
    params.compareValue = "666";
    params.queryType = TIMESPAN;
    params.byPath = 0;
    params.queryNums = 10000;
    DB_DataBuffer buffer;
    buffer.savePath = "JinfeiTTE";
    long count;
    // char x[3] = {'1', '2', '3'};
    // buffer.buffer = x;
    // buffer.length = 3;
    // DB_InsertRecord(&buffer, 0);
    // sleep(100);
    // return 0;
    auto startTime = std::chrono::system_clock::now();
    // DB_MAX(&buffer, &params);
    for (int i = 0; i < 1000; i++)
    {
        DB_QueryByTimespan(&buffer, &params);
        // cout << buffer.length << endl;
        free(buffer.buffer);
    }

    // if (buffer.bufferMalloced)
    // {
    //     char buf[buffer.length];
    //     memcpy(buf, buffer.buffer, buffer.length);
    //     cout << buffer.length << endl;
    //     for (int i = 0; i < buffer.length; i++)
    //     {
    //         cout << (int)buf[i] << " ";
    //         if (i % 11 == 0)
    //             cout << endl;
    //     }

    //     free(buffer.buffer);
    // }
    // DB_MAX(&buffer);
    auto endTime = std::chrono::system_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
    if (buffer.bufferMalloced)
    {
        // char buf[buffer.length];
        // memcpy(buf, buffer.buffer, buffer.length);
        // cout << buffer.length << endl;
        // for (int i = 0; i < buffer.length; i++)
        // {
        //     cout << (int)buf[i] << " ";
        //     if (i % 11 == 0)
        //         cout << endl;
        // }

        // free(buffer.buffer);
    }
    // DB_QueryByFileID(&buffer, &params);
    return 0;
    // cout << settings("Pack_Mode") << endl;

    double total = 0;
    // for (int i = 0; i < 10; i++)
    // {
    //     startTime = std::chrono::system_clock::now();
    //     DB_QueryByTimespan_Old(&buffer, &params);

    //     endTime = std::chrono::system_clock::now();
    //     std::cout << "第" << i + 1 << "次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
    //     free(buffer.buffer);
    //     // cout << buffer.length << endl;
    //     buffer.length = 0;
    //     total += (endTime - startTime).count();
    // }
    // cout << "使用缓存,不使用多线程的平均查询时间:" << total / 10 << endl;
    // total = 0;
    for (int i = 0; i < 10; i++)
    {
        startTime = std::chrono::system_clock::now();
        DB_QueryByTimespan(&buffer, &params);

        endTime = std::chrono::system_clock::now();
        std::cout << "第" << i + 1 << "次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
        // cout << buffer.length << endl;
        free(buffer.buffer);
        buffer.length = 0;
        total += (endTime - startTime).count();
    }
    cout << "使用缓存和单线程的平均查询时间:" << total / 10 << endl;
    total = 0;
    return 0;
    for (int i = 0; i < 10; i++)
    {
        startTime = std::chrono::system_clock::now();
        DB_QueryByTimespan(&buffer, &params);

        endTime = std::chrono::system_clock::now();
        std::cout << "第" << i + 11 << "次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
        free(buffer.buffer);
        total += (endTime - startTime).count();
    }
    cout << "使用缓存和多线程的平均查询时间:" << total / 10 << endl;
    return 0;
    // long count = 0;
    // DB_GetNormalDataCount(&params, &count);
    // cout << count << endl;
    // DB_GetAbnormalDataCount(&params, &count);
    // cout << count << endl;
    // sleep(10);
    // DB_MAX(&buffer, &params);
    // if (buffer.bufferMalloced)
    // {
    //     char buf[buffer.length];
    //     memcpy(buf, buffer.buffer, buffer.length);
    //     cout << buffer.length << endl;
    //     for (int i = 0; i < buffer.length; i++)
    //     {
    //         cout << (int)buf[i] << " ";
    //         if (i % 11 == 0)
    //             cout << endl;
    //     }

    //     free(buffer.buffer);
    // }
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