/**
 * @file Logger.hpp
 * @brief 日志系统 - 基于 spdlog 的单例日志管理器
 *
 * 功能:
 * - 多级别日志 (trace, debug, info, warn, error, critical)
 * - 同时输出到控制台和文件
 * - 日志文件按日期+进程ID命名
 * - 启动时清理上次日志
 * - Windows MiniDump 崩溃捕获
 */
#pragma once

#include <string>
#include <memory>
#include <filesystem>

// spdlog headers
#include <spdlog/spdlog.h>

namespace MoldWing {

/**
 * @brief 日志管理器单例类
 */
class Logger {
public:
    /**
     * @brief 获取 Logger 单例实例
     */
    static Logger& instance();

    /**
     * @brief 初始化日志系统
     * @param appName 应用程序名称，用于日志文件命名
     * @param cleanPreviousLogs 是否清理上次运行的日志
     * @return 是否初始化成功
     */
    bool initialize(const std::string& appName = "MoldWing", bool cleanPreviousLogs = true);

    /**
     * @brief 关闭日志系统
     */
    void shutdown();

    /**
     * @brief 获取日志目录路径
     */
    std::filesystem::path getLogDirectory() const { return m_logDir; }

    /**
     * @brief 获取当前日志文件路径
     */
    std::filesystem::path getCurrentLogFile() const { return m_currentLogFile; }

    /**
     * @brief 强制刷新日志到磁盘
     */
    void flush();

    /**
     * @brief 获取 spdlog logger 实例
     */
    std::shared_ptr<spdlog::logger> getLogger() const { return m_logger; }

    /**
     * @brief 创建 MiniDump 文件 (供崩溃处理器调用)
     */
    static void createMiniDump();

    // 禁止复制和移动
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

private:
    Logger() = default;
    ~Logger();

    /**
     * @brief 获取可执行文件所在目录
     */
    std::filesystem::path getExecutableDir() const;

    /**
     * @brief 生成日志文件名 (格式: AppName_YYYYMMDD_HHMMSS_PID.log)
     */
    std::string generateLogFileName(const std::string& appName) const;

    /**
     * @brief 清理上次运行的日志文件
     */
    void cleanPreviousLogs(const std::string& appName);

    /**
     * @brief 安装 Windows 崩溃处理器
     */
    void installCrashHandler();

private:
    std::shared_ptr<spdlog::logger> m_logger;
    std::filesystem::path m_logDir;
    std::filesystem::path m_currentLogFile;
    bool m_initialized = false;
};

} // namespace MoldWing

// =============================================================================
// 便捷日志宏 (使用 MW_ 前缀避免与 DiligentEngine 宏冲突)
// =============================================================================

#define MW_LOG_TRACE(...)    SPDLOG_LOGGER_TRACE(MoldWing::Logger::instance().getLogger(), __VA_ARGS__)
#define MW_LOG_DEBUG(...)    SPDLOG_LOGGER_DEBUG(MoldWing::Logger::instance().getLogger(), __VA_ARGS__)
#define MW_LOG_INFO(...)     SPDLOG_LOGGER_INFO(MoldWing::Logger::instance().getLogger(), __VA_ARGS__)
#define MW_LOG_WARN(...)     SPDLOG_LOGGER_WARN(MoldWing::Logger::instance().getLogger(), __VA_ARGS__)
#define MW_LOG_ERROR(...)    SPDLOG_LOGGER_ERROR(MoldWing::Logger::instance().getLogger(), __VA_ARGS__)
#define MW_LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(MoldWing::Logger::instance().getLogger(), __VA_ARGS__)

// 简化版本 (仅在不与 DiligentEngine 冲突的情况下使用)
// 注意: LOG_ERROR 与 DiligentEngine 冲突，不提供简化版本
#define LOG_TRACE(...)    MW_LOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)    MW_LOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)     MW_LOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)     MW_LOG_WARN(__VA_ARGS__)
// LOG_ERROR 不定义，使用 MW_LOG_ERROR 代替
#define LOG_CRITICAL(...) MW_LOG_CRITICAL(__VA_ARGS__)
