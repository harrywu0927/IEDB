/******************************************
 * @file Query.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.9.0
 * @date Last Modification in 2022-07-27
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
	if (path == NULL)
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
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
	if (pathToUnset == NULL)
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
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
	if (params->byPath != 0 && params->pathCode == NULL)
	{
		return StatusCode::PATHCODE_CHECK_ERROR;
	}
	if (params->compareType != CMP_NONE)
	{
		if (params->compareValue == NULL || params->compareVariable == NULL)
			return StatusCode::INVALID_QRY_PARAMS;
	}
	if (params->order != ODR_NONE)
	{
		if (params->sortVariable == NULL)
			return StatusCode::INVALID_QRY_PARAMS;
	}
	switch (params->queryType)
	{
	case TIMESPAN:
	{
		if (params->start > params->end && params->end != 0)
		{
			return StatusCode::INVALID_TIMESPAN;
		}
		else if (params->start == 0 && params->end == 0)
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
			else
			{
				long start = atol(params->fileID);
				long end = atol(params->fileIDend);
				if (start > end)
					return StatusCode::INVALID_QRY_PARAMS;
			}
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
 * @brief 检查文件是否人为增加或删除内容
 *
 * @param tem
 * @param buff
 * @param size
 */
void CheckFile(Template &tem, void *buff, size_t size)
{
	if (tem.hasImage)
	{
		DataTypeConverter converter;
		char *imgPos = (char *)buff + tem.totalBytes; //起始图片位置
		size_t imgOffset = tem.totalBytes;			  //当前偏移量
		int imgNum = 0;								  //图片个数
		for (auto &schema : tem.schemas)
		{
			if (schema.second.valueType == ValueType::IMAGE)
				imgNum++;
		}
		for (int i = 0; i < imgNum; i++)
		{
			if (imgOffset >= size)
			{
				throw iedb_err(StatusCode::DATAFILE_MODIFIED);
			}
			uint imgSize = converter.ToUInt16(imgPos) * converter.ToUInt16(imgPos + 2) * converter.ToUInt16(imgPos + 4);
			imgPos += 6 + imgSize;
			imgOffset += 6 + imgSize;
		}
		if (imgOffset < size)
			throw iedb_err(StatusCode::DATAFILE_MODIFIED);
	}
	else
	{
		if (tem.totalBytes != size)
			throw iedb_err(StatusCode::DATAFILE_MODIFIED);
	}
}

/**
 * @brief 提取数据时可用到的静态参数，避免DataExtration函数的参数过多从而略微影响性能
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
	// long bytes;
	// long pos;
	vector<string> names;
	int cmpIndex;
	int sortIndex;
	// DataType type;	// the type of the value to compare or sort
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
	int err;
	if (Qry_Params->valueName != NULL)
	{
		auto namesSplitedBySpace = DataType::splitWithStl(Qry_Params->valueName, " ");
		auto namesSplitedByComma = DataType::splitWithStl(Qry_Params->valueName, ",");
		auto names = namesSplitedByComma.size() > 1 ? namesSplitedByComma : namesSplitedBySpace;
		Ext_Params.names = names;
	}
	if (Qry_Params->byPath)
	{
		err = CurrentTemplate.FindMultiDatatypePosByCode(Qry_Params->pathCode, Ext_Params.posList, Ext_Params.bytesList, Ext_Params.typeList);
		Ext_Params.copyBytes = CurrentTemplate.GetBytesByCode(Qry_Params->pathCode);
		if (CurrentTemplate.GetAllPathsByCode(Qry_Params->pathCode, Ext_Params.pathCodes) != 0)
			return StatusCode::UNKNOWN_PATHCODE;
		// Find the index of variable to be sorted or compared in paths.
		if (Qry_Params->order != ODR_NONE)
		{
			string sortVariable = Qry_Params->sortVariable;
			bool found = false;
			int sortpos = 0;
			for (int i = 0; i < Ext_Params.pathCodes.size(); i++)
			{
				if (Ext_Params.pathCodes[i].name == sortVariable)
				{
					found = true;
					Ext_Params.sortIndex = i;
					if (Ext_Params.typeList[i].valueType == ValueType::IMAGE)
						return StatusCode::QUERY_TYPE_NOT_SURPPORT;
					break;
				}

				sortpos += DataType::GetTypeBytes(Ext_Params.typeList[i]);
			}
			if (!found)
				return StatusCode::UNKNOWN_VARIABLE_NAME;
			Ext_Params.sortPos = sortpos;
		}
		if (Qry_Params->compareType != CMP_NONE)
		{
			string cmpVariable = Qry_Params->compareVariable;
			bool found = false;
			for (int i = 0; i < Ext_Params.pathCodes.size(); i++)
			{
				if (Ext_Params.pathCodes[i].name == cmpVariable)
				{
					found = true;
					Ext_Params.cmpIndex = i;
					if (Ext_Params.typeList[i].valueType == ValueType::IMAGE)
						return StatusCode::QUERY_TYPE_NOT_SURPPORT;
					break;
				}
			}
			if (!found)
				return StatusCode::UNKNOWN_VARIABLE_NAME;
		}
	}
	else
	{
		err = CurrentTemplate.FindMultiDatatypePosByNames(Ext_Params.names, Ext_Params.posList, Ext_Params.bytesList, Ext_Params.typeList);
		Ext_Params.copyBytes = 0;
		for (auto &byte : Ext_Params.bytesList)
		{
			Ext_Params.copyBytes += byte;
		}
		if (Qry_Params->order != ODR_NONE)
		{
			string sortVariable = Qry_Params->sortVariable;
			bool found = false;
			int sortpos = 0;
			for (int i = 0; i < Ext_Params.names.size(); i++)
			{
				if (Ext_Params.names[i] == sortVariable)
				{
					found = true;
					Ext_Params.sortIndex = i;
					if (Ext_Params.typeList[i].valueType == ValueType::IMAGE)
						return StatusCode::QUERY_TYPE_NOT_SURPPORT;
					break;
				}

				sortpos += DataType::GetTypeBytes(Ext_Params.typeList[i]);
			}
			if (!found)
				return StatusCode::UNKNOWN_VARIABLE_NAME;
			Ext_Params.sortPos = sortpos;
		}
		if (Qry_Params->compareType != CMP_NONE)
		{
			string cmpVariable = Qry_Params->compareVariable;
			bool found = false;
			for (int i = 0; i < Ext_Params.names.size(); i++)
			{
				if (Ext_Params.names[i] == cmpVariable)
				{
					found = true;
					Ext_Params.cmpIndex = i;
					if (Ext_Params.typeList[i].valueType == ValueType::IMAGE)
						return StatusCode::QUERY_TYPE_NOT_SURPPORT;
					break;
				}
			}
			if (!found)
				return StatusCode::UNKNOWN_VARIABLE_NAME;
		}
	}
	Ext_Params.hasArray = Qry_Params->byPath ? CurrentTemplate.checkHasArray(Qry_Params->pathCode) : CurrentTemplate.checkHasArray(Ext_Params.names);
	Ext_Params.hasTS = Qry_Params->byPath ? CurrentTemplate.checkHasTimeseries(Qry_Params->pathCode) : CurrentTemplate.checkHasTimeseries(Ext_Params.names);
	Ext_Params.hasIMG = Qry_Params->byPath ? CurrentTemplate.checkHasImage(Qry_Params->pathCode) : CurrentTemplate.checkHasImage(Ext_Params.names);
	if (err != 0)
	{
		IOBusy = false;
		return err;
	}
	return 0;
}

/**
 * @brief 从文件中提取合适的数据到堆区的内存中，为保证兼容性，性能有损失，建议用于带图片的数据
 *
 * @param mallocedMemory 已分配内存
 * @param Ext_Params 提取参数
 * @param Qry_params 查询参数
 * @param cur 当前已选择的总字节数
 * @param timestamp 时间戳
 * @param buff 数据内容
 * @return 0 : 数据被选中，-1 : 数据未选中
 * @note 尽可能将函数的参数个数控制在7个以内
 */
