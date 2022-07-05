/******************************************
 * @file Query.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.8.6
 * @date 2022-06-21
 *
 * @copyright Copyright (c) 2022
 *
 ******************************************/

#include <utils.hpp>
#include <unordered_map>
#include <stack>
#include <tuple>
#include <atomic>
using namespace std;
Template CurrentTemplate;
mutex curMutex;
mutex indexMutex;
mutex memMutex; //防止多线程访问冲突

/**
 * @brief 从指定路径加载模版文件
 * @param path    路径
 *
 * @return  0:success,
 *          others: StatusCode
 * @note    在文件夹中找到模版文件，找到后读取，每个变量的前30字节为变量名，接下来30字节为数据类型，最后10字节为
 *          路径编码，分别解析之，构造、生成模版结构并将其设置为当前模版
 */
int DB_LoadSchema(const char *path)
{
	return TemplateManager::SetTemplate(path);
}

/**
 * @brief 卸载指定路径下的模版文件
 * @param pathToUnset    路径
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int DB_UnloadSchema(const char *pathToUnset)
{
	return TemplateManager::UnsetTemplate(pathToUnset);
}

/**
 * @brief 回收已分配的内存
 *
 * @param mallocedMemory
 */
void GarbageMemRecollection(vector<tuple<char *, long, long, long>> &mallocedMemory)
{
	for (auto &mem : mallocedMemory)
	{
		free(std::get<0>(mem));
	}
}

/**
 * @brief 在进行所有的查询操作前，检查输入的查询参数是否合法，若可能引发严重错误，则不允许进入查询
 * @param params        查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int CheckQueryParams(DB_QueryParams *params)
{
	if (params->pathToLine == NULL)
	{
		return StatusCode::EMPTY_PATH_TO_LINE;
	}
	if (params->pathCode == NULL && params->valueName == NULL)
	{
		return StatusCode::NO_PATHCODE_OR_NAME;
	}
	if (params->byPath == 0 && params->valueName == NULL)
	{
		return StatusCode::VARIABLE_NAME_CHECK_ERROR;
	}
	if (params->byPath == 1 && params->pathCode == NULL)
	{
		return StatusCode::PATHCODE_CHECK_ERROR;
	}
	if (params->byPath == 1)
	{
		vector<PathCode> codes;
		if (CurrentTemplate.GetAllPathsByCode(params->pathCode, codes) != 0)
			return StatusCode::UNKNOWN_PATHCODE;

		if (codes.size() > 1 && params->compareType != CMP_NONE && params->valueName == NULL)
			return StatusCode::VARIABLE_NOT_ASSIGNED;
		if (codes.size() > 1 && params->order != ODR_NONE && params->valueName == NULL)
			return StatusCode::VARIABLE_NOT_ASSIGNED;
		vector<PathCode> pathCode;
		if (params->valueName != NULL && CurrentTemplate.GetCodeByName(params->valueName, pathCode) == 0 && (params->order != ODR_NONE || params->compareType != CMP_NONE))
		{
			bool codeFound = false;
			for (auto &code : codes)
			{
				if (code == pathCode[0])
				{
					codeFound = true;
					break;
				}
			}
			if (!codeFound)
				return StatusCode::VARIABLE_NAME_NOT_INT_CODE;
		}
	}
	else
	{
	}
	switch (params->queryType)
	{
	case TIMESPAN:
	{
		if ((params->start == 0 && params->end == 0) || params->start > params->end)
		{
			return StatusCode::INVALID_TIMESPAN;
		}
		else if (params->end == 0)
		{
			params->end = getMilliTime();
		}
		break;
	}
	case LAST:
	{
		if (params->queryNums == 0)
		{
			return StatusCode::NO_QUERY_NUM;
		}
		break;
	}
	case FILEID:
	{
		if (params->fileID == NULL)
		{
			return StatusCode::NO_FILEID;
		}
		if (params->fileID != NULL && (params->fileIDend != NULL))
		{
			if (params->queryNums != 0 && params->queryNums != 1)
				return StatusCode::AMBIGUOUS_QUERY_PARAMS;
		}
		break;
	}
	default:
		return StatusCode::NO_QUERY_TYPE;
		break;
	}
	return 0;
}

/**
 * @brief 提取数据时可用到的静态参数，避免DataExtration函数的参数过多，略微影响性能
 *
 * @note 64位Linux系统中函数参数少于7个时， 参数从左到右放入寄存器: rdi, rsi, rdx, rcx, r8, r9。 当参数为7个以上时， 前 6 个与前面一样， 但后面的依次从 “右向左” 放入栈中，即和32位汇编一样。
 *
 */
struct Extraction_Params
{
	bool hasIMG;
	bool hasArray;
	bool hasTS;
	vector<long> posList;	   // position of each value
	vector<long> bytesList;	   // number of bytes of each value
	vector<DataType> typeList; // type of each value
	vector<PathCode> pathCodes;
	long bytes;
	long pos;
	DataType type;	// the type of the value to compare or sort
	long copyBytes; // number of bytes to copy
	int compareBytes;
	long sortPos; // the position of the value to sort
};

/**
 * @brief Get the Extraction Params object
 *
 * @param Ext_Params
 * @param Qry_Params
 * @return int
 */
int GetExtractionParams(Extraction_Params &Ext_Params, DB_QueryParams *Qry_Params)
{
	Ext_Params.hasArray = Qry_Params->byPath ? CurrentTemplate.checkHasArray(Qry_Params->pathCode) : CurrentTemplate.checkIsArray(Qry_Params->valueName);
	Ext_Params.hasTS = Qry_Params->byPath ? CurrentTemplate.checkHasTimeseries(Qry_Params->pathCode) : CurrentTemplate.checkIsTimeseries(Qry_Params->valueName);
	Ext_Params.hasIMG = Qry_Params->byPath ? CurrentTemplate.checkHasImage(Qry_Params->pathCode) : CurrentTemplate.checkIsImage(Qry_Params->valueName);

	int err;
	err = Qry_Params->byPath ? CurrentTemplate.FindMultiDatatypePosByCode(Qry_Params->pathCode, Ext_Params.posList, Ext_Params.bytesList, Ext_Params.typeList) : CurrentTemplate.FindDatatypePosByName(Qry_Params->valueName, Ext_Params.pos, Ext_Params.bytes, Ext_Params.type);
	if (err != 0)
	{
		IOBusy = false;
		return err;
	}
	Ext_Params.copyBytes = Qry_Params->byPath ? CurrentTemplate.GetBytesByCode(Qry_Params->pathCode) : Ext_Params.bytes;
	Ext_Params.sortPos = 0;
	if (Qry_Params->byPath && (Qry_Params->valueName != NULL || strcmp(Qry_Params->valueName, "") == 0))
	{
		Ext_Params.sortPos = CurrentTemplate.FindSortPosFromSelectedData(Ext_Params.bytesList, Qry_Params->valueName, Qry_Params->pathCode, Ext_Params.typeList);
		Ext_Params.compareBytes = CurrentTemplate.FindDatatypePosByName(Qry_Params->valueName, Ext_Params.pos, Ext_Params.bytes, Ext_Params.type) == 0 ? Ext_Params.bytes : 0;
	}
	return 0;
}

/**
 * @brief 从文件中提取合适的数据到堆区的内存中
 *
 * @param mallocedMemory 已分配内存
 * @param Ext_Params 提取参数
 * @param Qry_params 查询参数
 * @param cur 当前已选择的总字节数
 * @param timestamp 时间戳
 * @param buff 数据内容
 * @return int
 * @note 尽可能将函数的参数个数控制在7个以内
 */
int DataExtraction(vector<tuple<char *, long, long, long>> &mallocedMemory, Extraction_Params &Ext_Params, DB_QueryParams *Qry_params, long &cur, long &timestamp, char *buff)
{
	int err = 0;
	//获取数据的偏移量和字节数
	if (Ext_Params.hasIMG) //数据带有图片
	{
		Ext_Params.copyBytes = 0;
		if (Qry_params->byPath)
		{
			Ext_Params.bytesList.clear();
			Ext_Params.posList.clear();
			char *pathCode = Qry_params->pathCode;
			if (Ext_Params.typeList.size() == 0)
				err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, Ext_Params.posList, Ext_Params.bytesList, Ext_Params.typeList);
			else
				err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, Ext_Params.posList, Ext_Params.bytesList);

			for (int i = 0; i < Ext_Params.bytesList.size() && err == 0; i++)
			{
				Ext_Params.copyBytes += Ext_Params.typeList[i].hasTime ? Ext_Params.bytesList[i] + 8 : Ext_Params.bytesList[i];
			}
		}
		else
		{
			err = CurrentTemplate.FindDatatypePosByName(Qry_params->valueName, buff, Ext_Params.pos, Ext_Params.bytes, Ext_Params.type);
			Ext_Params.copyBytes = Ext_Params.type.hasTime ? Ext_Params.bytes + 8 : Ext_Params.bytes;
		}
		if (err != 0)
		{
			return err;
		}
	}

	char copyValue[Ext_Params.copyBytes];

	if (Qry_params->byPath)
	{
		long lineCur = 0; //记录此行当前写位置
		for (int i = 0; i < Ext_Params.bytesList.size(); i++)
		{
			long curBytes = Ext_Params.typeList[i].hasTime ? Ext_Params.bytesList[i] + 8 : Ext_Params.bytesList[i];
			memcpy(copyValue + lineCur, buff + Ext_Params.posList[i], curBytes);
			lineCur += curBytes;
		}
		if (Ext_Params.hasIMG && (Qry_params->valueName != NULL || strcmp(Qry_params->valueName, "") == 0)) //此处，若编码为精确搜索，而又输入了不同的变量名，FindSortPosFromSelectedData将返回0
		{
			Ext_Params.sortPos = CurrentTemplate.FindSortPosFromSelectedData(Ext_Params.bytesList, Qry_params->valueName, Ext_Params.pathCodes, Ext_Params.typeList);
			Ext_Params.compareBytes = CurrentTemplate.FindDatatypePosByName(Qry_params->valueName, buff, Ext_Params.pos, Ext_Params.bytes, Ext_Params.type) == 0 ? Ext_Params.bytes : 0;
		}
	}
	else
	{
		memcpy(copyValue, buff + Ext_Params.pos, Ext_Params.copyBytes);
		Ext_Params.compareBytes = Ext_Params.bytes;
	}

	bool canCopy = false; //根据比较结果决定是否允许拷贝

	if (Qry_params->compareType != CMP_NONE && Ext_Params.compareBytes != 0 && !Ext_Params.type.isTimeseries && !Ext_Params.type.isArray) //可比较
	{
		char value[Ext_Params.compareBytes]; //值缓存
		memcpy(value, buff + Ext_Params.pos, Ext_Params.compareBytes);
		//根据比较结果决定是否加入结果集
		int compareRes = DataType::CompareValue(Ext_Params.type, value, Qry_params->compareValue);
		switch (Qry_params->compareType)
		{
		case DB_CompareType::GT:
		{
			if (compareRes == 1)
			{
				canCopy = true;
			}
			break;
		}
		case DB_CompareType::LT:
		{
			if (compareRes == -1)
			{
				canCopy = true;
			}
			break;
		}
		case DB_CompareType::GE:
		{
			if (compareRes >= 0)
			{
				canCopy = true;
			}
			break;
		}
		case DB_CompareType::LE:
		{
			if (compareRes <= 0)
			{
				canCopy = true;
			}
			break;
		}
		case DB_CompareType::EQ:
		{
			if (compareRes == 0)
			{
				canCopy = true;
			}
			break;
		}
		default:
			break;
		}
	}
	else //不可比较，直接拷贝此数据
		canCopy = true;

	if (canCopy) //需要此数据
	{
		char *memory = new char[Ext_Params.copyBytes];
		memcpy(memory, copyValue, Ext_Params.copyBytes);
		cur += Ext_Params.copyBytes;
		mallocedMemory.push_back(make_tuple(memory, Ext_Params.copyBytes, Ext_Params.sortPos, timestamp));
	}
	return 0;
}

