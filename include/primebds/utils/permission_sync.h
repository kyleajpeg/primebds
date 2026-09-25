#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace primebds::utils {

enum class PermissionSyncOrigin { Live, Join };

inline constexpr std::uint64_t PermissionRepairDelayTicks = 20;

inline bool shouldSchedulePermissionRepair(PermissionSyncOrigin origin, bool force_reconcile,
                                           bool reconciled) {
    return origin == PermissionSyncOrigin::Join && !force_reconcile && reconciled;
}

// Server-thread login identities. A UUID can refer to a different player session
// by the time a scheduled callback runs; generation also invalidates disable/re-enable work.
class PermissionSyncSessions {
public:
    std::uint64_t start(const std::string &uuid) {
        const auto id = ++sequence_;
        sessions_[uuid] = id;
        return id;
    }

    std::uint64_t session(const std::string &uuid) const {
        const auto found = sessions_.find(uuid);
        return found == sessions_.end() ? 0 : found->second;
    }

    bool active(const std::string &uuid, std::uint64_t id) const {
        return id != 0 && session(uuid) == id;
    }

    void end(const std::string &uuid) { sessions_.erase(uuid); }

    void clear() {
        sessions_.clear();
        ++generation_;
    }

    std::uint64_t generation() const { return generation_; }

private:
    std::uint64_t sequence_ = 0;
    std::uint64_t generation_ = 1;
    std::unordered_map<std::string, std::uint64_t> sessions_;
};

struct PermissionRepairTask {
    std::uint64_t session;
    std::uint64_t generation;
    std::uint64_t request;
    std::uint32_t task_id = 0;
};

// Server-thread ownership: old callbacks cannot erase newer work for the same UUID,
// including a replacement task within the same session.
class PermissionRepairTasks {
public:
    std::uint64_t nextRequestId() { return ++request_sequence_; }

    const PermissionRepairTask *find(const std::string &uuid) const {
        const auto it = tasks_.find(uuid);
        return it == tasks_.end() ? nullptr : &it->second;
    }

    bool track(const std::string &uuid, const PermissionRepairTask &task) {
        return tasks_.emplace(uuid, task).second;
    }

    std::optional<PermissionRepairTask> take(const std::string &uuid) {
        const auto it = tasks_.find(uuid);
        if (it == tasks_.end()) return std::nullopt;
        const auto task = it->second;
        tasks_.erase(it);
        return task;
    }

    std::optional<PermissionRepairTask> take(const std::string &uuid, std::uint64_t session,
                                            std::uint64_t generation, std::uint64_t request) {
        const auto *task = find(uuid);
        if (!task || task->session != session || task->generation != generation ||
            task->request != request) return std::nullopt;
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
    std::uint64_t request_sequence_ = 0;
    std::unordered_map<std::string, PermissionRepairTask> tasks_;
};

} // namespace primebds::utils
