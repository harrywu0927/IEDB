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
        logger = Logger("backuplog");
// logger = spdlog::get("backuplog.log");
// if (logger == nullptr)
//     logger = spdlog::rotating_logger_mt("backup_logger", "backuplog.log", 1024 * 1024 * 5, 1);
// logger.info("Backup initilization succeed");
#ifdef DEBUG
        spdlog::info("Backup initilization succeed");
#endif
    }
    catch (fs::filesystem_error &e)
    {
        // spdlog::critical("Backup initilization filed: {}", e.what());
        logger.critical("Backup initilization filed: {}. Check your path or permission.", e.what());
    }
    catch (const std::exception &e)
    {
        // std::cerr << e.what() << '\n';
        logger.critical("Backup initilization filed: {}", e.what());
    }
}

/**
 * @brief 流式计算文件内容的sha1值，抛出iedberr异常
 *
 * @param size 长度
 * @param file 文件指针
 * @param sha1 给定sha1值 20字节
 */
void BackupHelper::ComputeSHA1(size_t size, FILE *file, Byte *sha1)
{
    /* Compute the sha1 value of a compressed pack in stream-style */
    int streamSize = 1024 * 1024 * 4;
    Byte *compressedBuffer = new Byte[streamSize];
    SHA_CTX c;
    SHA1_Init(&c);
    int sum = 0;
    while (sum + streamSize < size)
    {
        if (std::fread(compressedBuffer, 1, streamSize, file) < streamSize)
            throw iedb_err(StatusCode::DATAFILE_MODIFIED);
        SHA1_Update(&c, compressedBuffer, streamSize);
        sum += streamSize;
    }
    if (sum < size && std::fread(compressedBuffer, 1, size - sum, file) == size - sum)
        SHA1_Update(&c, compressedBuffer, size - sum);
    SHA1_Final(sha1, &c);
}

/**
 * @brief 校验文件内容，并将文件指针指向应开始写入的位置
 *
 * @param file
 * @param filesize bak文件大小
 * @param filenum 文件个数
 *
 * @note 当遇到校验失败的数据时，目前直接丢弃其后数据
 */
