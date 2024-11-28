#include "Config.h"
#include <fstream>
#include <iostream>

void Config::loadConfig(const std::string &filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        throw ConfigException("Could not open config file: " + filename);
    }

    std::map<std::string, bool> requiredParams = {
        {"num-cpu", false},
        {"scheduler", false},
        {"quantum-cycles", false},
        {"batch-process-freq", false},
        {"min-ins", false},
        {"max-ins", false},
        {"delays-per-exec", false},
        {"max-overall-mem", false},
        {"mem-per-frame", false},
        {"min-mem-per-proc", false},
        {"max-mem-per-proc", false}};

    std::string param;
    while (file >> param)
    {
        if (param == "num-cpu")
        {
            file >> numCPU;
            requiredParams[param] = true;
        }
        else if (param == "scheduler")
        {
            file >> schedulerType;
            requiredParams[param] = true;
        }
        else if (param == "quantum-cycles")
        {
            file >> quantumCycles;
            requiredParams[param] = true;
        }
        else if (param == "batch-process-freq")
        {
            file >> batchProcessFreq;
            requiredParams[param] = true;
        }
        else if (param == "min-ins")
        {
            file >> minInstructions;
            requiredParams[param] = true;
        }
        else if (param == "max-ins")
        {
            file >> maxInstructions;
            requiredParams[param] = true;
        }
        else if (param == "delays-per-exec")
        {
            file >> delaysPerExec;
            requiredParams[param] = true;
        }
        else if (param == "max-overall-mem")
        {
            file >> maxOverallMem;
            requiredParams[param] = true;
        }
        else if (param == "mem-per-frame")
        {
            file >> memPerFrame;
            requiredParams[param] = true;
        }
        else if (param == "min-mem-per-proc")
        {
            file >> minMemPerProc;
            requiredParams[param] = true;
        }
        else if (param == "max-mem-per-proc")
        {
            file >> maxMemPerProc;
            requiredParams[param] = true;
        }
        else
        {
            throw ConfigException("Unknown parameter: " + param);
        }
    }

    // Check if all required parameters were found
    for (const auto &param : requiredParams)
    {
        if (!param.second)
        {
            throw ConfigException("Missing required parameter: " + param.first);
        }
    }

    validateParameters();
    initialized = true;
}

void Config::validateParameters()
{
    if (numCPU < 1 || numCPU > 128)
    {
        throw ConfigException("Invalid number of CPUs (must be between 1 and 128): " + std::to_string(numCPU));
    }

    if (schedulerType != "fcfs" && schedulerType != "rr")
    {
        throw ConfigException("Invalid scheduler type (must be either 'fcfs' or 'rr'): " + schedulerType);
    }

    if (quantumCycles < 1)
    {
        throw ConfigException("Invalid quantum cycles (must be at least 1): " + std::to_string(quantumCycles));
    }

    if (batchProcessFreq < 1)
    {
        throw ConfigException("Invalid batch process frequency (must be at least 1): " + std::to_string(batchProcessFreq));
    }

    if (minInstructions < 1)
    {
        throw ConfigException("Invalid minimum instructions (must be at least 1): " + std::to_string(minInstructions));
    }

    if (maxInstructions < minInstructions)
    {
        throw ConfigException("Invalid maximum instructions (must be greater than or equal to min-ins): " + std::to_string(maxInstructions));
    }

    if (delaysPerExec < 0)
    {
        throw ConfigException("Invalid delays per execution (must be non-negative): " + std::to_string(delaysPerExec));
    }

    if (maxOverallMem < 2 || !isPowerOfTwo(maxOverallMem))
    {
        throw ConfigException("Invalid max-overall-mem (must be power of 2 >= 2): " +
                              std::to_string(maxOverallMem));
    }

    if (memPerFrame < 2 || !isPowerOfTwo(memPerFrame))
    {
        throw ConfigException("Invalid mem-per-frame (must be power of 2 >= 2): " +
                              std::to_string(memPerFrame));
    }

    if (minMemPerProc < 2)
    {
        throw ConfigException("Invalid min-mem-per-proc (must be >= 2): " +
                              std::to_string(minMemPerProc));
    }

    if (maxMemPerProc < minMemPerProc)
    {
        throw ConfigException("Invalid max-mem-per-proc (must be >= min-mem-per-proc): " +
                              std::to_string(maxMemPerProc));
    }
    if (maxOverallMem < memPerFrame)
    {
        throw ConfigException("max-overall-mem must be greater than mem-per-frame");
    }

    if (maxMemPerProc > maxOverallMem)
    {
        throw ConfigException("max-mem-per-proc cannot exceed max-overall-mem");
    }
}