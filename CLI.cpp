#include "CLI.h"
#include <iostream>
#include <sstream>
#include "Config.h"
#include <thread>
#include <chrono>
#include <iomanip>
#include <windows.h>
#include "MemoryManager.h"

void CLI::displayHeader()
{
    std::cout << R"(
   ____ ____   ___  ____  _____ ______   __
  / ___/ ___| / _ \|  _ \| ____/ ___\ \ / /
 | |   \___ \| | | | |_) |  _| \___ \\ V / 
 | |___ ___) | |_| |  __/| |___ ___) || |  
  \____|____/ \___/|_|   |_____|____/ |_|  
                                           
Welcome to CSOPESY OS Emulator!
    )" << std::endl;
}

void CLI::start()
{
    displayHeader();
    std::string input;

    while (true)
    {
        if (currentScreen == "main")
        {
            std::cout << "\nroot\\> ";
        }
        else
        {
            std::cout << "\n"
                      << currentScreen << "\\> ";
        }

        std::getline(std::cin, input);

        if (input == "exit")
        {
            if (currentScreen != "main")
            {
                clearScreen();
                currentScreen = "main";
                displayHeader();
                continue;
            }
            break;
        }

        try
        {
            handleCommand(input);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
}

void CLI::handleCommand(const std::string &command)
{
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    // Only allow initialize and exit before initialization
    if (!initialized && cmd != "initialize" && cmd != "exit")
    {
        std::cout << "Please initialize the system first using the 'initialize' command.\n";
        return;
    }

    if (currentScreen == "main")
    {
        if (cmd == "initialize")
        {
            initialize();
        }
        else if (cmd == "screen")
        {
            std::string flag;
            std::string processName;
            iss >> flag >> processName;

            if (flag == "-s" || flag == "-r" || flag == "-ls")
            {
                handleScreenCommand(flag, processName);
            }
            else
            {
                std::cout << "Invalid screen command. Use -s <name>, -r <name>, or -ls\n";
            }
        }
        else if (cmd == "scheduler-test")
        {
            ProcessManager::getInstance().startBatchProcessing();
            std::cout << "Batch process generation started.\n";
        }
        else if (cmd == "scheduler-stop")
        {
            ProcessManager::getInstance().stopBatchProcessing();
            std::cout << "Batch process generation stopped.\n";
        }
        else if (cmd == "report-util")
        {
            Scheduler::getInstance().getCPUUtilization();
        }
        else if (cmd == "process-smi")
        {
            displayProcessMemoryInfo();
        }
        else if (cmd == "vmstat")
        {
            displayVirtualMemoryStats();
        }
        else if (cmd != "exit")
        {
            std::cout << "Invalid command.\n";
        }
    }
    else
    {
        if (cmd == "process-smi")
        {
            auto process = ProcessManager::getInstance().getProcess(currentScreen);
            if (process)
            {
                process->displayProcessInfo();
            }
            else
            {
                std::cout << "Process not found. Returning to main screen.\n";
                currentScreen = "main";
                clearScreen();
                displayHeader();
            }
        }
        else if (cmd != "exit")
        {
            std::cout << "Invalid command. Available commands: process-smi, exit\n";
        }
    }
}

void CLI::initialize()
{
    try
    {
        Config::getInstance().loadConfig("config.txt");
        MemoryManager::getInstance().initialize();
        initialized = true;
        Scheduler::getInstance().startScheduling();
        std::cout << "System initialized successfully.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Initialization failed: " << e.what() << std::endl;
        initialized = false;
    }
}

void CLI::handleScreenCommand(const std::string &flag, const std::string &processName)
{
    if (flag == "-s")
    {
        if (processName.empty())
        {
            std::cout << "Error: Process name required\n";
            return;
        }

        try
        {
            ProcessManager::getInstance().createProcess(processName);
            clearScreen();
            currentScreen = processName;

            std::cout << "\n================================\n";
            std::cout << "Process Screen: " << processName << "\n";
            std::cout << "================================\n";
            std::cout << "Available commands:\n";
            std::cout << "  process-smi - Show process information\n";
            std::cout << "  exit       - Return to main menu\n";
            std::cout << "================================\n\n";

            auto process = ProcessManager::getInstance().getProcess(processName);
            if (process)
            {
                process->displayProcessInfo();
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error in screen command: " << e.what() << std::endl;
            currentScreen = "main";
        }
    }
    else if (flag == "-r")
    {
        auto process = ProcessManager::getInstance().getProcess(processName);
        if (process && process->getState() != Process::FINISHED)
        {
            clearScreen();
            currentScreen = processName;

            std::cout << "\n================================\n";
            std::cout << "Process Screen: " << processName << "\n";
            std::cout << "================================\n";
            std::cout << "Available commands:\n";
            std::cout << "  process-smi - Show process information\n";
            std::cout << "  exit       - Return to main menu\n";
            std::cout << "================================\n\n";

            process->displayProcessInfo();
        }
        else
        {
            std::cout << "Process " << processName << " not found.\n";
        }
    }
    else if (flag == "-ls")
    {
        ProcessManager::getInstance().listProcesses();
    }
}

void CLI::displayProcessScreen(const std::string &processName)
{
    clearScreen();

    std::cout << "\n================================\n";
    std::cout << "Process Screen: " << processName << "\n";
    std::cout << "================================\n";
    std::cout << "Available commands:\n";
    std::cout << "  process-smi - Show process information\n";
    std::cout << "  exit       - Return to main menu\n";
    std::cout << "================================\n\n";

    auto process = ProcessManager::getInstance().getProcess(processName);
    if (process)
    {
        process->displayProcessInfo();
    }
}

void CLI::displayProcessMemoryInfo()
{
    auto &memManager = MemoryManager::getInstance();
    auto &scheduler = Scheduler::getInstance();

    // Memory Overview
    size_t totalMem = memManager.getTotalMemory() / 1024; // Convert to KB
    size_t usedMem = memManager.getUsedMemory() / 1024;
    size_t freeMem = memManager.getFreeMemory() / 1024;

    std::cout << "\n=== Memory and Process Overview ===\n";
    std::cout << "Memory Usage: " << usedMem << "KB/" << totalMem << "KB ("
              << (usedMem * 100 / totalMem) << "%)\n";

    // CPU Overview
    int totalCores = Config::getInstance().getNumCPU();
    uint64_t activeTicks = scheduler.getActiveTicks();
    uint64_t totalTicks = scheduler.getTotalTicks();
    double cpuUtil = totalTicks > 0 ? (activeTicks * 100.0 / totalTicks) : 0;

    std::cout << "CPU Utilization: " << std::fixed << std::setprecision(1) << cpuUtil << "%\n";
    std::cout << "Memory Type: " << (memManager.isPageBasedAllocation() ? "Paged" : "Flat") << "\n\n";

    // Process list
    ProcessManager::getInstance().listProcesses();
}

void CLI::displayVirtualMemoryStats()
{
    auto &memManager = MemoryManager::getInstance();
    auto &scheduler = Scheduler::getInstance();

    // Memory statistics
    size_t totalMem = memManager.getTotalMemory() / 1024; // Convert to KB
    size_t usedMem = memManager.getUsedMemory() / 1024;
    size_t freeMem = memManager.getFreeMemory() / 1024;

    // CPU statistics
    uint64_t idleTicks = scheduler.getIdleTicks();
    uint64_t activeTicks = scheduler.getActiveTicks();
    uint64_t totalTicks = scheduler.getTotalTicks();

    // Page statistics
    uint64_t pagesIn = memManager.getPagesPagedIn();
    uint64_t pagesOut = memManager.getPagesPagedOut();

    // Display formatted output
    std::cout << "\n=== Virtual Memory Statistics ===\n";
    std::cout << std::left << std::setw(20) << "Memory (KB):"
              << "total=" << totalMem << ", used=" << usedMem << ", free=" << freeMem << "\n";

    std::cout << std::left << std::setw(20) << "CPU Ticks:"
              << "idle=" << idleTicks << ", active=" << activeTicks << ", total=" << totalTicks << "\n";

    std::cout << std::left << std::setw(20) << "Page Operations:"
              << "in=" << pagesIn << ", out=" << pagesOut << "\n\n";
}

void CLI::clearScreen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}