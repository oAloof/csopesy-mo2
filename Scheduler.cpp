#include "Scheduler.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <ctime>
#include "Utils.h"

Scheduler::Scheduler()
{
    size_t numCPUs = Config::getInstance().getNumCPU();
    coreStatus.resize(numCPUs, false);
}

void Scheduler::startScheduling()
{
    if (isInitialized)
        return;

    processingActive = true;
    isInitialized = true;

    // Reset CPU cycles
    cpuCycles.store(0);

    int numCPUs = Config::getInstance().getNumCPU();
    for (int i = 0; i < numCPUs; ++i)
    {
        cpuThreads.emplace_back(&Scheduler::executeProcesses, this);
    }

    // Start the cycle counter thread
    cycleCounterActive = true;
    cycleCounterThread = std::thread(&Scheduler::cycleCounterLoop, this);
}

void Scheduler::stopScheduling()
{
    processingActive = false;
    cycleCounterActive = false;
    cv.notify_all();
    syncCv.notify_all();

    for (auto &thread : cpuThreads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    cpuThreads.clear();

    // Stop the cycle counter thread
    if (cycleCounterThread.joinable())
    {
        cycleCounterThread.join();
    }
}

void Scheduler::addProcess(std::shared_ptr<Process> process)
{
    if (!process)
        return;

    std::unique_lock<std::timed_mutex> lock(mutex, std::defer_lock);
    if (!lock.try_lock_for(std::chrono::milliseconds(100)))
    {
        return;
    }

    readyQueue.push(process);
    lock.unlock();

    cv.notify_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void Scheduler::executeProcesses()
{
    while (processingActive)
    {
        std::shared_ptr<Process> currentProcess = nullptr;
        bool hasProcess = false;

        {
            std::unique_lock<std::timed_mutex> lock(mutex);

            if (!readyQueue.empty())
            {
                hasProcess = true;
            }
            else
            {
                hasProcess = cv.wait_for(lock, std::chrono::milliseconds(100), [this]
                                         { return !processingActive || !readyQueue.empty(); });
            }

            if (!processingActive)
                break;

            if (hasProcess && !readyQueue.empty())
            {
                currentProcess = getNextProcess();
            }
        }

        if (currentProcess)
        {
            currentProcess->setState(Process::RUNNING);
            int delays = Config::getInstance().getDelaysPerExec();
            int currentDelay = 0;

            while (!currentProcess->isFinished() && processingActive)
            {
                if (Config::getInstance().getSchedulerType() == "rr" &&
                    currentProcess->getQuantumTime() >= Config::getInstance().getQuantumCycles())
                {
                    updateCoreStatus(currentProcess->getCPUCoreID(), false);
                    handleQuantumExpiration(currentProcess);
                    break;
                }

                if (currentDelay < delays)
                {
                    currentDelay++;
                }
                else
                {
                    currentProcess->executeCurrentCommand(currentProcess->getCPUCoreID());
                    currentProcess->moveToNextLine();
                    currentDelay = 0;

                    if (Config::getInstance().getSchedulerType() == "rr")
                    {
                        currentProcess->incrementQuantumTime();
                    }
                }

                waitForCycleSync();
            }

            {
                std::lock_guard<std::timed_mutex> lock(mutex);
                if (currentProcess->isFinished())
                {
                    currentProcess->setState(Process::FINISHED);
                    finishedProcesses.push_back(currentProcess);
                    updateCoreStatus(currentProcess->getCPUCoreID(), false);
                }
                else if (Config::getInstance().getSchedulerType() != "rr")
                {
                    currentProcess->setState(Process::READY);
                    readyQueue.push(currentProcess);
                }

                auto it = std::find(runningProcesses.begin(), runningProcesses.end(), currentProcess);
                if (it != runningProcesses.end())
                {
                    runningProcesses.erase(it);
                }
            }

            cv.notify_all();
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            cv.notify_all();
        }
    }
}

std::shared_ptr<Process> Scheduler::getNextProcess()
{
    if (readyQueue.empty())
    {
        return nullptr;
    }

    int availableCore = -1;
    for (size_t i = 0; i < coreStatus.size(); ++i)
    {
        if (!coreStatus[i])
        {
            availableCore = static_cast<int>(i);
            break;
        }
    }

    if (availableCore == -1)
    {
        return nullptr;
    }

    std::shared_ptr<Process> nextProcess;
    if (Config::getInstance().getSchedulerType() == "rr")
    {
        nextProcess = roundRobinSchedule();
    }
    else
    {
        nextProcess = fcfsSchedule();
    }

    if (nextProcess)
    {
        nextProcess->setCPUCoreID(availableCore);
        coreStatus[availableCore] = true;
        runningProcesses.push_back(nextProcess);
    }

    return nextProcess;
}

std::shared_ptr<Process> Scheduler::fcfsSchedule()
{
    if (readyQueue.empty())
    {
        return nullptr;
    }

    auto process = readyQueue.front();
    readyQueue.pop();
    return process;
}

std::shared_ptr<Process> Scheduler::roundRobinSchedule()
{
    if (readyQueue.empty())
        return nullptr;

    auto process = readyQueue.front();
    readyQueue.pop();

    if (isQuantumExpired(process))
    {
        handleQuantumExpiration(process);

        if (!readyQueue.empty())
        {
            process = readyQueue.front();
            readyQueue.pop();
        }
        else
        {
            return nullptr;
        }
    }

    return process;
}

bool Scheduler::isQuantumExpired(const std::shared_ptr<Process> &process) const
{
    return process->getQuantumTime() >= Config::getInstance().getQuantumCycles();
}

void Scheduler::handleQuantumExpiration(std::shared_ptr<Process> process)
{
    process->resetQuantumTime();
    process->setState(Process::READY);
    readyQueue.push(process);
}

void Scheduler::getCPUUtilization() const
{
    std::stringstream report;
    int totalCores;
    int usedCores;
    std::vector<std::shared_ptr<Process>> runningProcessesCopy;
    std::vector<std::shared_ptr<Process>> finishedProcessesCopy;

    {
        std::lock_guard<std::timed_mutex> lock(mutex);
        totalCores = Config::getInstance().getNumCPU();
        usedCores = runningProcesses.size();
        runningProcessesCopy = runningProcesses;
        finishedProcessesCopy = finishedProcesses;
    }

    report << "CPU utilization: " << (usedCores * 100 / totalCores) << "%\n";
    report << "Cores used: " << usedCores << "\n";
    report << "Cores available: " << (totalCores - usedCores) << "\n\n";

    report << "Running processes:\n";
    for (const auto &process : runningProcessesCopy)
    {
        report << process->getName()
               << " (" << formatTimestamp(std::chrono::system_clock::now()) << ")   "
               << "Core: " << process->getCPUCoreID() << "    "
               << process->getCommandCounter() << " / " << process->getLinesOfCode() << "\n";
    }

    report << "\nFinished processes:\n";
    for (const auto &process : finishedProcessesCopy)
    {
        report << process->getName()
               << " (" << formatTimestamp(std::chrono::system_clock::now()) << ")   "
               << "Finished    "
               << process->getLinesOfCode() << " / " << process->getLinesOfCode() << "\n";
    }

    std::cout << report.str();

    std::ofstream logFile("csopesy-log.txt", std::ios::app);
    if (logFile.is_open())
    {
        logFile << report.str() << "\n";
        logFile.close();
        std::cout << "Report generated at c:/csopesy-log.txt\n";
    }
}

void Scheduler::waitForCycleSync()
{
    const int CYCLE_SPEED = 1000; // Base timing in microseconds
    const int CYCLE_WAIT = 500;

    try
    {
        std::unique_lock<std::timed_mutex> syncLock(syncMutex);
        std::this_thread::sleep_for(std::chrono::microseconds(CYCLE_SPEED));

        int runningCount;
        {
            std::unique_lock<std::timed_mutex> lock(mutex);
            runningCount = runningProcesses.size();
        }

        if (runningCount == 0)
        {
            incrementCPUCycles();
            std::this_thread::sleep_for(std::chrono::microseconds(CYCLE_WAIT));
            return;
        }

        try
        {
            coresWaiting++;
            if (coresWaiting >= runningCount)
            {
                incrementCPUCycles();
                coresWaiting = 0;
                syncCv.notify_all();
                std::this_thread::sleep_for(std::chrono::microseconds(CYCLE_WAIT));
            }
            else
            {
                syncCv.wait_for(syncLock,
                                std::chrono::microseconds(CYCLE_WAIT),
                                [this]
                                { return coresWaiting == 0; });

                std::this_thread::sleep_for(std::chrono::microseconds(CYCLE_SPEED));

                if (coresWaiting > 0)
                {
                    coresWaiting = 0;
                    incrementCPUCycles();
                    syncCv.notify_all();
                }
            }
        }
        catch (...)
        {
            if (coresWaiting > 0)
            {
                coresWaiting--;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(CYCLE_SPEED));
            throw;
        }
    }
    catch (...)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(CYCLE_SPEED));
    }
}

void Scheduler::updateCoreStatus(int coreID, bool active)
{
    if (coreID >= 0 && coreID < static_cast<int>(coreStatus.size()))
    {
        coreStatus[coreID] = active;
        if (!active)
        {
            cv.notify_all();
        }
    }
}

void Scheduler::cycleCounterLoop()
{
    while (cycleCounterActive)
    {
        bool shouldSleep = false;
        {
            std::unique_lock<std::timed_mutex> lock(syncMutex);
            if (runningProcesses.empty() && readyQueue.empty())
            {
                incrementCPUCycles();
                shouldSleep = true;
            }
        }

        if (shouldSleep)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}
