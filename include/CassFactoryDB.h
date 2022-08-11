#ifndef _CASS_FACTORYDB_H
#define _CASS_FACTORYDB_H
#endif
/**
 *  数据交换缓冲区
 *  查询时，buffer中的数据存放方式：第1个字节为查询到的变量总数(0<N<256)，
 *  随后依次为：数据类型代号1字节、（时间序列长度4字节）、（数组长度4字节）、路径编码10字节
 *  数据类型代号如下：
 *  1:UINT, 2:USINT, 3:UDINT, 4:INT, 5:BOOL, 6:SINT, 7:DINT, 8:REAL, 9:TIME, 10:IMAGE
 *  如果变量不是数组且携带时间，则此变量的代号值+10
 *  如果变量是数组且不带时间，则此变量的代号值+20
 *  如果变量是数组且携带时间，则此变量的代号值+30
 *  如果是时间序列，+40
 *  如果是时间序列和数组，+50
 *  当变量是数组时，数据类型代号后会紧跟4字节数组长度
 *  当变量是时间序列时，数据类型代号后会紧跟4字节时间序列长度
 *  当变量是数组和时间序列时，数据类型代号后会紧跟4字节时间序列长度以及4字节数组长度
 *  随后为数据区,按行依序排列
 */
#pragma once
struct DB_DataBuffer
{
    char *buffer;         //缓冲区地址
    long length;          //缓冲区总长度
    const char *savePath; //存储路径
    int bufferMalloced;   //是否查询到数据
};
//比较方式
enum DB_CompareType
{
    CMP_NONE,
    GT, //大于
    LT, //小于
    EQ, //等于
    GE, //大于等于
    LE  //小于等于
};
//查询条件
enum DB_QueryType
{
    TIMESPAN, //时间区间
    LAST,     //最新条数
    FILEID,   //文件ID
    QRY_NONE
};
//排列方式
enum DB_Order
{
    ASCEND,   //按值升序
    DESCEND,  //按值降序
    DISTINCT, //去除重复
    TIME_ASC, //按时间升序
    TIME_DSC, //按时间降序
    ODR_NONE
};
//查询请求参数
struct DB_QueryParams
{
    char *pathCode;                  //路径编码
    const char *valueName;           //变量名
    int byPath;                      // 1表示根据路径编码查询，0表示根据变量名查询
    int isContinue;                  //是否继续获取数据，1表示接续上次未传输完的数据，0表示结束
    long start;                      //开始时间
    long end;                        //结束时间
    long queryNums;                  //查询记录条数
    const char *compareValue;        //比较某个值
    const char *fileID;              //文件ID
    const char *pathToLine;          //到产线层级的路径
    enum DB_CompareType compareType; //比较方式
    enum DB_QueryType queryType;     //查询条件
    enum DB_Order order;             //排列方式
    const char *sortVariable;        //排序的变量
    const char *compareVariable;     //比较的变量
    int queryID;                     //请求ID
    const char *fileIDend;           //结束文件ID
};

//在指定路径下从模版文件(.tem)加载模版
int DB_LoadSchema(const char *pathToSet);

//卸载指定路径下的当前模版
int DB_UnloadSchema(const char *pathToUnset);

