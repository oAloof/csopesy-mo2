#include "ProcessManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include "Utils.h"

void ProcessManager::createProcess(const std::string &name)
{
    if (name.empty())
    {
        throw std::runtime_error("Process name cannot be empty");
    }

    std::unique_lock<std::mutex> lock(processesMutex);
    if (processes.find(name) != processes.end())
    {
        throw std::runtime_error("Process with name '" + name + "' already exists");
    }

    try
    {
        auto process = std::make_shared<Process>(nextPID++, name);
        processes[name] = process;
        // Release lock before scheduling to prevent deadlock
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
    int activeCount = 0;

    {
        std::lock_guard<std::mutex> lock(processesMutex);
        totalCores = Config::getInstance().getNumCPU();

        // Create a snapshot of processes to prevent holding the lock during output
        for (const auto &pair : processes)
        {
            processSnapshot.push_back(pair.second);
            if (pair.second->getState() == Process::RUNNING)
            {
                activeCount++;
            }
        }
    }

    std::cout << "CPU utilization: " << (activeCount * 100 / totalCores) << "%\n";
    std::cout << "Cores used: " << activeCount << "\n";
    std::cout << "Cores available: " << (totalCores - activeCount) << "\n\n";

    std::cout << "Running processes:\n";
    for (const auto &process : processSnapshot)
    {
        if (process->getState() == Process::RUNNING)
        {
            process->displayProcessInfo();
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

std::string ProcessManager::generateProcessName() const
{
    std::ostringstream oss;
    oss << 'p' << std::setfill('0') << std::setw(2) << nextPID;
    return oss.str();
}