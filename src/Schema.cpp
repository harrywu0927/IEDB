#include "../include/Schema.h"
#include "../include/FS_header.h"
#include "../include/STDFB_header.h"
using namespace std;
//往模板里添加新的树节点
int EDVDB_AddNodeToSchema(struct TreeNodeParams *params)
{
    return 0;
}

//修改模板里的树节点
int EDVDB_UpdateNodeToSchema(struct TreeNodeParams *params)
{
    return 0;
}

//删除模板里的树节点
int EDVDB_DeleteNodeToSchema(struct TreeNodeParams *params)
{
    return 0;
}

int EDVDB_LoadZipSchema(const char *path)
{
    vector<string> files;
    readFileList(path, files);
    string temPath = "";
    for (string file : files) //找到带有后缀ziptem的文件
    {
        string s = file;
        vector<string> vec = DataType::StringSplit(const_cast<char *>(s.c_str()), ".");
        if (vec[vec.size() - 1].find("ziptem") != string::npos)
        {
            temPath = file;
            break;
        }
    }
    if (temPath == "")
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    long length;
    EDVDB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &length);
    char buf[length];
    int err = EDVDB_OpenAndRead(const_cast<char *>(temPath.c_str()), buf);
    if (err != 0)
        return err;
    int i = 0;
    
    DataTypeConverter dtc;
    vector<string> dataName;
    vector<DataType> dataTypes;
    while (i < length)
    {
        char variable[30], dataType[30],standardValue[10],maxValue[10],minValue[10];
        memcpy(variable, buf + i, 30);
        i += 30;
        memcpy(dataType, buf + i, 30);
        i += 30;
        memcpy(standardValue, buf + i, 10);
        i += 10;
        memcpy(maxValue, buf + i, 10);
        i += 10;
        memcpy(minValue, buf + i, 10);
        i += 10;
        vector<string> paths;
        
        dataName.push_back(variable);
        DataType type;

        memcpy(type.standardValue, standardValue , 10);
        memcpy(type.maxValue, maxValue , 10);
        memcpy(type.minValue, minValue , 10);
        // strcpy(type.standardValue,standardValue);
        // strcpy(type.maxValue,maxValue);
        // strcpy(type.minValue,minValue);
        if (DataType::GetDataTypeFromStr(dataType, type) == 0)
        {    
            dataTypes.push_back(type);
        }
    }
    
    ZipTemplate tem(dataName, dataTypes, path);
    return ZipTemplateManager::SetTemplate(tem);
}

