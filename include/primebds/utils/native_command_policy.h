#pragma once

#include "primebds/utils/native_world_commands.h"
#include <set>
#include <string>

namespace primebds::hierarchy {

// Shared by native authorization and command ownership collision detection.
inline const std::set<std::string> ordinaryNativeCommands = {
    "help", "list", "me", "tell", "w", "whisper", "msg", "say",
    "version", "plugins", "status", "seed", "packstack", "banlist"};
inline const std::set<std::string> consoleOnlyNativeCommands = {
    "execute", "function", "schedule", "script", "scriptevent",
    "wsserver", "permission", "allowlist", "whitelist", "ban-ip", "banip", "unban-ip", "pardon-ip",
    "reload", "reloadconfig", "reloadpacketlimitconfig", "changesetting", "gametest"};
inline const std::set<std::string> ownerWorldNativeCommands = {
    "stop", "save", "difficulty", "gamerule", "toggledownfall", "daylock", "fill", "clone", "setblock", "structure", "place",
    "setworldspawn", "mobevent", "tickingarea", "setmaxplayers", "scoreboard"};
inline const std::set<std::string> firstTargetNativeCommands = {
    "kick", "ban", "pardon", "unban", "op", "deop",
    "give", "clear", "kill", "effect", "enchant", "title", "titleraw", "tellraw", "damage", "inputpermission",
    "camera", "hud", "fog", "playanimation", "stopsound", "spawnpoint", "clearspawnpoint", "tag", "transfer"};
inline const std::set<std::string> specialTargetNativeCommands = {
    "gamemode", "xp", "playsound", "teleport", "tp"};

/// Protect the labels that ChromeVale already treats as native or Endstone built-ins.
inline bool isProtectedNativeCommand(const std::string &name) {
    return ordinaryNativeCommands.contains(name) || consoleOnlyNativeCommands.contains(name) ||
           ownerWorldNativeCommands.contains(name) || firstTargetNativeCommands.contains(name) ||
           specialTargetNativeCommands.contains(name) || !delegatedWorldPermission(name).empty();
}

} // namespace primebds::hierarchy
