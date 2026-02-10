#include "mcp.h"
#include "safe_main_thread_call.h"
#include <SDK/foobar2000.h>
#include <spdlog/spdlog.h>
#include <pfc/unicode-normalize.h>

#include <string>

namespace
{
    std::wstring utf8_to_utf16(const std::string& s)
    {
        if (s.empty()) return {};
        const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
        if (len <= 0) return {};
        std::wstring result(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), result.data(), len);
        return result;
    }

    std::string utf16_to_utf8(const std::wstring& s)
    {
        if (s.empty()) return {};
        const int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0, nullptr,
                                            nullptr);
        if (len <= 0) return {};
        std::string result(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), result.data(), len, nullptr, nullptr);
        return result;
    }

    bool potential_normalization_issue(const std::string& utf8_str)
    {
        auto nfc = pfc::unicodeNormalizeC(utf8_str.c_str());
        auto nfd = pfc::unicodeNormalizeD(utf8_str.c_str());

        // If NFC and NFD differ, there's a potential normalization issue
        return nfc != nfd;
    }

    std::wstring get_actual_path(const std::wstring& inputPath)
    {
        // Open handle; for dirs FILE_FLAG_BACKUP_SEMANTICS is required.
        HANDLE h = CreateFileW(
            inputPath.c_str(),
            0, // no access needed to query metadata
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr);

        if (h == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error("CreateFileW failed");
        }

        DWORD flags = FILE_NAME_NORMALIZED; // request normalized name
        // You may also use VOLUME_NAME_DOS to get "C:\..." form (often with \\?\ prefix).
        flags |= VOLUME_NAME_DOS;

        DWORD needed = GetFinalPathNameByHandleW(h, nullptr, 0, flags);
        if (needed == 0)
        {
            CloseHandle(h);
            throw std::runtime_error("GetFinalPathNameByHandleW size query failed");
        }

        std::wstring out(needed, L'\0');
        DWORD written = GetFinalPathNameByHandleW(h, &out[0], needed, flags);
        CloseHandle(h);

        if (written == 0)
        {
            throw std::runtime_error("GetFinalPathNameByHandleW failed");
        }

        // written does not necessarily include the trailing null in a convenient way
        out.resize(written);

        // Common: result begins with "\\?\"
        // If you want a normal Win32 path, you can optionally strip it when safe:
        // if (out.rfind(L"\\\\?\\", 0) == 0) out = out.substr(4);

        return out;
    }
}

std::optional<std::string> resolve_filesystem_path(const std::string& utf8_path)
{
    std::wstring wpath = utf8_to_utf16(utf8_path);
    if (wpath.empty()) return std::nullopt;

    try
    {
        auto normalized_path = get_actual_path(wpath);
        return utf16_to_utf8(normalized_path);
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to resolve path '{}': {}", utf8_path, e.what());
        return std::nullopt;
    }
}

static mcp::server::configuration get_configuration(const std::string& host, int port)
{
    auto config = mcp::server::configuration{};
    config.host = host;
    config.port = port;
    return config;
}

