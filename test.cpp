#include<iostream>
#include"include/STDFB_header.h"
#include"include/DataTypeConvert.hpp"
using namespace std;

int main(){
    cout<<"DDD"<<endl;
    long len;
    //char * buf = testQuery(&len);
    //delete buf;
    EDVDB_LoadSchema("data.dat");
    return 0;
}