#include "utils.hpp"

using namespace std;

#define ZIP_TIMESTAMP_SIZE 8
#define ZIP_ZIPPOS_SIZE 2
#define ZIP_ENDFLAG_SIZE 1
#define ZIP_ZIPTYPE_SIZE 1

enum ZipType
{
    ONLY_DATA,     //只有数据
    ONLY_TIME,     //只有时间戳
    DATA_AND_TIME, //既有数据又有时间戳
    TS_AND_ARRAY,  //既是时序又是数组
    ONLY_TS,       //只是时序
};

int CheckZipParams(DB_ZipParams *params);

class ZipUtils
{
private:
public:
    static void addZipPos(int schemaPos, char *writebuff, long &writebuff_pos);
    static void addEndFlag(char *writebuff, long &writebuff_pos);
    static void addZipType(ZipType ziptype, char *writebuff, long &writebuff_pos);
    static int IsTSZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType);
    static void IsArrayZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos);
    static int IsArrayZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType);
    static int IsTsAndArrayZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType);
    static void addStandardValue(int schemaPos, char *writebuff, long &writebuff_pos, ValueType::ValueType DataType);
    static void addArrayStandardValue(int schemaPos, char *writebuff, long &writebuff_pos, ValueType::ValueType DataType);
    static void addTsTime(int schemaPos, uint64_t startTime, char *writebuff, long &writebuff_pos, int tsPos);
    static int IsTSReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType);
    static int IsArrayReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos);
    static int IsArrayReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType);
    static int IsTsAndArrayReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType);
    static int IsNotArrayAndTSReZip(int schemaPos, char *writebuff, long &writebuff_pos, char *readbuff, long &readbuff_pos, ValueType::ValueType DataType);
};

//原单线程压缩还原函数
int DB_ZipSwitchFile_Single(const char *ZipTemPath, const char *pathToLine);

int DB_ReZipSwitchFile_Single(const char *ZipTemPath, const char *pathToLine);

int DB_ZipSwitchFileByTimeSpan_Single(struct DB_ZipParams *params);

int DB_ReZipSwitchFileByTimeSpan_Single(struct DB_ZipParams *params);

int DB_ZipSwitchFileByFileID_Single(struct DB_ZipParams *params);

int DB_ReZipSwitchFileByFileID_Single(struct DB_ZipParams *params);

int DB_ZipAnalogFile_Single(const char *ZipTemPath, const char *pathToLine);

int DB_ReZipAnalogFile_Single(const char *ZipTemPath, const char *pathToLine);

int DB_ZipAnalogFileByTimeSpan_Single(struct DB_ZipParams *params);

int DB_ReZipAnalogFileByTimeSpan_Single(struct DB_ZipParams *params);

int DB_ZipAnalogFileByFileID_Single(struct DB_ZipParams *params);

int DB_ReZipAnalogFileByFileID_Single(struct DB_ZipParams *params);

int DB_ZipFile_Single(const char *ZipTemPath, const char *pathToLine);

int DB_ReZipFile_Single(const char *ZipTemPath, const char *pathToLine);

int DB_ZipFileByTimeSpan_Single(struct DB_ZipParams *params);

int DB_ReZipFileByTimeSpan_Single(struct DB_ZipParams *params);

int DB_ZipFileByFileID_Single(struct DB_ZipParams *params);

int DB_ReZipFileByFileID_Single(struct DB_ZipParams *params);