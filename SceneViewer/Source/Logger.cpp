#include "Logger.h"
#include <Windows.h>
#include <memory>
#include <iomanip>
#include <sstream>

std::string LevelToString(CLogger::Level L)
{
    switch (L)
    {
    case CLogger::Level::Info:  return "INFO";
    case CLogger::Level::Warn:  return "WARN";
    case CLogger::Level::Error: return "ERROR";
    default:                    return "UNK";
    }
}

void CLogger::Init(const std::string& FilePath)
{
    if (!FilePath.empty())
    {
        LogFile.open(FilePath, std::ofstream::out | std::ofstream::trunc);
    }
}

CLogger::~CLogger()
{
    if (LogFile.is_open())
    {
        LogFile.close();
    }
}

std::string CLogger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::tm timeInfo;
    localtime_s(&timeInfo, &time);

    std::stringstream ss;
    ss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void CLogger::Log(Level LevelArg, const char* Fmt, ...)
{
    char MessageBuf[4096];
    va_list Args;
    va_start(Args, Fmt);
    vsnprintf_s(MessageBuf, sizeof(MessageBuf), _TRUNCATE, Fmt, Args);
    va_end(Args);

    std::ostringstream Ss;
    Ss << "[" + GetTimestamp() + "]" << " [" << LevelToString(LevelArg) << "] " << MessageBuf << "\n";
    std::string OutStr = Ss.str();

    std::lock_guard<std::mutex> Lk(LoggerMutex);
    OutputDebugStringA(OutStr.c_str());
    if (LogFile && LogFile.is_open())
    {
        LogFile << OutStr;
        LogFile.flush();
    }
}