
#ifdef __cplusplus
extern "C"
{
#endif
    struct DataBuffer
    {
        char *buffer;   //缓冲区地址
        long length;    //长度
        char *savePath; //存储路径
    };
    //在指定路径下从模版文件(.tem)加载模版
    int EDVDB_LoadSchema(char pathToSet[]);

    //卸载指定路径下的当前模版
    int EDVDB_UnloadSchema(char pathToUnset[]);

    //读取指定路径编码下的数据到新开辟的缓冲区，读取后需要清空此缓冲区
    //@param hasMore    1表示缓冲区已满，还有数据
    //@param isContinue 是否继续获取数据，1表示接续上次未传输完的数据，0表示结束
    int EDVDB_QueryByPath(struct DataBuffer *buffer, char pathCode[], int codeLength, int isContinue, int *hasMore);

    //读取指定路径编码下一段时间内的数据到缓冲区
    //@param hasMore    1表示缓冲区已满，还有数据
    //@param isContinue 是否继续获取数据，1表示接续上次未传输完的数据，0表示结束
    //@param start      起始时间
    //@param end        结束时间
    int EDVDB_QueryByTimezone(struct DataBuffer *buffer, char pathCode[], int codeLength, long start, long end, int isContinue, int *hasMore);

    //读取指定路径编码下最新的若干条记录
    //@param hasMore    1表示缓冲区已满，还有数据
    //@param isContinue 是否继续获取数据，1表示接续上次未传输完的数据，0表示结束
    //@param num        读取条数
    int EDVDB_QueryLastRecords(struct DataBuffer *buffer, char pathCode[], int codeLength, int num, int isContinue, int *hasMore);

    //插入一条记录
    int EDVDB_InsertRecord(struct DataBuffer *buffer);

    //插入多条记录
    int EDVDB_InsertRecords(struct DataBuffer buffer[]);

    //删除一条记录
    int EDVDB_DeleteRecord();

    //删除一段时间内的记录
    int EDVDB_DeleteRecords();

    //最大值
    //根据已查询得到的结果获取最大值
    //int EDVDB_MAX(DataBuffer *buffer);  
    //手动设置搜索条件，结果将存入buffer中
    //@param start  起始时间
    //@param end    结束时间
    int EDVDB_MAX(struct DataBuffer *buffer, char pathCode[], int codeLength, long start, long end); 

    //最小值
    int EDVDB_MIN();

    //标准差
    int EDVDB_STD();

    //方差
    int EDVDB_STDEV();

    //求和
    int EDVDB_SUM();

    //计次数
    int EDVDB_COUNT();

    //平均值
    int EDVDB_AVG();

    char *testQuery(long *len);

#ifdef __cplusplus
}
#endif
