/***************************************
 * @file Recovery.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.9.3
 * @date Last modification in 2022-12-14
 *
 * @copyright Copyright (c) 2022
 *
 ***************************************/

#include <BackupHelper.h>

/**
 * @brief
 *
 * @param file
 * @param pos
 * @param bodyPos
 * @return int
 */
int BackupHelper::BakRestoration(FILE *file, size_t &pos, size_t bodyPos) noexcept
{

    return 0;
}

/**
 * @brief 从备份中恢复包数据
 *
 * @param path 目标数据包的路径(含Label)
 * @return int
 * @note 读取所有bak文件，首先检查备份文件的完整性，根据SHA1校验码检查正确性，若出现异常，尝试修复，修复后寻找指定的包，找到后将备份中的包解压并覆盖到原数据文件夹
 */
int BackupHelper::DataRecovery(string path)
{
    if (!fs::exists(path))
        return StatusCode::DATAFILE_NOT_FOUND;
    if (fs::is_directory(path))
    {
        return -1;
    }
    vector<string> bakFiles;
    string tmp = path;
    removeFilenameLabel(tmp);
    string backupDir = backupPath + "/" + fs::path(tmp).parent_path().string();
    readFiles(bakFiles, backupDir, ".bak");
    vector<pair<string, long>> bakWithTime;
    for (auto &bak : bakFiles)
    {
        bakWithTime.push_back({bak, stol(fs::path(bak).stem())});
    }
    sortByTime(bakWithTime, TIME_ASC);
    /* Find the backup which includes the pack */
    string selectedBackup;
    long packTimestamp = stol(fs::path(path).stem());
    if (bakWithTime.size() == 1)
        selectedBackup = bakWithTime[0].first;
    for (int i = 0; i < bakWithTime.size() - 1; i++)
    {
        if (packTimestamp >= bakWithTime[i].second * 1000 && packTimestamp <= bakWithTime[i + 1].second * 1000)
        {
            selectedBackup = bakWithTime[i].first;
            break;
        }
        else if (i == bakWithTime.size() - 2)
            selectedBackup = bakWithTime[i + 1].first;
    }

    FILE *backup;
    if (!(backup = fopen(selectedBackup.c_str(), "r+b")))
    {
        logger.critical("Error when opening {} : {}", selectedBackup, strerror(errno));
        return errno;
    }
    long filenum;
    time_t timestamp;
    string bakpath;

    fseek(backup, -10, SEEK_END);
    long pos = ftell(backup);
    // char check[11] = {0};
    // fread(check, 10, 1, backup);
    fseek(backup, 0, SEEK_SET);
    if (ReadBakHead(backup, timestamp, filenum, bakpath) == -1)
        return StatusCode::UNKNWON_DATAFILE;
    time_t nowTime = time(0);
    if (nowTime < timestamp)
    {
        fclose(backup);
        logger.critical("Backup file's timestamp exceeded current time, it may have broken:{}", selectedBackup);
        return StatusCode::UNKNWON_DATAFILE;
    }
    size_t filesize = fs::file_size(selectedBackup);
    size_t curPos = ftell(backup);
    size_t startPos = curPos;
    FILE_CNT_DTYPE scanedFile = 0;
    size_t pakLen;
    PACK_PATH_SIZE_DTYPE pakPathLen;
    COMPRESSED_PACK_SIZE_DTYPE compressedSize;
    // if (string(check) != "checkpoint")
    // {
    //     logger.warn("Backup file {} broken, trying to rollback to previous checkpoint...", selectedBackup);
    //     size_t prevPos = curPos;
    //     while (curPos < filesize - 23 && scanedFile < filenum)
    //     {
    //         prevPos = curPos;
    //         fread(&pakLen, 8, 1, backup);
    //         fread(&pakPathLen, 2, 1, backup);
    //         fseek(backup, pakPathLen + 5, SEEK_CUR);
    //         fread(&compressedSize, 8, 1, backup);

    //         if (fseek(backup, compressedSize, SEEK_CUR) != 0)
    //         {
    //             /* File pointer exceeded the end of file */
    //             break;
    //         }
    //         Byte sha1[20];
    //         ComputeSHA1(compressedSize, backup, sha1);
    //         /* Compare the computed sha1 value and the stored value */
    //         Byte sha1_backup[20];
    //         std::fread(sha1_backup, 1, 20, backup);
    //         if (memcmp(sha1, sha1_backup, 20) != 0)
    //             break;
    //         curPos = ftell(backup);
    //         scanedFile++;
    //     }

    //     /* Add checkpoint to end of file */
    //     fseek(backup, prevPos, SEEK_SET);
    //     fwrite("checkpoint", 10, 1, backup);
    //     fflush(backup);
    //     fs::resize_file(selectedBackup, prevPos + 10);
    //     logger.warn("Rollback completed, {} file(s) lost.", filenum - scanedFile);
    // }
    Byte *uncompressed = nullptr;
    Byte *compressed = nullptr;
    try
    {
        fseek(backup, startPos, SEEK_SET);
        for (int i = 0; i < filenum; i++)
        {
            fread(&pakLen, sizeof(PACK_SIZE_DTYPE), 1, backup);
            fread(&pakPathLen, sizeof(PACK_PATH_SIZE_DTYPE), 1, backup);
            char pakPath[pakPathLen + 1];
            memset(pakPath, 0, pakPathLen + 1);
            fread(pakPath, pakPathLen, 1, backup);

            if (string(pakPath) == path)
            {
                /* Package found, uncompress the data and overwrite it to <path> */
                Byte outProps[LZMA_PROPS_SIZE] = {0};
                fread(outProps, LZMA_PROPS_SIZE, 1, backup);
                fread(&compressedSize, sizeof(PACK_SIZE_DTYPE), 1, backup);
                uncompressed = new Byte[pakLen];
                compressed = new Byte[compressedSize];
                fread(compressed, 1, compressedSize, backup);

                // 计算和检查SHA1值
                Byte sha1[SHA1_SIZE], sha1_backup[SHA1_SIZE];
                std::fread(sha1_backup, 1, SHA1_SIZE, backup);
                SHA1(compressed, compressedSize, sha1);
                if (memcmp(sha1, sha1_backup, SHA1_SIZE) != 0)
                {
                    throw iedb_err(StatusCode::DATAFILE_MODIFIED);
                }

                int err = 0;
                if ((err = LzmaUncompress(uncompressed, &pakLen, compressed, &compressedSize, outProps, LZMA_PROPS_SIZE)) != 0)
                {
                    throw iedb_err(err);
                }
                FILE *pack;
                if (!(pack = fopen(path.c_str(), "wb")))
                {
                    throw iedb_err(errno);
                }
                fwrite(uncompressed, 1, pakLen, pack);
                fclose(pack);
                fclose(backup);
                return 0;
            }
            else
            {
                fseek(backup, LZMA_PROPS_SIZE, SEEK_CUR);
                fread(&compressedSize, sizeof(compressedSize), 1, backup);
                fseek(backup, compressedSize + SHA1_SIZE, SEEK_CUR);
            }
        }
        fclose(backup);
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    catch (bad_alloc &e)
    {
        if (compressed != nullptr)
            delete[] compressed;
        if (uncompressed != nullptr)
            delete[] uncompressed;
        logger.critical("bad alloc when uncompressing");
        fclose(backup);
    }
    catch (iedb_err &e)
    {
        if (compressed != nullptr)
            delete[] compressed;
        if (uncompressed != nullptr)
            delete[] uncompressed;
        if (e.code <= 5)
        {
            logger.error("Error occured when uncompressing: code{}", e.what());
            // return e.code;
        }
        else
        {
            logger.error("Failed to recover {} : {}", path, e.what());
        }
        fclose(backup);
        return e.code;
    }
    catch (const std::exception &e)
    {
        if (compressed != nullptr)
            delete[] compressed;
        if (uncompressed != nullptr)
            delete[] uncompressed;
        std::cerr << e.what() << '\n';
    }
    return 0;
}

/**
 * @brief 恢复整个文件夹的数据
 *
 * @param path 目标文件夹的路径 含Label
 * @return int
 * @note 读取所有bak文件，首先检查备份文件的完整性，根据SHA1校验码检查正确性，若出现异常，尝试修复，修复后逐一将备份中的包覆盖到原数据文件夹
 */
int BackupHelper::RecoverAll(string path)
{
    if (!fs::exists(path))
        fs::create_directories(path);
    if (!fs::is_directory(path))
        return -1;
    string dir = path;
    removeFilenameLabel(dir);
    dir = backupPath + "/" + dir;
    vector<string> bakFiles;
    readFiles(bakFiles, dir, ".bak");

    for (auto &bak : bakFiles)
    {
        FILE *backup;
        if (!(backup = fopen(bak.c_str(), "rb")))
        {
            logger.critical("Error when opening {} : {}", bak, strerror(errno));
            return errno;
        }
        long filenum;
        time_t timestamp;
        string bakpath;

        fseek(backup, -10, SEEK_END);
        long pos = ftell(backup);
        char check[11] = {0};
        fread(check, 10, 1, backup);
        fseek(backup, 0, SEEK_SET);
        if (ReadBakHead(backup, timestamp, filenum, bakpath) == -1)
            return StatusCode::UNKNWON_DATAFILE;
        time_t nowTime = time(0);
        if (nowTime < timestamp)
        {
            // fclose(backup);
            logger.warn("Backup file's timestamp exceeded current time, it may have broken:{}", bak);
            // return StatusCode::UNKNWON_DATAFILE;
        }
        size_t filesize = fs::file_size(bak);
        size_t curPos = ftell(backup);
        size_t startPos = curPos;
        FILE_CNT_DTYPE scanedFile = 0;
        size_t pakLen;
        PACK_PATH_SIZE_DTYPE pakPathLen;
        COMPRESSED_PACK_SIZE_DTYPE compressedSize;
        // if (string(check) != "checkpoint")
        // {
        //     // spdlog::warn("Backup file {} broken, trying to rollback to previous checkpoint...", bak);
        //     logger.warn("Backup file {} broken, trying to rollback to previous checkpoint...", bak);
        //     size_t prevPos = curPos;
        //     while (curPos < filesize - 23 && scanedFile < filenum)
        //     {
        //         fread(&pakLen, 8, 1, backup);
        //         fread(&pakPathLen, 2, 1, backup);
        //         fseek(backup, pakPathLen + 5, SEEK_CUR);
        //         fread(&compressedSize, 8, 1, backup);

        //         if (fseek(backup, compressedSize, SEEK_CUR) != 0)
        //         {
        //             /* File pointer exceeded the end of file */
        //             break;
        //         }
        //         Byte sha1[20];
        //         ComputeSHA1(compressedSize, backup, sha1);
        //         /* Compare the computed sha1 value and the stored value */
        //         Byte sha1_backup[20];
        //         std::fread(sha1_backup, 1, 20, backup);
        //         if (memcmp(sha1, sha1_backup, 20) != 0)
        //             break;

        //         curPos = ftell(backup);
        //         scanedFile++;
        //     }
        //     /* Add checkpoint to end of file */
        //     fseek(backup, prevPos, SEEK_SET);
        //     fwrite("checkpoint", 10, 1, backup);
        //     fflush(backup);
        //     fs::resize_file(bak, prevPos + 10);
        //     // spdlog::warn("Rollback completed, {} file(s) lost.", filenum - scanedFile);
        //     logger.warn("Rollback completed, {} file(s) lost.", filenum - scanedFile);
        //     filenum = scanedFile;
        // }
        Byte *uncompressed = nullptr;
        Byte *compressed = nullptr;
        try
        {
            fseek(backup, startPos, SEEK_SET);
            for (int i = 0; i < filenum; i++)
            {
                fread(&pakLen, sizeof(PACK_SIZE_DTYPE), 1, backup);
                fread(&pakPathLen, sizeof(PACK_PATH_SIZE_DTYPE), 1, backup);
                char pakPath[pakPathLen + 1];
                memset(pakPath, 0, pakPathLen + 1);
                fread(pakPath, pakPathLen, 1, backup);
                Byte outProps[LZMA_PROPS_SIZE] = {0};
                fread(outProps, LZMA_PROPS_SIZE, 1, backup);
                fread(&compressedSize, sizeof(COMPRESSED_PACK_SIZE_DTYPE), 1, backup);
                uncompressed = new Byte[pakLen];
                compressed = new Byte[compressedSize];
                fread(compressed, 1, compressedSize, backup);

                // 计算和检查SHA1值
                Byte sha1[SHA1_SIZE], sha1_backup[SHA1_SIZE];
                std::fread(sha1_backup, 1, SHA1_SIZE, backup);
                SHA1(compressed, compressedSize, sha1);
                if (memcmp(sha1, sha1_backup, SHA1_SIZE) != 0)
                {
                    throw iedb_err(StatusCode::DATAFILE_MODIFIED);
                }

                int err = 0;
                if ((err = LzmaUncompress(uncompressed, &pakLen, compressed, &compressedSize, outProps, LZMA_PROPS_SIZE)) != 0)
                {
                    throw iedb_err(err);
                }
                FILE *pack;
                if (!(pack = fopen(string(pakPath).c_str(), "wb")))
                {
                    fclose(backup);
                    return errno;
                }
                fwrite(uncompressed, 1, pakLen, pack);
                fclose(pack);
                logger.info("{} recover success", string(pakPath));
            }
            fclose(backup);
        }
        catch (bad_alloc &e)
        {
            if (compressed != nullptr)
                delete[] compressed;
            if (uncompressed != nullptr)
                delete[] uncompressed;
            logger.critical("Memory allocation failed when compressing data!");
            return StatusCode::MEMORY_INSUFFICIENT;
        }
        catch (iedb_err &e)
        {
            if (compressed != nullptr)
                delete[] compressed;
            if (uncompressed != nullptr)
                delete[] uncompressed;
            if (e.code <= 5)
            {
                logger.error("Error occured when uncompressing: code{}", e.what());
                // return e.code;
            }
            else
            {
                logger.error("Error: {}", e.what());
            }
            return e.code;
        }
        catch (const std::exception &e)
        {
            if (compressed != nullptr)
                delete[] compressed;
            if (uncompressed != nullptr)
                delete[] uncompressed;
            std::cerr << e.what() << '\n';
        }
    }

    return 0;
}

int DB_Recovery(const char *path)
{
    return fs::is_directory(path) ? backupHelper.RecoverAll(path) : backupHelper.DataRecovery(path);
}

// int main()
// {
//     // BackupHelper helper;
//     // helper.DataRecovery("testIEDB/JinfeiSixteen/1650140143507-1650150945955.pak");
//     backupHelper.RecoverAll("testIEDB/JinfeiSixteen");
//     return 0;
// }