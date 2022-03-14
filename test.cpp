#include<iostream>
#include"src/STDFB_header.h"
#include"src/DataTypeConvert.hpp"
using namespace std;

int main(){
    cout<<"DDD"<<endl;
    long len;
    //char * buf = testQuery(&len);
    //delete buf;
    EDVDB_LoadSchema("data.dat");
    return 0;
}