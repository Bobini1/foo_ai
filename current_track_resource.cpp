//
// Created by PC on 08/02/2026.
//

#include "current_track_resource.h"

mcp::json current_track_resource::get_metadata() const
{
    return {
        {"uri", get_uri()},
        {"name", "Current track"},
        {
            "description",
            "Tracks the state of playback. "
            "Subscribe to this resource to get notified of changes to active track and pausing/unpausing. "
            "To get the info about the current track, call the list_current_track tool."
        }
    };
}

mcp::json current_track_resource::read() const
{
    return {
        {"uri", get_uri()},
        {
            "text", std::format("Playing: {}, song last changed {} seconds ago", playing ? "Yes" : "No",
                                std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::steady_clock::now() - last_update_time).count())
        }
    };
}

current_track_resource::current_track_resource() : resource("current_track://."),
                                                   play_callback_impl_base(
                                                       flag_on_playback_stop | flag_on_playback_new_track |
                                                       flag_on_playback_pause | flag_on_playback_starting)
{
}

void current_track_resource::on_playback_stop(play_control::t_stop_reason p_reason)
{
    playing = false;
    notify_change();
}

void current_track_resource::on_playback_new_track(metadb_handle_ptr p_track)
{
    playing = true;
    notify_change();
}

void current_track_resource::on_playback_pause(bool p_state)
{
    playing = !p_state;
    notify_change();
}

void current_track_resource::on_playback_starting(play_control::t_track_command p_command, bool p_paused)
{
    playing = !p_paused;
    notify_change();
}

void current_track_resource::notify_change() const
{
    mcp::resource_manager::instance().notify_resource_changed(get_uri());
}
