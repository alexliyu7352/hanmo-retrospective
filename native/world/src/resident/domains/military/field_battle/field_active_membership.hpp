#pragma once
#include "resident/domains/travel/field_battle_exit_placement.hpp"
#include <type_traits>
namespace hanmo::resident {
struct FieldBattleWorldState;
// Shape only, used after the actual same-World source reader succeeds.
// Non-null pointers alone never establish a participation interval.
inline bool has_field_engagement_borrowed_entry(
    const FieldActorEngagementEntry &entry) noexcept {
  return std::visit(
      [](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, FieldActorDoubleContactEntry>)
          return value.installation && value.contact;
        else
          return value.contact && value.field_contact_head &&
                 value.entered_head && value.entered_at;
      },
      entry);
}
// Shape only after the complete original reader; a null pointer alternative
// never grants an adopted release. No casualty/status flag stands in for S1.
inline bool has_field_engagement_borrowed_release(
    const FieldActorEngagementRelease &release) noexcept {
  return std::visit(
      [](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::monostate>)
          return false;
        else
          return value != nullptr;
      },
      release);
}
// Borrowed same-world original S1 adoption. An absent release is meaningful
// only after the actual entry and original named membership have been proved.
class FieldBattleActiveMembershipRead {
public:
  virtual ~FieldBattleActiveMembershipRead() = default;
  virtual bool bound_to(const CommonStore &,
                        const FieldBattleWorldState &) const noexcept = 0;
  virtual Outcome<FieldActorEngagementRead>
  read_field_active_member_engagement(const StableId &battle_id,
                                      const StableId &actor_id) const = 0;
};
} // namespace hanmo::resident
