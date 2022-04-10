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
    // long fp;
    // DB_Open("testopen.idb","wb",&fp);
    // DB_Close(fp);
    // for (int i = 0; i < 50; i++)
    // {
    //     string path = "testidb" + to_string(i) + ".idbzip";
    //     FILE *fp = fopen(path.c_str(), "wb");
    //     fclose(fp);
    //     usleep(100000);
    // }
    DB_Pack("jinfei",0,0);
    return 0;

    DB_DataBuffer buffer;
    buffer.savePath = "jinfei";
    buffer.length = 11;
    char buf[11] = {'3'};
    buffer.buffer = buf;
    for (int i = 0; i < 30; i++)
    {
        char type = 1;
        memcpy(buffer.buffer,&type,1);
        DB_InsertRecord(&buffer, 1);
        usleep(100000);
    }
    
    
    //DB_ReadFile(&buffer);
    return 0;
}