foobar_mcp::foobar_mcp(const std::string& host, int port)
    : server(get_configuration(host, port))
{
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

    server.register_resource(playlist_resource_);
    server.register_resource(current_track_resource_);

    const mcp::tool list_library_tool = mcp::tool_builder("list_library")
                                        .with_description("Get tracks from the user's media library")
                                        .with_number_param("limit",
                                                           "Max tracks to return (default: 50). "
                                                           "Use a low limit with sorting when you want to find <the oldest song> or "
                                                           "<the longest song> for example. ",
                                                           false)
                                        .with_number_param("offset", "Skip first N tracks", false)
                                        .with_string_param(
                                            "query", "foobar2000 search query "
                                            "(e.g. '%artist% HAS beatles SORT DESCENDING BY %date%') "
                                            "Operators: AND, OR, NOT, HAS, IS, SORT BY, EQUAL, GREATER, LESS, BEFORE, DURING, AFTER, DURING LAST n SECONDS/MINUTES/HOURS/DAYS/WEEKS, MISSING, PRESENT. "
                                            "HAS can be used with * instead of field name. "
                                            "Special query: ALL to get all tracks without filtering. Can be sorted. "
                                            "Docs: https://wiki.hydrogenaudio.org/index.php?title=Foobar2000:Query_syntax ",
                                            false)
                                        .with_array_param("fields", "Fields to return: "
                                                          "path, duration_seconds or any tag contained in audio files. "
                                                          "Default: path, artist, title, album. "
                                                          "Other common tags: genre, date, composer, performer, "
                                                          "album artist, tracknumber, discnumber, comment, subtitle, "
                                                          "bitrate, filesize, samplerate, channels, last_modified",
                                                          "string",
                                                          false)
                                        .build();

    server.register_tool(list_library_tool, std::bind_front(&foobar_mcp::list_library_handler, this));

    const mcp::tool list_playlists_tool = mcp::tool_builder("list_playlists")
                                          .with_description(
                                              "Get all playlists with their metadata (name, track count, last modified, active/playing status). "
                                              "Use this to discover available playlists and their GUIDs for use with other playlist tools. "
                                              "You can read the playlists://. resource to get the same data and subscribe to changes.")
                                          .build();

    server.register_tool(list_playlists_tool, std::bind_front(&foobar_mcp::list_playlists_handler, this));

    const mcp::tool list_playlist_tool = mcp::tool_builder("list_playlist")
                                         .with_description("Get tracks from a playlist")
                                         .with_string_param("playlist_guid",
                                                            "ID of the playlist to retrieve tracks from",
                                                            true)
                                         .with_number_param("limit",
                                                            "Max tracks to return (default: 50). "
                                                            "Use a low limit with sorting when you want to find <the oldest song> or "
                                                            "<the longest song> for example. ",
                                                            false)
                                         .with_number_param("offset", "Skip first N tracks", false)
                                         .with_string_param(
                                             "query", "foobar2000 search query "
                                             "(e.g. '%artist% HAS beatles SORT DESCENDING BY %date%') "
                                             "Operators: AND, OR, NOT, HAS, IS, SORT BY, EQUAL, GREATER, LESS, BEFORE, DURING, AFTER, DURING LAST n SECONDS/MINUTES/HOURS/DAYS/WEEKS, MISSING, PRESENT. "
                                             "HAS can be used with * instead of field name. "
                                             "Special query: ALL to get all tracks without filtering. Can be sorted. "
                                             "Docs: https://wiki.hydrogenaudio.org/index.php?title=Foobar2000:Query_syntax ",
                                             false)
                                         .with_array_param("fields", "Fields to return: "
                                                           "path, duration_seconds or any tag contained in audio files. "
                                                           "Default: path, artist, title, album. "
                                                           "Other common tags: genre, date, composer, performer, "
                                                           "album artist, tracknumber, discnumber, comment, subtitle, "
                                                           "bitrate, filesize, samplerate, channels, last_modified",
                                                           "string",
                                                           false)
                                         .build();

    server.register_tool(list_playlist_tool, std::bind_front(&foobar_mcp::list_playlist_handler, this));

    auto current_track_tool = mcp::tool_builder("list_current_track")
                              .with_description("Get the currently playing track")
                              .with_array_param("fields", "Fields to return: "
                                                "path, duration_seconds or any tag contained in audio files. "
                                                "Default: path, artist, title, album. "
                                                "Other common tags: genre, date, composer, performer, "
                                                "album artist, tracknumber, discnumber, comment, subtitle, "
                                                "bitrate, filesize, samplerate, channels, last_modified",
                                                "string",
                                                false)
                              .build();

    server.register_tool(current_track_tool, std::bind_front(&foobar_mcp::list_current_track_handler, this));

    auto set_active_playlist_tool = mcp::tool_builder("set_active_playlist")
                                    .with_description(
                                        "Set the active playlist. Adding, modifying and removing tracks happens on the active playlist. "
                                        "This does not change the currently playing playlist or track. ")
                                    .with_string_param("playlist_guid", "GUID of the playlist to set as active. "
                                                       "Get GUIDs from the playlists://. resource.", true)
                                    .build();

    server.register_tool(set_active_playlist_tool, std::bind_front(&foobar_mcp::set_active_playlist_handler, this));

    auto set_playing_playlist_tool = mcp::tool_builder("set_playing_playlist")
                                     .with_description(
                                         "Set the playing playlist. Also sets it as the active playlist. "
                                         "The music player will start picking tracks to play from this playlist. "
                                         "Does not start playback on its own. Use set_playback_state to play or pause.")
                                     .with_string_param("playlist_guid",
                                                        "GUID of the playlist to set as currently playing. "
                                                        "Get GUIDs from the playlists://. resource.",
                                                        true)
                                     .build();

    server.register_tool(set_playing_playlist_tool, std::bind_front(&foobar_mcp::set_playing_playlist_handler, this));

    auto set_playback_state_tool = mcp::tool_builder("set_playback_state")
                                   .with_description(
                                       "Set the playback state. Playing (true) or paused (false). "
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
                           .with_array_param("uris", "Absolute file uris to add", "string", true)
                           .with_number_param("index", "Index to insert at (default: append)", false)
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

static result handle_tracks(search_index::ptr items, const int limit, const int offset,
                            std::string query,
                            const std::vector<std::string>& fields)
{
    mcp::json tracks = mcp::json::array();

    // Apply search filter if query provided
    auto list = pfc::list_t<metadb_handle_ptr>{};
    if (query.empty())
    {
        query = "ALL";
    }
    try
    {
        auto mgr = search_filter_manager_v2::get();
        search_filter_v2::ptr filter = mgr->create_ex(query.c_str(),
                                                      nullptr,
                                                      search_filter_manager_v2::KFlagAllowSort | search_filter_manager_v2::KFlagSuppressNotify);
        const auto res = items->search(filter, nullptr, search_index::flag_sort, fb2k::noAbort);
        list = res->as_list_of<metadb_handle>();
    }
    catch (std::exception& e)
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params,
                                 std::format("Invalid search query: {}", e.what()));
    }

    const size_t total = list.get_count();
    const size_t start = std::min<size_t>(offset, total);
    const size_t end = std::min<size_t>(start + limit, total);

    for (size_t i = start; i < end; ++i)
    {
        metadb_handle_ptr item = list[i];
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
        const auto items = search_index_manager::get()->get_library_index();
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

mcp::json foobar_mcp::list_playlists_handler(const mcp::json& params, const std::string& session_id) const
{
    auto resource_data = playlist_resource_->read();
    return {
        {
            {"type", "text"},
            {"text", resource_data["text"]}
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
            auto playlist_manager = playlist_manager_v5::get();
            auto guid = pfc::GUID_from_text(playlist_guid.c_str());
            auto index = playlist_manager->find_playlist_by_guid(guid);
            if (index == pfc::infinite_size)
            {
                throw mcp::mcp_exception(mcp::error_code::invalid_params, "Playlist not found");
            }
            bool active = playlist_manager::get()->get_active_playlist() == index;
            bool playing = playlist_manager::get()->get_playing_playlist() == index;
            auto name_ptr = pfc::string8{};
            playlist_manager::get()->playlist_get_name(index, name_ptr);
            std::string name = name_ptr.c_str();
            auto search_index = search_index_manager::get()->create_playlist_index(guid);
            auto result = handle_tracks(search_index, limit, offset, query, fields);
            auto result2 = playlist_result{result, "", ""};
            auto current_track = playlist_manager::get()->playlist_get_focus_item(index);
            result2.current_track_index = "-1";
            if (current_track != pfc::infinite_size)
            {
                result2.current_track_index = std::to_string(current_track);
                auto track = metadb_handle_ptr{};
                if (playlist_manager::get()->playlist_get_item_handle(track, index, current_track))
                {
                    auto info = track->get_info_ref();
                    result2.current_track_path = track->get_path();
                }
            }
            return std::make_tuple(result2.result, result2.current_track_index, result2.current_track_path, playing,
                                   active, name);
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
            auto search_index = search_index_manager::get()->create_index(items, nullptr);
            auto result = handle_tracks(search_index, 1, 0, "", fields);
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
    if (!params.contains("uris"))
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "uris parameter is required");
    }
    auto index = params.value("index", SIZE_MAX);

    auto uris = params["uris"].get<std::vector<std::string>>();
    auto uris_size = uris.size();

    auto [added, inserted_index] = safe_main_thread_call([uris = std::move(uris), index]()
    {
        auto list = pfc::list_t<metadb_handle_ptr>{};
        for (const auto& p : uris)
        {
            std::string path = p;

            // Check if path starts with file:// or has no protocol and contains unicode
            bool is_file_uri = path.starts_with("file://");
            bool has_no_protocol = path.find("://") == std::string::npos;
            bool can_have_norm_issue = potential_normalization_issue(path);

            if ((is_file_uri || has_no_protocol) && can_have_norm_issue)
            {
                std::string filesystem_path = path;

                // Remove file:// prefix if present
                if (is_file_uri)
                {
                    filesystem_path = path.substr(7);
                }

                auto c_path = reinterpret_cast<const char8_t*>(filesystem_path.c_str());
                auto normalized_c = pfc::unicodeNormalizeC(filesystem_path.c_str());
                auto normalized_d = pfc::unicodeNormalizeD(filesystem_path.c_str());
                if (auto resolved = std::filesystem::path(c_path); exists(resolved))
                {
                }
                else if (auto resolved = std::filesystem::path(reinterpret_cast<const char8_t*>(normalized_c.c_str()));
                    exists(resolved))
                {
                    path = normalized_c.c_str();
                }
                else if (auto resolved = std::filesystem::path(reinterpret_cast<const char8_t*>(normalized_d.c_str()));
                    exists(resolved))
                {
                    path = normalized_d.c_str();
                }
                else
                {
                    spdlog::error("Failed to resolve path: {}", path);
                    continue;
                }
            }

            metadb_handle_ptr handle;
            metadb::get()->handle_create(handle, make_playable_location(path.c_str(), 0));
            if (handle.is_valid())
            {
                list.add_item(handle);
            }
        }
        // Request metadata info load for all added tracks
        if (list.get_count() > 0)
        {
            metadb_io_v2::get()->load_info_async(list, metadb_io::load_info_default, nullptr, 0,
                                                 nullptr);
        }

        auto inserted_index = playlist_manager::get()->activeplaylist_insert_items(index, list,
            bit_array_true{});
        return std::make_pair(list.get_count(), inserted_index);
    });

    return {
        {
            {"type", "text"},
            {
                "text",
                std::format("Added {} tracks to the active playlist ({} failed) "
                            "starting at index {}", added, uris_size - added, inserted_index)
            }
        }
    };
}

mcp::json foobar_mcp::remove_tracks_handler(const mcp::json& params, const std::string& session_id)
{
    auto track_indices = params["track_indices"].get<std::set<size_t>>();
    auto track_indices_size = track_indices.size();

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
                std::format("Removed {} tracks from the playlist ({} failed)", removed, track_indices_size - removed)
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
        const auto index = playlist_manager_v5::get()->
            find_playlist_by_guid(pfc::GUID_from_text(playlist_guid.c_str()));
        if (index == pfc::infinite_size)
        {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Playlist not found");
        }
        playlist_manager::get()->set_active_playlist(index);
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

    safe_main_thread_call([playlist_guid = std::move(playlist_guid)]()
    {
        const auto index = playlist_manager_v5::get()->
            find_playlist_by_guid(pfc::GUID_from_text(playlist_guid.c_str()));
        if (index == pfc::infinite_size)
        {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Playlist not found");
        }
        playlist_manager::get()->set_active_playlist(index);
        playlist_manager::get()->set_playing_playlist(index);
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
        auto track = metadb_handle_ptr{};
        auto playing = play_control::get()->get_now_playing(track);
        if (!playing)
        {
            auto active_playlist = playlist_manager::get()->get_active_playlist();
            auto focus_item = playlist_manager::get()->playlist_get_focus_item(active_playlist);
            if (focus_item == pfc::infinite_size)
            {
                focus_item = 0;
            }
            auto count = playlist_manager::get()->playlist_get_item_count(active_playlist);
            if (count == 0)
            {
                throw mcp::mcp_exception(mcp::error_code::invalid_params,
                                         "Active playlist is empty, cannot start playback");
            }
            playlist_manager::get()->activeplaylist_execute_default_action(focus_item);
        }
        else
        {
            play_control::get()->pause(!state);
        }
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
        if (const auto count = playlist_manager::get()->activeplaylist_get_item_count(); index >= count)
        {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Index out of bounds");
        }
        playlist_manager::get()->activeplaylist_execute_default_action(index);
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
        if (const auto count = playlist_manager::get()->activeplaylist_get_item_count(); index >= count)
        {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Index out of bounds");
        }
        playlist_manager::get()->activeplaylist_set_focus_item(index);
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
            const auto index = playlist_manager_v5::get()->find_playlist_by_guid(
                pfc::GUID_from_text(playlist_guid.c_str()));
            if (index == pfc::infinite_size)
            {
                throw mcp::mcp_exception(mcp::error_code::invalid_params, "Playlist not found");
            }
            playlist_manager::get()->playlist_rename(index, new_name.c_str(), pfc::infinite_size);
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
        const auto index = playlist_manager_v5::get()->
            find_playlist_by_guid(pfc::GUID_from_text(playlist_guid.c_str()));
        if (index == pfc::infinite_size)
        {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Playlist not found");
        }
        playlist_manager::get()->remove_playlist(index);
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
    try
    {
        server = std::make_unique<foobar_mcp>(host, port);
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to start MCP server: {}", e.what());
        server = nullptr;
    }
}

void mcp_manager::stop()
{
    server.reset();
}
