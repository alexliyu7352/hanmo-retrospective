#include "character_condition_read.hpp"
#include "authored_origin.hpp"
#include "battle_injury_catalog_v1.hpp"
#include "battle_injury_episode_v1.hpp"
#include "character_condition_replay.hpp"
#include "character_origin_validation.hpp"
#include "condition_application_fact.hpp"
#include "condition_application_history.hpp"
#include "condition_capacity.hpp"
#include "craft_condition.hpp"
#include "created_origin.hpp"
#include "effective_attributes_v1.hpp"
#include "gather_condition_v1.hpp"
#include "injury_capability_map.hpp"
#include "injury_field_coverage_v1.hpp"
#include "injury_original_results_v1.hpp"
#include "injury_personal_coverage_v1.hpp"
#include "injury_progress_replay_v1.hpp"
#include "injury_service_coverage_v1.hpp"
#include "injury_source_links_v1.hpp"
#include "injury_sources_v1.hpp"
#include "long_rest_condition.hpp"
#include "military_availability.hpp"
#include "military_supply_condition.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/native_personal_battle_lifecycle.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "service_condition.hpp"
#include <array>
#include <cstdint>
#include <limits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
// 未闭合的原身体来源仍明确拒绝；只统一替身错误码，原缺项保存在 reason。
Status condition_not_implemented(const char *reason,
                                 const StableId &path = {}) {
  auto result = common_failure("NOT_IMPLEMENTED", path,
                               ErrorKind::unsupported_capability);
  result.error.reason = reason;
  return result;
}