/**
 * @brief 从文件中提取合适的数据到堆区的内存中
 *
 * @param
 * @param mallocedMemory 已分配内存
 * @param Ext_Params 提取参数
 * @param Qry_params 查询参数
 * @param cur 当前已选择的总字节数
 * @param timestamp 时间戳
 * @param data 原始数据内容
 * @return int
 * @note 尽可能将函数的参数个数控制在7个以内
 */
inline int DataExtraction_NonIMG(char *buffer, vector<tuple<char *, long, long, long>> &mallocedMemory, Extraction_Params &Ext_Params, DB_QueryParams *Qry_params, long &cur, char *data, long &timestamp)
{
	if (Qry_params->byPath)
	{
		int lineCur = 0; //记录此行当前写位置
		for (int i = 0; i < Ext_Params.bytesList.size(); i++)
		{
			int curBytes = Ext_Params.typeList[i].hasTime ? Ext_Params.bytesList[i] + 8 : Ext_Params.bytesList[i];
			memcpy(buffer + cur + lineCur, data + Ext_Params.posList[i], curBytes);
			lineCur += curBytes;
		}
	}
	else
	{
		memcpy(buffer + cur, data + Ext_Params.pos, Ext_Params.copyBytes);
	}

	bool canCopy = false; //根据比较结果决定是否允许拷贝

	if (Qry_params->compareType != CMP_NONE && Ext_Params.compareBytes != 0 && !Ext_Params.type.isTimeseries && !Ext_Params.type.isArray) //可比较
	{
		//根据比较结果决定是否加入结果集
		int compareRes = DataType::CompareValue(Ext_Params.type, data + Ext_Params.pos, Qry_params->compareValue);
		switch (Qry_params->compareType)
		{
		case DB_CompareType::GT:
		{
			if (compareRes == 1)
			{
				canCopy = true;
			}
			break;
		}
		case DB_CompareType::LT:
		{
			if (compareRes == -1)
			{
				canCopy = true;
			}
			break;
		}
		case DB_CompareType::GE:
		{
			if (compareRes >= 0)
			{
				canCopy = true;
			}
			break;
		}
		case DB_CompareType::LE:
		{
			if (compareRes <= 0)
			{
				canCopy = true;
			}
			break;
		}
		case DB_CompareType::EQ:
		{
			if (compareRes == 0)
			{
				canCopy = true;
			}
			break;
		}
		default:
			break;
		}
	}
	else //不可比较，直接拷贝此数据
		canCopy = true;

	if (canCopy) //需要此数据
	{
		mallocedMemory.push_back(make_tuple(buffer + cur, Ext_Params.copyBytes, Ext_Params.sortPos, timestamp));
		cur += Ext_Params.copyBytes;
	}
	return 0;
}

/**
 * @brief Write the selected data to a single buffer
 *
 * @param mallocedMemory
 * @param Ext_Params
 * @param params
 * @param length
 * @return int
 */
int WriteDataToBuffer(vector<tuple<char *, long, long, long>> &mallocedMemory, Extraction_Params &Ext_Params, DB_QueryParams *params, DB_DataBuffer *buffer, long &length)
{
	if (length == 0)
	{
		buffer->buffer = NULL;
		buffer->bufferMalloced = 0;
		IOBusy = false;
		return StatusCode::NO_DATA_QUERIED;
	}
	//动态分配内存
	char typeNum = params->byPath ? Ext_Params.typeList.size() : 1; //数据类型总数
	char head[(int)typeNum * 19 + 1];
	int startPos = params->byPath ? CurrentTemplate.writeBufferHead(params->pathCode, Ext_Params.typeList, head) : CurrentTemplate.writeBufferHead(params->valueName, Ext_Params.type, head); //写入缓冲区头，获取数据区起始位置

	char *data = (char *)malloc(startPos + length);
	memcpy(data, head, startPos);

	if (data == NULL)
	{
		buffer->buffer = NULL;
		buffer->bufferMalloced = 0;
		if (Ext_Params.hasIMG)
			GarbageMemRecollection(mallocedMemory);
		IOBusy = false;
		return StatusCode::BUFFER_FULL;
	}
	//拷贝数据
	long cur = 0;
	if (params->queryType != LAST)
	{
		if (params->order == TIME_DSC) //时间降序，从内存地址集中反向拷贝
		{
			for (int i = mallocedMemory.size() - 1; i >= 0; i--)
			{
				memcpy(data + cur + startPos, get<0>(mallocedMemory[i]), get<1>(mallocedMemory[i]));
				cur += get<1>(mallocedMemory[i]);
			}
		}
		else //否则按照默认顺序升序排列即可
		{
			for (auto &mem : mallocedMemory)
			{
				memcpy(data + cur + startPos, get<0>(mem), get<1>(mem));
				cur += get<1>(mem);
			}
		}
	}
	else
	{
		if (params->order == TIME_ASC)
		{
			for (int i = mallocedMemory.size() - 1; i >= 0; i--)
			{
				memcpy(data + cur + startPos, get<0>(mallocedMemory[i]), get<1>(mallocedMemory[i]));
				cur += get<1>(mallocedMemory[i]);
			}
		}
		else //否则按照默认顺序升序排列即可
		{
			for (auto &mem : mallocedMemory)
			{
				memcpy(data + cur + startPos, get<0>(mem), get<1>(mem));
				cur += get<1>(mem);
			}
		}
	}
	buffer->bufferMalloced = 1;
	buffer->buffer = data;
	buffer->length = cur + startPos;
	IOBusy = false;
	return 0;
}

/**
 * @brief 根据指定数据类型的数值对查询结果排序或去重，此函数仅对内存地址-长度对操作
 * @param mallocedMemory        已在堆区分配的内存地址-长度对集
 * @param poses           指定数据在缓冲区中的位置
 * @param params        查询参数
 * @param type        数据类型
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int sortResultByValue(vector<pair<char *, long>> &mallocedMemory, vector<long> &poses, DB_QueryParams *params, DataType &type)
{
	if (type.isArray || type.valueType == ValueType::IMAGE)
		return StatusCode::QUERY_TYPE_NOT_SURPPORT;
	vector<pair<pair<char *, long>, long>> memAndPos; //使数据偏移与内存-长度对建立关联
	for (int i = 0; i < poses.size(); i++)
	{
		memAndPos.push_back(make_pair(mallocedMemory[i], poses[i]));
	}

	switch (params->order)
	{
	case ASCEND: //升序
	{
		sort(memAndPos.begin(), memAndPos.end(),
			 [&type](pair<pair<char *, long>, long> iter1, pair<pair<char *, long>, long> iter2) -> bool
			 {
				 char value1[type.valueBytes], value2[type.valueBytes];
				 memcpy(value1, iter1.first.first + iter1.second, type.valueBytes);
				 memcpy(value2, iter2.first.first + iter2.second, type.valueBytes);
				 return DataType::CompareValueInBytes(type, value1, value2) < 0;
			 });
		for (int i = 0; i < mallocedMemory.size(); i++)
		{
			mallocedMemory[i] = memAndPos[i].first;
		}

		break;
	}

	case DESCEND: //降序
	{
		sort(memAndPos.begin(), memAndPos.end(),
			 [&type](pair<pair<char *, long>, long> iter1, pair<pair<char *, long>, long> iter2) -> bool
			 {
				 char value1[type.valueBytes], value2[type.valueBytes];
				 memcpy(value1, iter1.first.first + iter1.second, type.valueBytes);
				 memcpy(value2, iter2.first.first + iter2.second, type.valueBytes);
				 return DataType::CompareValueInBytes(type, value1, value2) > 0;
			 });
		for (int i = 0; i < mallocedMemory.size(); i++)
		{
			mallocedMemory[i] = memAndPos[i].first;
		}
		break;
	}
	case DISTINCT: //去除重复
	{
		vector<pair<char *, long>> existedValues;
		for (int i = 0; i < mallocedMemory.size(); i++)
		{
			char value[type.valueBytes];
			memcpy(value, mallocedMemory[i].first + poses[i], type.valueBytes);
			bool isRepeat = false;
			for (auto &&added : existedValues)
			{
				bool equals = true;
				for (int i = 0; i < added.second; i++)
				{
					if (value[i] != added.first[i])
						equals = false;
				}
				if (equals)
				{
					free(mallocedMemory[i].first);					  //重复值，释放此内存
					mallocedMemory.erase(mallocedMemory.begin() + i); //注：此操作在查询量大时效率可能很低
					isRepeat = true;
					i--;
				}
			}
			if (!isRepeat) //不是重复值
			{
				char *v = value;
				existedValues.push_back(make_pair(v, type.valueBytes));
			}
		}

		break;
	}
	default:
		break;
	}
	// DataTypeConverter converter;
	// int a = 0;
	// for (auto it = mallocedMemory.begin(); it != mallocedMemory.end(); it++)
	// {
	//     char val[type.valueBytes];
	//     memcpy(val, it->first + memAndPos[a].second, type.valueBytes);
	//     cout << converter.ToUInt32(val) << endl;
	//     a++;
	// }

	return 0;
}

/**
 * @brief 根据指定数据类型的数值对查询结果排序或去重，此函数仅对内存地址-长度对操作
 * @param mallocedMemory        已在堆区分配的内存地址-长度-排序值偏移量-时间戳元组
 * @param params        查询参数
 * @param type        数据类型
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int sortResult(vector<tuple<char *, long, long, long>> &mallocedMemory, DB_QueryParams *params, DataType &type)
{
	if (type.isArray || type.valueType == ValueType::IMAGE || type.isTimeseries)
		return StatusCode::QUERY_TYPE_NOT_SURPPORT;
	switch (params->order)
	{
	case ASCEND: //升序
	{
		sort(mallocedMemory.begin(), mallocedMemory.end(),
			 [&type](tuple<char *, long, long, long> iter1, tuple<char *, long, long, long> iter2) -> bool
			 {
				 return DataType::CompareValueInBytes(type, std::get<0>(iter1) + std::get<2>(iter1), std::get<0>(iter2) + std::get<2>(iter2)) < 0;
			 });
		break;
	}

	case DESCEND: //降序
	{
		sort(mallocedMemory.begin(), mallocedMemory.end(),
			 [&type](tuple<char *, long, long, long> iter1, tuple<char *, long, long, long> iter2) -> bool
			 {
				 return DataType::CompareValueInBytes(type, std::get<0>(iter1) + std::get<2>(iter1), std::get<0>(iter2) + std::get<2>(iter2)) > 0;
			 });
		break;
	}
	case DISTINCT: //去除重复
	{
		bool hasIMG = params->byPath ? CurrentTemplate.checkHasImage(params->pathCode) : type.valueType == ValueType::IMAGE;
		vector<pair<char *, int>> existedValues;
		for (int i = 0; i < mallocedMemory.size(); i++)
		{
			char *value = std::get<0>(mallocedMemory[i]) + std::get<2>(mallocedMemory[i]);
			bool isRepeat = false;
			for (auto &&added : existedValues)
			{
				bool equals = true;
				for (int i = 0; i < added.second; i++)
				{
					if (value[i] != added.first[i])
						equals = false;
				}
				if (equals)
				{
					if (hasIMG)
						free(get<0>(mallocedMemory[i]));			  //重复值，释放此内存
					mallocedMemory.erase(mallocedMemory.begin() + i); //注：此操作在查询量大时效率可能很低
					isRepeat = true;
					i--;
				}
			}
			if (!isRepeat) //不是重复值
			{
				existedValues.push_back(make_pair(value, type.valueBytes));
			}
		}

		break;
	}
	case TIME_ASC:
	{
		sort(mallocedMemory.begin(), mallocedMemory.end(),
			 [](tuple<char *, long, long, long> iter1, tuple<char *, long, long, long> iter2) -> bool
			 {
				 return std::get<3>(iter1) < std::get<3>(iter2);
			 });
		break;
	}
	case TIME_DSC:
	{
		sort(mallocedMemory.begin(), mallocedMemory.end(),
			 [](tuple<char *, long, long, long> iter1, tuple<char *, long, long, long> iter2) -> bool
			 {
				 return std::get<3>(iter1) > std::get<3>(iter2);
			 });
		break;
	}
	default:
		break;
	}

	return 0;
}

//对文件根据时间升序或降序排序
void sortByTime(vector<pair<string, long>> &selectedFiles, DB_Order order)
{
	switch (order)
	{
	case ODR_NONE:
		break;
	case TIME_ASC:
		sort(selectedFiles.begin(), selectedFiles.end(),
			 [](pair<string, long> iter1, pair<string, long> iter2) -> bool
			 {
				 return iter1.second < iter2.second;
			 });
		break;
	case TIME_DSC:
		sort(selectedFiles.begin(), selectedFiles.end(),
			 [](pair<string, long> iter1, pair<string, long> iter2) -> bool
			 {
				 return iter1.second > iter2.second;
			 });
		break;
	default: //其他Order类型延后处理
		break;
	}
}
/**
 * @brief 执行自定义查询，自由组合查询请求参数，按照3个主查询条件之一，附加辅助条件查询
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int DB_ExecuteQuery(DB_DataBuffer *buffer, DB_QueryParams *params)
{
	switch (params->queryType)
	{
	case TIMESPAN:
		return DB_QueryByTimespan(buffer, params);
		break;
	case LAST:
		return DB_QueryLastRecords(buffer, params);
		break;
	case FILEID:
		return DB_QueryByFileID(buffer, params);
		break;
	default:
		break;
	}
	return StatusCode::NO_QUERY_TYPE;
}

/**
 * @brief 根据给定的查询条件在某一产线文件夹下的数据文件中获取符合条件的整个文件的数据，可比较数据大小筛选，可按照时间
 *          将结果升序或降序排序，数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int DB_QueryWholeFile(DB_DataBuffer *buffer, DB_QueryParams *params)
{
	// int check = CheckQueryParams(params);
	// if (check != 0)
	// 	return check;
	if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
		return StatusCode::SCHEMA_FILE_NOT_FOUND;

	vector<PathCode> pathCodes;
	if (params->byPath)
	{
		int err = CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
		if (err != 0)
			return err;
		if (pathCodes.size() > 1)
		{
			if ((params->queryType != QRY_NONE || params->compareType != CMP_NONE) && (params->valueName == NULL || strcmp(params->valueName, "") == 0)) //若此编码包含的数据类型大于1，而未指定变量名，又需要比较或排序，返回异常
				return StatusCode::INVALID_QRY_PARAMS;
		}
		else
		{
			if ((params->valueName == NULL || strcmp(params->valueName, "") == 0) && (params->queryType != QRY_NONE || params->compareType != CMP_NONE)) //由于编码会变为全0，因此若需要排序或比较，需要添加变量名
			{
				params->valueName = pathCodes[0].name.c_str();
			}
		}
		char zeros[10] = {0};
		memcpy(params->pathCode, zeros, 10);
	}
	else
	{
		params->byPath = 1;
		char zeros[10] = {0};
		memcpy(params->pathCode, zeros, 10);
	}
	switch (params->queryType)
	{
	case TIMESPAN:
		return DB_QueryByTimespan(buffer, params);
	case LAST:
		return DB_QueryLastRecords(buffer, params);
	case FILEID:
		return DB_QueryByFileID(buffer, params);
	default:
		return StatusCode::QUERY_TYPE_NOT_SURPPORT;
	}
	return StatusCode::QUERY_TYPE_NOT_SURPPORT;
}

/**
 * @brief 根据给定的时间段和路径编码或变量名在某一产线文件夹下的数据文件中查询数据，可比较数据大小筛选，可按照时间
 *          将结果升序或降序排序，数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note   单线程
 */
