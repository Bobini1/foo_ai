//
// Created by PC on 06/02/2026.
//

#ifndef FOOBAR_AI_PLAYLIST_RESOURCE_H
#define FOOBAR_AI_PLAYLIST_RESOURCE_H

#include <mcp_resource.h>
#include <SDK/foobar2000.h>

class playlist_resource : public mcp::resource, playlist_callback_impl_base
{
    void on_playlist_activate(t_size p_old, t_size p_new) override;
    void on_playlist_created(t_size p_index, const char* p_name, t_size p_name_len) override;
    void on_playlists_removed(const ::bit_array& p_mask, t_size p_old_count, t_size p_new_count) override;
    void on_playlist_renamed(t_size p_index, const char* p_new_name, t_size p_new_name_len) override;
    void on_playlists_reorder(const t_size* p_order, t_size p_count) override;
    void on_items_reordered(t_size p_playlist, const t_size* p_order, t_size p_count) override;
    void on_items_removing(t_size p_playlist, const bit_array& p_mask, t_size p_old_count, t_size p_new_count) override;
    void on_items_removed(t_size p_playlist, const bit_array& p_mask, t_size p_old_count, t_size p_new_count) override;
    void on_items_modified(t_size p_playlist, const bit_array& p_mask) override;
    void on_items_replaced(t_size p_playlist, const bit_array& p_mask,
        const pfc::list_base_const_t<t_on_items_replaced_entry>& p_data) override;

    void notify_changed() const;

public:

    explicit playlist_resource();
    mcp::json get_metadata() const override;
    mcp::json read() const override;
    std::vector<std::chrono::steady_clock::time_point> playlist_update_times;
    std::vector<std::string> playlist_ids;
    std::optional<t_size> get_playlist_index(const std::string& playlist_id) const;
};


#endif //FOOBAR_AI_PLAYLIST_RESOURCE_H