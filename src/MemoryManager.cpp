#include <MemoryManager.h>

#ifdef __linux__
static char *os_getline(char *sin, os_line_data *line, char delim)
{
    char *out = sin;
    if (*out == '\0')
        return NULL;
    //	while (*out && (*out == delim)) { out++; }
    line->val = out;
    while (*out && (*out != delim))
    {
        out++;
    }
    line->len = out - line->val;
    //	while (*out && (*out == delim)) { out++; }
    if (*out && (*out == delim))
    {
        out++;
    }
    if (*out == '\0')
        return NULL;
    return out;
}
int Parser_EnvInfo(char *buffer, int size, MEM_OCCUPY *lpMemory)
{
    int state = 0;
    char *p = buffer;
    while (p)
    {
        os_line_data line = {0};
        p = os_getline(p, &line, ':');
        if (p == NULL || line.len <= 0)
            continue;

        if (line.len == 8 && strncmp(line.val, "MemTotal", 8) == 0)
        {
            char *point = strtok(p, " ");
            memcpy(lpMemory->name1, "MemTotal", 8);
            lpMemory->MemTotal = atol(point);
        }
        else if (line.len == 7 && strncmp(line.val, "MemFree", 7) == 0)
        {
            char *point = strtok(p, " ");
            memcpy(lpMemory->name2, "MemFree", 7);
            lpMemory->MemFree = atol(point);
        }
        else if (line.len == 7 && strncmp(line.val, "Buffers", 7) == 0)
        {
            char *point = strtok(p, " ");
            memcpy(lpMemory->name3, "Buffers", 7);
            lpMemory->Buffers = atol(point);
        }
        else if (line.len == 6 && strncmp(line.val, "Cached", 6) == 0)
        {
            char *point = strtok(p, " ");
            memcpy(lpMemory->name4, "Cached", 6);
            lpMemory->Cached = atol(point);
        }
    }
}

void get_procmeminfo(MEM_OCCUPY *lpMemory)
{
    FILE *fd;
    char buff[128] = {0};
    fd = fopen("/proc/meminfo", "r");
    if (fd < 0)
        return;
    fgets(buff, sizeof(buff), fd);
    Parser_EnvInfo(buff, sizeof(buff), lpMemory);

    fgets(buff, sizeof(buff), fd);
    Parser_EnvInfo(buff, sizeof(buff), lpMemory);

    fgets(buff, sizeof(buff), fd);
    Parser_EnvInfo(buff, sizeof(buff), lpMemory);

    fgets(buff, sizeof(buff), fd);
    Parser_EnvInfo(buff, sizeof(buff), lpMemory);

    fclose(fd);
}
#endif

/**
 * @brief Get the Memory Usage
 *
 * @param total
 * @param available
 */
void MemoryManager::GetMemoryUsage(size_t &total, size_t &available)
{
#ifdef __linux__
    MEM_OCCUPY mem;
    get_procmeminfo(&mem);
    total = mem.MemTotal;
    available = mem.MemFree;
#else
    u_int64_t total_mem = 0;
    float used_mem = 0;

    vm_size_t page_size;
    vm_statistics_data_t vm_stats;

    // Get total physical memory
    int mib[] = {CTL_HW, HW_MEMSIZE};
    size_t length = sizeof(total_mem);
    sysctl(mib, 2, &total_mem, &length, NULL, 0);

    mach_port_t mach_port = mach_host_self();
    mach_msg_type_number_t count = sizeof(vm_stats) / sizeof(natural_t);
    if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
        KERN_SUCCESS == host_statistics(mach_port, HOST_VM_INFO,
                                        (host_info_t)&vm_stats, &count))
    {
        used_mem = static_cast<float>(
            (vm_stats.active_count + vm_stats.wire_count) * page_size);
    }

    uint usedMem = convert_unit(static_cast<float>(used_mem), BYTES);
    uint totalMem = convert_unit(static_cast<float>(total_mem), BYTES);
    total = totalMem;
    available = totalMem - usedMem;
    // cout << "total:" << total << "\navail:" << available;
#endif
}

MemoryPages::MemoryPages()
{
    memPages = memoryManager.GetMemory(MEMORY_PAGE_SIZE * PAGE_NUM, Mem_Type::PAGE_LIST);
    totalPage = memPages.length / memPages.length;
}

MemoryPages::~MemoryPages()
{
}

BlockID MemoryManager::AllocateID()
{
}

IEDB_Memory MemoryManager::GetMemory(size_t size, Mem_Type type)
{
    IEDB_Memory mem;
    if (size <= PAGE_THRESH)
    {
        for (auto it = pagesSet.begin(); it != pagesSet.end(); it++)
        {
            if (pagesMap[*it].InsertPages(size))
            {
                mem.ID = *it;
                mem.length = size;
                mem.type = Mem_Type::PAGE;
                break;
            }
        }
        // Allocate a new pagelist.
        MemoryPages pages;
        pagesMap[pages.memPages.ID] = pages;
        pagesSet.insert(pages.memPages.ID);
        pages.InsertPages(size);
        mem.ID = pages.memPages.ID;
        mem.length = size;
        mem.type = Mem_Type::PAGE;
    }
    else
    {
        for (auto it = emptyBlocks.begin(); it != emptyBlocks.end(); it++)
        {
            if (it->length > size)
            {
                mem.length = size;
                mem.content = it->content;
                mem.offset = it->offset;
                //不可对空闲块属性原地修改，因为可能破坏set的有序性
                IEDB_Memory newMem;
                newMem.length = it->length - size;
                newMem.offset = it->offset + size;
                newMem.content = (char *)it->content + newMem.offset;
                emptyBlocks.erase(it);
                newMem.type = Mem_Type::BLOCK;
                emptyBlocks.insert(newMem);
                break;
            }
            else if (it->length == size)
            {
                emptyBlocks.erase(it);
                mem.setAttributes(it->content, size, 0, it->offset);
                break;
            }
        }
        mem.ID = AllocateID();
        mem.type = Mem_Type::BLOCK;
        if (type == Mem_Type::BLOCK)
            blockMap[mem.ID] = mem;
    }
    return mem;
}

void MemoryManager::Recollect(IEDB_Memory &mem)
{
    if (mem.type == Mem_Type::BLOCK)
    {
    }
    else if (mem.type == Mem_Type::PAGE)
    {
        pagesMap[mem.ID].RecollectPages(mem);
    }
}

bool MemoryPages::InsertPages(size_t &size)
{
}

void MemoryPages::RecollectPages(IEDB_Memory &pages)
{
}
