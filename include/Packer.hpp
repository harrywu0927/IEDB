//负责数据文件的打包，打包后的数据将存为一个文件，文件名为文件ID+时间段.pak，
//格式为每8字节时间戳，接20字节文件ID，接8字节文件长度(0表示为压缩文件)，接文件内容；

class Packer
{
public:
    static int packMode;          //定时打包或存储一定数量后打包
    static int packNum;           //一次打包的文件数量
    static long packTimeInterval; //定时打包时间间隔
    static int Pack(string path)
    {
        return 0;
    }
};

class PackFileReader
{
private:
    long curPos;
    char *packBuffer;

public:
    PackFileReader() {}
    PackFileReader(const char *packFilePath)
    {
    }
    ~PackFileReader()
    {
        free(packBuffer);
    }
    int Read(char *buffer);
};
