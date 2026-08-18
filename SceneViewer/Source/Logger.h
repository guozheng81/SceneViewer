#pragma once

#include <string>
#include <cstdarg>
#include <mutex>
#include <fstream>

class CLogger
{
private:
    std::ofstream LogFile;
    std::mutex LoggerMutex;

    std::string GetTimestamp();

public:
    enum class Level { Info, Warn, Error };

	CLogger() = default;
    ~CLogger();

    static CLogger& GetInstance() {
        static CLogger instance;
        return instance;
    }

    // Delete copy constructor and assignment operator to ensure singleton pattern
    CLogger(const CLogger&) = delete;
    CLogger& operator=(const CLogger&) = delete;

    void Init(const std::string& FilePath = "");

    // Thread-safe formatted log.
    void Log(Level LevelArg, const char* Fmt, ...);
};

#define LOG_INFO(...) CLogger::GetInstance().Log(CLogger::Level::Info, __VA_ARGS__)
#define LOG_WARN(...) CLogger::GetInstance().Log(CLogger::Level::Warn, __VA_ARGS__)
#define LOG_ERROR(...) CLogger::GetInstance().Log(CLogger::Level::Error, __VA_ARGS__)