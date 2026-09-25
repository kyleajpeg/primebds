#include <endstone/command/command_sender_wrapper.h>
#include "primebds/plugin.h"
#include "primebds/commands/command_registry.h"
#include "primebds/utils/config/config_manager.h"
#include "primebds/utils/database/user_db.h"
#include "primebds/utils/database/session_db.h"
#include "primebds/utils/database/server_db.h"
#include "primebds/utils/permissions/permission_manager.h"
#include "primebds/utils/logging.h"
#include "primebds/utils/command_audit.h"
#include "primebds/utils/hierarchy.h"
#include "primebds/utils/player_state_policy.h"
#include "primebds/utils/permission_snapshot.h"

#include "primebds/commands/command_metadata.h"

#include <algorithm>
#include <ctime>
#include <exception>
#include <filesystem>
#include <set>

namespace primebds {

    void PrimeBDS::onLoad() {
        getLogger().info("PrimeBDS v{} loading...", getDescription().getVersion());

        // Ensure data directory exists
        std::filesystem::create_directories(getDataFolder());

        // Create subdirectories
        auto db_dir = getDataFolder() / "database";
        auto profiles_dir = getDataFolder() / "allowlist_profiles";
        std::filesystem::create_directories(db_dir);
        std::filesystem::create_directories(profiles_dir);

        // Point ConfigManager at Endstone's data folder
        config::ConfigManager::setDataFolder(getDataFolder().string());

        // Initialise databases
        db = std::make_unique<db::UserDB>((db_dir / "users.db").string());
        sldb = std::make_unique<db::SessionDB>((db_dir / "sessions.db").string());
        serverdb = std::make_unique<db::ServerDB>((db_dir / "server.db").string());

        // Load configuration
        config::ConfigManager::instance().load();

        last_shutdown_time = (int64_t)std::time(nullptr);
    }

    void PrimeBDS::onEnable() {
        getLogger().info("PrimeBDS v{} enabled.", getDescription().getVersion());

        // Register event listener
        listener_ = std::make_unique<EventListener>(*this);
        registerEvent(&EventListener::onPlayerLogin, *listener_);
        registerEvent(&EventListener::onPlayerJoin, *listener_);
        registerEvent(&EventListener::onPlayerQuit, *listener_);
        registerEvent(&EventListener::onPlayerKick, *listener_);
        registerEvent(&EventListener::onPlayerChat, *listener_);
        // Observe submissions before our normal-priority permission/remap handler.
        // Receive cancelled events too; an attempt is still worth recording.
        registerEvent(&EventListener::onPlayerCommandAudit, *listener_, endstone::EventPriority::Lowest, false);
        registerEvent(&EventListener::onPlayerCommand, *listener_);
        registerEvent(&EventListener::onServerCommand, *listener_);
        registerEvent(&EventListener::onPlayerDeath, *listener_);
        registerEvent(&EventListener::onEntityDamage, *listener_);
        registerEvent(&EventListener::onEntityKnockback, *listener_);
        registerEvent(&EventListener::onPlayerGameModeChange, *listener_);
        registerEvent(&EventListener::onPlayerInteractActor, *listener_);
        registerEvent(&EventListener::onPlayerTeleport, *listener_);
        registerEvent(&EventListener::onPlayerBedEnter, *listener_);
        registerEvent(&EventListener::onPlayerEmote, *listener_);
        registerEvent(&EventListener::onPlayerSkinChange, *listener_);
        registerEvent(&EventListener::onLeavesDecay, *listener_);
        registerEvent(&EventListener::onItemPickup, *listener_);
        registerEvent(&EventListener::onServerLoad, *listener_);

        // Commands are declared in the ENDSTONE_PLUGIN macro body below;
        // onCommand() dispatches to our CommandRegistry handlers.

        // Generate / sync commands.json
        config::ConfigManager::instance().loadCommandConfig();

        // Check for inactive sessions from unclean shutdown
        checkForInactiveSessions();
        // Main-thread maintenance: no retained player pointers, no async game access.
        auto task = getServer().getScheduler().runTaskTimer(*this, [this, tick = 0u]() mutable {
            const bool feed = (++tick % 10u) == 0;
            if (isgod.empty()) return;
            for (auto *player : getServer().getOnlinePlayers())
                if (player) maintainGodMode(*player, feed);
        }, 1, 1);
        if (task) god_maintenance_task_ = task->getTaskId();
        else getLogger().error("Could not start god-mode health/hunger maintenance.");
    }

