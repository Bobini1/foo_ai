//
// Created by PC on 05/02/2026.
//

#ifndef FOOBAR_AI_MCP_H
#define FOOBAR_AI_MCP_H

#include <memory>
#include <string>
#include <cpp-mcp/mcp_server.h>

class foobar_mcp
{
    std::unique_ptr<mcp::server> server;
    mcp::json hello_handler(const mcp::json& params, const std::string& session_id);

public:
    foobar_mcp(const std::string& host, int port);
};

class mcp_manager
{
    std::unique_ptr<foobar_mcp> m_server;

public:
    static mcp_manager& instance();
    void start(const std::string& host, int port);
    void stop();
    void restart(const std::string& host, int port);
};

#endif //FOOBAR_AI_MCP_H
