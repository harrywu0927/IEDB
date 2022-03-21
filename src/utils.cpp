#include <utils.hpp>
//获取某一目录下的所有文件
//不递归子文件夹
void readFileList(string path, vector<string> &files){
    struct dirent *ptr;
    DIR *dir;
    dir=opendir(path.c_str());
    while((ptr=readdir(dir))!=NULL)
    {
        if(ptr->d_name[0] == '.')
            continue;

        if(ptr->d_type == 8)
        {
            string p;
            files.push_back(p.append(path).append("/").append(ptr->d_name));
        }
    }
    closedir(dir);
}

void readIDBFilesList(string path, vector<string> &files){
    struct dirent *ptr;
    DIR *dir;
    dir=opendir(path.c_str());
    while((ptr=readdir(dir))!=NULL)
    {
        if(ptr->d_name[0] == '.')
            continue;

        if(ptr->d_type == 8)
        {
            string p;
            string datafile = ptr->d_name;
            if(datafile.find(".idb")!=string::npos)
                files.push_back(p.append(path).append("/").append(ptr->d_name));
        }
    }
    closedir(dir);
}

//获取绝对时间(自1970/1/1至今)
//精确到毫秒
long getMilliTime()
{
    struct timeval time;
    gettimeofday(&time,NULL);
    return time.tv_sec*1000 + time.tv_usec/1000;
}


long Template::FindDatatypePos(char pathCode[])
{
    // for (size_t i = 0; i < 10; i++)
    // {
    //     cout<<(int)pathCode[i];
    // }
    
    long pos = 0;
    for (size_t i = 0; i < CurrentTemplate.schemas.size(); i++)
    {
        bool codeEquals = true;
        for (size_t k = 0; k < 10; k++) //判断路径编码是否相等
        {
            if (pathCode[k] != CurrentTemplate.schemas[i].first.code[k])
                codeEquals = false;
        }
        // if (codeEquals)
        // {
        //     // free(pathCode);
        //     int num = 1;
        //     if (CurrentTemplate.schemas[i].second.isArray)
        //     {
        //         /*
        //             图片数据中前2个字节为长度，长度不包括这2个字节
        //             格式改变时，此处需要更改，下面else同理
        //             请注意！
        //         */

        //         if (CurrentTemplate.schemas[i].second.valueType == ValueType::IMAGE)
        //         {

        //             char imgLen[2];
        //             imgLen[0] = buff[pos];
        //             imgLen[1] = buff[pos + 1];
        //             num = (int)converter.ToUInt16(imgLen) + 2;
        //         }
        //         else
        //             num = CurrentTemplate.schemas[i].second.arrayLen;
        //     }
        //     long length = (long)(num * CurrentTemplate.schemas[i].second.valueBytes);
        //     return 0;
        // }
        // else
        // {
        //     int num = 1;
        //     if (CurrentTemplate.schemas[i].second.isArray)
        //     {
        //         if (CurrentTemplate.schemas[i].second.valueType == ValueType::IMAGE)
        //         {

        //             char imgLen[2];
        //             imgLen[0] = buff[pos];
        //             imgLen[1] = buff[pos + 1];
        //             num = (int)converter.ToUInt16(imgLen) + 2;
        //         }
        //         else
        //             num = CurrentTemplate.schemas[i].second.arrayLen;
        //     }
        //     pos += num * CurrentTemplate.schemas[i].second.valueBytes;
        // }
    }
}