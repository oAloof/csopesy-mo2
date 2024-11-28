#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <cstdint>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include "Config.h"

class Process;

struct MemoryBlock
{
    size_t startAddress;
    size_t size;
    bool isFree;
    std::weak_ptr<Process> process; // Owner process
};

struct Page
{
    uint32_t frameNumber;
    bool isPresent;
    std::weak_ptr<Process> process;
};

class MemoryManager
{
public:
    static MemoryManager &getInstance()
    {
        static MemoryManager instance;
        return instance;
    }

    // Initialize memory system
    void initialize();

    // Memory allocation methods
    bool allocateMemory(std::shared_ptr<Process> process);
    void deallocateMemory(std::shared_ptr<Process> process);

    // Memory status
    size_t getTotalMemory() const { return totalMemory; }
    size_t getUsedMemory() const { return usedMemory; }
    size_t getFreeMemory() const { return totalMemory - usedMemory; }
    bool isPageBasedAllocation() const { return usePageBasedAllocation; }

    // Memory statistics for vmstat
    uint64_t getPagesPagedIn() const { return pagesPagedIn; }
    uint64_t getPagesPagedOut() const { return pagesPagedOut; }

private:
    MemoryManager() : totalMemory(0), usedMemory(0), pagesPagedIn(0), pagesPagedOut(0),
                      usePageBasedAllocation(false), initialized(false) {}

    // Memory configuration
    size_t totalMemory;
    size_t usedMemory;
    bool usePageBasedAllocation;
    size_t pageSize;
    bool initialized;

    // Memory tracking
    std::vector<MemoryBlock> memoryBlocks;             // For flat allocation
    std::vector<Page> pageTable;                       // For paged allocation
    std::map<int, std::vector<uint32_t>> processPages; // Process ID to page numbers

    // Statistics
    uint64_t pagesPagedIn;
    uint64_t pagesPagedOut;

    // Thread safety
    mutable std::mutex memoryMutex;

    // Internal methods
    bool allocateFlat(std::shared_ptr<Process> process);
    bool allocatePaged(std::shared_ptr<Process> process);
    void coalesceFreeBlocks();
    bool findFreePages(size_t numPages, std::vector<uint32_t> &allocatedFrames);
    void swapOutOldestProcess();
};

#endif