    void PrimeBDS::onDisable() {
        getLogger().info("PrimeBDS v{} disabled.", getDescription().getVersion());

        permission_sessions.clear();
        for (const auto &uuid : permission_repairs_.uuids()) cancelPermissionRepair(uuid);
        if (god_maintenance_task_ >= 0) {
            getServer().getScheduler().cancelTask(god_maintenance_task_);
            god_maintenance_task_ = -1;
        }
        // End all active sessions
        for (auto *player : getServer().getOnlinePlayers()) {
            sldb->endSession(player->getXuid());
            // Save logout position
            auto loc = player->getLocation();
            std::string pos_str = std::to_string(loc.getX()) + "," +
                                  std::to_string(loc.getY()) + "," +
                                  std::to_string(loc.getZ()) + "," +
                                  player->getDimension().getName();
            db->updateUser(player->getXuid(), "last_logout_pos", pos_str);
        }

        // Save config
        config::ConfigManager::instance().save();

        last_shutdown_time = (int64_t)std::time(nullptr);
    }

    // ---------------------------------------------------------------------------
    // Command dispatch
    // ---------------------------------------------------------------------------

    bool PrimeBDS::onCommand(endstone::CommandSender &sender,
                             const endstone::Command &command,
                             const std::vector<std::string> &args) {
        if (auto *player = sender.asPlayer(); player && permissions_pending.contains(player->getXuid())) {
            sender.sendMessage("Your permissions are still loading. Please try again shortly."); return true;
        }
        // Defense in depth for callers reaching the executor directly.
        if (!command.testPermission(sender))
            return true;
        auto &registry = CommandRegistry::instance();
        auto *reg = registry.find(command.getName());
        if (reg) {
            if (!config::ConfigManager::instance().isCommandEnabled(command.getName())) {
                sender.sendMessage("\u00a7cThis command is disabled.");
                return false;
            }
            return reg->handler(*this, sender, args);
        }
        sender.sendMessage("\u00a7cUnknown command.");
        return false;
    }

    void PrimeBDS::maintainGodMode(endstone::Player &player, bool refill_hunger) {
        utils::maintainGodVitals(isgod, player, [&]() {
            // Wrapper suppresses recurring success feedback without changing global gamerules.
            bool error = false;
            endstone::CommandSenderWrapper quiet(getServer().getCommandSender(), {},
                [&](const endstone::Message &) { error = true; });
            const bool success = getServer().dispatchCommand(quiet, utils::feedCommand(player.getName()));
            if (!success || error) {
                if (god_feed_failures_.insert(player.getUniqueId().str()).second)
                    getLogger().error("God-mode hunger refill failed for {}; check effect command availability.", player.getName());
            } else god_feed_failures_.erase(player.getUniqueId().str());
        }, refill_hunger);
    }

    void PrimeBDS::reconcilePlayerState(endstone::Player &player) {
        const auto has = [&](const std::string &node) { return player.hasPermission("primebds.command." + node); };
        std::map<std::string, bool> preferences;
        for (const auto *node : {"msgtoggle", "socialspy", "modspy", "altspy", "staffchat", "afk"})
            preferences["primebds.command." + std::string(node)] = has(node);
        db->resetUnavailableSettings(player.getXuid(), preferences);
        if (!has("god") && !has("god.other")) isgod.set(player, false);
        if (!has("afk")) afk_cache.erase(player.getXuid());
        for (auto *intervals : {&monitor_intervals, &blockscan_intervals}) {
            const auto node = intervals == &monitor_intervals ? "monitor" : "blockscan";
            if (has(node)) continue;
            auto found = intervals->find(player.getName());
            if (found != intervals->end()) {
                getServer().getScheduler().cancelTask(found->second);
                intervals->erase(found);
            }
        }
        const auto mode = player.getGameMode();
        const bool allowed_flight = player.getAllowFlight();
        const bool was_flying = player.isFlying();
        const auto walk_speed = player.getWalkSpeed();
        const auto fly_speed = player.getFlySpeed();
        if (!utils::mayKeepGameMode(static_cast<int>(mode), [&](const std::string &node) {
                return player.hasPermission(node);
            })) player.setGameMode(endstone::GameMode::Survival);
        const auto current_mode = player.getGameMode();
        if (!has("fly") && current_mode != endstone::GameMode::Creative &&
            current_mode != endstone::GameMode::Spectator) {
            player.setFlying(false);
            player.setAllowFlight(false);
        } else if (has("fly") && mode != current_mode) {
            player.setAllowFlight(allowed_flight);
            player.setFlying(allowed_flight && was_flying);
        }
        player.setWalkSpeed(has("speed") ? walk_speed : utils::NormalWalkSpeed);
        player.setFlySpeed(has("speed") ? fly_speed : utils::NormalFlySpeed);
        if (!has("nickname") && !has("nickname.other")) player.setNameTag(player.getName());
    }

