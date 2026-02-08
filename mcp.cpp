#include "mcp.h"
#include "safe_main_thread_call.h"
#include <SDK/foobar2000.h>
#include <future>

foobar_mcp::foobar_mcp(const std::string& host, int port, std::shared_ptr<playlist_resource> playlist_resource,
                       std::shared_ptr<current_track_resource> current_track_resource)
    : server(mcp::server::configuration{host, port}), m_playlist_resource(std::move(playlist_resource)),
      m_current_track_resource(std::move(current_track_resource))
{
    // Set server info and capabilities
    server.set_server_info("foo_ai", "1.0.0");
    server.set_capabilities({
        mcp::json::object({
            {"tools", mcp::json::object()},
            {
                "resources", mcp::json::object({
                    {"subscribe", true}
                })
            }
        })
    });

    // Register the list_library tool
    const mcp::tool list_library_tool = mcp::tool_builder("list_library")
                                        .with_description("Get tracks from the user's media library")
                                        .with_number_param("limit", "Max tracks to return (default: 50)", false)
                                        .with_number_param("offset", "Skip first N tracks", false)
                                        .with_string_param(
                                            "query", "foobar2000 search query (e.g. 'artist HAS beatles')",
                                            false)
                                        .with_array_param("fields", "Fields to return: "
                                                          "path, duration_seconds or any tag contained in audio files. "
                                                          "Default: path, artist, title, album",
                                                          "string",
                                                          false)
                                        .build();

    server.register_tool(list_library_tool, std::bind_front(&foobar_mcp::list_library_handler, this));

    const mcp::tool list_playlist_tool = mcp::tool_builder("list_playlist")
                                         .with_description("Get tracks from a playlist")
                                         .with_string_param("playlist_guid", "ID of the playlist to retrieve tracks from",
                                                            true)
                                         .with_number_param("limit", "Max tracks to return (default: 50)", false)
                                         .with_number_param("offset", "Skip first N tracks", false)
                                         .with_string_param(
                                             "query", "foobar2000 search query (e.g. 'artist HAS beatles')",
                                             false)
                                         .with_array_param("fields", "Fields to return: "
                                                           "path, duration_seconds or any tag contained in audio files. "
                                                           "Default: path, artist, title, album",
                                                           "string",
                                                           false)
                                         .build();

    server.register_tool(list_playlist_tool, std::bind_front(&foobar_mcp::list_playlist_handler, this));

    auto current_track_tool = mcp::tool_builder("list_current_track")
                              .with_description("Get the currently playing track")
                              .with_array_param("fields", "Fields to return: "
                                                "path, duration_seconds or any tag contained in audio files. "
                                                "Default: path, artist, title, album",
                                                "string",
                                                false)
                              .build();

    server.register_tool(current_track_tool, std::bind_front(&foobar_mcp::list_current_track_handler, this));

    auto set_active_playlist_tool = mcp::tool_builder("set_active_playlist")
                                    .with_description(
                                        "Set the active playlist. Adding, modifying and removing tracks happens on the active playlist. "
                                        "This does not change the currently playing playlist or track. ")
                                    .with_string_param("playlist_guid", "ID of the playlist to set as active", true)
                                    .build();

    server.register_tool(set_active_playlist_tool, std::bind_front(&foobar_mcp::set_active_playlist_handler, this));

    auto set_playing_playlist_tool = mcp::tool_builder("set_playing_playlist")
                                     .with_description(
                                         "Set the playing playlist. Also sets it as the active playlist. "
                                         "The music player will start picking tracks to play from this playlist. "
                                         "Does not stop the currently playing track or change the playback state.")
                                     .with_string_param("playlist_guid", "ID of the playlist to set as currently playing",
                                                        true)
                                     .build();

    server.register_tool(set_playing_playlist_tool, std::bind_front(&foobar_mcp::set_playing_playlist_handler, this));

    auto set_playback_state_tool = mcp::tool_builder("set_playback_state")
                                   .with_description(
                                       "Set the playback state. Played (true) or paused (false). "
                                       "This does not change the currently playing track or playlist.")
                                   .with_boolean_param("state", "Playback state to set", true)
                                   .build();

    server.register_tool(set_playback_state_tool, std::bind_front(&foobar_mcp::set_playback_state_handler, this));

    auto play_at_index_tool = mcp::tool_builder("play_at_index")
                              .with_description(
                                  "Start playback at a specific track index in the currently active playlist, immediately. "
                                  "This will make this playlist the playing playlist.")
                              .with_number_param("index", "Track index to start playback at", true)
                              .build();
    server.register_tool(play_at_index_tool, std::bind_front(&foobar_mcp::play_at_index_handler, this));

    auto add_tracks_tool = mcp::tool_builder("add_tracks")
                           .with_description("Add filesystem tracks to the active playlist")
                           .with_array_param("paths", "Absolute file paths to add", "string", true)
                           .with_number_param("position", "Index to insert at (default: append)", false)
                           .build();
    server.register_tool(add_tracks_tool, std::bind_front(&foobar_mcp::add_tracks_handler, this));

    auto remove_tracks_tool = mcp::tool_builder("remove_tracks")
                              .with_description("Remove specific entries from the active playlist")
                              .with_array_param("track_indices", "Indices to remove", "number", true)
                              .build();
    server.register_tool(remove_tracks_tool, std::bind_front(&foobar_mcp::remove_tracks_handler, this));

    auto remove_all_tracks_tool = mcp::tool_builder("remove_all_tracks")
                                  .with_description("Clear every track from the active playlist")
                                  .build();
    server.register_tool(remove_all_tracks_tool, std::bind_front(&foobar_mcp::remove_all_tracks_handler, this));

    auto move_tracks_tool = mcp::tool_builder("move_tracks")
                            .with_description("Reorder the active playlist by providing a full permutation")
                            .with_array_param("order", "Permutation describing the new order", "number", true)
                            .build();
    server.register_tool(move_tracks_tool, std::bind_front(&foobar_mcp::move_tracks_handler, this));

    auto set_focus_tool = mcp::tool_builder("set_focus")
                          .with_description("Set the focused entry in the active playlist")
                          .with_number_param("index", "Track index that should receive focus", true)
                          .build();
    server.register_tool(set_focus_tool, std::bind_front(&foobar_mcp::set_focus_handler, this));

    auto create_playlist_tool = mcp::tool_builder("create_playlist")
                                .with_description("Create a new playlist")
                                .with_string_param("name", "Name of the new playlist", true)
                                .build();
    server.register_tool(create_playlist_tool, std::bind_front(&foobar_mcp::create_playlist_handler, this));

    auto rename_playlist_tool = mcp::tool_builder("rename_playlist")
                                .with_description("Rename an existing playlist")
                                .with_string_param("playlist_guid", "ID of the playlist to rename", true)
                                .with_string_param("new_name", "New name for the playlist", true)
                                .build();
    server.register_tool(rename_playlist_tool, std::bind_front(&foobar_mcp::rename_playlist_handler, this));

    auto delete_playlist_tool = mcp::tool_builder("delete_playlist")
                                .with_description("Delete a playlist")
                                .with_string_param("playlist_guid", "ID of the playlist to delete", true)
                                .build();
    server.register_tool(delete_playlist_tool, std::bind_front(&foobar_mcp::delete_playlist_handler, this));

    server.start(false);
}

