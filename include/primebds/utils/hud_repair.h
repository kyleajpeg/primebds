#pragma once

#include "primebds/utils/hud_diagnostic.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace primebds::utils {

inline constexpr std::uint64_t HudRepairDelayTicks = 20;

inline SyncOrigin hudEffectiveSyncOrigin(SyncOrigin origin, bool force_reconcile) {
    return force_reconcile ? SyncOrigin::Live : origin;
}

inline bool shouldScheduleHudRepair(SyncOrigin origin, bool force_reconcile, int executed) {
    return origin == SyncOrigin::Join && !force_reconcile && executed > 0;
}

struct HudRepairTask {
    std::uint64_t session;
    std::uint64_t generation;
    std::uint64_t parent_sync;
    std::uint64_t queued_ms;
    int task_id = -1;
};

// Server-thread bookkeeping only. A stale callback must not remove the repair
// belonging to a later login with the same UUID.
class HudRepairTasks {
public:
    const HudRepairTask *find(const std::string &uuid) const {
        const auto it = tasks_.find(uuid);
        return it == tasks_.end() ? nullptr : &it->second;
    }

    bool track(const std::string &uuid, const HudRepairTask &task) {
        return tasks_.emplace(uuid, task).second;
    }

    std::optional<HudRepairTask> take(const std::string &uuid) {
        const auto it = tasks_.find(uuid);
        if (it == tasks_.end()) return std::nullopt;
        const auto task = it->second;
        tasks_.erase(it);
        return task;
    }

    std::optional<HudRepairTask> take(const std::string &uuid, std::uint64_t session,
                                     std::uint64_t generation, std::uint64_t parent_sync) {
        const auto *task = find(uuid);
        if (!task || task->session != session || task->generation != generation ||
            task->parent_sync != parent_sync) return std::nullopt;
        return take(uuid);
    }

    std::vector<std::string> uuids() const {
        std::vector<std::string> result;
        result.reserve(tasks_.size());
        for (const auto &[uuid, task] : tasks_) result.push_back(uuid);
        return result;
    }

    bool empty() const { return tasks_.empty(); }

private:
    std::unordered_map<std::string, HudRepairTask> tasks_;
};

} // namespace primebds::utils
