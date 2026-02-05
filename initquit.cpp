//
// Created by PC on 05/02/2026.
//
#include <SDK/foobar2000.h>
#include "mcp.h"
#include "Preferences.h"

class myinitquit : public initquit
{
public:
    void on_init() override
    {
        pfc::string8 endpoint;
        foo_ai::get_endpoint(endpoint);

        // Parse host:port from endpoint
        std::string ep(endpoint.c_str());
        std::string host = "localhost";
        int port = 12345;

        auto colon = ep.find(':');
        if (colon != std::string::npos)
        {
            host = ep.substr(0, colon);
            port = std::stoi(ep.substr(colon + 1));
        }
        else if (!ep.empty())
        {
            host = ep;
        }

        mcp_manager::instance().start(host, port);
    }

    void on_quit() override
    {
        mcp_manager::instance().stop();
    }
};

static initquit_factory_t<myinitquit> g_myinitquit_factory;
