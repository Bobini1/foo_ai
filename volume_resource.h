//
// Created by Bobini on 10/02/2026.
//

#ifndef FOOBAR_AI_VOLUME_RESOURCE_H
#define FOOBAR_AI_VOLUME_RESOURCE_H

#include <mcp_resource.h>
#include <SDK/foobar2000.h>

class volume_resource : public mcp::resource, play_callback_impl_base
{
public:
    volume_resource();
    mcp::json get_metadata() const override;
    mcp::json read() const override;

private:
    void on_volume_change(float p_new_val) override;
    void notify_change() const;
};

#endif //FOOBAR_AI_VOLUME_RESOURCE_H

