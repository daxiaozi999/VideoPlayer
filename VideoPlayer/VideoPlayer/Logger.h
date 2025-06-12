/*****************************************************************
 * Logger.h - A thread-safe singleton logger implementation
 *
 * Features:
 * - Singleton pattern
 * - Thread-safe operation
 * - Multiple log levels (DEBUG, INFO, WARNING, ERROR)
 * - Output to console or file
 * - Support for variadic arguments
 * - File and line number logging
 * - Conditional logging
 *****************************************************************/

#ifndef _LOGGER_
#define _LOGGER_

#include <string>
#include <mutex>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>

 // Default log file name
const std::string DEFAULT_LOG_FILE = "log.txt";

// Log level enumeration
enum class LogLevel { _DEBUG_, _INFO_, _WARNING_, _ERROR_ };

/**
 * @brief Logger class using singleton pattern, supports different log levels,
 *        outputs to console or file, and ensures thread safety.
 */
class Logger {
public:
    /**
     * @brief Get the singleton instance of Logger.
     * @return Reference to the Logger.
     */
    static Logger& getInstance();

    /**
     * @brief Set the log level, logs below this level will be ignored.
     * @param level Log level.
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief Set log output to file.
     * @param fileName Log file name, defaults to DEFAULT_LOG_FILE.
     */
    void setOutputToFile(const std::string& fileName = DEFAULT_LOG_FILE);

    /**
     * @brief Set log output to console.
     */
    void setOutputToConsole();

    /**
     * @brief Log a message.
     * @param level Log level.
     * @param logMessage Log message.
     */
    void log(LogLevel level, const std::string& logMessage);

    /**
     * @brief Get string representation of log level.
     * @param level Log level.
     * @return String representation of the log level.
     */
    std::string logLevelToString(LogLevel level) const;

    /**
     * @brief Destructor.
     */
    ~Logger();
private:
    static std::unique_ptr<Logger> instance;  ///< Singleton instance
    static std::once_flag initInstanceFlag;   ///< Flag for std::call_once
    std::ofstream file;                       ///< File output stream
    LogLevel logCurrentLevel;                 ///< Current log level
    bool outputToConsole;                     ///< Whether to output to console
    mutable std::mutex mtx;                   ///< Mutex for thread safety

    /**
     * @brief Private constructor.
     */
    Logger();

    /**
     * @brief Write log message to output destination.
     * @param logMessage Formatted log message.
     */
    void writeLog(const std::string& logMessage);

    /**
     * @brief Get current time with millisecond precision.
     * @return Current time as formatted string.
     */
    std::string getCurrentTime() const;

    // Prevent copying and assignment
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

/**
 * @brief Helper function to write a single argument to stream.
 * @tparam T Type of argument.
 * @param oss Output string stream.
 * @param arg Argument to write.
 */
template<typename T>
void logToStream(std::ostringstream& oss, T&& arg) {
    oss << std::forward<T>(arg);
}

/**
 * @brief Helper function to write multiple arguments to stream.
 * @tparam T Type of first argument.
 * @tparam Args Types of remaining arguments.
 * @param oss Output string stream.
 * @param arg First argument to write.
 * @param args Remaining arguments to write.
 */
template<typename T, typename... Args>
void logToStream(std::ostringstream& oss, T&& arg, Args&&... args) {
    oss << std::forward<T>(arg);
    logToStream(oss, std::forward<Args>(args)...);
}

// Enhanced logging macros supporting variadic arguments of different types
#define LOG_DEBUG(...) do { \
    std::ostringstream oss; \
    logToStream(oss, __VA_ARGS__); \
    Logger::getInstance().log(LogLevel::_DEBUG_, oss.str()); \
} while(0)

#define LOG_INFO(...) do { \
    std::ostringstream oss; \
    logToStream(oss, __VA_ARGS__); \
    Logger::getInstance().log(LogLevel::_INFO_, oss.str()); \
} while(0)

#define LOG_WARNING(...) do { \
    std::ostringstream oss; \
    logToStream(oss, __VA_ARGS__); \
    Logger::getInstance().log(LogLevel::_WARNING_, oss.str()); \
} while(0)

#define LOG_ERROR(...) do { \
    std::ostringstream oss; \
    logToStream(oss, __VA_ARGS__); \
    Logger::getInstance().log(LogLevel::_ERROR_, oss.str()); \
} while(0)

// File and line information macros
#define LOG_DEBUG_FL(...) do { \
    std::ostringstream oss; \
    oss << __FILE__ << ":" << __LINE__ << " "; \
    logToStream(oss, __VA_ARGS__); \
    Logger::getInstance().log(LogLevel::_DEBUG_, oss.str()); \
} while(0)

#define LOG_INFO_FL(...) do { \
    std::ostringstream oss; \
    oss << __FILE__ << ":" << __LINE__ << " "; \
    logToStream(oss, __VA_ARGS__); \
    Logger::getInstance().log(LogLevel::_INFO_, oss.str()); \
} while(0)

#define LOG_WARNING_FL(...) do { \
    std::ostringstream oss; \
    oss << __FILE__ << ":" << __LINE__ << " "; \
    logToStream(oss, __VA_ARGS__); \
    Logger::getInstance().log(LogLevel::_WARNING_, oss.str()); \
} while(0)

#define LOG_ERROR_FL(...) do { \
    std::ostringstream oss; \
    oss << __FILE__ << ":" << __LINE__ << " "; \
    logToStream(oss, __VA_ARGS__); \
    Logger::getInstance().log(LogLevel::_ERROR_, oss.str()); \
} while(0)

// Conditional logging macros
#define LOG_DEBUG_IF(condition, ...) do { if (condition) { LOG_DEBUG(__VA_ARGS__); } } while(0)
#define LOG_INFO_IF(condition, ...) do { if (condition) { LOG_INFO(__VA_ARGS__); } } while(0)
#define LOG_WARNING_IF(condition, ...) do { if (condition) { LOG_WARNING(__VA_ARGS__); } } while(0)
#define LOG_ERROR_IF(condition, ...) do { if (condition) { LOG_ERROR(__VA_ARGS__); } } while(0)

// Conditional logging macros with file and line information
#define LOG_DEBUG_FL_IF(condition, ...) do { if (condition) { LOG_DEBUG_FL(__VA_ARGS__); } } while(0)
#define LOG_INFO_FL_IF(condition, ...) do { if (condition) { LOG_INFO_FL(__VA_ARGS__); } } while(0)
#define LOG_WARNING_FL_IF(condition, ...) do { if (condition) { LOG_WARNING_FL(__VA_ARGS__); } } while(0)
#define LOG_ERROR_FL_IF(condition, ...) do { if (condition) { LOG_ERROR_FL(__VA_ARGS__); } } while(0)

#endif // _LOGGER_