int DB_QueryByTimespan_Single_Old(DB_DataBuffer *buffer, DB_QueryParams *params)
{
	IOBusy = true;
	int check = CheckQueryParams(params);
	if (check != 0)
		return check;
	vector<pair<string, long>> filesWithTime, selectedFiles;
	auto selectedPacks = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
	//获取每个数据文件，并带有时间戳
	readDataFilesWithTimestamps(params->pathToLine, filesWithTime);

	//筛选落入时间区间内的文件
	for (auto &file : filesWithTime)
	{
		if (file.second >= params->start && file.second <= params->end)
		{
			selectedFiles.push_back(make_pair(file.first, file.second));
		}
	}

	//确认当前模版
	if (TemplateManager::CheckTemplate(params->pathToLine) != 0 && ZipTemplateManager::CheckZipTemplate(params->pathToLine) != 0)
	{
		IOBusy = false;
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
	}

	//根据时间升序排序
	sortByTime(selectedFiles, TIME_ASC);

	vector<tuple<char *, long, long, long>> mallocedMemory; //内存地址-长度-排序值偏移-时间戳元组集
	long cur = 0;

	int err;
	Extraction_Params Ext_Params;
	GetExtractionParams(Ext_Params, params);

	//先对时序在前的包文件检索
	for (auto &pack : selectedPacks)
	{
		auto pk = packManager.GetPack(pack.first);
		PackFileReader packReader(pk.first, pk.second);
		if (packReader.packBuffer == NULL)
			continue;
		int fileNum;
		string templateName;
		packReader.ReadPackHead(fileNum, templateName);
		// if (TemplateManager::CheckTemplate(templateName) != 0)
		// 	return StatusCode::SCHEMA_FILE_NOT_FOUND;
		if (params->byPath)
		{
			CurrentTemplate.GetAllPathsByCode(params->pathCode, Ext_Params.pathCodes);
		}
		for (int i = 0; i < fileNum; i++)
		{
			long timestamp;
			int readLength, zipType;
			long dataPos = packReader.Next(readLength, timestamp, zipType);
			if (timestamp < params->start || timestamp > params->end) //在时间区间外
				continue;
			char *buff;
			switch (zipType)
			{
			case 0:
			{
				buff = packReader.packBuffer + dataPos; //直接指向目标数据区，无需再拷贝数据
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
				continue;
			}
			err = DataExtraction(mallocedMemory, Ext_Params, params, cur, timestamp, buff);
			if (zipType != 0)
				delete[] buff;
			if (err != 0)
				return err;
		}
	}

	//对时序在后的普通文件检索
	for (auto &file : selectedFiles)
	{
		long len;
		DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
		char *buff = new char[CurrentTemplate.totalBytes];
		DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
		if (file.first.find(".idbzip") != string::npos)
		{
			ReZipBuff(&buff, (int &)len, params->pathToLine);
		}

		err = DataExtraction(mallocedMemory, Ext_Params, params, cur, file.second, buff);
		delete[] buff;
		if (err != 0)
			return err;
	}
	if (params->order != TIME_ASC && params->order != TIME_DSC) //时间排序仅需在拷贝时反向
		sortResult(mallocedMemory, params, Ext_Params.type);
	if (cur == 0)
	{
		buffer->buffer = NULL;
		buffer->bufferMalloced = 0;
		IOBusy = false;
		return StatusCode::NO_DATA_QUERIED;
	}
	//动态分配内存
	char typeNum = params->byPath ? Ext_Params.typeList.size() : 1; //数据类型总数
	char head[(int)typeNum * 19 + 1];
	int startPos;																			  //数据区起始位置
	if (!params->byPath)																	  //根据变量名查询，仅单个变量
		startPos = CurrentTemplate.writeBufferHead(params->valueName, Ext_Params.type, head); //写入缓冲区头，获取数据区起始位置
	else																					  //根据路径编码查询，可能有多个变量
		startPos = CurrentTemplate.writeBufferHead(params->pathCode, Ext_Params.typeList, head);
	char *data = (char *)malloc(startPos + cur);
	memcpy(data, head, startPos);

	if (data == NULL)
	{
		buffer->buffer = NULL;
		buffer->bufferMalloced = 0;
		IOBusy = false;
		return StatusCode::BUFFER_FULL;
	}
	//拷贝数据
	cur = 0;
	if (params->order == TIME_DSC) //时间降序，从内存地址集中反向拷贝
	{
		for (int i = mallocedMemory.size() - 1; i >= 0; i--)
		{
			memcpy(data + cur + startPos, get<0>(mallocedMemory[i]), get<1>(mallocedMemory[i]));
			delete[] get<0>(mallocedMemory[i]);
			cur += get<1>(mallocedMemory[i]);
		}
	}
	else //否则按照默认顺序升序排列即可
	{
		for (auto &mem : mallocedMemory)
		{
			memcpy(data + cur + startPos, get<0>(mem), get<1>(mem));
			delete[] get<0>(mem);
			cur += get<1>(mem);
		}
	}

	buffer->bufferMalloced = 1;
	buffer->buffer = data;
	buffer->length = cur + startPos;
	IOBusy = false;
	return 0;
}

/**
 * @brief 根据给定的时间段和路径编码或变量名在某一产线文件夹下的数据文件中查询数据，可比较数据大小筛选，可按照时间
 *          将结果升序或降序排序，数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note   单线程
 */
int DB_QueryByTimespan_Single(DB_DataBuffer *buffer, DB_QueryParams *params)
{
	IOBusy = true;
	//确认当前模版
	if (TemplateManager::CheckTemplate(params->pathToLine) != 0 && ZipTemplateManager::CheckZipTemplate(params->pathToLine) != 0)
	{
		IOBusy = false;
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
	}
	int check = CheckQueryParams(params);
	if (check != 0)
		return check;
	vector<pair<string, long>> filesWithTime, selectedFiles;
	auto selectedPacks = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
	//获取每个数据文件，并带有时间戳
	readDataFilesWithTimestamps(params->pathToLine, filesWithTime);

	//筛选落入时间区间内的文件
	for (auto &file : filesWithTime)
	{
		if (file.second >= params->start && file.second <= params->end)
		{
			selectedFiles.push_back(make_pair(file.first, file.second));
		}
	}

	//根据时间升序排序
	sortByTime(selectedFiles, TIME_ASC);

	vector<tuple<char *, long, long, long>> mallocedMemory; //内存地址-长度-排序值偏移-时间戳元组集
	long cur = 0;

	int err;
	Extraction_Params Ext_Params;
	err = GetExtractionParams(Ext_Params, params);
	if (err != 0)
		return err;
	char *rawBuff = nullptr;
	int startPos;
	if (!Ext_Params.hasIMG) //当查询条件不含图片时，结果的总长度已确定
	{
		char typeNum = params->byPath ? (Ext_Params.typeList.size() == 0 ? 1 : Ext_Params.typeList.size()) : 1; //数据类型总数
		char head[(int)typeNum * 19 + 1];
		//数据区起始位置
		if (!params->byPath)																	  //根据变量名查询，仅单个变量
			startPos = CurrentTemplate.writeBufferHead(params->valueName, Ext_Params.type, head); //写入缓冲区头，获取数据区起始位置
		else																					  //根据路径编码查询，可能有多个变量
			startPos = CurrentTemplate.writeBufferHead(params->pathCode, Ext_Params.typeList, head);
		//此处可能会分配多余的空间，但最多在两个包的可接受大小内
		int size = Ext_Params.copyBytes * (selectedFiles.size() + fileIDManager.GetPacksRhythmNum(selectedPacks)) + startPos;
		rawBuff = (char *)malloc(Ext_Params.copyBytes * (selectedFiles.size() + fileIDManager.GetPacksRhythmNum(selectedPacks)) + startPos);
		memcpy(rawBuff, head, startPos);
		cur = startPos;
	}
	/**
	 * @brief 完全压缩的数据还原后必然相同，因此仅需还原一次后保存备用
	 * 此处仅还原了非图片部分的数据，遇到图片后将图片与之拼接即可
	 */
	char *completeZiped = new char[CurrentTemplate.totalBytes];
	int rezipedlen = 0;
	ReZipBuff(&completeZiped, rezipedlen, params->pathToLine);
	//先对时序在前的包文件检索
	for (auto &pack : selectedPacks)
	{
		auto pk = packManager.GetPack(pack.first);
		PackFileReader packReader(pk.first, pk.second);
		if (packReader.packBuffer == NULL)
			continue;
		int fileNum;
		string templateName;
		packReader.ReadPackHead(fileNum, templateName);
		if (params->byPath)
		{
			CurrentTemplate.GetAllPathsByCode(params->pathCode, Ext_Params.pathCodes);
		}
		for (int i = 0; i < fileNum; i++)
		{
			char *buff = nullptr, *img = nullptr;
			long timestamp;
			int zipType, readLength;
			long dataPos = packReader.Next(readLength, timestamp, zipType, &buff, completeZiped);
			if (timestamp < params->start || timestamp > params->end) //在时间区间外
				continue;

			if (Ext_Params.hasIMG)
			{
				char *newBuffer;
				long len;
				err = params->byPath ? FindImage(&img, len, pack.first, i, params->pathCode) : FindImage(&img, len, pack.first, i, params->valueName);
				if (err != 0)
				{
					if (Ext_Params.hasIMG)
						GarbageMemRecollection(mallocedMemory);
					else
						free(rawBuff);
					return err;
				}
				newBuffer = new char[readLength + len];
				memcpy(newBuffer, buff, zipType == 1 ? CurrentTemplate.totalBytes : readLength);
				memcpy(newBuffer + readLength, img, len);
				if (zipType == 2)
					delete[] buff;
				buff = newBuffer;
				err = DataExtraction(mallocedMemory, Ext_Params, params, cur, timestamp, buff);
			}
			else
			{
				err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, timestamp);
			}

			if (zipType == 2 || Ext_Params.hasIMG)
				delete[] buff;
			if (err != 0)
			{
				if (Ext_Params.hasIMG)
					GarbageMemRecollection(mallocedMemory);
				else
					free(rawBuff);
				return err;
			}
		}
	}

	//对时序在后的普通文件检索
	for (auto &file : selectedFiles)
	{
		long len;
		DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
		char *buff = new char[len];
		DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
		if (file.first.find(".idbzip") != string::npos)
		{
			ReZipBuff(&buff, (int &)len, params->pathToLine);
		}
		if (Ext_Params.hasIMG)
			err = DataExtraction(mallocedMemory, Ext_Params, params, cur, file.second, buff);
		else
			err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, file.second);
		delete[] buff;
		if (err != 0)
		{
			if (Ext_Params.hasIMG)
				GarbageMemRecollection(mallocedMemory);
			else
				free(rawBuff);
			return err;
		}
	}
	delete[] completeZiped;
	if (params->order != TIME_ASC && params->order != TIME_DSC && params->order != ODR_NONE) //时间排序仅需在拷贝时反向
		sortResult(mallocedMemory, params, Ext_Params.type);
	else if (params->order == ODR_NONE || params->order == TIME_ASC)
	{
		buffer->buffer = rawBuff;
		buffer->bufferMalloced = 1;
		buffer->length = cur;
		return 0;
	}
	if (cur == startPos)
	{
		buffer->buffer = NULL;
		buffer->length = 0;
		buffer->bufferMalloced = 0;
		return StatusCode::NO_DATA_QUERIED;
	}
	return WriteDataToBuffer(mallocedMemory, Ext_Params, params, buffer, cur);
}

