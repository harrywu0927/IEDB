#include "utils.hpp"
#include <string>

using namespace std;
class BackupHelper
{
private:
    string backupPath;

public:
    BackupHelper() {}
    ~BackupHelper() {}
    int ChangeBackupPath(string path);
    int BackupUpdate();
    int DataRecovery();
    int BakRestoration();
};