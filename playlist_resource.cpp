//
// Created by PC on 06/02/2026.
//

#include "playlist_resource.h"

#include <future>

void playlist_resource::on_playlist_activate(t_size p_old, t_size p_new)
{
    notify_changed();
}

void playlist_resource::on_playlist_created(t_size p_index, const char* p_name, t_size p_name_len)
{
    playlist_update_times[p_index] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::on_playlists_removed(const bit_array& p_mask, t_size p_old_count, t_size p_new_count)
{
    for (t_size i = 0, j = 0; i < p_old_count; ++i)
    {
        if (p_mask[i])
        {
            playlist_update_times.erase(playlist_update_times.begin() + j);
            playlist_ids.erase(playlist_ids.begin() + j);
        }
        else
        {
            ++j;
        }
    }
    notify_changed();
}

void playlist_resource::on_playlist_renamed(t_size p_index, const char* p_new_name, t_size p_new_name_len)
{
    playlist_update_times[p_index] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::on_playlists_reorder(const t_size* p_order, t_size p_count)
{
    auto new_update_times = decltype(playlist_update_times){};
    auto new_ids = decltype(playlist_ids){};
    for (t_size i = 0; i < p_count; ++i)
    {
        auto old_index = p_order[i];
        new_update_times.push_back(playlist_update_times[old_index]);
        new_ids.push_back(playlist_ids[old_index]);
    }
    playlist_update_times = std::move(new_update_times);
    playlist_ids = std::move(new_ids);
    notify_changed();
}

void playlist_resource::on_items_reordered(t_size p_playlist, const t_size* p_order, t_size p_count)
{
    playlist_update_times[p_playlist] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::on_items_removing(t_size p_playlist, const bit_array& p_mask, t_size p_old_count,
                                          t_size p_new_count)
{
    playlist_update_times[p_playlist] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::on_items_removed(t_size p_playlist, const bit_array& p_mask, t_size p_old_count,
                                         t_size p_new_count)
{
    playlist_update_times[p_playlist] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::on_items_modified(t_size p_playlist, const bit_array& p_mask)
{
    playlist_update_times[p_playlist] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::on_items_replaced(t_size p_playlist, const bit_array& p_mask,
                                          const pfc::list_base_const_t<t_on_items_replaced_entry>& p_data)
{
    playlist_update_times[p_playlist] = std::chrono::steady_clock::now();
    notify_changed();
}

void playlist_resource::notify_changed() const
{
    mcp::resource_manager::instance().notify_resource_changed(get_uri());
}

playlist_resource::playlist_resource() : resource("playlists://."), playlist_callback_impl_base(
                                             flag_on_playlist_activate | flag_on_playlist_renamed |
                                             flag_on_playlists_removed | flag_on_playlist_created |
                                             flag_on_playlists_reorder | flag_on_items_reordered |
                                             flag_on_items_removing | flag_on_items_removed |
                                             flag_on_items_modified | flag_on_items_replaced)
{
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
    auto promise = std::promise<std::vector<mcp::json>>{};
    fb2k::inMainThread([this, &promise]()
    {
        auto now = std::chrono::steady_clock::now();
        auto count = playlist_manager::get()->get_playlist_count();
        auto active = playlist_manager::get()->get_active_playlist();
        auto playing = playlist_manager::get()->get_playing_playlist();
        auto playlists = std::vector<mcp::json>{};
        for (t_size i = 0; i < count; ++i)
        {
            auto lastModified = std::chrono::duration_cast<std::chrono::seconds>(
                now - playlist_update_times[i]);
            auto id = playlist_ids[i];
            auto name = pfc::string8{};
            auto playlist_info = mcp::json{
                {"id", id},
                {"name", name.c_str()},
                {"track_count", playlist_manager::get()->playlist_get_item_count(i)},
                {
                    "last_modified",
                    playlist_update_times[i] == std::chrono::steady_clock::time_point{}
                        ? "Unknown"
                        : std::format("{} seconds ago", lastModified.count())
                },
                {"active", i == active},
                {"playing", i == playing}
            };
            playlists.push_back(std::move(playlist_info));
        }
        promise.set_value(std::move(playlists));
    });

    auto future = promise.get_future();
    auto playlists = future.get();

    return {
        {"uri", get_uri()},
        {"text", playlists}
    };
}

std::optional<t_size> playlist_resource::get_playlist_index(const std::string& playlist_id) const
{
    auto it = std::ranges::find(playlist_ids, playlist_id);
    if (it != playlist_ids.end())
    {
        return std::distance(playlist_ids.begin(), it);
    }
    return std::nullopt;
}
