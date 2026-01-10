#pragma once
#include <iostream>
#include <sstream>
/*
How to define a macro
#define PI 3.14159
Anywhere PI appears it is replaced with 3.14159.

TO DO
Add timestamp to each log entry
Add file logging to a file like errors.log

*/

// an enum is a user-defined type consisting of a set of named integral constants
// which makes code more readable and maintainable
enum LOG_LEVEL {    // Enum values are basically integers.
    LOG_LEVEL_INIT = 0,        // set initialization level to 0
    LOG_LEVEL_ERROR = 1,        // set error level to 1
    LOG_LEVEL_WARNING = 2,        // set warning level to 2
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_TRACE = 4,
    LOG_LEVEL_DEBUG = 5
};

// ANSI color codes
// ANSI color codes are special terminal escape sequences(also called ANSI escape codes
// that tell a terminal to change text color,
// background color, or text attributes(bold, underline, etc.).
// They start with the escape character(ESC, \x1b or \033) followed by '[' and one or more numeric parameters ending in 'm'.
// Example: ESC[31m sets foreground to red, ESC[0m resets formatting.
#define COLOR_RESET     "\x1b[0m"     // Reset color back to default 
#define COLOR_RED       "\x1b[31m"    // Red color
#define COLOR_YELLOW    "\x1b[33m"    // Yellow color
#define COLOR_GREEN     "\x1b[32m"    // Green color
#define COLOR_CYAN      "\x1b[36m"    // Cyan color
#define COLOR_BLUE      "\x1b[34m"     // Blue color
// Set active log level based on build configuration
#ifdef _DEBUG
#ifndef LOG_LEVEL_ACTIVE
#define LOG_LEVEL_ACTIVE LOG_LEVEL_DEBUG
#endif

// Function-like macro to get color code based on log level
#define LOG_COLOR(level) \
        ((level) == LOG_LEVEL_INIT ?    COLOR_GREEN : \
         (level) == LOG_LEVEL_ERROR ?   COLOR_RED : \
         (level) == LOG_LEVEL_WARNING ? COLOR_YELLOW : \
         (level) == LOG_LEVEL_INFO ?    COLOR_GREEN : \
         (level) == LOG_LEVEL_TRACE ?   COLOR_CYAN : \
         COLOR_BLUE)


#define LOG_CONCAT_IMPL(a, b) a##b // helper macro to concatenate tokens
#define LOG_CONCAT(a, b) LOG_CONCAT_IMPL(a, b) // macro to concatenate tokens with expansion
// Generic logging macro that handles different log levels and message formatting 
#define LOG(level, level_name, ...) do { \
    /* Cache the evaluated `level` once in a temporary whose name includes the source line. */ \
    auto LOG_CONCAT(_log_lv_, __LINE__) = (level); \
    /* Only proceed if the message level is allowed by the active log level. */ \
    if (LOG_CONCAT(_log_lv_, __LINE__) <= LOG_LEVEL_ACTIVE) { \
        /* Create a per-call std::ostringstream with a unique name to collect the message. */ \
        std::ostringstream LOG_CONCAT(_log_stream_, __LINE__); \
        /* Stream the user-provided variadic arguments into the stringstream. */ \
        LOG_CONCAT(_log_stream_, __LINE__) << __VA_ARGS__; \
        /* Print color, prefix, the composed message, then reset color and end the line. */ \
        std::cout << LOG_COLOR(LOG_CONCAT(_log_lv_, __LINE__)) \
                  << "[LOG][" << (level_name) << "] " \
                  << LOG_CONCAT(_log_stream_, __LINE__).str() \
                  << COLOR_RESET << std::endl; \
    } \
} while (0) 

//in Debug: macros log messages
#define LOG_INIT(...)    LOG(LOG_LEVEL_INIT,    "INIT", __VA_ARGS__)
#define LOG_ERROR(...)   LOG(LOG_LEVEL_ERROR,   "ERROR", __VA_ARGS__)
#define LOG_WARNING(...) LOG(LOG_LEVEL_WARNING, "WARN", __VA_ARGS__)
#define LOG_INFO(...)    LOG(LOG_LEVEL_INFO,    "INFO", __VA_ARGS__)
#define LOG_TRACE(...)   LOG(LOG_LEVEL_TRACE,   "TRACE", __VA_ARGS__)
#define LOG_DEBUG(...)   LOG(LOG_LEVEL_DEBUG,   "DEBUG", __VA_ARGS__)

#else
// In release: macros do nothing ie; no logging to the console
#define LOG_INIT(x)
#define LOG_ERROR(...)
#define LOG_WARNING(...)
#define LOG_INFO(...)
#define LOG_TRACE(...)
#define LOG_DEBUG(...)
#endif
