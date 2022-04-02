#include <stdio.h>
#include "include/CassIEDB.h"
#include <stdlib.h>
#include <sys/sysctl.h>
#include <string.h>
#include <time.h>
int main()
{
    // char *data = "test";
    // struct DB_DataBuffer buffer;
    // buffer.buffer = data;
    // buffer.length = 4;
    // buffer.savePath = "";
    // DB_InsertRecord(&buffer,0);
    // return 0;
    printf("%d",DB_LoadZipSchema("/"));
    return 0;
    

    struct tm t;
    t.tm_year = 122;
    t.tm_mon = 3;
    t.tm_mday = 1;
    t.tm_hour = 19;
    t.tm_min = 30;
    t.tm_sec = 30;
    t.tm_isdst = -1;
    time_t seconds = mktime(&t);
    int ms = 100;
    long millis = seconds * 1000 + ms;
    //return 0;
    //start 1648812610100
    //end 1648812630100
    struct DB_QueryParams params;
    params.pathToLine = "";
    params.fileID = "XinFeng2";
    char code[10];
    code[0] = (char)0;
    code[1] = (char)1;
    code[2] = (char)0;
    code[3] = (char)1;
    code[4] = 'R';
    code[5] = (char)1;
    code[6] = 0;
    code[7] = (char)0;
    code[8] = (char)0;
    code[9] = (char)0;
    params.pathCode = code;
    params.valueName = "S1R8";
    params.start = 1648516212100;
    params.end = 1648516221100;
    params.order = ASCEND;
    params.compareType = GT;
    params.compareValue = "6";
    params.queryType = FILEID;
    params.byPath = 1;
    params.queryNums = 3;
    struct DB_DataBuffer buffer;

    DB_MAX(&buffer, &params);
    return 0;
    if (buffer.buffer == NULL)
    {
        printf("buffer null\n");
    }
    params.pathToLine = "./";
    params.fileID = "XinFeng1";
    buffer.length = 0;
    DB_QueryByFileID(&buffer, &params);
    if (buffer.bufferMalloced)
        free(buffer.buffer);
    // DB_QueryByFileID(buffer,params);
    // for (size_t i = 0; i < buffer.length; i++)
    // {
    //     printf("%d ",buffer.buffer[i]);
    // }
    // free(buffer.buffer);

    return 0;
}