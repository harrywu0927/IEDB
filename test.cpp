#include<iostream>
#include"include/CassFactoryDB.h"
#include"include/utils.hpp"
#include <string>
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
using namespace std;

int main(){

    char buf[2];
    short x = 80;
    memcpy(buf,&x,2);
    float value = 0;
    void *pf;
    pf = &value;
    for (char i = 0; i < 4; i++)
    {
        *((unsigned char *)pf + i) = buf[i];
    }
    DataTypeConverter converter;
    cout<<value<<endl;
    cout<<converter.ToFloat(buf)<<endl;
    return 0;


    long curtime = getMilliTime();
    time_t time = curtime/1000;
    struct tm *dateTime = localtime(&time);
    string fileID = "xingfeng";
    string finalPath = "";
    //string time = ctime(&curtime);
    //time.pop_back();
    finalPath = finalPath.append(".").append("/")
                        .append(fileID).append("_")
                        .append(to_string(1900+dateTime->tm_year)).append("-")
                        .append(to_string(1+dateTime->tm_mon)).append("-")
                        .append(to_string(dateTime->tm_mday)).append("-")
                        .append(to_string(dateTime->tm_hour)).append("-")
                        .append(to_string(dateTime->tm_min)).append("-")
                        .append(to_string(dateTime->tm_sec)).append("-")
                        .append(to_string(curtime % 1000))
                        .append(".idb");
    cout<<finalPath<<endl;
    return 0;
}