    std::map<std::string, bool> PrimeBDS::savedPermissions(const std::string &xuid, const std::string &rank) {
        auto &pm = permissions::PermissionManager::instance();
        auto rank_permissions = pm.getRankPermissions(rank);
        auto user_permissions = db->getPermissions(xuid);
        auto &managed_perms = pm.MANAGED_PERMISSIONS_LIST;

        // Linked permission groups — if any in the group are true, all become true
        static const std::vector<std::vector<std::string>> linked_groups = {
            {"primebds.command.permban", "endstone.command.ban"},
            {"primebds.command.ipban", "endstone.command.banip"},
            {"primebds.command.removeban", "endstone.command.unban", "endstone.command.unbanip"},
            {"primebds.command.filterlist", "endstone.command.banlist"}};

        // Build final permission map: start all managed perms as false
        std::map<std::string, bool> final_permissions;
        for (auto &p : managed_perms)
            final_permissions[p] = false;

        // Layer rank permissions
        for (auto &[perm, allowed] : rank_permissions)
            final_permissions[perm] = allowed;

        // Layer user-specific overrides (highest priority)
        for (auto &[perm, allowed] : user_permissions)
            final_permissions[perm] = allowed;

        // Apply linked groups
        for (auto &group : linked_groups) {
            bool seen_true = false;
            bool seen_false = false;
            for (auto &perm : group) {
                auto it = final_permissions.find(perm);
                if (it != final_permissions.end()) {
                    if (it->second)
                        seen_true = true;
                    else
                        seen_false = true;
                }
            }
            if (seen_true || seen_false) {
                bool group_value = seen_true;
                for (auto &perm : group)
                    final_permissions[perm] = group_value;
            }
        }

        utils::applyPluginPermissionGroups(final_permissions);
        return final_permissions;
    }

    bool PrimeBDS::reloadCustomPerms(endstone::Player &player, utils::PermissionSyncOrigin origin,
                                     bool force_reconcile) {
        auto &pm = permissions::PermissionManager::instance();
        auto user = db->getOnlineUser(player.getXuid());
        if (!user) {
            // Player not in DB yet — save them first
            db->saveUser(player.getXuid(), player.getUniqueId().str(),
                         player.getName(), static_cast<int>(player.getPing().count()),
                         player.getDeviceOS(), player.getDeviceId(),
                         static_cast<int64_t>(player.getRuntimeId()),
                         player.getGameVersion());
            user = db->getOnlineUser(player.getXuid());
            if (!user)
                return false;
        }

        std::string internal_rank = pm.checkRankExists(*this, player, user->internal_rank);
        auto final_permissions = savedPermissions(player.getXuid(), internal_rank);

        {
            std::set<endstone::PermissionAttachment *> to_remove;
            for (auto *info : player.getEffectivePermissions()) {
                if (!info)
                    continue;
                if (info->getPermission() == "primebdsoverride") {
                    if (auto *att = info->getAttachment())
                        to_remove.insert(att);
                }
            }
            for (auto *att : to_remove)
                att->remove();
        }

        // Create new attachment and apply all permissions
        auto *attachment = player.addAttachment(*this, "primebdsoverride", true);
        if (!attachment)
            return false;

        for (const auto &[perm, value] : final_permissions) {
            if (perm == "minecraft" || perm == "minecraft.command" || perm == "endstone" || perm == "endstone.command") continue;
            attachment->setPermission(perm, value);
        }

        {
            auto op_it = final_permissions.find("primebds.minecraft.op");
            bool wants_op = op_it != final_permissions.end() && op_it->second;
            if (player.isValid() && wants_op != player.isOp()) player.setOp(wants_op);

        }

        player.updateCommands();
        player.recalculatePermissions();
        bool reconciled = false;
        if (hierarchy::lower(internal_rank) != hierarchy::lower(user->internal_rank)) {
            reconcilePlayerState(player);
            reconciled = true;
        }
        pm.clearPrefixSuffixCache();
        pm.invalidatePermCache(player.getXuid());
        const int pending = db->pendingStateReset(player.getXuid());
        if (pending) {
            reconcilePlayerState(player);
            reconciled = true;
            db->clearPendingStateReset(player.getXuid());
        }
        if (force_reconcile && !reconciled) {
            reconcilePlayerState(player);
            reconciled = true;
        }
        // Deliver only after permissions and gameplay state successfully synchronize.
        // The stored baseline survives offline changes/restarts; restoration is silent.
        if (!force_reconcile) {
            if (const auto rank = db->pendingRankNotice(player.getXuid()))
                player.sendMessage("§aYour rank is now §e" + *rank + "§r");
        }
        db->clearPendingRankNotice(player.getXuid());
        permissions_pending.erase(player.getXuid());
        if (utils::shouldSchedulePermissionRepair(origin, force_reconcile, reconciled))
            schedulePermissionRepair(player);
        return true;
    }

