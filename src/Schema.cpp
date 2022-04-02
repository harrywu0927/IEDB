
#include "../include/CassFactoryDB.h"
#include "../include/utils.hpp"
using namespace std;
//往模板里添加新的树节点
int DB_AddNodeToSchema(struct DB_TreeNodeParams *params)
{
    return 0;
}

//修改模板里的树节点
int DB_UpdateNodeToSchema(struct DB_TreeNodeParams *params)
{
    return 0;
}

//删除模板里的树节点
int DB_DeleteNodeToSchema(struct DB_TreeNodeParams *params)
{
    return 0;
}

/**
 * @brief 加载压缩模板
 * 
 * @param path 压缩模板路径
 * @return 0:success,
 *         others: StatusCode 
 */
int DB_LoadZipSchema(const char *path)
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
    DB_GetFileLengthByPath(const_cast<char *>(temPath.c_str()), &length);
    char buf[length];
    int err = DB_OpenAndRead(const_cast<char *>(temPath.c_str()), buf);
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

/**
 * @brief 卸载当前压缩模板
 * 
 * @param pathToUnset 压缩模板路径
 * @return  0:success,
 *          other:StatusCode
 */
int DB_UnloadZipSchema(const char *pathToUnset)
{
    return ZipTemplateManager::UnsetZipTemplate(pathToUnset);
}

/**
 * @brief 压缩已有的.idb文件
 * 
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idb文件路径
 * @return  0:success,
 *          other:StatusCode
 */
