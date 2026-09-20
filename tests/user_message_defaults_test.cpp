#include "primebds/utils/database/user_db.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using primebds::db::DatabaseManager;
using primebds::db::UserDB;

static void check(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}

struct TestDirectory {
    fs::path path = fs::temp_directory_path() /
        ("primebds-message-defaults-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    TestDirectory() { check(fs::create_directory(path), "Create isolated test directory"); }
    ~TestDirectory() { std::error_code error; fs::remove_all(path, error); }
};

static void join(UserDB &db, const std::string &id) {
    db.saveUser(id, "test-uuid", "Player" + id, 25, "test-os", "test-device", 1, "test-version");
}

static int preference(UserDB &db, const std::string &id) {
    const auto user = db.getOnlineUser(id);
    check(user.has_value(), "Player exists: " + id);
    return user->enabled_mt;
}

static void exerciseDatabase(const fs::path &path, bool legacy) {
    {
        UserDB db(path.string());
        if (legacy) {
            check(preference(db, "old-disabled") == 0, "Upgrade preserves existing disabled preference");
            check(preference(db, "old-enabled") == 1, "Upgrade preserves existing enabled preference");
            join(db, "old-disabled");
            join(db, "old-enabled");
            check(preference(db, "old-disabled") == 0, "Legacy returning player stays disabled");
            check(preference(db, "old-enabled") == 1, "Legacy returning player stays enabled");
        }
        join(db, "new-disabled");
        check(preference(db, "new-disabled") == 1, "New player starts with messages enabled");
        db.updateUser("new-disabled", "enabled_mt", "0");
        join(db, "new-disabled");
        check(preference(db, "new-disabled") == 0, "Reconnect preserves opt-out");
        db.setUserRank("new-disabled", "Builder");
        check(preference(db, "new-disabled") == 0, "Rank change preserves opt-out");
        join(db, "new-enabled");
        check(preference(db, "new-enabled") == 1, "Another new player starts enabled");
    }
    {
        UserDB db(path.string());
        join(db, "new-disabled");
        join(db, "new-enabled");
        check(preference(db, "new-disabled") == 0, "Restart preserves disabled preference");
        check(preference(db, "new-enabled") == 1, "Restart preserves enabled preference");
        join(db, "after-restart");
        check(preference(db, "after-restart") == 1, "New arrivals after restart still start enabled");
    }
}

int main() {
    try {
        TestDirectory directory;
        const auto fresh_path = directory.path / "fresh.db";
        std::string users_schema;
        {
            UserDB fresh(fresh_path.string());
            const auto row = fresh.queryRow("SELECT sql FROM sqlite_master WHERE type='table' AND name='users'");
            check(row.has_value(), "Read users schema");
            users_schema = row->at("sql");
        }
        const std::string new_default = "enabled_mt INTEGER DEFAULT 1";
        const auto pos = users_schema.find(new_default);
        check(pos != std::string::npos, "Fresh schema defaults messages to enabled");
        users_schema.replace(pos, new_default.size(), "enabled_mt INTEGER DEFAULT 0");

        const auto legacy_path = directory.path / "legacy.db";
        {
            DatabaseManager legacy(legacy_path.string());
            legacy.execute(users_schema);
            legacy.execute("INSERT INTO users (xuid, name) VALUES ('old-disabled', 'OldDisabled')");
            legacy.execute("INSERT INTO users (xuid, name, enabled_mt) VALUES ('old-enabled', 'OldEnabled', 1)");
        }
        exerciseDatabase(fresh_path, false);
        exerciseDatabase(legacy_path, true);
        std::cout << "New-player message defaults and saved-preference tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
