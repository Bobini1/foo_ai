//
// Created by Bobini on 05/02/2026.
//
#include <SDK/foobar2000.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/pattern_formatter.h>
#include "mcp.h"
#include "preferences.h"

class foobar_sink : public spdlog::sinks::base_sink<std::mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        const auto str = fmt::to_string(formatted);
        switch (spdlog::level::level_enum level = msg.level)
        {
        case spdlog::level::trace:
            FB2K_console_formatter() << "[TRACE] " << str.c_str();
            break;
        case spdlog::level::debug:
            FB2K_console_formatter() << "[DEBUG] " << str.c_str();
            break;
        case spdlog::level::info:
            FB2K_console_formatter() << "[INFO] " << str.c_str();
            break;
        case spdlog::level::warn:
            FB2K_console_formatter() << "[WARN] " << str.c_str();
            break;
        case spdlog::level::err:
            FB2K_console_formatter() << "[ERROR] " << str.c_str();
            break;
        case spdlog::level::critical:
            FB2K_console_formatter() << "[CRITICAL] " << str.c_str();
            break;
        default:
            FB2K_console_formatter() << "[UNKNOWN] " << str.c_str();
            break;
        }
    }

    void flush_() override
    {
    }
};

class myinitquit : public initquit
{
public:
    void on_init() override
    {
#ifdef _DEBUG
        spdlog::set_level(spdlog::level::debug);
#else
        spdlog::set_level(spdlog::level::err);
#endif
        const auto logger = std::make_shared<spdlog::logger>("foobar_ai", std::make_shared<foobar_sink>());

        // Set formatter without newline at the end
        auto f = std::make_unique<spdlog::pattern_formatter>("[%n] [%Y-%m-%d %H:%M:%S.%e] [%l] %v", spdlog::pattern_time_type::local, "");
        logger->set_formatter(std::move(f));

        spdlog::set_default_logger(logger);

        // Start server if enabled
        foo_ai::restart_mcp_server();
    }

    void on_quit() override
    {
        mcp_manager::instance().stop();
    }
};

static initquit_factory_t<myinitquit> g_myinitquit_factory;
