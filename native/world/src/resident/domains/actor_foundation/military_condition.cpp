#include "military_condition.hpp"
#include "authored_origin.hpp"
#include "condition_application_fact.hpp"
#include "condition_capacity_contract.hpp"
#include "injury_capability_map.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
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
template <class T> T take(Outcome<T> out) {
  if (!out.ok)
    throw Failure{std::move(out.error)};
  return std::move(out.value);
}
template <class T, class F> Outcome<T> guarded(F fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &f) {
    return {false, {}, std::move(f.error)};
  }
}
bool same_time(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b)) == 0;
}
bool allocated(const CommonState &c, const StableId &id) {
  constexpr char prefix[] = "condition:";
  if (id.rfind(prefix, 0) != 0)
    return false;
  const auto text = id.substr(sizeof(prefix) - 1);
  Integer n = 0;
  const auto p = std::from_chars(text.data(), text.data() + text.size(), n);
  return p.ec == std::errc{} && p.ptr == text.data() + text.size() && n > 0 &&
         n < c.next_instance_id && text == std::to_string(n);
}
const OriginalMilitaryWoundRule &
origin_rule(const CommonStore &common, const ActorFoundationCatalog &catalog) {
  take(validate_original_military_wound_rule_definition(catalog));
  if (catalog.identity.content_digest != common.state().metadata.content_digest)
    fail("MilitaryWoundRuleIdentityMismatch");
  return *catalog.original_military_wound_rule;
}
// 先核原军伤规则与目录身份；缺少真实 episode 应用时在任何写入前拒绝。
const OriginalMilitaryWoundRule &rule(const CommonStore &common,
                                      const ActorFoundationCatalog &catalog) {
  const auto &r = origin_rule(common, catalog);
  take(validate_original_military_wound_rule(catalog));
  // A content choice does not define episode merging/recurrence or its actual
  // downstream effects. Reject before mutation until those original writers are
  // installed; never replace Applied with NoAdditionalEffect.
  if (r.selected_applied)
    throw Failure{
        condition_not_implemented("OriginalInjuryEpisodeApplicationRequired")
            .error};
  return r;
}
MilitaryCasualtyConditionCause
original(const MilitaryState &legacy, const MilitaryExactState &exact,
         const SourceRef &ref, Integer ordinal,
         const ActorFoundationCatalog &catalog,
         const MilitaryCasualtyHistoryContext &history) {
  const auto selected = take(read_selected_military_injury_source(
      catalog, legacy, exact, ref, ordinal, history));
  const auto &source = selected.original;
  return {MilitaryCasualtyConditionCause::Disposition::wounded,
          source.casualty_id,
          source.battle_id,
          source.actor_id,
          source.occurred_at,
          2,
          source.source_ordinal};
}

