//
// Created by PC on 10/02/2026.
//

#include "volume_resource.h"
#include "safe_main_thread_call.h"

volume_resource::volume_resource()
    : resource("volume://."),
      play_callback_impl_base(flag_on_volume_change)
{
}

mcp::json volume_resource::get_metadata() const
{
    return {
        {"uri", get_uri()},
        {"name", "Volume"},
        {
            "description",
            "Tracks the playback volume and mute state. "
            "Subscribe to this resource to get notified of volume changes. "
            "To control the volume, use the set_volume or toggle_mute tools."
        }
    };
}

mcp::json volume_resource::read() const
{
    auto [volume_db, is_muted, custom_mode] = safe_main_thread_call([]()
    {
        auto pc = playback_control::get();
        float volume = pc->get_volume();
        bool muted = pc->is_muted();

        // Check if custom volume mode is active (v3 API)
        bool custom_mode = false;
        auto pc_v3 = playback_control_v3::get();
        if (pc_v3.is_valid())
        {
            custom_mode = pc_v3->custom_volume_is_active();
        }

        return std::make_tuple(volume, muted, custom_mode);
    });

    mcp::json result = {
        {"uri", get_uri()},
        {"volume_db", volume_db},
        {"is_muted", is_muted}
    };

    // Format a human-readable text description
    std::string text;
    if (is_muted)
    {
        text = "Volume: Muted";
    }
    else if (custom_mode)
    {
        text = std::format("Volume: {} dB (custom mode)", volume_db);
    }
    else
    {
        text = std::format("Volume: {} dB", volume_db);
    }

    result["text"] = text;

    return result;
}

void volume_resource::on_volume_change(float p_new_val)
{
    notify_change();
}

void volume_resource::notify_change() const
{
    mcp::resource_manager::instance().notify_resource_changed(get_uri());
}

