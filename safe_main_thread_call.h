#pragma once

#include <SDK/foobar2000.h>
#include <cpp-mcp/mcp_server.h>
#include <future>
#include <format>

// Wrapper function to safely execute foobar2000 operations on the main thread
// Converts all exceptions to mcp_exceptions
template<typename Func>
auto safe_main_thread_call(Func&& func) -> decltype(func())
{
    using ReturnType = decltype(func());
    std::promise<ReturnType> promise;
    auto future = promise.get_future();

    fb2k::inMainThreadSynchronous2([&promise, func = std::forward<Func>(func)]() mutable
    {
        try
        {
            if constexpr (std::is_void_v<ReturnType>)
            {
                func();
                promise.set_value();
            }
            else
            {
                promise.set_value(func());
            }
        }
        catch (const mcp::mcp_exception& e)
        {
            // Already an mcp_exception, just forward it
            promise.set_exception(std::current_exception());
        }
        catch (const std::exception& e)
        {
            // Convert standard exceptions to mcp_exceptions
            promise.set_exception(std::make_exception_ptr(
                mcp::mcp_exception(mcp::error_code::internal_error,
                    std::format("foobar2000 error: {}", e.what()))));
        }
        catch (...)
        {
            // Convert unknown exceptions to mcp_exceptions
            promise.set_exception(std::make_exception_ptr(
                mcp::mcp_exception(mcp::error_code::internal_error,
                    "Unknown error in foobar2000 operation")));
        }
    });

    return future.get();
}

