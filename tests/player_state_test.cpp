#include "primebds/utils/database/user_db.h"
#include "primebds/utils/player_state_policy.h"
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
            database.setUserRank("offline","Owner");
            database.updateUser("offline","enabled_ss","1");
            database.updateUser("offline","enabled_ms","1");
            check(database.getUserByName("OFFLINE PLAYER")->xuid == "offline", "Offline name lookup ignores case and preserves XUID");
            std::map<std::string,bool> retained;
            for (const auto *node : {"gmc","gma","gmsp","fly","speed","nickname","god","socialspy"})
                retained["primebds.command." + std::string(node)] = true;
            database.assignRank("offline","Admin",retained);
            auto user=database.getUserByName("offline player");
            check(user && user->internal_rank == "Admin" && user->enabled_ss && !user->enabled_ms,
                "Offline assignment updates cached identity and only revoked preferences");
            check(database.pendingStateReset("offline") == 0, "Retained gameplay permissions do not enqueue resets");
            database.assignRank("offline","Default",{});
            check(database.pendingStateReset("offline") & utils::SpeedReset, "Offline demotion queues speed reset");
            check(!database.getOnlineUser("offline")->enabled_ss, "Offline demotion clears spies immediately");
            const auto pending=database.pendingStateReset("offline");
            database.assignRank("offline","Owner",retained);
            check(database.pendingStateReset("offline") == pending && !database.getOnlineUser("offline")->enabled_ss,
                "Offline re-promotion cannot resurrect preferences or discard pending gameplay revocations");
            database.saveUser("offline","offline-uuid","Renamed Player",1,"os","device",1,"version");
            check(database.getOnlineUser("offline")->internal_rank == "Owner" && database.pendingStateReset("offline") == pending,
                "Returning-player save preserves offline rank changes and pending revocations");
            check(!database.getUserByName("Never Joined"), "Unknown players are not fabricated");
        }
        {
            db::UserDB reopened(path);
            const auto pending=reopened.pendingStateReset("offline");
            check((pending & utils::SpeedReset) && utils::modeWasRevoked(1,pending), "Offline gameplay revocations survive restart");
            check(!reopened.getOnlineUser("offline")->enabled_ss, "Offline spy revocation survives restart");
            reopened.clearPendingStateReset("offline");
            check(reopened.pendingStateReset("offline") == 0, "Acknowledged reconnect reset is consumed only once");
            check(!reopened.getOnlineUser("one")->enabled_ss, "Revocation survives restart");
            check(reopened.getWarnings("one").size()==3, "Warnings survive repeated migration/startup");
        }
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
