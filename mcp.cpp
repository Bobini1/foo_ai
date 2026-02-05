#include "mcp.h"

foobar_mcp::foobar_mcp(const std::string& host, int port)
    : server(std::make_unique<mcp::server>(mcp::server::configuration{host, port}))
{
    server->start();
}

mcp::json foobar_mcp::hello_handler(const mcp::json& params, const std::string& /* session_id */)
{
    return mcp::json{{"message", "Hello from foobar2000!"}};
}

mcp_manager& mcp_manager::instance()
{
    static mcp_manager mgr;
    return mgr;
}

void mcp_manager::start(const std::string& host, int port)
{
    m_server = std::make_unique<foobar_mcp>(host, port);
}

void mcp_manager::stop()
{
    m_server.reset();
}

void mcp_manager::restart(const std::string& host, int port)
{
    stop();
    start(host, port);
}