void BackupHelper::CheckBackup(FILE *file, size_t filesize, long filenum)
{
    size_t curPos = ftell(file);
    size_t lastPos = curPos;
    long scanedFile = 0;
    long pakLen;
    unsigned short pakPathLen;
    size_t compressedSize;
    Byte *compressedBuffer = nullptr;
    try
    {
        while (curPos < filesize - 23 && scanedFile < filenum)
        {
            lastPos = curPos;
            std::fread(&pakLen, 8, 1, file);
            std::fread(&pakPathLen, 2, 1, file);
            std::fseek(file, pakPathLen + 5, SEEK_CUR);
            std::fread(&compressedSize, 8, 1, file);

            if (std::ftell(file) + compressedSize > filesize)
            {
                /* File pointer exceeds the end of file */
                throw iedb_err(StatusCode::DATAFILE_MODIFIED);
            }
            Byte sha1[20];
            ComputeSHA1(compressedSize, file, sha1);
            /* Compare the computed sha1 value and the stored value */
            Byte sha1_backup[20];
            std::fread(sha1_backup, 1, 20, file);
            if (memcmp(sha1, sha1_backup, 20) != 0)
            {
                throw iedb_err(StatusCode::DATAFILE_MODIFIED);
            }

            curPos = ftell(file);
            scanedFile++;
        }
    }
    catch (bad_alloc &e)
    {
        logger.critical("Bad allocation occured. Stopping roll back");
        throw e;
    }
    catch (iedb_err &e)
    {
        logger.error("Datafile modified detected");
        throw e;
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
 * @note 目前尚未找到LZMA增量压缩的方法，因此暂时将整个包都放入内存中压缩和计算校验码
 */
int WritePacks(vector<string> &paks, FILE *file)
{
    int err = 0;
    char *writeBuffer = new char[WRITE_BUFFER_SIZE];
    char *readBuffer = new char[READ_BUFFER_SIZE];
    std::setvbuf(file, writeBuffer, _IOFBF, WRITE_BUFFER_SIZE); //设置写缓冲
    for (auto &pak : paks)
    {
        FILE *pakfile;
        Byte *compressedBuffer = nullptr;
        Byte *pakBuffer = nullptr;
        try
        {
            if (!(pakfile = fopen(pak.c_str(), "rb")))
            {
                throw iedb_err(errno);
            }
            long paklen;
            DB_GetFileLengthByFilePtr((long)pakfile, &paklen);
            unsigned short pakPathLen = pak.length();
            // DB_Write((long)file, (char *)(&paklen), 8);
            // DB_Write((long)file, (char *)(&pakPathLen), 2);
            // DB_Write((long)file, (char *)(pak.c_str()), pakPathLen);

            std::fwrite(&paklen, 8, 1, file);
            std::fwrite(&pakPathLen, 2, 1, file);
            std::fwrite(pak.c_str(), pakPathLen, 1, file);

            /* Compress the content of pack using LZMA */
            pakBuffer = new Byte[paklen];
            int readlen = 0;
            long pakpos = 0;
            while ((readlen = std::fread(readBuffer, 1, READ_BUFFER_SIZE, pakfile)) > 0)
            {
                memcpy(pakBuffer + pakpos, readBuffer, readlen);
                pakpos += readlen;
            }
            compressedBuffer = new Byte[paklen];
            size_t compressedLen = paklen, outpropsSize = LZMA_PROPS_SIZE;
            Byte outprops[5];
            if ((err = LzmaCompress(compressedBuffer, &compressedLen, pakBuffer, paklen, outprops, &outpropsSize, 5, 1 << 24, 3, 0, 2, 3, 2)) != 0)
            {
                throw iedb_err(err);
            }
            delete[] pakBuffer;
            /* Write the props parameters which is needed when uncompressing */
            fwrite(outprops, LZMA_PROPS_SIZE, 1, file);

            fwrite(&compressedLen, sizeof(compressedLen), 1, file);
            fwrite(compressedBuffer, compressedLen, 1, file);

            /* Write the SHA1 hashed value */
            Byte md[20] = {0};
            SHA1(compressedBuffer, compressedLen, md);
            fwrite(md, 20, 1, file);
            delete[] compressedBuffer;
        }
        catch (bad_alloc &e)
        {
            fclose(pakfile);
            delete[] writeBuffer;
            delete[] readBuffer;
            if (pakBuffer != nullptr)
                delete[] pakBuffer;
            if (compressedBuffer != nullptr)
                delete[] compressedBuffer;
            backupHelper.logger.critical("Memory allocation failed when compressing data!");
            throw e;
        }
        catch (iedb_err &e)
        {
            fclose(pakfile);
            delete[] writeBuffer;
            delete[] readBuffer;
            delete[] pakBuffer;
            backupHelper.logger.error("Failed to uncompress data, error code {}", e.code);
            throw e;
        }
        catch (std::exception &e)
        {
            fclose(pakfile);
            delete[] writeBuffer;
            delete[] readBuffer;
            if (pakBuffer != nullptr)
                delete[] pakBuffer;
            if (compressedBuffer != nullptr)
                delete[] compressedBuffer;
            throw e;
        }
        fclose(pakfile);
    }
    // fwrite("checkpoint", 10, 1, file); //检查点

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
    logger.info("Executing command: {}", cmd);
    int err = system(cmd.c_str());
    if (err == 0)
    {
        logger.info("Backup path changed from {} to {}", backupPath, path);
        backupPath = path;
    }
    else
        logger.error("Faile to change backup path to {}, error code : {}", path, err);

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
    catch (fs::filesystem_error &e)
    {
        logger.error("Error:{}", e.what());
    }
    catch (const std::exception &e)
    {
        // std::cerr << e.what() << '\n';
        logger.error("Error when checking data to update: {}", e.what());
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
        logger.error(strerror(errno));
        return errno;
    }

    try
    {
        if ((err = WriteBakHead(file, timestamp, pakFiles.size(), backupPath)) != 0)
            throw iedb_err(err);
        WritePacks(pakFiles, file);
        logger.info("Backup {}.bak completed in {}/{}.\nCheckpoint at {}", timestamp, backupPath, tmp, ctime(&timestamp));
        lastBackupTime[path] = timestamp;
    }
    catch (iedb_err &e)
    {
        fclose(file);
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
    logger.info("Backup preparation completed. Now start backup...");
    for (auto &line : filesToUpdate)
    {
        vector<string> bakFiles;
        readFiles(bakFiles, line.first, ".bak");
        /* 若不存在bak文件，新建一个 */
        if (bakFiles.size() == 0)
        {
            logger.info("Backup file not found in {}. Create a new backup file", line.first);
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
            logger.info("Backup {} is larger than 4GB, changing to {}.bak", latestBak, t);
            latestBak = fs::path(latestBak).parent_path().string() + "/" + to_string(t) + ".bak";
            FILE *file = fopen(latestBak.c_str(), "wb");
            if ((err = WriteBakHead(file, t, line.second.size(), backupPath)) != 0)
                return err;
            try
            {
                WritePacks(line.second, file);
                logger.info("Backup {}.bak completed in {}. Checkpoint at {}", t, latestBak, ctime(&t));
                lastBackupTime[line.first] = t;
            }
            catch (iedb_err &e)
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

        std::fseek(file, -10, SEEK_END);
        long pos = ftell(file);
        char check[11] = {0};
        std::fread(check, 10, 1, file);
        std::fseek(file, 0, SEEK_SET);
        if (ReadBakHead(file, timestamp, filenum, path) == -1)
            return StatusCode::UNKNWON_DATAFILE;
        time_t nowTime = time(0);
        if (nowTime < timestamp)
        {
            /* 此处仍保留数据完好的可能性 */
            logger.warn("Backup file's timestamp exceeds current time, it may have broken:{}", latestBak);
        }
        size_t filesize = fs::file_size(latestBak);
        try
        {
            CheckBackup(file, filesize, filenum);
        }
        catch (bad_alloc &e)
        {
        }
        catch (iedb_err &e)
        {
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
        }

        try
        {
            /* Start backup */
            std::fseek(file, pos, SEEK_SET);
            WritePacks(line.second, file);
            /* Update bak head */
            fflush(file);
            std::fseek(file, 0, SEEK_SET);
            long newFileNum = filenum + line.second.size();
            err = WriteBakHead(file, nowTime, newFileNum, path);
            fclose(file);
            logger.info("Backup {}.bak update completed in {}. Checkpoint at {}", nowTime, line.first, ctime(&nowTime));
            lastBackupTime[line.first] = nowTime;
        }
        catch (bad_alloc &e)
        {
        }
        catch (iedb_err &e)
        {
            fclose(file);
            RuntimeLogger.error("Error occured when updating backup : {}", e.what());
            return e.code;
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

int main()
{
    // BackupHelper helper;
    // backupHelper.CreateBackup("testIEDB/JinfeiSixteen");
    FILE *file = fopen("testIEDB_Backup/JinfeiSixteen/1660033849.bak", "rb");
    size_t filesize = fs::file_size("testIEDB_Backup/JinfeiSixteen/1660033849.bak");
    long filenum, timestamp;
    string path;
    backupHelper.ReadBakHead(file, timestamp, filenum, path);
    backupHelper.CheckBackup(file, filesize, filenum);
    return 0;
}