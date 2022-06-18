/*******************************************
 * @file Delete.cpp
 * @author your name (you@domain.com)
 * @brief 整条数据按条件删除。因不常用，未作特殊速度优化
 * @version 0.8.4
 * @date Modified in 2022-06-15
 *
 * @copyright Copyright (c) 2022
 *
 *******************************************/

#include <utils.hpp>

/**
 * @brief 决定此数据是否可删除
 *
 * @param params 查询参数
 * @param buffer 数据缓冲区指针
 * @return bool
 */
bool CanDelete(DB_QueryParams *params, char *buffer)
{
    //获取数据的偏移量和数据类型
    long pos = 0, bytes = 0;
    DataType type;
    long compareBytes = 0;
    int err = params->byPath ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buffer, pos, compareBytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buffer, pos, compareBytes, type);
    if (err != 0)
    {
        return err;
    }
    bool canDelete = false;
    if (compareBytes != 0) //可比较
    {
        char value[compareBytes]; //数值
        memcpy(value, buffer + pos, compareBytes);
        //根据比较结果决定是否加入结果集
        int compareRes = DataType::CompareValue(type, value, params->compareValue);
        switch (params->compareType)
        {
        case DB_CompareType::GT:
        {
            if (compareRes > 0)
            {
                canDelete = true;
            }
            break;
        }
        case DB_CompareType::LT:
        {
            if (compareRes < 0)
            {
                canDelete = true;
            }
            break;
        }
        case DB_CompareType::GE:
        {
            if (compareRes >= 0)
            {
                canDelete = true;
            }
            break;
        }
        case DB_CompareType::LE:
        {
            if (compareRes <= 0)
            {
                canDelete = true;
            }
            break;
        }
        case DB_CompareType::EQ:
        {
            if (compareRes == 0)
            {
                canDelete = true;
            }
            break;
        }
        default:
            break;
        }
    }
    return canDelete;
}

/**
 * @brief 根据自定义查询请求参数，删除符合条件的记录
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  删除包中文件后需要修改此包总文件数，重新打包，修改包文件名
 */
