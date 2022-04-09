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
    DB_DataBuffer buffer;
    buffer.savePath = "jinfei";
    buffer.length = 11;
    buffer.buffer = "testsetstes";
    for (int i = 0; i < 11; i++)
    {
        DB_InsertRecord(&buffer, 0);
    }
    
    
    //DB_ReadFile(&buffer);
    return 0;
}