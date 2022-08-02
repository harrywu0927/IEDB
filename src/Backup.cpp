#include <BackupHelper.h>

BackupHelper::BackupHelper()
{
    try
    {
        backupPath = settings("Backup_Path");
        dataPath = settings("Filename_Label");
        if (!fs::exists(backupPath))
            fs::create_directories(backupPath);
        vector<string> dirs;
        readAllDirs(dirs, backupPath);
        for (auto &dir : dirs)
        {
            vector<string> bakfiles;
            readFiles(bakfiles, dir, ".bak", true);
            lastBackupTime[dir] = 0;
            /* Check the maximum of last write time in all bak files */
            for (auto &bak : bakfiles)
            {
                auto ftime = fs::last_write_time(bak);
                time_t timestamp = decltype(ftime)::clock::to_time_t(ftime);
                if (timestamp > lastBackupTime[dir])
                {
                    lastBackupTime[dir] = timestamp;
                }
            }
        }
        logger = spdlog::get("backuplog.log");
        if (logger == nullptr)
            logger = spdlog::rotating_logger_mt("backup_logger", "backuplog.log", 1024 * 1024 * 5, 1);
        // logger->info("Backup initilization succeed");
        spdlog::info("Backup initilization succeed");
    }
    catch (fs::filesystem_error &e)
    {
        spdlog::critical("Backup initilization filed: {}", e.what());
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

/**
 * @brief 将待更新的包写入备份文件中，调用方有义务关闭文件
 *
 * @param paks 包列表
 * @param file 文件指针
 * @return int
 */
int WritePacks(vector<string> &paks, FILE *file)
{
    int err = 0;
    char *writeBuffer = new char[WRITE_BUFFER_SIZE];
    char *readBuffer = new char[READ_BUFFER_SIZE];
    setvbuf(file, writeBuffer, _IOFBF, WRITE_BUFFER_SIZE); //设置写缓冲
    for (auto &pak : paks)
    {
        FILE *pakfile;
        try
        {
            if (!(pakfile = fopen(pak.c_str(), "rb")))
            {
                return err;
            }
            long paklen;
            DB_GetFileLengthByFilePtr((long)pakfile, &paklen);
            unsigned short pakPathLen = pak.length();
            // DB_Write((long)file, (char *)(&paklen), 8);
            // DB_Write((long)file, (char *)(&pakPathLen), 2);
            // DB_Write((long)file, (char *)(pak.c_str()), pakPathLen);

            fwrite(&paklen, 8, 1, file);
            fwrite(&pakPathLen, 2, 1, file);
            fwrite(pak.c_str(), pakPathLen, 1, file);

            /* Compress the content of pack using LZMA */
            Byte *pakBuffer = nullptr;
            pakBuffer = new Byte[paklen];
            int readlen = 0;
            long pakpos = 0;
            while ((readlen = fread(readBuffer, 1, READ_BUFFER_SIZE, pakfile)) > 0)
            {
                memcpy(pakBuffer + pakpos, readBuffer, readlen);
                pakpos += readlen;
            }
            Byte *compressedBuffer = nullptr;
            compressedBuffer = new Byte[paklen];
            size_t compressedLen = paklen, outpropsSize = LZMA_PROPS_SIZE;
            Byte outprops[5];
            if ((err = LzmaCompress(compressedBuffer, &compressedLen, pakBuffer, paklen, outprops, &outpropsSize, 5, 1 << 24, 3, 0, 2, 3, 2)) != 0)
            {
                throw err;
            }
            /* Write the props parameters which is needed when uncompressing */
            fwrite(outprops, LZMA_PROPS_SIZE, 1, file);

            fwrite(&compressedLen, sizeof(compressedLen), 1, file);
            fwrite(compressedBuffer, compressedLen, 1, file);
        }
        catch (bad_alloc &e)
        {
            fclose(pakfile);
            delete[] writeBuffer;
            delete[] readBuffer;
            backupHelper.logger->critical("Memory allocation failed when compressing data!");
            spdlog::critical("Memory allocation failed when compressing data!");
            throw e;
        }
        catch (int &e)
        {
            fclose(pakfile);
            delete[] writeBuffer;
            delete[] readBuffer;
            backupHelper.logger->error("Failed to uncompress data, error code {}", e);
            spdlog::error("Error occured when uncompressing: code{}", e);
            throw e;
        }
        catch (std::exception &e)
        {
            fclose(pakfile);
            cerr << e.what() << endl;
            delete[] writeBuffer;
            delete[] readBuffer;
            throw e;
        }
        fclose(pakfile);
    }
    fwrite("checkpoint", 10, 1, file); //检查点

    delete[] writeBuffer;
    delete[] readBuffer;
    return err;
}

/**
 * @brief 备份目录更换和数据迁移
 *
 * @param path 新的目录
 * @return int
 */
int BackupHelper::ChangeBackupPath(string path)
{
    try
    {
        if (!fs::exists(path))
            fs::create_directories(path);
    }
    catch (fs::filesystem_error &e)
    {
        std::cerr << e.what() << '\n';
        return -1;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return -1;
    }

    string cmd = "mv " + backupPath + "/* " + path + "/";
    logger->info("Executing command: {}", cmd);
    spdlog::info("Executing command: {}", cmd);
    int err = system(cmd.c_str());
    if (err == 0)
    {
        spdlog::info("Backup path changed from {} to {}", backupPath, path);
        logger->info("Backup path changed from {} to {}", backupPath, path);
        backupPath = path;
    }
    else
        logger->error("Faile to change backup path to {}, error code : {}", path, err);

    return err;
}

/**
 * @brief 检查数据目录下未被更新的数据，即最后写入时间在最近更新时间戳之后的文件，只检查pak文件
 *
 * @param files 未被更新的 <文件夹,文件列表> 哈希表
 * @return int
 */
int BackupHelper::CheckDataToUpdate(unordered_map<string, vector<string>> &files)
{
    try
    {
        error_code err;
        if (!fs::is_directory(dataPath, err))
            return err.value();
        vector<string> dataDirs;
        readAllDirs(dataDirs, dataPath);
        for (auto const &dataDir : dataDirs)
        {
            string bakDir = backupPath;
            string tmp = dataDir;
            removeFilenameLabel(tmp);
            bakDir += "/" + tmp;
            for (auto const &dir_entry : fs::directory_iterator{dataDir, err})
            {
                if (fs::is_regular_file(dir_entry) && dir_entry.path().extension() == ".pak")
                {
                    auto ftime = fs::last_write_time(dir_entry.path());
                    if (decltype(ftime)::clock::to_time_t(ftime) >= lastBackupTime[bakDir])
                    {
                        files[dataDir].push_back(dir_entry.path().string());
                    }
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        spdlog::error("Error when checking data to update: {}", e.what());
        logger->error("Error when checking data to update: {}", e.what());
    }
    return 0;
}

/**
 * @brief 创建备份文件
 *
 * @param path 待创建备份的数据路径
 * @return int
 * @note
 */
int BackupHelper::CreateBackup(string path)
{
    time_t timestamp = time(0);
    FILE *file;
    int err;

    vector<string> pakFiles;
    readFiles(pakFiles, path, ".pak");
    string tmp = path;
    removeFilenameLabel(tmp);
    if (!fs::exists(backupPath + "/" + tmp))
        fs::create_directories(backupPath + "/" + tmp);
    if (!(file = fopen((backupPath + "/" + tmp + "/" + to_string(timestamp) + ".bak").c_str(), "wb")))
    {
        perror("err");
        return errno;
    }
    if ((err = WriteBakHead(file, timestamp, pakFiles.size(), backupPath)) != 0)
        return err;
    try
    {
        WritePacks(pakFiles, file);
        // cout << "Backup " << to_string(timestamp) + ".bak completed in " << backupPath + "/" + tmp << ".\nCheckpoint at " << ctime(&timestamp) << "\n";
        spdlog::info("Backup {}.bak completed in {}/{}.\nCheckpoint at {}", timestamp, backupPath, tmp, ctime(&timestamp));
        logger->info("Backup {}.bak completed in {}/{}.\nCheckpoint at {}", timestamp, backupPath, tmp, ctime(&timestamp));
        lastBackupTime[path] = timestamp;
    }
    catch (int &e)
    {
        fclose(file);
        throw e;
        return e;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    fclose(file);

    return 0;
}

/**
 * @brief 更新备份
 *
 * @return int
 */
int BackupHelper::BackupUpdate()
{
    unordered_map<string, vector<string>> filesToUpdate;

    int err;
    if ((err = CheckDataToUpdate(filesToUpdate)) != 0)
        return err;
    for (auto &line : filesToUpdate)
    {
        vector<string> bakFiles;
        readFiles(bakFiles, line.first, ".bak");
        /* 若不存在bak文件，新建一个 */
        if (bakFiles.size() == 0)
        {
            // cout << "Backup file not found. Creating new backup...\n";
            spdlog::info("Backup file not found in {}. Create a new backup file", line.first);
            logger->info("Backup file not found in {}. Create a new backup file", line.first);
            CreateBackup(line.first);
            continue;
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
        /* When backup file size is greater than 4GB, create a new backup */
        if (fs::file_size(latestBak) > ((size_t)1 << 32))
        {

            time_t t = time(0);
            spdlog::info("Backup {} is larger than 4GB, changing to {}.bak", latestBak, t);
            logger->info("Backup {} is larger than 4GB, changing to {}.bak", latestBak, t);
            latestBak = fs::path(latestBak).parent_path().string() + "/" + to_string(t) + ".bak";
            FILE *file = fopen(latestBak.c_str(), "wb");
            if ((err = WriteBakHead(file, t, line.second.size(), backupPath)) != 0)
                return err;
            try
            {
                WritePacks(line.second, file);
                // cout << "Backup " << to_string(t) + ".bak completed in " << latestBak << ".\nCheckpoint at " << ctime(&t) << "\n";
                spdlog::info("Backup {}.bak completed in {}. Checkpoint at {}", t, latestBak, ctime(&t));
                logger->info("Backup {}.bak completed in {}. Checkpoint at {}", t, latestBak, ctime(&t));
                lastBackupTime[line.first] = t;
            }
            catch (int &e)
            {
                fclose(file);
                throw e;
            }
            catch (const std::exception &e)
            {
                std::cerr << e.what() << '\n';
            }
            fclose(file);
            return 0;
        }
        /* Check file */
        FILE *file;
        if (!(file = fopen(latestBak.c_str(), "r+b")))
            return errno;
        long filenum;
        time_t timestamp;
        string path;

        fseek(file, -10, SEEK_END);
        long pos = ftell(file);
        char check[11] = {0};
        fread(check, 10, 1, file);
        fseek(file, 0, SEEK_SET);
        if (ReadBakHead(file, timestamp, filenum, path) == -1)
            return StatusCode::UNKNWON_DATAFILE;
        time_t nowTime = time(0);
        if (nowTime < timestamp)
        {
            fclose(file);
            spdlog::critical("Backup file's timestamp exceeded current time, it may have broken:{}", latestBak);
            logger->critical("Backup file's timestamp exceeds current time, it may have broken:{}", latestBak);
            return StatusCode::UNKNWON_DATAFILE;
        }
        if (string(check) != "checkpoint")
        {
            // cout << "Backup file broken, trying to rollback to previous checkpoint...\n";
            spdlog::warn("Backup file {} broken, trying to rollback to previous checkpoint...", latestBak);
            logger->warn("Backup file {} broken, trying to rollback to previous checkpoint...", latestBak);
            /* 从头遍历备份文件，检查写入时中断的部分数据，将其覆盖 */
            size_t filesize = fs::file_size(latestBak);
            size_t curPos = ftell(file);
            size_t lastPos = curPos;
            long scanedFile = 0;
            long pakLen;
            unsigned short pakPathLen;
            size_t compressedSize;
            while (curPos < filesize - 23 && scanedFile < filenum)
            {
                lastPos = curPos;
                fread(&pakLen, 8, 1, file);
                fread(&pakPathLen, 2, 1, file);
                fseek(file, pakPathLen + 5, SEEK_CUR);
                fread(&compressedSize, 8, 1, file);

                if (fseek(file, compressedSize, SEEK_CUR) != 0)
                {
                    /* File pointer exceeded the end of file */
                    break;
                    // fclose(file);
                    // cout << "Failed to rollback, the bak file may be maliciously modified.\n";
                    // throw StatusCode::UNKNWON_DATAFILE;
                    // return StatusCode::UNKNWON_DATAFILE;
                }
                curPos = ftell(file);
                scanedFile++;
            }
            // cout << "Rollback completed, " << filenum - scanedFile << " file(s) lost.\n";
            spdlog::warn("Rollback completed, {} file(s) lost.", filenum - scanedFile);
            logger->warn("Rollback completed, {} file(s) lost.", filenum - scanedFile);
            pos = lastPos;
            filenum = scanedFile;
        }

        /* Start backup */
        fseek(file, pos, SEEK_SET);
        try
        {
            WritePacks(line.second, file);
            /* Update bak head */
            fflush(file);
            fseek(file, 0, SEEK_SET);
            long newFileNum = filenum + line.second.size();
            err = WriteBakHead(file, nowTime, newFileNum, path);
            fclose(file);
            // cout << "Backup " << to_string(nowTime) + ".bak update completed in " << line.first << ".\nCheckpoint at " << ctime(&nowTime) << "\n";
            spdlog::info("Backup {}.bak update completed in {}. Checkpoint at {}", nowTime, line.first, ctime(&nowTime));
            logger->info("Backup {}.bak update completed in {}. Checkpoint at {}", nowTime, line.first, ctime(&nowTime));
            lastBackupTime[line.first] = nowTime;
        }
        catch (int &e)
        {
            fclose(file);
            throw e;
            return e;
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            fclose(file);
        }
    }

    return err;
}

int DB_Backup(const char *path)
{
    return backupHelper.CreateBackup(path);
}

// int main()
// {
//     BackupHelper helper;
//     helper.BackupUpdate();
//     return 0;
// }