/**
 * @brief 线程任务，处理单个包
 *
 * @param pack 包的内存地址-长度对
 * @param params 查询参数
 * @param cur 当前已分配的内存总长度
 * @param mallocedMemory 已分配的内存地址-长度-排序值偏移量-时间戳元组，临界资源，需要加线程互斥锁
 * @param type  排序值的数据类型
 * @param typeList 选中的数据类型列表
 * @return int
 */
int PackProcess(pair<char *, long> pack, DB_QueryParams *params, atomic<long> *cur, vector<tuple<char *, long, long, long>> *mallocedMemory, DataType *type, vector<DataType> *typeList)
{
	PackFileReader packReader(pack.first, pack.second);
	if (packReader.packBuffer == NULL)
		return StatusCode::DATAFILE_NOT_FOUND;
	int fileNum;
	string templateName;
	packReader.ReadPackHead(fileNum, templateName);
	if (TemplateManager::CheckTemplate(templateName) != 0)
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
	vector<PathCode> pathCodes;
	if (params->byPath)
	{
		CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
	}

	for (int i = 0; i < fileNum; i++)
	{
		long timestamp;
		int readLength, zipType;
		long dataPos = packReader.Next(readLength, timestamp, zipType);
		if (timestamp < params->start || timestamp > params->end) //在时间区间外
			continue;
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
			continue;
			break;
		}
		//获取数据的偏移量和数据类型
		long pos = 0, bytes = 0;
		vector<long> posList, bytesList;
		long copyBytes = 0;
		int err;
		if (params->byPath)
		{
			char *pathCode = params->pathCode;
			if (typeList->size() == 0)
				err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, *typeList);
			else
				err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList);
			for (int i = 0; i < bytesList.size(); i++)
			{
				copyBytes += typeList->at(i).hasTime ? bytesList[i] + 8 : bytesList[i];
			}
		}
		else
		{
			err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, *type);
			copyBytes = type->hasTime ? bytes + 8 : bytes;
		}
		if (err != 0)
		{
			continue;
		}

		char copyValue[copyBytes]; //将要拷贝的数值
		long sortPos = 0;
		int compareBytes = 0;
		if (params->byPath)
		{
			int lineCur = 0; //记录此行当前写位置
			for (int i = 0; i < bytesList.size(); i++)
			{
				int curBytes = typeList->at(i).hasTime ? bytesList[i] + 8 : bytesList[i];
				memcpy(copyValue + lineCur, buff + posList[i], curBytes);
				lineCur += curBytes;
			}
			if (params->valueName != NULL) //此处，若编码为精确搜索，而又输入了不同的变量名，FindSortPosFromSelectedData将返回0
			{
				sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, pathCodes, *typeList);
				compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, *type) == 0 ? bytes : 0;
			}
		}
		else
		{
			memcpy(copyValue, buff + pos, copyBytes);
			compareBytes = bytes;
		}

		bool canCopy = false;
		if (params->compareType != CMP_NONE && compareBytes != 0) //可比较
		{
			char value[compareBytes]; //数值
			memcpy(value, buff + pos, compareBytes);

			//根据比较结果决定是否加入结果集
			int compareRes = DataType::CompareValue(*type, value, params->compareValue);
			switch (params->compareType)
			{
			case DB_CompareType::GT:
			{
				if (compareRes > 0)
				{
					canCopy = true;
				}
				break;
			}
			case DB_CompareType::LT:
			{
				if (compareRes < 0)
				{
					canCopy = true;
				}
				break;
			}
			case DB_CompareType::GE:
			{
				if (compareRes >= 0)
				{
					canCopy = true;
				}
				break;
			}
			case DB_CompareType::LE:
			{
				if (compareRes <= 0)
				{
					canCopy = true;
				}
				break;
			}
			case DB_CompareType::EQ:
			{
				if (compareRes == 0)
				{
					canCopy = true;
				}
				break;
			}
			default:
				break;
			}
		}
		else //不可比较，直接拷贝此数据
			canCopy = true;
		if (canCopy) //需要此数据
		{
			char *memory = new (std::nothrow) char[copyBytes];
			if (memory == nullptr)
			{
				cout << "memory null" << endl;
			}
			else
			{
				memcpy(memory, copyValue, copyBytes);
				*cur += copyBytes;
				auto t = make_tuple(memory, copyBytes, sortPos, timestamp);
				memMutex.lock();
				mallocedMemory->push_back(t);
				memMutex.unlock();
			}
		}
		delete[] buff;
	}
	return 0;
}

/**
 * @brief 线程任务，处理单个包
 *
 * @param packInfo 包的信息
 * @param params 查询参数
 * @param cur 当前已分配的内存总长度
 * @param mallocedMemory 已分配的内存地址-长度-排序值偏移量-时间戳元组，临界资源，需要加线程互斥锁
 * @param Ext_Params
 * @return int
 */
int PackProcess_BrandNew(pair<string, pair<char *, long>> *packInfo, DB_QueryParams *params, atomic<long> *cur, vector<tuple<char *, long, long, long>> *mallocedMemory, Extraction_Params *Ext_Params)
{
	auto pack = packManager.GetPack(packInfo->first);
	PackFileReader packReader(pack.first, pack.second);
	if (packReader.packBuffer == NULL)
		return StatusCode::DATAFILE_NOT_FOUND;
	int fileNum;
	string templateName;
	packReader.ReadPackHead(fileNum, templateName);
	// if (TemplateManager::CheckTemplate(templateName) != 0)
	// 	return StatusCode::SCHEMA_FILE_NOT_FOUND;
	vector<PathCode> pathCodes;
	if (params->byPath)
	{
		CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
	}
	char *completeZiped = new char[CurrentTemplate.totalBytes];
	int rezipedlen = 0;
	ReZipBuff(&completeZiped, rezipedlen, params->pathToLine);
	for (int i = 0; i < fileNum; i++)
	{
		long timestamp;
		int readLength, zipType;
		long dataPos = packReader.Next(readLength, timestamp, zipType);
		if (timestamp < params->start || timestamp > params->end) //在时间区间外
			continue;
		char *buff = nullptr, *img = nullptr;
		switch (zipType)
		{
		case 0:
		{
			buff = packReader.packBuffer + dataPos; //直接指向目标数据区，无需再拷贝数据
			break;
		}
		case 1:
		{
			buff = completeZiped;
			break;
		}
		case 2:
		{
			buff = new char[CurrentTemplate.totalBytes];
			memcpy(buff, packReader.packBuffer + dataPos, readLength);
			ReZipBuff(&buff, readLength);
			break;
		}
		default:
			continue;
		}

		if (Ext_Params->hasIMG)
		{
			char *newBuffer;
			long len;
			string path = packInfo->first;
			int err = params->byPath ? FindImage(&img, len, path, i, params->pathCode) : FindImage(&img, len, path, i, params->valueName);
			if (err != 0)
			{
				errorCode = err;
				continue;
			}
			newBuffer = new char[readLength + len];
			memcpy(newBuffer, buff, zipType == 1 ? CurrentTemplate.totalBytes : readLength);
			memcpy(newBuffer + readLength, img, len);
			if (zipType == 2)
				delete[] buff;
			buff = newBuffer;
		}
		//获取数据的偏移量和数据类型
		int err;
		if (Ext_Params->hasIMG) //数据带有图片
		{
			if (params->byPath)
			{
				char *pathCode = params->pathCode;
				if (Ext_Params->typeList.size() == 0)
					err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, Ext_Params->posList, Ext_Params->bytesList, Ext_Params->typeList);
				else
					err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, Ext_Params->posList, Ext_Params->bytesList);
				for (int i = 0; i < Ext_Params->bytesList.size(); i++)
				{
					Ext_Params->copyBytes += Ext_Params->typeList.at(i).hasTime ? Ext_Params->bytesList[i] + 8 : Ext_Params->bytesList[i];
				}
			}
			else
			{
				err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, Ext_Params->pos, Ext_Params->bytes, Ext_Params->type);
				Ext_Params->copyBytes = Ext_Params->type.hasTime ? Ext_Params->bytes + 8 : Ext_Params->bytes;
			}
		}
		if (err != 0)
		{
			continue;
		}

		char copyValue[Ext_Params->copyBytes]; //将要拷贝的数值
		if (params->byPath)
		{
			int lineCur = 0; //记录此行当前写位置
			for (int i = 0; i < Ext_Params->bytesList.size(); i++)
			{
				int curBytes = Ext_Params->typeList.at(i).hasTime ? Ext_Params->bytesList[i] + 8 : Ext_Params->bytesList[i];
				memcpy(copyValue + lineCur, buff + Ext_Params->posList[i], curBytes);
				lineCur += curBytes;
			}
			if (Ext_Params->hasIMG && (params->valueName != NULL || strcmp(params->valueName, "") == 0)) //此处，若编码为精确搜索，而又输入了不同的变量名，FindSortPosFromSelectedData将返回0
			{
				Ext_Params->sortPos = CurrentTemplate.FindSortPosFromSelectedData(Ext_Params->bytesList, params->valueName, pathCodes, Ext_Params->typeList);
				Ext_Params->compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, Ext_Params->pos, Ext_Params->bytes, Ext_Params->type) == 0 ? Ext_Params->bytes : 0;
			}
		}
		else
		{
			memcpy(copyValue, buff + Ext_Params->pos, Ext_Params->copyBytes);
			Ext_Params->compareBytes = Ext_Params->bytes;
		}

		bool canCopy = false;
		if (params->compareType != CMP_NONE && Ext_Params->compareBytes != 0 && !Ext_Params->type.isTimeseries && !Ext_Params->type.isArray) //可比较
		{
			char value[Ext_Params->compareBytes]; //数值
			memcpy(value, buff + Ext_Params->pos, Ext_Params->compareBytes);

			//根据比较结果决定是否加入结果集
			int compareRes = DataType::CompareValue(Ext_Params->type, value, params->compareValue);
			switch (params->compareType)
			{
			case DB_CompareType::GT:
			{
				if (compareRes > 0)
				{
					canCopy = true;
				}
				break;
			}
			case DB_CompareType::LT:
			{
				if (compareRes < 0)
				{
					canCopy = true;
				}
				break;
			}
			case DB_CompareType::GE:
			{
				if (compareRes >= 0)
				{
					canCopy = true;
				}
				break;
			}
			case DB_CompareType::LE:
			{
				if (compareRes <= 0)
				{
					canCopy = true;
				}
				break;
			}
			case DB_CompareType::EQ:
			{
				if (compareRes == 0)
				{
					canCopy = true;
				}
				break;
			}
			default:
				break;
			}
		}
		else //不可比较，直接拷贝此数据
			canCopy = true;
		if (canCopy) //需要此数据
		{
			char *memory = new (std::nothrow) char[Ext_Params->copyBytes];
			if (memory == nullptr)
			{
				cout << "memory null" << endl;
			}
			else
			{
				memcpy(memory, copyValue, Ext_Params->copyBytes);
				*cur += Ext_Params->copyBytes;
				auto t = make_tuple(memory, Ext_Params->copyBytes, Ext_Params->sortPos, timestamp);
				memMutex.lock();
				mallocedMemory->push_back(t);
				memMutex.unlock();
			}
		}
		if (zipType == 2 || Ext_Params->hasIMG)
			delete[] buff;
	}
	return 0;
}

