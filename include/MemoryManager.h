#include <unistd.h>
#include <sys/sysctl.h>
#include <memory>
class MemoryManager
{
private:
    long capacity;
    long used;

public:
    void *GetMemory(size_t size);
    void *GetExternalMemory(size_t size);
    void Recollect(void *mem);
};