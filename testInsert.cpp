#include <iostream>
#include "include/CassFactoryDB.h"
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
int main()
{
    DB_DataBuffer buffer;
    buffer.savePath = "../testJinFei/Jinfei91_2022-4-1-19-28-49-807.idb";
    cout << DB_ReadFile(&buffer) << endl;
    buffer.savePath = "jinfeitest";
    auto startTime = std::chrono::system_clock::now();

    for (size_t i = 0; i < 100000; i++)
    {
        DB_InsertRecord(&buffer, 0);
    }
    auto endTime = std::chrono::system_clock::now();
    std::cout << "100000次Insert耗时:" << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms" << std::endl;
}