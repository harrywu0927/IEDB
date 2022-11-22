#ifndef _MEMORY_MANAGER_H
#define _MEMORY_MANAGER_H
#endif
#include <unistd.h>
#include <sys/sysctl.h>
#include <memory>
#include <utils.hpp>
#ifndef __linux__
#include <mach/mach.h>
#endif
#include <unordered_map>
#include <unordered_set>
#include <set>

using namespace std;

#define MEMORY_PAGE_SIZE 4096
#define PAGE_NUM 1024
#define PAGE_THRESH MEMORY_PAGE_SIZE *PAGE_NUM / 2

enum Mem_Type
{
    BLOCK = 1,
    PAGE = 2
};

typedef unsigned char Byte;
typedef unsigned int PagesListID;
typedef unsigned int BlockID;

#ifndef __linux__
enum BYTE_UNITS
{
    BYTES = 0,
    KILOBYTES = 1,
    MEGABYTES = 2,
    GIGABYTES = 3
};

template <class T>
T convert_unit(T num, int to, int from = BYTES)
{
    for (; from < to; from++)
    {
        num /= 1024;
    }
    return num;
}
#endif
#ifdef __linux__
typedef struct MEMPACKED
{
    char name1[20];
    unsigned long MemTotal;
    char name2[20];
    unsigned long MemFree;
    char name3[20];
    unsigned long Buffers;
    char name4[20];
    unsigned long Cached;

} MEM_OCCUPY;

typedef struct os_line_data
{
    char *val;
    int len;
} os_line_data;

#endif

typedef struct IEDB_Memory
{
    void *content;
    size_t length;
    unsigned int ID; //若是page，ID为从属PagesList的ID
    Byte type;       // 1 = block, 2 = page
    size_t offset;
    bool operator<(const IEDB_Memory &right) const
    {
        if (this->ID == right.ID) //根据id去重
            return false;
        return this->length < right.length; //升序
    }
} IEDB_Memory;

typedef struct
{
    unsigned int offset; //页号
    unsigned int pages;  //页数
} Page;

/**
 * @brief 对于零散内存，采用分页制分配，每页大小为MEMORY_PAGE_SIZE
 *  不足MEMORY_PAGE_SIZE的部分依然分配MEMORY_PAGE_SIZE大小
 */
class MemoryPages
{
    friend class MemoryManager;

private:
    IEDB_Memory memPages;
    // Byte *startAddr;
    unsigned int totalPage;
    PagesListID ID;

public:
    MemoryPages();
    ~MemoryPages();
    bool InsertPages(size_t &size);
    void RecollectPages(Page &pages);
};

/**
 * @brief 数据库独立内存管理，对于较大内存块 Block（大于MEMORY_PAGE_SIZE）采用ID映射的方式分配和查找，小于MEMORY_PAGE_SIZE的内存使用 Page，采用页式管理
 *
 */
class MemoryManager
{
    friend class MemoryPage;

private:
    size_t capacity;
    size_t used;
    size_t available;
    Byte *startAddr;
    unordered_map<BlockID, IEDB_Memory> blockMap;
    unordered_map<PagesListID, MemoryPages> pagesMap;
    unordered_set<PagesListID> pagesSet;
    set<IEDB_Memory> emptyBlocks;
    BlockID AllocateID();

public:
    MemoryManager()
    {
        GetMemoryUsage(capacity, available);
        int percentage = atoi(settings("Memory_Alloc_Percentage").c_str());
        startAddr = new Byte[capacity * percentage / 100];
        IEDB_Memory mem;
        mem.content = startAddr;
        mem.ID = 0;
        mem.length = capacity * percentage / 100;
        mem.offset = 0;
        mem.type = Mem_Type::BLOCK;
        emptyBlocks.insert(mem);
        cout << "Memory allocate " << capacity * percentage / 100;
    };
    ~MemoryManager()
    {
        delete[] startAddr;
    };
    IEDB_Memory GetMemory(size_t size);
    void *GetExternalMemory(size_t size);
    void GetMemoryUsage(size_t &total, size_t &available);
    void Recollect(IEDB_Memory &mem);
};
extern MemoryManager memoryManager;