struct result
{
    mcp::json tracks;
    size_t total;
};

static result handle_tracks(pfc::list_t<metadb_handle_ptr> items, const int limit, const int offset,
                            const std::string& query,
                            const std::vector<std::string>& fields)
{
    mcp::json tracks = mcp::json::array();

    // Apply search filter if query provided
    if (!query.empty())
    {
        try
        {
            auto mgr = search_filter_manager_v2::get();
            search_filter_v2::ptr filter = mgr->create_ex(query.c_str(),
                                                          fb2k::makeCompletionNotify([](unsigned)
                                                          {
                                                          }),
                                                          search_filter_manager_v2::KFlagSuppressNotify);

            auto list = metadb_handle_list{items};
            filter->test_multi_here(list, fb2k::noAbort);
        }
        catch (std::exception& e)
        {
            throw mcp::mcp_exception(mcp::error_code::invalid_params,
                                    std::format("Invalid search query: {}", e.what()));
        }
    }

    const size_t total = items.get_count();
    const size_t start = std::min<size_t>(offset, total);
    const size_t end = std::min<size_t>(start + limit, total);

    for (size_t i = start; i < end; ++i)
    {
        metadb_handle_ptr item = items[i];
        mcp::json track;

        metadb_info_container::ptr info_ptr;
        if (item->get_info_ref(info_ptr))
        {
            const file_info& info = info_ptr->info();

            for (const auto& field : fields)
            {
                if (field == "path")
                {
                    track["path"] = item->get_path();
                }
                else if (field == "duration_seconds")
                {
                    track["duration_seconds"] = info.get_length();
                }
                else if (info.meta_exists(field.c_str()))
                {
                    track[field] = info.meta_get(field.c_str(), 0);
                }
            }
        }

        tracks.push_back(track);
    }

    return {std::move(tracks), total};
}

