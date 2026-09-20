#include "primebds/utils/database/session_db.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
static void check(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
struct TestDirectory {
    fs::path path = fs::temp_directory_path() / ("primebds-activity-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TestDirectory() { check(fs::create_directory(path), "Create test directory"); }
    ~TestDirectory() { std::error_code error; fs::remove_all(path, error); }
};
int main() {
    try {
        TestDirectory directory;
        const auto path = directory.path / "sessions.db";
        {
            primebds::db::SessionDB db(path.string());
            check(db.getActivitySummary("highest", 1000).empty(), "Empty database");
            db.execute("INSERT INTO sessions (xuid,name,join_time,leave_time) VALUES "
                "('a','OldName',100,200), ('a','Alice',900,0), "
                "('b','Bob',500,800), ('c','Charlie',950,975), ('c','Charlie',990,980)");
            const auto highest = db.getActivitySummary("highest", 1000);
            check(highest.size() == 3, "One row per player");
            check(highest[0].at("xuid") == "b" && highest[0].at("total") == "300", "Highest playtime first");
            check(highest[1].at("name") == "Alice" && highest[1].at("total") == "200", "Latest name and live session included");
            check(highest[2].at("total") == "25", "Negative session duration clamped");
            const auto lowest = db.getActivitySummary("lowest", 1000);
            check(lowest[0].at("xuid") == "c", "Lowest first");
            const auto recent = db.getActivitySummary("recent", 1000);
            check(recent[0].at("xuid") == "c" && recent[1].at("xuid") == "a", "Most recent join first");
            check(db.getActivitySummary("highest", 1010)[1].at("total") == "210", "Active duration advances");
            bool rejected = false;
            try { db.getActivitySummary("total; DROP TABLE sessions", 1000); }
            catch (const std::invalid_argument &) { rejected = true; }
            check(rejected, "Only known sort orders allowed");
        }
        primebds::db::SessionDB reopened(path.string());
        check(reopened.getActivitySummary("highest", 1000).size() == 3, "Records preserved after reopen");
        std::cout << "Activity database regression tests passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
