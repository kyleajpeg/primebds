/// @file config_defaults.cpp
/// Default configuration values.

#include "primebds/utils/config/config_defaults.h"

namespace primebds::config {

    nlohmann::json getDefaultModules() {
        return {
            {"afk", {{"enabled", true}, {"timeout", 300}, {"kick_afk", false}, {"kick_timeout", 600}, {"message", "\u00a77{player} is now AFK."}, {"return_message", "\u00a77{player} is no longer AFK."}}},
            {"back", {{"enabled", true}, {"delay", 0}, {"cooldown", 0}}},
            {"broadcast", {{"enabled", true}, {"prefix", "\u00a78[\u00a7cBroadcast\u00a78]\u00a7r "}, {"playsound", "note.pling"}}},
            {"better_chat", {{"enabled", true}, {"format", "\u00a78[\u00a7r{prefix}\u00a78] \u00a7r{name}\u00a78: \u00a7f{message}"}, {"chat_cooldown", 0}}},
            {"combat", {{"enabled", true}, {"hit_cooldown", 0.45}, {"custom_damage", {{"enabled", false}, {"base_damage", 1.0}, {"critical_multiplier", 1.5}, {"sprint_bonus", 0.5}}}, {"custom_knockback", {{"enabled", false}, {"base_knockback_x", 1.0}, {"base_knockback_y", 0.4}, {"sprint_multiplier", 1.5}}}, {"tag_overrides", {{"enabled", false}}}}},
            {"connections", {{"custom_join_message", {{"enabled", true}, {"message", "\u00a7a+ \u00a77{player}"}}}, {"custom_leave_message", {{"enabled", true}, {"message", "\u00a7c- \u00a77{player}"}}}, {"motd", {{"enabled", false}, {"message", "\u00a7eWelcome, {player}!"}}}, {"alt_detection", {{"enabled", true}}}}},
            {"crasher_patch", {{"enabled", true}}},
            {"discord_webhook", {{"command_logs", {{"enabled", false}, {"webhook", ""}}}, {"moderation_logs", {{"enabled", false}, {"webhook", ""}}}, {"chat_logs", {{"enabled", false}, {"webhook", ""}}}, {"connection_logs", {{"enabled", false}, {"webhook", ""}}}, {"embed_for_log", {{"enabled", false}, {"title", "Log"}, {"color", 0x3498db}}}}},
            {"gamerules", {{"bed_enabled", true}, {"emotes_enabled", true}, {"leaves_decay_enabled", true}, {"skin_change_enabled", true}}},
            {"homes", {{"enabled", true}, {"max_homes", 5}, {"teleport_delay", 0}, {"teleport_cooldown", 0}}},
            {"permissions_manager", {{"minecraft", true}, {"primebds", true}, {"endstone", true}, {"*", true}}},
            {"spawns", {{"enabled", true}, {"teleport_delay", 0}, {"teleport_cooldown", 0}}},
            {"warps", {{"enabled", true}, {"teleport_delay", 0}, {"teleport_cooldown", 0}}}};
    }

    nlohmann::json getDefaultPermissions() {
        return {
            {"Default", {{"permissions", {{"endstone.broadcast", true}, {"endstone.broadcast.user", true}, {"endstone.command.version", true}, {"endstone.command.plugins", true}, {"primebds.command.ping", true}, {"primebds.command.reply", true}, {"minecraft.command.list", true}, {"minecraft.command.tell", true}, {"minecraft.command.me", true}}}, {"inherits", nlohmann::json::array()}, {"weight", 0}}},
            {"Operator", {{"permissions", {{"*", true}}}, {"inherits", {"Default"}}, {"weight", 100}, {"prefix", "\u00a78[\u00a7cAdmin\u00a78] \u00a7c"}, {"suffix", "\u00a7r"}}}};
    }

    std::vector<std::string> getDefaultRules() {
        return {};
    }

} // namespace primebds::config
