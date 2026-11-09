#include "field_control_acceptance.hpp"
#include "field_session.hpp"
#include "resident/kernel/original_world_read_boundary.hpp"
#include "resident/storage/common_store.hpp"
#include <tuple>
namespace hanmo::resident {
namespace {

void fresh_prefix(CommonActualFactPrefix &out,
                  const CommonActualFactPrefix &in) {
  out.common_fact_count = in.common_fact_count;
  if (in.last_common_fact)
    out.last_common_fact =
        SourceRef{checkpoint_fresh_string_copy(in.last_common_fact->domain),
                  checkpoint_fresh_string_copy(in.last_common_fact->kind),
                  checkpoint_fresh_string_copy(in.last_common_fact->id)};
}
} // namespace
FieldControlAcceptanceBoundary::FieldControlAcceptanceBoundary(
    const FieldControlAcceptanceBoundary &other) {


  copied_fields = CheckpointCopyLease::reserve();
  fresh_prefix(common_prefix, other.common_prefix);
  last_local_event_sequence = other.last_local_event_sequence;
  observed_at = other.observed_at;
}
FieldControlAcceptanceBoundary &FieldControlAcceptanceBoundary::operator=(
    const FieldControlAcceptanceBoundary &other) {
  if (this != &other) {
    FieldControlAcceptanceBoundary fresh(other);
    *this = std::move(fresh);
  }
  return *this;
}
FieldControlAcceptanceBoundary &FieldControlAcceptanceBoundary::operator=(
    FieldControlAcceptanceBoundary &&other) {
  if (this != &other) {
    copied_fields =
        CheckpointCopyLease::hold_both(copied_fields, other.copied_fields);
    common_prefix = std::move(other.common_prefix);
    last_local_event_sequence = other.last_local_event_sequence;
    observed_at = std::move(other.observed_at);
    copied_fields = std::move(other.copied_fields);
  }
  return *this;
}
Outcome<FieldControlAcceptanceBoundary> capture_field_control_acceptance(
    const CommonStore &c, const FieldBattleSession &s,
    const CurrentMomentRead &clock, const OriginalWorldReadBoundary &proof,
    bool committed_command) {
  if (!proof.bound_to(c, clock))
    return {false,
            {},
            common_failure("FieldControlAcceptanceWorldBoundaryRequired",
                           s.battle_id)
                .error};
  auto observed = committed_command ? proof.read_committed_moment()
                                    : proof.read_observed_moment();
  if (!observed.ok)
    return {false, {}, observed.error};
  auto current = read_current_common_fact_prefix(c);
  if (!current.ok)
    return {false, {}, current.error};
  if (s.next_event_sequence <= 0 ||
      s.events.size() != static_cast<std::size_t>(s.next_event_sequence - 1))
    return {
        false,
        {},
        common_failure("FieldControlAcceptanceEventPrefixRequired", s.battle_id)
            .error};
  Integer sequence = 1;
  for (const auto &e : s.events)
    if (e.sequence != sequence++ ||
        std::tie(e.at.tick, e.at.t) > std::tie(s.now.tick, s.now.t))
      return {false,
              {},
              common_failure("FieldControlAcceptanceEventPrefixRequired",
                             s.battle_id)
                  .error};
  FieldControlAcceptanceBoundary out;


  out.copied_fields = CheckpointCopyLease::reserve();
  out.common_prefix.common_fact_count = current.value.common_fact_count;
  if (const auto *h = current.value.last_common_fact)
    out.common_prefix.last_common_fact =
        SourceRef{checkpoint_fresh_string_copy(h->domain),
                  checkpoint_fresh_string_copy(h->kind),
                  checkpoint_fresh_string_copy(h->id)};
  out.last_local_event_sequence = s.next_event_sequence - 1;
  out.observed_at = std::move(observed.value);
  return {true, std::move(out), {}};
}
Status
validate_field_control_acceptance(const CommonStore &c,
                                  const FieldBattleSession &s,
                                  const FieldControlAcceptanceBoundary &cut) {
  if (cut.observed_at.half_day_tick < 0)
    return common_failure("FieldControlAcceptanceMomentRequired", s.battle_id);
  auto canonical = compare_world_moments(cut.observed_at, cut.observed_at);
  if (!canonical.ok)
    return {false, {}, canonical.error};
  auto prefix = read_actual_common_fact_prefix(c, cut.common_prefix);
  if (!prefix.ok)
    return {false, {}, prefix.error};
  if (cut.last_local_event_sequence < 0 ||
      cut.last_local_event_sequence >= s.next_event_sequence ||
      static_cast<std::size_t>(cut.last_local_event_sequence) > s.events.size())
    return common_failure("FieldControlAcceptanceEventPrefixRequired",
                          s.battle_id);
  Integer n = 1;
  for (const auto &e : s.events) {
    if (n > cut.last_local_event_sequence)
      break;
    if (e.sequence != n++)
      return common_failure("FieldControlAcceptanceEventPrefixRequired",
                            s.battle_id);
  }
  return common_success();
}
} // namespace hanmo::resident
