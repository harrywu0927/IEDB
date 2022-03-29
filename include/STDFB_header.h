
#ifdef __cplusplus
extern "C"
{
#endif
    /*
     *  数据交换缓冲区
     *  查询时，buffer中的数据存放方式：第1个字节为查询到的数据类型总数(N)，
     *  随后的N*11字节依次为：数据类型代号1字节、路径编码10字节
     *  数据类型代号如下：
     *  1:UINT, 2:USINT, 3:UDINT, 4:INT, 5:BOOL, 6:SINT, 7:DINT, 8:REAL, 9:TIME, 10:IMAGE
     *  如果变量携带时间，则此变量的代号值+10
     *  随后为数据区
     *  查询整个文件时，只有数据区
    */
    struct DataBuffer
    {
        char *buffer;   //缓冲区地址
        long length;    //缓冲区总长度
        const char *savePath; //存储路径
        int bufferMalloced; //是否查询到数据
    };
    //比较方式
    enum CompareType{
        CMP_NONE,
        GT, //大于
        LT, //小于
        EQ, //等于
        GE, //大于等于
        LE  //小于等于
    };
    //查询条件
    enum QueryType{
        TIMESPAN,   //时间区间
        LAST,       //最新条数
        FILEID,     //文件ID
        QRY_NONE
    };
    //排列方式
    enum Order{
        ASCEND,     //按值升序
        DESCEND,    //按值降序
        DISTINCT,   //去除重复
        TIME_ASC,   //按时间升序
        TIME_DSC,   //按时间降序
        ODR_NONE
    };
    //查询请求参数
    struct QueryParams
    {
        char *pathCode; //路径编码
        const char *valueName;  //变量名
        int byPath;     //1表示根据路径编码查询，0表示根据变量名查询
        int isContinue; //是否继续获取数据，1表示接续上次未传输完的数据，0表示结束
        long start;     //开始时间
        long end;       //结束时间
        long queryNums; //查询记录条数
        const char *compareValue;  //比较某个值
        const char *fileID;   //文件ID
        const char *pathToLine;    //到产线层级的路径
        enum CompareType compareType;  //比较方式
        enum QueryType queryType;   //查询条件
        enum Order order;       //排列方式
        int queryID;    //请求ID
    };

    //在指定路径下从模版文件(.tem)加载模版
    int EDVDB_LoadSchema(const char *pathToSet);

    //卸载指定路径下的当前模版
    int EDVDB_UnloadSchema(const char *pathToUnset);

    //读取指定变量的数据到新开辟的缓冲区，读取后需要清空此缓冲区
    int EDVDB_QueryByPath(struct DataBuffer *buffer, struct QueryParams *params);

    //读取指定变量一段时间内的数据到缓冲区
    int EDVDB_QueryByTimespan(struct DataBuffer *buffer, struct QueryParams *params);

    //读取指定变量最新的若干条记录
    int EDVDB_QueryLastRecords(struct DataBuffer *buffer, struct QueryParams *params);

    //根据文件ID和变量在某一产线文件夹下的数据文件中查询数据
    int EDVDB_QueryByFileID(struct DataBuffer *buffer, struct QueryParams *params);

    //读取所有符合查询条件的整个文件的数据
    int EDVDB_QueryWholeFile(struct DataBuffer *buffer, struct QueryParams *params);

    //自定义查询
    int EDVDB_ExecuteQuery(struct DataBuffer *buffer, struct QueryParams *params);

    //插入一条记录
    int EDVDB_InsertRecord(struct DataBuffer *buffer, int addTime);

    //插入多条记录
    int EDVDB_InsertRecords(struct DataBuffer buffer[], int recordsNum, int addTime);

    //按条件删除记录
    int EDVDB_DeleteRecords(struct QueryParams *params);

    //最大值
    int EDVDB_MAX(struct DataBuffer *buffer, struct QueryParams *params); 
    
    //最小值
    int EDVDB_MIN(struct DataBuffer *buffer, struct QueryParams *params);

    //标准差
    int EDVDB_STD(struct DataBuffer *buffer, struct QueryParams *params);

    //方差
    int EDVDB_STDEV(struct DataBuffer *buffer, struct QueryParams *params);

    //求和
    int EDVDB_SUM(struct DataBuffer *buffer, struct QueryParams *params);

    //计次数
    int EDVDB_COUNT(struct DataBuffer *buffer, struct QueryParams *params);

    //平均值
    int EDVDB_AVG(struct DataBuffer *buffer, struct QueryParams *params);

#ifdef __cplusplus
}
#endif
