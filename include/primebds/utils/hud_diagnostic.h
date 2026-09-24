#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace primebds::utils {

enum class SyncOrigin { Live, Join };
enum class HudTestMode { Baseline, Skip, Custom };
enum class HudTestResult { Denied, Usage, Status, Changed };
enum class HudComponent { Preferences, God, Tasks, GameMode, Flying, AllowFlight, WalkSpeed, FlySpeed, NameTag };
enum class HudStepResult { Executed, Skipped, NotEligible };
using HudMask = std::uint16_t;
inline constexpr HudMask HudAllComponents = 511;
inline constexpr std::array<HudComponent, 9> HudComponents = {
    HudComponent::Preferences, HudComponent::God, HudComponent::Tasks,
    HudComponent::GameMode, HudComponent::Flying, HudComponent::AllowFlight,
    HudComponent::WalkSpeed, HudComponent::FlySpeed, HudComponent::NameTag
};

inline constexpr HudMask hudComponentMask(HudComponent component) {
    return static_cast<HudMask>(1u << static_cast<unsigned>(component));
}
inline const char *hudComponentName(HudComponent component) {
    switch (component) {
        case HudComponent::Preferences: return "preferences";
        case HudComponent::God: return "god";
        case HudComponent::Tasks: return "tasks";
        case HudComponent::GameMode: return "gamemode";
        case HudComponent::Flying: return "flying";
        case HudComponent::AllowFlight: return "allowflight";
        case HudComponent::WalkSpeed: return "walkspeed";
        case HudComponent::FlySpeed: return "flyspeed";
        case HudComponent::NameTag: return "nametag";
    }
    return "unknown";
}
inline std::string hudSelectionList(HudMask selection, bool enabled = true) {
    std::string list;
    for (const auto component : HudComponents) {
        if (((selection & hudComponentMask(component)) != 0) == enabled) {
            if (!list.empty()) list += ",";
            list += hudComponentName(component);
        }
    }
    return list.empty() ? "none" : list;
}
inline HudMask hudComponentSelection(const std::string &name) {
    if (name == "speeds")
        return hudComponentMask(HudComponent::WalkSpeed) | hudComponentMask(HudComponent::FlySpeed);
    if (name == "flight")
        return hudComponentMask(HudComponent::Flying) | hudComponentMask(HudComponent::AllowFlight);
    for (const auto component : HudComponents)
        if (name == hudComponentName(component)) return hudComponentMask(component);
    return 0;
}

inline const char *hudOriginName(SyncOrigin origin) {
    return origin == SyncOrigin::Join ? "join" : "live";
}
inline const char *hudModeName(HudTestMode mode) {
    switch (mode) {
        case HudTestMode::Baseline: return "baseline";
        case HudTestMode::Skip: return "skip";
        case HudTestMode::Custom: return "custom";
    }
    return "unknown";
}
inline HudTestMode hudSelectionMode(HudMask selection) {
    return selection == HudAllComponents ? HudTestMode::Baseline
         : selection == 0 ? HudTestMode::Skip : HudTestMode::Custom;
}
inline const char *hudStepResultName(HudStepResult result) {
    switch (result) {
        case HudStepResult::Executed: return "executed";
        case HudStepResult::Skipped: return "diagnostically-skipped";
        case HudStepResult::NotEligible: return "not-eligible";
    }
    return "unknown";
}
inline std::vector<std::string> hudTestUsages() {
    return {
        "/hudtest (baseline|skip|status)<action: hudtest_mode>",
        "/hudtest (enable|disable)<action: hudtest_toggle> "
        "(preferences|god|tasks|gamemode|flying|allowflight|walkspeed|flyspeed|nametag|speeds|flight)<component: hudtest_component>"
    };
}

struct HudSyncContext {
    std::uint64_t id;
    SyncOrigin origin;
    HudTestMode mode;
    HudMask selection;

    bool allows(HudComponent component) const {
        return origin == SyncOrigin::Live || (selection & hudComponentMask(component)) != 0;
    }

    bool skipsReconciliation() const {
        return origin == SyncOrigin::Join && selection == 0;
    }
};

// Diagnostic state is deliberately session-only. A restart restores baseline.
// A sync captures its complete selection once; later commands cannot alter it.
class HudTestState {
public:
    HudTestMode mode() const { return hudSelectionMode(selection_); }
    HudMask selectedMask() const { return selection_; }

    HudTestResult apply(bool is_console, const std::vector<std::string> &args) {
        if (!is_console) return HudTestResult::Denied;
        if (args.size() == 1) {
            if (args[0] == "status") return HudTestResult::Status;
            if (args[0] == "baseline") selection_ = HudAllComponents;
            else if (args[0] == "skip") selection_ = 0;
            else return HudTestResult::Usage;
            return HudTestResult::Changed;
        }
        if (args.size() != 2 || (args[0] != "enable" && args[0] != "disable"))
            return HudTestResult::Usage;
        const auto mask = hudComponentSelection(args[1]);
        if (mask == 0) return HudTestResult::Usage;
        if (args[0] == "enable") selection_ = static_cast<HudMask>(selection_ | mask);
        else selection_ = static_cast<HudMask>(selection_ & ~mask);
        return HudTestResult::Changed;
    }

    HudSyncContext beginSync(SyncOrigin origin) { return {++sequence_, origin, mode(), selection_}; }

private:
    HudMask selection_ = HudAllComponents;
    std::uint64_t sequence_ = 0;
};

template <typename Apply>
HudStepResult runHudStep(const HudSyncContext &context, HudComponent component,
                         bool eligible, Apply &&apply) {
    if (!eligible) return HudStepResult::NotEligible;
    if (!context.allows(component)) return HudStepResult::Skipped;
    std::forward<Apply>(apply)();
    return HudStepResult::Executed;
}

template <typename Apply, typename Skipped>
void runHudReconciliation(const HudSyncContext &context, Apply &&apply, Skipped &&skipped) {
    if (context.skipsReconciliation()) std::forward<Skipped>(skipped)();
    else std::forward<Apply>(apply)();
}

} // namespace primebds::utils
