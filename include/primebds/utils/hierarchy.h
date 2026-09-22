#pragma once
#include "primebds/utils/hierarchy_policy.h"
#include <endstone/endstone.hpp>

namespace primebds { class PrimeBDS; }
namespace primebds::hierarchy {
Rank rankOf(const std::string &name);
Rank playerRank(PrimeBDS &plugin, const std::string &name);
bool isConsole(PrimeBDS &plugin, endstone::CommandSender &sender);
bool isAdministrator(PrimeBDS &plugin, endstone::CommandSender &sender);
std::optional<Rank> globalMuteAuthority(PrimeBDS &plugin);
bool isGloballyMuted(PrimeBDS &plugin, endstone::Player &player);
bool mayTarget(PrimeBDS &plugin, endstone::CommandSender &sender,
               const std::string &target, bool allow_self = true);
bool requireTarget(PrimeBDS &plugin, endstone::CommandSender &sender,
                   const std::string &target, bool allow_self = true);
bool mayObserve(PrimeBDS &plugin, endstone::Player &viewer,
                const std::vector<std::string> &participants);
bool authorizePluginCommand(PrimeBDS &plugin, endstone::CommandSender &sender,
                            const std::string &name, const std::vector<std::string> &args);
bool authorizeNativeCommand(PrimeBDS &plugin, endstone::Player &sender,
                            const std::string &name, const std::vector<std::string> &args);
void socialSpy(PrimeBDS &plugin, endstone::Player &sender, const std::string &target,
               const std::string &message);
void commandSpy(PrimeBDS &plugin, endstone::Player &sender, const std::string &command);
void moderationLog(PrimeBDS &plugin, endstone::CommandSender &sender,
                   const std::string &target, const std::string &message);
} // namespace primebds::hierarchy