mcp::json foobar_mcp::list_library_handler(const mcp::json& params, const std::string& session_id)
{
    int limit = 50;
    int offset = 0;
    std::string query;
    std::vector<std::string> fields = {"path", "artist", "title", "album"};

    if (params.contains("limit")) limit = params["limit"].get<int>();
    if (params.contains("offset")) offset = params["offset"].get<int>();
    if (params.contains("query")) query = params["query"].get<std::string>();
    if (params.contains("fields"))
    {
        fields.clear();
        for (const auto& f : params["fields"])
        {
            fields.push_back(f.get<std::string>());
        }
    }

    auto [tracks, total] = safe_main_thread_call([limit, offset, query = std::move(query), fields = std::move(fields)]()
    {
        const auto lib_api = library_manager::get();
        pfc::list_t<metadb_handle_ptr> items;
        lib_api->get_all_items(items);
        return handle_tracks(items, limit, offset, query, fields);
    });
    return {
        {
            {"type", "text"},
            {
                "text",
                std::format("Total tracks: {}, Returned tracks: {}, tracks: {}", total, tracks.size(), tracks.dump())
            }
        }
    };
}

struct playlist_result
{
    result result;
    std::string current_track_index;
    std::string current_track_path;
};

mcp::json foobar_mcp::list_playlist_handler(const mcp::json& params, const std::string& session_id) const
{
    if (!params.contains("playlist_guid"))
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "playlist_guid parameter is required");
    }

    int limit = 50;
    int offset = 0;
    std::string query;
    std::vector<std::string> fields = {"path", "artist", "title", "album"};
    auto playlist_guid = params["playlist_guid"].get<std::string>();
    if (params.contains("limit")) limit = params["limit"].get<int>();
    if (params.contains("offset")) offset = params["offset"].get<int>();
    if (params.contains("query")) query = params["query"].get<std::string>();
    if (params.contains("fields"))
    {
        fields.clear();
        for (const auto& f : params["fields"])
        {
            fields.push_back(f.get<std::string>());
        }
    }

    auto [result, index, path, playing, active, name] = safe_main_thread_call(
        [this, limit, offset, query = std::move(query), fields = std::move(fields), playlist_guid]()
        {
            pfc::list_t<metadb_handle_ptr> items;
            auto index = m_playlist_resource->get_playlist_index(playlist_guid);
            if (!index.has_value())
            {
                throw mcp::mcp_exception(mcp::error_code::invalid_params, "Playlist not found");
            }
            bool active = playlist_manager::get()->get_active_playlist() == index.value();
            bool playing = playlist_manager::get()->get_playing_playlist() == index.value();
            auto name_ptr = pfc::string8{};
            playlist_manager::get()->playlist_get_name(index.value(), name_ptr);
            std::string name = name_ptr.c_str();

            playlist_manager::get()->playlist_enum_items(index.value(),
                                                         [&items](size_t, const metadb_handle_ptr& handle, bool)
                                                         {
                                                             items.add_item(handle);
                                                             return true;
                                                         }, bit_array_true{});
            auto result = handle_tracks(items, limit, offset, query, fields);
            auto result2 = playlist_result{result, "", ""};
            auto current_track = playlist_manager::get()->playlist_get_focus_item(index.value());
            result2.current_track_index = "-1";
            if (current_track != pfc::infinite_size)
            {
                result2.current_track_index = std::to_string(current_track);
                auto track = metadb_handle_ptr{};
                if (playlist_manager::get()->playlist_get_item_handle(track, index.value(), current_track))
                {
                    auto info = track->get_info_ref();
                    result2.current_track_path = track->get_path();
                }
            }
            return std::make_tuple(result2.result, result2.current_track_index, result2.current_track_path, playing, active, name);
        });

    auto [tracks, total] = result;
    return {
        {
            {"type", "text"},
            {
                "text",
                std::format(
                    "Name: {}, Playing?: {}, Active? {}, Current focused track: {} (index {}), Total tracks: {}, Returned tracks: {}, tracks: {}",
                    name, playing ? "Yes" : "No", active ? "Yes" : "No", path.empty() ? "None" : path, index, total,
                    tracks.size(), tracks.dump())
            }
        }
    };
}

