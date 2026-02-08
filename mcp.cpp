#include "mcp.h"
#include <SDK/foobar2000.h>
#include <future>

foobar_mcp::foobar_mcp(const std::string& host, int port, std::shared_ptr<playlist_resource> playlist_resource)
    : server(mcp::server::configuration{host, port}), m_playlist_resource(std::move(playlist_resource))
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
                                  .with_string_param("query", "foobar2000 search query (e.g. 'artist HAS beatles')",
                                                     false)
                                  .with_array_param("fields", "Fields to return: "
                                                    "path, duration_seconds or any tag contained in audio files. "
                                                    "Default: path, artist, title, album, duration_seconds", "string",
                                                    false)
                                  .build();

    server.register_tool(list_library_tool, std::bind_front(&foobar_mcp::list_library_handler, this));

    const mcp::tool list_playlist_tool = mcp::tool_builder("list_playlist")
                                  .with_description("Get tracks from a playlist")
                                  .with_string_param("playlist_id", "ID of the playlist to retrieve tracks from", true)
                                  .with_number_param("limit", "Max tracks to return (default: 50)", false)
                                  .with_number_param("offset", "Skip first N tracks", false)
                                  .with_string_param("query", "foobar2000 search query (e.g. 'artist HAS beatles')",
                                                     false)
                                  .with_array_param("fields", "Fields to return: "
                                                    "path, duration_seconds or any tag contained in audio files. "
                                                    "Default: path, artist, title, album, duration_seconds", "string",
                                                    false)
                                  .build();

    server.start(false);
}

struct result
{
    mcp::json tracks;
    size_t total;
};
static result handle_tracks(pfc::list_t<metadb_handle_ptr> items, int limit, int offset, std::string query, std::vector<std::string> fields)
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
    std::vector<std::string> fields = {"path", "artist", "title", "album", "duration_seconds"};

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
                std::format("total_tracks: {}, Returned tracks: {}, tracks: {}", total, tracks.size(), tracks.dump())
            }
        }
    };
}

mcp::json foobar_mcp::list_playlist_handler(const mcp::json& params, const std::string& session_id) const
{
    if (!params.contains("playlist_id") || !params["playlist_id"].is_string())
    {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "playlist_id (string) parameter is required");
    }

    int limit = 50;
    int offset = 0;
    std::string query;
    std::vector<std::string> fields = {"path", "artist", "title", "album", "duration_seconds"};
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

    auto promise = std::promise<result>{};
    fb2k::inMainThread([this, &promise, limit, offset, query = std::move(query), fields = std::move(fields), playlist_id]()
    {
        pfc::list_t<metadb_handle_ptr> items;
        auto index = m_playlist_resource->get_playlist_index(playlist_id);
        if (!index.has_value())
        {
            promise.set_exception(std::make_exception_ptr(mcp::mcp_exception(mcp::error_code::invalid_params, "Playlist not found")));
            return;
        }
        playlist_manager::get()->playlist_enum_items(index.value(), [&items](size_t, const metadb_handle_ptr& handle, bool)
        {
            items.add_item(handle);
            return true;
        }, bit_array_true{});
        promise.set_value(handle_tracks(items, limit, offset, query, fields));
    });

    auto [tracks, total] = promise.get_future().get();
    return {
            {
                {"type", "text"},
                {
                    "text",
                    std::format("total_tracks: {}, Returned tracks: {}, tracks: {}", total, tracks.size(), tracks.dump())
                }
            }
    };
    return {};
}


mcp_manager& mcp_manager::instance()
{
    static mcp_manager mgr;
    return mgr;
}

void mcp_manager::start(const std::string& host, int port)
{
    server = std::make_unique<foobar_mcp>(host, port, playlist_resource);
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
