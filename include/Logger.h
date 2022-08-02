#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/details/backtracer.h>
#include <string>
#include <spdlog/fmt/fmt.h>

using namespace std;
template <typename... Args>
using format_string_t = fmt::format_string<Args...>;
class Logger
{
private:
    shared_ptr<spdlog::logger> logger;

public:
    Logger(string name)
    {
        logger = spdlog::get(name + ".log");
        if (logger == nullptr)
            logger = spdlog::rotating_logger_mt(name, name + ".log", 1024 * 1024 * 5, 1);
    }
    ~Logger(){};
    template <typename... Args>
    void info(format_string_t<Args...> fmt, Args &&...args)
    {
        logger->info(fmt, std::forward<Args>(args)...);
        spdlog::info(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(format_string_t<Args...> fmt, Args &&...args)
    {
        logger->warn(fmt, std::forward<Args>(args)...);
        spdlog::warn(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(format_string_t<Args...> fmt, Args &&...args)
    {
        logger->error(fmt, std::forward<Args>(args)...);
        spdlog::error(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void critical(format_string_t<Args...> fmt, Args &&...args)
    {
        logger->critical(fmt, std::forward<Args>(args)...);
        spdlog::critical(fmt, std::forward<Args>(args)...);
    }
};

extern Logger RuntimeLogger;