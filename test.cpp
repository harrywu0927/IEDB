#include<iostream>
<<<<<<< HEAD
#include<stdlib.h>
#include "string.h"
#include "include/Schema.h"
using namespace std;

int main(){
     
     short a=-10;
     short b=-24;
    char ch1[2]={0};
    char ch2[2]={0};
    char ch3[2]={0};
    char ch4[2]={0};

    memcpy(ch1,&a,2);
    memcpy(ch2,&b,2);
    for(int i=0;i<2;i++)
    {
        ch3[1-i]|=a;
        a>>=8;
        ch4[1-i]|=b;
        b>>=8;
    }
    DataTypeConverter dt;
     short test1=dt.ToInt16_m(ch1);
     short test2=dt.ToInt16_m(ch2);
     short test3=dt.ToInt16(ch3);
     short test4=dt.ToInt16(ch4);
    cout<<test1<<endl;
    cout<<test2<<endl;
    cout<<test3<<endl;
    cout<<test4<<endl;
=======
#include"include/STDFB_header.h"
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
    float test = 123.45;
    const char *res = "23456.78";
    stringstream ss;
    ss << res;
    ss >> test;
    return  0;



    float a = -800.2345;
    char buf[4];
    int x = -800;
    memcpy(buf,&x,4);
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
>>>>>>> fa0f9d8d4d6713d3bd9950fca4459c72927fe442
    return 0;
}