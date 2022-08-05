/***************************************
 * @file Recovery.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.9.0
 * @date Last modification in 2022-08-01
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
int BackupHelper::BakRestoration(FILE *file, size_t &pos, size_t bodyPos)
{
    return 0;
}

/**
 * @brief 从备份中恢复包数据
 *
 * @param path 目标数据包的路径(含Label)
 * @return int
 */
int BackupHelper::DataRecovery(string path)
{
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
    try
    {
        FILE *backup;
        if (!(backup = fopen(selectedBackup.c_str(), "r+b")))
        {
            // spdlog::critical("Error when opening {} : {}", selectedBackup, strerror(errno));
            logger.critical("Error when opening {} : {}", selectedBackup, strerror(errno));
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
            fclose(backup);
            // logger.critical("Backup file's timestamp exceeded current time, it may have broken:{}", selectedBackup);
            spdlog::critical("Backup file's timestamp exceeded current time, it may have broken:{}", selectedBackup);
            return StatusCode::UNKNWON_DATAFILE;
        }
        size_t filesize = fs::file_size(selectedBackup);
        size_t curPos = ftell(backup);
        size_t startPos = curPos;
        long scanedFile = 0;
        size_t pakLen;
        unsigned short pakPathLen;
        size_t compressedSize;
        if (string(check) != "checkpoint")
        {
            // spdlog::warn("Backup file {} broken, trying to rollback to previous checkpoint...", selectedBackup);
            logger.warn("Backup file {} broken, trying to rollback to previous checkpoint...", selectedBackup);
            size_t prevPos = curPos;
            while (curPos < filesize - 23 && scanedFile < filenum)
            {
                prevPos = curPos;
                fread(&pakLen, 8, 1, backup);
                fread(&pakPathLen, 2, 1, backup);
                fseek(backup, pakPathLen + 5, SEEK_CUR);
                fread(&compressedSize, 8, 1, backup);

                if (fseek(backup, compressedSize, SEEK_CUR) != 0)
                {
                    /* File pointer exceeded the end of file */
                    break;
                }
                curPos = ftell(backup);
                scanedFile++;
            }
            /* Add checkpoint to end of file */
            fseek(backup, prevPos, SEEK_SET);
            fwrite("checkpoint", 10, 1, backup);
            fflush(backup);
            fs::resize_file(selectedBackup, prevPos + 10);
            // cout << "Rollback completed, " << filenum - scanedFile << " file(s) lost.\n";
            // spdlog::warn("Rollback completed, {} file(s) lost.", filenum - scanedFile);
            logger.warn("Rollback completed, {} file(s) lost.", filenum - scanedFile);
        }

        fseek(backup, startPos, SEEK_SET);
        for (int i = 0; i < filenum; i++)
        {
            fread(&pakLen, 8, 1, backup);
            fread(&pakPathLen, 2, 1, backup);
            char pakPath[pakPathLen + 1];
            memset(pakPath, 0, pakPathLen + 1);
            fread(pakPath, pakPathLen, 1, backup);

            if (string(pakPath) == path)
            {
                /* Package found, uncompress the data and overwrite it to <path> */
                Byte outProps[5] = {0};
                fread(outProps, 5, 1, backup);
                fread(&compressedSize, 8, 1, backup);
                Byte *uncompressed = new Byte[pakLen];
                Byte *compressed = new Byte[compressedSize];
                fread(compressed, 1, compressedSize, backup);
                int err = 0;
                if ((err = LzmaUncompress(uncompressed, &pakLen, compressed, &compressedSize, outProps, LZMA_PROPS_SIZE)) != 0)
                {
                    delete[] uncompressed;
                    delete[] compressed;
                    throw err;
                }
                FILE *pack;
                if (!(pack = fopen(path.c_str(), "wb")))
                {
                    delete[] uncompressed;
                    delete[] compressed;
                    fclose(backup);
                    return errno;
                }
                fwrite(uncompressed, 1, pakLen, pack);
                fclose(pack);
                fclose(backup);
                return 0;
            }
            else
            {
                fseek(backup, 5, SEEK_CUR);
                fread(&compressedSize, 8, 1, backup);
                fseek(backup, compressedSize, SEEK_CUR);
            }
        }
        fclose(backup);
        return StatusCode::DATAFILE_NOT_FOUND;
    }
    catch (bad_alloc &e)
    {
        // spdlog::critical("bad alloc when uncompressing");
        logger.critical("bad alloc when uncompressing");
    }
    catch (int &e)
    {
        // spdlog::error("Error occured when uncompressing: code{}", e);
        logger.error("Error occured when uncompressing: code{}", e);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    return 0;
}

/**
 * @brief 恢复整个文件夹的数据
 *
 * @param path 目标文件夹的路径 含Label
 * @return int
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
    try
    {
        for (auto &bak : bakFiles)
        {
            FILE *backup;
            if (!(backup = fopen(bak.c_str(), "rb")))
            {
                // spdlog::critical("Error when opening {} : {}", bak, strerror(errno));
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
                fclose(backup);
                // spdlog::critical("Backup file's timestamp exceeded current time, it may have broken:{}", bak);
                logger.critical("Backup file's timestamp exceeded current time, it may have broken:{}", bak);
                return StatusCode::UNKNWON_DATAFILE;
            }
            size_t filesize = fs::file_size(bak);
            size_t curPos = ftell(backup);
            size_t startPos = curPos;
            long scanedFile = 0;
            size_t pakLen;
            unsigned short pakPathLen;
            size_t compressedSize;
            if (string(check) != "checkpoint")
            {
                // spdlog::warn("Backup file {} broken, trying to rollback to previous checkpoint...", bak);
                logger.warn("Backup file {} broken, trying to rollback to previous checkpoint...", bak);
                size_t prevPos = curPos;
                while (curPos < filesize - 23 && scanedFile < filenum)
                {
                    fread(&pakLen, 8, 1, backup);
                    fread(&pakPathLen, 2, 1, backup);
                    fseek(backup, pakPathLen + 5, SEEK_CUR);
                    fread(&compressedSize, 8, 1, backup);

                    if (fseek(backup, compressedSize, SEEK_CUR) != 0)
                    {
                        /* File pointer exceeded the end of file */
                        break;
                    }
                    curPos = ftell(backup);
                    scanedFile++;
                }
                /* Add checkpoint to end of file */
                fseek(backup, prevPos, SEEK_SET);
                fwrite("checkpoint", 10, 1, backup);
                fflush(backup);
                fs::resize_file(bak, prevPos + 10);
                // spdlog::warn("Rollback completed, {} file(s) lost.", filenum - scanedFile);
                logger.warn("Rollback completed, {} file(s) lost.", filenum - scanedFile);
                filenum = scanedFile;
            }

            fseek(backup, startPos, SEEK_SET);
            for (int i = 0; i < filenum; i++)
            {
                fread(&pakLen, 8, 1, backup);
                fread(&pakPathLen, 2, 1, backup);
                char pakPath[pakPathLen + 1];
                memset(pakPath, 0, pakPathLen + 1);
                fread(pakPath, pakPathLen, 1, backup);
                Byte outProps[5] = {0};
                fread(outProps, 5, 1, backup);
                fread(&compressedSize, 8, 1, backup);
                Byte *uncompressed = new Byte[pakLen];
                Byte *compressed = new Byte[compressedSize];
                fread(compressed, 1, compressedSize, backup);
                int err = 0;
                if ((err = LzmaUncompress(uncompressed, &pakLen, compressed, &compressedSize, outProps, LZMA_PROPS_SIZE)) != 0)
                {
                    delete[] uncompressed;
                    delete[] compressed;
                    throw err;
                }
                FILE *pack;
                if (!(pack = fopen(string(pakPath).c_str(), "wb")))
                {
                    delete[] uncompressed;
                    delete[] compressed;
                    fclose(backup);
                    return errno;
                }
                fwrite(uncompressed, 1, pakLen, pack);
                fclose(pack);
                // spdlog::info("{} recover success", string(pakPath));
                logger.info("{} recover success", string(pakPath));
            }
            fclose(backup);
        }
    }
    catch (bad_alloc &e)
    {
        // spdlog::critical("Memory allocation failed when compressing data!");
        logger.critical("Memory allocation failed when compressing data!");
    }
    catch (int &e)
    {
        // spdlog::error("Error occured when uncompressing: code{}", e);
        logger.error("Error occured when uncompressing: code{}", e);
        return e;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
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