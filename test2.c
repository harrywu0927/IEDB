#include<stdio.h>
#include"include/STDFB_header.h"
#include <stdlib.h>
#include <sys/sysctl.h>
#include<string.h>

int main(){
    char *str = (char*) malloc(10);
    memcpy(str,"123",3);
    memcpy(str+3,"456",3);
    
    printf("%s",str);free(str);
    return 0;
    struct QueryParams params;
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
    struct DataBuffer buffer;
    if(buffer.buffer == NULL){
        printf("buffer null\n");
    }
    params.pathToLine = "./";
    params.fileID = "XinFeng1";
    buffer.length = 0;
    EDVDB_QueryByFileID(&buffer, &params);
    if(buffer.bufferMalloced)
        free(buffer.buffer);
    // EDVDB_QueryByFileID(buffer,params);
    // for (size_t i = 0; i < buffer.length; i++)
    // {
    //     printf("%d ",buffer.buffer[i]);
    // }
    // free(buffer.buffer);
    
    return 0;
}