int DB_ZipFile(const char *ZipTemPath,const char *pathToLine)
{
    int err=0;
    err=DB_LoadZipSchema(ZipTemPath);//加载压缩模板
    if(err)
    {
        cout<<"未加载模板"<<endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;
    vector<string> files;
    readIDBFilesList(pathToLine,files);
    if(files.size()==0)
    {
        cout<<"没有文件！"<<endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    
    for(size_t fileNum=0;fileNum<files.size();fileNum++)//循环压缩文件夹下的所有.idb文件
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(files[fileNum].c_str()),&len);
        char readbuff[len];//文件内容
        char writebuff[len];//写入没有被压缩的数据
        if(DB_OpenAndRead(const_cast<char *>(files[fileNum].c_str()),readbuff))//将文件内容读取到readbuff
        {
            cout<<"未找到文件"<<endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        
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
        
        if(writebuff_pos==readbuff_pos)//表明数据没有被压缩,保持原文件
        {
            cout<<files[fileNum]+"文件数据没有被压缩!"<<endl;
            //return 1;//1表示数据没有被压缩
        }
        else
        {
            DB_DeleteFile(const_cast<char *>(files[fileNum].c_str()));//删除原文件
            long fp;
            string finalpath=files[fileNum].append("zip");//给压缩文件后缀添加zip，暂定，根据后续要求更改
            //创建新文件并写入
            err = DB_Open(const_cast<char *>(finalpath.c_str()),"wb",&fp);
            if (err == 0)
            {
                err = DB_Write(fp, writebuff, writebuff_pos);
                
                if (err == 0)
                {
                    DB_Close(fp);
                }
            }
        }   
    }
    return err;
}

/**
 * @brief 压缩收到的一整条数据
 * 
 * @param ZipTemPath 压缩模板路径
 * @param filepath 存储文件路径
 * @param buff 接收到的数据
 * @param buffLength 接收到的数据长度
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipRecvBuff(const char *ZipTemPath,const char *filepath,char *buff,long *buffLength)
{
    int err=0;
    err=DB_LoadZipSchema(ZipTemPath);//加载压缩模板
    if(err)
    {
        cout<<"未加载模板"<<endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    long len=*buffLength;

    char writebuff[len];//写入没有被压缩的数据
    
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
        err = DB_Open(const_cast<char *>(filepath),"wb",&fp);
        if (err == 0)
        {
            err = DB_Write(fp,const_cast<char *>(buff) , *buffLength);
        
            if (err == 0)
            {
                DB_Close(fp);
            }
        }
        return 1;//表示数据未压缩
    }

    long fp;
    string finalpath=filepath;
    finalpath=finalpath.append("zip");//给压缩文件后缀添加zip，暂定，根据后续要求更改
    //创建新文件并写入
    err = DB_Open(const_cast<char *>(finalpath.c_str()),"wb",&fp);
    if (err == 0)
    {
        err = DB_Write(fp, writebuff, writebuff_pos);
        
        if (err == 0)
        {
            DB_Close(fp);
        }
    }
    return err;
}

/**
 * @brief 压缩只有开关量类型的已有.idb文件
 * 
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idb文件路径
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipSwitchFile(const char *ZipTemPath,const char *pathToLine)
{
    int err;
    err=DB_LoadZipSchema(ZipTemPath);//加载压缩模板
    if(err)
    {
        cout<<"未加载模板"<<endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;
    vector<string> files;
    readIDBFilesList(pathToLine,files);
    if(files.size()==0)
    { 
        cout<<"没有文件！"<<endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }

    for(size_t fileNum=0;fileNum<files.size();fileNum++)//循环以给每个.idb文件进行压缩处理
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(files[fileNum].c_str()),&len);
        char readbuff[len];//文件内容
        char writebuff[len];//写入没有被压缩的数据
        long readbuff_pos=0;
        long writebuff_pos=0;

        if(DB_OpenAndRead(const_cast<char *>(files[fileNum].c_str()),readbuff))//将文件内容读取到readbuff
        {
            cout<<"未找到文件"<<endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }  
        for(int i=0;i<CurrentZipTemplate.schemas[i].first.size();i++)
        {
            if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::UDINT)//开关量的持续时长
            {
                
                uint32 standardBoolTime=converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                uint32 maxBoolTime=converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
                uint32 minBoolTime=converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

                //4个字节的持续时长,暂定，根据后续情况可能进行更改
                char value[4]={0};
                memcpy(value,readbuff+readbuff_pos,4);
                uint32 currentBoolTime=converter.ToUInt32(value);
                if(currentBoolTime!=standardBoolTime && (currentBoolTime<minBoolTime || currentBoolTime>maxBoolTime))
                {
                    //添加变量名方便知道未压缩的变量是哪个
                    memcpy(writebuff+writebuff_pos,const_cast<char *>(CurrentZipTemplate.schemas[i].first.c_str()),CurrentZipTemplate.schemas[i].first.length());
                    writebuff_pos+=CurrentZipTemplate.schemas[i].first.length();

                    memcpy(writebuff+writebuff_pos,readbuff+readbuff_pos,4);
                    writebuff_pos+=4;
                }
                readbuff_pos+=4;
            }
            else
            {
                cout<<"存在开关量以外的类型，请检查模板或者更换功能块"<<endl;
                return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            }
        }
        if(writebuff_pos>readbuff_pos)//表明数据没有被压缩,不做处理
        {
            cout<<files[fileNum]+"文件数据没有被压缩!"<<endl;
            //return 1;//1表示数据没有被压缩
        }
        else
        {
            DB_DeleteFile(const_cast<char *>(files[fileNum].c_str()));//删除原文件
            long fp;
            string finalpath=files[fileNum].append("zip");//给压缩文件后缀添加zip，暂定，根据后续要求更改
            //创建新文件并写入
            err = DB_Open(const_cast<char *>(finalpath.c_str()),"wb",&fp);
            if (err == 0)
            {
                if(writebuff_pos!=0)
                    err = DB_Write(fp, writebuff, writebuff_pos);
                
                if (err == 0)
                {
                    DB_Close(fp);
                }
            }
        }
    }
    return err;
}

/**
 * @brief 还原压缩后的开关量文件为原状态
 * 
 * @param ZipTemPath 压缩模板路径
 * @param pathToLine .idbzip压缩文件路径
 * @return  0:success,
 *          others:StatusCode 
 */
int DB_ReZipSwitchFile(const char *ZipTemPath,const char *pathToLine)
{
    int err=0;
    err=DB_LoadZipSchema(ZipTemPath);//加载压缩模板
    if(err)
    {
        cout<<"未加载模板"<<endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    DataTypeConverter converter;
    vector<string> files;

    readIDBZIPFilesList(pathToLine,files);
    if(files.size()==0)
    {
        cout<<"没有.idbzip文件!"<<endl;
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    for(size_t fileNum=0;fileNum<files.size();fileNum++)
    {
        long len;
        DB_GetFileLengthByPath(const_cast<char *>(files[fileNum].c_str()),&len);
        char readbuff[len];//文件内容
        char writebuff[CurrentZipTemplate.schemas.size()*4];//写入没有被压缩的数据
        long readbuff_pos=0;
        long writebuff_pos=0;

        if(DB_OpenAndRead(const_cast<char *>(files[fileNum].c_str()),readbuff))//将文件内容读取到readbuff
        {
            cout<<"未找到文件"<<endl;
            return StatusCode::DATAFILE_NOT_FOUND;
        }
        for(size_t i=0;i<CurrentZipTemplate.schemas.size();i++)
        {
            if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::UDINT)//开关量持续时长
            {
                if(len==0)//表示文件完全压缩
                {
                    uint32 standardBoolTime=converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                    char boolTime[4]={0};
                    for(int j=0;j<4;j++)
                    {
                        boolTime[3-j]|=standardBoolTime;
                        standardBoolTime>>=8;
                    }
                    memcpy(writebuff+writebuff_pos,boolTime,4);//持续时长
                    writebuff_pos+=4;
                }
                else//文件未完全压缩
                {
                    if(readbuff_pos<len)//还有未压缩的数据
                    {
                        char valueName[CurrentZipTemplate.schemas[i].first.length()];
                        memcpy(valueName,readbuff+readbuff_pos,CurrentZipTemplate.schemas[i].first.length());
                        if(valueName==CurrentZipTemplate.schemas[i].first)//是未压缩数据的变量名
                        {
                            readbuff_pos+=CurrentZipTemplate.schemas[i].first.length();
                            memcpy(writebuff+writebuff_pos,readbuff+readbuff_pos,4);
                            writebuff_pos+=4;
                            readbuff_pos+=4;
                        }
                    }
                    else//没有未压缩的数据了
                    {
                        uint32 standardBoolTime=converter.ToInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
                        char boolTime[4]={0};
                        for(int j=0;j<4;j++)
                        {
                            boolTime[3-i]|=standardBoolTime;
                            standardBoolTime>>=8;
                        }
                        memcpy(writebuff+writebuff_pos+1,boolTime,4);//持续时长
                        writebuff_pos+=4;
                    }

                }
            }
            else
            {
                cout<<"存在开关量以外的类型，请检查模板或者更换功能块"<<endl;
                return StatusCode::DATA_TYPE_MISMATCH_ERROR;
            }
        }

        //DB_DeleteFile(const_cast<char *>(files[fileNum].c_str()));//删除原文件
        long fp;
        string finalpath=files[fileNum].substr(0,files[fileNum].length()-3);//去掉后缀的zip
        //创建新文件并写入
        err = DB_Open(const_cast<char *>(finalpath.c_str()),"wb",&fp);
        if (err == 0)
        {
            err = DB_Write(fp, writebuff, writebuff_pos);
                    
            if (err == 0)
            {
                DB_Close(fp);
            }
        }
    }
    return err;
}

/**
 * @brief 压缩接收到只有开关量类型的整条数据
 * 
 * @param ZipTemPath 压缩模板路径
 * @param filepath 存储文件路径
 * @param buff 接收到的数据
 * @param buffLength 接收的数据长度
 * @return  0:success,
 *          others:StatusCode
 */
int DB_ZipRecvSwitchBuff(const char *ZipTemPath,const char *filepath,char *buff,long *buffLength)
{
    int err=0;
    err=DB_LoadZipSchema(ZipTemPath);//加载压缩模板
    if(err)
    {
        cout<<"未加载模板"<<endl;
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    long len=*buffLength;

    char writebuff[len];//写入没有被压缩的数据
    
    DataTypeConverter converter;
    long buff_pos=0;
    long writebuff_pos=0;

    for(int i=0;i<CurrentZipTemplate.schemas[i].first.size();i++)
    {
        if(CurrentZipTemplate.schemas[i].second.valueType==ValueType::BOOL)//BOOL变量
        {
            
            uint32 standardBoolTime=converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.standardValue);
            uint32 maxBoolTime=converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.maxValue);
            uint32 minBoolTime=converter.ToUInt32_m(CurrentZipTemplate.schemas[i].second.minValue);

            //1个字节的布尔值和4个字节的持续时长,暂定，根据后续情况可能进行更改
            buff_pos+=1;
            char value[4]={0};
            memcpy(value,buff+buff_pos,4);
            uint32 currentBoolTime=converter.ToUInt32(value);
            if(currentBoolTime!=standardBoolTime && (currentBoolTime<minBoolTime || currentBoolTime>maxBoolTime))
            {
                //添加变量名方便知道未压缩的变量是哪个,暂定方案，根据情况进行修改
                memcpy(writebuff+writebuff_pos,const_cast<char *>(CurrentZipTemplate.schemas[i].first.c_str()),CurrentZipTemplate.schemas[i].first.length());
                writebuff_pos+=CurrentZipTemplate.schemas[i].first.length();

                memcpy(writebuff+writebuff_pos,buff+buff_pos-1,5);
                writebuff_pos+=5;
            }
            buff_pos+=4;
        }
        else
        {
            cout<<"存在开关量以外的类型，请检查模板或者更换功能块"<<endl;
            return StatusCode::DATA_TYPE_MISMATCH_ERROR;
        }
    }

    if(writebuff_pos>buff_pos)//表明数据没有被压缩
    {
        cout<<"数据未压缩"<<endl;
        
        long fp;
        //创建新文件并写入
        err = DB_Open(const_cast<char *>(filepath),"wb",&fp);
        if (err == 0)
        {
            err = DB_Write(fp,const_cast<char *>(buff), *buffLength);
        
            if (err == 0)
            {
                DB_Close(fp);
            }
        }
        return 1;
    }

    long fp;
    string finalpath=filepath;
    finalpath=finalpath.append("zip");//给压缩文件后缀添加zip，暂定，根据后续要求更改
    //创建新文件并写入
    err = DB_Open(const_cast<char *>(finalpath.c_str()),"wb",&fp);
    if (err == 0)
    {
        err = DB_Write(fp, writebuff, writebuff_pos);
        
        if (err == 0)
        {
            return DB_Close(fp);
        }
    }
    return err;
}

int main()
{
    //EDVDB_LoadZipSchema("/");
    // long len;
    // DB_GetFileLengthByPath("XinFeng_0100.dat",&len);
    // char readbuf[len];
    // EDVDB_OpenAndRead("XinFeng_0100.dat",readbuf);
    //DB_ZipSwitchFile("/jinfei/","/jinfei/");
    DB_ReZipSwitchFile("/jinfei/","/jinfei");
    //EDVDB_ZipRecvBuff("/","XinFeng_0100.dat",readbuf,len);
    //EDVDB_ZipFile("/","/");
    return 0;
}