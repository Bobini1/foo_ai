#include <helpers/foobar2000+atl.h>

#include <pfc/int_types.h>
#include <SDK/cfg_var.h>

#include "resources.h"

#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>

#include <utility>
#include <spdlog/spdlog.h>

#include "mcp.h"

// Sample preferences interface: two meaningless configuration settings accessible through a preferences page and one accessible through advanced preferences.

// Dark Mode:
// (1) Add fb2k::CDarkModeHooks member.
// (2) Initialize it in our WM_INITDIALOG handler.
// (3) Tell foobar2000 that this prefernces page supports dark mode, by returning correct get_state() flags.
// That's all.


// These GUIDs identify the variables within our component's configuration file.
static constexpr GUID guid_IDC_editEndpoint = {
    0xbd5c777, 0x735c, 0x440d, {0x8c, 0x71, 0x49, 0xb6, 0xac, 0xff, 0xce, 0xb8}
};

static constexpr GUID guid_IDC_checkEnable = {
    0x482465a5, 0x7ab6, 0x42ee, {0xb8, 0x1a, 0x53, 0xe4, 0x6f, 0x56, 0xa2, 0xb6}
};

// defaults
static constexpr char cfg_editEndpoint[] = "localhost:9910";
static constexpr bool cfg_checkEnable = true;

namespace foo_ai
{
    cfg_var_modern::cfg_string IDC_editEndpoint(guid_IDC_editEndpoint, cfg_editEndpoint);
    cfg_var_modern::cfg_bool IDC_checkEnable(guid_IDC_checkEnable, cfg_checkEnable);

    void get_endpoint(pfc::string_base& out)
    {
        IDC_editEndpoint.get(out);
    }

    bool is_server_enabled()
    {
        return IDC_checkEnable.get();
    }

    void restart_mcp_server()
    {
        if (!is_server_enabled())
        {
            mcp_manager::instance().stop();
            return;
        }

        pfc::string8 endpoint;
        get_endpoint(endpoint);

        std::string ep(endpoint.c_str());
        std::string host = "localhost";
        int port = 9910;

        auto colon = ep.find(':');
        if (colon != std::string::npos)
        {
            host = ep.substr(0, colon);
            try
            {
                port = std::stoi(ep.substr(colon + 1));
            } catch (const std::exception&)
            {
                spdlog::error("Invalid port number in endpoint configuration: '{}'", ep.substr(colon + 1));
            }
        }
        else if (!ep.empty())
        {
            host = ep;
        }
        if (port < 1 || port > 65535)
        {
            spdlog::error("Port number out of range in endpoint configuration: {}", port);
            port = 9910;
        }

        mcp_manager::instance().start(host, port);
    }
}

using namespace foo_ai;

#ifdef _WIN32
class CMyPreferences : public CDialogImpl<CMyPreferences>, public preferences_page_instance
{
public:
    //Constructor - invoked by preferences_page_impl helpers - don't do Create() in here, preferences_page_impl does this for us
    explicit CMyPreferences(preferences_page_callback::ptr callback) : m_bMsgHandled(0), m_callback(std::move(callback))
    {
    }

    //Note that we don't bother doing anything regarding destruction of our class.
    //The host ensures that our dialog is destroyed first, then the last reference to our preferences_page_instance object is released, causing our object to be deleted.


    //dialog resource ID
    enum { IDD = IDD_PREFERENCES };

    // preferences_page_instance methods (not all of them - get_wnd() is supplied by preferences_page_impl helpers)
    t_uint32 get_state();
    void apply();
    void reset();

    //WTL message map
    BEGIN_MSG_MAP_EX(CMyPreferences)
        MSG_WM_INITDIALOG(OnInitDialog)
        COMMAND_HANDLER_EX(IDC_EDIT_ENDPOINT, EN_CHANGE, OnEditChange)
        COMMAND_HANDLER_EX(IDC_CHECK_ENABLE, BN_CLICKED, OnCheckboxChange)
    END_MSG_MAP()

private:
    BOOL OnInitDialog(CWindow, LPARAM);
    void OnEditChange(UINT, int, CWindow);
    void OnCheckboxChange(UINT, int, CWindow);
    bool HasChanged() const;
    void OnChanged();
    pfc::string8 UpdateSSEUrl();

    const preferences_page_callback::ptr m_callback;

    // Dark mode hooks object, must be a member of dialog class.
    fb2k::CDarkModeHooks m_dark;
};

