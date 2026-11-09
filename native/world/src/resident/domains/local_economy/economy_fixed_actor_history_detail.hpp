// 经济 Fixed 原转换只借身份与原身体 cause；不重复 S0 工量或身体 reducer。
#pragma once
#include "economy_actor_action_history_detail.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include <limits>
namespace hanmo::resident::economy_actor_history_detail {
class FixedChain {
  const StableId &action_;
  std::string_view domain_, kind_, id_;
  Integer schema_;
  const FixedServiceTransition *previous_ = nullptr;

public:
  FixedChain(const StableId &action, std::string_view domain,
             std::string_view kind, std::string_view id, Integer schema)
      : action_(action), domain_(domain), kind_(kind), id_(id),
        schema_(schema) {}
  bool next(const FixedServiceTransition &t) {
    if (t.action_ref != action_ || t.source_schema != 1 ||
        t.transition_id.empty() || t.execution_source_schema != schema_ ||
        t.execution_source_ref.domain != domain_ ||
        t.execution_source_ref.kind != kind_ ||
        t.execution_source_ref.id != id_ ||
        (previous_ && (previous_->transition_ordinal ==
                           std::numeric_limits<Integer>::max() ||
                       previous_->kind == FixedTransitionKind::terminal)) ||
        t.transition_ordinal !=
            (previous_ ? previous_->transition_ordinal + 1 : 0) ||
        bool(t.previous_transition_ref) != bool(previous_) ||
        (previous_ && (*t.previous_transition_ref != previous_->transition_id ||
                       compare(t.occurred_at, previous_->occurred_at) < 0)))
      fail("EconomicActorHistoryActualFixedChainRequired", t.transition_id);
    const bool first = !previous_;
    previous_ = &t;
    return first;
  }
};
// 真实保存的原变化行证明此转换的 actor/M，不调用会回入 ActionTime 的身体全
// validator。 该 cause 不是 Action 取得/释放应用头，调用方仍必须传 null 给
// same-M sink。
inline void condition_change(const CharacterConditionSources &body,
                             const StableId &actor, const StableId &execution,
                             const FixedServiceTransition &t) {
  const auto *cause = std::get_if<FixedEligibilityChanged>(&t.cause);
  if (!cause || cause->original_owner_execution_ref != execution ||
      cause->original_cause_schema != 2 ||
      cause->original_cause_ref.domain != "actor_foundation" ||
      cause->original_cause_ref.kind != "condition_receipt" ||
      t.transition_id != cause->original_cause_ref.id ||
      cause->transition_rule_ref.empty())
    fail("EconomicActorHistoryOriginalConditionCauseRequired", t.transition_id);
  const auto *receipt =
      body.state.character_condition_extensions.applied_receipts.find(
          cause->original_cause_ref.id);
  if (!receipt || receipt->result_id != cause->original_cause_ref.id ||
      receipt->actor_id != actor ||
      receipt->effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
      compare(receipt->effective_at.exact_moment, t.occurred_at))
    fail("EconomicActorHistoryConditionMomentMismatch", t.transition_id);
  std::size_t actual = 0;
  for (const auto &input : t.input_basis_refs)
    if (const auto *applied = std::get_if<FixedAppliedInput>(&input))
      if (same_source_ref(applied->source_ref, cause->original_cause_ref)) {
        if (applied->source_schema != 2)
          fail("EconomicActorHistoryConditionSchemaMismatch", t.transition_id);
        ++actual;
      }
  if (actual != 1)
    fail("EconomicActorHistoryConditionInputRequired", t.transition_id);
}
inline void condition_interval(FixedIntervals &intervals,
                               const FixedServiceTransition &t) {
  switch (t.kind) {
  case FixedTransitionKind::suspended:
    intervals.pause(t.occurred_at, nullptr);
    break;
  case FixedTransitionKind::resumed:
    intervals.resume(t.occurred_at, nullptr);
    break;
  case FixedTransitionKind::input_rebound:
    intervals.rebound(t.occurred_at, nullptr);
    break;
  default:
    fail("EconomicActorHistoryUnknownConditionTransition", t.transition_id);
  }
}
} // namespace hanmo::resident::economy_actor_history_detail
