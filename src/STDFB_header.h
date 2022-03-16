
#ifdef __cplusplus
extern "C"
{
#endif
    //数据交换缓冲区
    struct DataBuffer
    {
        char *buffer;   //缓冲区地址
        long length;    //长度
        char *savePath; //存储路径
    };
    enum compareType{
        GT, //大于
        LT, //小于
        EQ, //等于
        GE, //大于等于
        LE  //小于等于
    };
    //查询请求参数
    struct QueryParams
    {
        char *pathCode; //路径编码
        int codeLength; //编码长度
        int isContinue; //是否继续获取数据，1表示接续上次未传输完的数据，0表示结束
        int hasMore;    //1表示缓冲区已满，还有数据
        long start;     //开始时间
        long end;       //结束时间
        long queryNums; //查询记录条数
        char* compareValue;  //比较某个值
        enum compareType type;  //比较类型
    };
    //在指定路径下从模版文件(.tem)加载模版
    int EDVDB_LoadSchema(char pathToSet[]);

    //卸载指定路径下的当前模版
    int EDVDB_UnloadSchema(char pathToUnset[]);

    //读取指定路径编码下的数据到新开辟的缓冲区，读取后需要清空此缓冲区
    int EDVDB_QueryByPath(struct DataBuffer *buffer, struct QueryParams *params);

    //读取指定路径编码下一段时间内的数据到缓冲区
    int EDVDB_QueryByTimezone(struct DataBuffer *buffer, struct QueryParams *params);

    //读取指定路径编码下最新的若干条记录
    int EDVDB_QueryLastRecords(struct DataBuffer *buffer, struct QueryParams *params);

    //自定义查询
    int EDVDB_ExecuteQuery(struct DataBuffer *buffer, struct QueryParams *params);

    //插入一条记录
    int EDVDB_InsertRecord(struct DataBuffer *buffer, int addTime);

    //插入多条记录
    int EDVDB_InsertRecords(struct DataBuffer buffer[], int addTime);

    //按条件删除记录
    int EDVDB_DeleteRecords(struct QureryParams *params);

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
