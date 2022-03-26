
//打开文件
//mode为打开文件的方式,如“wb”,"rb"等
int EDVDB_Open(char path[], char mode[], long *fptr);

//写入数据
int EDVDB_Write(long fp, char buf[],long length);

//读取文件,数据将存放在buf数组中，在Open过后使用
int EDVDB_Read(long fp,char buf[]);

//根据路径直接读取文件
int EDVDB_OpenAndRead(char path[],char buf[]);

//关闭文件
int EDVDB_Close(long fp);

//创建文件夹
int EDVDB_CreateDirectory(char path[]);

//删除文件夹
int EDVDB_DeleteDirectory(char path[]);

//删除文件
int EDVDB_DeleteFile(char path[]);

//获取文件长度
int EDVDB_GetFileLengthByPath(char path[], long *length);  //需要在Close过后使用
int EDVDB_GetFileLengthByFilePtr(long fileptr, long *length);