int DataExtraction(vector<tuple<char *, long, long, long>> &mallocedMemory, Extraction_Params &Ext_Params, DB_QueryParams *Qry_params, long &cur, long &timestamp, char *buff)
{
	int err = 0;
	if (Ext_Params.hasIMG) //数据带有图片
	{
		Ext_Params.copyBytes = 0;
		Ext_Params.bytesList.clear();
		Ext_Params.posList.clear();
		if (Qry_params->byPath)
		{
			if (Ext_Params.typeList.size() == 0)
				err = CurrentTemplate.FindMultiDatatypePosByCode(Qry_params->pathCode, buff, Ext_Params.posList, Ext_Params.bytesList, Ext_Params.typeList);
			else
				err = CurrentTemplate.FindMultiDatatypePosByCode(Qry_params->pathCode, buff, Ext_Params.posList, Ext_Params.bytesList);
		}
		else
		{
			if (Ext_Params.typeList.size() == 0)
				err = CurrentTemplate.FindMultiDatatypePosByNames(Ext_Params.names, buff, Ext_Params.posList, Ext_Params.bytesList, Ext_Params.typeList);
			else
				err = CurrentTemplate.FindMultiDatatypePosByNames(Ext_Params.names, buff, Ext_Params.posList, Ext_Params.bytesList);
		}
		for (int i = 0; i < Ext_Params.bytesList.size() && err == 0; i++)
		{
			Ext_Params.copyBytes += Ext_Params.typeList[i].hasTime ? Ext_Params.bytesList[i] + 8 : Ext_Params.bytesList[i];
		}
		if (err != 0)
		{
			return err;
		}
	}

	char *copyValue = new char[Ext_Params.copyBytes];

	long lineCur = 0; //记录此行当前写位置
	for (int i = 0; i < Ext_Params.bytesList.size(); i++)
	{
		long curBytes = Ext_Params.typeList[i].hasTime ? Ext_Params.bytesList[i] + 8 : Ext_Params.bytesList[i];
		memcpy(copyValue + lineCur, buff + Ext_Params.posList[i], curBytes);
		lineCur += curBytes;
	}

	bool canCopy = false; //根据比较结果决定是否允许拷贝

	if (Qry_params->compareType != CMP_NONE && Ext_Params.compareBytes != 0 && !Ext_Params.typeList[Ext_Params.cmpIndex].isTimeseries && !Ext_Params.typeList[Ext_Params.cmpIndex].isArray) //可比较
	{
		// char value[Ext_Params.compareBytes]; //值缓存
		// memcpy(value, buff + Ext_Params.posList[Ext_Params.cmpIndex], Ext_Params.compareBytes);
		//根据比较结果决定是否加入结果集
		int compareRes = DataType::CompareValue(Ext_Params.typeList[Ext_Params.cmpIndex], buff + Ext_Params.posList[Ext_Params.cmpIndex], Qry_params->compareValue);
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
		cur += Ext_Params.copyBytes;
		mallocedMemory.push_back(make_tuple(copyValue, Ext_Params.copyBytes, Ext_Params.sortPos, timestamp));
		return 0;
	}
	else
		delete[] copyValue;
	return -1;
}

/**
 * @brief 从文件中提取合适的数据到堆区的内存中,针对非图片数据特别优化
 *
 * @param buffer 结果缓冲区
 * @param mallocedMemory 数据的内存信息
 * @param Ext_Params 提取参数
 * @param Qry_params 查询参数
 * @param cur 当前已选择的总字节数
 * @param data 原始数据内容
 * @param timestamp 时间戳
 * @return 0 : 数据被选中，-1 : 数据未选中
 * @note 尽可能将函数的参数个数控制在7个以内
 */
