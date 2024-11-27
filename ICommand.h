#ifndef ICOMMAND_H
#define ICOMMAND_H

#include <string>

class ICommand
{
public:
    enum CommandType
    {
        PRINT
    };

    ICommand(int pid, CommandType commandType);
    virtual ~ICommand() = default;

    virtual void execute() = 0;
    virtual std::string getLogDetails() const = 0;

    CommandType getCommandType() const { return commandType; }

protected:
    int pid; // Process ID
    CommandType commandType;
};

#endif