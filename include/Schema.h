#include "utils.hpp"
//标准模板数据信息
struct TreeNodeParams
{
    char *pathCode;//编码
    char *valueName;//变量名
    ValueType::ValueType valueType;//数据类型

    long startTime;//开始时间
    long endTime;//结束时间
    bool hasTime;//带８字节时间戳
};

//压缩模板的数据信息
struct ZipNodeParams
{
    char *valueName;//变量名
    ValueType::ValueType valueType;//数据类型
    char *standardValue;//标准值
    char *maxValue;//最大值
    char *minValue;//最小值
    bool hasTime;//带八字节时间戳
};

//往模板里添加新的树节点
int EDVDB_AddNodeToSchema(struct TreeNodeParams *params);

//修改模板里的树节点
int EDVDB_UpdateNodeToSchema(struct TreeNodeParams *params);

//删除模板里的树节点
int EDVDB_DeleteNodeToSchema(struct TreeNodeParams *params); 

//在指定路径下从压缩模板文件(.ziptem)加载模板
int EDVDB_LoadZipSchema(const char *pathToSet);

//卸载指定路径下的压缩模板
int EDVDB_UnloadZipSchema(const char *pathToUnset);

//压缩已有文件
int EDVDB_ZipFile(const char* ZipTemPath,string filepath);

//还原压缩后的文件
int EDVDB_ReZipFile(const char *ZipTemPath,string filepath);

//压缩接收到的整条数据
int EDVDB_ZipRecvBuff(const char *ZipTemPath,string filepath,const char *buff,long buffLength);

//压缩只有开关量的文件
int EDVDB_ZipSwitchFile(const char *ZipTemPath,string filepath);

//还原被压缩的开关量文件返回原状态
int EDVDB_ReZipSwitchFile(const char *ZipTemPath,string filepath);

//压缩接收到的只有开关量类型的整条数据
int EDVDB_ZipRecvSwitchBuff(const char *ZipTemPath,string filepath,const char *buff,long buffLength);

//压缩只有模拟量的文件
int EDVDB_ZipAnalogFile(const char *ZipTemPath,string filepath);

//还原被压缩的模拟量文件返回原状态
int EDVDB_ReZipAnalogFile(const char *ZipTemPath,string filepath);

//压缩接收到的只有模拟量的文件
int EDVDB_ZipRecvAnalogFile(const char *ZipTempPath,string filepath,const char *buff,long buffLength);