[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> result) {
  if (!result.ok)
    throw Failure{std::move(result.error)};
  return std::move(result.value);
}
template <class T, class F> Outcome<T> guarded(F fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
const ActionTimeRegistration &
work_registration(const CharacterConditionSources &sources,
                  const WorkOrStudyConditionCause &cause) {
  if (!sources.growth || !sources.growth_catalog || !sources.shared ||
      !sources.elapsed || !sources.scale || !sources.action_registrations)
    fail("CompleteGrowthConditionSourcesRequired", cause.action_id);
  const auto *raw = sources.growth->facts.find(cause.action_id);
  if (!raw)
    fail("GrowthWorkConditionOriginalSourceRequired", cause.action_id);
  const auto *study = std::get_if<StudyFact>(raw);
  const auto *training = std::get_if<TrainingFact>(raw);
  const StableId kind =
      study      ? (study->terms.study_teaching_agreement ? "teach_study"
                                                          : "study_skill")
      : training ? training->action_kind
                 : StableId{};
  const ActionTimeRegistration *selected = nullptr;
  for (const auto &r : *sources.action_registrations)
    if (r.owner_domain == "growth" && r.action_kind == kind) {
      if (selected)
        fail("DuplicateGrowthConditionRegistration", kind);
      selected = &r;
    }
  if (!selected)
    fail("GrowthConditionRegistrationMissing", kind);
  return *selected;
}
void actual_source(const CharacterConditionSources &s,
                   const ConditionReceiptV2 &row,
                   const FrozenBodyScalars &before) {
  if (row.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
      take(compare_world_moments(row.effective_at.exact_moment, s.actual_now)) >
          0)
    fail("ConditionReceiptActualMomentInvalid", row.result_id);
  if (const auto *ref = std::get_if<CharacterInjurySourceRefV1>(&row.cause)) {
    if (!before.alive || !s.injury_sources)
      fail("CharacterInjuryActualOwningSourceRequired", row.result_id);
    const auto *application =
        s.injury_sources->applications.find(ref->result_id);
    const auto *current =
        s.state.character_condition_extensions.actor_condition_heads.find(
            row.actor_id);
    if (!application || !current || application->actor_id != row.actor_id ||
        ref->result_id != row.result_id)
      fail("CharacterInjuryActualApplicationRequired", row.result_id);
    // 反读本次之前的精确头，不以应用行自报before points证明自己。
    // Borrow and prove the unique predecessor before allocating its id copy.
    // The current head's id need not have the same length as this older id.
    const StableId *predecessor_id = nullptr;
    if (row.condition_revision_before)
      for (const auto &item :
           s.state.character_condition_extensions.applied_receipts.rows()) {
        if (item.second.actor_id == row.actor_id &&
            item.second.condition_revision_after ==
                row.condition_revision_before) {
          if (predecessor_id)
            fail("CharacterInjuryActualPrefixRequired", row.result_id);
          predecessor_id = &item.first;
        }
      }
    if (row.condition_revision_before && !predecessor_id)
      fail("CharacterInjuryActualPrefixRequired", row.result_id);


    auto prior_memory = CheckpointMemoryCharge(0);
    auto prior = *current;
    prior.condition_revision = row.condition_revision_before;
    prior.latest_receipt_ref.reset();
    if (predecessor_id)
      prior.latest_receipt_ref = ConditionReceiptRefV2{*predecessor_id};
    const CharacterInjuryReplaySourcesV1 original{
        s, *s.injury_sources, s.injury_personal, s.injury_field};
    const auto progress = take(read_character_injury_progress_at_head_v1(
        original, prior, row.effective_at.exact_moment));
    if (progress.injury != before.injury ||
        application->progress.before.injury != progress.injury ||
        application->progress.before.points != progress.points)
      fail("CharacterInjuryActualBeforeMismatch", row.result_id);
    if (std::holds_alternative<OriginalRestProvidedCause>(
            application->original_source))
      take(validate_rest_injury_original_result_v1(s, *s.injury_sources, row));
    else if (std::holds_alternative<CharacterTreatmentActionSourceV1>(
                 application->original_source))
      take(validate_treatment_injury_original_result_v1(s, *s.injury_sources,
                                                        row));
    else {
      // Real original body/source and explicit authored result are verified.
      // Full-world episode/current-effect coverage remains independently gated.
      take(validate_battle_injury_selected_result_v1(s, row));
    }
  } else if (std::holds_alternative<GatherFatigueConditionCauseV1>(row.cause)) {
    if (!before.alive)
      fail("GatherConditionBeforeActorDead", row.result_id);
    take(validate_gather_condition_source_v1(s, row));
  } else if (std::holds_alternative<TravelExertionConditionCause>(row.cause)) {
    if (!s.movement)
      fail("TravelConditionSourceRequired", row.result_id);
    take(validate_travel_exertion_condition_receipt(s.common.state(), row,
                                                    s.catalog, *s.movement));
  } else if (const auto *rest =
                 std::get_if<OriginalRestProvidedCause>(&row.cause)) {
    if (rest->rest_kind == "long_rest")
      take(validate_long_rest_condition_receipt(s, row, before));
    else {
      if (!s.growth || !s.rest_time)
        fail("RestConditionSourceRequired", row.result_id);
      take(validate_authored_rest_receipt(s.common.state(), row, s.catalog,
                                          *s.growth, *s.rest_time));
    }
  } else if (const auto *cause =
                 std::get_if<WorkOrStudyConditionCause>(&row.cause)) {
    if (cause->kind == WorkOrStudyConditionCause::Kind::service_completion)
      take(validate_original_work_service_condition(s, row));
    else if (cause->kind == WorkOrStudyConditionCause::Kind::craft_gate)
      take(validate_craft_gate_condition_receipt(s, row));
    else {
      const auto &reg = work_registration(s, *cause);
      take(validate_growth_work_condition_receipt(
          s.common, s.catalog, *s.growth, *s.growth_catalog, *s.shared,
          *s.elapsed, *s.scale, s.catalog.identity, reg, s.actual_now, row,
          s.interaction, s.interaction_catalog));
    }
  } else if (const auto *cause =
                 std::get_if<MilitaryCasualtyConditionCause>(&row.cause)) {
    if (cause->disposition ==
        MilitaryCasualtyConditionCause::Disposition::wounded) {
      if (!s.military || !s.military_exact)
        fail("MilitaryConditionSourcesRequired", row.result_id);
      take(validate_military_wound_condition(
          s.common, s.state, s.catalog, *s.military, *s.military_exact, row,
          s.actual_now, s.military_formations, s.military_returns,
          s.military_history_order));
    } else {
      if (!s.deaths || !s.life_sources)
        fail("LifeConditionSourceRequired", row.result_id);
      take(validate_life_termination_condition(
          s.common, s.state, *s.deaths, s.catalog, *s.life_sources, row,
          s.actual_now, s.military_history_order));
    }
  } else if (std::holds_alternative<ProvisionShortfallConditionCause>(
                 row.cause)) {
    if (!before.alive)
      fail("MilitarySupplyConditionAliveBeforeRequired", row.result_id);
    take(validate_military_supply_shortage_condition(s, row));
  } else if (std::holds_alternative<LifeTerminationConditionCause>(row.cause)) {
    if (!s.deaths || !s.life_sources)
      fail("LifeConditionSourceRequired", row.result_id);
    take(validate_life_termination_condition(
        s.common, s.state, *s.deaths, s.catalog, *s.life_sources, row,
        s.actual_now, s.military_history_order));
  } else
    fail("UnregisteredCharacterConditionCause", row.result_id);
}
bool same_key(const CharacterConditionBusinessKey &a,
              const CharacterConditionBusinessKey &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *x = std::get_if<GatherFatigueConditionBusinessKeyV1>(&a)) {
    const auto &y = std::get<GatherFatigueConditionBusinessKeyV1>(b);
    return x->receipt_id == y.receipt_id && x->actor_id == y.actor_id;
  }
  if (const auto *x = std::get_if<RestConditionBusinessKey>(&a)) {
    const auto &y = std::get<RestConditionBusinessKey>(b);
    return x->action_id == y.action_id && x->actor_id == y.actor_id &&
           x->completion_ordinal == y.completion_ordinal;
  }
  if (const auto *x = std::get_if<TravelConditionBusinessKey>(&a)) {
    const auto &y = std::get<TravelConditionBusinessKey>(b);
    return x->unit_ref == y.unit_ref && x->actor_id == y.actor_id &&
           x->exertion_source_id == y.exertion_source_id;
  }
  if (const auto *x = std::get_if<LifeTerminationConditionBusinessKey>(&a)) {
    const auto &y = std::get<LifeTerminationConditionBusinessKey>(b);
    return x->cause_tag == y.cause_tag && x->source_id == y.source_id &&
           x->actor_id == y.actor_id && x->source_ordinal == y.source_ordinal;
  }
  if (const auto *x = std::get_if<ProvisionShortfallConditionBusinessKey>(&a)) {
    const auto &y = std::get<ProvisionShortfallConditionBusinessKey>(b);
    return same_source_ref(x->original_supply_ref, y.original_supply_ref) &&
           x->source_schema == y.source_schema &&
           x->source_ordinal == y.source_ordinal && x->actor_id == y.actor_id &&
           x->purpose == y.purpose;
  }
  if (const auto *x = std::get_if<CharacterInjuryConditionBusinessKeyV1>(&a)) {
    const auto &y = std::get<CharacterInjuryConditionBusinessKeyV1>(b);
    // The referenced independent source row owns the underlying battle/action
    // once identity; this is only Condition's finite reference-key equality.
    return x->source_ref.result_id == y.source_ref.result_id &&
           x->actor_id == y.actor_id;
  }
  const auto &x = std::get<WorkOrStudyConditionBusinessKey>(a);
  const auto &y = std::get<WorkOrStudyConditionBusinessKey>(b);
  return x.action_id == y.action_id && x.actor_id == y.actor_id &&
         x.ordinal == y.ordinal;
}
bool same_head(const ActorConditionHead &a, const ActorConditionHead &b) {
  return a.actor_id == b.actor_id &&
         a.condition_revision == b.condition_revision &&
         a.initial_condition_seed_ref.seed_key ==
             b.initial_condition_seed_ref.seed_key &&
         a.latest_receipt_ref.has_value() == b.latest_receipt_ref.has_value() &&
         (!a.latest_receipt_ref ||
          a.latest_receipt_ref->result_id == b.latest_receipt_ref->result_id);
}
CharacterConditionAtHead initial(const CharacterConditionSources &s,
                                 const StableId &actor) {
  auto origin = take(read_character_initial_values_v2(
      s.common, s.state, s.catalog, s.family_catalog, s.shared, actor,
      s.actual_now, s.born_sources));
  CharacterConditionAtHead result;
  result.head = {actor, 0, origin.origin->initial_condition_seed_ref, {}};
  result.body = {origin.alive,
                 origin.condition->hp,
                 origin.condition->fatigue,
                 origin.condition->injury,
                 {"alive", "hp", "fatigue", "injury"}};
  result.observed_at = origin.origin->created_at.exact_moment;
  return result;
}
void apply(CharacterConditionAtHead &value, const ConditionReceiptV2 &receipt) {
  if (value.head.condition_revision == std::numeric_limits<Integer>::max() ||
      receipt.condition_revision_before != value.head.condition_revision ||
      receipt.condition_revision_after != value.head.condition_revision + 1 ||
      receipt.actor_id != value.head.actor_id ||
      receipt.scalar_changes.empty() ||
      take(compare_world_moments(value.observed_at,
                                 receipt.effective_at.exact_moment)) > 0)
    fail("CharacterConditionReceiptChainMismatch", receipt.result_id);
  std::array<bool, 4> changed{};
  for (const auto &delta : receipt.scalar_changes) {
    const auto n = static_cast<std::size_t>(delta.field);
    if (n >= changed.size() || changed[n])
      fail("DuplicateConditionScalar", receipt.result_id);
    changed[n] = true;
    Integer actual = 0;
    switch (delta.field) {
    case ConditionScalarChange::Field::hp:
      actual = value.body.hp;
      break;
    case ConditionScalarChange::Field::fatigue:
      actual = value.body.fatigue;
      break;
    case ConditionScalarChange::Field::injury:
      actual = value.body.injury;
      break;
    case ConditionScalarChange::Field::alive:
      actual = value.body.alive ? 1 : 0;
      break;
    }
    const Integer upper = delta.field == ConditionScalarChange::Field::alive ? 1
                          : delta.field == ConditionScalarChange::Field::injury
                              ? 3
                              : 100;
    if (actual != delta.before || delta.after < 0 || delta.after > upper)
      fail("CharacterConditionBeforeAfterMismatch", receipt.result_id);
    switch (delta.field) {
    case ConditionScalarChange::Field::hp:
      value.body.hp = delta.after;
      break;
    case ConditionScalarChange::Field::fatigue:
      value.body.fatigue = delta.after;
      break;
    case ConditionScalarChange::Field::injury:
      value.body.injury = delta.after;
      break;
    case ConditionScalarChange::Field::alive:
      value.body.alive = delta.after == 1;
      break;
    }
  }
  value.head.condition_revision = receipt.condition_revision_after;
  value.head.latest_receipt_ref = ConditionReceiptRefV2{receipt.result_id};
  value.observed_at = receipt.effective_at.exact_moment;
}
} // namespace
Status validate_character_condition_chain_structure(
    const CharacterConditionSources &s) {
  return guarded<Done>([&] {
    take(validate_complete_character_origin_sources(
        s.common, s.state, s.catalog, s.family_catalog, s.shared, s.actual_now,
        s.born_sources));
    take(compare_world_moments(s.actual_now, s.actual_now));
    take(validate_condition_application_fact_headers(s.common, s.state,
                                                     s.catalog));
    const auto source1 = take(condition_application_facts_selected(
        s.catalog.identity, ConditionApplicationHistoryOwner::injury_sources));
    if (source1) {
      if (!s.injury_sources || !s.injury_personal || !s.injury_field ||
          !s.injury_field_clock || !s.military)
        fail("CharacterInjuryCompleteWorldSourcesRequired");
      if (!s.injury_field_clock->bound_to(s.common, *s.military,
                                          *s.injury_field))
        fail("CharacterInjuryFieldClockBindingRequired");
      const auto actual = take(s.injury_field_clock->read_current_moment());
      if (take(compare_world_moments(actual, s.actual_now)) != 0)
        fail("CharacterInjuryFieldActualMomentMismatch");
      take(validate_character_injury_source_links_v1(
          s.common, s.state, s.catalog, *s.injury_sources));
      take(validate_injury_service_source_coverage_v1(s, *s.injury_sources));
    } else if (s.injury_sources || s.injury_personal || s.injury_field ||
               s.injury_field_clock)
      fail("CharacterInjuryUnselectedWorldSources");
    const auto &extensions = s.state.character_condition_extensions;
    if (extensions.actor_condition_heads.size() !=
            s.common.state().actors.size() ||
        s.state.character_origin_records.origins.size() !=
            s.common.state().actors.size())
      fail("CharacterConditionActorCoverageMismatch");
    const auto &receipts = extensions.applied_receipts.rows();
    for (std::size_t n = 0; n < receipts.size(); ++n) {
      const auto &row = receipts[n];
      if (row.first != row.second.result_id ||
          !s.common.actor(row.second.actor_id) ||
          row.second.effective_at.kind !=
              ActorFoundationAt::Kind::exact_moment ||
          take(compare_world_moments(row.second.effective_at.exact_moment,
                                     s.actual_now)) > 0)
        fail("CharacterConditionReceiptIdentityMismatch", row.first);
      for (std::size_t j = 0; j < n; ++j)
        if (same_key(receipts[j].second.business_key, row.second.business_key))
          fail("DuplicateCharacterConditionBusinessKey", row.first);
    }
    for (const auto &row : s.common.state().actors.rows()) {
      auto replay = initial(s, row.first);
      for (const auto &receipt : receipts)
        if (receipt.second.actor_id == row.first)
          apply(replay, receipt.second);
      const auto *head = extensions.actor_condition_heads.find(row.first);
      if (!head || !same_head(replay.head, *head) ||
          replay.body.alive != row.second.alive ||
          replay.body.hp != row.second.hp ||
          replay.body.fatigue != row.second.fatigue ||
          replay.body.injury != row.second.injury)
        fail("CharacterConditionCurrentMismatch", row.first);
    }
    return Done{};
  });
}
Outcome<CharacterConditionAtHead>
read_character_condition_chain_cut(const CharacterConditionSources &s,
                                   const ActorConditionHead &expected,
                                   const WorldMoment &at) {
  return guarded<CharacterConditionAtHead>([&] {
    take(validate_character_condition_chain_structure(s));
    take(compare_world_moments(at, at));
    if (expected.condition_revision < 0 ||
        take(compare_world_moments(at, s.actual_now)) > 0)
      fail("CharacterHistoricalConditionBoundaryInvalid", expected.actor_id);
    auto value = initial(s, expected.actor_id);
    for (const auto &row :
         s.state.character_condition_extensions.applied_receipts.rows()) {
      const auto &receipt = row.second;
      if (receipt.actor_id != expected.actor_id)
        continue;
      const auto order =
          take(compare_world_moments(receipt.effective_at.exact_moment, at));
      if (receipt.condition_revision_after > expected.condition_revision) {
        if (order < 0)
          fail("CharacterHistoricalConditionHeadStale", receipt.result_id);
        continue;
      }
      if (order > 0)
        fail("CharacterHistoricalConditionHeadFuture", receipt.result_id);
      apply(value, receipt);
    }
    if (!same_head(value.head, expected))
      fail("CharacterHistoricalConditionHeadMismatch", expected.actor_id);
    value.observed_at = at;
    return value;
  });
}
Outcome<CharacterConditionAtHead>
read_character_condition_verified_prefix(const CharacterConditionSources &s,
                                         const ActorConditionHead &expected,
                                         const WorldMoment &at) {
  return guarded<CharacterConditionAtHead>([&] {
    auto checked = take(read_character_condition_chain_cut(s, expected, at));
    auto replay = initial(s, expected.actor_id);
    for (const auto &entry :
         s.state.character_condition_extensions.applied_receipts.rows()) {
      const auto &receipt = entry.second;
      if (receipt.actor_id != expected.actor_id)
        continue;
      if (receipt.condition_revision_after > expected.condition_revision)
        break;
      actual_source(s, receipt, replay.body);
      apply(replay, receipt);
    }
    if (!same_head(replay.head, expected) ||
        replay.body.alive != checked.body.alive ||
        replay.body.hp != checked.body.hp ||
        replay.body.fatigue != checked.body.fatigue ||
        replay.body.injury != checked.body.injury)
      fail("CharacterConditionVerifiedPrefixMismatch", expected.actor_id);
    return checked;
  });
}
Outcome<CharacterConditionAppliedPrefix>
capture_character_condition_applied_prefix(const CharacterConditionSources &s) {
  return guarded<CharacterConditionAppliedPrefix>([&] {
    const auto &rows =
        s.state.character_condition_extensions.applied_receipts.rows();
    if (rows.size() >
        static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
      fail("CharacterConditionAppliedPrefixOverflow");
    CharacterConditionAppliedPrefix out;
    out.applied_receipt_count = static_cast<Integer>(rows.size());
    if (!rows.empty()) {
      const auto &last = rows.back();
      if (last.first.empty() || last.first != last.second.result_id)
        fail("CharacterConditionReceiptIdentityMismatch", last.first);
      out.last_applied_receipt.emplace(
          ConditionReceiptRefV2{last.second.result_id});
    }
    take(validate_character_condition_applied_prefix(s, out));
    return out;
  });
}
Status validate_character_condition_applied_prefix(
    const CharacterConditionSources &s,
    const CharacterConditionAppliedPrefix &prefix) {
  return guarded<Done>([&] {
    const auto &rows =
        s.state.character_condition_extensions.applied_receipts.rows();
    if (prefix.applied_receipt_count < 0 ||
        static_cast<std::uintmax_t>(prefix.applied_receipt_count) >
            static_cast<std::uintmax_t>(rows.size()) ||
        bool(prefix.applied_receipt_count) != bool(prefix.last_applied_receipt))
      fail("CharacterConditionAppliedPrefixInvalid");
    const auto count = static_cast<std::size_t>(prefix.applied_receipt_count);
    for (std::size_t i = 0; i < count; ++i)
      if (rows[i].first.empty() || rows[i].first != rows[i].second.result_id)
        fail("CharacterConditionReceiptIdentityMismatch", rows[i].first);
    if (count && prefix.last_applied_receipt->result_id !=
                     rows[count - 1].second.result_id)
      fail("CharacterConditionAppliedPrefixAnchorMismatch",
           prefix.last_applied_receipt->result_id);
    return Done{};
  });
}
Outcome<CharacterConditionAtHead> read_character_condition_at_applied_prefix(
    const CharacterConditionSources &s, const StableId &actor,
    const CharacterConditionAppliedPrefix &prefix, const WorldMoment &at) {
  return guarded<CharacterConditionAtHead>([&] {
    take(validate_character_condition_applied_prefix(s, prefix));
    auto expected = initial(s, actor);
    if (take(compare_world_moments(at, expected.observed_at)) < 0)
      fail("CharacterHistoricalConditionBoundaryBeforeOrigin", actor);
    const auto &rows =
        s.state.character_condition_extensions.applied_receipts.rows();
    for (std::size_t i = 0;
         i < static_cast<std::size_t>(prefix.applied_receipt_count); ++i) {
      const auto &receipt = rows[i].second;
      if (receipt.actor_id != actor)
        continue;
      if (expected.head.condition_revision ==
              std::numeric_limits<Integer>::max() ||
          receipt.condition_revision_before !=
              expected.head.condition_revision ||
          receipt.condition_revision_after !=
              expected.head.condition_revision + 1)
        fail("CharacterConditionReceiptChainMismatch", receipt.result_id);
      expected.head.condition_revision = receipt.condition_revision_after;
      expected.head.latest_receipt_ref =
          ConditionReceiptRefV2{receipt.result_id};
    }
    // Selection is not validation: preserve all original origin, stale/future,
    // scalar and actual-cause gates, including the zero-receipt branch.
    return take(read_character_condition_verified_prefix(s, expected.head, at));
  });
}
// 校验同一真实身体链及原来源；效果与伤情应用未闭合时不接受候选。
Status
validate_complete_character_conditions(const CharacterConditionSources &s) {
  return guarded<Done>([&] {
    take(validate_character_condition_chain_structure(s));
    take(validate_gather_condition_coverage_v1(s));
    take(compare_world_moments(s.actual_now, s.actual_now));
    if (s.actual_now.half_day_tick < s.common.tick())
      fail("CharacterConditionClockBeforeWorld");
    const auto &extensions = s.state.character_condition_extensions;
    if (battle_injury_source1_selected(s.catalog.identity)) {
      // A real finite installation, never an empty-provider escape. The
      // owning central factory must install the same-World proof only with
      // the complete original effect readers/consumers. Do not call the
      // composer here: Work/Growth source proofs may read Condition prefixes.
      if (!s.attribute_absence ||
          !s.attribute_absence->bound_to(s.common, s.state, s.catalog))
        fail("CompleteConditionSameWorldAttributeSourcesRequired");
      take(s.attribute_absence->validate_finite_attribute_source_absence(
          s.actual_now));
      take(validate_battle_injury_catalog_v1(s.catalog));
      take(validate_injury_capability_map(s.catalog));
      if (!extensions.recovery_resolutions.empty() ||
          !extensions.combat_return_bindings.empty() ||
          !extensions.instant_limits.empty())
        throw Failure{condition_not_implemented(
                          "CompleteOtherConditionEffectSourcesRequired")
                          .error};
      take(validate_battle_injury_episode_sources_v1(s));
      if (!s.military || !s.personal_catalog)
        fail("CompleteBattleInjuryOriginalRootsRequired");
      // Full publication validation enumerates actual producer results, not
      // merely the applications that happened to be written. Keep these out
      // of the verified-prefix/capture path during an atomic candidate write.
      take(validate_field_injury_source_coverage_v1(
          s, *s.injury_sources, *s.military, *s.injury_field));
      const auto personal_lifecycle =
          take(selected_native_personal_battle_lifecycle(s.catalog.identity));
      take(validate_personal_injury_source_coverage_v1(
          s, *s.injury_sources, *s.injury_personal, *s.personal_catalog,
          personal_lifecycle));
      for (const auto &entry : extensions.applied_receipts.rows()) {
        const auto &selected = entry.second.selected_injury_result;
        if (selected.kind == SelectedInjuryResult::Kind::applied) {
          // Reverse episode/effect coverage above and the immutable authored
          // selection are both required; no arbitrary selected payload passes.
          take(validate_battle_injury_selected_result_v1(s, entry.second));
        } else if (selected.actual_decision_source_ref ||
                   !selected.effect_items.empty())
          fail("InactiveConditionInjuryEffects", entry.first);
      }
    } else {
      if (!extensions.episodes.empty() ||
          !extensions.effect_contributions.empty() ||
          !extensions.recovery_resolutions.empty() ||
          !extensions.combat_return_bindings.empty() ||
          !extensions.instant_limits.empty())
        throw Failure{
            condition_not_implemented("CompleteConditionEffectReadersRequired")
                .error};
      for (const auto &entry : extensions.applied_receipts.rows()) {
        const auto &selected = entry.second.selected_injury_result;
        if (selected.kind == SelectedInjuryResult::Kind::applied)
          throw Failure{
              condition_not_implemented(
                  "OriginalInjuryEpisodeApplicationRequired", entry.first)
                  .error};
        if (selected.actual_decision_source_ref ||
            !selected.effect_items.empty())
          fail("InactiveConditionInjuryEffects", entry.first);
      }
    }
    if (extensions.actor_condition_heads.size() !=
            s.common.state().actors.size() ||
        s.state.character_origin_records.origins.size() !=
            s.common.state().actors.size())
      fail("CharacterConditionActorCoverageMismatch");
    const auto &receipts = extensions.applied_receipts.rows();
    for (std::size_t i = 0; i < receipts.size(); ++i) {
      const auto &row = receipts[i];
      if (row.first != row.second.result_id ||
          !s.common.actor(row.second.actor_id))
        fail("CharacterConditionReceiptIdentityMismatch", row.first);
      for (std::size_t j = 0; j < i; ++j)
        if (same_key(receipts[j].second.business_key, row.second.business_key))
          fail("DuplicateCharacterConditionBusinessKey", row.first);
    }
    for (const auto &row : s.common.state().actors.rows()) {
      auto replay = initial(s, row.first);
      for (const auto &receipt : receipts)
        if (receipt.second.actor_id == row.first) {
          actual_source(s, receipt.second, replay.body);
          apply(replay, receipt.second);
        }
      const auto *head = extensions.actor_condition_heads.find(row.first);
      const auto &actor = row.second;
      if (!head || !same_head(replay.head, *head) ||
          replay.body.alive != actor.alive || replay.body.hp != actor.hp ||
          replay.body.fatigue != actor.fatigue ||
          replay.body.injury != actor.injury)
        fail("CharacterConditionCurrentMismatch", row.first);
    }
    take(validate_military_availability_coverage(s));
    take(validate_condition_purpose_capacity_coverage(s));
    take(validate_craft_gate_condition_coverage(s));
    take(validate_military_supply_shortage_coverage(s));
    if (s.military_exact) {
      if (!s.military)
        fail("MilitaryConditionSourcesRequired");
      take(validate_military_wound_coverage(
          s.common, s.state, s.catalog, *s.military, *s.military_exact,
          s.actual_now, s.military_formations, s.military_returns,
          s.military_history_order));
    }
    if (s.movement)
      take(validate_travel_exertion_condition_coverage(
          s.common.state(), s.state, s.catalog, *s.movement));
    if (s.deaths) {
      if (!s.life_sources)
        fail("LifeConditionSourceRequired");
      take(validate_life_termination_coverage(
          s.common, s.state, *s.deaths, s.catalog, *s.life_sources,
          s.actual_now, s.military_history_order));
    }
    if (s.growth) {
      take(validate_original_work_service_condition_coverage(s));
      bool long_rest = false;
      for (const auto &row : s.growth->facts.rows())
        if (std::holds_alternative<RestFact>(row.second))
          long_rest = true;
      if (long_rest)
        take(validate_long_rest_condition_coverage(s));
      // source1已由链结构入口逐完成表反验唯一typed应用；不能再要求旧直接cause。
      const bool injury_service_owner =
          take(condition_application_facts_selected(
              s.catalog.identity,
              ConditionApplicationHistoryOwner::injury_sources));
      if (!injury_service_owner) {
        for (const auto &row : s.growth->life_activities.rows()) {
          const auto &actual = row.second.view;
          if (actual.kind != LifeActivityKind::service ||
              actual.activity != "rest" ||
              actual.phase != LifeActivityPhase::completed)
            continue;
          std::size_t count = 0;
          for (const auto &receipt : receipts)
            if (const auto *cause = std::get_if<OriginalRestProvidedCause>(
                    &receipt.second.cause);
                cause && cause->action_id == row.first &&
                receipt.second.actor_id == actual.actor_id)
              ++count;
          if (count != 1)
            fail("RestConditionSourceCoverageMismatch", row.first);
        }
      }
      bool work = false;
      for (const auto &row : s.growth->facts.rows())
        if (std::holds_alternative<StudyFact>(row.second) ||
            std::holds_alternative<TrainingFact>(row.second))
          work = true;
      if (work) {
        if (!s.growth_catalog || !s.shared || !s.elapsed || !s.scale ||
            !s.action_registrations)
          fail("CompleteGrowthConditionSourcesRequired");
        take(validate_growth_work_condition_coverage(
            s.common, s.state, s.catalog, *s.growth, *s.growth_catalog,
            *s.shared, *s.elapsed, *s.scale, s.catalog.identity,
            *s.action_registrations, s.actual_now));
      }
    }
    return Done{};
  });
}
Outcome<CharacterConditionAtHead>
read_character_condition_at_head(const CharacterConditionSources &s,
                                 const ActorConditionHead &expected,
                                 const WorldMoment &actual_boundary) {
  return guarded<CharacterConditionAtHead>([&] {
    take(validate_complete_character_conditions(s));
    take(compare_world_moments(actual_boundary, actual_boundary));
    if (expected.condition_revision < 0 ||
        take(compare_world_moments(actual_boundary, s.actual_now)) > 0)
      fail("CharacterHistoricalConditionBoundaryInvalid", expected.actor_id);
    auto value = initial(s, expected.actor_id);
    for (const auto &row :
         s.state.character_condition_extensions.applied_receipts.rows()) {
      const auto &receipt = row.second;
      if (receipt.actor_id != expected.actor_id)
        continue;
      const auto relation = take(compare_world_moments(
          receipt.effective_at.exact_moment, actual_boundary));
      if (receipt.condition_revision_after > expected.condition_revision) {
        if (relation < 0)
          fail("CharacterHistoricalConditionHeadStale", receipt.result_id);
        continue;
      }
      if (relation > 0)
        fail("CharacterHistoricalConditionHeadFuture", receipt.result_id);
      apply(value, receipt);
    }
    if (!same_head(value.head, expected))
      fail("CharacterHistoricalConditionHeadMismatch", expected.actor_id);
    value.observed_at = actual_boundary;
    return value;
  });
}
Outcome<CharacterConditionAtHead>
read_character_condition_at_moment(const CharacterConditionSources &s,
                                   const StableId &actor,
                                   const WorldMoment &boundary) {
  return guarded<CharacterConditionAtHead>([&] {
    take(validate_complete_character_conditions(s));
    take(compare_world_moments(boundary, boundary));
    if (take(compare_world_moments(boundary, s.actual_now)) > 0)
      fail("CharacterHistoricalConditionBoundaryInvalid", actor);
    auto value = initial(s, actor);
    for (const auto &row :
         s.state.character_condition_extensions.applied_receipts.rows()) {
      const auto &receipt = row.second;
      if (receipt.actor_id != actor)
        continue;
      if (take(compare_world_moments(receipt.effective_at.exact_moment,
                                     boundary)) <= 0)
        apply(value, receipt);
    }
    value.observed_at = boundary;
    return value;
  });
}
Outcome<CharacterConditionAtHead>
read_current_character_condition(const CharacterConditionSources &s,
                                 const StableId &actor) {
  const auto *head =
      s.state.character_condition_extensions.actor_condition_heads.find(actor);
  if (!head)
    return {false,
            {},
            common_failure("CharacterConditionHeadMissing", actor).error};
  return read_character_condition_at_head(s, *head, s.actual_now);
}
// 核对同候选的当前身体与原源重放；任一字段或时钟变化均拒绝。
Outcome<const ActorConditionHead *>
read_verified_current_character_condition_head(
    const CharacterConditionSources &s, const CommonStore &common,
    const AuthoredActorFoundationState &foundation, const StableId &actor,
    const CurrentMomentRead &clock) {
  return guarded<const ActorConditionHead *>([&] {
    if (&s.common != &common || &s.state != &foundation)
      fail("CharacterConditionCurrentCandidateMismatch", actor);
    const auto now = take(clock.read_current_moment());
    if (take(compare_world_moments(now, s.actual_now)))
      fail("CharacterConditionCurrentMomentMismatch", actor);
    if (now.half_day_tick < common.tick())
      fail("CharacterConditionClockBeforeWorld", actor);
    const auto *head =
        foundation.character_condition_extensions.actor_condition_heads.find(
            actor);
    if (!head)
      fail("CharacterConditionHeadMissing", actor);
    const auto revision = common.revision();
    // 复用逐源核验的人物前缀，不采用只验结构的切面，
    // 也不重入军旅接收的全局反向覆盖；此前缀中留存的原因仍须逐一核验。
    const auto verified =
        take(read_character_condition_verified_prefix(s, *head, now));
    const auto *current =
        foundation.character_condition_extensions.actor_condition_heads.find(
            actor);
    const auto *body = common.state().actors.find(actor);
    if (common.revision() != revision || current != head || !current || !body ||
        !same_head(*current, verified.head) ||
        body->alive != verified.body.alive || body->hp != verified.body.hp ||
        body->fatigue != verified.body.fatigue ||
        body->injury != verified.body.injury ||
        take(compare_world_moments(take(clock.read_current_moment()), now)))
      fail("CharacterConditionCurrentCaptureChanged", actor);
    return current;
  });
}
} // namespace hanmo::resident
namespace hanmo::resident {
Outcome<const ConditionReceiptV2 *> read_verified_character_condition_change(
    const CharacterConditionSources &sources,
    const ConditionReceiptRefV2 &ref) {
  auto valid = validate_complete_character_conditions(sources);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *receipt =
      sources.state.character_condition_extensions.applied_receipts.find(
          ref.result_id);
  if (!receipt || receipt->result_id != ref.result_id ||
      receipt->effective_at.kind != ActorFoundationAt::Kind::exact_moment)
    return {
        false,
        {},
        common_failure("CharacterConditionChangeSourceRequired", ref.result_id)
            .error};
  return {true, receipt, {}};
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<const ConditionReceiptV2 *>
read_character_condition_receipt_provenance(const CharacterConditionSources &s,
                                            const ConditionReceiptRefV2 &ref) {
  return guarded<const ConditionReceiptV2 *>([&] {
    take(validate_character_condition_chain_structure(s));
    const auto *target =
        s.state.character_condition_extensions.applied_receipts.find(
            ref.result_id);
    if (!target || target->result_id != ref.result_id)
      fail("CharacterConditionChangeSourceRequired", ref.result_id);
    auto replay = initial(s, target->actor_id);
    bool found = false;
    for (const auto &entry :
         s.state.character_condition_extensions.applied_receipts.rows()) {
      const auto &receipt = entry.second;
      if (receipt.actor_id != target->actor_id)
        continue;
      actual_source(s, receipt, replay.body);
      apply(replay, receipt);
      if (&receipt == target) {
        found = true;
        break;
      }
    }
    if (!found)
      fail("CharacterConditionOriginalPrefixRequired", ref.result_id);
    return target;
  });
}
} // namespace hanmo::resident

namespace hanmo::resident {
// 真应用头定义前缀；原 at_head 的 effective-M 最大门保持原样，不能混用。
Outcome<CharacterConditionAtHead>
read_character_condition_before_application_fact(
    const CharacterConditionSources &sources, const StableId &actor,
    const SourceRef &consumer, const WorldMoment &at) {
  return guarded<CharacterConditionAtHead>([&] {
    if (!take(condition_application_facts_selected(sources.catalog.identity)))
      fail("ConditionApplicationHistorySelectionRequired", consumer.id);
    take(validate_character_condition_chain_structure(sources));
    take(compare_world_moments(at, at));
    if (take(compare_world_moments(at, sources.actual_now)) > 0)
      fail("CharacterHistoricalConditionBoundaryInvalid", actor);
    const auto position =
        take(read_actual_common_fact_position(sources.common, consumer));
    const auto origin = take(read_character_initial_values_before_fact_v2(
        sources.common, sources.state, sources.catalog, sources.family_catalog,
        sources.shared, actor, consumer, at, sources.actual_now,
        sources.born_sources));
    if (!origin.present || !origin.initial.origin || !origin.initial.condition)
      fail("CharacterConditionActorNotPresentBeforeFact", actor);
    auto result = initial(sources, actor);
    const auto &facts = sources.common.state().facts;
    for (std::size_t n = 0; n < position; ++n) {
      const auto *header = facts.find(facts.order()[n]);
      if (!header)
        fail("ConditionApplicationOriginalOrderRequired", facts.order()[n]);
      if (!owns_condition_application_fact_kind(header->kind))
        continue;
      const auto application = take(read_condition_application_fact_header(
          sources.common, sources.state, sources.catalog, *header));
      const auto &receipt = *application.receipt;
      if (receipt.actor_id != actor)
        continue;
      if (take(compare_world_moments(receipt.effective_at.exact_moment, at)) >
          0)
        fail("CharacterConditionFutureApplicationBeforeFact",
             receipt.result_id);
      // 仍由同一原 cause reader 与唯一四标量 reducer 验真；不另写身体规则。
      actual_source(sources, receipt, result.body);
      apply(result, receipt);
    }
    result.observed_at = at;
    return result;
  });
}
} // namespace hanmo::resident