inline int DataExtraction_NonIMG(char *buffer, vector<tuple<char *, long, long, long>> &mallocedMemory, Extraction_Params &Ext_Params, DB_QueryParams *Qry_params, long &cur, char *data, long &timestamp)
{
	int lineCur = 0; //记录此行当前写位置
	for (int i = 0; i < Ext_Params.bytesList.size(); i++)
	{
		int curBytes = Ext_Params.typeList[i].hasTime ? Ext_Params.bytesList[i] + 8 : Ext_Params.bytesList[i];
		memcpy(buffer + cur + lineCur, data + Ext_Params.posList[i], curBytes);
		lineCur += curBytes;
	}

	bool canCopy = false; //根据比较结果决定是否允许拷贝

	if (Qry_params->compareType != CMP_NONE && Ext_Params.compareBytes != 0 && !Ext_Params.typeList[Ext_Params.cmpIndex].isTimeseries && !Ext_Params.typeList[Ext_Params.cmpIndex].isArray) //可比较
	{
		//根据比较结果决定是否加入结果集
		int compareRes = DataType::CompareValue(Ext_Params.typeList[Ext_Params.cmpIndex], data + Ext_Params.posList[Ext_Params.cmpIndex], Qry_params->compareValue);
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
		return 0;
	}
	return -1;
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
	char typeNum = Ext_Params.typeList.size(); //数据类型总数
	char head[(int)typeNum * 19 + 1];
	//数据区起始位置
	int startPos = params->byPath ? CurrentTemplate.writeBufferHead(params->pathCode, Ext_Params.typeList, head) : CurrentTemplate.writeBufferHead(Ext_Params.names, Ext_Params.typeList, head); //写入缓冲区头，获取数据区起始位置

	char *data = (char *)malloc(startPos + length);
	if (data == NULL)
	{
		buffer->buffer = NULL;
		buffer->bufferMalloced = 0;
		if (Ext_Params.hasIMG)
			GarbageMemRecollection(mallocedMemory);
		IOBusy = false;
		return StatusCode::BUFFER_FULL;
	}
	memcpy(data, head, startPos);
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
	if (Ext_Params.hasIMG)
		GarbageMemRecollection(mallocedMemory);
	buffer->bufferMalloced = 1;
	buffer->buffer = data;
	buffer->length = cur + startPos;
	IOBusy = false;
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
		vector<tuple<char *, long, long, long>> newMemory;
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
						free(get<0>(mallocedMemory[i])); //重复值，释放此内存
					// mallocedMemory.erase(mallocedMemory.begin() + i); //注：此操作在查询量大时效率可能很低
					isRepeat = true;
					// i--;
				}
			}
			if (!isRepeat) //不是重复值
			{
				existedValues.push_back(make_pair(value, type.valueBytes));
				newMemory.push_back(mallocedMemory[i]);
			}
		}
		mallocedMemory = newMemory;
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
int sortResult_NonIMG(vector<tuple<char *, long>> &mallocedMemory, long sortPos, DB_QueryParams *params, DataType &type)
{
	if (type.isArray || type.valueType == ValueType::IMAGE || type.isTimeseries)
		throw iedb_err(StatusCode::QUERY_TYPE_NOT_SURPPORT);
	switch (params->order)
	{
	case ASCEND: //升序
	{
		sort(mallocedMemory.begin(), mallocedMemory.end(),
			 [&type, &sortPos](tuple<char *, long> iter1, tuple<char *, long> iter2) -> bool
			 {
				 return DataType::CompareValueInBytes(type, std::get<0>(iter1) + sortPos, std::get<0>(iter2) + sortPos) < 0;
			 });
		break;
	}

	case DESCEND: //降序
	{
		sort(mallocedMemory.begin(), mallocedMemory.end(),
			 [&type, &sortPos](tuple<char *, long> iter1, tuple<char *, long> iter2) -> bool
			 {
				 return DataType::CompareValueInBytes(type, std::get<0>(iter1) + sortPos, std::get<0>(iter2) + sortPos) > 0;
			 });
		break;
	}
	case DISTINCT: //去除重复
	{
		bool hasIMG = params->byPath ? CurrentTemplate.checkHasImage(params->pathCode) : type.valueType == ValueType::IMAGE;
		vector<pair<char *, int>> existedValues;
		vector<tuple<char *, long>> newMemory;
		for (int i = 0; i < mallocedMemory.size(); i++)
		{
			char *value = std::get<0>(mallocedMemory[i]) + sortPos;
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
					isRepeat = true;
				}
			}
			if (!isRepeat) //不是重复值
			{
				existedValues.push_back(make_pair(value, type.valueBytes));
				newMemory.push_back(mallocedMemory[i]);
			}
		}
		mallocedMemory = newMemory;
		break;
	}
	case TIME_ASC:
	{
		sort(mallocedMemory.begin(), mallocedMemory.end(),
			 [](tuple<char *, long> iter1, tuple<char *, long> iter2) -> bool
			 {
				 return std::get<1>(iter1) < std::get<1>(iter2);
			 });
		break;
	}
	case TIME_DSC:
	{
		sort(mallocedMemory.begin(), mallocedMemory.end(),
			 [](tuple<char *, long> iter1, tuple<char *, long> iter2) -> bool
			 {
				 return std::get<1>(iter1) > std::get<1>(iter2);
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
	default:
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
	char zeros[10] = {0};
	params->pathCode = zeros;
	params->byPath = 1;
	return DB_ExecuteQuery(buffer, params);
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
int DB_QueryByTimespan(DB_DataBuffer *buffer, DB_QueryParams *params)
{
	IOBusy = true;
	int check = CheckQueryParams(params);
	if (check != 0)
	{
		IOBusy = false;
		buffer->bufferMalloced = 0;
		return check;
	}
	//确认当前模版
	if (TemplateManager::CheckTemplate(params->pathToLine) != 0 || ZipTemplateManager::CheckZipTemplate(params->pathToLine) != 0)
	{
		IOBusy = false;
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
	}

	int err;
	Extraction_Params Ext_Params;
	err = GetExtractionParams(Ext_Params, params);
	if (err != 0)
	{
		IOBusy = false;
		return err;
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

	//根据时间升序排序
	sortByTime(selectedFiles, TIME_ASC);

	vector<tuple<char *, long, long, long>> mallocedMemory; //内存地址-长度-排序值偏移-时间戳元组集
	long cur = 0;

	char *rawBuff = nullptr;
	int startPos;
	try
	{
		if (!Ext_Params.hasIMG) //当查询条件不含图片时，结果的总长度已确定
		{
			char typeNum = Ext_Params.typeList.size(); //数据类型总数
			char head[(int)typeNum * 19 + 1];
			startPos = params->byPath ? CurrentTemplate.writeBufferHead(params->pathCode, Ext_Params.typeList, head) : CurrentTemplate.writeBufferHead(Ext_Params.names, Ext_Params.typeList, head); //写入缓冲区头，获取数据区起始位置

			//此处可能会分配多余的空间，但最多在两个包的可接受大小内
			// int size = Ext_Params.copyBytes * (selectedFiles.size() + fileIDManager.GetPacksRhythmNum(selectedPacks)) + startPos;
			rawBuff = (char *)malloc(Ext_Params.copyBytes * (selectedFiles.size() + fileIDManager.GetPacksRhythmNum(selectedPacks)) + startPos);
			if (rawBuff == NULL)
			{
				throw bad_alloc();
			}
			memcpy(rawBuff, head, startPos);
			cur = startPos;
		}
	}
	catch (bad_alloc &e)
	{
		IOBusy = false;
		return StatusCode::MEMORY_INSUFFICIENT;
	}
	catch (iedb_err &e)
	{
		RuntimeLogger.critical(e.what());
		IOBusy = false;
		return e.code;
	}
	catch (const std::exception &e)
	{
		RuntimeLogger.critical("Unexpected error occured:{}", e.what());
		IOBusy = false;
		return -1;
	}

	/**
	 * @brief 完全压缩的数据还原后必然相同，因此仅需还原一次后保存备用
	 * 此处仅还原了非图片部分的数据，遇到图片后将图片与之拼接即可
	 */
	char *completeZiped = nullptr;
	char *tempBuff = nullptr;
	try
	{
		completeZiped = new char[CurrentTemplate.totalBytes];
		tempBuff = new char[CurrentTemplate.totalBytes];
		int rezipedlen = 0;
		ReZipBuff(&completeZiped, rezipedlen, params->pathToLine);
		CheckFile(CurrentTemplate, completeZiped, CurrentTemplate.totalBytes);
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
				long dataPos = packReader.Next(readLength, timestamp, zipType);
				if (timestamp < params->start || timestamp > params->end) //在时间区间外
					continue;
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
					buff = tempBuff;
					memcpy(buff, packReader.packBuffer + dataPos, readLength);
					ReZipBuff(&buff, readLength);
					break;
				}
				default:
					RuntimeLogger.critical("Unknown ziptype detected, data file {} may have broken.", pack.first);
					throw iedb_err(StatusCode::DATAFILE_MODIFIED);
				}
				if (Ext_Params.hasIMG)
				{
					char *newBuffer = nullptr;
					long len = 0;
					err = params->byPath ? FindImage(&img, len, pack.first, i, params->pathCode) : FindImage(&img, len, pack.first, i, Ext_Params.names);
					if (err != 0)
					{
						throw iedb_err(err);
					}
					newBuffer = new char[readLength + len];
					memcpy(newBuffer, buff, zipType == 1 ? CurrentTemplate.totalBytes : readLength);
					memcpy(newBuffer + readLength, img, len);
					// if (zipType == 2)
					// 	delete[] buff;
					buff = newBuffer;
					err = DataExtraction(mallocedMemory, Ext_Params, params, cur, timestamp, buff);
				}
				else
				{
					err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, timestamp);
				}

				if (err > 0)
				{
					throw iedb_err(err);
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
			if (fs::path(file.first).extension() == ".idbzip")
			{
				ReZipBuff(&buff, (int &)len, params->pathToLine);
			}
			CheckFile(CurrentTemplate, buff, len);
			if (Ext_Params.hasIMG)
				err = DataExtraction(mallocedMemory, Ext_Params, params, cur, file.second, buff);
			else
				err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, file.second);
			delete[] buff;
			if (err > 0)
			{
				throw iedb_err(err);
			}
		}
	}
	catch (bad_alloc &e)
	{
		RuntimeLogger.critical("Memory allocation failed!");
		if (Ext_Params.hasIMG)
			GarbageMemRecollection(mallocedMemory);
		else
			free(rawBuff);
		IOBusy = false;
		if (completeZiped != nullptr)
			delete[] completeZiped;
		if (tempBuff != nullptr)
			delete[] tempBuff;
		return StatusCode::MEMORY_INSUFFICIENT;
	}
	catch (iedb_err &e)
	{
		if (Ext_Params.hasIMG)
			GarbageMemRecollection(mallocedMemory);
		else
			free(rawBuff);
		IOBusy = false;
		delete[] completeZiped;
		delete[] tempBuff;
		RuntimeLogger.critical("Error occured : {}. Query params : {}", e.what(), e.paramInfo(params));
		return e.code;
	}
	catch (...)
	{
		if (Ext_Params.hasIMG)
			GarbageMemRecollection(mallocedMemory);
		else
			free(rawBuff);
		IOBusy = false;
		delete[] completeZiped;
		delete[] tempBuff;
		RuntimeLogger.critical(strerror(errno));
		return errno;
	}

	delete[] completeZiped;
	delete[] tempBuff;
	if (params->order != TIME_ASC && params->order != TIME_DSC && params->order != ODR_NONE) //时间排序仅需在拷贝时反向
		sortResult(mallocedMemory, params, Ext_Params.typeList[Ext_Params.sortIndex]);
	else if (params->order == ODR_NONE || params->order == TIME_ASC)
	{
		if (!Ext_Params.hasIMG)
		{
			buffer->buffer = rawBuff;
			buffer->bufferMalloced = 1;
			buffer->length = cur;
			IOBusy = false;
			return 0;
		}
	}
	if (cur == startPos && !Ext_Params.hasIMG)
	{
		buffer->buffer = NULL;
		buffer->length = 0;
		buffer->bufferMalloced = 0;
		IOBusy = false;
		if (rawBuff != nullptr)
			free(rawBuff);
		return StatusCode::NO_DATA_QUERIED;
	}
	err = WriteDataToBuffer(mallocedMemory, Ext_Params, params, buffer, cur);
	if (!Ext_Params.hasIMG)
		free(rawBuff);
	IOBusy = false;
	return err;
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
int PackProcess(pair<string, pair<char *, long>> *packInfo, DB_QueryParams *params, atomic<long> *cur, vector<tuple<char *, long, long, long>> *mallocedMemory, Extraction_Params *Ext_Params)
{
	auto pack = packInfo->second;
	PackFileReader packReader(pack.first, pack.second);
	if (packReader.packBuffer == NULL)
		return StatusCode::DATAFILE_NOT_FOUND;
	int fileNum;
	string templateName;
	packReader.ReadPackHead(fileNum, templateName);
	// if (TemplateManager::CheckTemplate(templateName) != 0)
	// 	return StatusCode::SCHEMA_FILE_NOT_FOUND;
	// vector<PathCode> pathCodes;
	// if (params->byPath)
	// {
	// 	CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
	// }
	char *completeZiped = new char[CurrentTemplate.totalBytes];
	char *tempBuff = new char[CurrentTemplate.totalBytes];
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
			buff = tempBuff;
			memcpy(buff, packReader.packBuffer + dataPos, readLength);
			ReZipBuff(&buff, readLength);
			break;
		}
		default:
			RuntimeLogger.critical("Unknown ziptype detected, data file {} may have broken.", packInfo->first);
			continue;
		}

		if (Ext_Params->hasIMG)
		{
			char *newBuffer;
			long len;
			string path = packInfo->first;
			int err = params->byPath ? FindImage(&img, len, path, i, params->pathCode) : FindImage(&img, len, path, i, Ext_Params->names);
			if (err != 0)
			{
				errorCode = err;
				continue;
			}
			newBuffer = new char[readLength + len];
			memcpy(newBuffer, buff, zipType == 1 ? CurrentTemplate.totalBytes : readLength);
			memcpy(newBuffer + readLength, img, len);
			// if (zipType == 2)
			// 	delete[] buff;
			buff = newBuffer;
		}
		//获取数据的偏移量和数据类型
		int err;
		if (Ext_Params->hasIMG) //数据带有图片
		{
			if (Ext_Params->typeList.size() == 0)
				err = params->byPath ? CurrentTemplate.FindMultiDatatypePosByCode(params->pathCode, buff, Ext_Params->posList, Ext_Params->bytesList, Ext_Params->typeList) : CurrentTemplate.FindMultiDatatypePosByNames(Ext_Params->names, buff, Ext_Params->posList, Ext_Params->bytesList, Ext_Params->typeList);
			else
				err = params->byPath ? CurrentTemplate.FindMultiDatatypePosByCode(params->pathCode, buff, Ext_Params->posList, Ext_Params->bytesList) : CurrentTemplate.FindMultiDatatypePosByNames(Ext_Params->names, buff, Ext_Params->posList, Ext_Params->bytesList);
			for (int i = 0; i < Ext_Params->bytesList.size(); i++)
			{
				Ext_Params->copyBytes += Ext_Params->typeList.at(i).hasTime ? Ext_Params->bytesList[i] + 8 : Ext_Params->bytesList[i];
			}
		}
		if (err != 0)
		{
			continue;
		}

		char *copyValue = new char[Ext_Params->copyBytes]; //将要拷贝的数值
		if (copyValue == nullptr)
		{
			cout << "memory null\n";
			return StatusCode::MEMORY_INSUFFICIENT;
		}
		int lineCur = 0; //记录此行当前写位置
		for (int i = 0; i < Ext_Params->bytesList.size(); i++)
		{
			int curBytes = Ext_Params->typeList.at(i).hasTime ? Ext_Params->bytesList[i] + 8 : Ext_Params->bytesList[i];
			memcpy(copyValue + lineCur, buff + Ext_Params->posList[i], curBytes);
			lineCur += curBytes;
		}
		// if (Ext_Params->hasIMG && (params->valueName != NULL || strcmp(params->valueName, "") == 0)) //此处，若编码为精确搜索，而又输入了不同的变量名，FindSortPosFromSelectedData将返回0
		// {
		// 	Ext_Params->sortPos = CurrentTemplate.FindSortPosFromSelectedData(Ext_Params->bytesList, params->valueName, pathCodes, Ext_Params->typeList);
		// 	Ext_Params->compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, Ext_Params->pos, Ext_Params->bytes, Ext_Params->type) == 0 ? Ext_Params->bytes : 0;
		// }

		bool canCopy = false;
		if (params->compareType != CMP_NONE && Ext_Params->compareBytes != 0 && !Ext_Params->typeList[Ext_Params->cmpIndex].isTimeseries && !Ext_Params->typeList[Ext_Params->cmpIndex].isArray) //可比较
		{
			//根据比较结果决定是否加入结果集
			int compareRes = DataType::CompareValue(Ext_Params->typeList[Ext_Params->cmpIndex], buff + Ext_Params->posList[Ext_Params->cmpIndex], params->compareValue);
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
			*cur += Ext_Params->copyBytes;
			auto t = make_tuple(copyValue, Ext_Params->copyBytes, Ext_Params->sortPos, timestamp);
			memMutex.lock();
			mallocedMemory->push_back(t);
			memMutex.unlock();
		}
		// if (zipType == 2 || Ext_Params->hasIMG)
		// 	delete[] buff;
	}
	delete[] completeZiped;
	delete[] tempBuff;
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
int PackProcess_NonIMG(pair<string, pair<char *, long>> *packInfo, DB_QueryParams *params, atomic<long> *cur, vector<tuple<char *, long, long, long>> *mallocedMemory, Extraction_Params *Ext_Params, char *rawBuff)
{
	auto pack = packInfo->second;
	if (pack.first == NULL)
		cout << "pack null\n";
	PackFileReader packReader(pack.first, pack.second);
	if (packReader.packBuffer == NULL)
		return StatusCode::DATAFILE_NOT_FOUND;
	int fileNum;
	string templateName;
	packReader.ReadPackHead(fileNum, templateName);
	// if (TemplateManager::CheckTemplate(templateName) != 0)
	// 	return StatusCode::SCHEMA_FILE_NOT_FOUND;
	// vector<PathCode> pathCodes;
	// if (params->byPath)
	// {
	// 	CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
	// }
	char *completeZiped = new char[CurrentTemplate.totalBytes];
	char *tempBuff = new char[CurrentTemplate.totalBytes];
	char *copyValue = new char[Ext_Params->copyBytes]; //将要拷贝的数值
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
			buff = tempBuff;
			memcpy(buff, packReader.packBuffer + dataPos, readLength);
			ReZipBuff(&buff, readLength);
			break;
		}
		default:
			continue;
		}
		//获取数据的偏移量和数据类型
		int err;

		int lineCur = 0; //记录此行当前写位置
		for (int i = 0; i < Ext_Params->bytesList.size(); i++)
		{
			int curBytes = Ext_Params->typeList.at(i).hasTime ? Ext_Params->bytesList[i] + 8 : Ext_Params->bytesList[i];
			memcpy(copyValue + lineCur, buff + Ext_Params->posList[i], curBytes);
			lineCur += curBytes;
		}

		bool canCopy = false;
		if (params->compareType != CMP_NONE && Ext_Params->compareBytes != 0 && !Ext_Params->typeList[Ext_Params->cmpIndex].isTimeseries && !Ext_Params->typeList[Ext_Params->cmpIndex].isArray) //可比较
		{
			//根据比较结果决定是否加入结果集
			int compareRes = DataType::CompareValue(Ext_Params->typeList[Ext_Params->cmpIndex], buff + Ext_Params->posList[Ext_Params->cmpIndex], params->compareValue);
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
			/* !此处不可在rawBuff后直接加上*cur，由于拷贝前cur的值可能被其他线程修改，故会产生数据错位! */
			long offset = cur->load(std::memory_order_seq_cst);
			cur->store(cur->load(std::memory_order_seq_cst) + Ext_Params->copyBytes, std::memory_order_seq_cst);
			// *cur += Ext_Params->copyBytes;
			memcpy(rawBuff + offset, copyValue, Ext_Params->copyBytes);

			auto t = make_tuple(rawBuff + offset, Ext_Params->copyBytes, Ext_Params->sortPos, timestamp);
			memMutex.lock();
			mallocedMemory->push_back(t);
			memMutex.unlock();
		}
		// if (zipType == 2 || Ext_Params->hasIMG)
		// 	memset(tempBuff,0,CurrentTemplate.totalBytes);
	}
	delete[] completeZiped;
	delete[] tempBuff;
	delete[] copyValue;
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
// int DB_QueryByTimespan(DB_DataBuffer *buffer, DB_QueryParams *params)
// {
// 	IOBusy = true;
// 	if (maxThreads <= 2) //线程数量<=2时，多线程已无必要
// 	{
// 		return DB_QueryByTimespan_Single(buffer, params);
// 	}
// 	//确认当前模版
// 	if (TemplateManager::CheckTemplate(params->pathToLine) != 0 && ZipTemplateManager::CheckZipTemplate(params->pathToLine) != 0)
// 	{
// 		IOBusy = false;
// 		return StatusCode::SCHEMA_FILE_NOT_FOUND;
// 	}
// 	/* 当变量数为1个时，使用单线程速度更快 */
// 	if (params->byPath)
// 	{
// 		vector<PathCode> vec;
// 		CurrentTemplate.GetAllPathsByCode(params->pathCode, vec);
// 		if (vec.size() == 1)
// 			return DB_QueryByTimespan_Single(buffer, params);
// 	}
// 	else
// 	{
// 		return DB_QueryByTimespan_Single(buffer, params);
// 	}
// 	int check = CheckQueryParams(params);
// 	if (check != 0)
// 	{
// 		IOBusy = false;
// 		return check;
// 	}
// 	int err;
// 	Extraction_Params Ext_Params;
// 	err = GetExtractionParams(Ext_Params, params);
// 	if (err != 0)
// 		return err;
// 	vector<pair<string, long>> filesWithTime, selectedFiles;
// 	auto selectedPacks = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
// 	//获取每个数据文件，并带有时间戳
// 	readDataFilesWithTimestamps(params->pathToLine, filesWithTime);

