#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <random>
#include "ICommand.h"
#include "Config.h"
#include "PrintCommand.h"

class Process
{
public:
    enum ProcessState
    {
        READY,
        RUNNING,
        WAITING,
        FINISHED
    };

    // Constructor
    Process(int pid, const std::string &name);

    // Command management
    void addCommand(ICommand::CommandType commandType);
    void executeCurrentCommand(int coreID);
    void moveToNextLine();

    // Process status
    bool isFinished();
    int getCommandCounter();
    int getLinesOfCode();
    ProcessState getState();
    void setState(ProcessState state);
    std::chrono::system_clock::time_point getCreationTime() const { return creationTime; }

    // Core assignment
    void setCPUCoreID(int cpuCoreID);
    int getCPUCoreID();

    // Process identification
    int getPID() const;
    std::string getName() const;

    // Round Robin support
    void resetQuantumTime() { quantumTime = 0; }
    uint32_t getQuantumTime() { return quantumTime.load(); }
    void incrementQuantumTime() { ++quantumTime; }

    // Process-smi command
    void displayProcessInfo();

    size_t getMemoryRequirement() const { return memoryRequirement; }

private:
    // Basic process information
    const int pid;
    const std::string name;
    std::atomic<ProcessState> state;
    std::atomic<int> cpuCoreID;
    std::chrono::system_clock::time_point creationTime;

    size_t memoryRequirement;

    // Command management
    std::vector<std::shared_ptr<ICommand>> commandList;
    std::atomic<int> commandCounter;

    // Round Robin timing
    std::atomic<uint32_t> quantumTime;

    mutable std::mutex processMutex;

    // Generation methods
    int generateInstructionCount() const;
    size_t generateMemoryRequirement() const;
};

#endif
