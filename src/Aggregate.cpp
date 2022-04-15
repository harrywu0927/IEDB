
#include <utils.hpp>

int DB_GetNormalDataCount(DB_QueryParams *params, long *count)
{
    long normal = 0;
    vector<string> packFiles, dataFiles;
    vector<pair<string, long>> selectedPacks, dataWithTime;
    switch (params->queryType)
    {
    case TIMESPAN:
    {
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &file : dataWithTime)
        {
            struct stat fileInfo;
            if (stat(file.first.c_str(), &fileInfo) == -1)
            {
                continue;
            }
            if (S_ISREG(fileInfo.st_mode)) //是常规文件
            {
                if (file.second >= params->start && file.second <= params->end)
                {
                    if (fileInfo.st_size == 0)
                    {
                        normal++;
                        continue;
                    }
                    if (file.first.back() != 'p')
                    {
                        DB_DataBuffer buffer;
                        buffer.savePath = file.first.c_str();
                        DB_ReadFile(&buffer);
                        if (IsNormalIDBFile(buffer.buffer, params->pathToLine))
                            normal++;
                    }
                }
            }
        }
        for (auto &file : packFiles)
        {
            string tmp = file;
            while (tmp.back() == '/')
                tmp.pop_back();
            vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
            string packName = vec[vec.size() - 1];
            vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
            if (timespan.size() > 0)
            {
                long start = atol(timespan[0].c_str());
                long end = atol(timespan[1].c_str());
                if ((start < params->start && end >= params->start) || (start < params->end && end >= params->start) || (start <= params->start && end >= params->end) || (start >= params->start && end <= params->end)) //落入或部分落入时间区间
                {
                    selectedPacks.push_back(make_pair(file, start));
                }
            }
        }
        for (auto &pack : selectedPacks)
        {
            PackFileReader packReader(pack.first);
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            for (int i = 0; i < fileNum; i++)
            {
                long timestamp;
                int zipType, readLength;
                packReader.Next(readLength, timestamp, zipType);
                if (timestamp >= params->start && timestamp <= params->end && zipType == 1)
                    normal++;
            }
        }
        *count = normal;
        break;
    }
    case LAST:
    {
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        sortByTime(dataWithTime, TIME_DSC);
        long i;
        for (i = 0; i < params->queryNums && i < dataWithTime.size(); i++)
        {
            struct stat fileInfo;
            if (stat(dataWithTime[i].first.c_str(), &fileInfo) == -1)
            {
                continue;
            }
            if (S_ISREG(fileInfo.st_mode)) //是常规文件
            {
                if (fileInfo.st_size == 0)
                {
                    normal++;
                    continue;
                }
                if (dataWithTime[i].first.back() != 'p')
                {
                    DB_DataBuffer buffer;
                    buffer.savePath = dataWithTime[i].first.c_str();
                    DB_ReadFile(&buffer);
                    if (IsNormalIDBFile(buffer.buffer, params->pathToLine))
                        normal++;
                }
            }
        }
        if (i == params->queryNums)
        {
            *count = normal;
            return 0;
        }
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &&file : packFiles)
        {
            string tmp = file;
            while (tmp.back() == '/')
                tmp.pop_back();
            vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
            string packName = vec[vec.size() - 1];
            vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
            if (timespan.size() > 0)
            {
                long start = atol(timespan[0].c_str());
                selectedPacks.push_back(make_pair(file, start));
            }
        }
        sortByTime(selectedPacks, TIME_DSC);
        for (auto &&pack : selectedPacks)
        {
            PackFileReader packReader(pack.first);
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            if (i + fileNum < params->queryNums)
            {
                i += fileNum;
                for (int j = 0; j < fileNum; j++)
                {
                    long timestamp;
                    int zipType, readLength;
                    packReader.Next(readLength, timestamp, zipType);
                    if (zipType == 1)
                        normal++;
                }
            }
            else
            {
                long timestamp;
                int zipType, readLength;
                packReader.Skip(fileNum - (params->queryNums - i));
                for (int j = fileNum - (params->queryNums - i); j < fileNum; j++)
                {
                    packReader.Next(readLength, timestamp, zipType);
                    if (zipType == 1)
                        normal++;
                }
            }
        }
        *count = normal;
        break;
    }
    case FILEID:
    {
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
        break;
    }
    default:
    {
        readDataFiles(params->pathToLine, dataFiles);
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &file : dataFiles)
        {
            struct stat fileInfo;
            if (stat(file.c_str(), &fileInfo) == -1)
            {
                continue;
            }
            if (S_ISREG(fileInfo.st_mode)) //是常规文件
            {
                if (fileInfo.st_size == 0)
                {
                    normal++;
                    continue;
                }
                if (file.back() != 'p')
                {
                    DB_DataBuffer buffer;
                    buffer.savePath = file.c_str();
                    DB_ReadFile(&buffer);
                    if (IsNormalIDBFile(buffer.buffer, params->pathToLine))
                        normal++;
                }
            }
        }
        for (auto &pack : packFiles)
        {
            PackFileReader packReader(pack);
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            for (int i = 0; i < fileNum; i++)
            {
                long timestamp;
                int zipType, readLength;
                packReader.Next(readLength, timestamp, zipType);
                if (zipType == 1)
                    normal++;
            }
        }
        *count = normal;
    }
    }
    return 0;
}
int DB_GetAbnormalDataCount(DB_QueryParams *params, long *count)
{
    long abnormal = 0;
    vector<string> packFiles, dataFiles;
    vector<pair<string, long>> selectedPacks, dataWithTime;
    switch (params->queryType)
    {
    case TIMESPAN:
    {
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &file : dataWithTime)
        {
            struct stat fileInfo;
            if (stat(file.first.c_str(), &fileInfo) == -1)
            {
                continue;
            }
            if (S_ISREG(fileInfo.st_mode)) //是常规文件
            {
                if (file.second >= params->start && file.second <= params->end)
                {
                    if (file.first.back() != 'p') // is .idb
                    {
                        DB_DataBuffer buffer;
                        buffer.savePath = file.first.c_str();
                        DB_ReadFile(&buffer);
                        if (!IsNormalIDBFile(buffer.buffer, params->pathToLine))
                            abnormal++;
                    }
                    else
                    {
                        if (fileInfo.st_size != 0)
                            abnormal++;
                    }
                }
            }
        }
        for (auto &file : packFiles)
        {
            string tmp = file;
            while (tmp.back() == '/')
                tmp.pop_back();
            vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
            string packName = vec[vec.size() - 1];
            vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
            if (timespan.size() > 0)
            {
                long start = atol(timespan[0].c_str());
                long end = atol(timespan[1].c_str());
                if ((start < params->start && end >= params->start) || (start < params->end && end >= params->start) || (start <= params->start && end >= params->end) || (start >= params->start && end <= params->end)) //落入或部分落入时间区间
                {
                    selectedPacks.push_back(make_pair(file, start));
                }
            }
        }
        for (auto &pack : selectedPacks)
        {
            PackFileReader packReader(pack.first);
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            for (int i = 0; i < fileNum; i++)
            {
                long timestamp;
                int zipType, readLength;
                packReader.Next(readLength, timestamp, zipType);
                if (timestamp >= params->start && timestamp <= params->end && zipType != 1)
                    abnormal++;
            }
        }
        *count = abnormal;
        break;
    }
    case LAST:
    {
        readDataFilesWithTimestamps(params->pathToLine, dataWithTime);
        sortByTime(dataWithTime, TIME_DSC);
        long i;
        for (i = 0; i < params->queryNums && i < dataWithTime.size(); i++)
        {
            struct stat fileInfo;
            if (stat(dataWithTime[i].first.c_str(), &fileInfo) == -1)
            {
                continue;
            }
            if (S_ISREG(fileInfo.st_mode)) //是常规文件
            {
                if (dataWithTime[i].first.back() != 'p') // is .idb
                {
                    DB_DataBuffer buffer;
                    buffer.savePath = dataWithTime[i].first.c_str();
                    DB_ReadFile(&buffer);
                    if (!IsNormalIDBFile(buffer.buffer, params->pathToLine))
                        abnormal++;
                }
                else
                {
                    if (fileInfo.st_size != 0)
                        abnormal++;
                }
            }
        }
        if (i == params->queryNums)
        {
            *count = abnormal;
            return 0;
        }
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &&file : packFiles)
        {
            string tmp = file;
            while (tmp.back() == '/')
                tmp.pop_back();
            vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
            string packName = vec[vec.size() - 1];
            vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
            if (timespan.size() > 0)
            {
                long start = atol(timespan[0].c_str());
                selectedPacks.push_back(make_pair(file, start));
            }
        }
        sortByTime(selectedPacks, TIME_DSC);
        for (auto &&pack : selectedPacks)
        {
            PackFileReader packReader(pack.first);
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            if (i + fileNum < params->queryNums)
            {
                i += fileNum;
                for (int j = 0; j < fileNum; j++)
                {
                    long timestamp;
                    int zipType, readLength;
                    packReader.Next(readLength, timestamp, zipType);
                    if (zipType != 1)
                        abnormal++;
                }
            }
            else
            {
                long timestamp;
                int zipType, readLength;
                packReader.Skip(fileNum - (params->queryNums - i));
                for (int j = fileNum - (params->queryNums - i); j < fileNum; j++)
                {
                    packReader.Next(readLength, timestamp, zipType);
                    if (zipType != 1)
                        abnormal++;
                }
            }
        }
        *count = abnormal;
        break;
    }
    case FILEID:
    {
        return StatusCode::QUERY_TYPE_NOT_SURPPORT;
        break;
    }
    default:
    {
        readDataFiles(params->pathToLine, dataFiles);
        readPakFilesList(params->pathToLine, packFiles);
        for (auto &file : dataFiles)
        {
            struct stat fileInfo;
            if (stat(file.c_str(), &fileInfo) == -1)
            {
                continue;
            }
            if (S_ISREG(fileInfo.st_mode)) //是常规文件
            {
                if (file.back() != 'p') // is .idb
                {
                    DB_DataBuffer buffer;
                    buffer.savePath = file.c_str();
                    DB_ReadFile(&buffer);
                    if (!IsNormalIDBFile(buffer.buffer, params->pathToLine))
                        abnormal++;
                }
                else
                {
                    if (fileInfo.st_size != 0)
                        abnormal++;
                }
            }
        }
        for (auto &pack : packFiles)
        {
            PackFileReader packReader(pack);
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            for (int i = 0; i < fileNum; i++)
            {
                long timestamp;
                int zipType, readLength;
                packReader.Next(readLength, timestamp, zipType);
                if (zipType != 1)
                    abnormal++;
            }
        }
        *count = abnormal;
    }
    }
    return 0;
}
// int main()
// {
//     DB_QueryParams params;
//     params.pathToLine = "JinfeiThirteen";
//     params.fileID = "JinfeiThirteen103845";
//     char code[10];
//     code[0] = (char)0;
//     code[1] = (char)1;
//     code[2] = (char)0;
//     code[3] = (char)0;
//     code[4] = 0;
//     code[5] = (char)0;
//     code[6] = 0;
//     code[7] = (char)0;
//     code[8] = (char)0;
//     code[9] = (char)0;
//     params.pathCode = code;
//     params.valueName = "S2OFF";
//     // params.valueName = NULL;
//     params.start = 1649897531555;
//     params.end = 1649901032603;
//     params.order = ASCEND;
//     params.compareType = LT;
//     params.compareValue = "666";
//     params.queryType = TIMESPAN;
//     params.byPath = 0;
//     params.queryNums = 10;
//     long count;
//     DB_GetAbnormalDataCount(&params, &count);
//     return 0;
// }