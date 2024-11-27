#include "PrintCommand.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "CLI.h"

// PrintCommand::PrintCommand(int pid, const std::string &toPrint)
//     : ICommand(pid, CommandType::PRINT), toPrint(toPrint)
// {
// }

PrintCommand::PrintCommand(int pid, const std::string &processName)
    : ICommand(pid, CommandType::PRINT), processName(processName)
{
}

void PrintCommand::execute()
{
    // // Create log filename based on process ID
    // std::string filename = "process_" + std::to_string(pid) + "_log.txt";
    // std::ofstream logFile(filename, std::ios::app); // Open in append mode

    // // Get current timestamp
    // auto now = std::chrono::system_clock::now();
    // std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    // std::tm localTime = *std::localtime(&now_c);

    // // Write to log file with timestamp
    // logFile << "("
    //         << std::put_time(&localTime, "%m/%d/%Y %H:%M:%S%p")
    //         << ") PID " << pid << ": " << toPrint << "\n";

}

std::string PrintCommand::getLogDetails() const
{
    return "Hello world from " + processName;
}