//压缩已有文件
int EDVDB_ZipFile(const char *ZipTemPath,string filepath)
{
    int err=0;
    err=EDVDB_LoadZipSchema(ZipTemPath);//加载压缩模板
    if(err)
    {
        cout<<"未加载模板"<<endl;
        return err;
    }
    long len;
    EDVDB_GetFileLengthByPath(const_cast<char *>(filepath.c_str()),&len);
    char readbuff[len];//文件内容
    char writebuff[len]={0};//写入没有被压缩的数据
    if(EDVDB_OpenAndRead(const_cast<char *>(filepath.c_str()),readbuff))//将文件内容读取到readbuff
        return errno;

    DataTypeConverter converter;
    long readbuff_pos=0;
    long writebuff_pos=0;

    for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++)//循环比较
    {
        if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::BOOL)//BOOL变量
        {
            char standardBool=CurrentZipTemplate.schemas[i].second.standardValue[0];
            
            //８个字节的时间数据,根据实际要求后续可能需要更改
            readbuff_pos+=8;
            if(standardBool != readbuff[readbuff_pos])
            {
                memcpy(writebuff+writebuff_pos,readbuff+readbuff_pos-8,9);
                writebuff_pos+=9;
            }
            readbuff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
           
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::USINT)//USINT变量
        {
            char standardUsint=CurrentZipTemplate.schemas[i].second.standardValue[0];
            char maxUsint=CurrentZipTemplate.schemas[i].second.maxValue[0];
            char minUsint=CurrentZipTemplate.schemas[i].second.minValue[0];

            //８个字节的时间数据,根据实际要求后续可能需要更改
            readbuff_pos+=8;
            if(standardUsint!=readbuff[readbuff_pos] && (readbuff[readbuff_pos]<minUsint || readbuff[readbuff_pos]>maxUsint))
            {
                memcpy(writebuff+writebuff_pos,readbuff+readbuff_pos-8,9);
                writebuff_pos+=9;
            }
            readbuff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::UINT)//UINT变量
        {
            uint16_t standardUint=converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
            uint16_t maxUint=converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
            uint16_t minUint=converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.minValue);

            //８个字节的时间数据,根据实际要求后续可能需要更改
            readbuff_pos+=8;
            char value[2]={0};
            memcpy(value,readbuff+readbuff_pos,2);
            uint16_t currentValue=converter.ToUInt16(value);
            if(currentValue!=standardUint && (currentValue<minUint || currentValue>maxUint))
            {
                memcpy(writebuff+writebuff_pos,readbuff+readbuff_pos-8,10);
                writebuff_pos+=10;
            }
            readbuff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::UDINT)//UDINT变量
        {
            uint32_t standardUdint=converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
            uint32_t maxUdint=converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
            uint32_t minUdint=converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

            //８个字节的时间数据,根据实际要求后续可能需要更改
            readbuff_pos+=8;
            char value[4]={0};
            memcpy(value,readbuff+readbuff_pos,4);
            uint32_t currentValue=converter.ToUInt32(value);
            if(currentValue!=standardUdint && (currentValue<minUdint || currentValue>maxUdint))
            {
                memcpy(writebuff+writebuff_pos,readbuff+readbuff_pos-8,12);
                writebuff_pos+=12;
            }
            readbuff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::SINT)//SINT变量
        {
            char standardSint=CurrentZipTemplate.schemas[i].second.standardValue[0];
            char maxSint=CurrentZipTemplate.schemas[i].second.maxValue[0];
            char minSint=CurrentZipTemplate.schemas[i].second.minValue[0];

            //８个字节的时间数据,根据实际要求后续可能需要更改
            readbuff_pos+=8;
            if(standardSint!=readbuff[readbuff_pos] && (readbuff[readbuff_pos]>=maxSint || readbuff[readbuff_pos]<=minSint))
            {
                memcpy(writebuff+writebuff_pos,readbuff+readbuff_pos-8,9);
                writebuff_pos+=9;
            }
            readbuff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::INT)//INT变量
        {
            short standardInt=converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
            short maxInt=converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
            short minInt=converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.minValue);

            //８个字节的时间数据,根据实际要求后续可能需要更改
            readbuff_pos+=8;
            char value[2]={0};
            memcpy(value,readbuff+readbuff_pos,2);
            short currentValue=converter.ToInt16(value);
            if(standardInt!=currentValue && (currentValue<minInt || currentValue>maxInt))
            {
                memcpy(writebuff+writebuff_pos,readbuff+readbuff_pos-8,10);
                writebuff_pos+=10;
            }
            readbuff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::DINT)//DINT变量
        {
            int standardDint=converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
            int maxDint=converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
            int minDint=converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

            //８个字节的时间数据,根据实际要求后续可能需要更改
            readbuff_pos+=8;
            char value[4]={0};
            memcpy(value,readbuff+readbuff_pos,4);
            int currentValue=converter.ToInt32(value);
            if(standardDint!=currentValue && (currentValue<minDint || currentValue>maxDint))
            {
                memcpy(writebuff+writebuff_pos,readbuff+readbuff_pos-8,12);
                writebuff_pos+=12;
            }
            readbuff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::REAL)//REAL变量
        {
            float standardFloat=converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
            float maxFloat=converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.maxValue);
            float minFloat=converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.minValue);

            //８个字节的时间数据,根据实际要求后续可能需要更改
            readbuff_pos+=8;
            char value[4]={0};
            memcpy(value,readbuff+readbuff_pos,4);
            float currentValue=converter.ToFloat(value);
            if(currentValue!=standardFloat && (currentValue<minFloat || currentValue>maxFloat))
            {
                memcpy(writebuff+writebuff_pos,readbuff+readbuff_pos-8,12);
                writebuff_pos+=12;
            }
            readbuff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::IMAGE)
        {
            //暂定２个字节的图片长度
            char length[2]={0};
            memcpy(length,readbuff+readbuff_pos,2);
            uint16_t imageLength=converter.ToUInt16(length);
            readbuff_pos+=2;

            //８个字节的时间数据,根据实际要求后续可能需要更改
            readbuff_pos+=8;
            
            memcpy(writebuff+writebuff_pos,readbuff+readbuff_pos-8,imageLength);
            writebuff_pos+=imageLength;
            readbuff_pos+=imageLength;
        }
    }
    
    if(writebuff_pos==readbuff_pos)//表明数据没有被压缩
    {
        cout<<"数据没有被压缩!"<<endl;
        return 1;//1表示数据没有被压缩
    }

    EDVDB_DeleteFile(const_cast<char *>(filepath.c_str()));//删除原文件
    long fp;
    string finalpath=filepath.append("zip");//给压缩文件后缀添加zip，暂定，根据后续要求更改
    //创建新文件并写入
    err = EDVDB_Open(const_cast<char *>(finalpath.c_str()),"wb",&fp);
    if (err == 0)
    {
        err = EDVDB_Write(fp, writebuff, writebuff_pos);
        
        if (err == 0)
        {
            return EDVDB_Close(fp);
        }
    }
    return err;
}

