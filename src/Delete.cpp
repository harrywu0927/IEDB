#include <utils.hpp>

/**
 * @brief 根据自定义查询请求参数，删除符合条件的记录
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note  删除包中文件后需要修改此包总文件数，重新打包
 */
int DB_DeleteRecords(DB_QueryParams *params)
{
    int check = CheckQueryParams(params);
    if (check != 0)
        return check;
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
            else
            {
                return StatusCode::FILENAME_MODIFIED;
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
             *  否则，将包内的范围外的数据拷出后写成新的文件，将旧文件删除
             */
            for (auto &pack : selectedPacks)
            {
                if (std::get<0>(pack.second) >= params->start && std::get<1>(pack.second) <= params->end)
                {
                    DB_DeleteFile(const_cast<char *>(pack.first.c_str()));
                    continue;
                }
                PackFileReader packReader(pack.first);
                if (packReader.packBuffer == NULL)
                    continue;
                int fileNum;
                string templateName;
                packReader.ReadPackHead(fileNum, templateName);
                // TemplateManager::CheckTemplate(templateName);
                int deleteNum = 0;
                //一次分配一整个pak长度的空间，避免频繁分配影响性能
                // shared_ptr<char> newPack = make_shared<char>(packReader.GetPackLength());
                char *newPack = new char[packReader.GetPackLength()];
                long cur = 4;
                memcpy(newPack + cur, templateName.c_str(), templateName.length() <= 20 ? templateName.length() : 20);
                cur += 20;
                for (int i = 0; i < fileNum; i++)
                {
                    long timestamp;
                    int readLength, zipType;
                    string fileID;
                    long dataPos = packReader.Next(readLength, timestamp, fileID, zipType);
                    if (timestamp >= params->start || timestamp <= params->end) //在时间区间内
                    {
                        deleteNum++;
                        continue;
                    }
                    memcpy(newPack + cur, &timestamp, 8);
                    cur += 8;
                    memcpy(newPack + cur, fileID.c_str(), 20);
                    cur += 20;
                    memcpy(newPack + cur++, (char *)&zipType, 1);
                    if (zipType != 1) //非完全压缩
                    {
                        memcpy(newPack + cur, &readLength, 4);
                        cur += 4;
                        memcpy(newPack + cur, packReader.packBuffer + dataPos, readLength);
                        cur += readLength;
                    }
                }
                int newFileNum = fileNum - deleteNum;
                memcpy(newPack, &newFileNum, 4);
                char mode[2] = {'w', 'b'};
                long fp;
                DB_Open(const_cast<char *>(pack.first.c_str()), mode, &fp);
                fwrite(newPack, cur, 1, (FILE *)fp);
                delete[] newPack;
                DB_Close(fp);
            }
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
                    ReZipBuff(buff, (int &)len, params->pathToLine);
                }
                TemplateManager::CheckTemplate(params->pathToLine);
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
                for (int i = 0; i < fileNum; i++)
                {
                    long timestamp;
                    int readLength, zipType;
                    string fileID;
                    long dataPos = packReader.Next(readLength, timestamp, fileID, zipType);
                    if (timestamp >= params->start || timestamp <= params->end) //在时间区间内
                    {
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
                            ReZipBuff(buff, readLength, params->pathToLine);
                            break;
                        }
                        case 2:
                        {
                            memcpy(buff, packReader.packBuffer + dataPos, readLength);
                            ReZipBuff(buff, readLength, params->pathToLine);
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
                    memcpy(newPack + cur, &timestamp, 8);
                    cur += 8;
                    memcpy(newPack + cur, fileID.c_str(), 20);
                    cur += 20;
                    memcpy(newPack + cur++, (char *)&zipType, 1);
                    if (zipType != 1) //非完全压缩
                    {
                        memcpy(newPack + cur, &readLength, 4);
                        cur += 4;
                        memcpy(newPack + cur, packReader.packBuffer + dataPos, readLength);
                        cur += readLength;
                    }
                }
                int newFileNum = fileNum - deleteNum;
                memcpy(newPack, &newFileNum, 4);
                char mode[2] = {'w', 'b'};
                long fp;
                DB_Open(const_cast<char *>(pack.first.c_str()), mode, &fp);
                fwrite(newPack, cur, 1, (FILE *)fp);
                delete[] newPack;
                DB_Close(fp);
            }
        }
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
            for (auto &file : selectedFiles)
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
                    DB_DeleteFile(const_cast<char *>(pack.first.c_str()));
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
                for (int i = 0; i < fileNum - (params->queryNums - selectedNum); i++)
                {
                    dataPos = packReader.Next(readLength, timestamp, zipType);
                }
                memcpy(newPack, packReader.packBuffer, dataPos + readLength);
                int newNum = fileNum - params->queryNums + selectedNum;
                memcpy(newPack, &newNum, 4); //覆写新的文件个数
                long fp;
                char mode[2] = {'w', 'b'};
                DB_Open(const_cast<char *>(pack.first.c_str()), mode, &fp);
                fwrite(newPack, dataPos + readLength, 1, (FILE *)fp);
                delete[] newPack;
                DB_Close(fp);
                return 0;
            }
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
                    ReZipBuff(buff, (int &)len, params->pathToLine);
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
                    auto fileInfo = filestk.top();
                    filestk.pop();
                    long dataPos = fileInfo.first;
                    int readLength = get<0>(fileInfo.second);
                    long timestamp = get<1>(fileInfo.second);
                    string fileID = get<2>(fileInfo.second);
                    int zipType = get<3>(fileInfo.second);
                    if (deleteComplete) //删除已完成，直接拷贝剩余数据
                    {
                        memcpy(newPack + cur, &timestamp, 8);
                        cur += 8;
                        memcpy(newPack + cur, fileID.c_str(), 20);
                        cur += 20;
                        memcpy(newPack + cur++, (char *)&zipType, 1);
                        memcpy(newPack + cur, packReader.packBuffer + dataPos, packReader.GetPackLength() - dataPos);
                        int newFileNum = fileNum - deleteNum;
                        memcpy(newPack, &newFileNum, 4);
                        char mode[2] = {'w', 'b'};
                        long fp;
                        DB_Open(const_cast<char *>(pack.first.c_str()), mode, &fp);
                        fwrite(newPack, cur, 1, (FILE *)fp);
                        delete[] newPack;
                        DB_Close(fp);
                        return 0;
                    }
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
                        ReZipBuff(buff, readLength, params->pathToLine);
                        break;
                    }
                    case 2:
                    {
                        memcpy(buff, packReader.packBuffer + dataPos, readLength);
                        ReZipBuff(buff, readLength, params->pathToLine);
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
                    memcpy(newPack + cur, &timestamp, 8);
                    cur += 8;
                    memcpy(newPack + cur, fileID.c_str(), 20);
                    cur += 20;
                    memcpy(newPack + cur++, (char *)&zipType, 1);
                    if (zipType != 1) //非完全压缩
                    {
                        memcpy(newPack + cur, &readLength, 4);
                        cur += 4;
                        memcpy(newPack + cur, packReader.packBuffer + dataPos, readLength);
                        cur += readLength;
                    }
                }
                int newFileNum = fileNum - deleteNum;
                memcpy(newPack, &newFileNum, 4);
                char mode[2] = {'w', 'b'};
                long fp;
                DB_Open(const_cast<char *>(pack.first.c_str()), mode, &fp);
                fwrite(newPack, cur, 1, (FILE *)fp);
                delete[] newPack;
                DB_Close(fp);
                if (deleteComplete)
                    return 0;
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
                if (fid == fileid) //剔除这部分数据
                {
                    char *newPack = new char[packReader.GetPackLength()];
                    int newNum = fileNum - 1;
                    memcpy(newPack, packReader.packBuffer,dataPos - 28);
                    memcpy(newPack, &newNum, 4);
                    memcpy(newPack + dataPos - 28, packReader.packBuffer + dataPos + readLength, packReader.GetPackLength() - (dataPos + readLength));
                    long fp;
                    char mode[2] = {'w', 'b'};
                    DB_Open(const_cast<char *>(pack.c_str()), mode, &fp);
                    fwrite(newPack, packReader.GetPackLength() - 28 - readLength, 1, (FILE *)fp);
                    delete[] newPack;
                    return DB_Close(fp);
                }
            }
        }

        break;
    }
    default:
        break;
    }
    return StatusCode::NO_QUERY_TYPE;
}
