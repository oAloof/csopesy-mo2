#include "ICommand.h"

ICommand::ICommand(int pid, CommandType commandType)
    : pid(pid), commandType(commandType)
{
}
