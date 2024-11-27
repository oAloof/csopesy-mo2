#ifndef CLI_H
#define CLI_H

#include <string>
#include <memory>
#include <windows.h>
#include "ProcessManager.h"

class CLI
{
public:
    static CLI &getInstance()
    {
        static CLI instance;
        return instance;
    }

    void start();
    std::string getCurrentScreen() const { return currentScreen; }

private:
    CLI() : initialized(false), currentScreen("main") {}

    bool initialized;
    std::string currentScreen;

    void displayHeader();
    void clearScreen();
    void displayProcessScreen(const std::string &processName);
    void handleCommand(const std::string &command);
    void handleScreenCommand(const std::string &flag, const std::string &processName);
    void initialize();
};

#endif