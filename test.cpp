#include <iostream>
#include "include/CassFactoryDB.h"
#include "include/utils.hpp"
#include "include/spdlog/spdlog.h"
#include "include/compress/LzmaLib.h"
// #include <numpy/arrayobject.h>
#include <string>
#include <fstream>
#include <dirent.h>
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <tuple>
#include <time.h>
#include <thread>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <fcntl.h>
#include <ctime>
#include <future>
#ifndef __linux__
#include <mach/mach.h>
#endif
#include <mutex>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <experimental/filesystem>
using namespace std;
#ifdef __linux__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
mutex imute, countmute;
// namespace fs = std::filesystem;
void checkSettings()
{
    FILE *fp = fopen("settings.json", "r+");

    while (1)
    {
        fseek(fp, 0, SEEK_END);
        int len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char buff[len];
        fread(buff, len, 1, fp);
        string str = buff;
        string contents(str);
        neb::CJsonObject tmp(contents);

        cout << tmp("Filename_Label") << endl;

        this_thread::sleep_for(chrono::seconds(3));
    }
}

int main()
{DataTypeConverter converter;
	DB_QueryParams params;
	params.pathToLine = "JinfeiTem";
	params.fileID = "62";
	// params.fileIDend = "300000";
	params.fileIDend = NULL;
	char code[10];
	code[0] = (char)0;
	code[1] = (char)0;
	code[2] = (char)0;
	code[3] = (char)0;
	code[4] = 0;
	// code[4] = 'R';
	code[5] = (char)0;
	code[6] = 0;
	code[7] = (char)0;
	code[8] = (char)0;
	code[9] = (char)0;
	params.pathCode = code;
	// params.valueName = "S1ON,S1OFF";
	params.valueName = "A1RTem";
	params.start = 1553728593562;
	params.end = 1751908603642;
	// params.end = 1651894834176;
	params.order = ODR_NONE;
	params.sortVariable = "S1ON";
	params.compareType = CMP_NONE;
	params.compareValue = "100";
	params.compareVariable = "S1ON";
	params.queryType = FILEID;
	params.byPath = 0;
	params.queryNums = 1;
	DB_DataBuffer buffer;
	buffer.savePath = "/";
	// cout << settings("Pack_Mode") << endl;
	// vector<pair<string, long>> files;
	// readDataFilesWithTimestamps("", files);
	// Packer::Pack("/",files);
	auto startTime = std::chrono::system_clock::now();
	// char zeros[10] = {0};
	// memcpy(params.pathCode, zeros, 10);
	cout<<DB_QueryByFileID(&buffer, &params);
	// return 0;
	// DB_QueryLastRecords(&buffer, &params);
	// DB_QueryByTimespan_Single(&buffer, &params);
	auto endTime = std::chrono::system_clock::now();
	// free(buffer.buffer);
	std::cout << "第一次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;

	if (buffer.bufferMalloced)
	{
		cout << buffer.length << endl;
		for (int i = 0; i < buffer.length; i++)
		{
			cout << (int)*(char *)(buffer.buffer + i) << " ";
			if (i % 11 == 0)
				cout << endl;
		}

		free(buffer.buffer);
	}
	// buffer.buffer = NULL;
	return 0;
    // spdlog::info("welcome");
    // spdlog::critical("critical");
    // spdlog::error("error");
    // return 0;
    DB_ZipNodeParams param;
    param.pathToLine = "arrTest";
    param.valueName = "DB1";
    param.valueType = 8;
    param.newPath = "newArrTest";
    param.hasTime = 1;
    param.isTS = 1;
    param.tsLen = 10;
    param.tsSpan = 1000;
    param.isArrary = 1;
    param.arrayLen = 5;
    param.standardValue = "-100.5 -200.5 -300.5 -400.5 -500.5";
    param.maxValue = "-600.5 -700.5 -800.5 -900.5 -1000.5";
    param.minValue = "-1100.5 -1200.5 -1300.5 -1400.5 -1500.5";
    // DB_AddNodeToZipSchema(&param);
    // DB_DeleteNodeToZipSchema(&param);
    DB_ZipNodeParams newParam;
    newParam.pathToLine = "dsfsf";
    newParam.newPath = "newArrTest";
    newParam.valueName = "DB77";
    newParam.valueType = 3;
    newParam.isTS = 0;
    newParam.tsLen = 6;
    newParam.isArrary = 0;
    newParam.arrayLen = 10;
    //char a[4] = {'1','2','0',0};
    newParam.maxValue = "999";
    newParam.minValue = "98";
    newParam.standardValue = "80";
    newParam.tsSpan = 20000;
    newParam.hasTime = 0;
    DB_UpdateNodeToZipSchema(&param, &newParam);
    return 0;
    // DB_LoadZipSchema("arrTest");
    // DataTypeConverter dt;
    // cout<<CurrentZipTemplate.schemas.size()<<endl;
    // char test[5][5];
    // memcpy(test[0],CurrentZipTemplate.schemas[0].second.max[0],5);
    // uint32_t max1 = dt.ToUInt32_m(test[0]);
    // memcpy(test[1],CurrentZipTemplate.schemas[0].second.max[1],5);
    // uint32_t max2 = dt.ToUInt32_m(test[1]);
    // memcpy(test[2],CurrentZipTemplate.schemas[0].second.max[2],5);
    // uint32_t max3 = dt.ToUInt32_m(test[2]);
    // memcpy(test[3],CurrentZipTemplate.schemas[0].second.max[3],5);
    // uint32_t max4 = dt.ToUInt32_m(test[3]);
    // memcpy(test[4],CurrentZipTemplate.schemas[0].second.max[4],5);
    // uint32_t max5 = dt.ToUInt32_m(test[4]);
    // cout<<max1<<endl;cout<<max2<<endl;cout<<max3<<endl;cout<<max4<<endl;cout<<max5<<endl;

    // char bool1 = CurrentZipTemplate.schemas[1].second.max[0][0];
    // char bool2 = CurrentZipTemplate.schemas[1].second.max[1][0];
    // char bool3 = CurrentZipTemplate.schemas[1].second.max[2][0];
    // char bool4 = CurrentZipTemplate.schemas[1].second.max[3][0];
    // char bool5 = CurrentZipTemplate.schemas[1].second.max[4][0];
    // char bool6 = CurrentZipTemplate.schemas[1].second.max[5][0];
    // cout<<bool1<<endl;cout<<bool2<<endl;cout<<bool3<<endl;cout<<bool4<<endl;cout<<bool5<<endl;cout<<bool6<<endl;
    // return 0;
    FILE *fp = fopen("lib/libCassFactoryDB.dylib", "rb");
    Byte *buf = new Byte[1024 * 1024 * 10];
    int ret = 0;
    int len = 0;
    while ((ret = fread(buf, 1, 1024 * 1024, fp)) > 0)
    {
        len += ret;
    }
    fclose(fp);
    cout << len << endl;
    Byte *res = new Byte[1024 * 1024 * 10];
    size_t reslen = 1024 * 1024 * 10, outpropsize = LZMA_PROPS_SIZE;
    Byte outprops[5];
    cout << LzmaCompress(res, &reslen, buf, len, outprops, &outpropsize, 5, 1 << 24, 3, 0, 2, 32, 2) << endl;
    cout << reslen << endl;
    SizeT uncompressedsize = 1024 * 1024 * 10;
    cout << LzmaUncompress(buf, &uncompressedsize, res, &reslen, outprops, LZMA_PROPS_SIZE);
    delete[] buf;
    delete[] res;

    return 0;
    int process = 0;
    for (int i = 0; i < 10; i++)
    {
        printf("\rprocess %4d ", i);
        //在Linux下\033[K是清除光标到行尾的数据
        // printf("process %4d \r\033[K", process);
        // Linux下printf有缓冲区，要用fflush来显示，不然不能及时显示
        fflush(stdout);
        sleep(1);
    }
    return 0;
    fs::path testpath = "testIEDB/test2/fsdfeg.feg";
    cout << testpath.parent_path();
    try
    {
        for (auto const &direntry : fs::recursive_directory_iterator(testpath))
        {
            cout << direntry.path() << endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    return 0;
    fs::path mypath = "./testIEDB/";
    // fs::create_directories(mypath);
    auto freespace = fs::space(mypath);

    // fs::path file = "settings.json";
    // cout << (file.filename().extension() == ".json") << endl;
    // auto time = fs::last_write_time(file);
    // cout << decltype(time)::clock::to_time_t(time) << endl;
    // auto size = fs::file_size(file);
    // cout << size << endl;
    // for (auto const &dir_entry : fs::recursive_directory_iterator{"src"})
    // {
    //     cout << dir_entry.path() << endl;
    // }

    // // cout << mypath.filename() / "sdf.fe" << endl;
    // return 0;
    
    // int (*M)[10] = new int[10][10];
    // char(*maxValue)[10];
    // maxValue = new char[10][10];
    // char max[10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    // char min[10] = {'9', '8', '7', '6', '5', '4', '3', '2', '1', '0'};
    // memcpy(maxValue[1], min, 10);
    // memcpy(maxValue[0], max, 10);
    // for (int i = 0; i < 10; i++)
    //     cout << maxValue[0][i] << endl;
    // for (int i = 0; i < 10; i++)
    //     cout << maxValue[1][i] << endl;
    // delete[] maxValue;
    // return 0;

    // DB_DataBuffer dbf;
    // dbf.savePath = "JinfeiSeven/JinfeiSeven1527050_2022-5-12-18-10-35-620.idb";
    // DB_ReadFile(&dbf);
    // dbf.savePath = "TEST";
    // auto start = std::chrono::system_clock::now();
    // for (int i = 0; i < 100; i++)
    // {

    // }
    // auto end = std::chrono::system_clock::now();
    // std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << std::endl;
    // // sleep(100);
    // return 0;

    // DB_QueryParams params;
    // params.pathToLine = "JinfeiTem";
    // params.fileID = "62";
    // params.fileIDend = NULL;
    // char code[10];
    // code[0] = (char)0;
    // code[1] = (char)1;
    // code[2] = (char)0;
    // code[3] = (char)0;
    // code[4] = 0;
    // code[5] = (char)0;
    // code[6] = 0;
    // code[7] = (char)0;
    // code[8] = (char)0;
    // code[9] = (char)0;
    // params.pathCode = code;
    // params.valueName = "A1RTem";
    // // params.valueName = NULL;
    // // params.start = 0;
    // // params.end = 1652099030250;
    // params.start = 1553728593562;
    // params.end = 1651897393777;
    // params.order = ODR_NONE;
    // params.compareType = CMP_NONE;
    // params.compareValue = "666";
    // params.queryType = FILEID;
    // params.byPath = 0;
    // params.queryNums = 1;
    // DB_DataBuffer buffer;
    // buffer.savePath = "JinfeiTTE";
    // long count;
    // // char x[3] = {'1', '2', '3'};
    // // buffer.buffer = x;
    // // buffer.length = 3;
    // // DB_InsertRecord(&buffer, 0);
    // // sleep(100);
    // // return 0;
    // auto startTime = std::chrono::system_clock::now();
    // // DB_MAX(&buffer, &params);
    // for (int i = 0; i < 1000; i++)
    // {
    //     DB_QueryByTimespan(&buffer, &params);
    //     // cout << buffer.length << endl;
    //     free(buffer.buffer);
    // }

    // // if (buffer.bufferMalloced)
    // // {
    // //     char buf[buffer.length];
    // //     memcpy(buf, buffer.buffer, buffer.length);
    // //     cout << buffer.length << endl;
    // //     for (int i = 0; i < buffer.length; i++)
    // //     {
    // //         cout << (int)buf[i] << " ";
    // //         if (i % 11 == 0)
    // //             cout << endl;
    // //     }

    // //     free(buffer.buffer);
    // // }
    // // DB_MAX(&buffer);
    // auto endTime = std::chrono::system_clock::now();
    // std::cout << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
    // if (buffer.bufferMalloced)
    // {
    //     // char buf[buffer.length];
    //     // memcpy(buf, buffer.buffer, buffer.length);
    //     // cout << buffer.length << endl;
    //     // for (int i = 0; i < buffer.length; i++)
    //     // {
    //     //     cout << (int)buf[i] << " ";
    //     //     if (i % 11 == 0)
    //     //         cout << endl;
    //     // }

    //     // free(buffer.buffer);
    // }
    return 0;
}