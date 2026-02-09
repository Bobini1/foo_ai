//
// Created by PC on 05/02/2026.
//

#ifndef FOOBAR_AI_MCP_H
#define FOOBAR_AI_MCP_H

#include <memory>
#include <string>
#include <cpp-mcp/mcp_server.h>
#include <SDK/foobar2000.h>

#include "current_track_resource.h"
#include "playlist_resource.h"

class current_track_resource;

class foobar_mcp
{
    mcp::server server;
    std::shared_ptr<playlist_resource> playlist_resource_ = std::make_shared<playlist_resource>();
    std::shared_ptr<current_track_resource> current_track_resource_ = std::make_shared<current_track_resource>();

    mcp::json list_library_handler(const mcp::json& params, const std::string& session_id);
    mcp::json list_playlists_handler(const mcp::json& params, const std::string& session_id) const;
    mcp::json list_playlist_handler(const mcp::json& params, const std::string& session_id) const;
    mcp::json list_current_track_handler(const mcp::json& params, const std::string& session_id) const;
    mcp::json add_tracks_handler(const mcp::json& params, const std::string& session_id);
    mcp::json remove_tracks_handler(const mcp::json& params, const std::string& session_id);
    mcp::json remove_all_tracks_handler(const mcp::json& params, const std::string& session_id);
    mcp::json move_tracks_handler(const mcp::json& params, const std::string& session_id);
    mcp::json set_active_playlist_handler(const mcp::json& params, const std::string& session_id) const;
    mcp::json set_playing_playlist_handler(const mcp::json& params, const std::string& session_id) const;
    mcp::json set_playback_state_handler(const mcp::json& params, const std::string& session_id);
    mcp::json play_at_index_handler(const mcp::json& params, const std::string& session_id);
    mcp::json set_focus_handler(const mcp::json& params, const std::string& session_id);
    mcp::json create_playlist_handler(const mcp::json& params, const std::string& session_id);
    mcp::json rename_playlist_handler(const mcp::json& params, const std::string& session_id);
    mcp::json delete_playlist_handler(const mcp::json& params, const std::string& session_id);

public:
    foobar_mcp(const std::string& host, int port);
};

class mcp_manager
{
    std::unique_ptr<foobar_mcp> server;

public:
    static mcp_manager& instance();
    void start(const std::string& host, int port);
    void stop();
};

#endif //FOOBAR_AI_MCP_H
