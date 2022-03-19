#include<stdio.h>
#include"include/STDFB_header.h"
#include <stdlib.h>
#include <sys/sysctl.h>

int main(){
    struct QueryParams params;
    char code[10];
    code[0] = 0;
    code[1] = 1;
    code[2] = 0;
    code[3] = 1;
    code[4] = 'R';
    code[5] = 1;
    code[6] = 0;
    code[7] = 0;
    code[8] = 0;
    code[9] = 0;
    params.pathCode = code;
    params.pathToLine = "./";
    params.fileID = "XinFeng8";
    struct DataBuffer buffer;
    // EDVDB_QueryByFileID(buffer,params);
    // for (size_t i = 0; i < buffer.length; i++)
    // {
    //     printf("%d ",buffer.buffer[i]);
    // }
    // free(buffer.buffer);
    
    return 0;
}