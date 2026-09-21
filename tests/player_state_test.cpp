#include "primebds/utils/database/user_db.h"
#include "primebds/utils/player_state_policy.h"
#include "primebds/utils/permission_snapshot.h"
#include <filesystem>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <ctime>

using namespace primebds;
static void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    auto directory = std::filesystem::temp_directory_path() / ("primebds-state-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    try {
        auto path = (directory / "users.db").string();
        {
            db::DatabaseManager legacy(path);
            legacy.execute("CREATE TABLE warnings (id INTEGER PRIMARY KEY AUTOINCREMENT, xuid TEXT, "
                "name TEXT, warn_reason TEXT, warn_time INTEGER, added_by TEXT)");
            legacy.execute("INSERT INTO warnings (xuid,name,warn_reason,warn_time,added_by) "
                "VALUES ('one','Member','Legacy reason',12345,'Moderator')");
        }
        {
            db::UserDB database(path);
            database.saveUser("one","uuid","Member",1,"os","device",1,"version");
            for (const auto *flag : {"enabled_ss","enabled_ms","enabled_as","enabled_sc","is_afk"})
                database.updateUser("one",flag,"1");
            database.updateUser("one","enabled_mt","0");
            std::map<std::string,bool> allowed;
            for (const auto *node : {"socialspy","modspy","altspy","staffchat","afk","msgtoggle"})
                allowed["primebds.command." + std::string(node)] = true;
            database.setUserRank("one","Admin");
            database.resetUnavailableSettings("one",allowed);
            auto user = database.getOnlineUser("one");
            check(user && user->enabled_ss && user->enabled_ms && user->enabled_as && user->enabled_sc &&
                user->is_afk && !user->enabled_mt, "Retained abilities preserve all preferences, including PM opt-out");
            allowed["primebds.command.socialspy"] = false;
            allowed["primebds.command.modspy"] = false;
            database.setUserRank("one","Moderator");
            database.resetUnavailableSettings("one",allowed);
            user = database.getOnlineUser("one");
            check(user && !user->enabled_ss && !user->enabled_ms && user->enabled_sc && !user->enabled_mt,
                "Only revoked abilities reset; cached data is refreshed");
            allowed["primebds.command.socialspy"] = true;
            allowed["primebds.command.modspy"] = true;
            database.setUserRank("one","Owner");
            database.resetUnavailableSettings("one",allowed);
            user = database.getOnlineUser("one");
            check(user && !user->enabled_ss && !user->enabled_ms, "Re-promotion cannot resurrect disabled spies");
            const auto old = database.getWarnings("one");
            check(old.size()==1 && old[0].warn_time==12345 && old[0].expires_at==-1,
                "Legacy warning migration preserves timestamp and marks unknown expiry");
            const auto expires = std::time(nullptr)+3600;
            database.addWarning("one","Member","Timed","Moderator",expires);
            database.addWarning("one","Member","Permanent","Moderator");
            auto warnings = database.getWarnings("one");
            check(warnings.size()==3 && warnings[1].expires_at==expires && warnings[2].expires_at==0,
                "New timed/permanent warning expirations are persisted correctly");
            check(database.getWarnings("different-member").empty(), "Warning histories are scoped to XUID");
        }
        {
            db::UserDB database(path);
            database.saveUser("offline","offline-uuid","Offline Player",1,"os","device",1,"version");
            database.setUserRank("offline","Admin");
            database.updateUser("offline","enabled_ss","1");
            database.updateUser("offline","enabled_ms","1");
            database.updateUser("offline","enabled_mt","0");
            check(database.getUserByName("OFFLINE PLAYER")->xuid == "offline", "Case-insensitive saved identity");
            database.assignRank("offline","Default");
            auto user=database.getUserByName("offline player");
            check(user && user->internal_rank == "Default" && user->enabled_ss && user->enabled_ms && !user->enabled_mt,
                "Offline demotion changes authority immediately but preserves preferences until reconnect");
            check(database.pendingStateReset("offline") != 0, "Reconnect reconciliation survives offline changes");
        }
        {
            db::UserDB database(path);
            database.assignRank("offline","Admin");
            database.assignRank("offline","Default");
            database.assignRank("offline","Admin");
            check(database.getOnlineUser("offline")->enabled_ss && database.getOnlineUser("offline")->enabled_ms,
                "Intermediate offline ranks cannot erase saved toggles, including across restart");
            database.saveUser("offline","offline-uuid","Renamed Player",1,"os","device",1,"version");
            check(database.getOnlineUser("offline")->internal_rank == "Admin" && database.pendingStateReset("offline"),
                "Reconnect save preserves the final rank and reconciliation flag");
        }
        {
            db::UserDB reopened(path);
            std::map<std::string,bool> finalPermissions{{"primebds.command.socialspy",true},
                {"primebds.command.modspy",true}, {"primebds.command.msgtoggle",true},
                {"primebds.command.gmc",true}, {"primebds.command.speed",true}};
            // Same preference reconciliation invoked after live permissions load.
            reopened.resetUnavailableSettings("offline",finalPermissions);
            auto user=reopened.getOnlineUser("offline");
            check(user->enabled_ss && user->enabled_ms && !user->enabled_mt,
                "Final permitted rank preserves original spies and PM opt-out");
            auto has=[&](const std::string &node){return finalPermissions[node];};
            check(utils::mayKeepGameMode(1,has), "Final creative grant preserves mode regardless of intermediate ranks");
            reopened.clearPendingStateReset("offline");
            check(!reopened.pendingStateReset("offline"), "Successful reconciliation acknowledged once");
            // A final demotion still revokes on reconnect, rather than revoking at assignment time.
            reopened.assignRank("offline","Moderator");
            check(reopened.getOnlineUser("offline")->enabled_ms, "Partial demotion deferred while offline");
            finalPermissions["primebds.command.modspy"]=false;
            reopened.resetUnavailableSettings("offline",finalPermissions);
            user=reopened.getOnlineUser("offline");
            check(user->enabled_ss && !user->enabled_ms, "Only settings absent from the final rank reset");
            reopened.assignRank("offline","Default");
            reopened.resetUnavailableSettings("offline",{});
            user=reopened.getOnlineUser("offline");
            check(!user->enabled_ss && user->enabled_mt, "Final Default resets spies and restores default PM preference");
            // A pre-.9 pending bit mask is now only a flag, never a list of forced revocations.
            reopened.execute("UPDATE users SET pending_state_reset = 127 WHERE xuid = ?", {"offline"});
            reopened.assignRank("offline","Owner");
            check(reopened.pendingStateReset("offline") == 1, "Latest assignment replaces legacy accumulated revocation masks");
            check(!reopened.getOnlineUser("one")->enabled_ss, "Previously applied online revocations remain applied");
            check(reopened.getWarnings("one").size()==3, "Warnings survive repeated startup");
            check(!reopened.getUserByName("Never Joined"), "Unknown players are not fabricated");
        }
        std::map<std::string,bool> grouped{{"primebds.command",true}, {"primebds.command.speed",false},
            {"primebds.minecraft.op",false}, {"minecraft.command",true}, {"minecraft.command.op",false}};
        utils::applyPluginPermissionGroups(grouped);
        check(grouped["primebds.command.speed"], "Plugin command group resolves consistently for saved and live state");
        check(!grouped["primebds.minecraft.op"] && !grouped["minecraft.command.op"], "Command groups never manufacture native operator authority");
        grouped["primebds.minecraft.op"]=true;
        grouped["primebds.command"]=false;
        utils::applyPluginPermissionGroups(grouped);
        check(grouped["primebds.minecraft.op"] && !grouped["primebds.command.speed"], "Explicit native OP marker is independent of plugin command groups");
        std::map<std::string,bool> permissions;
        auto has=[&](const std::string &node){return permissions[node];};
        check(utils::mayKeepGameMode(0,has), "Survival is always retained");
        check(!utils::mayKeepGameMode(1,has) && !utils::mayKeepGameMode(2,has) &&
              !utils::mayKeepGameMode(3,has), "Unpermitted gameplay modes reset");
        permissions["primebds.command.gmc"]=true;
        check(utils::mayKeepGameMode(1,has) && !utils::mayKeepGameMode(3,has), "Creative capability does not grant spectator");
        permissions["primebds.command.spectate"]=true;
        check(utils::mayKeepGameMode(3,has), "Spectate capability preserves spectator mode");
        permissions["minecraft.command.gamemode"]=true;
        check(utils::mayKeepGameMode(1,has) && utils::mayKeepGameMode(3,has), "Native gamemode grant preserves modes");
        std::filesystem::remove_all(directory);
        std::cout << "Permission-dependent settings, gameplay modes and warning migration tests passed.\n";
    } catch (const std::exception &error) {
        std::filesystem::remove_all(directory);
        std::cerr << error.what() << '\n'; return 1;
    }
}
