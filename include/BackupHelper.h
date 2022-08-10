#ifndef _BACKUP_HELPER_H
#define _BACKUP_HELPER_H
#endif

#include "utils.hpp"
#include <string>
#include <CassFactoryDB.h>

using namespace std;
#define WRITE_BUFFER_SIZE 1 << 25 // 32MB
#define READ_BUFFER_SIZE 1 << 25  // 32MB
/**
 * @brief bak文件头定义：
 * 按照字节序依次为：
 *  8字节最近更新的时间戳
 *  8字节文件总数
 *  2字节数据文件夹绝对路径长度 P
 *  P字节路径
 *
 * bak文件体格式为：
 *  8字节pak文件大小
 *  2字节pak文件路径长度 P
 *  P字节pak文件路径
 *  5字节LZMA参数
 *  8字节压缩后大小 C
 *  C字节压缩后文件内容
 *  20字节压缩后pak的SHA1哈希值
 */
class BackupHelper
{
private:
    string backupPath;
    string dataPath;
    unordered_map<string, time_t> lastBackupTime;

public:
    // shared_ptr<spdlog::logger> logger;
    Logger logger;
    BackupHelper();
    ~BackupHelper() {}
    int ChangeBackupPath(string path);
    int BackupUpdate();
    int CreateBackup(string path);
    int CheckDataToUpdate(unordered_map<string, vector<string>> &files);
    int DataRecovery(string path);
    int RecoverAll(string path);
    int BakRestoration(FILE *file, size_t &pos, size_t bodyPos = 0) noexcept;
    void ComputeSHA1(size_t size, FILE *file, Byte *sha1);
    void CheckBackup(FILE *file, size_t filesize, long filenum);
    int ReadBakHead(FILE *file, long &timestamp, long &fileNum, string &path)
    {
        char h[18];
        fread(h, 18, 1, file);
        memcpy(&timestamp, h, 8);
        memcpy(&fileNum, h + 8, 8);
        ushort L = *((ushort *)(h + 16));
        if (L > 1000) //文件内容可能被篡改
            return -1;
        char p[L];
        fread(p, L, 1, file);
        path = p;
        return 0;
    }
    int WriteBakHead(FILE *file, long timestamp, long fileNum, string path)
    {
        ushort L = path.length();
        char x[18 + L];
        memcpy(x, &timestamp, 8);
        memcpy(x + 8, &fileNum, 8);
        memcpy(x + 16, &L, 2);
        memcpy(x + 18, path.c_str(), L);
        return fwrite(x, 18 + L, 1, file) == EOF ? errno : 0;
    }
};

extern BackupHelper backupHelper;