// 	//筛选落入时间区间内的文件
// 	for (auto &file : filesWithTime)
// 	{
// 		if (file.second >= params->start && file.second <= params->end)
// 		{
// 			selectedFiles.push_back(make_pair(file.first, file.second));
// 		}
// 	}

// 	vector<tuple<char *, long, long, long>> mallocedMemory; //当前已分配的内存地址-长度-排序值偏移-时间戳元组
// 	atomic<long> cur(0);									//已选择的数据总长

// 	char *rawBuff = nullptr;
// 	if (!Ext_Params.hasIMG)
// 	{
// 		char typeNum = Ext_Params.typeList.size(); //数据类型总数
// 		char head[(int)typeNum * 19 + 1];
// 		//数据区起始位置
// 		int startPos = params->byPath ? CurrentTemplate.writeBufferHead(params->pathCode, Ext_Params.typeList, head) : CurrentTemplate.writeBufferHead(Ext_Params.names, Ext_Params.typeList, head); //写入缓冲区头，获取数据区起始位置
// 		rawBuff = (char *)malloc(Ext_Params.copyBytes * (selectedFiles.size() + fileIDManager.GetPacksRhythmNum(selectedPacks)) + startPos);
// 		if (rawBuff == NULL)
// 			return StatusCode::MEMORY_INSUFFICIENT;
// 		memcpy(rawBuff, head, startPos);
// 		cur = startPos;
// 	}
// 	//先对时序在前的包文件检索
// 	if (selectedPacks.size() > 0)
// 	{
// 		int index = 0;
// 		future_status status[maxThreads - 1];
// 		future<int> f[maxThreads - 1];
// 		for (int j = 0; j < maxThreads - 1; j++)
// 		{
// 			status[j] = future_status::ready;
// 		}
// 		do
// 		{
// 			for (int j = 0; j < maxThreads - 1; j++) //留一个线程循环遍历线程集，确认每个线程的运行状态
// 			{
// 				if (status[j] == future_status::ready)
// 				{
// 					auto pk = make_pair(selectedPacks[index].first, packManager.GetPack(selectedPacks[index].first));
// 					if (!Ext_Params.hasIMG)
// 						f[j] = async(std::launch::async, PackProcess_NonIMG, &pk, params, &cur, &mallocedMemory, &Ext_Params, rawBuff);
// 					else
// 					{
// 						f[j] = async(std::launch::async, PackProcess, &pk, params, &cur, &mallocedMemory, &Ext_Params);
// 					}
// 					status[j] = f[j].wait_for(chrono::milliseconds(1));
// 					index++;
// 					if (index == selectedPacks.size())
// 						break;
// 				}
// 				else
// 				{
// 					status[j] = f[j].wait_for(chrono::milliseconds(1));
// 				}
// 			}
// 		} while (index < selectedPacks.size());
// 		for (int j = 0; j < maxThreads - 1; j++)
// 		{
// 			if (status[j] != future_status::ready)
// 				f[j].wait();
// 		}
// 	}

