#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>

inline std::string formatTimestamp(const std::chrono::system_clock::time_point &time)
{
    auto time_c = std::chrono::system_clock::to_time_t(time);
    std::tm *ltm = std::localtime(&time_c);

    std::stringstream ss;
    ss << std::setfill('0')
       << std::setw(2) << ltm->tm_mon + 1 << "/"
       << std::setw(2) << ltm->tm_mday << "/"
       << "2024 "
       << std::setw(2) << ltm->tm_hour << ":"
       << std::setw(2) << ltm->tm_min << ":"
       << std::setw(2) << ltm->tm_sec;
    return ss.str();
}

#endif