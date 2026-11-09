#include "injury_original_results_v1.hpp"
#include "authored_origin.hpp"
#include "character_condition_read.hpp"
#include "condition_capacity_contract.hpp"
#include "injury_source_links_v1.hpp"
#include "injury_service_coverage_v1.hpp"
#include "injury_sources_v1.hpp"
#include "resident/domains/growth/growth_fixed_service_model.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/growth/growth_recovery_qualification.hpp"
#include "resident/domains/growth/growth_recovery_terminal_v1.hpp"
#include "resident/domains/personal/personal_model.hpp"
#include "resident/storage/common_store.hpp"
#include "rest_condition.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> result) {
  if (!result.ok)
    throw Failure{std::move(result.error)};
  return std::move(result.value);
}
[[noreturn]] void fail(const char *code, const StableId &id) {
  throw Failure{common_failure(code, id).error};
}
const CharacterInjuryApplicationV1 &
application(const CharacterConditionSources &body,
            const CharacterInjurySourcesStateV1 &sources,
            const ConditionReceiptV2 &receipt) {
  if (body.state.character_condition_extensions.applied_receipts.find(
          receipt.result_id) != &receipt)
    fail("CharacterInjuryOriginalReceiptIdentityRequired", receipt.result_id);
  take(validate_character_injury_source_links_v1(body.common, body.state,
                                                 body.catalog, sources));
  const auto *source = sources.applications.find(receipt.result_id);
  if (!source)
    fail("CharacterInjuryApplicationMissing", receipt.result_id);
  if (receipt.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
      take(compare_world_moments(receipt.effective_at.exact_moment,
                                 body.actual_now)) > 0)
    fail("CharacterInjuryOriginalResultFuture", receipt.result_id);
  return *source;
}
void moment(const ConditionReceiptV2 &receipt, const WorldMoment &actual) {
  if (take(compare_world_moments(receipt.effective_at.exact_moment, actual)))
    fail("CharacterInjuryOriginalResultMomentMismatch", receipt.result_id);
}
void battle_result(const CharacterInjuryApplicationV1 &source,
                   const ConditionReceiptV2 &receipt,
                   const CharacterBattleInjuryReadV1 &raw) {
  if (!raw.completed_at)
    fail("CharacterInjuryOriginalBattleMomentMissing", receipt.result_id);
  moment(receipt, *raw.completed_at);
  // The genuine original source, not the application row, supplies incoming.
  if (raw.incoming_injury < 0 || raw.incoming_injury > 3)
    fail("CharacterInjuryOriginalBattleGradeInvalid", receipt.result_id);
  const auto &credit = source.progress;
  // Even the incoming-zero branch must reject an invalid before-state rather
  // than preserve arbitrary integers. This is shape, not prefix authentication.
  if (credit.before.injury < 0 || credit.before.injury > 3 ||
      credit.before.points < 0 || credit.before.points > 3 ||
      (!credit.before.injury && credit.before.points))
    fail("CharacterInjuryOriginalBeforeInvalid", receipt.result_id);
  auto expected = credit.before;
  if (raw.incoming_injury)
    expected =
        take(apply_growth_battle_injury_v1(credit.before, raw.incoming_injury));
  if (credit.offered_points || credit.used_points ||
      credit.after.injury != expected.injury ||
      credit.after.points != expected.points)
    fail("CharacterInjuryOriginalBattleMergeMismatch", receipt.result_id);
  // 野战来源只含本段 wounded；dead 不通过伤势函数授权。
}
// 精确原写集：同值写仍保留 presence，禁止夹带 alive 或未登记标量。
void scalars(const ConditionReceiptV2 &receipt,
             const GrowthRecoveryCredit &credit, bool hp, bool fatigue,
             bool injury, Integer hp_after, Integer fatigue_after) {
  const auto &rows = receipt.scalar_changes;
  if (rows.size() !=
      std::size_t(hp) + std::size_t(fatigue) + std::size_t(injury))
    fail("CharacterInjuryOriginalBodyWriteSetMismatch", receipt.result_id);
  std::size_t next = 0;
  const auto check = [&](ConditionScalarChange::Field field, Integer after,
                         Integer maximum) {
    const auto &row = rows[next++];
    if (row.field != field || row.before < 0 || row.before > maximum ||
        row.after != after || after < 0 || after > maximum)
      fail("CharacterInjuryOriginalBodyFormulaMismatch", receipt.result_id);
  };
  if (hp)
    check(ConditionScalarChange::Field::hp, hp_after, 100);
  if (fatigue)
    check(ConditionScalarChange::Field::fatigue, fatigue_after, 100);
  if (injury) {
    if (rows[next].before != credit.before.injury)
      fail("CharacterInjuryOriginalBodyBeforeMismatch", receipt.result_id);
    check(ConditionScalarChange::Field::injury, credit.after.injury, 3);
  }
}
// 仅借原收据前像供原公式；前像真实性另由精确 Condition prefix 认证。
Integer scalar_before(const ConditionReceiptV2 &receipt,
                      ConditionScalarChange::Field field) {
  const ConditionScalarChange *found = nullptr;
  for (const auto &row : receipt.scalar_changes)
    if (row.field == field) {
      if (found)
        fail("CharacterInjuryRepeatedBodyScalar", receipt.result_id);
      found = &row;
    }
  if (!found || found->before < 0 || found->before > 100)
    fail("CharacterInjuryOriginalBodyScalarRequired", receipt.result_id);
  return found->before;
}
} // namespace
Status validate_personal_injury_original_result_v1(
    const CharacterConditionSources &body,
    const CharacterInjurySourcesStateV1 &sources,
    const ConditionReceiptV2 &receipt, const PersonalState &personal,
    const PersonalCatalog &catalog, Integer selected_lifecycle) {
  try {
    const auto &source = application(body, sources, receipt);
    const auto *original =
        std::get_if<CharacterBattleReceiptSourceV1>(&source.original_source);
    if (!original)
      fail("CharacterInjuryOriginalPersonalSourceMismatch", receipt.result_id);
    auto raw = take(read_personal_battle_injury_source_v1(
        body.common, personal, catalog, selected_lifecycle, *original,
        source.actor_id, body.actual_now));
    battle_result(source, receipt, raw);
    const auto *battle = personal.battles.find(original->original_receipt.id);
    const auto *unit = battle ? battle->units.find(source.actor_id) : nullptr;
    if (!unit)
      fail("CharacterInjuryPersonalUnitRequired", receipt.result_id);
    const Integer fatigue =
        scalar_before(receipt, ConditionScalarChange::Field::fatigue);
    scalars(receipt, source.progress, true, true, unit->hp == 0,
            std::max<Integer>(1, unit->hp),
            std::min<Integer>(100, fatigue + 6));
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_field_injury_original_result_v1(
    const CharacterConditionSources &body,
    const CharacterInjurySourcesStateV1 &sources,
    const ConditionReceiptV2 &receipt, const MilitaryState &military,
    const FieldBattleWorldState &field) {
  try {
    const auto &source = application(body, sources, receipt);
    const auto *original =
        std::get_if<CharacterBattleReceiptSourceV1>(&source.original_source);
    if (!original)
      fail("CharacterInjuryOriginalFieldSourceMismatch", receipt.result_id);
    auto raw = take(read_field_battle_injury_source_v1(
        body.common, military, field, *original, source.actor_id,
        body.actual_now, body.injury_field_clock));
    battle_result(source, receipt, raw);
    const Integer hp = scalar_before(receipt, ConditionScalarChange::Field::hp);
    scalars(receipt, source.progress, true, false, true,
            std::min<Integer>(hp, 70), 0);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_treatment_injury_original_result_v1(
    const CharacterConditionSources &body,
    const CharacterInjurySourcesStateV1 &sources,
    const ConditionReceiptV2 &receipt) {
  try {
    const auto &source = application(body, sources, receipt);
    const auto *original =
        std::get_if<CharacterTreatmentActionSourceV1>(&source.original_source);
    if (!original || original->completion_ordinal != 1)
      fail("CharacterInjuryOriginalTerminalSourceMismatch", receipt.result_id);
    take(validate_injury_recovery_selection_v1(body));
    if (!body.injury_recovery_catalog)
      fail("CharacterInjuryTreatmentSelectionRequired", receipt.result_id);
    auto raw = take(
        read_growth_terminal_recovery_source_v1(body, original->action_id));
    if (!raw.original_activity || !raw.original_completion ||
        raw.original_activity->view.actor_id != source.actor_id)
      fail("CharacterInjuryOriginalTreatmentActorMismatch", receipt.result_id);
    moment(receipt, raw.original_completion->occurred_at);
    const auto expected = take(apply_growth_terminal_recovery_points_v1(
        source.progress.before, raw.method_points_per_half_day,
        raw.actual_completed_half_days));
    if (source.progress.offered_points != expected.offered_points ||
        source.progress.used_points != expected.used_points ||
        source.progress.after.injury != expected.after.injury ||
        source.progress.after.points != expected.after.points)
      fail("CharacterInjuryOriginalTerminalCreditMismatch", receipt.result_id);
    const auto *terms =
        std::get_if<ServiceTerms>(&raw.original_activity->terms);
    if (!terms || terms->hp_gain < 0 || terms->hp_gain > 100 ||
        terms->fatigue_delta < -100 || terms->fatigue_delta > 100)
      fail("CharacterInjuryTreatmentTermsRequired", receipt.result_id);
    // 原Growth provided先于finish：完整终局服务采用此次真实恢复后的伤级上限。
    const auto hp = scalar_before(receipt, ConditionScalarChange::Field::hp);
    const auto fatigue =
        scalar_before(receipt, ConditionScalarChange::Field::fatigue);
    scalars(receipt, source.progress, true, true, true,
            std::clamp<Integer>(
                hp + terms->hp_gain, 0,
                growth_injury_hp_limit(source.progress.after.injury)),
            std::clamp<Integer>(fatigue + terms->fatigue_delta, 0, 100));
    if (receipt.rule_ref.id != "character_terminal_recovery_credit" ||
        receipt.rule_ref.version != 1 ||
        receipt.rule_ref.catalog_digest !=
            body.catalog.identity.content_digest ||
        receipt.selected_injury_result.kind !=
            SelectedInjuryResult::Kind::not_an_injury ||
        receipt.selected_injury_result.actual_decision_source_ref ||
        !receipt.selected_injury_result.effect_items.empty() ||
        !receipt.created_effect_keys.empty() ||
        !receipt.recovery_resolution_refs.empty() ||
        receipt.life_transition_ref ||
        !original_condition_capacity_refs_shape(body.catalog, receipt))
      fail("CharacterInjuryTerminalBodyRuleMismatch", receipt.result_id);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_rest_injury_original_result_v1(
    const CharacterConditionSources &body,
    const CharacterInjurySourcesStateV1 &sources,
    const ConditionReceiptV2 &receipt) {
  try {
    const auto &source = application(body, sources, receipt);
    const auto *rest =
        std::get_if<OriginalRestProvidedCause>(&source.original_source);
    if (!rest || !body.growth || !body.rest_time)
      fail("CharacterInjuryRestOwningSourcesRequired", receipt.result_id);
    const auto raw = take(read_authored_rest_completion(
        body.common.state(), *body.growth, body.catalog, *rest,
        receipt.actor_id, *body.rest_time, body.actual_now));
    take(validate_injury_recovery_selection_v1(body));
    if (!raw.activity || (body.injury_recovery_catalog &&
                          body.injury_recovery_catalog->service_roles.find(
                              raw.activity->context.role_id)))
      fail("CharacterInjuryRestMethodOwnerMismatch", receipt.result_id);
    moment(receipt, raw.completed_at);
    const auto after = take(apply_growth_rest_recovery_v1(
        source.progress.before, raw.reduces_injury));
    if (source.progress.offered_points || source.progress.used_points ||
        source.progress.after.injury != after.injury ||
        source.progress.after.points != after.points)
      fail("CharacterInjuryRestCarryMismatch", receipt.result_id);
    const auto *terms = raw.activity
                            ? std::get_if<ServiceTerms>(&raw.activity->terms)
                            : nullptr;
    if (!terms)
      fail("CharacterInjuryRestTermsRequired", receipt.result_id);
    const Integer hp = scalar_before(receipt, ConditionScalarChange::Field::hp);
    const Integer fatigue =
        scalar_before(receipt, ConditionScalarChange::Field::fatigue);
    scalars(receipt, source.progress, true, true, raw.reduces_injury,
            std::clamp<Integer>(hp + terms->hp_gain, 0, 100),
            std::clamp<Integer>(fatigue + terms->fatigue_delta, 0, 100));
    if (!actor_foundation_rule_equal(receipt.rule_ref,
                                     body.catalog.rest_rule) ||
        receipt.selected_injury_result.kind !=
            SelectedInjuryResult::Kind::not_an_injury ||
        receipt.selected_injury_result.actual_decision_source_ref ||
        !receipt.selected_injury_result.effect_items.empty() ||
        !receipt.created_effect_keys.empty() ||
        !receipt.recovery_resolution_refs.empty() ||
        receipt.life_transition_ref ||
        !original_condition_capacity_refs_shape(body.catalog, receipt))
      fail("CharacterInjuryRestBodyRuleMismatch", receipt.result_id);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
