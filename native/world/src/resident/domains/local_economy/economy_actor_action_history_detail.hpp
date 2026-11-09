// 同一无持久状态借读辅助；只核源身份与次序，不提供身体/费用/权限许可。
#pragma once
#include "economy_actor_action_history.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident::economy_actor_history_detail {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
[[noreturn]] inline void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
inline int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
inline void registration(const ActionTimeRegistration &requested,
                         const ActionTimeRegistration &actual) {
  if (requested.action_kind != actual.action_kind ||
      requested.owner_domain != actual.owner_domain ||
      requested.profile_id != actual.profile_id ||
      requested.profile_version != actual.profile_version ||
      requested.source_schema != actual.source_schema ||
      requested.mode != actual.mode ||
      requested.remaining_meaning != actual.remaining_meaning ||
      requested.shared_gate_kind != actual.shared_gate_kind ||
      requested.retained_wait_phases != actual.retained_wait_phases)
    fail("EconomicActorHistoryRegistrationMismatch");
}
inline const FactRecord &header(const CommonStore &common, const StableId &id,
                                std::string_view domain,
                                std::string_view kind) {
  const auto *row = common.state().facts.find(id);
  if (!row || row->id != id || row->domain != domain || row->kind != kind)
    fail("EconomicActorHistoryOriginalApplicationRequired", id);
  return *row;
}
inline std::size_t order(const CommonStore &common, const FactRecord &actual) {
  if (common.state().facts.find(actual.id) != &actual)
    fail("EconomicActorHistoryBorrowedHeaderRequired", actual.id);
  const auto &ids = common.state().facts.order();
  for (std::size_t i = 0; i < ids.size(); ++i)
    if (ids[i] == actual.id)
      return i;
  fail("EconomicActorHistoryApplicationOrderRequired", actual.id);
}
// 只保借用指针到同步 sink 返回；同 M 无真实应用头留 null，由中央严格拒。
class FixedIntervals {
  const CommonStore &common_;
  const StableId &action_;
  const WorldMoment &now_;
  ActionTimeActorHistorySink &sink_;
  const WorldMoment *opened_, *last_;
  const FactRecord *opened_header_, *last_header_;
  bool terminal_ = false;
  void boundary(const WorldMoment &at, const FactRecord *application) {
    if (compare(at, *last_) < 0 || compare(at, now_) > 0)
      fail("EconomicActorHistoryMomentOrderMismatch", action_);
    if (application && last_header_ &&
        order(common_, *application) <= order(common_, *last_header_))
      fail("EconomicActorHistoryApplicationOrderMismatch", action_);
    last_ = &at;
    if (application)
      last_header_ = application;
  }

public:
  FixedIntervals(const CommonStore &common, const StableId &action,
                 const WorldMoment &started, const FactRecord &application,
                 const WorldMoment &now, ActionTimeActorHistorySink &sink)
      : common_(common), action_(action), now_(now), sink_(sink),
        opened_(&started), last_(&started), opened_header_(&application),
        last_header_(&application) {
    order(common_, application);
    if (action.empty() || compare(started, now) > 0)
      fail("EconomicActorHistoryActualStartRequired", action);
  }
  void pause(const WorldMoment &at, const FactRecord *application) {
    if (terminal_ || !opened_)
      fail("EconomicActorHistoryActualPauseRequired", action_);
    boundary(at, application);
    take(sink_.original_actor_occupancy(
        {action_, *opened_, opened_header_, &at, application}));
    opened_ = nullptr;
    opened_header_ = nullptr;
  }
  void resume(const WorldMoment &at, const FactRecord *application) {
    if (terminal_ || opened_)
      fail("EconomicActorHistoryActualResumeRequired", action_);
    boundary(at, application);
    opened_ = &at;
    opened_header_ = application;
  }
  void rebound(const WorldMoment &at, const FactRecord *application) {
    if (terminal_)
      fail("EconomicActorHistoryAfterTerminal", action_);
    boundary(at, application);
  }
  void end(const WorldMoment &at, const FactRecord *application) {
    if (terminal_)
      fail("EconomicActorHistoryDuplicateTerminal", action_);
    boundary(at, application);
    if (opened_)
      take(sink_.original_actor_occupancy(
          {action_, *opened_, opened_header_, &at, application}));
    opened_ = nullptr;
    opened_header_ = nullptr;
    terminal_ = true;
  }
  void finish() {
    if (opened_)
      take(sink_.original_actor_occupancy(
          {action_, *opened_, opened_header_, nullptr, nullptr}));
  }
  bool working() const noexcept { return opened_ != nullptr; }
  bool terminal() const noexcept { return terminal_; }
};
} // namespace hanmo::resident::economy_actor_history_detail
