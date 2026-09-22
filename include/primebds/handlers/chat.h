/// @file chat.h
/// Chat event handler - mute system, staff chat, enhanced chat formatting.

#pragma once

#include <endstone/endstone.hpp>

namespace primebds {
    class PrimeBDS;
}

namespace primebds::handlers {

    void handleChatEvent(PrimeBDS &plugin, endstone::PlayerChatEvent &event);
    bool sendPublicChat(PrimeBDS &plugin, endstone::Player &player, const std::string &message);

} // namespace primebds::handlers