/**
 * @brief 根据给定的时间段和路径编码或变量名在某一产线文件夹下的数据文件中查询数据，可比较数据大小筛选，可按照时间
 *          将结果升序或降序排序，数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note   支持idb文件和pak文件混合查询,此处默认pak文件中的时间均早于idb和idbzip文件！！
 */
int DB_QueryByTimespan(DB_DataBuffer *buffer, DB_QueryParams *params)
{
	IOBusy = true;
	if (maxThreads <= 2) //线程数量<=2时，多线程已无必要
	{
		return DB_QueryByTimespan_Single(buffer, params);
	}
	//确认当前模版
	if (TemplateManager::CheckTemplate(params->pathToLine) != 0 && ZipTemplateManager::CheckZipTemplate(params->pathToLine) != 0)
	{
		IOBusy = false;
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
	}
	/**
	 * @brief 当变量数为1个时，使用单线程速度更快
	 *
	 */
	if (params->byPath)
	{
		vector<PathCode> vec;
		CurrentTemplate.GetAllPathsByCode(params->pathCode, vec);
		if (vec.size() == 1)
			return DB_QueryByTimespan_Single(buffer, params);
	}
	else
	{
		return DB_QueryByTimespan_Single(buffer, params);
	}
	int check = CheckQueryParams(params);
	if (check != 0)
	{
		IOBusy = false;
		return check;
	}
	vector<pair<string, long>> filesWithTime, selectedFiles;
	auto selectedPacks = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
	//获取每个数据文件，并带有时间戳
	readDataFilesWithTimestamps(params->pathToLine, filesWithTime);

	//筛选落入时间区间内的文件
	for (auto &file : filesWithTime)
	{
		if (file.second >= params->start && file.second <= params->end)
		{
			selectedFiles.push_back(make_pair(file.first, file.second));
		}
	}

	vector<tuple<char *, long, long, long>> mallocedMemory; //当前已分配的内存地址-长度-排序值偏移-时间戳元组
	atomic<long> cur(0);									//已选择的数据总长
	// atomic<vector<tuple<char *, long, long, long>>> mallocedMemory;
	int err;
	Extraction_Params Ext_Params;
	err = GetExtractionParams(Ext_Params, params);
	if (err != 0)
		return err;
	//先对时序在前的包文件检索
	if (selectedPacks.size() > 0)
	{
		int index = 0;
		future_status status[maxThreads - 1];
		future<int> f[maxThreads - 1];
		for (int j = 0; j < maxThreads - 1; j++)
		{
			status[j] = future_status::ready;
		}
		do
		{
			for (int j = 0; j < maxThreads - 1; j++) //留一个线程循环遍历线程集，确认每个线程的运行状态
			{
				if (status[j] == future_status::ready)
				{
					auto pk = make_pair(selectedPacks[index].first, packManager.GetPack(selectedPacks[index].first));
					f[j] = async(std::launch::async, PackProcess_BrandNew, &pk, params, &cur, &mallocedMemory, &Ext_Params);
					status[j] = f[j].wait_for(chrono::milliseconds(1));
					index++;
					if (index == selectedPacks.size())
						break;
				}
				else
				{
					status[j] = f[j].wait_for(chrono::milliseconds(1));
				}
			}
		} while (index < selectedPacks.size());
		for (int j = 0; j < maxThreads - 1; j++)
		{
			if (status[j] != future_status::ready)
				f[j].wait();
		}
	}

	if (params->byPath)
		CurrentTemplate.GetAllPathsByCode(params->pathCode, Ext_Params.pathCodes);
	//对时序在后的普通文件检索
	long cur_nonatomic = cur; //非原子类型,兼容DataExtraction
	for (auto &file : selectedFiles)
	{
		long len;
		DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
		char *buff = new char[len];
		DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
		if (file.first.find(".idbzip") != string::npos)
		{
			ReZipBuff(&buff, (int &)len, params->pathToLine);
		}
		err = DataExtraction(mallocedMemory, Ext_Params, params, cur_nonatomic, file.second, buff);
		delete[] buff;
		if (err != 0)
			return err;
	}
	cur = cur_nonatomic;

	if (cur == 0)
	{
		buffer->buffer = NULL;
		buffer->bufferMalloced = 0;
		IOBusy = false;
		return StatusCode::NO_DATA_QUERIED;
	}
	if (params->order != TIME_ASC && params->order != TIME_DSC) //时间排序仅需在拷贝时反向
		sortResult(mallocedMemory, params, Ext_Params.type);
	return WriteDataToBuffer(mallocedMemory, Ext_Params, params, buffer, cur_nonatomic);
}

/**
 * @brief 线程任务，处理单个包
 *
 * @param pack 包的内存地址-长度对
 * @param params 查询参数
 * @param cur 当前已分配的内存总长度，临界资源，需要加线程互斥锁
 * @param mallocedMemory 已分配的内存地址-长度-排序值偏移量-时间戳元组，临界资源，需要加线程互斥锁
 * @param type  排序值的数据类型
 * @param typeList 选中的数据类型列表
 * @return int
 */
int PackProcess_New(DB_QueryParams *params, atomic<long> *cur, atomic<int> *index, vector<int> *complete, int threadIndex, vector<pair<string, tuple<long, long>>> *selectedPacks, vector<tuple<char *, long, long, long>> *mallocedMemory, DataType *type)
{
	while (1)
	{
		indexMutex.lock();
		if (*index >= selectedPacks->size())
		{
			indexMutex.unlock();
			complete->at(threadIndex) = 1;
			return 1;
		}
		auto pack = packManager.GetPack(selectedPacks->at(*index).first);
		*index += 1;
		indexMutex.unlock();
		vector<DataType> typeList;
		PackFileReader packReader(pack.first, pack.second);
		if (packReader.packBuffer == NULL)
			return 1;
		int fileNum;
		string templateName;
		packReader.ReadPackHead(fileNum, templateName);
		if (TemplateManager::CheckTemplate(templateName) != 0)
			return 1;
		vector<PathCode> pathCodes;
		if (params->byPath)
		{
			CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
		}

		for (int i = 0; i < fileNum; i++)
		{
			long timestamp;
			int readLength, zipType;
			long dataPos = packReader.Next(readLength, timestamp, zipType);
			if (timestamp < params->start || timestamp > params->end) //在时间区间外
				continue;
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
				continue;
				break;
			}
			//获取数据的偏移量和数据类型
			long pos = 0, bytes = 0;
			vector<long> posList, bytesList;
			long copyBytes = 0;
			int err;
			if (params->byPath)
			{
				char *pathCode = params->pathCode;
				if (typeList.size() == 0)
					err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
				else
					err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList);
				for (int i = 0; i < bytesList.size(); i++)
				{
					copyBytes += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
				}
			}
			else
			{
				err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, *type);
				copyBytes = type->hasTime ? bytes + 8 : bytes;
			}
			if (err != 0)
			{
				continue;
			}

			char copyValue[copyBytes]; //将要拷贝的数值
			long sortPos = 0;
			int compareBytes = 0;
			if (params->byPath)
			{
				int lineCur = 0; //记录此行当前写位置
				for (int i = 0; i < bytesList.size(); i++)
				{
					int curBytes = typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
					memcpy(copyValue + lineCur, buff + posList[i], curBytes);
					lineCur += curBytes;
				}
				if (params->valueName != NULL) //此处，若编码为精确搜索，而又输入了不同的变量名，FindSortPosFromSelectedData将返回0
				{
					sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, pathCodes, typeList);
					compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, *type) == 0 ? bytes : 0;
				}
			}
			else
			{
				memcpy(copyValue, buff + pos, copyBytes);
				compareBytes = bytes;
			}

			bool canCopy = false;
			if (params->compareType != CMP_NONE && compareBytes != 0) //可比较
			{
				char value[compareBytes]; //数值
				memcpy(value, buff + pos, compareBytes);

				//根据比较结果决定是否加入结果集
				int compareRes = DataType::CompareValue(*type, value, params->compareValue);
				switch (params->compareType)
				{
				case DB_CompareType::GT:
				{
					if (compareRes > 0)
					{
						canCopy = true;
					}
					break;
				}
				case DB_CompareType::LT:
				{
					if (compareRes < 0)
					{
						canCopy = true;
					}
					break;
				}
				case DB_CompareType::GE:
				{
					if (compareRes >= 0)
					{
						canCopy = true;
					}
					break;
				}
				case DB_CompareType::LE:
				{
					if (compareRes <= 0)
					{
						canCopy = true;
					}
					break;
				}
				case DB_CompareType::EQ:
				{
					if (compareRes == 0)
					{
						canCopy = true;
					}
					break;
				}
				default:
					break;
				}
			}
			else //不可比较，直接拷贝此数据
				canCopy = true;
			if (canCopy) //需要此数据
			{
				char *memory = new (std::nothrow) char[copyBytes];
				if (memory == nullptr)
				{
					cout << "memory null" << endl;
				}
				else
				{
					memcpy(memory, copyValue, copyBytes);
					*cur += copyBytes;
					auto t = make_tuple(memory, copyBytes, sortPos, timestamp);
					memMutex.lock();
					mallocedMemory->push_back(t);
					memMutex.unlock();
				}
			}
			delete[] buff;
		}
	}
	// complete->at(threadIndex) = 1;
}

