#include "ProcessManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include "Utils.h"
#include "MemoryManager.h"

void ProcessManager::createProcess(const std::string &name)
{
    if (name.empty())
    {
        throw std::runtime_error("Process name cannot be empty");
    }

    if (!MemoryManager::getInstance().isInitialized())
    {
        throw std::runtime_error("Memory Manager not initialized");
    }

    std::unique_lock<std::mutex> lock(processesMutex);
    if (processes.find(name) != processes.end())
    {
        throw std::runtime_error("Process with name '" + name + "' already exists");
    }

    try
    {
        // Create process first
        auto process = std::make_shared<Process>(nextPID++, name);

        // Try to allocate memory
        if (!MemoryManager::getInstance().allocateMemory(process))
        {
            throw std::runtime_error("Failed to allocate memory for process '" + name + "'");
        }

        processes[name] = process;
        lock.unlock();
        Scheduler::getInstance().addProcess(process);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to create process: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<Process> ProcessManager::getProcess(const std::string &name)
{
    std::shared_ptr<Process> result;
    {
        std::lock_guard<std::mutex> lock(processesMutex);
        auto it = processes.find(name);
        if (it != processes.end())
        {
            result = it->second;
        }
    }
    return result;
}

void ProcessManager::listProcesses()
{
    std::vector<std::shared_ptr<Process>> processSnapshot;
    int totalCores;
    size_t totalMemory = MemoryManager::getInstance().getTotalMemory();
    size_t usedMemory = MemoryManager::getInstance().getUsedMemory();
    int activeCount = 0;

    {
        std::lock_guard<std::mutex> lock(processesMutex);
        totalCores = Config::getInstance().getNumCPU();

        for (const auto &pair : processes)
        {
            processSnapshot.push_back(pair.second);
            if (pair.second->getState() == Process::RUNNING)
            {
                activeCount++;
            }
        }
    }

    // Display memory in KB
    std::cout << "Memory Usage: " << (usedMemory / 1024) << "KB/"
              << (totalMemory / 1024) << "KB ("
              << ((usedMemory * 100) / totalMemory) << "%)\n";
    std::cout << "CPU utilization: " << (activeCount * 100 / totalCores) << "%\n";
    std::cout << "Cores used: " << activeCount << "\n";
    std::cout << "Cores available: " << (totalCores - activeCount) << "\n\n";

    std::cout << "Running processes:\n";
    for (const auto &process : processSnapshot)
    {
        if (process->getState() == Process::RUNNING)
        {
            process->displayProcessInfo();
            // Memory requirement is already in KB
            std::cout << "Memory: " << process->getMemoryRequirement() << "KB\n";
        }
    }

    std::cout << "\nFinished processes:\n";
    for (const auto &process : processSnapshot)
    {
        if (process->getState() == Process::FINISHED)
        {
            process->displayProcessInfo();
        }
    }
}

void ProcessManager::startBatchProcessing()
{
    if (!Config::getInstance().isInitialized())
    {
        throw std::runtime_error("System must be initialized before starting batch processing");
    }

    {
        std::lock_guard<std::mutex> lock(batchMutex);
        if (!batchProcessingActive)
        {
            batchProcessingActive = true;
            batchProcessThread = std::thread(&ProcessManager::batchProcessingLoop, this);
        }
    }
}

void ProcessManager::stopBatchProcessing()
{
    {
        std::lock_guard<std::mutex> lock(batchMutex);
        if (batchProcessingActive)
        {
            batchProcessingActive = false;
            if (batchProcessThread.joinable())
            {
                batchProcessThread.join();
            }
        }
    }
}

void ProcessManager::batchProcessingLoop()
{
    int processCounter = 1;
    uint64_t lastCycle = Scheduler::getInstance().getCPUCycles();
    const uint64_t batchFreq = Config::getInstance().getBatchProcessFreq();

    while (batchProcessingActive)
    {
        uint64_t currentCycle = Scheduler::getInstance().getCPUCycles();

        // Check if enough cycles have passed since last process creation
        if ((currentCycle - lastCycle) >= batchFreq)
        {
            std::ostringstream processName;
            processName << "p" << std::setfill('0') << std::setw(2) << processCounter++;

            try
            {
                createProcess(processName.str());
                lastCycle = currentCycle; // Update last creation cycle
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error creating batch process: " << e.what() << std::endl;
            }
        }

        // Longer sleep time to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void ProcessManager::listProcessesWithMemory()
{
    std::lock_guard<std::mutex> lock(processesMutex);
    for (const auto &pair : processes)
    {
        auto process = pair.second;
        if (process && process->getState() == Process::RUNNING)
        {
            // Convert KB to MiB for display
            double memMiB = process->getMemoryRequirement() / 1024.0;
            std::cout << std::left << std::setw(10) << process->getName()
                      << std::fixed << std::setprecision(0) << memMiB << "MiB\n";
        }
    }
}

std::string ProcessManager::generateProcessName() const
{
    std::ostringstream oss;
    oss << 'p' << std::setfill('0') << std::setw(2) << nextPID;
    return oss.str();
}