BOOL CMyPreferences::OnInitDialog(CWindow, LPARAM)
{
    // Enable dark mode
    // One call does it all, applies all relevant hacks automatically
    m_dark.AddDialogWithControls(*this);

    auto string = pfc::string_formatter();
    IDC_editEndpoint.get(string);
    uSetDlgItemText(*this, IDC_EDIT_ENDPOINT, string);

    // Set checkbox state
    uButton_SetCheck(*this, IDC_CHECK_ENABLE, IDC_checkEnable.get());

    // Update SSE URL display
    UpdateSSEUrl();

    return FALSE;
}

void CMyPreferences::OnEditChange(UINT, int, CWindow)
{
    // not much to do here
    OnChanged();
}

void CMyPreferences::OnCheckboxChange(UINT, int, CWindow)
{
    OnChanged();
}

t_uint32 CMyPreferences::get_state()
{
    // IMPORTANT: Always return dark_mode_supported - tell foobar2000 that this preferences page is dark mode compliant.
    t_uint32 state = preferences_state::resettable | preferences_state::dark_mode_supported;
    if (HasChanged()) state |= preferences_state::changed;
    return state;
}

void CMyPreferences::reset()
{
    uSetDlgItemText(*this, IDC_EDIT_ENDPOINT, cfg_editEndpoint);
    uButton_SetCheck(*this, IDC_CHECK_ENABLE, cfg_checkEnable);
    OnChanged();
}

void CMyPreferences::apply()
{
    const auto endpoint = UpdateSSEUrl();
    IDC_editEndpoint = endpoint;

    // Save checkbox state
    IDC_checkEnable = uButton_GetCheck(*this, IDC_CHECK_ENABLE) != 0;

    restart_mcp_server();

    OnChanged();
    //our dialog content has not changed but the flags have - our currently shown values now match the settings so the apply button can be disabled
}

bool CMyPreferences::HasChanged() const
{
    pfc::string8 endpoint;
    uGetDlgItemText(*this, IDC_EDIT_ENDPOINT, endpoint);
    bool checkEnabled = uButton_GetCheck(*this, IDC_CHECK_ENABLE) != 0;
    //returns whether our dialog content is different from the current configuration (whether the apply button should be enabled or not)
    return endpoint != IDC_editEndpoint || checkEnabled != IDC_checkEnable.get();
}

void CMyPreferences::OnChanged()
{
    //tell the host that our state has changed to enable/disable the apply button appropriately.
    m_callback->on_state_changed();
}

pfc::string8 CMyPreferences::UpdateSSEUrl()
{
    pfc::string8 endpoint;
    uGetDlgItemText(*this, IDC_EDIT_ENDPOINT, endpoint);

    std::string ep(endpoint.c_str());
    std::string host = "localhost";
    int port = 9910;

    auto colon = ep.find(':');
    if (colon != std::string::npos)
    {
        host = ep.substr(0, colon);
        try
        {
            port = std::stoi(ep.substr(colon + 1));
        } catch (const std::exception&)
        {
            spdlog::error("Invalid port number in endpoint configuration: '{}'", ep.substr(colon + 1));
        }
    }
    else if (!ep.empty())
    {
        host = ep;
    }
    if (port < 1 || port > 65535)
    {
        spdlog::error("Port number out of range in endpoint configuration: {}", port);
        port = 9910;
    }

    std::string sseUrl = "http://" + host + ":" + std::to_string(port) + "/sse";
    uSetDlgItemText(*this, IDC_TEXT_SSE_URL, sseUrl.c_str());
    return (host + ":" + std::to_string(port)).c_str();
}

class preferences_page_myimpl : public preferences_page_impl<CMyPreferences>
{
    // preferences_page_impl<> helper deals with instantiation of our dialog; inherits from preferences_page_v3.
public:
    virtual ~preferences_page_myimpl() = default;
    const char* get_name() override;

    GUID get_guid() override;
    GUID get_parent_guid() override;
};

const char* preferences_page_myimpl::get_name()
{
    return "AI";
}

GUID preferences_page_myimpl::get_guid()
{
    // This is our GUID. Replace with your own when reusing the code.
    return GUID{0xb90d1b13, 0xcb44, 0x4dbb, {0xb3, 0xce, 0x60, 0x98, 0xae, 0x54, 0x7a, 0xe4}};
}

GUID preferences_page_myimpl::get_parent_guid()
{
    return guid_tools;
}

static preferences_page_factory_t<preferences_page_myimpl> g_preferences_page_myimpl_factory;
#endif // _WIN32