mcp::json foobar_mcp::list_current_track_handler(const mcp::json& params, const std::string& session_id) const
{
    std::vector<std::string> fields = {"path", "artist", "title", "album", "duration_seconds"};
    if (params.contains("fields"))
    {
        fields.clear();
        for (const auto& f : params["fields"])
        {
            fields.push_back(f.get<std::string>());
        }
    }

    auto json = safe_main_thread_call(
        [fields = std::move(fields)]
        {
            pfc::list_t<metadb_handle_ptr> items;
            auto track = metadb_handle_ptr{};
            auto playing = play_control::get()->get_now_playing(track);
            if (!playing)
            {
                return mcp::json(nullptr);
            }
            items.add_item(track);
            auto result = handle_tracks(items, 1, 0, "", fields);
            auto json = result.tracks[0];
            json["is_playing"] = play_control::get()->is_playing() && !play_control::get()->is_paused();
            json["position_seconds"] = play_control::get()->playback_get_position();
            return json;
        });

    if (json == nullptr)
    {
        return {
            {
                {"type", "text"},
                {
                    "text",
                    "No track is currently selected"
                }
            }
        };
    }
    return {
        {
            {"type", "text"},
            {
                "text",
                std::format("current track: {}", json.dump())
            }
        }
    };
}

mcp::json foobar_mcp::add_tracks_handler(const mcp::json& params, const std::string& session_id)
{
    if (!params.contains("paths"))
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "paths parameter is required");
    }
    auto position = params.value("position", SIZE_MAX);

    auto paths = params["paths"].get<std::vector<std::string>>();

    auto added = safe_main_thread_call([paths = std::move(paths), position]()
    {
        auto list = pfc::list_t<metadb_handle_ptr>{};
        for (const auto& p : paths)
        {
            metadb_handle_ptr handle;
            metadb::get()->handle_create(handle, make_playable_location(p.c_str(), 0));
            if (handle.is_valid())
            {
                list.add_item(handle);
            }
        }
        // Request metadata info load for all added tracks
        if (list.get_count() > 0)
        {
            metadb_io_v2::get()->load_info_async(list, metadb_io::load_info_default, nullptr, 0,
                                                 fb2k::makeCompletionNotify([](unsigned)
                                                 {
                                                 }));
        }

        playlist_manager::get()->playlist_insert_items(playlist_manager::get()->get_active_playlist(), position, list,
                                                       bit_array_true{});
        return list.get_count();
    });

    return {
        {
            {"type", "text"},
            {
                "text",
                std::format("Added {} tracks to the active playlist ({} failed)", added, paths.size() - added)
            }
        }
    };
}

