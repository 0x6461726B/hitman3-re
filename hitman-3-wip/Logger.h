#pragma once
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>

enum class LogLevel {
    Trace,   // Detailed debugging information
    Debug,   // Debugging messages
    Info,    // General information
    Warning, // Potentially problematic situations
    Error,   // Recoverable errors
    Critical // Fatal errors requiring immediate attention
};

class Logger {
  public:
    // Configuration structure
    struct Config {
        bool console_output = true;
        bool file_output = true;
        std::string filename = "application.log";
        LogLevel min_level = LogLevel::Trace;
    };

    // Singleton access
    static Logger &instance() {
        static Logger instance;
        return instance;
    }

    // Configuration methods
    void configure(const Config &config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = config;
    }

    // Main logging method
    template <typename... Args> void log(LogLevel level, const char *format, Args... args) {
        if (level < m_config.min_level)
            return;

        std::lock_guard<std::mutex> lock(m_mutex);

        const auto message = format_message(level, format, args...);

        if (m_config.console_output) {
            write_console(level, message);
        }

        if (m_config.file_output) {
            write_file(message);
        }
    }

    // Shorthand methods
    template <typename... Args> void trace(const char *format, Args... args) { log(LogLevel::Trace, format, args...); }

    template <typename... Args> void debug(const char *format, Args... args) { log(LogLevel::Debug, format, args...); }

    template <typename... Args> void info(const char *format, Args... args) { log(LogLevel::Info, format, args...); }

    template <typename... Args> void warn(const char *format, Args... args) { log(LogLevel::Warning, format, args...); }

    template <typename... Args> void error(const char *format, Args... args) { log(LogLevel::Error, format, args...); }

    template <typename... Args> void critical(const char *format, Args... args) {
        log(LogLevel::Critical, format, args...);
    }

  private:
    ~Logger() {
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    template <typename... Args> std::string format_message(LogLevel level, const char *format, Args... args) {
        auto now = std::chrono::system_clock::now();
        const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        struct tm newtime;

        // Use localtime_s safely
        if (localtime_s(&newtime, &now_time) != 0) {
            // Handle error if needed
            memset(&newtime, 0, sizeof(newtime));
        }

        std::stringstream ss;
        ss << std::put_time(&newtime, "[%Y-%m-%d %H:%M:%S] ") << level_to_string(level) << ": "
           << format_string(format, args...) << "\n";

        return ss.str();
    }
    // Safe string formatting
    template <typename... Args> std::string format_string(const char *format, Args... args) {
        try {
            int size_s = std::snprintf(nullptr, 0, format, args...) + 1;
            if (size_s <= 0)
                return "Formatting error";

            auto size = static_cast<size_t>(size_s);
            auto buf = std::make_unique<char[]>(size);
            std::snprintf(buf.get(), size, format, args...);
            return std::string(buf.get(), buf.get() + size - 1);
        } catch (...) {
            return "Formatting error";
        }
    }

    // Console output with colors
    void write_console(LogLevel level, const std::string &message) {
        std::cout << level_to_color(level) << message << "\x1b[0m";
    }

    // File output
    void write_file(const std::string &message) {
        if (!m_file.is_open()) {
            m_file.open(m_config.filename, std::ios::out | std::ios::app);
        }

        if (m_file.good()) {
            m_file << message;
            m_file.flush();
        }
    }

    // Helper conversions
    const char *level_to_color(LogLevel level) const {
        switch (level) {
        case LogLevel::Critical:
            return "\x1b[95m"; // Bright magenta
        case LogLevel::Error:
            return "\x1b[91m"; // Bright red
        case LogLevel::Warning:
            return "\x1b[93m"; // Bright yellow
        case LogLevel::Info:
            return "\x1b[32m"; // Green
        case LogLevel::Debug:
            return "\x1b[34m"; // Blue
        case LogLevel::Trace:
            return "\x1b[90m"; // Dark gray
        default:
            return "\x1b[0m"; // Reset
        }
    }

    const char *level_to_string(LogLevel level) const {
        switch (level) {
        case LogLevel::Critical:
            return "CRITICAL";
        case LogLevel::Error:
            return "ERROR   ";
        case LogLevel::Warning:
            return "WARNING ";
        case LogLevel::Info:
            return "INFO    ";
        case LogLevel::Debug:
            return "DEBUG   ";
        case LogLevel::Trace:
            return "TRACE   ";
        default:
            return "UNKNOWN ";
        }
    }

    // Member variables
    std::mutex m_mutex;
    Config m_config;
    std::ofstream m_file;
};