// 	//对时序在后的普通文件检索
// 	long cur_nonatomic = cur; //非原子类型,兼容DataExtraction
// 	for (auto &file : selectedFiles)
// 	{
// 		long len;
// 		DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
// 		if (len == 0)
// 		{
// 			cout << "file null\n";
// 			continue;
// 		}
// 		char *buff = new char[len];
// 		DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
// 		if (fs::path(file.first).extension() == "idbzip")
// 		{
// 			ReZipBuff(&buff, (int &)len, params->pathToLine);
// 		}
// 		if (Ext_Params.hasIMG)
// 			err = DataExtraction(mallocedMemory, Ext_Params, params, cur_nonatomic, file.second, buff);
// 		else
// 			err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur_nonatomic, buff, file.second);
// 		delete[] buff;
// 		if (err > 0)
// 			return err;
// 	}
// 	cur = cur_nonatomic;

// 	if (cur == 0)
// 	{
// 		buffer->buffer = NULL;
// 		buffer->bufferMalloced = 0;
// 		IOBusy = false;
// 		return StatusCode::NO_DATA_QUERIED;
// 	}
// 	if (params->order != TIME_ASC && params->order != TIME_DSC && params->order != ODR_NONE) //时间排序仅需在拷贝时反向
// 		sortResult(mallocedMemory, params, Ext_Params.typeList[Ext_Params.sortIndex]);
// 	else if (params->order == ODR_NONE || params->order == TIME_ASC)
// 	{
// 		if (!Ext_Params.hasIMG)
// 		{
// 			buffer->buffer = rawBuff;
// 			buffer->bufferMalloced = 1;
// 			buffer->length = cur;
// 			return 0;
// 		}
// 	}
// 	err = WriteDataToBuffer_New(mallocedMemory, Ext_Params, params, buffer, cur_nonatomic);
// 	if (!Ext_Params.hasIMG)
// 		free(rawBuff);
// 	return err;
// }

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
				char *memory = new char[copyBytes];
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
// int DB_QueryByTimespan_MT(DB_DataBuffer *buffer, DB_QueryParams *params)
// {
// 	IOBusy = true;
// 	if (maxThreads <= 2) //线程数量<=2时，多线程已无必要
// 	{
// 		return DB_QueryByTimespan_Single(buffer, params);
// 	}
// 	//确认当前模版
// 	if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
// 		return StatusCode::SCHEMA_FILE_NOT_FOUND;
// 	if (params->byPath)
// 	{
// 		vector<PathCode> vec;
// 		CurrentTemplate.GetAllPathsByCode(params->pathCode, vec);
// 		if (vec.size() == 1)
// 			return DB_QueryByTimespan_Single(buffer, params);
// 	}
// 	int check = CheckQueryParams(params);
// 	if (check != 0)
// 	{
// 		IOBusy = false;
// 		return check;
// 	}
// 	vector<pair<string, long>> filesWithTime, selectedFiles;
// 	auto selectedPacks = packManager.GetPacksByTime(params->pathToLine, params->start, params->end);
// 	//获取每个数据文件，并带有时间戳
// 	readDataFilesWithTimestamps(params->pathToLine, filesWithTime);

// 	//筛选落入时间区间内的文件
// 	for (auto &file : filesWithTime)
// 	{
// 		if (file.second >= params->start && file.second <= params->end)
// 		{
// 			selectedFiles.push_back(make_pair(file.first, file.second));
// 		}
// 	}

// 	vector<tuple<char *, long, long, long>> mallocedMemory; //当前已分配的内存地址-长度-排序值偏移-时间戳元组
// 	atomic<long> cur(0);									//已选择的数据总长
// 	// atomic<vector<tuple<char *, long, long, long>>> mallocedMemory;
// 	DataType type;
// 	vector<DataType> typeList, selectedTypes;
// 	vector<long> sortDataPoses; //按值排序时要比较的数据的偏移量
// 	vector<PathCode> pathCodes;
// 	typeList = CurrentTemplate.GetAllTypes(params->pathCode);
// 	//先对时序在前的包文件检索
// 	if (selectedPacks.size() > 0)
// 	{
// 		atomic<int> index(0);
// 		future<int> threads[maxThreads - 1];
// 		future_status status[maxThreads - 1];
// 		vector<int> complete(maxThreads - 1, 0);
// 		for (int j = 0; j < maxThreads - 1; j++)
// 		{
// 			status[j] = future_status::ready;
// 		}
// 		for (int j = 0; j < maxThreads - 1 && j < selectedPacks.size(); j++)
// 		{
// 			int threadIndex = j;
// 			threads[j] = async(PackProcess_New, params, &cur, &index, &complete, threadIndex, &selectedPacks, &mallocedMemory, &type);
// 			status[j] = threads[j].wait_for(chrono::milliseconds(1));
// 		}
// 		for (int j = 0; j < maxThreads - 1 && j < selectedPacks.size(); j++)
// 		{
// 			if (status[j] != future_status::ready)
// 				threads[j].wait();
// 		}
// 	}

// 	if (TemplateManager::CheckTemplate(params->pathToLine) != 0)
// 	{
// 		IOBusy = false;
// 		return StatusCode::SCHEMA_FILE_NOT_FOUND;
// 	}
// 	if (params->byPath)
// 		CurrentTemplate.GetAllPathsByCode(params->pathCode, pathCodes);
// 	//对时序在后的普通文件检索
// 	for (auto &file : selectedFiles)
// 	{
// 		// typeList.clear();
// 		long len;

// 		if (file.first.find(".idbzip") != string::npos)
// 		{
// 			DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
// 		}
// 		else if (file.first.find("idb") != string::npos)
// 		{
// 			DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
// 		}
// 		char *buff = new char[CurrentTemplate.totalBytes];
// 		DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
// 		if (file.first.find(".idbzip") != string::npos)
// 		{
// 			ReZipBuff(&buff, (int &)len, params->pathToLine);
// 		}

// 		//获取数据的偏移量和数据类型
// 		long pos = 0, bytes = 0;
// 		vector<long> posList, bytesList;
// 		long copyBytes = 0;
// 		int err;
// 		if (params->byPath)
// 		{
// 			char *pathCode = params->pathCode;
// 			if (typeList.size() == 0)
// 				err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList, typeList);
// 			else
// 				err = CurrentTemplate.FindMultiDatatypePosByCode(pathCode, buff, posList, bytesList);
// 			for (int i = 0; i < bytesList.size(); i++)
// 			{
// 				copyBytes += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
// 			}
// 		}
// 		else
// 		{
// 			err = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type);
// 			copyBytes = type.hasTime ? bytes + 8 : bytes;
// 		}
// 		if (err != 0)
// 		{
// 			continue;
// 		}

// 		char copyValue[copyBytes]; //将要拷贝的数值
// 		long sortPos = 0;
// 		int compareBytes = 0;
// 		if (params->byPath)
// 		{
// 			int lineCur = 0; //记录此行当前写位置
// 			for (int i = 0; i < bytesList.size(); i++)
// 			{
// 				int curBytes = typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
// 				memcpy(copyValue + lineCur, buff + posList[i], curBytes);
// 				lineCur += curBytes;
// 			}
// 			if (params->valueName != NULL)
// 			{
// 				sortPos = CurrentTemplate.FindSortPosFromSelectedData(bytesList, params->valueName, pathCodes, typeList);
// 				compareBytes = CurrentTemplate.FindDatatypePosByName(params->valueName, buff, pos, bytes, type) == 0 ? bytes : 0;
// 			}
// 		}
// 		else
// 		{
// 			memcpy(copyValue, buff + pos, copyBytes);
// 			compareBytes = bytes;
// 		}

