//
// Created by PC on 05/02/2026.
//

#ifndef FOOBAR_AI_MCP_H
#define FOOBAR_AI_MCP_H

#include <memory>
#include <string>
#include <cpp-mcp/mcp_server.h>
#include <SDK/foobar2000.h>

#include "playlist_resource.h"

class current_track_resource;

class foobar_mcp
{
    mcp::server server;
    std::shared_ptr<playlist_resource> m_playlist_resource;
    std::shared_ptr<current_track_resource> m_current_track_resource;

    mcp::json list_library_handler(const mcp::json& params, const std::string& session_id);
    mcp::json list_playlist_handler(const mcp::json& params, const std::string& session_id) const;
    mcp::json current_track_handler(const mcp::json& params, const std::string& session_id) const;

public:
    foobar_mcp(const std::string& host, int port, std::shared_ptr<playlist_resource> playlist_resource, std::shared_ptr<current_track_resource> current_track_resource);
};

class mcp_manager
{
    std::unique_ptr<foobar_mcp> server;
    std::shared_ptr<playlist_resource> playlist_resource;
    std::shared_ptr<current_track_resource> current_track_resource;

public:
    static mcp_manager& instance();
    void start(const std::string& host, int port);
    void stop();
    void restart(const std::string& host, int port);
};

#endif //FOOBAR_AI_MCP_H
