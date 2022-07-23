#include "utils.hpp"
#include <string>

using namespace std;

/**
 * @brief bak文件头定义：
 * 按照字节序依次为：
 *  8字节最近更新的时间戳
 *  8字节文件总数
 *  2字节数据文件夹绝对路径长度 L
 *  L字节路径
 *
 */
class BackupHelper
{
private:
    string backupPath;
    string dataPath;
    long lastBackupTime;

public:
    BackupHelper() {
        backupPath = settings("Backup_Path");
        dataPath = settings("Filename_Label");
        vector<string> bakfiles;
        readFiles(bakfiles, backupPath, ".bak");
        lastBackupTime = 0;
    }
    ~BackupHelper() {}
    int ChangeBackupPath(string path);
    int BackupUpdate();
    int CreateBackup();
    int CheckDataToUpdate(vector<string> &files);
    int DataRecovery();
    int BakRestoration();
    int ReadBakHead(FILE *file, long &timestamp, long &fileNum, string &path)
    {
        char h[18];
        fread(h, 18, 1, file);
        memcpy(&timestamp, h, 8);
        memcpy(&fileNum, h + 8, 8);
        ushort L = __builtin_bswap16(*((ushort *)(h + 16)));
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