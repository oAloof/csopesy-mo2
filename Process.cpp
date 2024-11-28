#include "Process.h"
#include <iostream>
#include "PrintCommand.h"
#include <random>
#include <chrono>
#include <thread>
#include <iomanip>
#include "Utils.h"

Process::Process(int pid, const std::string &name)
    : pid(pid),
      name(name),
      state(READY),
      cpuCoreID(-1),
      commandCounter(0),
      quantumTime(0),
      creationTime(std::chrono::system_clock::now()),
      memoryRequirement(generateMemoryRequirement())
{
    // Generate random number of instructions based on config
    int numInstructions = generateInstructionCount();

    // Initialize with dummy instructions
    for (int i = 0; i < numInstructions; ++i)
    {
        addCommand(ICommand::PRINT);
    }
}

void Process::addCommand(ICommand::CommandType commandType)
{
    if (commandType == ICommand::PRINT)
    {
        auto command = std::make_shared<PrintCommand>(pid, name);
        commandList.push_back(command);
    }
}

void Process::executeCurrentCommand(int coreID)
{
    if (commandCounter < commandList.size())
    {
        try
        {
            // Ensure thread-safe execution of the command
            commandList[commandCounter]->execute();
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error executing command in process " << pid
                      << ": " << e.what() << std::endl;
        }
    }
}

void Process::moveToNextLine()
{
    if (commandCounter < commandList.size())
    {
        ++commandCounter;
    }
}

bool Process::isFinished()
{
    return commandCounter >= commandList.size();
}

int Process::generateInstructionCount() const
{
    auto &config = Config::getInstance();
    int min = config.getMinInstructions();
    int max = config.getMaxInstructions();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);

    return dis(gen);
}

size_t Process::generateMemoryRequirement() const
{
    auto &config = Config::getInstance();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(
        config.getMinMemPerProc(),
        config.getMaxMemPerProc());
    return dis(gen);
}

void Process::displayProcessInfo()
{
    std::string processInfo;

    {
        std::lock_guard<std::mutex> lock(processMutex);

        processInfo += name + " (" + formatTimestamp(creationTime) + ") ";

        if (state == FINISHED)
        {
            processInfo += "Finished   " + std::to_string(getLinesOfCode()) + " / " + std::to_string(getLinesOfCode()) + "\n";
        }
        else
        {
            processInfo += "Core: " + std::to_string(cpuCoreID) + "    " +
                           std::to_string(getCommandCounter()) + " / " + std::to_string(getLinesOfCode()) + "\n";
        }
    }

    std::cout << processInfo;
}

// Getters and setters
int Process::getPID() const { return pid; }
std::string Process::getName() const { return name; }

Process::ProcessState Process::getState()
{
    return state.load();
}

void Process::setState(ProcessState newState)
{
    state.store(newState);
}

void Process::setCPUCoreID(int id)
{
    cpuCoreID.store(id);
}

int Process::getCPUCoreID()
{
    return cpuCoreID.load();
}

int Process::getCommandCounter()
{
    return commandCounter.load();
}

int Process::getLinesOfCode()
{
    return static_cast<int>(commandList.size());
}