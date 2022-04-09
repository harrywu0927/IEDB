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

    long len;

    DB_GetFileLengthByPath("Jinfei91_2022-4-1-19-28-49-807.idb",&len);
    cout<<len<<endl;
    char readbuf[len];
    DB_OpenAndRead("Jinfei91_2022-4-1-19-28-49-807.idb",readbuf);

    // DB_ZipSwitchFile("/jinfei/","/jinfei/");
    //   DB_ReZipSwitchFile("/jinfei/","/jinfei/");
    DB_ZipRecvSwitchBuff("/jinfei","/jinfei",readbuf,&len);
    cout<<len<<endl;
    return 0;
}