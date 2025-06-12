/*****************************************************************
 * Logger.cpp - Implementation of the Logger class
 *****************************************************************/

#include "Logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

 // Static member initialization
std::unique_ptr<Logger> Logger::instance = nullptr;
std::once_flag Logger::initInstanceFlag;

// Get singleton instance
Logger& Logger::getInstance() {
    std::call_once(initInstanceFlag, []() {
        instance.reset(new Logger());
        });
    return *instance;
}

// Set log level
void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mtx);
    logCurrentLevel = level;
}

// Set output to file
void Logger::setOutputToFile(const std::string& fileName) {
    std::lock_guard<std::mutex> lock(mtx);
    if (file.is_open()) {
        file.close();
    }
    file.open(fileName, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open log file: " << fileName << std::endl;
    }
    outputToConsole = false;
}

// Set output to console
void Logger::setOutputToConsole() {
    std::lock_guard<std::mutex> lock(mtx);
    if (file.is_open()) {
        file.close();
    }
    outputToConsole = true;
}

// Log level to string conversion
std::string Logger::logLevelToString(LogLevel level) const {
    switch (level) {
    case LogLevel::_DEBUG_:   return "DEBUG";
    case LogLevel::_INFO_:    return "INFO";
    case LogLevel::_WARNING_: return "WARNING";
    case LogLevel::_ERROR_:   return "ERROR";
    default:                  return "UNKNOWN";
    }
}

// Record log
void Logger::log(LogLevel level, const std::string& logMessage) {
    if (level < logCurrentLevel) {
        return;
    }

    std::string timestamp = getCurrentTime();
    std::ostringstream oss;
    oss << "[" << timestamp << "] [" << logLevelToString(level) << "] " << logMessage;
    writeLog(oss.str());
}

// Private constructor
Logger::Logger() : logCurrentLevel(LogLevel::_DEBUG_), outputToConsole(false) {}

// Destructor
Logger::~Logger() {
    if (file.is_open()) {
        file.close();
    }
}

// Write log
void Logger::writeLog(const std::string& logMessage) {
    std::lock_guard<std::mutex> lock(mtx);
    if (outputToConsole) {
        std::cout << logMessage << std::endl;
    }
    else if (file.is_open()) {
        file << logMessage << std::endl;
        file.flush(); // Ensure log is written immediately
    }
}

// Get current time with milliseconds
std::string Logger::getCurrentTime() const {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);

    // Get milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm local_tm;
    localtime_s(&local_tm, &now_c);

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}