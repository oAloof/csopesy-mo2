#include "CLI.h"
#include <iostream>
#include <sstream>
#include "Config.h"
#include <thread>
#include <chrono>
#include <iomanip>
#include <windows.h>

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

void CLI::clearScreen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}