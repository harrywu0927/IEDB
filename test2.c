#include<stdio.h>
#include"src/STDFB_header.h"
#include <stdlib.h>

int main(){
    long len;
    //EDVDB_LoadSchema("rabbit.dat");
    //char* buf = testQuery(&len);
    //char* c;
    //printf("%d",sizeof(c));
    //return 0;
    struct DataBuffer buffer;
    

    printf("%s",buffer.buffer);
    free(buffer.buffer);
    //printf("%s",buffer.buffer);
    return 0;
}