    void PrimeBDS::cancelPermissionRepair(const std::string &uuid) {
        const auto task = permission_repairs_.take(uuid);
        if (task) getServer().getScheduler().cancelTask(task->task_id);
    }

    void PrimeBDS::schedulePermissionRepair(endstone::Player &player) {
        const auto uuid = player.getUniqueId();
        const auto key = uuid.str();
        const auto xuid = player.getXuid();
        const auto session = permission_sessions.session(key);
        const auto generation = permission_sessions.generation();
        if (!session || !player.isValid()) return;
        if (const auto *existing = permission_repairs_.find(key)) {
            if (existing->session == session && existing->generation == generation) return;
            cancelPermissionRepair(key);
        }
        const auto request = permission_repairs_.nextRequestId();
        try {
            auto task = getServer().getScheduler().runTaskLater(*this,
                [this, uuid, key, xuid, session, generation, request]() {
                    // Check session identity before resolving the UUID to a player.
                    if (permission_sessions.generation() != generation ||
                        !permission_sessions.active(key, session)) {
                        permission_repairs_.take(key, session, generation, request);
                        return;
                    }
                    const auto *tracked = permission_repairs_.find(key);
                    if (!tracked || tracked->session != session || tracked->generation != generation ||
                        tracked->request != request) return;
                    auto *p = getServer().getPlayer(uuid);
                    if (!p || !p->isValid() || p->getUniqueId().str() != key || p->getXuid() != xuid) {
                        permission_repairs_.take(key, session, generation, request);
                        return;
                    }
                    try {
                        // Reload the current saved rank; never restore a scheduling-time snapshot.
                        if (!reloadCustomPerms(*p, utils::PermissionSyncOrigin::Live, true))
                            getLogger().error("Delayed permission reconciliation failed for {}.", xuid);
                    } catch (const std::exception &error) {
                        getLogger().error("Delayed permission reconciliation failed for {}: {}", xuid, error.what());
                    } catch (...) {
                        getLogger().error("Delayed permission reconciliation failed for {}: unknown exception.", xuid);
                    }
                    permission_repairs_.take(key, session, generation, request);
                }, utils::PermissionRepairDelayTicks);
            if (!task) {
                getLogger().error("Could not schedule delayed permission reconciliation for {}.", xuid);
                return;
            }
            if (!permission_repairs_.track(key, {session, generation, request, task->getTaskId()})) {
                getServer().getScheduler().cancelTask(task->getTaskId());
                getLogger().error("Could not track delayed permission reconciliation for {}.", xuid);
            }
        } catch (const std::exception &error) {
            getLogger().error("Could not schedule delayed permission reconciliation for {}: {}", xuid, error.what());
        } catch (...) {
            getLogger().error("Could not schedule delayed permission reconciliation for {}: unknown exception.", xuid);
        }
    }

    void PrimeBDS::checkForInactiveSessions() {
        auto active = sldb->getActiveSessions();
        for (auto &session : active) {
            sldb->endSession(session["xuid"]);
        }
    }

    void EventListener::onPlayerDeath(endstone::PlayerDeathEvent &event) {
        handlers::handleDeathEvent(plugin_, event);
    }

    void EventListener::onPlayerTeleport(endstone::PlayerTeleportEvent &event) {
        handlers::handleTeleportEvent(plugin_, event);
    }

