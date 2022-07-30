/***************************************
 * @file Recovery.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.9.0
 * @date Last modification in 2022-07-30
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
        if (!(backup = fopen(selectedBackup.c_str(), "rb")))
        {
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
            return StatusCode::UNKNWON_DATAFILE;
        }
        size_t filesize = fs::file_size(selectedBackup);
        size_t curPos = ftell(backup);
        size_t lastPos = curPos;
        long scanedFile = 0;
        size_t pakLen;
        unsigned short pakPathLen;
        size_t compressedSize;
        if (string(check) != "checkpoint")
        {
            cout << "Backup file broken, trying to rollback to previous checkpoint...\n";
        }

        fseek(backup, lastPos, SEEK_SET);
        for (int i = 0; i < filenum; i++)
        {
            fread(&pakLen, 8, 1, backup);
            fread(&pakPathLen, 2, 1, backup);
            char pakPath[pakPathLen + 1];
            memset(pakPath, 0, pakPathLen + 1);
            fread(pakPath, pakPathLen, 1, backup);

            if (string(pakPath) == path)
            {
                /* Pack found, uncompress the data and overwrite it to <path> */
                Byte outProps[5] = {0};
                fread(outProps, 5, 1, backup);
                fread(&compressedSize, 8, 1, backup);
                Byte *uncompressed = new Byte[pakLen];
                Byte *compressed = new Byte[compressedSize];
                fread(compressed, 1, compressedSize, backup);
                int err = 0;
                if ((err = LzmaUncompress(uncompressed, &pakLen, compressed, &compressedSize, outProps, LZMA_PROPS_SIZE)) != 0)
                {
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
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    return 0;
}

int main()
{
    BackupHelper helper;
    helper.DataRecovery("testIEDB/JinfeiSixteen/1650140143507-1650150945955.pak");
    return 0;
}