//读取指定变量的数据到新开辟的缓冲区，读取后需要清空此缓冲区
int DB_QueryByPath(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//读取指定变量一段时间内的数据到缓冲区
int DB_QueryByTimespan(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//读取指定变量最新的若干条记录
int DB_QueryLastRecords(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//根据文件ID和变量在某一产线文件夹下的数据文件中查询数据
int DB_QueryByFileID(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//读取所有符合查询条件的整个文件的数据
int DB_QueryWholeFile(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//自定义查询
int DB_ExecuteQuery(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//插入一条记录
int DB_InsertRecord(struct DB_DataBuffer *buffer, int zip);

//插入多条记录
int DB_InsertRecords(struct DB_DataBuffer buffer[], int recordsNum, int addTime);

//按条件删除记录
int DB_DeleteRecords(struct DB_QueryParams *params);

//最大值
int DB_MAX(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//最小值
int DB_MIN(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//标准差
int DB_STD(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//方差
int DB_STDEV(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//求和
int DB_SUM(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//计次数
int DB_COUNT(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//平均值
int DB_AVG(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

//最大值
int DB_MAX(struct DB_DataBuffer *buffer);

//最小值
int DB_MIN(struct DB_DataBuffer *buffer);

//标准差
int DB_STD(struct DB_DataBuffer *buffer);

//方差
int DB_STDEV(struct DB_DataBuffer *buffer);

//求和
int DB_SUM(struct DB_DataBuffer *buffer);

//计次数
int DB_COUNT(struct DB_DataBuffer *buffer);

//平均值
int DB_AVG(struct DB_DataBuffer *buffer);

//中位数
int DB_MEDIAN(struct DB_DataBuffer *buffer);

//求积
int DB_PROD(struct DB_DataBuffer *buffer);

//离群点检测
int DB_Outlier_Detection(struct DB_QueryParams *params);

//异常节拍检测
int DB_GetAbnormalRhythm(struct DB_DataBuffer *buffer, struct DB_QueryParams *params, int mode, int no_query);

int DB_OutlierDetection(struct DB_DataBuffer *buffer, struct DB_QueryParams *params);

int DB_NoveltyFit(struct DB_QueryParams *params, double *maxLine, double *minLine);

//文件打包
int DB_Pack(const char *pathToLine, int num, int packAll);

//按条件统计正常数据条数
int DB_GetNormalDataCount(struct DB_QueryParams *params, long *count);

//按条件统计非正常数据条数
int DB_GetAbnormalDataCount(struct DB_QueryParams *params, long *count);

//读取文件内容到缓冲区
int DB_ReadFile(struct DB_DataBuffer *buffer);
//打开文件
// mode为打开文件的方式,如“wb”,"rb"等
int DB_Open(char *path, char *mode, long *fptr);

//写入数据
int DB_Write(long fp, char *buf, long length);

//读取文件,数据将存放在buf数组中，在Open过后使用
int DB_Read(long fp, char *buf);

//根据路径直接读取文件
int DB_OpenAndRead(char *path, char *buf);

//关闭文件
int DB_Close(long fp);

//创建文件夹
int DB_CreateDirectory(char *path);

//删除文件夹
int DB_DeleteDirectory(char *path);

//删除文件
int DB_DeleteFile(char *path);

//获取文件长度
int DB_GetFileLengthByPath(char *path, long *length);      //需要在Close过后使用
int DB_GetFileLengthByFilePtr(long fileptr, long *length); //使用此方式获取长度后，使用者有责任关闭文件

//备份
int DB_Backup(const char *path);

//数据恢复
int DB_Recovery(const char *path);

int DB_TemporaryFuncCall(void *param1, void *param2, void *param3, int funcID);

//标准模板数据信息
struct DB_TreeNodeParams
{
    char *pathCode;  //编码
    char *valueName; //变量名
    int valueType;   //数据类型
    int isArrary;
    int arrayLen;
    int isTS;
    int tsLen;
    unsigned int tsSpan;    //采样频率
    long startTime;         //开始时间
    long endTime;           //结束时间
    int hasTime;            //带８字节时间戳
    const char *pathToLine; //到产线层级的路径
    const char *newPath;
};

//压缩模板的数据信息
struct DB_ZipNodeParams
{
    char *valueName;     //变量名
    int valueType;       //数据类型
    char *standardValue; //标准值
    char *maxValue;      //最大值
    char *minValue;      //最小值
    int isArrary;
    int arrayLen;
    int isTS;
    int tsLen;
    unsigned int tsSpan;    //采样频率
    int hasTime;            //带八字节时间戳
    const char *pathToLine; //到产线层级的路径
    const char *newPath;
};

//往标准模板里添加新的树节点
int DB_AddNodeToSchema(struct DB_TreeNodeParams *TreeParams);

//修改标准模板里的树节点
int DB_UpdateNodeToSchema(struct DB_TreeNodeParams *TreeParams, struct DB_TreeNodeParams *newTreeParams);

//删除标准模板里的树节点
int DB_DeleteNodeToSchema(struct DB_TreeNodeParams *TreeParams);

//往压缩模板里添加新的树节点
int DB_AddNodeToZipSchema(struct DB_ZipNodeParams *ZipParams);

//修改压缩模板里的树节点
int DB_UpdateNodeToZipSchema(struct DB_ZipNodeParams *ZipParams, struct DB_ZipNodeParams *newZipParams);

//删除压缩模板里的树节点
int DB_DeleteNodeToZipSchema(struct DB_ZipNodeParams *ZipParams);

//在指定路径下从压缩模板文件(.ziptem)加载模板
int DB_LoadZipSchema(const char *pathToSet);

//卸载指定路径下的压缩模板
int DB_UnloadZipSchema(const char *pathToUnset);

//查询模板信息参数
struct DB_QueryNodeParams
{
    const char *valueName; //变量名
    char *pathcode;        //编码
    int valueType;         //数据类型
    char *standardValue;   //标准值
    char *maxValue;        //最大值
    char *minValue;        //最小值
    int isArrary;
    int arrayLen;
    int isTS;
    int tsLen;
    unsigned int tsSpan;    //采样频率
    int hasTime;            //带八字节时间戳
    const char *pathToLine; //到产线层级的路径
};

//查询模板节点信息
int DB_QueryNode(struct DB_QueryNodeParams *QueryParams);

//查询压缩模板节点信息
int DB_QueryZipNode(struct DB_QueryNodeParams *QueryParams);

//压缩已有文件
int DB_ZipFile(const char *ZipTemPath, const char *pathToLine);

//还原压缩后的文件
int DB_ReZipFile(const char *ZipTemPath, const char *pathToLine);

//压缩接收到的整条数据
int DB_ZipRecvBuff(const char *ZipTemPath, const char *filepath, char *buff, long *buffLength);

//压缩只有开关量的文件
int DB_ZipSwitchFile(const char *ZipTemPath, const char *pathToLine);

//还原被压缩的开关量文件返回原状态
int DB_ReZipSwitchFile(const char *ZipTemPath, const char *filepath);

//压缩接收到的只有开关量类型的整条数据
int DB_ZipRecvSwitchBuff(const char *ZipTemPath, const char *filepath, char *buff, long *buffLength);

//压缩只有模拟量的文件
int DB_ZipAnalogFile(const char *ZipTemPath, const char *pathToLine);

//还原被压缩的模拟量文件返回原状态
int DB_ReZipAnalogFile(const char *ZipTemPath, const char *pathToLine);

//压缩接收到的只有模拟量类型的整条数据
int DB_ZipRecvAnalogFile(const char *ZipTempPath, const char *filepath, char *buff, long *buffLength);

enum DB_ZipType
{
    TIME_SPAN,
    FILE_ID,
    PATH_TO_LINE,
};
struct DB_ZipParams
{
    const char *pathToLine; //到产线级的路径
    long start;             //开始时间
    long end;               //结束时间
    const char *fileID;     //文件ID
    long zipNums;
    const char *EID;
    enum DB_ZipType ZipType; //压缩方式
    char *buffer;            //数据缓存
    long bufferLen;          //数据长度
};

//根据时间段压缩开关量类型.idb文件
int DB_ZipSwitchFileByTimeSpan(struct DB_ZipParams *params);

//根据时间段压缩模拟量类型.idb文件
int DB_ZipAnalogFileByTimeSpan(struct DB_ZipParams *params);

//根据时间段压缩.idb文件
int DB_ZipFileByTimeSpan(struct DB_ZipParams *params);

//根据时间段还原开关量类型.idbzip文件
int DB_ReZipSwitchFileByTimeSpan(struct DB_ZipParams *params);

//根据时间段还原模拟量类型.idbzip文件
int DB_ReZipAnalogFileByTimeSpan(struct DB_ZipParams *params);

//根据时间段还原.idbzip文件
int DB_ReZipFileByTimeSpan(struct DB_ZipParams *params);

//根据文件ID压缩开关量.idb文件
int DB_ZipSwitchFileByFileID(struct DB_ZipParams *params);

//根据文件ID压缩模拟量.idb文件
int DB_ZipAnalogFileByFileID(struct DB_ZipParams *params);

//根据文件ID压缩.idb文件
int DB_ZipFileByFileID(struct DB_ZipParams *params);

//根据文件ID还原开关量类型.idbzip文件
int DB_ReZipSwitchFileByFileID(struct DB_ZipParams *params);

//根据文件ID还原模拟量类型.idbzip文件
int DB_ReZipAnalogFileByFileID(struct DB_ZipParams *params);

//根据文件ID还原.idbzip文件
int DB_ReZipFileByFileID(struct DB_ZipParams *params);

// #ifdef __cplusplus
// }
// #endif
