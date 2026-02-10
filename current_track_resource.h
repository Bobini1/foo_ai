//
// Created by Bobini on 08/02/2026.
//

#ifndef FOOBAR_AI_CURRENT_TRACK_RESOURCE_H
#define FOOBAR_AI_CURRENT_TRACK_RESOURCE_H
#include <mcp_resource.h>
#include <SDK/foobar2000.h>


class current_track_resource : public mcp::resource, play_callback_impl_base
{
    std::chrono::steady_clock::time_point last_update_time;
    bool playing = false;

public:
    mcp::json get_metadata() const override;
    mcp::json read() const override;

    current_track_resource();

private:
    void on_playback_stop(play_control::t_stop_reason p_reason) override;
    void on_playback_new_track(metadb_handle_ptr p_track) override;
    void on_playback_pause(bool p_state) override;
    void on_playback_starting(play_control::t_track_command p_command, bool p_paused) override;
    void notify_change() const;
};


#endif //FOOBAR_AI_CURRENT_TRACK_RESOURCE_H