mcp::json foobar_mcp::remove_tracks_handler(const mcp::json& params, const std::string& session_id)
{
    auto track_indices = params["track_indices"].get<std::set<size_t>>();

    auto removed = safe_main_thread_call([track_indices = std::move(track_indices)]()
    {
        auto removed = size_t{0};
        const auto mask = pfc::bit_array_lambda([&track_indices, &removed](size_t index)
        {
            const auto remove = track_indices.contains(index);
            if (remove) ++removed;
            return remove;
        });
        playlist_manager::get()->playlist_remove_items(playlist_manager::get()->get_active_playlist(), mask);
        return removed;
    });

    return {
        {
            {"type", "text"},
            {
                "text",
                std::format("Removed {} tracks from the playlist ({} failed)", removed, track_indices.size() - removed)
            }
        }
    };
}

mcp::json foobar_mcp::remove_all_tracks_handler(const mcp::json& params, const std::string& session_id)
{
    safe_main_thread_call([]()
    {
        const auto mask = bit_array_true{};
        playlist_manager::get()->playlist_remove_items(playlist_manager::get()->get_active_playlist(), mask);
    });

    return {
        {
            {"type", "text"},
            {
                "text",
                "Removed all tracks from the active playlist"
            }
        }
    };
}

mcp::json foobar_mcp::move_tracks_handler(const mcp::json& params, const std::string& session_id)
{
    if (!params.contains("order"))
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "order parameter is required");
    }

    auto order = params["order"].get<std::vector<size_t>>();

    safe_main_thread_call([order = std::move(order)]()
    {
        const auto playlist_index = playlist_manager::get()->get_active_playlist();
        const auto item_count = playlist_manager::get()->playlist_get_item_count(playlist_index);

        if (order.size() != item_count)
        {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "order length must match playlist length");
        }

        std::vector seen(item_count, false);
        std::vector<t_size> reorder(item_count);

        for (size_t i = 0; i < item_count; ++i)
        {
            const auto target = order[i];
            if (target >= item_count || seen[target])
            {
                throw mcp::mcp_exception(mcp::error_code::invalid_params,
                                       "order must be a valid permutation of track indices");
            }
            seen[target] = true;
            reorder[i] = target;
        }
        playlist_manager::get()->playlist_reorder_items(playlist_index, reorder.data(), item_count);
    });

    return {
        {
            {"type", "text"},
            {
                "text",
                "Reordered tracks in the active playlist"
            }
        }
    };
}

mcp::json foobar_mcp::set_active_playlist_handler(const mcp::json& params, const std::string& session_id) const
{
    if (!params.contains("playlist_guid"))
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "playlist_guid parameter is required");
    }
    auto playlist_guid = params["playlist_guid"].get<std::string>();

    safe_main_thread_call([this, playlist_guid = std::move(playlist_guid)]()
    {
        const auto index = m_playlist_resource->get_playlist_index(playlist_guid);
        if (!index.has_value())
        {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Playlist not found");
        }
        playlist_manager::get()->set_active_playlist(index.value());
    });

    return {
        {
            {"type", "text"},
            {
                "text",
                "Active playlist set successfully"
            }
        }
    };
}

mcp::json foobar_mcp::set_playing_playlist_handler(const mcp::json& params, const std::string& session_id) const
{
    if (!params.contains("playlist_guid"))
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "playlist_guid parameter is required");
    }
    auto playlist_guid = params["playlist_guid"].get<std::string>();

    safe_main_thread_call([this, playlist_guid = std::move(playlist_guid)]()
    {
        const auto index = m_playlist_resource->get_playlist_index(playlist_guid);
        if (!index.has_value())
        {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Playlist not found");
        }
        playlist_manager::get()->set_active_playlist(index.value());
        playlist_manager::get()->set_playing_playlist(index.value());
    });

    return {
        {
            {"type", "text"},
            {
                "text",
                "Playing playlist set successfully"
            }
        }
    };
}

mcp::json foobar_mcp::set_playback_state_handler(const mcp::json& params, const std::string& session_id)
{
    auto state = params["state"].get<bool>();
    safe_main_thread_call([state]()
    {
        play_control::get()->pause(state);
    });

    return {
        {
            {"type", "text"},
            {
                "text",
                state ? "Playback started" : "Playback paused"
            }
        }
    };
}