bool same_source(const MilitaryCasualtyConditionCause &a,
                 const MilitaryCasualtyConditionCause &b) {
  return a.disposition == b.disposition &&
         a.casualty_fact_id == b.casualty_fact_id &&
         a.battle_id == b.battle_id && a.actor_id == b.actor_id &&
         a.source_schema == b.source_schema &&
         a.source_ordinal == b.source_ordinal &&
         same_time(a.occurrence_at, b.occurrence_at);
}
} // namespace
MilitaryCasualtyConditionCause
military_death_condition_cause(const MilitaryDeathCause4 &source,
                               Integer schema, Integer ordinal) {
  return {MilitaryCasualtyConditionCause::Disposition::dead,
          source.casualty_fact_id,
          source.battle_id,
          source.actor_id,
          source.occurrence_at,
          schema,
          ordinal};
}
Status validate_military_wound_condition_origin(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog, const MilitaryState &legacy,
    const MilitaryExactState &exact, const ConditionReceiptV2 &receipt,
    const WorldMoment &now, const MilitaryFormationSourceRead *formations,
    const MilitaryReturnSourceRead *returns,
    MilitaryCasualtyHistoryOrder ordering) {
  return guarded<Done>([&] {
    const MilitaryCasualtyHistoryContext history{common, now, formations,
                                                 returns, ordering};
    const auto *cause =
        std::get_if<MilitaryCasualtyConditionCause>(&receipt.cause);
    const auto *key =
        std::get_if<LifeTerminationConditionBusinessKey>(&receipt.business_key);
    if (!cause || !key ||
        cause->disposition !=
            MilitaryCasualtyConditionCause::Disposition::wounded ||
        cause->source_schema != 2)
      fail("MilitaryWoundConditionCauseRequired", receipt.result_id);
    const auto &selected = origin_rule(common, catalog);
    const auto source =
        original(legacy, exact,
                 {"military", "military_casualties", cause->casualty_fact_id},
                 cause->source_ordinal, catalog, history);
    if (!same_source(*cause, source) ||
        !allocated(common.state(), receipt.result_id) ||
        receipt.actor_id != source.actor_id || key->cause_tag != 4 ||
        key->source_id != source.casualty_fact_id ||
        key->actor_id != source.actor_id ||
        key->source_ordinal != source.source_ordinal ||
        !actor_foundation_rule_equal(receipt.rule_ref, selected.rule_ref) ||
        receipt.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
        !same_time(receipt.effective_at.exact_moment, source.occurrence_at) ||
        take(compare_world_moments(source.occurrence_at, now)) > 0 ||
        receipt.condition_revision_before < 0 ||
        receipt.condition_revision_before ==
            std::numeric_limits<Integer>::max() ||
        receipt.condition_revision_after !=
            receipt.condition_revision_before + 1 ||
        receipt.scalar_changes.size() != 2 ||
        receipt.selected_injury_result.kind !=
            (selected.selected_applied
                 ? SelectedInjuryResult::Kind::applied
                 : SelectedInjuryResult::Kind::no_additional_effect) ||
        !actor_foundation_rule_equal(
            receipt.selected_injury_result.injury_rule_ref,
            selected.injury_rule_ref) ||
        !actor_foundation_rule_equal(
            receipt.selected_injury_result.result_choice_ref,
            selected.result_choice_ref) ||
        !receipt.recovery_resolution_refs.empty() ||
        receipt.life_transition_ref ||
        !original_condition_capacity_refs_shape(catalog, receipt) ||
        !state.character_origin_records.origins.find(source.actor_id))
      fail("MilitaryWoundConditionMismatch", receipt.result_id);
    for (const auto &row :
         state.character_condition_extensions.applied_receipts.rows())
      if (row.first != receipt.result_id)
        if (const auto *prior =
                std::get_if<LifeTerminationConditionBusinessKey>(
                    &row.second.business_key);
            prior && prior->cause_tag == 4 &&
            prior->source_id == source.casualty_fact_id &&
            prior->source_ordinal == source.source_ordinal)
          fail("DuplicateMilitaryWoundCondition", receipt.result_id);
    const auto &result = receipt.selected_injury_result;
    if (result.actual_decision_source_ref)
      fail("InjuryActualDecisionSourceRequired", receipt.result_id);
    if (selected.selected_applied) {
      const InjuryCapabilityMapEntry *mapping = nullptr;
      for (const auto &entry : catalog.injury_capability_map->entries)
        if (actor_foundation_rule_equal(entry.injury_rule_ref,
                                        selected.injury_rule_ref) &&
            actor_foundation_rule_equal(entry.result_choice_ref,
                                        selected.result_choice_ref))
          mapping = &entry;
      if (!mapping ||
          result.effect_items.size() != mapping->effect_items.size())
        fail("MilitaryWoundSelectedEffectsMismatch", receipt.result_id);
      for (std::size_t i = 0; i < result.effect_items.size(); ++i)
        if (!same_original_injury_effect(result.effect_items[i],
                                         mapping->effect_items[i]))
          fail("MilitaryWoundSelectedEffectsMismatch", receipt.result_id);
    } else if (!result.effect_items.empty() ||
               !receipt.created_effect_keys.empty())
      fail("InactiveConditionInjuryEffects", receipt.result_id);
    const auto &hp = receipt.scalar_changes[0];
    const auto &injury = receipt.scalar_changes[1];
    if (hp.field != ConditionScalarChange::Field::hp || hp.before < 0 ||
        hp.before > 100 || hp.after != std::min<Integer>(hp.before, 70) ||
        injury.field != ConditionScalarChange::Field::injury ||
        injury.before < 0 || injury.before > 3 ||
        injury.after != std::max<Integer>(1, injury.before))
      fail("MilitaryWoundOriginalFormulaMismatch", receipt.result_id);
    return Done{};
  });
}
// 先验原军旅事实与回执，再拒绝尚无原 episode 应用证明的附加伤效。
Status validate_military_wound_condition(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog, const MilitaryState &legacy,
    const MilitaryExactState &exact, const ConditionReceiptV2 &receipt,
    const WorldMoment &now, const MilitaryFormationSourceRead *formations,
    const MilitaryReturnSourceRead *returns,
    MilitaryCasualtyHistoryOrder ordering) {
  auto origin = validate_military_wound_condition_origin(
      common, state, catalog, legacy, exact, receipt, now, formations, returns,
      ordering);
  if (!origin.ok)
    return origin;
  if (receipt.selected_injury_result.kind ==
      SelectedInjuryResult::Kind::applied)
    return condition_not_implemented("OriginalInjuryEpisodeApplicationRequired",
                                     receipt.result_id);
  return common_success();
}
// 沿实际军旅伤损来源更新身体与回执；来源失效或重复冲突时拒绝。
Outcome<MilitaryWoundApplication> apply_verified_military_wound(
    CommonStore &common, AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog, const MilitaryState &legacy,
    const MilitaryExactState &exact, const SourceRef &ref, Integer ordinal,
    const ReadStamp &stamp, const ReadStampAuthority &stamps,
    const DomainReadVersionSource &versions,
    const MilitaryFormationSourceRead *formations,
    const MilitaryReturnSourceRead *returns) {
  return guarded<MilitaryWoundApplication>([&] {
    const MilitaryCasualtyHistoryContext history{common, stamp.span().to,
                                                 formations, returns};
    const auto &selected = rule(common, catalog);
    const auto source = original(legacy, exact, ref, ordinal, catalog, history);
    take(stamps.validate(stamp, common.state().identity,
                         ReadMode::phase_current, common, versions));
    bool actor_read = false;
    for (const auto &r : stamp.resources())
      if (r.resource.kind == ResourceKey::Kind::actor &&
          r.resource.id == source.actor_id)
        actor_read = true;
    if (!actor_read ||
        std::find(stamp.source_ids().begin(), stamp.source_ids().end(),
                  ref.id) == stamp.source_ids().end() ||
        !same_time(stamp.span().to, source.occurrence_at))
      fail("MilitaryWoundPhaseSourceMismatch", ref.id);
    const ConditionReceiptV2 *existing = nullptr;
    for (const auto &row :
         state.character_condition_extensions.applied_receipts.rows())
      if (const auto *key = std::get_if<LifeTerminationConditionBusinessKey>(
              &row.second.business_key);
          key && key->cause_tag == 4 && key->source_id == ref.id &&
          key->source_ordinal == ordinal) {
        if (existing)
          fail("DuplicateMilitaryWoundCondition", ref.id);
        existing = &row.second;
      }
    if (existing) {
      take(validate_military_wound_condition(common, state, catalog, legacy,
                                             exact, *existing, stamp.span().to,
                                             formations, returns));
      take(validate_condition_application_fact_for_receipt(common, state,
                                                           catalog, *existing));
      return MilitaryWoundApplication{{existing->result_id}, {}, true};
    }
    const auto *actor = common.state().actors.find(source.actor_id);
    auto *head =
        state.character_condition_extensions.actor_condition_heads.find(
            source.actor_id);
    if (!actor || !actor->alive || actor->hp < 0 || actor->hp > 100 ||
        actor->injury < 0 || actor->injury > 3 || !head ||
        head->condition_revision < 0 ||
        head->condition_revision == std::numeric_limits<Integer>::max() ||
        !state.character_origin_records.origins.find(source.actor_id))
      fail("MilitaryWoundActorSourceRequired", source.actor_id);
    const auto capture = common.actor_condition_capture_identity();
    if (!capture)
      fail("MilitaryWoundConditionCaptureRequired");
    auto produced = take(produce_selected_military_injury_result(
        catalog, legacy, exact, ref, ordinal, history));
    // 来源选定值以自身临时租约复制到真实原回执。
    // 既有 OrderedTable 预付目标副本；两份租约均不重复计作持久世界费用。


    auto selected_fields = CheckpointMemoryCharge(0);
    ConditionReceiptV2 receipt;
    receipt.result_id = take(common.next_id("condition"));
    receipt.actor_id = source.actor_id;
    receipt.business_key = LifeTerminationConditionBusinessKey{
        4, ref.id, source.actor_id, ordinal};
    receipt.cause = source;
    receipt.rule_ref = selected.rule_ref;
    receipt.effective_at.exact_moment = source.occurrence_at;
    receipt.condition_revision_before = head->condition_revision;
    receipt.condition_revision_after = head->condition_revision + 1;
    receipt.scalar_changes = {{ConditionScalarChange::Field::hp, actor->hp,
                               std::min<Integer>(actor->hp, 70)},
                              {ConditionScalarChange::Field::injury,
                               actor->injury,
                               std::max<Integer>(1, actor->injury)}};
    receipt.selected_injury_result = SelectedInjuryResult{produced.result()};
    take(validate_military_wound_condition(common, state, catalog, legacy,
                                           exact, receipt, stamp.span().to,
                                           formations, returns));
    const auto before = common.actor_condition_execution_drafts().size();
    ActorMutation mutation;
    mutation.actor_id = source.actor_id;
    mutation.hp = receipt.scalar_changes[0].after;
    mutation.injury = receipt.scalar_changes[1].after;
    take(common.apply_actor(mutation));
    const auto result_id = receipt.result_id;
    if (!state.character_condition_extensions.applied_receipts.insert(
            result_id, std::move(receipt)))
      fail("DuplicateConditionReceipt", result_id);
    head->condition_revision += 1;
    head->latest_receipt_ref = ConditionReceiptRefV2{result_id};
    if (common.actor_condition_execution_drafts().size() != before + 1)
      fail("MilitaryWoundActualWriteCountMismatch", result_id);
    BoundActorConditionWrite actual{{result_id}, capture, Integer(before + 1)};
    take(append_condition_application_fact_in_candidate(common, state, catalog,
                                                        actual));
    std::vector<BoundActorConditionWrite> writes;
    writes.push_back(std::move(actual));
    return MilitaryWoundApplication{{result_id}, std::move(writes), false};
  });
}
Status validate_military_wound_coverage(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog, const MilitaryState &legacy,
    const MilitaryExactState &exact, const WorldMoment &now,
    const MilitaryFormationSourceRead *formations,
    const MilitaryReturnSourceRead *returns,
    MilitaryCasualtyHistoryOrder ordering) {
  return guarded<Done>([&] {
    for (const auto &row :
         state.character_condition_extensions.applied_receipts.rows())
      if (const auto *cause =
              std::get_if<MilitaryCasualtyConditionCause>(&row.second.cause);
          cause && cause->disposition ==
                       MilitaryCasualtyConditionCause::Disposition::wounded)
        take(validate_military_wound_condition(common, state, catalog, legacy,
                                               exact, row.second, now,
                                               formations, returns, ordering));
    for (const auto &row : exact.dynamic_casualty_facts.rows())
      for (std::size_t i = 0; i < row.second.actor_changes.size(); ++i)
        if (row.second.actor_changes[i].status == "wounded") {
          std::size_t count = 0;
          for (const auto &receipt :
               state.character_condition_extensions.applied_receipts.rows())
            if (const auto *cause = std::get_if<MilitaryCasualtyConditionCause>(
                    &receipt.second.cause);
                cause && cause->casualty_fact_id == row.first &&
                cause->source_schema == 2 &&
                cause->source_ordinal == Integer(i) &&
                cause->disposition ==
                    MilitaryCasualtyConditionCause::Disposition::wounded)
              ++count;
          if (count != 1)
            fail("MilitaryWoundConditionCoverageMismatch", row.first);
        }
    return Done{};
  });
}
} // namespace hanmo::resident