// 		bool canCopy = false;
// 		if (params->compareType != CMP_NONE && compareBytes != 0) //可比较
// 		{
// 			char value[compareBytes]; //数值
// 			memcpy(value, buff + pos, compareBytes);
// 			//根据比较结果决定是否加入结果集
// 			int compareRes = DataType::CompareValue(type, value, params->compareValue);
// 			switch (params->compareType)
// 			{
// 			case DB_CompareType::GT:
// 			{
// 				if (compareRes > 0)
// 				{
// 					canCopy = true;
// 				}
// 				break;
// 			}
// 			case DB_CompareType::LT:
// 			{
// 				if (compareRes < 0)
// 				{
// 					canCopy = true;
// 				}
// 				break;
// 			}
// 			case DB_CompareType::GE:
// 			{
// 				if (compareRes >= 0)
// 				{
// 					canCopy = true;
// 				}
// 				break;
// 			}
// 			case DB_CompareType::LE:
// 			{
// 				if (compareRes <= 0)
// 				{
// 					canCopy = true;
// 				}
// 				break;
// 			}
// 			case DB_CompareType::EQ:
// 			{
// 				if (compareRes == 0)
// 				{
// 					canCopy = true;
// 				}
// 				break;
// 			}
// 			default:
// 				canCopy = true;
// 				break;
// 			}
// 		}
// 		else //不可比较，直接拷贝此数据
// 			canCopy = true;
// 		if (canCopy) //需要此数据
// 		{
// 			char *memory = new char[copyBytes];
// 			memcpy(memory, copyValue, copyBytes);
// 			cur += copyBytes;
// 			mallocedMemory.push_back(make_tuple(memory, copyBytes, sortPos, file.second));
// 		}
// 		delete[] buff;
// 	}
// 	sortResult(mallocedMemory, params, type);
// 	if (cur == 0)
// 	{
// 		buffer->buffer = NULL;
// 		buffer->bufferMalloced = 0;
// 		IOBusy = false;
// 		return StatusCode::NO_DATA_QUERIED;
// 	}
// 	//动态分配内存
// 	char typeNum = params->byPath ? typeList.size() : 1; //数据类型总数
// 	char *data = (char *)malloc(cur + (int)typeNum * 11 + 1);
// 	int startPos;																   //数据区起始位置
// 	if (!params->byPath)														   //根据变量名查询，仅单个变量
// 		startPos = CurrentTemplate.writeBufferHead(params->valueName, type, data); //写入缓冲区头，获取数据区起始位置
// 	else																		   //根据路径编码查询，可能有多个变量
// 		startPos = CurrentTemplate.writeBufferHead(params->pathCode, typeList, data);
// 	if (data == NULL)
// 	{
// 		buffer->buffer = NULL;
// 		buffer->bufferMalloced = 0;
// 		IOBusy = false;
// 		return StatusCode::BUFFER_FULL;
// 	}
// 	//拷贝数据
// 	cur = 0;
// 	if (params->order == TIME_DSC) //时间降序，从内存地址集中反向拷贝
// 	{
// 		for (int i = mallocedMemory.size() - 1; i >= 0; i--)
// 		{
// 			memcpy(data + cur + startPos, get<0>(mallocedMemory[i]), get<1>(mallocedMemory[i]));
// 			delete[] get<0>(mallocedMemory[i]);
// 			cur += get<1>(mallocedMemory[i]);
// 		}
// 	}
// 	else //否则按照默认顺序升序排列即可
// 	{
// 		for (auto &mem : mallocedMemory)
// 		{
// 			memcpy(data + cur + startPos, get<0>(mem), get<1>(mem));
// 			delete[] get<0>(mem);
// 			cur += get<1>(mem);
// 		}
// 	}
// 	buffer->bufferMalloced = 1;
// 	buffer->buffer = data;
// 	buffer->length = cur + startPos;
// 	IOBusy = false;
// 	return 0;
// }

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
	int check = CheckQueryParams(params);
	if (check != 0)
	{
		return check;
	}
	//确认当前模版
	if (TemplateManager::CheckTemplate(params->pathToLine) != 0 && ZipTemplateManager::CheckZipTemplate(params->pathToLine) != 0)
	{
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
	}

	int err;
	Extraction_Params Ext_Params;
	err = GetExtractionParams(Ext_Params, params);
	if (params->byPath && params->pathCode != NULL)
		CurrentTemplate.GetAllPathsByCode(params->pathCode, Ext_Params.pathCodes);
	if (err != 0)
	{
		return err;
	}
	IOBusy = true;
	vector<pair<string, long>> selectedFiles;

	//获取每个数据文件，并带有时间戳
	readDataFilesWithTimestamps(params->pathToLine, selectedFiles);

	//根据时间降序排序
	sortByTime(selectedFiles, TIME_DSC);

	//取排序后的文件中前queryNums个符合条件的文件的数据
	vector<tuple<char *, long, long, long>> mallocedMemory;
	long cur = 0; //记录已选中的数据总长度

	/*<-----!!!警惕内存泄露!!!----->*/
	//先对时序在后的普通文件检索
	int selectedNum = 0;

	char *rawBuff = nullptr;
	int startPos = 0;
	try
	{
		if (!Ext_Params.hasIMG) //当查询条件不含图片时，结果的总长度已确定
		{
			char typeNum = Ext_Params.typeList.size(); //数据类型总数
			char head[(int)typeNum * 19 + 1];
			//数据区起始位置
			startPos = params->byPath ? CurrentTemplate.writeBufferHead(params->pathCode, Ext_Params.typeList, head) : CurrentTemplate.writeBufferHead(Ext_Params.names, Ext_Params.typeList, head); //写入缓冲区头，获取数据区起始位置
			rawBuff = (char *)malloc(Ext_Params.copyBytes * params->queryNums + startPos);
			if (rawBuff == NULL)
			{
				throw bad_alloc();
			}
			memcpy(rawBuff, head, startPos);
			cur = startPos;
		}
	}
	catch (bad_alloc &e)
	{
		IOBusy = false;
		RuntimeLogger.critical("Bad allocation");
		return StatusCode::MEMORY_INSUFFICIENT;
	}
	catch (iedb_err &e)
	{
		RuntimeLogger.critical(e.what());
		IOBusy = false;
		return e.code;
	}
	catch (const std::exception &e)
	{
		RuntimeLogger.critical("Unexpected error occured:{}", e.what());
		IOBusy = false;
		return -1;
	}
	char *completeZiped = nullptr;
	char *tempBuff = nullptr;
	try
	{
		for (auto &file : selectedFiles)
		{
			long len; //文件长度
			char *buff;
			DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
			buff = new char[len];
			DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
			if (fs::path(file.first).extension() == "idbzip")
			{
				ReZipBuff(&buff, (int &)len, params->pathToLine);
			}
			CheckFile(CurrentTemplate, buff, len);
			if (Ext_Params.hasIMG)
				err = DataExtraction(mallocedMemory, Ext_Params, params, cur, file.second, buff);
			else
				err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, file.second);
			delete[] buff;
			if (err == 0)
				selectedNum++;
			else if (err != -1)
			{
				if (Ext_Params.hasIMG)
					GarbageMemRecollection(mallocedMemory);
				else
					free(rawBuff);
				IOBusy = false;
				return err;
			}
			if (selectedNum == params->queryNums)
				break;
		}

		completeZiped = new char[CurrentTemplate.totalBytes];
		tempBuff = new char[CurrentTemplate.totalBytes];
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
						buff = tempBuff;
						memcpy(buff, packReader.packBuffer + dataPos, readLength);
						ReZipBuff(&buff, readLength);
						break;
					}
					default:
					{
						RuntimeLogger.critical("Unknown ziptype detected, data file {} may have broken.", pack.first);
						throw iedb_err(StatusCode::DATAFILE_MODIFIED);
					}
					}
					if (Ext_Params.hasIMG)
					{
						char *newBuffer;
						long len;
						err = params->byPath ? FindImage(&img, len, pack.first, i, params->pathCode) : FindImage(&img, len, pack.first, i, Ext_Params.names);
						if (err != 0)
						{
							throw iedb_err(err);
						}
						newBuffer = new char[readLength + len];
						memcpy(newBuffer, buff, zipType == 1 ? CurrentTemplate.totalBytes : readLength);
						memcpy(newBuffer + readLength, img, len);
						buff = newBuffer;
						err = DataExtraction(mallocedMemory, Ext_Params, params, cur, timestamp, buff);
					}
					else
					{
						err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, timestamp);
					}

					if (err == 0)
						selectedNum++;
					else if (err > 0)
					{
						throw iedb_err(err);
					}
					if (selectedNum == params->queryNums)
						break;
				}
			}
		}
	}
	catch (bad_alloc &e)
	{
		RuntimeLogger.critical("Memory allocation failed!");
		if (Ext_Params.hasIMG)
			GarbageMemRecollection(mallocedMemory);
		else
			free(rawBuff);
		IOBusy = false;
		if (completeZiped != nullptr)
			delete[] completeZiped;
		if (tempBuff != nullptr)
			delete[] tempBuff;
		return StatusCode::MEMORY_INSUFFICIENT;
	}
	catch (iedb_err &e)
	{
		if (Ext_Params.hasIMG)
			GarbageMemRecollection(mallocedMemory);
		else
			free(rawBuff);
		IOBusy = false;
		delete[] completeZiped;
		delete[] tempBuff;
		RuntimeLogger.critical("Error occured : {}. Query params : {}", e.what(), e.paramInfo(params));
		return e.code;
	}
	catch (...)
	{
		if (Ext_Params.hasIMG)
			GarbageMemRecollection(mallocedMemory);
		else
			free(rawBuff);
		IOBusy = false;
		delete[] completeZiped;
		delete[] tempBuff;
		RuntimeLogger.critical(strerror(errno));
		return errno;
	}

	delete[] completeZiped;
	delete[] tempBuff;
	if (cur == startPos && !Ext_Params.hasIMG)
	{
		buffer->buffer = NULL;
		buffer->bufferMalloced = 0;
		IOBusy = false;
		if (rawBuff != nullptr)
			free(rawBuff);
		return StatusCode::NO_DATA_QUERIED;
	}
	if (params->order != TIME_DSC && params->order != ODR_NONE)
		sortResult(mallocedMemory, params, Ext_Params.typeList[Ext_Params.sortIndex]);
	else if (params->order == ODR_NONE || params->order == TIME_DSC)
	{
		if (!Ext_Params.hasIMG)
		{
			buffer->buffer = rawBuff;
			buffer->bufferMalloced = 1;
			buffer->length = cur;
			IOBusy = false;
			return 0;
		}
	}
	err = WriteDataToBuffer(mallocedMemory, Ext_Params, params, buffer, cur);
	if (!Ext_Params.hasIMG)
		free(rawBuff);
	IOBusy = false;
	return err;
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
	int check = CheckQueryParams(params);
	if (check != 0)
	{
		IOBusy = false;
		return check;
	}
	if (TemplateManager::CheckTemplate(params->pathToLine) != 0 && ZipTemplateManager::CheckZipTemplate(params->pathToLine) != 0)
	{
		IOBusy = false;
		return StatusCode::SCHEMA_FILE_NOT_FOUND;
	}

	Extraction_Params Ext_Params;
	int err = GetExtractionParams(Ext_Params, params);
	if (err != 0)
	{
		IOBusy = false;
		return err;
	}
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

	if (params->queryNums == 1 || params->queryNums == 0)
	{
		if (params->fileIDend == NULL) //单个文件查询
		{
			auto packInfo = packManager.GetPackByID(params->pathToLine, fileid, 1);
			if (packInfo.first != NULL && packInfo.second != 0)
			{
				auto pack = packManager.GetPack(packInfo.first);
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
						vector<long> posList, bytesList; //多个变量
						long copyBytes = 0;
						int err;
						DataType type;
						vector<DataType> typeList;
						// vector<string> names;

						err = params->byPath ? CurrentTemplate.FindMultiDatatypePosByCode(params->pathCode, buff, posList, bytesList, typeList) : CurrentTemplate.FindMultiDatatypePosByNames(Ext_Params.names, buff, posList, bytesList, typeList);
						for (int i = 0; i < bytesList.size() && err == 0; i++)
						{
							copyBytes += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
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

						char *data = (char *)malloc(copyBytes + 1 + (int)typeNum * 19);
						int startPos;
						if (!params->byPath)
							startPos = CurrentTemplate.writeBufferHead(Ext_Params.names, typeList, data); //写入缓冲区头，获取数据区起始位置
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
						long lineCur = 0;
						for (int i = 0; i < bytesList.size(); i++)
						{
							long curBytes = typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i]; //本次写字节数
							memcpy(data + startPos + lineCur, buff + posList[i], curBytes);
							lineCur += curBytes;
						}
						delete[] buff;
						buffer->buffer = data;
						buffer->length = copyBytes + startPos;
						IOBusy = false;
						return 0;
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
					if (fs::path(file).extension() == "idbzip")
					{
						ReZipBuff(&buff, (int &)len, params->pathToLine);
					}
					CheckFile(CurrentTemplate, buff, len);
					//获取数据的偏移量和字节数
					// long bytes = 0, pos = 0;		 //单个变量
					vector<long> posList, bytesList; //多个变量
					long copyBytes = 0;
					int err;
					DataType type;
					vector<DataType> typeList;
					err = params->byPath ? CurrentTemplate.FindMultiDatatypePosByCode(params->pathCode, buff, posList, bytesList, typeList) : CurrentTemplate.FindMultiDatatypePosByNames(Ext_Params.names, buff, posList, bytesList, typeList);
					for (int i = 0; i < bytesList.size() && err == 0; i++)
					{
						copyBytes += typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i];
					}
					if (err != 0)
					{
						buffer->buffer = NULL;
						buffer->bufferMalloced = 0;
						buffer->length = 0;
						IOBusy = false;
						return err;
					}

					//开始拷贝数据

					char typeNum = typeList.size(); //数据类型总数

					// char *data = (char *)malloc(copyBytes + 1 + (int)typeNum * 11);
					char head[(int)typeNum * 19 + 1];
					int startPos;
					if (!params->byPath)
						startPos = CurrentTemplate.writeBufferHead(Ext_Params.names, typeList, head); //写入缓冲区头，获取数据区起始位置
					else
						startPos = CurrentTemplate.writeBufferHead(params->pathCode, typeList, head);
					char *data = (char *)malloc(startPos + copyBytes);

					if (data == NULL)
					{
						buffer->buffer = NULL;
						buffer->bufferMalloced = 0;
						IOBusy = false;
						return StatusCode::BUFFER_FULL;
					}
					memcpy(data, head, startPos);
					buffer->bufferMalloced = 1;
					long lineCur = 0;
					for (int i = 0; i < bytesList.size(); i++)
					{
						// long curBytes = typeList[i].hasTime ? bytesList[i] + 8 : bytesList[i]; //本次写字节数
						memcpy(data + startPos + lineCur, buff + posList[i], bytesList[i]);
						lineCur += bytesList[i];
					}
					buffer->buffer = data;
					buffer->length = copyBytes + startPos;
					IOBusy = false;
					delete[] buff;
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

			char *rawBuff = nullptr;
			int startPos = 0;
			try
			{
				if (!Ext_Params.hasIMG) //当查询条件不含图片时，结果的总长度已确定
				{
					char typeNum = params->byPath ? (Ext_Params.typeList.size() == 0 ? 1 : Ext_Params.typeList.size()) : 1; //数据类型总数
					char head[(int)typeNum * 19 + 1];
					startPos = params->byPath ? CurrentTemplate.writeBufferHead(params->pathCode, Ext_Params.typeList, head) : CurrentTemplate.writeBufferHead(Ext_Params.names, Ext_Params.typeList, head); //写入缓冲区头，获取数据区起始位置
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
					//此处假设数据全部选中，可能会分配多余的空间
					rawBuff = (char *)malloc(Ext_Params.copyBytes * (end - start + 1) + startPos);
					if (rawBuff == NULL)
					{
						throw bad_alloc();
					}
					memcpy(rawBuff, head, startPos);
					cur = startPos;
				}
			}
			catch (bad_alloc &e)
			{
				IOBusy = false;
				RuntimeLogger.critical("Bad allocation");
				return StatusCode::MEMORY_INSUFFICIENT;
			}
			catch (iedb_err &e)
			{
				RuntimeLogger.critical(e.what());
				IOBusy = false;
				return e.code;
			}
			catch (const std::exception &e)
			{
				RuntimeLogger.critical("Unexpected error occured:{}", e.what());
				IOBusy = false;
				return -1;
			}
			char *completeZiped = nullptr;
			char *tempBuff = nullptr;
			try
			{
				completeZiped = new char[CurrentTemplate.totalBytes];
				tempBuff = new char[CurrentTemplate.totalBytes];
				int rezipedlen = 0;
				ReZipBuff(&completeZiped, rezipedlen, params->pathToLine);
				CheckFile(CurrentTemplate, completeZiped, CurrentTemplate.totalBytes);
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
									buff = tempBuff;
									memcpy(buff, packReader.packBuffer + dataPos, readLength);
									ReZipBuff(&buff, readLength);
									CheckFile(CurrentTemplate, buff, readLength);
									break;
								}
								default:
									RuntimeLogger.critical("Unknown ziptype detected, data file {} may have broken.", pack.first);
									throw iedb_err(StatusCode::DATAFILE_MODIFIED);
								}
								if (Ext_Params.hasIMG)
								{
									char *newBuffer;
									long len;
									string path = packInfo.first;
									err = params->byPath ? FindImage(&img, len, path, i, params->pathCode) : FindImage(&img, len, path, i, Ext_Params.names);
									if (err != 0)
									{
										throw iedb_err(err);
									}
									newBuffer = new char[readLength + len];
									memcpy(newBuffer, buff, zipType == 1 ? CurrentTemplate.totalBytes : readLength);
									memcpy(newBuffer + readLength, img, len);
									// if (zipType == 2)
									// 	delete[] buff;
									buff = newBuffer;

									err = DataExtraction(mallocedMemory, Ext_Params, params, cur, timestamp, buff);
								}
								else
								{
									err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, timestamp);
								}
								// if (zipType == 2 || Ext_Params.hasIMG)
								// 	delete[] buff;
								if (err > 0)
								{
									throw iedb_err(err);
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
						vector<string> vec = DataType::splitWithStl(fs::path(tmp).stem(), "_");
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
							if (fs::path(file.first).extension() == "idbzip")
							{
								ReZipBuff(&buff, (int &)len, params->pathToLine);
							}
							CheckFile(CurrentTemplate, buff, len);
							if (Ext_Params.hasIMG)
								err = DataExtraction(mallocedMemory, Ext_Params, params, cur, file.second, buff);
							else
								err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, file.second);
							delete[] buff;
							if (err > 0)
							{
								throw iedb_err(err);
							}
						}
						if (currentFileID.find(fileidEnd) != string::npos)
							break;
					}
				}
			}
			catch (bad_alloc &e)
			{
				RuntimeLogger.critical("Memory allocation failed!");
				if (Ext_Params.hasIMG)
					GarbageMemRecollection(mallocedMemory);
				else
					free(rawBuff);
				IOBusy = false;
				if (completeZiped != nullptr)
					delete[] completeZiped;
				if (tempBuff != nullptr)
					delete[] tempBuff;
				return StatusCode::MEMORY_INSUFFICIENT;
			}
			catch (iedb_err &e)
			{
				if (Ext_Params.hasIMG)
					GarbageMemRecollection(mallocedMemory);
				else
					free(rawBuff);
				IOBusy = false;
				delete[] completeZiped;
				delete[] tempBuff;
				cerr << "iedb_err catched\n";
				RuntimeLogger.critical("Error occured : {}. Query params : {}", e.what(), e.paramInfo(params));
				return e.code;
			}
			catch (...)
			{
				if (Ext_Params.hasIMG)
					GarbageMemRecollection(mallocedMemory);
				else
					free(rawBuff);
				IOBusy = false;
				delete[] completeZiped;
				delete[] tempBuff;
				RuntimeLogger.critical(strerror(errno));
				return errno;
			}

			delete[] tempBuff;
			delete[] completeZiped;

			if (cur == startPos && !Ext_Params.hasIMG)
			{
				buffer->buffer = NULL;
				buffer->length = 0;
				buffer->bufferMalloced = 0;
				IOBusy = false;
				if (rawBuff != nullptr)
					free(rawBuff);
				return StatusCode::NO_DATA_QUERIED;
			}
			if (params->order != ODR_NONE)
				sortResult(mallocedMemory, params, Ext_Params.typeList[Ext_Params.sortIndex]);
			if (!Ext_Params.hasIMG)
			{
				buffer->buffer = rawBuff;
				buffer->bufferMalloced = 1;
				buffer->length = cur;
				return 0;
			}
			err = WriteDataToBuffer(mallocedMemory, Ext_Params, params, buffer, cur);
			if (!Ext_Params.hasIMG)
				free(rawBuff);
			IOBusy = false;
			return err;
		}
	}
	//根据首ID+数量的多文件查询
	else
	{
		auto packsInfo = packManager.GetPackByIDs(params->pathToLine, fileid, params->queryNums, 1);
		bool firstIndexFound = false;

		vector<tuple<char *, long, long, long>> mallocedMemory; //内存地址-长度-排序值偏移-时间戳元组集
		long cur = 0;

		char *rawBuff = nullptr;
		int startPos;
		try
		{
			if (!Ext_Params.hasIMG) //当查询条件不含图片时，结果的总长度已确定
			{
				char typeNum = Ext_Params.typeList.size(); //数据类型总数
				char head[(int)typeNum * 19 + 1];
				startPos = params->byPath ? CurrentTemplate.writeBufferHead(params->pathCode, Ext_Params.typeList, head) : CurrentTemplate.writeBufferHead(Ext_Params.names, Ext_Params.typeList, head); //写入缓冲区头，获取数据区起始位置
				//此处可能会分配多余的空间
				rawBuff = (char *)malloc(Ext_Params.copyBytes * params->queryNums + startPos);
				if (rawBuff == NULL)
				{
					throw bad_alloc();
				}

				memcpy(rawBuff, head, startPos);
				cur = startPos;
			}
		}
		catch (bad_alloc &e)
		{
			IOBusy = false;
			return StatusCode::MEMORY_INSUFFICIENT;
		}
		catch (iedb_err &e)
		{
			RuntimeLogger.critical(e.what());
			IOBusy = false;
			return e.code;
		}
		catch (const std::exception &e)
		{
			RuntimeLogger.critical("Unexpected error occured:{}", e.what());
			IOBusy = false;
			return -1;
		}
		char *completeZiped = nullptr;
		char *tempBuff = nullptr;
		try
		{
			completeZiped = new char[CurrentTemplate.totalBytes];
			tempBuff = new char[CurrentTemplate.totalBytes];
			int rezipedlen = 0;
			ReZipBuff(&completeZiped, rezipedlen, params->pathToLine);
			CheckFile(CurrentTemplate, completeZiped, CurrentTemplate.totalBytes);
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
								buff = tempBuff;
								memcpy(buff, packReader.packBuffer + dataPos, readLength);
								ReZipBuff(&buff, readLength, params->pathToLine);
								break;
							}
							default:
								RuntimeLogger.critical("Unknown ziptype detected, data file {} may have broken.", pack.first);
								throw iedb_err(StatusCode::DATAFILE_MODIFIED);
							}
							if (Ext_Params.hasIMG)
							{
								char *newBuffer;
								long len;
								string path = packInfo.first;
								err = params->byPath ? FindImage(&img, len, path, i, params->pathCode) : FindImage(&img, len, path, i, Ext_Params.names);
								if (err != 0)
								{
									throw iedb_err(err);
								}
								newBuffer = new char[readLength + len];
								memcpy(newBuffer, buff, zipType == 1 ? CurrentTemplate.totalBytes : readLength);
								memcpy(newBuffer + readLength, img, len);
								// if (zipType == 2)
								// 	delete[] buff;
								buff = newBuffer;

								err = DataExtraction(mallocedMemory, Ext_Params, params, cur, timestamp, buff);
							}
							else
							{
								err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, timestamp);
							}
							scanNum++;
							// if (zipType == 2 || Ext_Params.hasIMG)
							// 	delete[] buff;
							if (err > 0)
							{
								throw iedb_err(err);
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
					auto vec = DataType::splitWithStl(fs::path(file.first).stem(), "_");
					if (vec.size() == 0)
					{
						RuntimeLogger.error("Unexpected file name format detected : {}", file.first);
						continue;
					}
					if (firstIndexFound || vec[0] == fileid)
					{
						firstIndexFound = true;
						long len; //文件长度
						DB_GetFileLengthByPath(const_cast<char *>(file.first.c_str()), &len);
						char *buff = new char[len];
						DB_OpenAndRead(const_cast<char *>(file.first.c_str()), buff);
						if (fs::path(file.first).extension() == "idbzip")
						{
							ReZipBuff(&buff, (int &)len, params->pathToLine);
						}
						CheckFile(CurrentTemplate, buff, len);
						if (Ext_Params.hasIMG)
							err = DataExtraction(mallocedMemory, Ext_Params, params, cur, file.second, buff);
						else
							err = DataExtraction_NonIMG(rawBuff, mallocedMemory, Ext_Params, params, cur, buff, file.second);
						delete[] buff;
						scanNum++;
						if (err > 0)
						{
							throw iedb_err(err);
						}
					}
				}
			}
		}
		catch (bad_alloc &e)
		{
			RuntimeLogger.critical("Memory allocation failed!");
			if (Ext_Params.hasIMG)
				GarbageMemRecollection(mallocedMemory);
			else
				free(rawBuff);
			IOBusy = false;
			if (completeZiped != nullptr)
				delete[] completeZiped;
			if (tempBuff != nullptr)
				delete[] tempBuff;
			return StatusCode::MEMORY_INSUFFICIENT;
		}
		catch (iedb_err &e)
		{
			if (Ext_Params.hasIMG)
				GarbageMemRecollection(mallocedMemory);
			else
				free(rawBuff);
			IOBusy = false;
			delete[] completeZiped;
			delete[] tempBuff;
			RuntimeLogger.critical("Error occured : {}. Query params : {}", e.what(), e.paramInfo(params));
			return e.code;
		}
		catch (...)
		{
			if (Ext_Params.hasIMG)
				GarbageMemRecollection(mallocedMemory);
			else
				free(rawBuff);
			IOBusy = false;
			delete[] completeZiped;
			delete[] tempBuff;
			RuntimeLogger.critical(strerror(errno));
			return errno;
		}

		delete[] tempBuff;
		delete[] completeZiped;
		if (cur == 0 || (cur == startPos && !Ext_Params.hasIMG))
		{
			buffer->buffer = NULL;
			buffer->bufferMalloced = 0;
			buffer->length = 0;
			IOBusy = false;
			return StatusCode::NO_DATA_QUERIED;
		}
		if (params->order != ODR_NONE)
			sortResult(mallocedMemory, params, Ext_Params.typeList[Ext_Params.sortIndex]);
		else if (!Ext_Params.hasIMG)
		{
			buffer->buffer = rawBuff;
			buffer->bufferMalloced = 1;
			buffer->length = cur;
			return 0;
		}
		err = WriteDataToBuffer(mallocedMemory, Ext_Params, params, buffer, cur);
		if (!Ext_Params.hasIMG)
			free(rawBuff);
		return err;
	}

	IOBusy = false;
	return StatusCode::DATAFILE_NOT_FOUND;
}
// int main()
// {
// 	// Py_Initialize();
// 	DataTypeConverter converter;
// 	DB_QueryParams params;
// 	params.pathToLine = "JinfeiSeven";
// 	params.fileID = "1527133";
// 	// params.fileIDend = "300000";
// 	params.fileIDend = NULL;
// 	char code[10];
// 	code[0] = (char)0;
// 	code[1] = (char)0;
// 	code[2] = (char)0;
// 	code[3] = (char)0;
// 	code[4] = 0;
// 	// code[4] = 'R';
// 	code[5] = (char)0;
// 	code[6] = 0;
// 	code[7] = (char)0;
// 	code[8] = (char)0;
// 	code[9] = (char)0;
// 	params.pathCode = code;
// 	// params.valueName = "S1ON,S1OFF";
// 	params.valueName = "S1ON";
// 	params.start = 1553728593562;
// 	params.end = 1751908603642;
// 	// params.end = 1651894834176;
// 	params.order = ODR_NONE;
// 	params.sortVariable = "S1ON";
// 	params.compareType = CMP_NONE;
// 	params.compareValue = "100";
// 	params.compareVariable = "S1ON";
// 	params.queryType = FILEID;
// 	params.byPath = 0;
// 	params.queryNums = 100;
// 	DB_DataBuffer buffer;
// 	buffer.savePath = "/";
// 	// cout << settings("Pack_Mode") << endl;
// 	// vector<pair<string, long>> files;
// 	// readDataFilesWithTimestamps("", files);
// 	// Packer::Pack("/",files);
// 	auto startTime = std::chrono::system_clock::now();
// 	// char zeros[10] = {0};
// 	// memcpy(params.pathCode, zeros, 10);
// 	cout << DB_QueryByFileID(&buffer, &params);
// 	// return 0;
// 	// DB_QueryLastRecords(&buffer, &params);
// 	// DB_QueryByTimespan_Single(&buffer, &params);
// 	auto endTime = std::chrono::system_clock::now();
// 	// free(buffer.buffer);
// 	std::cout << "第一次查询耗时:" << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << std::endl;

// 	if (buffer.bufferMalloced)
// 	{
// 		cout << buffer.length << endl;
// 		for (int i = 0; i < 50; i++)
// 		{
// 			cout << (int)*(char *)(buffer.buffer + i) << " ";
// 			if (i % 11 == 0)
// 				cout << endl;
// 		}

// 		free(buffer.buffer);
// 	}
// 	// buffer.buffer = NULL;
// 	return 0;
// }
