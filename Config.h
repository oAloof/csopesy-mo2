#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <stdexcept>
#include <cstdint>
#include <map>

class Config
{
public:
    // This allows only one instance of Config to exist
    static Config &getInstance()
    {
        static Config instance;
        return instance;
    }

    void loadConfig(const std::string &filename);
    bool isInitialized() const { return initialized; }

    // Getters
    int getNumCPU() const { return numCPU; }
    std::string getSchedulerType() const { return schedulerType; }
    uint32_t getQuantumCycles() const { return quantumCycles; }
    uint32_t getBatchProcessFreq() const { return batchProcessFreq; }
    uint32_t getMinInstructions() const { return minInstructions; }
    uint32_t getMaxInstructions() const { return maxInstructions; }
    uint32_t getDelaysPerExec() const { return delaysPerExec; }

    // Exception class for Config
    class ConfigException : public std::runtime_error
    {
    public:
        ConfigException(const std::string &msg) : std::runtime_error(msg) {}
    };

private:
    Config() : initialized(false) {}

    int numCPU;                // Range: [1, 128]
    std::string schedulerType; // fcfs or rr
    uint32_t quantumCycles;    // Range: [1, 2^32]
    uint32_t batchProcessFreq; // Range: [1, 2^32]
    uint32_t minInstructions;  // Range: [1, 2^32]
    uint32_t maxInstructions;  // Range: [1, 2^32]
    uint32_t delaysPerExec;    // Range: [0, 2^32]
    bool initialized;

    void validateParameters();
};

#endif