//压缩接收到的整条数据然后存入文件
int EDVDB_ZipRecvBuff(const char *ZipTemPath,string filepath,const char *buff,long buffLength)
{
    int err=0;
    err=EDVDB_LoadZipSchema(ZipTemPath);//加载压缩模板
    if(err)
    {
        cout<<"未加载模板"<<endl;
        return err;
    }
    long len=buffLength;

    char writebuff[len]={0};//写入没有被压缩的数据
    
    DataTypeConverter converter;
    long buff_pos=0;
    long writebuff_pos=0;

    for (size_t i = 0; i < CurrentZipTemplate.schemas.size(); i++)//循环比较
    {
        if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::BOOL)//BOOL变量
        {
            char standardBool=CurrentZipTemplate.schemas[i].second.standardValue[0];
            
            //８个字节的时间数据,根据实际要求后续可能需要更改
            buff_pos+=8;
            if(standardBool != buff[buff_pos])
            {
                memcpy(writebuff+writebuff_pos,buff+buff_pos-8,9);
                writebuff_pos+=9;
            }
            buff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
           
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::USINT)//USINT变量
        {
            char standardUsint=CurrentZipTemplate.schemas[i].second.standardValue[0];
            char maxUsint=CurrentZipTemplate.schemas[i].second.maxValue[0];
            char minUsint=CurrentZipTemplate.schemas[i].second.minValue[0];

            //８个字节的时间数据,根据实际要求后续可能需要更改
            buff_pos+=8;
            if(standardUsint!=buff[buff_pos] && (buff[buff_pos]<minUsint || buff[buff_pos]>maxUsint))
            {
                memcpy(writebuff+writebuff_pos,buff+buff_pos-8,9);
                writebuff_pos+=9;
            }
            buff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::UINT)//UINT变量
        {
            uint16_t standardUint=converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
            uint16_t maxUint=converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
            uint16_t minUint=converter.ToUInt16_m(CurrentZipTemplate.schemas[i].second.minValue);

            //８个字节的时间数据,根据实际要求后续可能需要更改
            buff_pos+=8;
            char value[2]={0};
            memcpy(value,buff+buff_pos,2);
            uint16_t currentValue=converter.ToUInt16(value);
            if(currentValue!=standardUint && (currentValue<minUint || currentValue>maxUint))
            {
                memcpy(writebuff+writebuff_pos,buff+buff_pos-8,10);
                writebuff_pos+=10;
            }
            buff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::UDINT)//UDINT变量
        {
            uint32_t standardUdint=converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
            uint32_t maxUdint=converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
            uint32_t minUdint=converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

            //８个字节的时间数据,根据实际要求后续可能需要更改
            buff_pos+=8;
            char value[4]={0};
            memcpy(value,buff+buff_pos,4);
            uint32_t currentValue=converter.ToUInt32(value);
            if(currentValue!=standardUdint && (currentValue<minUdint || currentValue>maxUdint))
            {
                memcpy(writebuff+writebuff_pos,buff+buff_pos-8,12);
                writebuff_pos+=12;
            }
            buff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::SINT)//SINT变量
        {
            char standardSint=CurrentZipTemplate.schemas[i].second.standardValue[0];
            char maxSint=CurrentZipTemplate.schemas[i].second.maxValue[0];
            char minSint=CurrentZipTemplate.schemas[i].second.minValue[0];

            //８个字节的时间数据,根据实际要求后续可能需要更改
            buff_pos+=8;
            if(standardSint!=buff[buff_pos] && (buff[buff_pos]>=maxSint || buff[buff_pos]<=minSint))
            {
                memcpy(writebuff+writebuff_pos,buff+buff_pos-8,9);
                writebuff_pos+=9;
            }
            buff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::INT)//INT变量
        {
            short standardInt=converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.standardValue);
            short maxInt=converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.maxValue);
            short minInt=converter.ToInt16_m(CurrentZipTemplate.schemas[i].second.minValue);

            //８个字节的时间数据,根据实际要求后续可能需要更改
            buff_pos+=8;
            char value[2]={0};
            memcpy(value,buff+buff_pos,2);
            short currentValue=converter.ToInt16(value);
            if(standardInt!=currentValue && (currentValue<minInt || currentValue>maxInt))
            {
                memcpy(writebuff+writebuff_pos,buff+buff_pos-8,10);
                writebuff_pos+=10;
            }
            buff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::DINT)//DINT变量
        {
            int standardDint=converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
            int maxDint=converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
            int minDint=converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

            //８个字节的时间数据,根据实际要求后续可能需要更改
            buff_pos+=8;
            char value[4]={0};
            memcpy(value,buff+buff_pos,4);
            int currentValue=converter.ToInt32(value);
            if(standardDint!=currentValue && (currentValue<minDint || currentValue>maxDint))
            {
                memcpy(writebuff+writebuff_pos,buff+buff_pos-8,12);
                writebuff_pos+=12;
            }
            buff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::REAL)//REAL变量
        {
            float standardFloat=converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.standardValue);
            float maxFloat=converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.maxValue);
            float minFloat=converter.ToFloat_m(CurrentZipTemplate.schemas[i].second.minValue);

            //８个字节的时间数据,根据实际要求后续可能需要更改
            buff_pos+=8;
            char value[4]={0};
            memcpy(value,buff+buff_pos,4);
            float currentValue=converter.ToFloat(value);
            if(currentValue!=standardFloat && (currentValue<minFloat || currentValue>maxFloat))
            {
                memcpy(writebuff+writebuff_pos,buff+buff_pos-8,12);
                writebuff_pos+=12;
            }
            buff_pos+=CurrentZipTemplate.schemas[i].second.valueBytes;
        }
        else if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::IMAGE)
        {
            //暂定２个字节的图片长度
            char length[2]={0};
            memcpy(length,buff+buff_pos,2);
            uint16_t imageLength=converter.ToUInt16(length);
            buff_pos+=2;

            //８个字节的时间数据,根据实际要求后续可能需要更改
            buff_pos+=8;
            
            memcpy(writebuff+writebuff_pos,buff+buff_pos-8,imageLength);
            writebuff_pos+=imageLength;
            buff_pos+=imageLength;
        }
    }

    if(writebuff_pos==buff_pos)
    {
        cout<<"数据未压缩"<<endl;
        
        long fp;
        //创建新文件并写入
        err = EDVDB_Open(const_cast<char *>(filepath.c_str()),"wb",&fp);
        if (err == 0)
        {
            err = EDVDB_Write(fp, writebuff, writebuff_pos);
        
            if (err == 0)
            {
                EDVDB_Close(fp);
            }
        }
        return 1;
    }

    long fp;
    string finalpath=filepath.append("zip");//给压缩文件后缀添加zip，暂定，根据后续要求更改
    //创建新文件并写入
    err = EDVDB_Open(const_cast<char *>(finalpath.c_str()),"wb",&fp);
    if (err == 0)
    {
        err = EDVDB_Write(fp, writebuff, writebuff_pos);
        
        if (err == 0)
        {
            return EDVDB_Close(fp);
        }
    }
    return err;
}

int main()
{
   // EDVDB_LoadZipSchema("./");
    long len;
    EDVDB_GetFileLengthByPath("XinFeng_0100.dat",&len);
    char readbuf[len];
    EDVDB_OpenAndRead("XinFeng_0100.dat",readbuf);

    EDVDB_ZipRecvBuff("/","XinFeng_0100.dat",readbuf,len);
    //EDVDB_ZipFile("/","XinFeng_0100.dat");
    return 0;
}