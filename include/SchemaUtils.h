#include "utils.hpp"

using namespace std;

/**
 * @brief 模板文件.tem单个节点的顺序为30字节变量名，30字节数据类型等信息，1字节是否带时间戳，10字节编码
 *  压缩模板文件.ziptem单个节点的顺序为30字节变量名，30字节数据类型等信息，5字节标准值(数组则数组长度乘以5)，
 *  5字节最大值(数组则数组长度乘以5)，5字节最小值(数组则数组长度乘以5)，1字节是否带时间戳，4字节采样频率（采样节拍）
 *
 */
#define SCHEMA_VARIABLENAME_LENGTH 30
#define SCHEMA_DATATYPE_LENGTH 30
#define SCHEMA_HASTIME_LENGTH 1
#define SCHEMA_PATHCODE_LENGTH 10
#define SCHEMA_MAXVALUE_LENGTH 5
#define SCHEMA_MINVALUE_LENGTH 5
#define SCHEMA_STANDARDVALUE_LENGTH 5
#define SCHEMA_TIMESERIES_SPAN_LENGTH 4
#define SCHEMA_SINGLE_NODE_LENGTH 71
#define SCHEMA_SINGLE_ZIPNODE_LENGTH 80
#define VALUE_STRING_LENGTH 10

enum MaxOrMin
{
    STANDARD,
    MAX,
    MIN
};

//检查模板相关输入
int checkInputVaribaleName(string variableName);

int checkInputPathcode(char pathcode[]);

int checkInputSingleValue(int variableType, string value);

int checkInputValue(string variableType, string value, int isArray, int arrayLen);

int checkSingleValueRange(int variableType, string standardValue, string maxValue, string minValue);

int checkValueRange(string variableType, string standardValue, string maxValue, string minValue, int isArray, int arrayLen);

int checkQueryNodeParam(struct DB_QueryNodeParams *params);

int getValueStringByValueType(char *value, ValueType::ValueType type, int schemaPos, int MaxOrMin, int isArray, int arrayLen);

//多模板相关函数
int DB_AddNodeToSchema_MultiTem(struct DB_TreeNodeParams *TreeParams);

int DB_UpdateNodeToSchema_MultiTem(struct DB_TreeNodeParams *TreeParams, struct DB_TreeNodeParams *newTreeParams);

int DB_DeleteNodeToSchema_MultiTem(struct DB_TreeNodeParams *TreeParams);

int DB_AddNodeToZipSchema_MultiZiptem(struct DB_ZipNodeParams *ZipParams);

int DB_UpdateNodeToZipSchema_MultiZiptem(struct DB_ZipNodeParams *ZipParams, struct DB_ZipNodeParams *newZipParams);

int DB_DeleteNodeToZipSchema_MultiZiptem(struct DB_ZipNodeParams *ZipParams);