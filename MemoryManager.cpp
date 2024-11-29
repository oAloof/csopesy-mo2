#include "MemoryManager.h"
#include "Process.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>

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

    size_t requiredBytes = process->getMemoryRequirement() * 1024;

    std::lock_guard<std::mutex> lock(memoryMutex); // Single lock scope

    // Calculate active memory within the lock
    size_t activeMemory = 0;
    for (const auto &pair : processPages)
    {
        auto proc = pageTable[pair.second[0]].process.lock();
        if (proc && proc->getState() != Process::WAITING)
        {
            activeMemory += proc->getMemoryRequirement() * 1024;
        }
    }

    // If not enough memory available, swap out oldest process
    while (activeMemory + requiredBytes > totalMemory)
    {
        bool swapped = swapOutOldestProcess();
        if (!swapped)
        {
            return false; // Failed to free enough memory
        }

        // Recalculate active memory after swap
        activeMemory = 0;
        for (const auto &pair : processPages)
        {
            auto proc = pageTable[pair.second[0]].process.lock();
            if (proc && proc->getState() != Process::WAITING)
            {
                activeMemory += proc->getMemoryRequirement() * 1024;
            }
        }
    }

    // Perform allocation
    bool success = usePageBasedAllocation ? allocatePaged(process) : allocateFlat(process);
    if (success)
    {
        usedMemory = activeMemory + requiredBytes;
    }
    return success;
}

bool MemoryManager::allocateFlat(std::shared_ptr<Process> process)
{
    size_t requiredBytes = process->getMemoryRequirement() * 1024; // Convert KB to bytes

    for (auto &block : memoryBlocks)
    {
        if (block.isFree && block.size >= requiredBytes)
        {
            // Split block if necessary
            if (block.size > requiredBytes)
            {
                MemoryBlock newBlock{
                    block.startAddress + requiredBytes,
                    block.size - requiredBytes,
                    true,
                    std::weak_ptr<Process>()};
                block.size = requiredBytes;
                memoryBlocks.push_back(newBlock);
            }

            block.isFree = false;
            block.process = process;
            usedMemory += requiredBytes; // Update used memory
            return true;
        }
    }

    return false;
}

bool MemoryManager::allocatePaged(std::shared_ptr<Process> process)
{
    size_t requiredBytes = process->getMemoryRequirement() * 1024;
    size_t numPagesNeeded = (requiredBytes + pageSize - 1) / pageSize;
    std::vector<uint32_t> allocatedFrames;

    if (findFreePages(numPagesNeeded, allocatedFrames))
    {
        processPages[process->getPID()] = allocatedFrames;
        for (uint32_t frame : allocatedFrames)
        {
            pageTable[frame].isPresent = true;
            pageTable[frame].process = process;
        }
        pagesPagedIn += numPagesNeeded;
        return true;
    }

    return false;
}

void MemoryManager::deallocateMemory(std::shared_ptr<Process> process)
{
    std::lock_guard<std::mutex> lock(memoryMutex);

    if (!process)
        return;

    if (usePageBasedAllocation)
    {
        auto it = processPages.find(process->getPID());
        if (it != processPages.end())
        {
            // Only update memory usage if process was active
            if (process->getState() != Process::WAITING)
            {
                size_t memToFree = process->getMemoryRequirement() * 1024;
                if (usedMemory >= memToFree)
                {
                    usedMemory -= memToFree;
                }
            }

            for (uint32_t frame : it->second)
            {
                pageTable[frame].isPresent = false;
                pageTable[frame].process.reset();
            }
            processPages.erase(it);
        }
    }
    else
    {
        // For flat memory
        for (auto &block : memoryBlocks)
        {
            if (!block.process.expired() && block.process.lock() == process)
            {
                // Only update memory usage if process was active
                if (process->getState() != Process::WAITING)
                {
                    if (usedMemory >= block.size)
                    {
                        usedMemory -= block.size;
                    }
                }
                block.isFree = true;
                block.process.reset();
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

bool MemoryManager::swapOutOldestProcess()
{
    // Already locked by allocateMemory
    std::shared_ptr<Process> oldestProcess = nullptr;
    std::chrono::system_clock::time_point oldestTime = std::chrono::system_clock::now();
    size_t memToFree = 0;
    size_t numPages = 0;
    int oldestPID = -1;

    if (usePageBasedAllocation)
    {
        for (const auto &pair : processPages)
        {
            for (uint32_t frame : pair.second)
            {
                if (!pageTable[frame].process.expired())
                {
                    auto process = pageTable[frame].process.lock();
                    if (process && process->getState() != Process::WAITING &&
                        process->getCreationTime() < oldestTime)
                    {
                        oldestProcess = process;
                        oldestTime = process->getCreationTime();
                        oldestPID = process->getPID();
                        memToFree = process->getMemoryRequirement() * 1024;
                        numPages = pair.second.size();
                    }
                }
            }
        }
    }
    else
    {
        for (const auto &block : memoryBlocks)
        {
            if (!block.process.expired())
            {
                auto process = block.process.lock();
                if (process && process->getState() != Process::WAITING &&
                    process->getCreationTime() < oldestTime)
                {
                    oldestProcess = process;
                    oldestTime = process->getCreationTime();
                    oldestPID = process->getPID();
                    memToFree = block.size;
                }
            }
        }
    }

    if (oldestProcess)
    {
        if (oldestProcess->getState() != Process::FINISHED)
        {
            oldestProcess->setState(Process::WAITING);
        }

        if (usePageBasedAllocation)
        {
            auto it = processPages.find(oldestPID);
            if (it != processPages.end())
            {
                pagesPagedOut += numPages;
                if (usedMemory >= memToFree)
                {
                    usedMemory -= memToFree;
                }

                for (uint32_t frame : it->second)
                {
                    pageTable[frame].isPresent = false;
                    pageTable[frame].process.reset();
                }
                processPages.erase(it);
            }
        }
        else
        {
            deallocateMemory(oldestProcess);
        }
        return true;
    }

    return false;
}

size_t MemoryManager::getUsedMemory() const
{
    std::lock_guard<std::mutex> lock(memoryMutex);

    size_t activeMemory = 0;
    if (usePageBasedAllocation)
    {
        for (const auto &pair : processPages)
        {
            auto proc = pageTable[pair.second[0]].process.lock();
            if (proc && (proc->getState() == Process::RUNNING || proc->getState() == Process::READY))
            {
                activeMemory += proc->getMemoryRequirement() * 1024;
            }
        }
    }
    else
    {
        for (const auto &block : memoryBlocks)
        {
            if (!block.process.expired() && !block.isFree)
            {
                auto proc = block.process.lock();
                if (proc && (proc->getState() == Process::RUNNING || proc->getState() == Process::READY))
                {
                    activeMemory += block.size;
                }
            }
        }
    }
    return activeMemory;
}