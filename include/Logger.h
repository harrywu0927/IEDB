#ifndef _LOGGER_H
#define _LOGGER_H
#endif
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/details/backtracer.h>
#include <string>
#include <spdlog/fmt/fmt.h>

#define DEBUG

using namespace std;
#pragma once
template <typename... Args>
using format_string_t = fmt::format_string<Args...>;
class Logger
{
private:
    shared_ptr<spdlog::logger> logger;

public:
    Logger() {}
    Logger(string name)
    {
        logger = spdlog::get(name + ".log");
        if (logger == nullptr)
            logger = spdlog::rotating_logger_mt(name, name + ".log", 1024 * 1024 * 5, 1);
    }
    ~Logger(){
        /* flush */
    };
    template <typename... Args>
    void info(format_string_t<Args...> fmt, Args &&...args)
    {
        logger->info(fmt, std::forward<Args>(args)...);
#ifdef DEBUG
        spdlog::info(fmt, std::forward<Args>(args)...);
#endif
    }

    template <typename... Args>
    void warn(format_string_t<Args...> fmt, Args &&...args)
    {
        logger->warn(fmt, std::forward<Args>(args)...);
#ifdef DEBUG
        spdlog::warn(fmt, std::forward<Args>(args)...);
#endif
    }

    template <typename... Args>
    void error(format_string_t<Args...> fmt, Args &&...args)
    {
        logger->error(fmt, std::forward<Args>(args)...);
#ifdef DEBUG
        spdlog::error(fmt, std::forward<Args>(args)...);
#endif
    }

    template <typename... Args>
    void critical(format_string_t<Args...> fmt, Args &&...args)
    {
        logger->critical(fmt, std::forward<Args>(args)...);
#ifdef DEBUG
        spdlog::critical(fmt, std::forward<Args>(args)...);
#endif
    }

    template <typename... Args>
    void debug(format_string_t<Args...> fmt, Args &&...args)
    {
        logger->debug(fmt, std::forward<Args>(args)...);
#ifdef DEBUG
        spdlog::debug(fmt, std::forward<Args>(args)...);
#endif
    }
};

extern Logger RuntimeLogger;