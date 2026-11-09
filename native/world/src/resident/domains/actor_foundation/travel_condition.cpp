#include "condition_application_fact.hpp"
#include "condition_capacity_contract.hpp"
// 原行旅完整单位的疲劳数学只有本写口执行，技术slice和通知均不重扣。
#include "authored_origin.hpp"
#include "character_condition_read.hpp"
#include "created_origin.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/domains/travel/movement_exact.hpp"
#include "resident/storage/common_store.hpp"
#include "travel_condition.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace hanmo::resident {
namespace {
// SourceRef完整三项比较，不能只凭同名ID或源表存在冒同一关闭事实。
bool source_equal(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
// S1完整source
// validator在前；本reader补核本次身体消费必须的单位关闭、参与与原规则。
Outcome<const TravelExertionSource *>
read_exertion(const CommonState &common, const ActorFoundationCatalog &catalog,
              const MovementExactState &movement, const StableId &id) {
  const auto fail =
      [&](const StableId &code) -> Outcome<const TravelExertionSource *> {
    return {false, nullptr, common_failure(code, id).error};
  };
  if (!catalog.travel_rule ||
      catalog.travel_rule->id != "original_travel_exertion" ||
      catalog.travel_rule->version != 1 ||
      catalog.travel_rule->catalog_digest != common.metadata.content_digest)
    return fail("TravelConditionRuleRequired");
  auto verified = read_verified_travel_exertion(movement, id);
  if (!verified.ok)
    return {false, nullptr, verified.error};
  const auto *source = movement.exertions.find(id);
  if (!source || source->id != id || source->original_fatigue_per_unit < 0 ||
      source->original_fatigue_per_unit > 100)
    return fail("InvalidTravelExertionSource");
  if (source->actual_participants.empty() ||
      source->actual_participants.size() > 4096)
    return fail("TravelExertionParticipantsInvalid");
  for (std::size_t n = 0; n < source->actual_participants.size(); ++n) {
    const auto &actor = source->actual_participants[n];
    if (actor.empty() || !common.actors.find(actor))
      return fail("TravelExertionParticipantsInvalid");
    for (std::size_t before = 0; before < n; ++before)
      if (source->actual_participants[before] == actor)
        return fail("TravelExertionParticipantsInvalid");
  }
  return {true, source, {}};
}
// 原整数疲劳范围内checked加法；封顶净0仍是真实一次写入。
Integer after_fatigue(Integer before, Integer increment) {
  return before >= 100 - increment ? 100 : before + increment;
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
} // namespace
Status validate_travel_exertion_condition_receipt(
    const CommonState &common, const ConditionReceiptV2 &receipt,
    const ActorFoundationCatalog &catalog, const MovementExactState &movement) {
  const auto *cause = std::get_if<TravelExertionConditionCause>(&receipt.cause);
  const auto *key =
      std::get_if<TravelConditionBusinessKey>(&receipt.business_key);
  if (!cause || !key)
    return common_failure("TravelConditionCauseMismatch", receipt.result_id);
  auto source =
      read_exertion(common, catalog, movement, cause->exertion_source_id);
  if (!source.ok)
    return {false, {}, source.error};
  const auto &actual = *source.value;
  if (!allocated(common, receipt.result_id) ||
      receipt.actor_id != cause->actor_id ||
      key->actor_id != receipt.actor_id || key->unit_ref != actual.unit_ref ||
      key->exertion_source_id != actual.id ||
      std::find(actual.actual_participants.begin(),
                actual.actual_participants.end(),
                receipt.actor_id) == actual.actual_participants.end() ||
      !actor_foundation_rule_equal(receipt.rule_ref, *catalog.travel_rule) ||
      receipt.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
      receipt.scalar_changes.size() != 1 ||
      receipt.selected_injury_result.kind !=
          SelectedInjuryResult::Kind::not_an_injury ||
      !receipt.created_effect_keys.empty() ||
      !receipt.recovery_resolution_refs.empty() ||
      receipt.life_transition_ref ||
      !original_condition_capacity_refs_shape(catalog, receipt))
    return common_failure("TravelConditionReceiptMismatch", receipt.result_id);
  auto same = compare_world_moments(receipt.effective_at.exact_moment,
                                    actual.actual_span.to);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value != 0)
    return common_failure("TravelConditionMomentMismatch", receipt.result_id);
  const auto &change = receipt.scalar_changes.front();
  if (change.field != ConditionScalarChange::Field::fatigue ||
      change.before < 0 || change.before > 100 ||
      change.after !=
          after_fatigue(change.before, actual.original_fatigue_per_unit))
    return common_failure("TravelConditionFormulaMismatch", receipt.result_id);
  return common_success();
}
Outcome<TravelConditionApplication> apply_travel_exertion_condition(
    CommonStore &common, AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog, const MovementExactState &movement,
    const StableId &exertion, const CharacterConditionSources *original,
    GrowthCapabilityRevisionWrite *capability_changes) {
  auto source = read_exertion(common.state(), catalog, movement, exertion);
  if (!source.ok)
    return {false, {}, source.error};
  const auto &actual = *source.value;
  TravelConditionApplication result;
  const auto capture = common.actor_condition_capture_identity();
  if (!capture)
    return {false,
            {},
            common_failure("TravelConditionActualCaptureRequired").error};
  bool complete_origin = false, capability_chain = false;
  for (const auto &module : catalog.identity.module_versions) {
    if (module.first == "actor_start_context" && module.second == "3")
      complete_origin = true;
    if (module.first == "growth_permanent_contributions" &&
        module.second == "3")
      capability_chain = true;
  }
  if ((complete_origin || capability_chain) &&
      (!original || &original->common != &common ||
       &original->state != &state || &original->catalog != &catalog ||
       original->movement != &movement))
    return {false,
            {},
            common_failure("TravelConditionOriginalSourcesRequired").error};
  if (capability_chain &&
      (!original->permanent_growth || !capability_changes ||
       !capability_changes->bound_to(common, *original->permanent_growth)))
    return {false,
            {},
            common_failure("TravelConditionCapabilityWriterRequired").error};
  // 先准备整批，缺任何真实人物origin/head或当前身体非法均不开始写第一人。
  for (const auto &actor : actual.actual_participants) {
    if (complete_origin) {
      auto origin = read_character_initial_values_v2(
          common, state, catalog, original->family_catalog, original->shared,
          actor, original->actual_now, original->born_sources);
      if (!origin.ok)
        return {false, {}, origin.error};
    } else {
      auto origin = read_authored_character_origin_v2(common.state(), state,
                                                      catalog, actor);
      if (!origin.ok)
        return {false, {}, origin.error};
    }
    const auto *head =
        state.character_condition_extensions.actor_condition_heads.find(actor);
    const auto *person = common.actor(actor);
    if (!head || head->condition_revision < 0 ||
        head->condition_revision == std::numeric_limits<Integer>::max() ||
        !person || person->fatigue < 0 || person->fatigue > 100)
      return {false,
              {},
              common_failure("TravelConditionActorInvalid", actor).error};
  }
  for (const auto &actor : actual.actual_participants) {
    const ConditionReceiptV2 *existing = nullptr;
    for (const auto &row :
         state.character_condition_extensions.applied_receipts.rows())
      if (const auto *cause =
              std::get_if<TravelExertionConditionCause>(&row.second.cause);
          cause && cause->exertion_source_id == exertion &&
          row.second.actor_id == actor) {
        if (existing)
          return {false,
                  {},
                  common_failure("DuplicateTravelConditionReceipt", exertion)
                      .error};
        existing = &row.second;
      }
    if (existing) {
      auto valid = validate_travel_exertion_condition_receipt(
          common.state(), *existing, catalog, movement);
      if (!valid.ok)
        return {false, {}, valid.error};
      valid = validate_condition_application_fact_for_receipt(
          common, state, catalog, *existing);
      if (!valid.ok)
        return {false, {}, valid.error};
      result.receipt_refs.push_back({existing->result_id});
      continue;
    }
    if (complete_origin || capability_chain) {
      auto now =
          compare_world_moments(actual.actual_span.to, original->actual_now);
      if (!now.ok)
        return {false, {}, now.error};
      if (now.value)
        return {
            false,
            {},
            common_failure("TravelConditionActualWriteMomentRequired", exertion)
                .error};
    }
    const auto *person = common.actor(actor);
    auto *head =
        state.character_condition_extensions.actor_condition_heads.find(actor);
    ConditionReceiptV2 receipt;
    receipt.actor_id = actor;
    receipt.business_key =
        TravelConditionBusinessKey{actual.unit_ref, actor, exertion};
    receipt.cause = TravelExertionConditionCause{exertion, actor};
    receipt.rule_ref = *catalog.travel_rule;
    receipt.effective_at.kind = ActorFoundationAt::Kind::exact_moment;
    receipt.effective_at.exact_moment = actual.actual_span.to;
    receipt.condition_revision_before = head->condition_revision;
    receipt.condition_revision_after = head->condition_revision + 1;
    receipt.scalar_changes = {
        {ConditionScalarChange::Field::fatigue, person->fatigue,
         after_fatigue(person->fatigue, actual.original_fatigue_per_unit)}};
    const auto draft_offset = common.actor_condition_execution_drafts().size();
    ActorMutation change;
    change.actor_id = actor;
    change.fatigue = receipt.scalar_changes.front().after;
    auto applied = common.apply_actor(change);
    if (!applied.ok)
      return {false, {}, applied.error};
    auto id = common.next_id("condition");
    if (!id.ok)
      return {false, {}, id.error};
    receipt.result_id = id.value;
    auto valid = validate_travel_exertion_condition_receipt(
        common.state(), receipt, catalog, movement);
    if (!valid.ok)
      return {false, {}, valid.error};
    if (!state.character_condition_extensions.applied_receipts.insert(
            id.value, std::move(receipt)))
      return {false,
              {},
              common_failure("DuplicateConditionReceipt", id.value).error};
    head->condition_revision += 1;
    head->latest_receipt_ref = ConditionReceiptRefV2{id.value};
    if (common.actor_condition_execution_drafts().size() != draft_offset + 1)
      return {false,
              {},
              common_failure("TravelConditionActualWriteCountMismatch").error};
    BoundActorConditionWrite actual_write{
        {id.value}, capture, Integer(draft_offset + 1)};
    valid = append_condition_application_fact_in_candidate(
        common, state, catalog, actual_write);
    if (!valid.ok)
      return {false, {}, valid.error};
    if (capability_chain) {
      auto recorded =
          capability_changes->record_actual_condition_change(actual_write);
      if (!recorded.ok)
        return {false, {}, recorded.error};
    }
    result.receipt_refs.push_back({id.value});
    result.actual_writes.push_back(std::move(actual_write));
  }
  return {true, std::move(result), {}};
}
Status validate_travel_exertion_condition_coverage(
    const CommonState &common, const AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog, const MovementExactState &movement) {
  for (const auto &row :
       state.character_condition_extensions.applied_receipts.rows())
    if (const auto *cause =
            std::get_if<TravelExertionConditionCause>(&row.second.cause)) {
      auto valid = validate_travel_exertion_condition_receipt(
          common, row.second, catalog, movement);
      if (!valid.ok)
        return valid;
      for (const auto &before :
           state.character_condition_extensions.applied_receipts.rows()) {
        if (&before == &row)
          break;
        if (const auto *previous =
                std::get_if<TravelExertionConditionCause>(&before.second.cause);
            previous &&
            previous->exertion_source_id == cause->exertion_source_id &&
            before.second.actor_id == row.second.actor_id)
          return common_failure("DuplicateTravelConditionReceipt", row.first);
      }
    }
  for (const auto &row : movement.exertions.rows()) {
    auto source = read_exertion(common, catalog, movement, row.first);
    if (!source.ok)
      return {false, {}, source.error};
    for (const auto &actor : source.value->actual_participants) {
      bool covered = false;
      for (const auto &receipt :
           state.character_condition_extensions.applied_receipts.rows())
        if (const auto *cause = std::get_if<TravelExertionConditionCause>(
                &receipt.second.cause);
            cause && cause->exertion_source_id == row.first &&
            receipt.second.actor_id == actor)
          covered = true;
      if (!covered)
        return common_failure("MissingTravelConditionReceipt",
                              row.first + "/" + actor);
    }
  }
  return common_success();
}
} // namespace hanmo::resident
