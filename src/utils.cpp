#include "utils.hpp"
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