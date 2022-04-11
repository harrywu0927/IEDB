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
#include <pthread.h>
#include <tuple>
#include <time.h>
#include <thread>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
using namespace std;
void *tick(void *ptr)
{
    int n=5;
    while (n--)
    {
        cout << "Hello World!" << endl;
        sleep(1);
    }
}
void tk(int n)
{
    for (size_t i = 0; i < n; i++)
    {
        cout << "Hello World!" << endl;
        sleep(1);
    }
    
}
int main()
{

    //std::thread t1(tk,5);
    //t1.join();
    // pthread_t pid;
    // int ret = pthread_create(&pid, NULL, tick, NULL);
    // if (ret != 0)
    // {
    //     cout << "pthread_create error: error_code=" << ret << endl;
    // }
    // pthread_exit(NULL);
    // DB_QueryParams params;
    // params.pathToLine = "jinfei";
    // params.fileID = "JinfeiTen102";
    // char code[10];
    // code[0] = (char)0;
    // code[1] = (char)1;
    // code[2] = (char)0;
    // code[3] = (char)2;
    // code[4] = 0;
    // code[5] = (char)0;
    // code[6] = 0;
    // code[7] = (char)0;
    // code[8] = (char)0;
    // code[9] = (char)0;
    // params.pathCode = code;
    // params.valueName = "S2OFF";
    // // params.valueName = NULL;
    // params.start = 1648812610100;
    // params.end = 1648812630100;
    // params.order = ASCEND;
    // params.compareType = GT;
    // params.compareValue = "666";
    // params.queryType = LAST;
    // params.byPath = 1;
    // params.queryNums = 8;
    // DB_DataBuffer buffer;
    // buffer.savePath = "/";
    // vector<long> bytes, positions;
    // vector<DataType> types;
    // // cout << settings("Pack_Mode") << endl;
    // // vector<pair<string, long>> files;
    // // readDataFilesWithTimestamps("", files);
    // // Packer::Pack("/",files);
    // DB_QueryLastRecords(&buffer, &params);
    // // DB_QueryByFileID(&buffer, &params);

    // DB_ZipSwitchFile("/jinfei/","/jinfei/");
    // DB_ReZipSwitchFile("/jinfei/","/jinfei/");
    // DB_ZipAnalogFile("/jinfei/","/jinfei/");
    // DB_ReZipAnalogFile("jinfei/", "jinfei/");
    // DB_ZipFile("jinfei/","jinfei/");
    // DB_ReZipFile("jinfei/", "jinfei/");
    //cout<<curNum["jinfeitwo"]<<endl;

    // // DB_QueryParams params;
    // // params.pathToLine = "jinfei";
    // // params.fileID = "JinfeiTen102";
    // // char code[10];
    // // code[0] = (char)0;
    // // code[1] = (char)1;
    // // code[2] = (char)0;
    // // code[3] = (char)2;
    // // code[4] = 0;
    // // code[5] = (char)0;
    // // code[6] = 0;
    // // code[7] = (char)0;
    // // code[8] = (char)0;
    // // code[9] = (char)0;
    // // params.pathCode = code;
    // // params.valueName = "S2OFF";
    // // // params.valueName = NULL;
    // // params.start = 1648812610100;
    // // params.end = 1648812630100;
    // // params.order = ASCEND;
    // // params.compareType = GT;
    // // params.compareValue = "666";
    // // params.queryType = LAST;
    // // params.byPath = 1;
    // // params.queryNums = 8;
    // // DB_DataBuffer buffer;
    // // buffer.savePath = "/";
    // // vector<long> bytes, positions;
    // // vector<DataType> types;
    // // // cout << settings("Pack_Mode") << endl;
    // // // vector<pair<string, long>> files;
    // // // readDataFilesWithTimestamps("", files);
    // // // Packer::Pack("/",files);
    // // DB_QueryLastRecords(&buffer, &params);
    // // // DB_QueryByFileID(&buffer, &params);

    // // if (buffer.bufferMalloced)
    // // {
    // //     char buf[buffer.length];
    // //     memcpy(buf, buffer.buffer, buffer.length);
    // //     cout << buffer.length << endl;
    // //     for (int i = 0; i < buffer.length; i++)
    // //     {
    // //         cout << (int)buf[i] << " ";
    // //         if (i % 11 == 0)
    // //             cout << endl;
    // //     }

    // //     free(buffer.buffer);
    // // }
    // //return 0;
    // DB_DataBuffer buffer;
    // buffer.savePath = "jinfei";
    // buffer.length = 11;
    // char buf[11] = {'3'};
    // buffer.buffer = buf;
    // for (int i = 0; i < 30; i++)
    // {
    //     char type = 1;
    //     memcpy(buffer.buffer,&type,1);
    //     DB_InsertRecord(&buffer, 1);
    //     usleep(100000);
    // }
    // return 0;
    DB_DataBuffer buffer;
    buffer.savePath = "jinfei";
    buffer.length = 11;
    char buf[11] = {'3'};
    buffer.buffer = buf;
    for (int i = 0; i < 10; i++)
    {
        char type = 1;
        memcpy(buffer.buffer,&type,1);
        DB_InsertRecord(&buffer, 1);
        usleep(10000000);
    }

    // DB_ReadFile(&buffer);
    return 0;
}