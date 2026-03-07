#pragma once

#include <cstdio>
#include <cstdlib>
#include <format>
#include <string_view>

namespace Neuron::Server
{

/// Console-friendly logging for the Server (headless console application).
/// Unlike DebugTrace (which targets OutputDebugString for GUI apps),
/// these functions write to stdout/stderr so operators can see output
/// in a terminal, Docker logs, or systemd journal.

template <class... Types>
void LogInfo(std::string_view fmt, Types&&... args)
{
    auto msg = std::vformat(fmt, std::make_format_args(args...));
    std::fputs(msg.c_str(), stdout);
    std::fflush(stdout);
}

template <class... Types>
void LogWarn(std::string_view fmt, Types&&... args)
{
    auto msg = std::vformat(fmt, std::make_format_args(args...));
    std::fprintf(stderr, "[WARN] %s", msg.c_str());
    std::fflush(stderr);
}

template <class... Types>
void LogError(std::string_view fmt, Types&&... args)
{
    auto msg = std::vformat(fmt, std::make_format_args(args...));
    std::fprintf(stderr, "[ERROR] %s", msg.c_str());
    std::fflush(stderr);
}

template <class... Types>
[[noreturn]] void LogFatal(std::string_view fmt, Types&&... args)
{
    auto msg = std::vformat(fmt, std::make_format_args(args...));
    std::fprintf(stderr, "[FATAL] %s\n", msg.c_str());
    std::fflush(stderr);
    std::abort();
}

} // namespace Neuron::Server
