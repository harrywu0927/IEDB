#include <BackupHelper.h>

BackupHelper::BackupHelper()
{
    backupPath = settings("Backup_Path");
    dataPath = settings("Filename_Label");
    if (!fs::exists(backupPath))
        fs::create_directories(backupPath);
    vector<string> bakfiles;
    readFiles(bakfiles, backupPath, ".bak");
    lastBackupTime = 0;
    /* Check the maximum of last write time in all bak files */
    for (auto &bak : bakfiles)
    {
        auto ftime = fs::last_write_time(bak);
        long timestamp = decltype(ftime)::clock::to_time_t(ftime);
        if (timestamp > lastBackupTime)
        {
            lastBackupTime = timestamp;
        }
    }
}

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
    backupPath = path;
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
 *  8字节pak文件大小
 *  2字节pak文件路径长度 L
 *  L字节pak文件路径
 *  pak文件内容
 */
int BackupHelper::CreateBackup()
{
    long timestamp = getMilliTime();
    FILE *file;
    int err;
    if (!(file = fopen((backupPath + "/" + to_string(timestamp) + ".bak").c_str(), "wb")))
    {
        perror("err");
        return errno;
    }

    vector<string> pakFiles;
    // for test
    readFiles(pakFiles, "testIEDB/JinfeiSixteen", ".pak", true);
    if ((err = WriteBakHead(file, timestamp, pakFiles.size(), backupPath)) != 0)
        return err;
    char *writeBuffer = new char[WRITE_BUFFER_SIZE]; // 20MB
    char *readBuffer = new char[READ_BUFFER_SIZE];
    setvbuf(file, writeBuffer, _IOFBF, WRITE_BUFFER_SIZE); //设置写缓冲
    for (auto &pak : pakFiles)
    {
        try
        {
            FILE *pakfile;
            if (!(pakfile = fopen(pak.c_str(), "rb")))
            {
                delete[] writeBuffer;
                delete[] readBuffer;
                return err;
            }
            long paklen;
            DB_GetFileLengthByFilePtr((long)pakfile, &paklen);
            unsigned short pakPathLen = pak.length();
            DB_Write((long)file, (char *)(&paklen), 8);
            DB_Write((long)file, (char *)(&pakPathLen), 2);
            DB_Write((long)file, (char *)(pak.c_str()), pakPathLen);

            // fwrite(&pakPathLen, 2, 1, file);
            // fwrite(pak.c_str(), pakPathLen, 1, file);
            // fwrite(&paklen, 8, 1, file);
            int readlen = 0;
            while ((readlen = fread(readBuffer, 1, READ_BUFFER_SIZE, pakfile)) > 0)
            {
                DB_Write((long)file, readBuffer, readlen);
            }
        }
        catch (int &e)
        {
            delete[] writeBuffer;
            delete[] readBuffer;
            fclose(file);
            return e;
        }
        catch (std::exception &e)
        {
            cerr << e.what() << endl;
            delete[] writeBuffer;
            delete[] readBuffer;
            fclose(file);
            return -1;
        }
    }
    fwrite("checkpoint", 10, 1, file); //检查点
    fclose(file);
    cout << "Backup " << to_string(timestamp) + ".bak completed in " << backupPath << ".\nCheckpoint at" << ctime(&timestamp) << "\n";
    delete[] writeBuffer;
    delete[] readBuffer;
    lastBackupTime = timestamp;
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

    readFiles(bakFiles, backupPath, ".bak");
    /* 若不存在bak文件，新建一个 */
    if (bakFiles.size() == 0)
    {
        cout << "Backup file not found. Creating new backup...\n";
        return CreateBackup();
    }
    int err;
    if ((err = CheckDataToUpdate(filesToUpdate)) != 0)
        return err;
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

    /* Check file */
    FILE *file;
    if (!(file = fopen(latestBak.c_str(), "a+b")))
        return errno;
    long timestamp, filenum;
    string path;

    fseek(file, -10, SEEK_END);
    long pos = ftell(file);
    char check[10];
    fread(check, 10, 1, file);
    fseek(file, 0, SEEK_SET);
    if (ReadBakHead(file, timestamp, filenum, path) == -1)
        return StatusCode::UNKNWON_DATAFILE;
    long nowTime = getMilliTime();
    if (nowTime < timestamp)
    {
        fclose(file);
        return StatusCode::UNKNWON_DATAFILE;
    }

    if (string(check) != "checkpoint")
    {
        cout << "Backup file broken, trying to rollback to previous checkpoint...\n";
        /* 从头遍历备份文件，检查写入时中断的部分数据，将其覆盖 */
        size_t filesize = fs::file_size(latestBak);
        size_t curPos = ftell(file);
        size_t lastPos = curPos;
        long scanedFile = 0;
        long pakLen;
        unsigned short pakPathLen;
        while (curPos < filesize - 10 && scanedFile < filenum)
        {
            lastPos = curPos;
            fread(&pakLen, 8, 1, file);
            fread(&pakPathLen, 2, 1, file);
            if (fseek(file, pakPathLen, SEEK_CUR) != 0 || fseek(file, pakLen, SEEK_CUR) != 0)
            {
                /* File pointer exceeded the end of file */
                fclose(file);
                cout << "Failed to rollback, the bak file may be maliciously modified.\n";
                throw StatusCode::UNKNWON_DATAFILE;
                return StatusCode::UNKNWON_DATAFILE;
            }
            curPos = ftell(file);
            scanedFile++;
        }
        cout << "Rollback completed, " << filenum - scanedFile << "file(s) lost.\n";

        pos = lastPos;
        filenum = scanedFile;
    }

    /* Start backup */
    fseek(file, pos, SEEK_SET);
    char *writeBuffer = new char[WRITE_BUFFER_SIZE]; // 20MB
    char *readBuffer = new char[READ_BUFFER_SIZE];
    setvbuf(file, writeBuffer, _IOFBF, WRITE_BUFFER_SIZE); //设置写缓冲
    for (auto &pak : filesToUpdate)
    {
        try
        {
            FILE *pakfile;
            if (!(pakfile = fopen(pak.c_str(), "rb")))
            {
                delete[] writeBuffer;
                delete[] readBuffer;
                return err;
            }
            long paklen;
            DB_GetFileLengthByFilePtr((long)pakfile, &paklen);
            unsigned short pakPathLen = pak.length();
            // fwrite(&pakPathLen, 2, 1, file);
            // fwrite(pak.c_str(), pakPathLen, 1, file);
            // fwrite(&paklen, 8, 1, file);
            DB_Write((long)file, (char *)(&paklen), 8);
            DB_Write((long)file, (char *)(&pakPathLen), 2);
            DB_Write((long)file, (char *)(pak.c_str()), pakPathLen);

            int readlen = 0;
            while ((readlen = fread(readBuffer, 1, READ_BUFFER_SIZE, pakfile)) > 0)
            {
                DB_Write((long)file, readBuffer, readlen);
            }
        }
        catch (int &e)
        {
            delete[] writeBuffer;
            delete[] readBuffer;
            fclose(file);
            throw e;
            return e;
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            delete[] writeBuffer;
            delete[] readBuffer;
            fclose(file);
            throw e;
            return -1;
        }
    }
    fwrite("checkpoint", 10, 1, file); //检查点

    /* Update bak head */
    fflush(file);
    fseek(file, 0, SEEK_SET);
    long newFileNum = filenum + filesToUpdate.size();
    err = WriteBakHead(file, nowTime, newFileNum, path);
    fclose(file);
    cout << "Backup " << to_string(timestamp) + ".bak completed in " << backupPath << ".\nCheckpoint at" << ctime(&timestamp) << "\n";
    delete[] writeBuffer;
    delete[] readBuffer;
    lastBackupTime = timestamp;
    return err;
}

// int main()
// {
//     BackupHelper helper;
//     helper.CreateBackup();
//     return 0;
// }