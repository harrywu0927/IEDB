#include<iostream>
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
    return 0;
}