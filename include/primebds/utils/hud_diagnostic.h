#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace primebds::utils {

enum class SyncOrigin { Live, Join };
enum class HudTestMode { Baseline, Skip };
enum class HudTestResult { Denied, Usage, Status, Changed };

inline const char *hudOriginName(SyncOrigin origin) {
    return origin == SyncOrigin::Join ? "join" : "live";
}
inline const char *hudModeName(HudTestMode mode) {
    return mode == HudTestMode::Skip ? "skip" : "baseline";
}

struct HudSyncContext {
    std::uint64_t id;
    SyncOrigin origin;
    HudTestMode mode;

    bool skipsReconciliation() const {
        return origin == SyncOrigin::Join && mode == HudTestMode::Skip;
    }
};

// Diagnostic state is deliberately session-only. A restart restores baseline.
// A sync captures its mode once; a later mode change cannot alter that context.
class HudTestState {
public:
    HudTestMode mode() const { return mode_; }

    HudTestResult apply(bool is_console, const std::vector<std::string> &args) {
        if (!is_console) return HudTestResult::Denied;
        if (args.size() != 1) return HudTestResult::Usage;
        if (args[0] == "status") return HudTestResult::Status;
        if (args[0] == "baseline") mode_ = HudTestMode::Baseline;
        else if (args[0] == "skip") mode_ = HudTestMode::Skip;
        else return HudTestResult::Usage;
        return HudTestResult::Changed;
    }

    HudSyncContext beginSync(SyncOrigin origin) { return {++sequence_, origin, mode_}; }

private:
    HudTestMode mode_ = HudTestMode::Baseline;
    std::uint64_t sequence_ = 0;
};

template <typename Apply, typename Skipped>
void runHudReconciliation(const HudSyncContext &context, Apply &&apply, Skipped &&skipped) {
    if (context.skipsReconciliation()) std::forward<Skipped>(skipped)();
    else std::forward<Apply>(apply)();
}

} // namespace primebds::utils
