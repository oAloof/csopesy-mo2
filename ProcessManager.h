#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <map>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include "Process.h"
#include "Scheduler.h"

class ProcessManager
{
public:
    static ProcessManager &getInstance()
    {
        static ProcessManager instance;
        return instance;
    }

    void createProcess(const std::string &name);
    std::shared_ptr<Process> getProcess(const std::string &name);
    void listProcesses();
    void startBatchProcessing();
    void stopBatchProcessing();

private:
    ProcessManager() : nextPID(1), batchProcessingActive(false), lastProcessCreationCycle(0) {}
    ~ProcessManager() { stopBatchProcessing(); }

    std::map<std::string, std::shared_ptr<Process>> processes;
    std::atomic<int> nextPID;
    std::atomic<bool> batchProcessingActive;
    std::thread batchProcessThread;
    std::mutex processesMutex;
    std::mutex batchMutex;
    uint64_t lastProcessCreationCycle;

    void batchProcessingLoop();
    std::string generateProcessName() const;
};

#endif