#include "mcp.h"
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
        {"tools", mcp::json::object()}
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
                                         .with_string_param("playlist_id", "ID of the playlist to retrieve tracks from",
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
    std::promise<result> promise;
    auto future = promise.get_future();

    fb2k::inMainThread([=, &promise, items = std::move(items)]() mutable
    {
        mcp::json tracks = mcp::json::array();

        // Apply search filter if query provided
        if (!query.empty())
        {
            try
            {
                search_filter_v2::ptr filter;
                auto mgr = search_filter_manager_v2::get();
                filter = mgr->create_ex(query.c_str(),
                                        fb2k::makeCompletionNotify([](unsigned)
                                        {
                                        }),
                                        search_filter_manager_v2::KFlagSuppressNotify);

                auto list = metadb_handle_list{items};
                filter->test_multi_here(list, fb2k::noAbort);
            }
            catch (...)
            {
                promise.set_exception(std::current_exception());
                return;
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

        promise.set_value({std::move(tracks), total});
    });

    using namespace std::string_literals;
    auto [tracks, total] = future.get();
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

    auto promise = std::promise<result>{};
    fb2k::inMainThread([&promise, limit, offset, query = std::move(query), fields = std::move(fields)]()
    {
        const auto lib_api = library_manager::get();
        pfc::list_t<metadb_handle_ptr> items;
        lib_api->get_all_items(items);
        promise.set_value(handle_tracks(items, limit, offset, query, fields));
    });

    auto [tracks, total] = promise.get_future().get();
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
    if (!params.contains("playlist_id"))
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "playlist_id (string) parameter is required");
    }

    int limit = 50;
    int offset = 0;
    std::string query;
    std::vector<std::string> fields = {"path", "artist", "title", "album"};
    auto playlist_id = params["playlist_id"].get<std::string>();
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

    auto promise = std::promise<playlist_result>{};
    auto playing = false;
    auto active = false;
    fb2k::inMainThread(
        [this, &promise, limit, offset, query = std::move(query), fields = std::move(fields), playlist_id, &playing, &
            active]() mutable
        {
            pfc::list_t<metadb_handle_ptr> items;
            auto index = m_playlist_resource->get_playlist_index(playlist_id);
            if (!index.has_value())
            {
                promise.set_exception(
                    std::make_exception_ptr(mcp::mcp_exception(mcp::error_code::invalid_params, "Playlist not found")));
                return;
            }
            active = playlist_manager::get()->get_active_playlist() == index.value();
            playing = playlist_manager::get()->get_playing_playlist() == index.value();

            playlist_manager::get()->playlist_enum_items(index.value(),
                                                         [&items](size_t, const metadb_handle_ptr& handle, bool)
                                                         {
                                                             items.add_item(handle);
                                                             return true;
                                                         }, bit_array_true{});
            auto result = handle_tracks(items, limit, offset, query, fields);
            auto result2 = playlist_result{result, "", ""};
            auto current_track = playlist_manager::get()->playlist_get_focus_item(index.value());
            result2.current_track_index = -1;
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
            promise.set_value(result2);
        });

    auto [result, index, path] = promise.get_future().get();
    auto [tracks, total] = result;
    return {
        {
            {"type", "text"},
            {
                "text",
                std::format(
                    "Playing?: {}, Active? {}, Current focused track: {} (index {}), Total tracks: {}, Returned tracks: {}, tracks: {}",
                    playing ? "Yes" : "No", active ? "Yes" : "No", path.empty() ? "None" : path, index, total,
                    tracks.size(), tracks.dump())
            }
        }
    };
}

mcp::json foobar_mcp::list_current_track_handler(const mcp::json& params, const std::string& session_id) const
{
    if (!params.contains("playlist_id") || !params["playlist_id"].is_string())
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "playlist_id (string) parameter is required");
    }

    std::vector<std::string> fields = {"path", "artist", "title", "album", "duration_seconds"};
    if (params.contains("fields"))
    {
        fields.clear();
        for (const auto& f : params["fields"])
        {
            fields.push_back(f.get<std::string>());
        }
    }

    auto promise = std::promise<mcp::json>{};
    fb2k::inMainThread(
        [&promise, fields = std::move(fields)]
        {
            pfc::list_t<metadb_handle_ptr> items;
            auto track = metadb_handle_ptr{};
            auto playing = play_control::get()->get_now_playing(track);
            if (!playing)
            {
                promise.set_value(nullptr);
                return;
            }
            items.add_item(track);
            auto result = handle_tracks(items, 1, 0, "", fields);
            auto json = result.tracks[0].get<nlohmann::json::object_t>();
            json["is_playing"] = play_control::get()->is_playing();
            json["position_seconds"] = play_control::get()->playback_get_position();
            promise.set_value(json);
        });

    auto json = promise.get_future().get();
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


mcp_manager& mcp_manager::instance()
{
    static mcp_manager mgr;
    return mgr;
}

void mcp_manager::start(const std::string& host, int port)
{
    server = std::make_unique<foobar_mcp>(host, port, playlist_resource, current_track_resource);
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
