//
// Created by PC on 06/02/2026.
//

#include "playlist_resource.h"
#include "safe_main_thread_call.h"

#include <future>

void playlist_resource::on_playlist_activate(t_size p_old, t_size p_new)
{
    notify_changed();
}

void playlist_resource::on_playlist_created(t_size p_index, const char* p_name, t_size p_name_len)
{
    auto guid = playlist_manager_v5::get()->playlist_get_guid(p_index);
    auto id = std::string{pfc::print_guid(guid)};
    playlist_update_times[id] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::on_playlists_removed(const bit_array& p_mask, t_size p_old_count, t_size p_new_count)
{
    auto manager = playlist_manager_v5::get();
    for (t_size i = 0; i < p_old_count; ++i)
    {
        if (p_mask[i])
        {
            auto guid = playlist_manager_v5::get()->playlist_get_guid(i);
            auto id = std::string{pfc::print_guid(guid)};
            playlist_update_times.erase(id);
        }
    }
    notify_changed();
}

void playlist_resource::on_playlist_renamed(t_size p_index, const char* p_new_name, t_size p_new_name_len)
{
    auto guid = playlist_manager_v5::get()->playlist_get_guid(p_index);
    auto id = std::string{pfc::print_guid(guid)};
    playlist_update_times[id] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::on_items_reordered(t_size p_playlist, const t_size* p_order, t_size p_count)
{
    auto guid = playlist_manager_v5::get()->playlist_get_guid(p_playlist);
    auto id = std::string{pfc::print_guid(guid)};
    playlist_update_times[id] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::on_items_removing(t_size p_playlist, const bit_array& p_mask, t_size p_old_count,
                                          t_size p_new_count)
{
    auto guid = playlist_manager_v5::get()->playlist_get_guid(p_playlist);
    auto id = std::string{pfc::print_guid(guid)};
    playlist_update_times[id] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::on_items_removed(t_size p_playlist, const bit_array& p_mask, t_size p_old_count,
                                         t_size p_new_count)
{
    auto guid = playlist_manager_v5::get()->playlist_get_guid(p_playlist);
    auto id = std::string{pfc::print_guid(guid)};
    playlist_update_times[id] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::on_items_modified(t_size p_playlist, const bit_array& p_mask)
{
    auto guid = playlist_manager_v5::get()->playlist_get_guid(p_playlist);
    auto id = std::string{pfc::print_guid(guid)};
    playlist_update_times[id] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::on_items_replaced(t_size p_playlist, const bit_array& p_mask,
                                          const pfc::list_base_const_t<t_on_items_replaced_entry>& p_data)
{
    auto guid = playlist_manager_v5::get()->playlist_get_guid(p_playlist);
    auto id = std::string{pfc::print_guid(guid)};
    playlist_update_times[id] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::notify_changed() const
{
    mcp::resource_manager::instance().notify_resource_changed(get_uri());
}

playlist_resource::playlist_resource() : resource("playlists://."), playlist_callback_impl_base(
                                             flag_on_playlist_activate | flag_on_playlist_renamed |
                                             flag_on_playlists_removed | flag_on_playlist_created |
                                             flag_on_items_reordered |
                                             flag_on_items_removing | flag_on_items_removed |
                                             flag_on_items_modified | flag_on_items_replaced)
{
    fb2k::inMainThreadSynchronous2([this]()
    {
        auto manager = playlist_manager_v5::get();
        const auto count = manager->get_playlist_count();
    });
}

mcp::json playlist_resource::get_metadata() const
{
    return {
        {"uri", get_uri()},
        {"name", "Playlists"},
        {
            "description",
            "Tracks the state of playlists, their names, the number of tracks, the \"last modified timestamp\", and the active playlist. "
            "Subscribe to this resource to get notified of changes to playlists, and read it to get the current state of playlists. "
            "Pay attention to playlist state changes to avoid overwriting wrong playlists. "
            "To get the tracks in a playlist, call the list_playlist tool with the appropriate playlist ID."
        }
    };
}

mcp::json playlist_resource::read() const
{
    auto playlists = safe_main_thread_call([this]()
    {
        auto manager = playlist_manager_v5::get();
        auto now = std::chrono::steady_clock::now();
        auto count = manager->get_playlist_count();
        auto active = manager->get_active_playlist();
        auto playing = manager->get_playing_playlist();
        auto playlists = mcp::json::array();
        for (t_size i = 0; i < count; ++i)
        {
            auto id = manager->playlist_get_guid(i);
            auto guid = std::string{pfc::print_guid(id)};
            auto updateTimeIt = playlist_update_times.find(guid);
            auto updateTime = updateTimeIt != playlist_update_times.end()
                                  ? updateTimeIt->second
                                  : std::chrono::steady_clock::time_point{};
            auto lastModified = std::chrono::duration_cast<std::chrono::seconds>(
                now - updateTime);
            auto name = pfc::string8{};
            manager->playlist_get_name(i, name);
            auto playlist_info = mcp::json{
                {"guid", guid},
                {"name", name.c_str()},
                {"track_count", manager->playlist_get_item_count(i)},
                {
                    "last_modified",
                    updateTime == std::chrono::steady_clock::time_point{}
                        ? "Before startup"
                        : std::format("{} seconds ago", lastModified.count())
                },
                {"active", i == active},
                {"playing", i == playing}
            };
            playlists.push_back(std::move(playlist_info));
        }
        return playlists;
    });

    return {
        {"uri", get_uri()},
        {"text", playlists.dump()}
    };
}