int DB_DeleteRecords(DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;
    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
    {
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    switch (params->queryType)
    {
    case TIMESPAN:
    {
        vector<string> packsList;
        vector<pair<string, long>> filesWithTime, selectedFiles;
        vector<pair<string, tuple<long, long>>> selectedPacks;
        readDataFilesWithTimestamps(params->pathToLine, filesWithTime);
        readPakFilesList(params->pathToLine, packsList);
        //筛选落入时间区间内的文件
        for (auto &file : filesWithTime)
        {
            if (file.second >= params->start && file.second <= params->end)
            {
                selectedFiles.push_back(make_pair(file.first, file.second));
            }
        }
        for (auto &pack : packsList)
        {
            string tmp = pack;
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
                    selectedPacks.push_back(make_pair(pack, make_tuple(start, end)));
                }
            }
        }
        if (params->compareType == CMP_NONE) //不需要比较数值，因此无需读取普通文件增加I/O耗时
        {
            for (auto &file : selectedFiles)
            {
                DB_DeleteFile(const_cast<char *>(file.first.c_str()));
            }
            /**
             *  对于包文件，若其起始时间和结束时间均在范围内，则直接删除，
             *  否则，将包内的范围外的数据拷出后写成新的文件，将旧文件覆盖
             */
            for (auto &pack : selectedPacks)
            {
                if (std::get<0>(pack.second) >= params->start && std::get<1>(pack.second) <= params->end)
                {
                    packManager.DeletePack(pack.first);
                    continue;
                }
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                TemplateManager::CheckTemplate(templateName);
                int deleteNum = 0;
                //一次分配一整个pak长度的空间，避免频繁分配影响性能
                char *newPack = new char[packReader.GetPackLength()];
                long cur = 4;
                memcpy(newPack + cur, templateName.c_str(), templateName.length() <= 20 ? templateName.length() : 20);
                cur += 20;
                bool packNameChange = false; //若删除的数据包含首尾文件，则包名的时间段需要修改
                for (int i = 0; i < fileNum; i++)
                {
                    long timestamp;
                    int readLength, zipType;
                    string fileID;
                    long dataPos = packReader.Next(readLength, timestamp, fileID, zipType);
                    if (timestamp >= params->start && timestamp <= params->end) //在时间区间内
                    {
                        deleteNum++;
                        if (timestamp == get<1>(pack.second) || timestamp == get<0>(pack.second))
                            packNameChange = true;
                        continue;
                    }
                    if (zipType != 1) //非完全压缩
                    {
                        memcpy(newPack + cur, packReader.packBuffer + dataPos - 33, 33 + readLength);
                        cur += readLength + 33;
                    }
                    else
                    {
                        memcpy(newPack + cur, packReader.packBuffer + dataPos - 29, 29);
                        cur += 29;
                    }
                }
                int newFileNum = fileNum - deleteNum;
                memcpy(newPack, &newFileNum, 4);

                if (packNameChange)
                {
                    long newpackStart, newpackEnd;
                    int readLength, zipType;
                    free(packReader.packBuffer);
                    packReader.packBuffer = newPack;
                    packReader.SetCurPos(24);
                    packReader.Next(readLength, newpackStart, zipType);
                    packReader.Skip(newFileNum - 2);
                    packReader.Next(readLength, newpackEnd, zipType);
                    auto vec = DataType::splitWithStl(pack.first, "/");
                    string newPackName = vec[0] + "/" + to_string(newpackStart) + "-" + to_string(newpackEnd) + ".pak";
                    char mode[2] = {'w', 'b'};
                    long fp;
                    DB_Open(const_cast<char *>(newPackName.c_str()), mode, &fp);
                    fwrite(newPack, cur, 1, (FILE *)fp);
                    delete[] newPack;
                    newPack = NULL;
                    DB_Close(fp);
                    DB_DeleteFile(const_cast<char *>(pack.first.c_str()));
                    continue;
                }
                char mode[2] = {'w', 'b'};
                long fp;
                DB_Open(const_cast<char *>(pack.first.c_str()), mode, &fp);
                fwrite(newPack, cur, 1, (FILE *)fp);
                delete[] newPack;
                DB_Close(fp);
            }
            return 0;
        }
        else
        {
            for (auto &file : selectedFiles)
            {
                long len;
                if (file.first.find(".idbzip") != string::npos)
                {
                    DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                }
                else if (file.first.find("idb") != string::npos)
                {
                    DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                }
                char *buff = new char[CurrentTemplate.totalBytes];
                DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
                if (file.first.find(".idbzip") != string::npos)
                {
                    ReZipBuff(&buff, (int &)len, params->pathToLine);
                }
                //获取数据的偏移量和数据类型
                long pos = 0, bytes = 0;
                DataType type;
                long compareBytes = 0;
                int err = params->byPath ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, compareBytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, compareBytes, type);
                if (err != 0)
                {
                    return err;
                }
                bool canDelete = false;
                if (compareBytes != 0) //可比较
                {
                    char value[compareBytes]; //数值
                    memcpy(value, buff + pos, compareBytes);
                    //根据比较结果决定是否加入结果集
                    int compareRes = DataType::CompareValue(type, value, params->compareValue);
                    switch (params->compareType)
                    {
                    case DB_CompareType::GT:
                    {
                        if (compareRes > 0)
                        {
                            canDelete = true;
                        }
                        break;
                    }
                    case DB_CompareType::LT:
                    {
                        if (compareRes < 0)
                        {
                            canDelete = true;
                        }
                        break;
                    }
                    case DB_CompareType::GE:
                    {
                        if (compareRes >= 0)
                        {
                            canDelete = true;
                        }
                        break;
                    }
                    case DB_CompareType::LE:
                    {
                        if (compareRes <= 0)
                        {
                            canDelete = true;
                        }
                        break;
                    }
                    case DB_CompareType::EQ:
                    {
                        if (compareRes == 0)
                        {
                            canDelete = true;
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }
                if (canDelete)
                {
                    DB_DeleteFile(const_cast<char *>(file.first.c_str()));
                }
                delete[] buff;
            }

            /**
             * 对于包文件的处理，与前述类似，不拷贝满足条件的数据
             * 仅将其余数据写成一个新的包
             */
            for (auto &pack : selectedPacks)
            {
                if (std::get<0>(pack.second) >= params->start && std::get<1>(pack.second) <= params->end)
                {
                    packManager.DeletePack(pack.first);
                    continue;
                }
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                TemplateManager::CheckTemplate(templateName);
                int deleteNum = 0;
                //一次分配一整个pak长度的空间，避免频繁分配影响性能
                // shared_ptr<char> newPack = make_shared<char>(packReader.GetPackLength());
                char *newPack = new char[packReader.GetPackLength()];
                long cur = 4;
                memcpy(newPack + cur, templateName.c_str(), templateName.length() <= 20 ? templateName.length() : 20);
                cur += 20;
                bool packNameChange = false; //若删除的数据包含首尾文件，则包名的时间段需要修改
                for (int i = 0; i < fileNum; i++)
                {
                    long timestamp;
                    int readLength, zipType;
                    string fileID;
                    long dataPos = packReader.Next(readLength, timestamp, fileID, zipType);
                    if (timestamp >= params->start && timestamp <= params->end) //在时间区间内
                    {
                        if (timestamp == get<1>(pack.second) || timestamp == get<0>(pack.second))
                            packNameChange = true;
                        /**
                         * 在此决定是否保留此数据，首先从压缩数据中还原出原始数据，进行常规比对
                         * 若满足删除条件，则直接continue
                         */
                        char *buff = new char[CurrentTemplate.totalBytes];
                        switch (zipType)
                        {
                        case 0:
                        {
                            memcpy(buff, packReader.packBuffer + dataPos, readLength);
                            break;
                        }
                        case 1:
                        {
                            ReZipBuff(&buff, readLength, params->pathToLine);
                            break;
                        }
                        case 2:
                        {
                            memcpy(buff, packReader.packBuffer + dataPos, readLength);
                            ReZipBuff(&buff, readLength, params->pathToLine);
                            break;
                        }
                        default:
                            delete[] buff;
                            return StatusCode::UNKNWON_DATAFILE;
                            break;
                        }
                        //获取数据的偏移量和数据类型
                        long pos = 0, bytes = 0;
                        DataType type;
                        long compareBytes = 0;
                        int err = params->byPath ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, compareBytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, compareBytes, type);
                        if (err != 0)
                        {
                            return err;
                        }
                        bool canDelete = false;
                        if (compareBytes != 0) //可比较
                        {
                            char value[compareBytes]; //数值
                            memcpy(value, buff + pos, compareBytes);
                            //根据比较结果决定是否加入结果集
                            int compareRes = DataType::CompareValue(type, value, params->compareValue);
                            switch (params->compareType)
                            {
                            case DB_CompareType::GT:
                            {
                                if (compareRes > 0)
                                {
                                    canDelete = true;
                                }
                                break;
                            }
                            case DB_CompareType::LT:
                            {
                                if (compareRes < 0)
                                {
                                    canDelete = true;
                                }
                                break;
                            }
                            case DB_CompareType::GE:
                            {
                                if (compareRes >= 0)
                                {
                                    canDelete = true;
                                }
                                break;
                            }
                            case DB_CompareType::LE:
                            {
                                if (compareRes <= 0)
                                {
                                    canDelete = true;
                                }
                                break;
                            }
                            case DB_CompareType::EQ:
                            {
                                if (compareRes == 0)
                                {
                                    canDelete = true;
                                }
                                break;
                            }
                            default:
                                break;
                            }
                        }
                        if (canDelete)
                        {
                            deleteNum++;
                            delete[] buff;
                            continue;
                        }
                        delete[] buff;
                    }
                    if (zipType != 1) //非完全压缩
                    {
                        memcpy(newPack + cur, packReader.packBuffer + dataPos - 33, 33 + readLength);
                        cur += readLength + 33;
                    }
                    else
                    {
                        memcpy(newPack + cur, packReader.packBuffer + dataPos - 29, 29);
                        cur += 29;
                    }
                }
                int newFileNum = fileNum - deleteNum;
                memcpy(newPack, &newFileNum, 4);

                if (packNameChange)
                {
                    long newpackStart, newpackEnd;
                    int readLength, zipType;
                    free(packReader.packBuffer);
                    packReader.packBuffer = newPack;
                    packReader.SetCurPos(24);
                    packReader.Next(readLength, newpackStart, zipType);
                    packReader.Skip(newFileNum - 2);
                    packReader.Next(readLength, newpackEnd, zipType);
                    auto vec = DataType::splitWithStl(pack.first, "/");
                    string newPackName = vec[0] + "/" + to_string(newpackStart) + "-" + to_string(newpackEnd) + ".pak";
                    char mode[2] = {'w', 'b'};
                    long fp;
                    DB_Open(const_cast<char *>(newPackName.c_str()), mode, &fp);
                    fwrite(newPack, cur, 1, (FILE *)fp);
                    // delete[] newPack;
                    // newPack = NULL;
                    DB_Close(fp);
                    DB_DeleteFile(const_cast<char *>(pack.first.c_str()));
                    continue;
                }
                char mode[2] = {'w', 'b'};
                long fp;
                DB_Open(const_cast<char *>(pack.first.c_str()), mode, &fp);
                fwrite(newPack, cur, 1, (FILE *)fp);
                delete[] newPack;
                DB_Close(fp);
            }
        }
        return 0;
        break;
    }
    case LAST:
    {
        /**
         * 删除最新条数的结构与TIMESPAN类似，若无需比较数值，直接删除，否则读取文件筛选
         * 此处默认普通文件的时序在后，即最新的数据均为idb、idbzip类型
         * 因此优先删除普通文件，数量不足时再从pak文件中删除
         */
        vector<string> packsList;
        vector<pair<string, long>> filesWithTime, selectedFiles, packsWithTime;
        readDataFilesWithTimestamps(params->pathToLine, filesWithTime);
        //确认当前模版
        string str = params->pathToLine;
        if (CurrentTemplate.path != str || CurrentTemplate.path == "")
        {
            int err = 0;
            err = DB_LoadSchema(params->pathToLine);
            if (err != 0)
            {
                return StatusCode::TEMPLATE_RESOLUTION_ERROR;
            }
        }

        sortByTime(filesWithTime, TIME_DSC);

        int selectedNum = 0;
        if (params->compareType == CMP_NONE)
        {
            for (auto &file : filesWithTime)
            {
                DB_DeleteFile(const_cast<char *>(file.first.c_str()));
                selectedNum++;
                if (selectedNum == params->queryNums)
                    return 0;
            }
            readPakFilesList(params->pathToLine, packsList);
            for (auto &pack : packsList)
            {
                string tmp = pack;
                while (tmp.back() == '/')
                    tmp.pop_back();
                vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
                string packName = vec[vec.size() - 1];
                vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
                if (timespan.size() > 0)
                {
                    long start = atol(timespan[0].c_str());
                    packsWithTime.push_back(make_pair(pack, start));
                }
                else
                {
                    return StatusCode::FILENAME_MODIFIED;
                }
            }
            sortByTime(packsWithTime, TIME_DSC);
            for (auto &pack : packsWithTime)
            {
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                //若此包中文件数量不大于还要删除的文件数量，直接将其删除
                if (fileNum <= params->queryNums - selectedNum)
                {
                    packManager.DeletePack(pack.first);
                    selectedNum += fileNum;
                    if (selectedNum == params->queryNums)
                        return 0;
                    continue;
                }
                /**
                 * 到这里，还要删除的文件数量一定小于fileNum，而pak中的文件按时间升序存放，
                 * 因此直接拷贝前fileNum - (params->queryNums - selectedNum)个文件的数据
                 * 将其写成一个新的pak文件
                 */
                char *newPack = new char[packReader.GetPackLength()];
                long timestamp;
                int readLength, zipType;
                long dataPos;
                packReader.Skip(fileNum - (params->queryNums - selectedNum) - 1);
                dataPos = packReader.Next(readLength, timestamp, zipType);

                memcpy(newPack, packReader.packBuffer, dataPos + readLength);
                int newNum = fileNum - params->queryNums + selectedNum;
                memcpy(newPack, &newNum, 4); //覆写新的文件个数
                auto vec = DataType::splitWithStl(pack.first, "/");
                string newPackName = vec[0] + "/" + to_string(pack.second) + "-" + to_string(timestamp) + ".pak";
                long fp;
                char mode[2] = {'w', 'b'};
                DB_Open(const_cast<char *>(newPackName.c_str()), mode, &fp);
                fwrite(newPack, dataPos + readLength, 1, (FILE *)fp);
                delete[] newPack;
                DB_Close(fp);
                DB_DeleteFile(const_cast<char *>(pack.first.c_str()));
                return 0;
            }
        }
        else
        {
            for (auto &file : filesWithTime)
            {
                long len;
                if (file.first.find(".idbzip") != string::npos)
                {
                    DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                }
                else if (file.first.find("idb") != string::npos)
                {
                    DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                }
                char *buff = new char[CurrentTemplate.totalBytes];
                DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
                if (file.first.find(".idbzip") != string::npos)
                {
                    ReZipBuff(&buff, (int &)len, params->pathToLine);
                }

                //获取数据的偏移量和数据类型
                long pos = 0, bytes = 0;
                DataType type;
                long compareBytes = 0;
                int err = params->byPath ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, compareBytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, compareBytes, type);
                if (err != 0)
                {
                    return err;
                }
                bool canDelete = false;
                if (compareBytes != 0) //可比较
                {
                    char value[compareBytes]; //数值
                    memcpy(value, buff + pos, compareBytes);
                    //根据比较结果决定是否加入结果集
                    int compareRes = DataType::CompareValue(type, value, params->compareValue);
                    switch (params->compareType)
                    {
                    case DB_CompareType::GT:
                    {
                        if (compareRes > 0)
                        {
                            canDelete = true;
                        }
                        break;
                    }
                    case DB_CompareType::LT:
                    {
                        if (compareRes < 0)
                        {
                            canDelete = true;
                        }
                        break;
                    }
                    case DB_CompareType::GE:
                    {
                        if (compareRes >= 0)
                        {
                            canDelete = true;
                        }
                        break;
                    }
                    case DB_CompareType::LE:
                    {
                        if (compareRes <= 0)
                        {
                            canDelete = true;
                        }
                        break;
                    }
                    case DB_CompareType::EQ:
                    {
                        if (compareRes == 0)
                        {
                            canDelete = true;
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }
                if (canDelete)
                {
                    DB_DeleteFile(const_cast<char *>(file.first.c_str()));
                    selectedNum++;
                }
                delete[] buff;
                if (selectedNum == params->queryNums)
                    return 0;
            }

            readPakFilesList(params->pathToLine, packsList);
            for (auto &pack : packsList)
            {
                string tmp = pack;
                while (tmp.back() == '/')
                    tmp.pop_back();
                vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
                string packName = vec[vec.size() - 1];
                vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
                if (timespan.size() > 0)
                {
                    long start = atol(timespan[0].c_str());
                    packsWithTime.push_back(make_pair(pack, start));
                }
                else
                {
                    return StatusCode::FILENAME_MODIFIED;
                }
            }
            sortByTime(packsWithTime, TIME_DSC);
            bool deleteComplete = false;
            for (auto &pack : packsWithTime)
            {
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                TemplateManager::CheckTemplate(templateName);
                int deleteNum = 0; //此包中已删除文件
                //一次分配一整个pak长度的空间，避免频繁分配影响性能
                // shared_ptr<char> newPack = make_shared<char>(packReader.GetPackLength());
                char *newPack = new char[packReader.GetPackLength()];
                long cur = 4;
                memcpy(newPack + cur, templateName.c_str(), templateName.length() <= 20 ? templateName.length() : 20);
                cur += 20;
                stack<pair<long, tuple<int, long, string, int>>> filestk;
                /**
                 * 比对数据时使用filestk使数据时间降序地弹出，将不满足删除条件的数据压入writeStk
                 * 最后写入时从writeStk中弹出的数据即为时间升序型
                 */
                stack<pair<long, tuple<int, long, string, int>>> writeStk;
                for (int i = 0; i < fileNum; i++)
                {
                    long timestamp; //暂时用不到时间戳
                    int readLength, zipType;
                    string fileID;
                    long dataPos = packReader.Next(readLength, timestamp, fileID, zipType);
                    auto t = make_tuple(readLength, timestamp, fileID, zipType);
                    filestk.push(make_pair(dataPos, t));
                }
                DataType type;
                while (!filestk.empty())
                {
                    if (deleteComplete) //删除已完成，直接拷贝剩余数据
                    {
                        /**
                         * @brief 至此已无需删除数据，此时filestk的顶部为时序最后的文件信息，
                         * 因此可以直接从包的头部拷贝至此文件
                         */
                        auto preInfo = filestk.top();
                        long preDataPos = preInfo.first;
                        int preReadLength = get<0>(preInfo.second);
                        int preZipType = get<3>(preInfo.second);
                        memcpy(newPack + cur, packReader.packBuffer + 24, preDataPos);
                        cur = preDataPos;
                        if (preZipType != 1) //非完全压缩
                        {
                            memcpy(newPack + cur, packReader.packBuffer + preDataPos - 4, preReadLength + 4);
                            cur += preReadLength + 4;
                        }
                        auto fileInfo = writeStk.top();
                        long newpackEnd;
                        while (!writeStk.empty())
                        {
                            fileInfo = writeStk.top();
                            long dataPos = fileInfo.first;
                            int readLength = get<0>(fileInfo.second);
                            int zipType = get<3>(fileInfo.second);
                            memcpy(newPack + cur, packReader.packBuffer + dataPos - 29, 29);
                            if (zipType != 1) //非完全压缩
                            {
                                memcpy(newPack + cur, packReader.packBuffer + dataPos - 33, 33 + readLength);
                                cur += readLength + 33;
                            }
                            else
                            {
                                memcpy(newPack + cur, packReader.packBuffer + dataPos - 29, 29);
                                cur += 29;
                            }
                            writeStk.pop();
                        }
                        memcpy(&newpackEnd, packReader.packBuffer + fileInfo.first - (get<3>(fileInfo.second) == 1 ? 29 : 33), 8);
                        int newFileNum = fileNum - deleteNum;
                        memcpy(newPack, &newFileNum, 4);
                        char mode[2] = {'w', 'b'};
                        long fp;
                        auto vec = DataType::splitWithStl(pack.first, "/");
                        string newPackName = vec[0] + "/" + to_string(pack.second) + "-" + to_string(newpackEnd) + ".pak";
                        DB_Open(const_cast<char *>(newPackName.c_str()), mode, &fp);
                        fwrite(newPack, cur, 1, (FILE *)fp);
                        delete[] newPack;
                        DB_Close(fp);
                        DB_DeleteFile(const_cast<char *>(pack.first.c_str()));
                        return 0;
                    }
                    auto fileInfo = filestk.top();
                    filestk.pop();
                    long dataPos = fileInfo.first;
                    int readLength = get<0>(fileInfo.second);
                    long timestamp = get<1>(fileInfo.second);
                    string fileID = get<2>(fileInfo.second);
                    int zipType = get<3>(fileInfo.second);

                    char *buff = new char[CurrentTemplate.totalBytes];
                    switch (zipType)
                    {
                    case 0:
                    {
                        memcpy(buff, packReader.packBuffer + dataPos, readLength);
                        break;
                    }
                    case 1:
                    {
                        ReZipBuff(&buff, readLength, params->pathToLine);
                        break;
                    }
                    case 2:
                    {
                        memcpy(buff, packReader.packBuffer + dataPos, readLength);
                        ReZipBuff(&buff, readLength, params->pathToLine);
                        break;
                    }
                    default:
                    {
                        delete[] buff;
                        return StatusCode::UNKNWON_DATAFILE;
                        break;
                    }
                    }
                    //获取数据的偏移量和字节数
                    long bytes = 0, pos = 0; //单个变量
                    long compareBytes = 0;
                    int error = params->byPath ? CurrentTemplate.FindDatatypePosByCode(params->pathCode, buff, pos, compareBytes, type) : CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, compareBytes, type);
                    if (error != 0)
                    {
                        return error;
                    }
                    bool canDelete = false;
                    if (compareBytes != 0) //可比较
                    {
                        char value[compareBytes]; //数值
                        memcpy(value, buff + pos, compareBytes);
                        //根据比较结果决定是否加入结果集
                        int compareRes = DataType::CompareValue(type, value, params->compareValue);
                        switch (params->compareType)
                        {
                        case DB_CompareType::GT:
                        {
                            if (compareRes > 0)
                            {
                                canDelete = true;
                            }
                            break;
                        }
                        case DB_CompareType::LT:
                        {
                            if (compareRes < 0)
                            {
                                canDelete = true;
                            }
                            break;
                        }
                        case DB_CompareType::GE:
                        {
                            if (compareRes >= 0)
                            {
                                canDelete = true;
                            }
                            break;
                        }
                        case DB_CompareType::LE:
                        {
                            if (compareRes <= 0)
                            {
                                canDelete = true;
                            }
                            break;
                        }
                        case DB_CompareType::EQ:
                        {
                            if (compareRes == 0)
                            {
                                canDelete = true;
                            }
                            break;
                        }
                        default:
                            break;
                        }
                    }
                    if (canDelete)
                    {
                        deleteNum++;
                        selectedNum++;
                        delete[] buff;
                        if (selectedNum == params->queryNums)
                            deleteComplete = true;
                        continue;
                    }
                    delete[] buff;
                    writeStk.push(fileInfo);
                }
                //运行到这里，代表此包中的数据已比对完毕，但还未达到删除条数
                int newFileNum = fileNum - deleteNum;
                memcpy(newPack, &newFileNum, 4);
                long newpackStart, newpackEnd;
                if (!writeStk.empty())
                {
                    auto fileInfo = writeStk.top();
                    writeStk.pop();
                    newpackStart = get<1>(fileInfo.second);
                    newpackEnd = newpackStart; //避免writeStk中只有一个元素的情况下newpackEnd为空
                }
                while (!writeStk.empty())
                {
                    auto fileInfo = writeStk.top();
                    long dataPos = fileInfo.first;
                    int readLength = get<0>(fileInfo.second);
                    int zipType = get<3>(fileInfo.second);
                    memcpy(newPack + cur, packReader.packBuffer + dataPos - 29, 29);
                    cur += 29;
                    if (zipType != 1) //非完全压缩
                    {
                        memcpy(newPack + cur, packReader.packBuffer + dataPos - 4, readLength + 4);
                        cur += readLength + 4;
                    }
                    writeStk.pop();
                    if (writeStk.empty())
                    {
                        newpackEnd = get<1>(fileInfo.second);
                    }
                }
                char mode[2] = {'w', 'b'};
                long fp;
                auto vec = DataType::splitWithStl(pack.first, "/");
                string newPackName = vec[0] + "/" + to_string(newpackStart) + "-" + to_string(newpackEnd) + ".pak";
                DB_Open(const_cast<char *>(newPackName.c_str()), mode, &fp);
                fwrite(newPack, cur, 1, (FILE *)fp);
                delete[] newPack;
                DB_Close(fp);
            }
        }
        break;
    }
    case FILEID:
    {
        string pathToLine = params->pathToLine;
        string fileid = params->fileID;
        while (pathToLine[pathToLine.length() - 1] == '/')
        {
            pathToLine.pop_back();
        }

        vector<string> paths = DataType::splitWithStl(pathToLine, "/");
        if (paths.size() > 0)
        {
            if (fileid.find(paths[paths.size() - 1]) == string::npos)
                fileid = paths[paths.size() - 1] + fileid;
        }
        else
        {
            if (fileid.find(paths[0]) == string::npos)
                fileid = paths[0] + fileid;
        }
        vector<string> packsList, dataFiles;
        readDataFiles(params->pathToLine, dataFiles);
        readPakFilesList(params->pathToLine, packsList);
        for (auto &file : dataFiles)
        {
            if (file.find(fileid) != string::npos)
                return DB_DeleteFile(const_cast<char *>(file.c_str()));
        }
        for (auto &pack : packsList)
        {
            PackFileReader packReader(pack);
            if (packReader.packBuffer == NULL)
                continue;
            int fileNum;
            string templateName;
            packReader.ReadPackHead(fileNum, templateName);
            for (int i = 0; i < fileNum; i++)
            {
                string fid;
                int readLength, zipType;
                long timestamp;
                long dataPos = packReader.Next(readLength, timestamp, fid, zipType);
                if (fid.find(fileid) != string::npos) //剔除这部分数据
                {
                    if (fileNum == 1)
                        packManager.DeletePack(pack);
                    char *newPack = new char[packReader.GetPackLength()];
                    int newNum = fileNum - 1;
                    memcpy(newPack, packReader.packBuffer, dataPos - 33);                                                                              //拷贝此文件前的数据
                    memcpy(newPack, &newNum, 4);                                                                                                       //覆写文件总数
                    memcpy(newPack + dataPos - 33, packReader.packBuffer + dataPos + readLength, packReader.GetPackLength() - (dataPos + readLength)); //拷贝此文件后的数据
                    long fp;
                    char mode[2] = {'w', 'b'};
                    DB_Open(const_cast<char *>(pack.c_str()), mode, &fp);
                    fwrite(newPack, packReader.GetPackLength() - 28 - readLength, 1, (FILE *)fp);
                    delete[] newPack;
                    return DB_Close(fp);
                }
            }
        }
        return 0;
        break;
    }
    default:
        break;
    }
    return StatusCode::NO_QUERY_TYPE;
}

/**
 * @brief 根据自定义查询请求参数，删除符合条件的记录
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  删除包中文件后需要修改此包总文件数，重新打包，修改包文件名
 */
int DB_DeleteRecords_New(DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;
    if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
    {
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    }
    switch (params->queryType)
    {
    case TIMESPAN:
    {
        vector<string> packsList;
        vector<pair<string, long>> filesWithTime, selectedFiles;
        vector<pair<string, tuple<long, long>>> selectedPacks;
        readDataFilesWithTimestamps(params->pathToLine, filesWithTime);
        readPakFilesList(params->pathToLine, packsList);
        //筛选落入时间区间内的文件
        for (auto &file : filesWithTime)
        {
            if (file.second >= params->start && file.second <= params->end)
            {
                selectedFiles.push_back(make_pair(file.first, file.second));
            }
        }
        for (auto &pack : packsList)
        {
            string tmp = pack;
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
                    selectedPacks.push_back(make_pair(pack, make_tuple(start, end)));
                }
            }
        }
        if (params->compareType == CMP_NONE) //不需要比较数值，因此无需读取普通文件增加I/O耗时
        {
            for (auto &file : selectedFiles)
            {
                DB_DeleteFile(const_cast<char *>(file.first.c_str()));
            }
            /**
             *  对于包文件，若其起始时间和结束时间均在范围内，则直接删除，
             *  否则，将包内的范围外的数据拷出后写成新的文件，将旧文件覆盖
             */
            for (auto &pack : selectedPacks)
            {
                if (std::get<0>(pack.second) >= params->start && std::get<1>(pack.second) <= params->end)
                {
                    packManager.DeletePack(pack.first);
                    continue;
                }
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                int deleteNum = 0;
                //一次分配一整个pak长度的空间，避免频繁分配影响性能
                char *newPack = (char *)malloc(packReader.GetPackLength());
                long cur = 4;
                memcpy(newPack + cur, templateName.c_str(), templateName.length() <= 20 ? templateName.length() : 20);
                cur += 20;
                bool packNameChange = false; //若删除的数据包含首尾文件，则包名的时间段需要修改
                for (int i = 0; i < fileNum; i++)
                {
                    long timestamp;
                    int readLength, zipType;
                    string fileID;
                    long dataPos = packReader.Next(readLength, timestamp, fileID, zipType);
                    if (timestamp >= params->start && timestamp <= params->end) //在时间区间内
                    {
                        deleteNum++;
                        if (timestamp == get<1>(pack.second) || timestamp == get<0>(pack.second))
                            packNameChange = true;
                        continue;
                    }
                    if (zipType != 1) //非完全压缩
                    {
                        memcpy(newPack + cur, packReader.packBuffer + dataPos - 33, 33 + readLength);
                        cur += readLength + 33;
                    }
                    else
                    {
                        memcpy(newPack + cur, packReader.packBuffer + dataPos - 29, 29);
                        cur += 29;
                    }
                }
                int newFileNum = fileNum - deleteNum;
                memcpy(newPack, &newFileNum, 4);

                if (packNameChange)
                {
                    long newpackStart, newpackEnd;
                    int readLength, zipType;
                    free(packReader.packBuffer);
                    packReader.packBuffer = newPack;
                    packReader.SetCurPos(24);
                    packReader.Next(readLength, newpackStart, zipType);
                    packReader.Skip(newFileNum - 2);
                    packReader.Next(readLength, newpackEnd, zipType);
                    auto vec = DataType::splitWithStl(pack.first, "/");
                    string newPackName = vec[0] + "/" + to_string(newpackStart) + "-" + to_string(newpackEnd) + ".pak";
                    char mode[2] = {'w', 'b'};
                    long fp;
                    DB_Open(const_cast<char *>(newPackName.c_str()), mode, &fp);
                    fwrite(newPack, cur, 1, (FILE *)fp);
                    // delete[] newPack;
                    // newPack = NULL;
                    DB_Close(fp);
                    DB_DeleteFile(const_cast<char *>(pack.first.c_str()));
                    continue;
                }
                char mode[2] = {'w', 'b'};
                long fp;
                DB_Open(const_cast<char *>(pack.first.c_str()), mode, &fp);
                fwrite(newPack, cur, 1, (FILE *)fp);
                free(newPack);
                DB_Close(fp);
            }
            return 0;
        }
        else
        {
            for (auto &file : selectedFiles)
            {
                long len;
                if (file.first.find(".idbzip") != string::npos)
                {
                    DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                }
                else if (file.first.find("idb") != string::npos)
                {
                    DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                }
                char *buff = new char[len];
                DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
                if (file.first.find(".idbzip") != string::npos)
                {
                    ReZipBuff(&buff, (int &)len, params->pathToLine);
                }

                if (CanDelete(params, buff))
                {
                    DB_DeleteFile(const_cast<char *>(file.first.c_str()));
                }
                delete[] buff;
            }

            /**
             * 对于包文件的处理，与前述类似，不拷贝满足条件的数据
             * 仅将其余数据写成一个新的包
             */
            for (auto &pack : selectedPacks)
            {
                if (std::get<0>(pack.second) >= params->start && std::get<1>(pack.second) <= params->end)
                {
                    packManager.DeletePack(pack.first);
                    continue;
                }
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                int deleteNum = 0;
                //一次分配一整个pak长度的空间，避免频繁分配影响性能
                // shared_ptr<char> newPack = make_shared<char>(packReader.GetPackLength());
                char *newPack = new char[packReader.GetPackLength()];
                long cur = 4;
                memcpy(newPack + cur, templateName.c_str(), templateName.length() <= 20 ? templateName.length() : 20);
                cur += 20;
                bool packNameChange = false; //若删除的数据包含首尾文件，则包名的时间段需要修改
                for (int i = 0; i < fileNum; i++)
                {
                    long timestamp;
                    int readLength, zipType;
                    string fileID;
                    long dataPos = packReader.Next(readLength, timestamp, fileID, zipType);
                    if (timestamp >= params->start && timestamp <= params->end) //在时间区间内
                    {
                        if (timestamp == get<1>(pack.second) || timestamp == get<0>(pack.second))
                            packNameChange = true;
                        /**
                         * 在此决定是否保留此数据，首先从压缩数据中还原出原始数据，进行常规比对
                         * 若满足删除条件，则直接continue
                         */
                        char *buff;
                        switch (zipType)
                        {
                        case 0:
                        {
                            // memcpy(buff, packReader.packBuffer + dataPos, readLength);
                            buff = packReader.packBuffer + dataPos;
                            break;
                        }
                        case 1:
                        {
                            buff = new char[CurrentTemplate.totalBytes];
                            ReZipBuff(&buff, readLength, params->pathToLine);
                            break;
                        }
                        case 2:
                        {
                            buff = new char[CurrentTemplate.totalBytes];
                            memcpy(buff, packReader.packBuffer + dataPos, readLength);
                            ReZipBuff(&buff, readLength, params->pathToLine);
                            break;
                        }
                        default:
                            return StatusCode::UNKNWON_DATAFILE;
                            break;
                        }

                        if (CanDelete(params, buff))
                        {
                            deleteNum++;
                            if (zipType != 0)
                                delete[] buff;
                            continue;
                        }
                        if (zipType != 0)
                            delete[] buff;
                    }
                    if (zipType != 1) //非完全压缩
                    {
                        memcpy(newPack + cur, packReader.packBuffer + dataPos - 33, 33 + readLength);
                        cur += readLength + 33;
                    }
                    else
                    {
                        memcpy(newPack + cur, packReader.packBuffer + dataPos - 29, 29);
                        cur += 29;
                    }
                }
                int newFileNum = fileNum - deleteNum;
                memcpy(newPack, &newFileNum, 4);

                if (packNameChange)
                {
                    long newpackStart, newpackEnd;
                    int readLength, zipType;
                    free(packReader.packBuffer);
                    packReader.packBuffer = newPack;
                    packReader.SetCurPos(24);
                    packReader.Next(readLength, newpackStart, zipType);
                    packReader.Skip(newFileNum - 2);
                    packReader.Next(readLength, newpackEnd, zipType);
                    auto vec = DataType::splitWithStl(pack.first, "/");
                    string newPackName = vec[0] + "/" + to_string(newpackStart) + "-" + to_string(newpackEnd) + ".pak";
                    char mode[2] = {'w', 'b'};
                    long fp;
                    DB_Open(const_cast<char *>(newPackName.c_str()), mode, &fp);
                    fwrite(newPack, cur, 1, (FILE *)fp);
                    delete[] newPack;
                    newPack = NULL;
                    DB_Close(fp);
                    DB_DeleteFile(const_cast<char *>(pack.first.c_str()));
                    continue;
                }
                char mode[2] = {'w', 'b'};
                long fp;
                DB_Open(const_cast<char *>(pack.first.c_str()), mode, &fp);
                fwrite(newPack, cur, 1, (FILE *)fp);
                delete[] newPack;
                DB_Close(fp);
            }
        }
        return 0;
        break;
    }
    case LAST:
    {
        /**
         * 删除最新条数的结构与TIMESPAN类似，若无需比较数值，直接删除，否则读取文件筛选
         * 此处默认普通文件的时序在后，即最新的数据均为idb、idbzip类型
         * 因此优先删除普通文件，数量不足时再从pak文件中删除
         */
        vector<string> packsList;
        vector<pair<string, long>> filesWithTime, selectedFiles, packsWithTime;
        readDataFilesWithTimestamps(params->pathToLine, filesWithTime);
        //确认当前模版
        string str = params->pathToLine;
        if (CurrentTemplate.path != str || CurrentTemplate.path == "")
        {
            int err = 0;
            err = DB_LoadSchema(params->pathToLine);
            if (err != 0)
            {
                return StatusCode::TEMPLATE_RESOLUTION_ERROR;
            }
        }

        sortByTime(filesWithTime, TIME_DSC);

        int selectedNum = 0;
        if (params->compareType == CMP_NONE)
        {
            for (auto &file : filesWithTime)
            {
                DB_DeleteFile(const_cast<char *>(file.first.c_str()));
                selectedNum++;
                if (selectedNum == params->queryNums)
                    return 0;
            }
            readPakFilesList(params->pathToLine, packsList);
            for (auto &pack : packsList)
            {
                string tmp = pack;
                while (tmp.back() == '/')
                    tmp.pop_back();
                vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
                string packName = vec[vec.size() - 1];
                vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
                if (timespan.size() > 0)
                {
                    long start = atol(timespan[0].c_str());
                    packsWithTime.push_back(make_pair(pack, start));
                }
                else
                {
                    return StatusCode::FILENAME_MODIFIED;
                }
            }
            sortByTime(packsWithTime, TIME_DSC);
            for (auto &pack : packsWithTime)
            {
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                //若此包中文件数量不大于还要删除的文件数量，直接将其删除
                if (fileNum <= params->queryNums - selectedNum)
                {
                    packManager.DeletePack(pack.first);
                    selectedNum += fileNum;
                    if (selectedNum == params->queryNums)
                        return 0;
                    continue;
                }
                /**
                 * 到这里，还要删除的文件数量一定小于fileNum，而pak中的文件按时间升序存放，
                 * 因此直接拷贝前fileNum - (params->queryNums - selectedNum)个文件的数据
                 * 将其写成一个新的pak文件
                 */
                char *newPack = new char[packReader.GetPackLength()];
                long timestamp;
                int readLength, zipType;
                long dataPos;
                packReader.Skip(fileNum - (params->queryNums - selectedNum) - 1);
                dataPos = packReader.Next(readLength, timestamp, zipType);

                memcpy(newPack, packReader.packBuffer, dataPos + readLength);
                int newNum = fileNum - params->queryNums + selectedNum;
                memcpy(newPack, &newNum, 4); //覆写新的文件个数
                auto vec = DataType::splitWithStl(pack.first, "/");
                string newPackName = vec[0] + "/" + to_string(pack.second) + "-" + to_string(timestamp) + ".pak";
                long fp;
                char mode[2] = {'w', 'b'};
                DB_Open(const_cast<char *>(newPackName.c_str()), mode, &fp);
                fwrite(newPack, dataPos + readLength, 1, (FILE *)fp);
                delete[] newPack;
                DB_Close(fp);
                DB_DeleteFile(const_cast<char *>(pack.first.c_str()));
                return 0;
            }
        }
        else
        {
            for (auto &file : filesWithTime)
            {
                long len;
                if (file.first.find(".idbzip") != string::npos)
                {
                    DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                }
                else if (file.first.find("idb") != string::npos)
                {
                    DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                }
                char *buff = new char[len];
                DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
                if (file.first.find(".idbzip") != string::npos)
                {
                    ReZipBuff(&buff, (int &)len, params->pathToLine);
                }

                if (CanDelete(params, buff))
                {
                    DB_DeleteFile(const_cast<char *>(file.first.c_str()));
                    selectedNum++;
                }
                delete[] buff;
                if (selectedNum == params->queryNums)
                    return 0;
            }

            readPakFilesList(params->pathToLine, packsList);
            for (auto &pack : packsList)
            {
                string tmp = pack;
                while (tmp.back() == '/')
                    tmp.pop_back();
                vector<string> vec = DataType::StringSplit(const_cast<char *>(tmp.c_str()), "/");
                string packName = vec[vec.size() - 1];
                vector<string> timespan = DataType::StringSplit(const_cast<char *>(packName.c_str()), "-");
                if (timespan.size() > 0)
                {
                    long start = atol(timespan[0].c_str());
                    packsWithTime.push_back(make_pair(pack, start));
                }
                else
                {
                    return StatusCode::FILENAME_MODIFIED;
                }
            }
            sortByTime(packsWithTime, TIME_DSC);
            bool deleteComplete = false;
            for (auto &pack : packsWithTime)
            {
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                int deleteNum = 0; //此包中已删除文件
                //一次分配一整个pak长度的空间，避免频繁分配影响性能
                // shared_ptr<char> newPack = make_shared<char>(packReader.GetPackLength());
                char *newPack = new char[packReader.GetPackLength()];
                long cur = 4;
                memcpy(newPack + cur, templateName.c_str(), templateName.length() <= 20 ? templateName.length() : 20);
                cur += 20;
                stack<pair<long, tuple<int, long, string, int>>> filestk;
                /**
                 * 比对数据时使用filestk使数据时间降序地弹出，将不满足删除条件的数据压入writeStk
                 * 最后写入时从writeStk中弹出的数据即为时间升序型
                 */
                stack<pair<long, tuple<int, long, string, int>>> writeStk;
                for (int i = 0; i < fileNum; i++)
                {
                    long timestamp; //暂时用不到时间戳
                    int readLength, zipType;
                    string fileID;
                    long dataPos = packReader.Next(readLength, timestamp, fileID, zipType);
                    auto t = make_tuple(readLength, timestamp, fileID, zipType);
                    filestk.push(make_pair(dataPos, t));
                }
                DataType type;
                while (!filestk.empty())
                {
                    if (deleteComplete) //删除已完成，直接拷贝剩余数据
                    {
                        /**
                         * @brief 至此已无需删除数据，此时filestk的顶部为时序最后的文件信息，
                         * 因此可以直接从包的头部拷贝至此文件
                         */
                        auto preInfo = filestk.top();
                        long preDataPos = preInfo.first;
                        int preReadLength = get<0>(preInfo.second);
                        int preZipType = get<3>(preInfo.second);
                        memcpy(newPack + cur, packReader.packBuffer + 24, preDataPos);
                        cur = preDataPos;
                        if (preZipType != 1) //非完全压缩
                        {
                            memcpy(newPack + cur, packReader.packBuffer + preDataPos - 4, preReadLength + 4);
                            cur += preReadLength + 4;
                        }
                        auto fileInfo = writeStk.top();
                        long newpackEnd;
                        while (!writeStk.empty())
                        {
                            fileInfo = writeStk.top();
                            long dataPos = fileInfo.first;
                            int readLength = get<0>(fileInfo.second);
                            int zipType = get<3>(fileInfo.second);
                            memcpy(newPack + cur, packReader.packBuffer + dataPos - 29, 29);
                            if (zipType != 1) //非完全压缩
                            {
                                memcpy(newPack + cur, packReader.packBuffer + dataPos - 33, 33 + readLength);
                                cur += readLength + 33;
                            }
                            else
                            {
                                memcpy(newPack + cur, packReader.packBuffer + dataPos - 29, 29);
                                cur += 29;
                            }
                            writeStk.pop();
                        }
                        memcpy(&newpackEnd, packReader.packBuffer + fileInfo.first - (get<3>(fileInfo.second) == 1 ? 29 : 33), 8);
                        int newFileNum = fileNum - deleteNum;
                        memcpy(newPack, &newFileNum, 4);
                        char mode[2] = {'w', 'b'};
                        long fp;
                        auto vec = DataType::splitWithStl(pack.first, "/");
                        string newPackName = vec[0] + "/" + to_string(pack.second) + "-" + to_string(newpackEnd) + ".pak";
                        DB_Open(const_cast<char *>(newPackName.c_str()), mode, &fp);
                        fwrite(newPack, cur, 1, (FILE *)fp);
                        delete[] newPack;
                        DB_Close(fp);
                        DB_DeleteFile(const_cast<char *>(pack.first.c_str()));
                        return 0;
                    }
                    auto fileInfo = filestk.top();
                    filestk.pop();
                    long dataPos = fileInfo.first;
                    int readLength = get<0>(fileInfo.second);
                    long timestamp = get<1>(fileInfo.second);
                    string fileID = get<2>(fileInfo.second);
                    int zipType = get<3>(fileInfo.second);

                    char *buff;
                    switch (zipType)
                    {
                    case 0:
                    {
                        // memcpy(buff, packReader.packBuffer + dataPos, readLength);
                        buff = packReader.packBuffer + dataPos;
                        break;
                    }
                    case 1:
                    {
                        buff = new char[CurrentTemplate.totalBytes];
                        ReZipBuff(&buff, readLength, params->pathToLine);
                        break;
                    }
                    case 2:
                    {
                        buff = new char[CurrentTemplate.totalBytes];
                        memcpy(buff, packReader.packBuffer + dataPos, readLength);
                        ReZipBuff(&buff, readLength, params->pathToLine);
                        break;
                    }
                    default:
                    {
                        return StatusCode::UNKNWON_DATAFILE;
                        break;
                    }
                    }

                    if (CanDelete(params, buff))
                    {
                        deleteNum++;
                        selectedNum++;
                        if (zipType != 0)
                            delete[] buff;
                        if (selectedNum == params->queryNums)
                            deleteComplete = true;
                        continue;
                    }
                    if (zipType != 0)
                        delete[] buff;
                    writeStk.push(fileInfo);
                }
                //运行到这里，代表此包中的数据已比对完毕，但还未达到删除条数
                int newFileNum = fileNum - deleteNum;
                memcpy(newPack, &newFileNum, 4);
                long newpackStart, newpackEnd;
                if (!writeStk.empty())
                {
                    auto fileInfo = writeStk.top();
                    writeStk.pop();
                    newpackStart = get<1>(fileInfo.second);
                    newpackEnd = newpackStart; //避免writeStk中只有一个元素的情况下newpackEnd为空
                }
                while (!writeStk.empty())
                {
                    auto fileInfo = writeStk.top();
                    long dataPos = fileInfo.first;
                    int readLength = get<0>(fileInfo.second);
                    int zipType = get<3>(fileInfo.second);
                    memcpy(newPack + cur, packReader.packBuffer + dataPos - 29, 29);
                    cur += 29;
                    if (zipType != 1) //非完全压缩
                    {
                        memcpy(newPack + cur, packReader.packBuffer + dataPos - 4, readLength + 4);
                        cur += readLength + 4;
                    }
                    writeStk.pop();
                    if (writeStk.empty())
                    {
                        newpackEnd = get<1>(fileInfo.second);
                    }
                }
                char mode[2] = {'w', 'b'};
                long fp;
                auto vec = DataType::splitWithStl(pack.first, "/");
                string newPackName = vec[0] + "/" + to_string(newpackStart) + "-" + to_string(newpackEnd) + ".pak";
                DB_Open(const_cast<char *>(newPackName.c_str()), mode, &fp);
                fwrite(newPack, cur, 1, (FILE *)fp);
                delete[] newPack;
                DB_Close(fp);
            }
        }
        break;
    }
    case FILEID:
    {
        string pathToLine = params->pathToLine;
        string fileid = params->fileID;
        string fileidEnd = params->fileIDend == NULL ? "" : params->fileIDend;
        while (pathToLine[pathToLine.length() - 1] == '/')
        {
            pathToLine.pop_back();
        }

        vector<string> paths = DataType::splitWithStl(pathToLine, "/");
        if (paths.size() > 0)
        {
            if (fileid.find(paths[paths.size() - 1]) == string::npos)
                fileid = paths[paths.size() - 1] + fileid;
            if (params->fileIDend != NULL && fileidEnd.find(paths[paths.size() - 1]) == string::npos)
                fileidEnd = paths[paths.size() - 1] + fileidEnd;
        }
        else
        {
            if (fileid.find(paths[0]) == string::npos)
                fileid = paths[0] + fileid;
            if (params->fileIDend != NULL && fileidEnd.find(paths[0]) == string::npos)
                fileidEnd = paths[0] + fileidEnd;
        }

        //将数据处理为时间升序，即ID升序
        vector<string> packFiles;
        vector<pair<string, long>> dataFiles;
        readDataFilesWithTimestamps(params->pathToLine, dataFiles);
        sortByTime(dataFiles, TIME_ASC);

        bool firstIndexFound = false;
        string currentFileID;
        if (params->queryNums == 1 || params->queryNums == 0)
        {
            if (params->fileIDend == NULL) //单文件删除
            {
                auto pack = packManager.GetPackByID(params->pathToLine, fileid, 1);
                for (auto &file : dataFiles)
                {
                    if (file.first.find(fileid) != string::npos)
                        return DB_DeleteFile(const_cast<char *>(file.first.c_str()));
                }
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    return StatusCode::UNKNWON_DATAFILE;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                for (int i = 0; i < fileNum; i++)
                {
                    string fid;
                    int readLength, zipType;
                    long timestamp;
                    long dataPos = packReader.Next(readLength, timestamp, fid, zipType);
                    if (fid.find(fileid) != string::npos) //剔除这部分数据
                    {
                        if (fileNum == 1)
                            packManager.DeletePack(pack.first);
                        char *newPack = new char[packReader.GetPackLength()];
                        int newNum = fileNum - 1;
                        memcpy(newPack, packReader.packBuffer, dataPos - 33);                                                                              //拷贝此文件前的数据
                        memcpy(newPack, &newNum, 4);                                                                                                       //覆写文件总数
                        memcpy(newPack + dataPos - 33, packReader.packBuffer + dataPos + readLength, packReader.GetPackLength() - (dataPos + readLength)); //拷贝此文件后的数据
                        long fp;
                        char mode[2] = {'w', 'b'};
                        DB_Open(pack.first, mode, &fp);
                        fwrite(newPack, packReader.GetPackLength() - 28 - readLength, 1, (FILE *)fp);
                        delete[] newPack;
                        return DB_Close(fp);
                    }
                }
            }
            else //根据首尾ID的多文件删除
            {
                auto packs = packManager.GetPackByIDs(params->pathToLine, fileid, fileidEnd, 1);
                if (packs.size() > 2 && params->compareType == CMP_NONE) //当包的个数大于2 且无需比较数据时，首尾以外的包可直接删除
                {
                    for (int i = 1; i < packs.size() - 1; i++)
                    {
                        DB_DeleteFile(packs[i].first);
                        packManager.DeletePack(packs[i].first);
                    }
                }
                /**
                 * @brief 由于尾部一定是未被打包的idb文件，因此pak文件的ID较前，首先扫描pak
                 * 若在pak中扫描到最后的ID，则无需再扫描idb文件
                 */
                bool deleteComplete = false;
                for (auto &pack : packs)
                {
                    PackFileReader packReader(pack.first);
                    if (packReader.packBuffer == NULL)
                        continue;
                    int fileNum, deleteNum = 0;
                    string templateName;
                    packReader.ReadPackHead(fileNum, templateName);
                    char *newPack = (char *)malloc(packReader.GetPackLength());
                    long cur = 4;
                    memcpy(newPack + cur, templateName.c_str(), templateName.length() <= 20 ? templateName.length() : 20);
                    cur += 20;
                    bool packNameChange = false; //若删除的数据包含首尾文件，则包名的时间段需要修改
                    for (int i = 0; i < fileNum; i++)
                    {
                        int zipType, readLength;
                        long dataPos = packReader.Next(readLength, currentFileID, zipType);
                        if (fileid == currentFileID)
                        {
                            firstIndexFound = true;
                        }
                        /**
                         * @brief 与查询等操作相反，删除时仅保留不满足条件的数据，
                         * 因此在ID范围外的均保留
                         */

                        if (!firstIndexFound)
                        {
                            memcpy(newPack + cur, packReader.packBuffer + dataPos, readLength);
                            cur += readLength + (zipType == 1 ? 29 : 33);
                        }
                        else
                        {
                            if (params->compareType != CMP_NONE)
                            {
                                char *buff;
                                switch (zipType)
                                {
                                case 0:
                                    buff = packReader.packBuffer + dataPos;
                                    break;
                                case 1:
                                    buff = new char[readLength];
                                    ReZipBuff(&buff, readLength, params->pathToLine);
                                    break;
                                case 2:
                                    buff = new char[readLength];
                                    memcpy(buff, packReader.packBuffer + dataPos, readLength);
                                    ReZipBuff(&buff, readLength, params->pathToLine);
                                    break;
                                default:
                                    continue;
                                }
                                if (!CanDelete(params, buff)) //判定不满足删除条件，保留之
                                {
                                    memcpy(newPack + cur, packReader.packBuffer + dataPos - (zipType == 1 ? 29 : 33), readLength + (zipType == 1 ? 29 : 33));
                                    cur += readLength;
                                }
                                else
                                {
                                    deleteNum++;
                                    if (i == 0) //删除了第一个节拍，包名的时间段需要修改
                                        packNameChange = true;
                                }
                            }
                            else
                            {
                                deleteNum++;
                                if (i == 0) //删除了第一个节拍，包名的时间段需要修改
                                    packNameChange = true;
                            }
                        }
                        if (currentFileID == fileidEnd)
                        {
                            deleteComplete = true;
                            if (i == fileNum - 1)
                                packNameChange = true;
                            else //到此，对这个包的操作已经结束，将包中剩下的数据拼接上去即可
                            {
                                dataPos = packReader.Next(readLength, currentFileID, zipType);
                                memcpy(newPack + cur, packReader.packBuffer + dataPos - (zipType == 1 ? 29 : 33), packReader.GetPackLength() + (zipType == 1 ? 29 : 33) - dataPos);
                                cur += packReader.GetPackLength() + (zipType == 1 ? 29 : 33) - dataPos;
                                break;
                            }
                        }
                    }
                    int newFileNum = fileNum - deleteNum;
                    memcpy(newPack, &newFileNum, 4);
                    if (packNameChange)
                    {
                        long newpackStart, newpackEnd;
                        int readLength, zipType;
                        free(packReader.packBuffer);
                        packReader.packBuffer = newPack;
                        packReader.SetCurPos(24);
                        packReader.Next(readLength, newpackStart, zipType);
                        if (newFileNum >= 2)
                        {
                            packReader.Skip(newFileNum - 2);
                            packReader.Next(readLength, newpackEnd, zipType);
                        }
                        else
                            newpackEnd = newpackStart;
                        auto vec = DataType::splitWithStl(pack.first, "/");
                        string newPackName = vec[0] + "/" + to_string(newpackStart) + "-" + to_string(newpackEnd) + ".pak ";
                        char mode[2] = {'w', 'b'};
                        long fp;
                        DB_Open(const_cast<char *>(newPackName.c_str()), mode, &fp);
                        fwrite(newPack, cur, 1, (FILE *)fp);
                        DB_Close(fp);
                        DB_DeleteFile(pack.first);
                        continue;
                    }
                    char mode[2] = {'w', 'b'};
                    long fp;
                    DB_Open(pack.first, mode, &fp);
                    fwrite(newPack, cur, 1, (FILE *)fp);
                    free(newPack);
                    DB_Close(fp);
                }
                if (deleteComplete)
                    return 0;
                else
                {
                    for (auto &file : dataFiles)
                    {
                        string tmp = file.first;
                        vector<string> vec = DataType::splitWithStl(tmp, "/");
                        if (vec.size() == 0)
                            continue;
                        vec = DataType::splitWithStl(vec[vec.size() - 1], "_");
                        if (vec.size() == 0)
                            continue;
                        if (vec[0] == fileid)
                            firstIndexFound = true;
                        if (firstIndexFound)
                        {
                            currentFileID = file.first; //此处 currentFileID 为文件路径
                            long len;
                            DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                            char *buff = new char[len];
                            DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
                            if (file.first.find(".idbzip") != string::npos)
                                ReZipBuff(&buff, (int &)len, params->pathToLine);
                            if (CanDelete(params, buff))
                            {
                                DB_DeleteFile(const_cast<char *>(file.first.c_str()));
                            }
                            delete[] buff;
                        }
                        if (currentFileID.find(fileidEnd) != string::npos)
                            return 0;
                    }
                }
            }
        }
        //根据首ID+数量的多文件删除
        else
        {
            auto packs = packManager.GetPackByIDs(params->pathToLine, fileid, params->queryNums, 1);
            if (packs.size() > 2 && params->compareType == CMP_NONE) //当包的个数大于2 且无需比较数据时，首尾以外的包可直接删除
            {
                for (int i = 1; i < packs.size() - 1; i++)
                {
                    DB_DeleteFile(packs[i].first);
                    packManager.DeletePack(packs[i].first);
                }
            }
            bool deleteComplete = false;
            long scanNum = 0;
            for (auto &pack : packs)
            {
                if (scanNum == params->queryNums)
                    break;
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum, deleteNum = 0;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                char *newPack = (char *)malloc(packReader.GetPackLength());
                long cur = 4;
                memcpy(newPack + cur, templateName.c_str(), templateName.length() <= 20 ? templateName.length() : 20);
                cur += 20;
                bool packNameChange = false; //若删除的数据包含首尾文件，则包名的时间段需要修改
                for (int i = 0; i < fileNum; i++)
                {
                    int zipType, readLength;
                    long dataPos = packReader.Next(readLength, currentFileID, zipType);
                    if (fileid == currentFileID)
                    {
                        firstIndexFound = true;
                    }
                    if (!firstIndexFound)
                    {
                        memcpy(newPack + cur, packReader.packBuffer + dataPos, readLength);
                        cur += readLength + (zipType == 1 ? 29 : 33);
                    }
                    else
                    {
                        if (params->compareType != CMP_NONE)
                        {
                            char *buff;
                            switch (zipType)
                            {
                            case 0:
                                buff = packReader.packBuffer + dataPos;
                                break;
                            case 1:
                                buff = new char[readLength];
                                ReZipBuff(&buff, readLength, params->pathToLine);
                                break;
                            case 2:
                                buff = new char[readLength];
                                memcpy(buff, packReader.packBuffer + dataPos, readLength);
                                ReZipBuff(&buff, readLength, params->pathToLine);
                                break;
                            default:
                                continue;
                            }
                            if (!CanDelete(params, buff)) //判定不满足删除条件，保留之
                            {
                                memcpy(newPack + cur, packReader.packBuffer + dataPos - (zipType == 1 ? 29 : 33), readLength + (zipType == 1 ? 29 : 33));
                                cur += readLength;
                            }
                            else
                            {
                                deleteNum++;
                                if (i == 0) //删除了第一个节拍，包名的时间段需要修改
                                    packNameChange = true;
                            }
                        }
                        else
                        {
                            deleteNum++;
                            if (i == 0) //删除了第一个节拍，包名的时间段需要修改
                                packNameChange = true;
                        }
                        scanNum++;
                        if (scanNum == params->queryNums)
                        {
                            deleteComplete = true;
                            if (i == fileNum - 1)
                                packNameChange = true;
                            else //到此，对这个包的操作已经结束，将包中剩下的数据拼接上去即可
                            {
                                dataPos = packReader.Next(readLength, currentFileID, zipType);
                                memcpy(newPack + cur, packReader.packBuffer + dataPos - (zipType == 1 ? 29 : 33), packReader.GetPackLength() + (zipType == 1 ? 29 : 33) - dataPos);
                                cur += packReader.GetPackLength() + (zipType == 1 ? 29 : 33) - dataPos;
                                break;
                            }
                        }
                    }
                }
                int newFileNum = fileNum - deleteNum;
                memcpy(newPack, &newFileNum, 4);
                if (packNameChange)
                {
                    long newpackStart, newpackEnd;
                    int readLength, zipType;
                    free(packReader.packBuffer);
                    packReader.packBuffer = newPack;
                    packReader.SetCurPos(24);
                    packReader.Next(readLength, newpackStart, zipType);
                    if (newFileNum >= 2)
                    {
                        packReader.Skip(newFileNum - 2);
                        packReader.Next(readLength, newpackEnd, zipType);
                    }
                    else
                        newpackEnd = newpackStart;
                    auto vec = DataType::splitWithStl(pack.first, "/");
                    string newPackName = vec[0] + "/" + to_string(newpackStart) + "-" + to_string(newpackEnd) + ".pak ";
                    char mode[2] = {'w', 'b'};
                    long fp;
                    DB_Open(const_cast<char *>(newPackName.c_str()), mode, &fp);
                    fwrite(newPack, cur, 1, (FILE *)fp);
                    DB_Close(fp);
                    DB_DeleteFile(pack.first);
                    continue;
                }
                char mode[2] = {'w', 'b'};
                long fp;
                DB_Open(pack.first, mode, &fp);
                fwrite(newPack, cur, 1, (FILE *)fp);
                free(newPack);
                DB_Close(fp);
            }
            if (scanNum == params->queryNums)
                return 0;
            else
            {
                for (auto &file : dataFiles)
                {
                    string tmp = file.first;
                    vector<string> vec = DataType::splitWithStl(tmp, "/");
                    if (vec.size() == 0)
                        continue;
                    vec = DataType::splitWithStl(vec[vec.size() - 1], "_");
                    if (vec.size() == 0)
                        continue;
                    if (vec[0] == fileid)
                        firstIndexFound = true;
                    if (firstIndexFound)
                    {
                        scanNum++;
                        long len;
                        DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
                        char *buff = new char[len];
                        DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
                        if (file.first.find(".idbzip") != string::npos)
                            ReZipBuff(&buff, (int &)len, params->pathToLine);
                        if (CanDelete(params, buff))
                        {
                            DB_DeleteFile(const_cast<char *>(file.first.c_str()));
                        }
                        delete[] buff;
                    }
                    if (scanNum == params->queryNums)
                        return 0;
                }
            }
        }
        return 0;
        break;
    }
    default:
        break;
    }
    return StatusCode::NO_QUERY_TYPE;
}
// int main()
// {
//     DB_QueryParams params;
//     params.pathToLine = "JinfeiSeven";
//     params.fileID = "1";
//     params.fileIDend = NULL;
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
//     params.valueName = "S1ON";
//     // params.valueName = NULL;
//     params.start = 1649890000000;
//     params.end = 1649898032603;
//     params.order = ASCEND;
//     params.compareType = LT;
//     params.compareValue = "666";
//     params.queryType = FILEID;
//     params.byPath = 0;
//     params.queryNums = 3;
//     DB_DeleteRecords_New(&params);
//     return 0;
// }