/**
 * @brief 根据给定的时间段和路径编码或变量名在某一产线文件夹下的数据文件中查询数据，可比较数据大小筛选，可按照时间
 *          将结果升序或降序排序，数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note   可能不稳定
 */
int DB_QueryByTimespan_MT(DB_DataBuffer *buffer, DB_QueryParams *params)
{
	IOBusy = true;
	if (maxThreads <= 2) //线程数量<=2时，多线程已无必要
	{
		return DB_QueryByTimespan_Single(buffer, params);
	}
	//确认当前模版
	if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
	if (params->byPath)
	{
		vector<PathCode> vec;
		CurrentTemplate.GetAllPathsByCode(params->pathCode, vec);
		if (vec.size() == 1)
			return DB_QueryByTimespan_Single(buffer, params);
	}
	int check = CheckQueryParams(params);
	if (check != 0)
	{
		IOBusy = false;
		return check;
	}
	vector<pair<string, long>> filesWithTime, selectedFiles;
	auto selectedPacks = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
	//获取每个数据文件，并带有时间戳
	readDataFilesWithTimestamps(params->pathToLine, filesWithTime);

	//筛选落入时间区间内的文件
	for (auto &file : filesWithTime)
	{
		if (file.second >= params->start && file.second <= params->end)
		{
			selectedFiles.push_back(make_pair(file.first, file.second));
		}
	}

	vector<tuple<char *, long, long, long>> mallocedMemory; //当前已分配的内存地址-长度-排序值偏移-时间戳元组
	atomic<long> cur(0);									//已选择的数据总长
	// atomic<vector<tuple<char *, long, long, long>>> mallocedMemory;
	DataType type;
	vector<DataType> typeList, selectedTypes;
	vector<long> sortDataPoses; //按值排序时要比较的数据的偏移量
	vector<PathCode> pathCodes;
	typeList = CurrentTemplate.GetAllTypes(params->pathCode);
	//先对时序在前的包文件检索
	if (selectedPacks.size() > 0)
	{
		atomic<int> index(0);
		future<int> threads[maxThreads - 1];
		future_status status[maxThreads - 1];
		vector<int> complete(maxThreads - 1, 0);
		for (int j = 0; j < maxThreads - 1; j++)
		{
			status[j] = future_status::ready;
		}
		for (int j = 0; j < maxThreads - 1 && j < selectedPacks.size(); j++)
		{
			int threadIndex = j;
			threads[j] = async(PackProcess_New, params, &cur, &index, &complete, threadIndex, &selectedPacks, &mallocedMemory, &type);
			status[j] = threads[j].wait_for(chrono::milliseconds(1));
		}
		for (int j = 0; j < maxThreads - 1 && j < selectedPacks.size(); j++)
		{
			if (status[j] != future_status::ready)
				threads[j].wait();
		}
	}

	if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
	{
		IOBusy = false;
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
	}
	if (params->byPath)
		CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
	//对时序在后的普通文件检索
	for (auto &file : selectedFiles)
	{
		// typeList.clear();
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
		vector<long> posList, bytesList;
		long copyBytes = 0;
		int err;
		if (params->byPath)
		{
			char *pathCode = params->pathCode;
			if (typeList.size() == 0)
				err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
			else
				err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList);
			for (int i = 0; i < bytesList.size(); i++)
			{
				copyBytes += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
			}
		}
		else
		{
			err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
			copyBytes = type.hasTime ? bytes + 8 : bytes;
		}
		if (err != 0)
		{
			continue;
		}

		char copyValue[copyBytes]; //将要拷贝的数值
		long sortPos = 0;
		int compareBytes = 0;
		if (params->byPath)
		{
			int lineCur = 0; //记录此行当前写位置
			for (int i = 0; i < bytesList.size(); i++)
			{
				int curBytes = typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
				memcpy(copyValue + lineCur, buff + posList[i], curBytes);
				lineCur += curBytes;
			}
			if (params->valueName != NULL)
			{
				sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, pathCodes, typeList);
				compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type) == 0 ? bytes : 0;
			}
		}
		else
		{
			memcpy(copyValue, buff + pos, copyBytes);
			compareBytes = bytes;
		}

		bool canCopy = false;
		if (params->compareType != CMP_NONE && compareBytes != 0) //可比较
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
					canCopy = true;
				}
				break;
			}
			case DB_CompareType::LT:
			{
				if (compareRes < 0)
				{
					canCopy = true;
				}
				break;
			}
			case DB_CompareType::GE:
			{
				if (compareRes >= 0)
				{
					canCopy = true;
				}
				break;
			}
			case DB_CompareType::LE:
			{
				if (compareRes <= 0)
				{
					canCopy = true;
				}
				break;
			}
			case DB_CompareType::EQ:
			{
				if (compareRes == 0)
				{
					canCopy = true;
				}
				break;
			}
			default:
				canCopy = true;
				break;
			}
		}
		else //不可比较，直接拷贝此数据
			canCopy = true;
		if (canCopy) //需要此数据
		{
			char *memory = new char[copyBytes];
			memcpy(memory, copyValue, copyBytes);
			cur += copyBytes;
			mallocedMemory.push_back(make_tuple(memory, copyBytes, sortPos, file.second));
		}
		delete[] buff;
	}
	sortResult(mallocedMemory, params, type);
	if (cur == 0)
	{
		buffer->buffer = NULL;
		buffer->bufferMalloced = 0;
		IOBusy = false;
		return StatusCode::NO_DATA_QUERIED;
	}
	//动态分配内存
	char typeNum = params->byPath ? typeList.size() : 1; //数据类型总数
	char *data = (char *)malloc(cur + (int)typeNum * 11 + 1);
	int startPos;																   //数据区起始位置
	if (!params->byPath)														   //根据变量名查询，仅单个变量
		startPos = CurrentTemplate.writeBufferHead(params->valueName, type, data); //写入缓冲区头，获取数据区起始位置
	else																		   //根据路径编码查询，可能有多个变量
		startPos = CurrentTemplate.writeBufferHead(params->pathCode, typeList, data);
	if (data == NULL)
	{
		buffer->buffer = NULL;
		buffer->bufferMalloced = 0;
		IOBusy = false;
		return StatusCode::BUFFER_FULL;
	}
	//拷贝数据
	cur = 0;
	if (params->order == TIME_DSC) //时间降序，从内存地址集中反向拷贝
	{
		for (int i = mallocedMemory.size() - 1; i >= 0; i--)
		{
			memcpy(data + cur + startPos, get<0>(mallocedMemory[i]), get<1>(mallocedMemory[i]));
			delete[] get<0>(mallocedMemory[i]);
			cur += get<1>(mallocedMemory[i]);
		}
	}
	else //否则按照默认顺序升序排列即可
	{
		for (auto &mem : mallocedMemory)
		{
			memcpy(data + cur + startPos, get<0>(mem), get<1>(mem));
			delete[] get<0>(mem);
			cur += get<1>(mem);
		}
	}
	buffer->bufferMalloced = 1;
	buffer->buffer = data;
	buffer->length = cur + startPos;
	IOBusy = false;
	return 0;
}

/**
 * @brief 根据路径编码或变量名在某一产线文件夹下的数据文件中查询指定条数最新的数据，
 *          数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note
 */
int DB_QueryLastRecords(DB_DataBuffer *buffer, DB_QueryParams *params)
{
	IOBusy = true;
	//确认当前模版
	if (TemplateManager::CheckTemplate(params->pathToLine) != 0 && ZipTemplateManager::CheckZipTemplate(params->pathToLine) != 0)
	{
		IOBusy = false;
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
	}
	int check = CheckQueryParams(params);
	if (check != 0)
	{
		IOBusy = false;
		return check;
	}
	vector<pair<string, long>> selectedFiles;

	//获取每个数据文件，并带有时间戳
	readDataFilesWithTimestamps(params->pathToLine, selectedFiles);

	//根据时间降序排序
	sortByTime(selectedFiles, TIME_DSC);

	//取排序后的文件中前queryNums个符合条件的文件的数据
	vector<tuple<char *, long, long, long>> mallocedMemory;
	long cur = 0; //记录已选中的数据总长度
	int err;
	/*<-----!!!警惕内存泄露!!!----->*/
	//先对时序在后的普通文件检索
	int selectedNum = 0;
	Extraction_Params Ext_Params;
	err = GetExtractionParams(Ext_Params, params);
	if (params->byPath && params->pathCode != NULL)
		CurrentTemplate.GetAllPathsByCode(params->pathCode, Ext_Params.pathCodes);
	if (err != 0)
		return err;
	char *rawBuff = nullptr;
	int startPos = 0;
	if (!Ext_Params.hasIMG) //当查询条件不含图片时，结果的总长度已确定
	{
		char typeNum = params->byPath ? (Ext_Params.typeList.size() == 0 ? 1 : Ext_Params.typeList.size()) : 1; //数据类型总数
		char head[(int)typeNum * 19 + 1];
		//数据区起始位置
		if (!params->byPath)																	  //根据变量名查询，仅单个变量
			startPos = CurrentTemplate.writeBufferHead(params->valueName, Ext_Params.type, head); //写入缓冲区头，获取数据区起始位置
		else																					  //根据路径编码查询，可能有多个变量
			startPos = CurrentTemplate.writeBufferHead(params->pathCode, Ext_Params.typeList, head);
		rawBuff = (char *)malloc(Ext_Params.copyBytes * params->queryNums + startPos);
		memcpy(rawBuff, head, startPos);
		cur = startPos;
	}
	for (auto &file : selectedFiles)
	{
		long len; //文件长度
		char *buff;
		DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
		buff = new char[len];
		DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
		if (file.first.find(".idbzip") != string::npos)
		{
			ReZipBuff(&buff, (int &)len, params->pathToLine);
		}
		if (Ext_Params.hasIMG)
			err = DataExtraction(mallocedMemory, Ext_Params, params, cur, file.second, buff);
		else
			err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, file.second);
		delete[] buff;
		if (err == 0)
			selectedNum++;
		else
		{
			if (Ext_Params.hasIMG)
				GarbageMemRecollection(mallocedMemory);
			else
				free(rawBuff);
			return err;
		}
		if (selectedNum == params->queryNums)
			break;
	}

	char *completeZiped = new char[CurrentTemplate.totalBytes];
	int rezipedlen = 0;
	ReZipBuff(&completeZiped, rezipedlen, params->pathToLine);
	if (selectedNum < params->queryNums)
	{
		//检索到的数量不够，继续从打包文件中获取
		int packNums = packManager.allPacks[params->pathToLine].size();
		for (int index = 1; selectedNum < params->queryNums && index <= packNums; index++)
		{
			auto pack = packManager.GetLastPack(params->pathToLine, index);
			PackFileReader packReader(pack.second.first, pack.second.second);
			if (packReader.packBuffer == NULL)
				continue;
			int fileNum;
			string templateName;
			packReader.ReadPackHead(fileNum, templateName);
			stack<pair<long, tuple<int, long, int>>> filestk;
			for (int i = 0; i < fileNum; i++)
			{
				long timestamp;
				int readLength, zipType;
				long dataPos = packReader.Next(readLength, timestamp, zipType);
				auto t = make_tuple(readLength, timestamp, zipType);
				filestk.push(make_pair(dataPos, t));
			}
			int i = fileNum; //指示当前节拍在包中的位序
			while (!filestk.empty())
			{
				i--;
				auto fileInfo = filestk.top();
				filestk.pop();
				long dataPos = fileInfo.first;
				int readLength = get<0>(fileInfo.second);
				long timestamp = get<1>(fileInfo.second);
				int zipType = get<2>(fileInfo.second);

				char *buff = nullptr, *img = nullptr;
				switch (zipType)
				{
				case 0:
				{
					buff = packReader.packBuffer + dataPos; //直接指向目标数据区，无需再拷贝数据
					break;
				}
				case 1:
				{
					buff = completeZiped;
					break;
				}
				case 2:
				{
					buff = new char[CurrentTemplate.totalBytes];
					memcpy(buff, packReader.packBuffer + dataPos, readLength);
					ReZipBuff(&buff, readLength);
					break;
				}
				default:
				{
					if (Ext_Params.hasIMG)
						GarbageMemRecollection(mallocedMemory);
					else
						free(rawBuff);
					return StatusCode::UNKNWON_DATAFILE;
				}
				}
				if (Ext_Params.hasIMG)
				{
					char *newBuffer;
					long len;
					err = params->byPath ? FindImage(&img, len, pack.first, i, params->pathCode) : FindImage(&img, len, pack.first, i, params->valueName);
					if (err != 0)
					{
						if (Ext_Params.hasIMG)
							GarbageMemRecollection(mallocedMemory);
						else
							free(rawBuff);
						return err;
					}
					newBuffer = new char[readLength + len];
					memcpy(newBuffer, buff, zipType == 1 ? CurrentTemplate.totalBytes : readLength);
					memcpy(newBuffer + readLength, img, len);
					if (zipType == 2)
						delete[] buff;
					buff = newBuffer;
					err = DataExtraction(mallocedMemory, Ext_Params, params, cur, timestamp, buff);
				}
				else
				{
					err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, timestamp);
				}

				if (zipType == 2 || Ext_Params.hasIMG)
					delete[] buff;
				if (err == 0)
					selectedNum++;
				else
				{
					if (Ext_Params.hasIMG)
						GarbageMemRecollection(mallocedMemory);
					else
						free(rawBuff);
					return err;
				}
				if (selectedNum == params->queryNums)
					break;
			}
		}
	}
	delete[] completeZiped;

	if (cur == startPos)
	{
		buffer->buffer = NULL;
		buffer->bufferMalloced = 0;
		IOBusy = false;
		return StatusCode::NO_DATA_QUERIED;
	}
	if (params->order != TIME_ASC && params->order != TIME_DSC && params->order != ODR_NONE) //时间排序仅需在拷贝时反向
		sortResult(mallocedMemory, params, Ext_Params.type);
	return WriteDataToBuffer(mallocedMemory, Ext_Params, params, buffer, cur);
}

