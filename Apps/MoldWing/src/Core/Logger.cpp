/**
 * @file Logger.cpp
 * @brief 日志系统实现
 */

#include "Core/Logger.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")
#endif

namespace MoldWing {

// 静态变量用于崩溃处理
static std::filesystem::path s_dumpDir;
static std::string s_appName;

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    shutdown();
}

bool Logger::initialize(const std::string& appName, bool cleanPreviousLogs) {
    if (m_initialized) {
        return true;
    }

    try {
        s_appName = appName;

        // 获取 exe 所在目录
        auto exeDir = getExecutableDir();
        m_logDir = exeDir / "logs";
        s_dumpDir = m_logDir;

        // 创建日志目录
        if (!std::filesystem::exists(m_logDir)) {
            std::filesystem::create_directories(m_logDir);
        }

        // 清理上次日志
        if (cleanPreviousLogs) {
            this->cleanPreviousLogs(appName);
        }

        // 生成日志文件名
        auto logFileName = generateLogFileName(appName);
        m_currentLogFile = m_logDir / logFileName;

        // 创建 sinks
        std::vector<spdlog::sink_ptr> sinks;

        // 控制台 sink (带颜色)
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(spdlog::level::trace);
        consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
        sinks.push_back(consoleSink);

        // 文件 sink (rotating, 最大 10MB, 保留 3 个备份)
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            m_currentLogFile.string(),
            10 * 1024 * 1024,  // 10 MB
            3                   // 3 backups
        );
        fileSink->set_level(spdlog::level::trace);
        fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] [%t] %v");
        sinks.push_back(fileSink);

        // 创建 logger
        m_logger = std::make_shared<spdlog::logger>(appName, sinks.begin(), sinks.end());
        m_logger->set_level(spdlog::level::trace);
        m_logger->flush_on(spdlog::level::warn);

        // 注册为默认 logger
        spdlog::set_default_logger(m_logger);

        // 安装崩溃处理器
        installCrashHandler();

        m_initialized = true;

        // 记录启动信息
        SPDLOG_INFO("=== {} 启动 ===", appName);
        SPDLOG_INFO("日志目录: {}", m_logDir.string());
        SPDLOG_INFO("日志文件: {}", m_currentLogFile.filename().string());

        return true;
    }
    catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
        return false;
    }
}

void Logger::shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_logger) {
        SPDLOG_INFO("=== {} 关闭 ===", s_appName);
        m_logger->flush();
    }

    spdlog::shutdown();
    m_logger.reset();
    m_initialized = false;
}

void Logger::flush() {
    if (m_logger) {
        m_logger->flush();
    }
}

std::filesystem::path Logger::getExecutableDir() const {
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
#else
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    return std::filesystem::path(std::string(path, (count > 0) ? count : 0)).parent_path();
#endif
}

std::string Logger::generateLogFileName(const std::string& appName) const {
    // 格式: AppName_YYYYMMDD_HHMMSS_PID.log
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream oss;
    oss << appName << "_"
        << std::put_time(&tm, "%Y%m%d_%H%M%S") << "_"
#ifdef _WIN32
        << GetCurrentProcessId()
#else
        << getpid()
#endif
        << ".log";

    return oss.str();
}

void Logger::cleanPreviousLogs(const std::string& appName) {
    if (!std::filesystem::exists(m_logDir)) {
        return;
    }

    try {
        // 删除同名应用的旧日志文件 (保留最近 7 天的)
        auto now = std::chrono::system_clock::now();
        auto sevenDaysAgo = now - std::chrono::hours(24 * 7);

        for (const auto& entry : std::filesystem::directory_iterator(m_logDir)) {
            if (!entry.is_regular_file()) continue;

            auto filename = entry.path().filename().string();

            // 检查是否是该应用的日志文件
            if (filename.find(appName + "_") != 0) continue;
            if (entry.path().extension() != ".log" && entry.path().extension() != ".dmp") continue;

            // 检查文件时间
            auto fileTime = std::filesystem::last_write_time(entry);
            auto fileTimePoint = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
            );

            if (fileTimePoint < sevenDaysAgo) {
                std::filesystem::remove(entry.path());
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to clean previous logs: " << e.what() << std::endl;
    }
}

#ifdef _WIN32
// Windows 崩溃处理
static LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    // 刷新日志
    if (auto logger = Logger::instance().getLogger()) {
        SPDLOG_CRITICAL("!!! 程序崩溃 - 未处理的异常 !!!");
        SPDLOG_CRITICAL("异常代码: 0x{:08X}", exceptionInfo->ExceptionRecord->ExceptionCode);
        SPDLOG_CRITICAL("异常地址: 0x{:016X}", reinterpret_cast<uintptr_t>(exceptionInfo->ExceptionRecord->ExceptionAddress));
        logger->flush();
    }

    // 创建 MiniDump
    Logger::createMiniDump();

    return EXCEPTION_CONTINUE_SEARCH;
}

void Logger::createMiniDump() {
    if (s_dumpDir.empty()) {
        return;
    }

    // 生成 dump 文件名
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &time);

    std::ostringstream oss;
    oss << s_appName << "_crash_"
        << std::put_time(&tm, "%Y%m%d_%H%M%S") << "_"
        << GetCurrentProcessId()
        << ".dmp";

    auto dumpPath = s_dumpDir / oss.str();

    // 创建 MiniDump 文件
    HANDLE hFile = CreateFileW(
        dumpPath.wstring().c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION exInfo;
        exInfo.ThreadId = GetCurrentThreadId();
        exInfo.ExceptionPointers = nullptr;  // Will be filled by the exception handler
        exInfo.ClientPointers = FALSE;

        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            static_cast<MINIDUMP_TYPE>(
                MiniDumpWithDataSegs |
                MiniDumpWithHandleData |
                MiniDumpWithIndirectlyReferencedMemory |
                MiniDumpWithProcessThreadData |
                MiniDumpWithThreadInfo
            ),
            nullptr,
            nullptr,
            nullptr
        );

        CloseHandle(hFile);

        if (auto logger = Logger::instance().getLogger()) {
            SPDLOG_CRITICAL("MiniDump 已保存: {}", dumpPath.string());
            logger->flush();
        }
    }
}
#endif

void Logger::installCrashHandler() {
#ifdef _WIN32
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);
    SPDLOG_DEBUG("Windows 崩溃处理器已安装");
#endif
}

} // namespace MoldWing
