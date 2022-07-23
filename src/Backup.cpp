#include <BackupHelper.h>

/**
 * @brief 备份目录更换和数据迁移
 *
 * @param path 新的目录
 * @return int
 */
int BackupHelper::ChangeBackupPath(string path)
{
    if (!fs::exists(path))
        fs::create_directories(path);
    string cmd = "mv " + backupPath + "/* " + path + "/";
    int err = system(cmd.c_str());
    return err;
}

/**
 * @brief 检查数据目录下未被更新的数据，即最后写入时间在最近更新时间戳之后的文件，只检查pak文件
 *
 * @param files 未被更新的文件列表
 * @return int
 */
int BackupHelper::CheckDataToUpdate(vector<string> &files)
{
    try
    {
        error_code err;
        if (!fs::is_directory(dataPath, err))
            return err.value();
        for (auto const &dir_entry : fs::recursive_directory_iterator{dataPath, err})
        {
            if (fs::is_regular_file(dir_entry) && dir_entry.path().extension() == ".pak")
            {
                auto ftime = fs::last_write_time(dir_entry.path());
                if (decltype(ftime)::clock::to_time_t(ftime) >= lastBackupTime)
                {
                    files.push_back(dir_entry.path().string());
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    return 0;
}

/**
 * @brief 创建备份文件
 *
 * @return int
 * @note bak文件体格式为：
 *  1字节pak文件路径长度 L
 *  L字节pak文件路径
 *  8字节文件大小
 *  pak文件内容
 */
int BackupHelper::CreateBackup()
{
    long timestamp = getMilliTime();
    FILE *file;
    int err;
    if (!(file = fopen((backupPath + "/" + to_string(timestamp) + ".bak").c_str(), "wb")))
        return errno;
    if ((err = WriteBakHead(file, timestamp, 0, backupPath)) != 0)
        return err;
    vector<string> pakFiles;
    // for test
    readFiles(pakFiles, "JinfeiSixteen", ".pak", true);
    char *writeBuffer = new char[1024 * 1024 * 4]; // 20MB
    char *readBuffer = new char[1024 * 1024 * 4];
    setvbuf(file, writeBuffer, _IOFBF, 1024 * 1024 * 4); //设置写缓冲
    for (auto &pak : pakFiles)
    {
        FILE *pakfile;
        if ((err = DB_Open(const_cast<char *>(pak.c_str()), const_cast<char *>("rb"), (long *)pakfile)) != 0)
        {
            delete[] writeBuffer;
            fclose(file);
            return err;
        }
        long paklen;
        DB_GetFileLengthByFilePtr((long)pakfile, &paklen);

        fwrite(pak.c_str(), pak.length(), 1, file);
        fwrite(&paklen, 8, 1, file);
        int readlen = 0;
        while ((readlen = fread(readBuffer, 1, 1024 * 1024 * 4, pakfile)) > 0)
        {
            fwrite(readBuffer, readlen, 1, file);
        }
    }
    fwrite("checkpoint", 10, 1, file); //检查点
    fclose(file);
    delete[] writeBuffer;
    return 0;
}

/**
 * @brief 更新备份
 *
 * @return int
 */
int BackupHelper::BackupUpdate()
{
    vector<string> filesToUpdate, bakFiles;
    int err;
    if ((err = CheckDataToUpdate(filesToUpdate)) != 0)
        return err;
    readFiles(bakFiles, backupPath, ".bak");
    /* 若不存在bak文件，新建一个 */
    if (bakFiles.size() == 0)
    {
        return CreateBackup();
    }
    /* 选择最新的bak文件更新 */
    string latestBak;
    long maxTimestamp = 0;
    for (auto &bak : bakFiles)
    {
        auto ftime = fs::last_write_time(bak);
        if (decltype(ftime)::clock::to_time_t(ftime) > maxTimestamp)
        {
            latestBak = bak;
            maxTimestamp = decltype(ftime)::clock::to_time_t(ftime);
        }
    }

    FILE *file;
    if (!(file = fopen(latestBak.c_str(), "a+b")))
        return errno;
    long timestamp, filenum;
    string path;
    fseek(file, 0, SEEK_SET);
    if (ReadBakHead(file, timestamp, filenum, path) == -1)
        return StatusCode::UNKNWON_DATAFILE;
}

int main()
{
    BackupHelper helper;
    helper.CreateBackup();
    return 0;
}