/**
 * @brief 根据文件ID和路径编码在某一产线文件夹下的数据文件中查询数据，数据存放在新开辟的缓冲区buffer中
 * @param buffer    数据缓冲区
 * @param params    查询请求参数
 *
 * @return  0:success,
 *          others: StatusCode
 * @note 获取产线文件夹下的所有数据文件，找到带有指定ID的文件后读取，加载模版，根据模版找到变量在数据中的位置
 *          找到后开辟内存空间，将数据放入，将缓冲区首地址赋值给buffer
 */
int DB_QueryByFileID(DB_DataBuffer *buffer, DB_QueryParams *params)
{
	IOBusy = true;
	if (TemplateManager::CheckTemplate(params->pathToLine) != 0 && ZipTemplateManager::CheckZipTemplate(params->pathToLine) != 0)
	{
		IOBusy = false;
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
	}
	int check = CheckQueryParams(params);
	cout << "check = " << check << endl;
	if (check != 0)
	{
		IOBusy = false;
		return check;
	}
	cout << "check complete " << endl;
	if (params->pathCode == NULL)
		cout << "pathcode null" << endl;
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

	Extraction_Params Ext_Params;
	int err = GetExtractionParams(Ext_Params, params);
	if (err != 0)
		return err;
	if (params->queryNums == 1 || params->queryNums == 0)
	{
		if (params->fileIDend == NULL) //单个文件查询
		{
			auto packInfo = packManager.GetPackByID(params->pathToLine, fileid, 1);
			if (packInfo.first != NULL && packInfo.second != 0)
			{
				auto pack = packManager.GetPack(packInfo.first);
				if (pack.first != NULL && pack.second != 0)
				{
					PackFileReader packReader(pack.first, pack.second);
					int fileNum;
					string templateName;
					packReader.ReadPackHead(fileNum, templateName);
					for (int i = 0; i < fileNum; i++)
					{
						string fileID;
						int readLength, zipType;
						long dataPos = packReader.Next(readLength, fileID, zipType);
						if (fileID == fileid)
						{
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
								IOBusy = false;
								return StatusCode::UNKNWON_DATAFILE;
								break;
							}

							if (Ext_Params.hasIMG)
							{
								char *img = nullptr;
								long len;
								char *newBuffer;
								string path = packInfo.first;
								err = params->byPath ? FindImage(&img, len, path, i, params->pathCode) : FindImage(&img, len, path, i, params->valueName);
								if (err != 0)
								{
									errorCode = err;
									continue;
								}
								newBuffer = new char[readLength + len];
								memcpy(newBuffer, buff, zipType == 1 ? CurrentTemplate.totalBytes : readLength);
								memcpy(newBuffer + readLength, img, len);
								if (zipType == 2)
									delete[] buff;
								buff = newBuffer;
							}
							//获取数据的偏移量和字节数
							long bytes = 0, pos = 0;		 //单个变量
							vector<long> posList, bytesList; //多个变量
							long copyBytes = 0;
							int err;
							DataType type;
							vector<DataType> typeList;
							char *pathCode;
							if (params->byPath)
							{
								pathCode = params->pathCode;
								err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
								for (int i = 0; i < bytesList.size() && err == 0; i++)
								{
									copyBytes += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
								}
							}
							else
							{
								err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
								copyBytes = type.hasTime ? bytes + 8 : bytes;
							}
							if (err != 0)
							{
								buffer->buffer = NULL;
								buffer->bufferMalloced = 0;
								IOBusy = false;
								return err;
							}

							//开始拷贝数据

							char typeNum = typeList.size() == 0 ? 1 : typeList.size(); //数据类型总数

							char *data = (char *)malloc(copyBytes + 1 + (int)typeNum * 11);
							int startPos;
							if (!params->byPath)
								startPos = CurrentTemplate.writeBufferHead(params->valueName, type, data); //写入缓冲区头，获取数据区起始位置
							else
								startPos = CurrentTemplate.writeBufferHead(params->pathCode, typeList, data);
							if (data == NULL)
							{
								buffer->buffer = NULL;
								buffer->bufferMalloced = 0;
								IOBusy = false;
								return StatusCode::BUFFER_FULL;
							}

							buffer->bufferMalloced = 1;
							if (params->byPath)
							{
								long lineCur = 0;
								for (int i = 0; i < bytesList.size(); i++)
								{
									long curBytes = typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i]; //本次写字节数
									memcpy(data + startPos + lineCur, buff + posList[i], curBytes);
									lineCur += curBytes;
								}
							}
							else
								memcpy(data + startPos, buff + pos, copyBytes);
							delete[] buff;
							buffer->buffer = data;
							buffer->length = copyBytes + startPos;
							IOBusy = false;
							return 0;
						}
					}
				}
			}
			vector<string> dataFiles;
			readDataFiles(params->pathToLine, dataFiles);
			for (string &file : dataFiles)
			{
				string tmp = file;
				vector<string> vec = DataType::splitWithStl(tmp, "/");
				if (vec.size() == 0)
					continue;
				vec = DataType::splitWithStl(vec[vec.size() - 1], "_");
				if (vec.size() == 0)
					continue;
				if (vec[0] == fileid)
				{
					long len; //文件长度
					DB_GetFileLengthByPath(const_cast<char *>(file.c_str()), &len);
					char *buff = new char[len];
					DB_OpenAndRead(const_cast<char *>(file.c_str()), buff);
					if (file.find(".idbzip") != string::npos)
					{
						ReZipBuff(&buff, (int &)len, params->pathToLine);
					}

					//获取数据的偏移量和字节数
					long bytes = 0, pos = 0;		 //单个变量
					vector<long> posList, bytesList; //多个变量
					long copyBytes = 0;
					int err;
					DataType type;
					vector<DataType> typeList;
					char *pathCode;
					if (params->byPath)
					{
						pathCode = params->pathCode;
						err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
						for (int i = 0; i < bytesList.size(); i++)
						{
							copyBytes += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
						}
					}
					else
					{
						err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
						copyBytes = type.hasTime ? bytes + 8 : bytes;
					}
					if (err != 0)
					{
						buffer->buffer = NULL;
						buffer->bufferMalloced = 0;
						buffer->length = 0;
						IOBusy = false;
						return err;
					}
					if (len < pos)
					{
						buffer->buffer = NULL;
						buffer->bufferMalloced = 0;
						IOBusy = false;
						return StatusCode::UNKNWON_DATAFILE;
					}

					//开始拷贝数据

					char typeNum = typeList.size() == 0 ? 1 : typeList.size(); //数据类型总数

					// char *data = (char *)malloc(copyBytes + 1 + (int)typeNum * 11);
					char head[(int)typeNum * 19 + 1];
					int startPos;
					if (!params->byPath)
						startPos = CurrentTemplate.writeBufferHead(params->valueName, type, head); //写入缓冲区头，获取数据区起始位置
					else
						startPos = CurrentTemplate.writeBufferHead(params->pathCode, typeList, head);
					char *data = (char *)malloc(startPos + copyBytes);
					memcpy(data, head, startPos);
					if (data == NULL)
					{
						buffer->buffer = NULL;
						buffer->bufferMalloced = 0;
						IOBusy = false;
						return StatusCode::BUFFER_FULL;
					}

					buffer->bufferMalloced = 1;
					if (params->byPath)
					{
						long lineCur = 0;
						for (int i = 0; i < bytesList.size(); i++)
						{
							long curBytes = typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i]; //本次写字节数
							memcpy(data + startPos + lineCur, buff + posList[i], curBytes);
							lineCur += curBytes;
						}
					}
					else
						memcpy(data + startPos, buff + pos, copyBytes);
					buffer->buffer = data;
					buffer->length = copyBytes + startPos;
					IOBusy = false;
					return 0;
				}
			}
		}
		//根据首尾ID的多文件查询
		else
		{
			auto packsInfo = packManager.GetPackByIDs(params->pathToLine, fileid, fileidEnd, 1);
			bool firstIndexFound = false;
			vector<tuple<char *, long, long, long>> mallocedMemory;
			long cur = 0;
			string currentFileID = "";
			char *completeZiped = new char[CurrentTemplate.totalBytes];
			int rezipedlen = 0;
			ReZipBuff(&completeZiped, rezipedlen, params->pathToLine);
			char *rawBuff = nullptr;
			int startPos;
			if (!Ext_Params.hasIMG) //当查询条件不含图片时，结果的总长度已确定
			{
				char typeNum = params->byPath ? (Ext_Params.typeList.size() == 0 ? 1 : Ext_Params.typeList.size()) : 1; //数据类型总数
				char head[(int)typeNum * 19 + 1];
				//数据区起始位置
				if (!params->byPath)																	  //根据变量名查询，仅单个变量
					startPos = CurrentTemplate.writeBufferHead(params->valueName, Ext_Params.type, head); //写入缓冲区头，获取数据区起始位置
				else																					  //根据路径编码查询，可能有多个变量
					startPos = CurrentTemplate.writeBufferHead(params->pathCode, Ext_Params.typeList, head);
				string startNum = "", endNum = "";
				for (int i = 0; i < fileid.length(); i++)
				{
					if (isdigit(fileid[i]))
					{
						startNum += fileid[i];
					}
				}
				for (int i = 0; i < fileidEnd.length(); i++)
				{
					if (isdigit(fileidEnd[i]))
					{
						endNum += fileidEnd[i];
					}
				}
				int start = atoi(startNum.c_str());
				int end = atoi(endNum.c_str());
				//此处可能会分配多余的空间
				rawBuff = (char *)malloc(Ext_Params.copyBytes * (end - start) + startPos);
				memcpy(rawBuff, head, startPos);
				cur = startPos;
			}
			for (auto &packInfo : packsInfo)
			{
				if (packInfo.first == NULL && packInfo.second == 0)
					continue;
				auto pack = packManager.GetPack(packInfo.first);
				if (pack.first != NULL && pack.second != 0)
				{
					PackFileReader packReader(pack.first, pack.second);
					int fileNum;
					string templateName;
					packReader.ReadPackHead(fileNum, templateName);

					for (int i = 0; i < fileNum; i++)
					{
						if (currentFileID == fileidEnd)
							break;
						int readLength, zipType;
						long timestamp;
						long dataPos = packReader.Next(readLength, timestamp, currentFileID, zipType);

						if (firstIndexFound || currentFileID == fileid) //找到首个相同ID的文件，故直接取接下来的若干个文件，无需再比较
						{
							firstIndexFound = true;
							char *buff = nullptr, *img = nullptr;
							switch (zipType)
							{
							case 0:
							{
								buff = packReader.packBuffer + dataPos; //直接指向目标数据区，无需再拷贝数据
								break;
							}
							case 1:
							{
								buff = completeZiped;
								break;
							}
							case 2:
							{
								buff = new char[CurrentTemplate.totalBytes];
								memcpy(buff, packReader.packBuffer + dataPos, readLength);
								ReZipBuff(&buff, readLength);
								break;
							}
							default:
								if (Ext_Params.hasIMG)
									GarbageMemRecollection(mallocedMemory);
								else
									free(rawBuff);
								return err;
							}
							if (Ext_Params.hasIMG)
							{
								char *newBuffer;
								long len;
								string path = packInfo.first;
								err = params->byPath ? FindImage(&img, len, path, i, params->pathCode) : FindImage(&img, len, path, i, params->valueName);
								if (err != 0)
								{
									GarbageMemRecollection(mallocedMemory);
									return err;
								}
								newBuffer = new char[readLength + len];
								memcpy(newBuffer, buff, zipType == 1 ? CurrentTemplate.totalBytes : readLength);
								memcpy(newBuffer + readLength, img, len);
								if (zipType == 2)
									delete[] buff;
								buff = newBuffer;

								err = DataExtraction(mallocedMemory, Ext_Params, params, cur, timestamp, buff);
							}
							else
							{
								err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, timestamp);
							}
							if (zipType == 2 || Ext_Params.hasIMG)
								delete[] buff;
							if (err != 0)
							{
								if (Ext_Params.hasIMG)
									GarbageMemRecollection(mallocedMemory);
								else
									free(rawBuff);
								return err;
							}
						}
					}
				}
			}
			if (currentFileID != fileidEnd) //还未结束
			{
				vector<pair<string, long>> dataFiles;
				readDataFilesWithTimestamps(params->pathToLine, dataFiles);
				sortByTime(dataFiles, TIME_ASC);
				for (auto &file : dataFiles)
				{
					string tmp = file.first;
					vector<string> vec = DataType::splitWithStl(tmp, "/");
					if (vec.size() == 0)
						continue;
					vec = DataType::splitWithStl(vec[vec.size() - 1], "_");
					if (vec.size() == 0)
						continue;
					if (firstIndexFound || vec[0] == fileid)
					{
						firstIndexFound = true;
						currentFileID = file.first;
						long len; //文件长度
						DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
						char *buff = new char[len];
						DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
						if (file.first.find(".idbzip") != string::npos)
						{
							ReZipBuff(&buff, (int &)len, params->pathToLine);
						}
						if (Ext_Params.hasIMG)
							err = DataExtraction(mallocedMemory, Ext_Params, params, cur, file.second, buff);
						else
							err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, file.second);
						delete[] buff;
						if (err != 0)
						{
							if (Ext_Params.hasIMG)
								GarbageMemRecollection(mallocedMemory);
							else
								free(rawBuff);
							return err;
						}
					}
					if (currentFileID.find(fileidEnd) != string::npos)
						break;
				}
			}

			sortResult(mallocedMemory, params, Ext_Params.type);
			return WriteDataToBuffer(mallocedMemory, Ext_Params, params, buffer, cur);
		}
	}
	//根据首ID+数量的多文件查询
	else
	{
		auto packsInfo = packManager.GetPackByIDs(params->pathToLine, fileid, params->queryNums, 1);
		bool firstIndexFound = false;

		vector<tuple<char *, long, long, long>> mallocedMemory; //内存地址-长度-排序值偏移-时间戳元组集
		long cur = 0;
		char *completeZiped = new char[CurrentTemplate.totalBytes];
		int rezipedlen = 0;
		ReZipBuff(&completeZiped, rezipedlen, params->pathToLine);
		char *rawBuff = nullptr;
		int startPos;
		if (!Ext_Params.hasIMG) //当查询条件不含图片时，结果的总长度已确定
		{
			char typeNum = params->byPath ? (Ext_Params.typeList.size() == 0 ? 1 : Ext_Params.typeList.size()) : 1; //数据类型总数
			char head[(int)typeNum * 19 + 1];
			//数据区起始位置
			if (!params->byPath)																	  //根据变量名查询，仅单个变量
				startPos = CurrentTemplate.writeBufferHead(params->valueName, Ext_Params.type, head); //写入缓冲区头，获取数据区起始位置
			else																					  //根据路径编码查询，可能有多个变量
				startPos = CurrentTemplate.writeBufferHead(params->pathCode, Ext_Params.typeList, head);
			//此处可能会分配多余的空间
			rawBuff = (char *)malloc(Ext_Params.copyBytes * params->queryNums + startPos);
			memcpy(rawBuff, head, startPos);
			cur = startPos;
		}
		int scanNum = 0;
		for (auto &packInfo : packsInfo)
		{
			if (packInfo.first == NULL && packInfo.second == 0)
				continue;
			auto pack = packManager.GetPack(packInfo.first);

			if (pack.first != NULL && pack.second != 0)
			{
				PackFileReader packReader(pack.first, pack.second);
				int fileNum;
				string templateName;
				packReader.ReadPackHead(fileNum, templateName);

				for (int i = 0; i < fileNum; i++)
				{
					if (scanNum == params->queryNums)
						break;
					string fileID;
					int readLength, zipType;
					long timestamp;
					long dataPos = packReader.Next(readLength, timestamp, fileID, zipType);

					if (firstIndexFound || fileID == fileid) //找到首个相同ID的文件，故直接取接下来的若干个文件，无需再比较
					{
						firstIndexFound = true;

						char *buff = nullptr, *img = nullptr;
						switch (zipType)
						{
						case 0:
						{
							buff = packReader.packBuffer + dataPos;
							break;
						}
						case 1:
						{
							buff = completeZiped;
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
							if (Ext_Params.hasIMG)
								GarbageMemRecollection(mallocedMemory);
							else
								free(rawBuff);
							return err;
						}
						if (Ext_Params.hasIMG)
						{
							char *newBuffer;
							long len;
							string path = packInfo.first;
							err = params->byPath ? FindImage(&img, len, path, i, params->pathCode) : FindImage(&img, len, path, i, params->valueName);
							if (err != 0)
							{
								GarbageMemRecollection(mallocedMemory);
								return err;
							}
							newBuffer = new char[readLength + len];
							memcpy(newBuffer, buff, zipType == 1 ? CurrentTemplate.totalBytes : readLength);
							memcpy(newBuffer + readLength, img, len);
							if (zipType == 2)
								delete[] buff;
							buff = newBuffer;

							err = DataExtraction(mallocedMemory, Ext_Params, params, cur, timestamp, buff);
						}
						else
						{
							err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, timestamp);
						}
						scanNum++;
						if (zipType == 2 || Ext_Params.hasIMG)
							delete[] buff;
						if (err != 0)
						{
							if (Ext_Params.hasIMG)
								GarbageMemRecollection(mallocedMemory);
							else
								free(rawBuff);
							return err;
						}
					}
				}
			}
		}
		if (scanNum < params->queryNums)
		{
			vector<pair<string, long>> dataFiles;
			readDataFilesWithTimestamps(params->pathToLine, dataFiles);
			sortByTime(dataFiles, TIME_ASC);

			for (auto &file : dataFiles)
			{
				if (scanNum == params->queryNums)
					break;
				string tmp = file.first;
				vector<string> vec = DataType::splitWithStl(tmp, "/");
				if (vec.size() == 0)
					continue;
				vec = DataType::splitWithStl(vec[vec.size() - 1], "_");
				if (vec.size() == 0)
					continue;
				if (firstIndexFound || vec[0] == fileid)
				{

					firstIndexFound = true;
					long len; //文件长度
					DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
					char *buff = new char[len];
					DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
					if (file.first.find(".idbzip") != string::npos)
					{
						ReZipBuff(&buff, (int &)len, params->pathToLine);
					}
					if (Ext_Params.hasIMG)
						err = DataExtraction(mallocedMemory, Ext_Params, params, cur, file.second, buff);
					else
						err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, file.second);
					delete[] buff;
					if (err != 0)
					{
						if (Ext_Params.hasIMG)
							GarbageMemRecollection(mallocedMemory);
						else
							free(rawBuff);
						return err;
					}
				}
			}
		}
		if (cur == 0 || cur == startPos)
		{
			buffer->buffer = NULL;
			buffer->bufferMalloced = 0;
			buffer->length = 0;
			IOBusy = false;
			return StatusCode::NO_DATA_QUERIED;
		}
		sortResult(mallocedMemory, params, Ext_Params.type);
		return WriteDataToBuffer(mallocedMemory, Ext_Params, params, buffer, cur);
	}

	IOBusy = false;
	return StatusCode::DATAFILE_NOT_FOUND;
}
int main()
{
	// Py_Initialize();
	DataTypeConverter converter;
	DB_QueryParams params;
	params.pathToLine = "JinfeiSeven";
	params.fileID = "534243556";
	// params.fileIDend = "2500";
	params.fileIDend = NULL;
	char code[10];
	code[0] = (char)0;
	code[1] = (char)1;
	code[2] = (char)0;
	code[3] = (char)1;
	code[4] = 0;
	// code[4] = 'R';
	code[5] = (char)0;
	code[6] = 0;
	code[7] = (char)0;
	code[8] = (char)0;
	code[9] = (char)0;
	params.pathCode = code;
	params.valueName = "S1ON";
	// params.valueName = NULL;
	params.start = 1553728593562;
	params.end = 1751908603642;
	params.order = ODR_NONE;
	params.compareType = LT;
	params.compareValue = "110";
	params.queryType = FILEID;
	params.byPath = 0;
	params.queryNums = 100;
	DB_DataBuffer buffer;
	buffer.savePath = "/";
	// cout << settings("Pack_Mode") << endl;
	// vector<pair<string, long>> files;
	// readDataFilesWithTimestamps("", files);
	// Packer::Pack("/",files);
	auto startTime = std::chrono::system_clock::now();
	// char zeros[10] = {0};
	// memcpy(params.pathCode, zeros, 10);
	// DB_QueryLastRecords(&buffer, &params);
	// free(buffer.buffer);
	// buffer.buffer = nullptr;
	// DB_QueryLastRecords(&buffer, &params);
	// free(buffer.buffer);
	// DB_QueryLastRecords(&buffer, &params);
	// free(buffer.buffer);
	// DB_QueryLastRecords(&buffer, &params);
	DB_QueryByFileID(&buffer, &params);
	auto endTime = std::chrono::system_clock::now();
	// free(buffer.buffer);
	std::cout << "第一次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
	// free(buffer.buffer);
	// startTime = std::chrono::system_clock::now();
	// DB_QueryByTimespan_Single_New(&buffer, &params);

	// endTime = std::chrono::system_clock::now();
	// std::cout << "第二次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
	// free(buffer.buffer);
	// startTime = std::chrono::system_clock::now();
	// DB_QueryByTimespan(&buffer, &params);

	// endTime = std::chrono::system_clock::now();
	// std::cout << "第三次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;
	// free(buffer.buffer);

	if (buffer.bufferMalloced)
	{
		cout << buffer.length << endl;
		// for (int i = 0; i < buffer.length; i++)
		// {
		// 	cout << (int)*(char *)(buffer.buffer + i) << " ";
		// 	if (i % 11 == 0)
		// 		cout << endl;
		// }

		free(buffer.buffer);
	}
	// buffer.buffer = NULL;
	return 0;
}
