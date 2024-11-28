#include "MemoryManager.h"
#include "Process.h"
#include <algorithm>
#include <stdexcept>

void MemoryManager::initialize()
{
    std::lock_guard<std::mutex> lock(memoryMutex);

    if (initialized)
        return;

    auto &config = Config::getInstance();
    totalMemory = config.getMaxOverallMem() * 1024; // Convert KB to bytes
    pageSize = config.getMemPerFrame() * 1024;      // Convert KB to bytes

    // Determine allocation strategy
    usePageBasedAllocation = (totalMemory != pageSize);

    if (usePageBasedAllocation)
    {
        // Initialize page table
        size_t numFrames = totalMemory / pageSize;
        pageTable.resize(numFrames, {0, false, std::weak_ptr<Process>()});
    }
    else
    {
        // Initialize single memory block for flat allocation
        memoryBlocks.push_back({0, totalMemory, true, std::weak_ptr<Process>()});
    }

    usedMemory = 0;
    pagesPagedIn = 0;
    pagesPagedOut = 0;
    initialized = true;
}

bool MemoryManager::allocateMemory(std::shared_ptr<Process> process)
{
    if (!initialized)
    {
        throw std::runtime_error("Memory Manager not initialized");
    }

    std::lock_guard<std::mutex> lock(memoryMutex);
    return usePageBasedAllocation ? allocatePaged(process) : allocateFlat(process);
}

bool MemoryManager::allocateFlat(std::shared_ptr<Process> process)
{
    for (auto &block : memoryBlocks)
    {
        if (block.isFree && block.size >= process->getMemoryRequirement())
        {
            // Split block if necessary
            if (block.size > process->getMemoryRequirement())
            {
                MemoryBlock newBlock{
                    block.startAddress + process->getMemoryRequirement(),
                    block.size - process->getMemoryRequirement(),
                    true,
                    std::weak_ptr<Process>()};
                block.size = process->getMemoryRequirement();
                memoryBlocks.push_back(newBlock);
            }

            block.isFree = false;
            block.process = process;
            usedMemory += block.size;
            return true;
        }
    }

    swapOutOldestProcess();
    return false;
}

bool MemoryManager::allocatePaged(std::shared_ptr<Process> process)
{
    size_t numPagesNeeded = (process->getMemoryRequirement() + pageSize - 1) / pageSize;
    std::vector<uint32_t> allocatedFrames;

    if (findFreePages(numPagesNeeded, allocatedFrames))
    {
        processPages[process->getPID()] = allocatedFrames;
        for (uint32_t frame : allocatedFrames)
        {
            pageTable[frame].isPresent = true;
            pageTable[frame].process = process;
        }
        usedMemory += numPagesNeeded * pageSize;
        pagesPagedIn += numPagesNeeded;
        return true;
    }

    swapOutOldestProcess();
    return false;
}

void MemoryManager::deallocateMemory(std::shared_ptr<Process> process)
{
    std::lock_guard<std::mutex> lock(memoryMutex);

    if (usePageBasedAllocation)
    {
        auto it = processPages.find(process->getPID());
        if (it != processPages.end())
        {
            for (uint32_t frame : it->second)
            {
                pageTable[frame].isPresent = false;
                pageTable[frame].process.reset();
            }
            usedMemory -= it->second.size() * pageSize;
            processPages.erase(it);
        }
    }
    else
    {
        for (auto &block : memoryBlocks)
        {
            if (!block.process.expired() && block.process.lock() == process)
            {
                block.isFree = true;
                block.process.reset();
                usedMemory -= block.size;
            }
        }
        coalesceFreeBlocks();
    }
}

void MemoryManager::coalesceFreeBlocks()
{
    bool merged;
    do
    {
        merged = false;
        for (auto it = memoryBlocks.begin(); it != memoryBlocks.end() - 1; ++it)
        {
            auto next = it + 1;
            if (it->isFree && next->isFree)
            {
                it->size += next->size;
                memoryBlocks.erase(next);
                merged = true;
                break;
            }
        }
    } while (merged);
}

bool MemoryManager::findFreePages(size_t numPages, std::vector<uint32_t> &allocatedFrames)
{
    allocatedFrames.clear();

    for (size_t i = 0; i < pageTable.size(); i++)
    {
        if (!pageTable[i].isPresent)
        {
            allocatedFrames.push_back(i);
            if (allocatedFrames.size() == numPages)
            {
                return true;
            }
        }
    }

    return false;
}

void MemoryManager::swapOutOldestProcess()
{
    // Find oldest process and swap it out
    // Will be implemented when we add backing store
    pagesPagedOut++; // Track statistics
}