    void EventListener::onPlayerBedEnter(endstone::PlayerBedEnterEvent &event) {
        handlers::handleBedEnterEvent(plugin_, event);
    }

    void EventListener::onPlayerEmote(endstone::PlayerEmoteEvent &event) {
        handlers::handleEmoteEvent(plugin_, event);
    }

    void EventListener::onPlayerSkinChange(endstone::PlayerSkinChangeEvent &event) {
        handlers::handleSkinChangeEvent(plugin_, event);
    }

    void EventListener::onLeavesDecay(endstone::LeavesDecayEvent &event) {
        handlers::handleLeavesDecayEvent(plugin_, event);
    }

    void EventListener::onPlayerGameModeChange(endstone::PlayerGameModeChangeEvent &event) {
        handlers::handleGamemodeEvent(plugin_, event);
    }

    void EventListener::onPlayerInteractActor(endstone::PlayerInteractActorEvent &event) {
        handlers::handleInteractEvent(plugin_, event);
    }

    void EventListener::onItemPickup(endstone::PlayerPickupItemEvent &event) {
        handlers::handleItemPickupEvent(plugin_, event);
    }

    void EventListener::onEntityDamage(endstone::ActorDamageEvent &event) {
        handlers::combat::handleDamageEvent(plugin_, event);
    }

    void EventListener::onEntityKnockback(endstone::ActorKnockbackEvent &event) {
        handlers::combat::handleKnockbackEvent(plugin_, event);
    }

    void EventListener::onPlayerLogin(endstone::PlayerLoginEvent &event) {
        const auto uuid = event.getPlayer().getUniqueId().str();
        plugin_.cancelPermissionRepair(uuid);
        plugin_.permission_sessions.start(uuid);
        handlers::connections::handleLoginEvent(plugin_, event);
        if (event.isCancelled()) {
            plugin_.cancelPermissionRepair(uuid);
            plugin_.permission_sessions.end(uuid);
        }
    }

    void EventListener::onPlayerJoin(endstone::PlayerJoinEvent &event) {
        handlers::connections::handleJoinEvent(plugin_, event);
    }

    void EventListener::onPlayerQuit(endstone::PlayerQuitEvent &event) {
        const auto uuid = event.getPlayer().getUniqueId().str();
        plugin_.permission_sessions.end(uuid);
        plugin_.cancelPermissionRepair(uuid);
        handlers::connections::handleLeaveEvent(plugin_, event);
    }

    void EventListener::onPlayerKick(endstone::PlayerKickEvent &event) {
        handlers::connections::handleKickEvent(plugin_, event);
    }

    void EventListener::onPlayerCommandAudit(endstone::PlayerCommandEvent &event) {
        plugin_.getLogger().info("{}", utils::formatCommandAttempt(event.getPlayer().getName(), event.getCommand()));
        hierarchy::commandSpy(plugin_, event.getPlayer(), event.getCommand());
    }

    void EventListener::onPlayerCommand(endstone::PlayerCommandEvent &event) {
        if (plugin_.permissions_pending.contains(event.getPlayer().getXuid())) {
            event.setCancelled(true);
            event.getPlayer().sendMessage("Your permissions are still loading. Please try again shortly.");
            return;
        }
        handlers::preprocesses::handleCommandPreprocess(plugin_, event);
    }

    void EventListener::onServerCommand(endstone::ServerCommandEvent &event) {
        handlers::preprocesses::handleServerCommandPreprocess(plugin_, event);
    }

    void EventListener::onPlayerChat(endstone::PlayerChatEvent &event) {
        handlers::handleChatEvent(plugin_, event);
    }

    void EventListener::onServerLoad(endstone::ServerLoadEvent &event) {
        handlers::handleServerLoadEvent(plugin_, event);
    }

} // namespace primebds

// ---------------------------------------------------------------------------
// Endstone plugin entry point
// ---------------------------------------------------------------------------

ENDSTONE_PLUGIN("primebds", "3.4.3-chromevale.16", primebds::PrimeBDS) {
    description = "An essentials plugin for diagnostics, stability, and quality of life on Minecraft Bedrock Edition.";
    authors = {"PrimeStrat"};

    // All command metadata is defined in command_metadata.cpp.
    // Update that file when adding, removing, or changing commands.
    primebds::registerEndstoneCommands(*this);
}
