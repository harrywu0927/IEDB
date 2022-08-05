#include <utils.hpp>
#include <spdlog/fmt/fmt.h>

unordered_map<int, string> strerr{
    {135, "Data type mismatch"},
    {136, "Template file not found"},
    {137, "Pathcode invalid"},
    {138, "Unknown data type"},
    {139, "Template resolution error"},
    {140, "Data file not found"},
    {141, "Unknown pathcode"},
    {142, "Unknown variable name"},
    {144, "Unknown data file"},
    {146, "Query type not support"},
    {149, "No data queried"},
    {150, "Variable name already exists"},
    {151, "Pathcode already exists"},
    {152, "Filename was modified"},
    {153, "Invalid timespan input"},
    {159, "Ziptype error"},
    {169, "Novel data detected"},
    {170, "Python script not found"},
    {171, "The specified method not found in python script"},
    {181, "Image not found in file"},
    {186, "Memory insufficient"},
    {187, "Backup file broken"},
    {189, "Data file was modified"}};

const char *iedb_err::what() const noexcept
{
    return code > 134 ? strerr[code].c_str() : strerror(code);
}

string iedb_err::paramInfo(DB_QueryParams *params)
{
    return fmt::format("Pathcode:{},Variables:{},Bypath:{},Start:{},End:{},QueryNum:{},Comparevalue:{},FileID:{},FileIDEnd:{},Pathtoline:{},Comparetype:{},Querytype:{},Ordertype:{},Sortvariable:{},Comparevariable:{}", params->pathCode == NULL ? "NULL" : params->pathCode, params->valueName == NULL ? "NULL" : params->valueName, params->byPath, params->start, params->end, params->queryNums, params->compareValue == NULL ? "NULL" : params->compareValue, params->fileID == NULL ? "NULL" : params->fileID, params->fileIDend == NULL ? "NULL" : params->fileIDend, params->pathToLine == NULL ? "NULL" : params->pathToLine, params->compareType, params->queryType, params->order, params->sortVariable == NULL ? "NULL" : params->sortVariable, params->compareVariable == NULL ? "NULL" : params->compareVariable);
}