mcp::json foobar_mcp::play_at_index_handler(const mcp::json& params, const std::string& session_id)
{
    if (!params.contains("index"))
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "index parameter is required");
    }
    auto index = params["index"].get<size_t>();

    safe_main_thread_call([index]()
    {
        const auto playlist_index = playlist_manager::get()->get_active_playlist();
        if (const auto count = playlist_manager::get()->playlist_get_item_count(playlist_index); index >= count)
        {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Index out of bounds");
        }
        playlist_manager::get()->playlist_execute_default_action(playlist_index, index);
    });

    return {
        {
            {"type", "text"},
            {
                "text",
                std::format("Playing track at index {}", index)
            }
        }
    };
}

mcp::json foobar_mcp::set_focus_handler(const mcp::json& params, const std::string& session_id)
{
    if (!params.contains("index"))
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "index parameter is required");
    }
    auto index = params["index"].get<size_t>();

    safe_main_thread_call([index]()
    {
        const auto playlist_index = playlist_manager::get()->get_active_playlist();
        if (const auto count = playlist_manager::get()->playlist_get_item_count(playlist_index); index >= count)
        {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Index out of bounds");
        }
        playlist_manager::get()->playlist_set_focus_item(playlist_index, index);
    });

    return {
        {
            {"type", "text"},
            {
                "text",
                std::format("Set focus to track at index {}", index)
            }
        }
    };
}

mcp::json foobar_mcp::create_playlist_handler(const mcp::json& params, const std::string& session_id)
{
    if (!params.contains("name"))
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "name parameter is required");
    }
    auto name = params["name"].get<std::string>();

    auto playlist_guid = safe_main_thread_call([name = std::move(name)]()
    {
        auto index = playlist_manager::get()->create_playlist(name.c_str(), pfc::infinite_size, pfc::infinite_size);
        playlist_manager::get()->playlist_rename(index, name.c_str(), pfc::infinite_size);
        return std::to_string(index);
    });

    return {
        {
            {"type", "text"},
            {
                "text",
                std::format("Created new playlist with name '{}'", name)
            }
        }
    };
}

mcp::json foobar_mcp::rename_playlist_handler(const mcp::json& params, const std::string& session_id)
{
    if (!params.contains("playlist_guid") || !params.contains("new_name"))
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "playlist_guid and new_name parameters are required");
    }
    auto playlist_guid = params["playlist_guid"].get<std::string>();
    auto new_name = params["new_name"].get<std::string>();

    safe_main_thread_call(
        [this, playlist_guid = std::move(playlist_guid), new_name = std::move(new_name)]()
        {
            const auto index = m_playlist_resource->get_playlist_index(playlist_guid);
            if (!index.has_value())
            {
                throw mcp::mcp_exception(mcp::error_code::invalid_params, "Playlist not found");
            }
            playlist_manager::get()->playlist_rename(index.value(), new_name.c_str(), pfc::infinite_size);
        });

    return {
        {
            {"type", "text"},
            {
                "text",
                std::format("Renamed playlist '{}' to '{}'", playlist_guid, new_name)
            }
        }
    };
}

mcp::json foobar_mcp::delete_playlist_handler(const mcp::json& params, const std::string& session_id)
{
    if (!params.contains("playlist_guid"))
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "playlist_guid parameter is required");
    }
    auto playlist_guid = params["playlist_guid"].get<std::string>();

    safe_main_thread_call([this, playlist_guid = std::move(playlist_guid)]()
    {
        const auto index = m_playlist_resource->get_playlist_index(playlist_guid);
        if (!index.has_value())
        {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Playlist not found");
        }
        playlist_manager::get()->remove_playlist(index.value());
    });

    return {
        {
            {"type", "text"},
            {
                "text",
                std::format("Deleted playlist '{}'", playlist_guid)
            }
        }
    };
}

mcp_manager& mcp_manager::instance()
{
    static mcp_manager mgr;
    return mgr;
}

void mcp_manager::start(const std::string& host, int port)
{
    server = std::make_unique<foobar_mcp>(host, port, playlist_resource_, current_track_resource_);
}

void mcp_manager::stop()
{
    server.reset();
}

void mcp_manager::restart(const std::string& host, int port)
{
    stop();
    start(host, port);
}
