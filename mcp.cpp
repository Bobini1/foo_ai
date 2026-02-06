#include "mcp.h"
#include <SDK/foobar2000.h>
#include <future>

foobar_mcp::foobar_mcp(const std::string& host, int port)
    : server(std::make_unique<mcp::server>(mcp::server::configuration{host, port}))
{
    // Set server info and capabilities
    server->set_server_info("foo_ai", "1.0.0");
    server->set_capabilities({
        {"tools", mcp::json::object()}
    });

    // Register the list_library tool
    mcp::tool list_library_tool = mcp::tool_builder("list_library")
                                  .with_description("Get tracks from the user's media library")
                                  .with_number_param("limit", "Max tracks to return (default: 100)", false)
                                  .with_number_param("offset", "Skip first N tracks", false)
                                  .with_string_param("query", "foobar2000 search query (e.g. 'artist HAS beatles')",
                                                     false)
                                  .with_array_param("fields", "Fields to return: "
                                                    "path, duration_seconds or any tag contained in audio files. "
                                                    "Default: path, artist, title, album, duration_seconds", "string",
                                                    false)
                                  .with_string_param("sort_by", "Field to sort by", false)
                                  .with_boolean_param("descending", "Sort descending", false)
                                  .build();

    server->register_tool(list_library_tool,
                          [this](const mcp::json& params, const std::string& session_id)
                          {
                              return list_library_handler(params, session_id);
                          });

    server->start(false);
}

mcp::json foobar_mcp::list_library_handler(const mcp::json& params, const std::string& session_id)
{
    int limit = 100;
    int offset = 0;
    std::string query;
    std::set<std::string> fields = {"path", "artist", "title", "album", "duration_seconds"};

    if (params.contains("limit")) limit = params["limit"].get<int>();
    if (params.contains("offset")) offset = params["offset"].get<int>();
    if (params.contains("query")) query = params["query"].get<std::string>();
    if (params.contains("fields"))
    {
        fields.clear();
        for (const auto& f : params["fields"])
        {
            fields.insert(f.get<std::string>());
        }
    }

    std::promise<std::pair<mcp::json, size_t>> promise;
    auto future = promise.get_future();

    fb2k::inMainThread([=, &promise]() mutable
    {
        mcp::json tracks = mcp::json::array();

        auto lib_api = library_manager::get();
        pfc::list_t<metadb_handle_ptr> items;
        lib_api->get_all_items(items);

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

                pfc::list_t<bool> results;
                results.set_count(items.get_count());
                filter->test_multi(items, results.get_ptr());
                pfc::list_t<metadb_handle_ptr> filtered_items;
                for (t_size i = 0; i < items.get_count(); ++i)
                {
                    if (results[i])
                    {
                        filtered_items.add_item(items[i]);
                    }
                }
                items = std::move(filtered_items);
            }
            catch (...)
            {
                // Invalid query, use unfiltered
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


mcp_manager& mcp_manager::instance()
{
    static mcp_manager mgr;
    return mgr;
}

void mcp_manager::start(const std::string& host, int port)
{
    m_server = std::make_unique<foobar_mcp>(host, port);
}

void mcp_manager::stop()
{
    m_server.reset();
}

void mcp_manager::restart(const std::string& host, int port)
{
    stop();
    start(host, port);
}
