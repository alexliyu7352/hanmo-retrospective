#include "military_supply_condition.hpp"
#include "authored_origin.hpp"
#include "character_condition_replay.hpp"
#include "character_condition_world.hpp"
#include "condition_application_fact.hpp"
#include "condition_capacity_contract.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw Failure{common_failure(code, id, kind).error};
}
template <class T> T take(Outcome<T> x) {
  if (!x.ok)
    throw Failure{std::move(x.error)};
  return std::move(x.value);
}
bool same_time(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b)) == 0;
}
bool selected_profile(const ActorFoundationCatalog &catalog) {
  std::size_t count = 0;
  for (const auto &module : catalog.identity.module_versions)
    if (module.first == "character_condition_extensions") {
      if (module.second != "7" && module.second != "4" && module.second != "8")
        return false;
      ++count;
    }
  return count == 1 && !catalog.identity.content_digest.empty();
}
// /1 仅明确 Field1+C8+伤源1 且未安装旧 dynamic 根；不是从空指针猜选择。
bool field_profile(const CharacterConditionSources &s) {
  const auto selected = [&](const char *key, const char *version) {
    std::size_t count = 0;
    for (const auto &m : s.catalog.identity.module_versions)
      if (m.first == key) {
        if (m.second != version)
          return false;
        ++count;
      }
    return count == 1;
  };
  return selected("field_battle", "1") &&
         selected("character_condition_extensions", "8") &&
         selected("character-injury-sources", "1") && !s.military_exact;
}
void selected(const CharacterConditionSources &s, Integer schema = 2) {
  if (!selected_profile(s.catalog) || !s.growth_catalog ||
      !s.growth_catalog->complete_capability_application_chain || !s.military ||
      (schema == 1
           ? (!field_profile(s) || !s.injury_field || !s.injury_field_clock ||
              !s.injury_field_clock->bound_to(s.common, *s.military,
                                              *s.injury_field))
           : schema != 2 || !s.military_exact) ||
      !s.military_supply_periods ||
      !s.catalog.original_military_supply_shortage_rule)
    fail("MilitarySupplyConditionSourcesRequired", {},
         ErrorKind::unsupported_capability);
  const auto &rule = *s.catalog.original_military_supply_shortage_rule;
  if (rule.id != original_military_supply_rule_ref || rule.version != 1 ||
      rule.catalog_digest != s.common.state().metadata.content_digest ||
      rule.catalog_digest != s.catalog.identity.content_digest)
    fail("MilitarySupplyConditionSelectedRuleMismatch");
}
bool allocated(const CommonState &c, const StableId &id) {
  constexpr std::string_view prefix = "condition:";
  if (id.size() <= prefix.size() || id.compare(0, prefix.size(), prefix) != 0)
    return false;
  const std::string_view digits(id.data() + prefix.size(),
                                id.size() - prefix.size());
  if (digits.front() == '0')
    return false;
  Integer n = 0;
  const auto p =
      std::from_chars(digits.data(), digits.data() + digits.size(), n);
  return p.ec == std::errc{} && p.ptr == digits.data() + digits.size() &&
         n > 0 && n < c.next_instance_id;
}
Integer fatigue_after(Integer before) {
  if (before < 0 || before > 100)
    fail("MilitarySupplyFatigueOutOfRange");
  return before >= 96 ? 100 : before + 4;
}
ExactMilitarySupplyActorConsequence original(const CharacterConditionSources &s,
                                             const SourceRef &ref,
                                             Integer schema, Integer ordinal) {
  selected(s, schema);
  if (schema == 1)
    return take(read_field_military_supply_actor_consequence(
        s.common, *s.military, *s.military_supply_periods, ref, 1, ordinal,
        s.actual_now));
  auto actual = take(read_exact_military_supply_actor_consequence(
      s.common, *s.military, *s.military_exact, *s.military_supply_periods, ref,
      schema, ordinal, s.actual_now));
  // The same original mouth cannot belong to two actual Army/Garrison supplies
  // in one period, even when one had enough food and therefore no receipt.
  std::size_t count = 0;
  for (const auto &row : s.military_exact->supply_effect_sources.rows())
    if (row.second.period_ref == actual.period_ref) {
      const auto supply = take(read_exact_military_supply_source(
          s.common, *s.military, *s.military_exact, *s.military_supply_periods,
          row.second.original_supply_fact_ref, 2, s.actual_now));
      for (const auto &mouth : supply.original_supply->mouth_ids)
        if (mouth == actual.actor_id)
          ++count;
    }
  if (count != 1)
    fail("MilitarySupplyMouthResponsibilityConflict", actual.actor_id);
  return actual;
}
bool key_matches(const ConditionReceiptV2 &row, const SourceRef &ref,
                 Integer ordinal, const StableId &actor, Integer schema = 2) {
  const auto *key =
      std::get_if<ProvisionShortfallConditionBusinessKey>(&row.business_key);
  return key && same_source_ref(key->original_supply_ref, ref) &&
         key->source_schema == schema && key->source_ordinal == ordinal &&
         key->actor_id == actor &&
         key->purpose == ProvisionShortfallConditionBusinessKey::Purpose::
                             military_supply_shortage;
}
const ConditionReceiptV2 *
existing(const CharacterConditionSources &s,
         const ExactMilitarySupplyActorConsequence &actual,
         Integer schema = 2) {
  const ConditionReceiptV2 *found = nullptr;
  for (const auto &row :
       s.state.character_condition_extensions.applied_receipts.rows())
    if (key_matches(row.second, actual.original_supply_ref,
                    actual.source_ordinal, actual.actor_id, schema)) {
      if (found)
        fail("DuplicateMilitarySupplyCondition", actual.original_supply_ref.id);
      found = &row.second;
    }
  return found;
}
void shape(const CharacterConditionSources &s, const ConditionReceiptV2 &row,
           const ExactMilitarySupplyActorConsequence &actual,
           Integer schema = 2) {
  const auto *cause = std::get_if<ProvisionShortfallConditionCause>(&row.cause);
  if (!cause || cause->actor_id != actual.actor_id ||
      cause->source_schema != schema ||
      cause->source_ordinal != actual.source_ordinal ||
      !same_source_ref(cause->original_supply_ref,
                       actual.original_supply_ref) ||
      row.actor_id != actual.actor_id ||
      !key_matches(row, actual.original_supply_ref, actual.source_ordinal,
                   actual.actor_id, schema))
    fail("MilitarySupplyConditionBusinessKeyMismatch", row.result_id);
  if (!allocated(s.common.state(), row.result_id) ||
      !actor_foundation_rule_equal(
          row.rule_ref, *s.catalog.original_military_supply_shortage_rule) ||
      row.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
      !same_time(row.effective_at.exact_moment, actual.occurred_at) ||
      row.condition_revision_before < 0 ||
      row.condition_revision_before == std::numeric_limits<Integer>::max() ||
      row.condition_revision_after != row.condition_revision_before + 1 ||
      row.scalar_changes.size() != 1 ||
      row.selected_injury_result.kind !=
          SelectedInjuryResult::Kind::not_an_injury ||
      !row.selected_injury_result.injury_rule_ref.id.empty() ||
      row.selected_injury_result.injury_rule_ref.version ||
      !row.selected_injury_result.injury_rule_ref.catalog_digest.empty() ||
      !row.selected_injury_result.result_choice_ref.id.empty() ||
      row.selected_injury_result.result_choice_ref.version ||
      !row.selected_injury_result.result_choice_ref.catalog_digest.empty() ||
      !row.created_effect_keys.empty() ||
      !row.recovery_resolution_refs.empty() || row.life_transition_ref ||
      !original_condition_capacity_refs_shape(s.catalog, row))
    fail("MilitarySupplyConditionReceiptMismatch", row.result_id);
  const auto &delta = row.scalar_changes.front();
  if (delta.field != ConditionScalarChange::Field::fatigue ||
      delta.after != fatigue_after(delta.before))
    fail("MilitarySupplyOriginalFatigueFormulaMismatch", row.result_id);
}
MilitarySupplyConditionApplication
application(const ConditionReceiptV2 &receipt, bool replay,
            const BoundActorConditionWrite *write = nullptr) {
  MilitarySupplyConditionApplication out;
  out.text_memory = CheckpointMemoryCharge(0);
  out.receipt_ref = {receipt.result_id};
  out.already_applied = replay;
  if (write)
    out.actual_writes.push_back(*write);
  return out;
}
} // namespace
Status register_original_military_supply_shortage_rule(
    ActorFoundationCatalog &catalog) {
  if (!selected_profile(catalog))
    return common_failure("MilitarySupplyCondition3SelectionRequired", {},
                          ErrorKind::unsupported_capability);
  ActorFoundationRuleRef expected{original_military_supply_rule_ref, 1,
                                  catalog.identity.content_digest};
  if (catalog.original_military_supply_shortage_rule) {
    return actor_foundation_rule_equal(
               *catalog.original_military_supply_shortage_rule, expected)
               ? common_success()
               : common_failure("MilitarySupplySelectedRuleConflict", {},
                                ErrorKind::internal);
  }
  catalog.original_military_supply_shortage_rule = std::move(expected);
  return common_success();
}
Status
validate_military_supply_shortage_condition(const CharacterConditionSources &s,
                                            const ConditionReceiptV2 &row) {
  try {
    const auto *cause =
        std::get_if<ProvisionShortfallConditionCause>(&row.cause);
    if (!cause)
      fail("MilitarySupplyConditionCauseRequired", row.result_id);
    const auto actual = original(s, cause->original_supply_ref,
                                 cause->source_schema, cause->source_ordinal);
    shape(s, row, actual, cause->source_schema);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<MilitarySupplyConditionApplication>
apply_verified_military_supply_shortage(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    const CharacterConditionSources &s, const SourceRef &ref, Integer ordinal,
    const ReadStamp &stamp, const ReadStampAuthority &authority,
    const DomainReadVersionSource &versions,
    GrowthCapabilityRevisionWrite &capability) {
  try {
    if (&s.common != &common || &s.state != &foundation ||
        !s.permanent_growth ||
        !capability.bound_to(common, *s.permanent_growth))
      fail("MilitarySupplyConditionCandidateMismatch");
    const auto actual = original(s, ref, 2, ordinal);
    take(authority.validate(stamp, common.state().identity,
                            ReadMode::phase_current, common, versions));
    bool actor_read = false, head_read = false;
    for (const auto &r : stamp.resources())
      if (r.resource.kind == ResourceKey::Kind::actor &&
          r.resource.id == actual.actor_id)
        actor_read = true;
    for (const auto &r : stamp.domains())
      if (r.domain == "actor_foundation" && r.kind == "condition_head" &&
          r.id == actual.actor_id)
        head_read = true;
    if (!actor_read || !head_read ||
        std::find(stamp.source_ids().begin(), stamp.source_ids().end(),
                  ref.id) == stamp.source_ids().end() ||
        !same_time(stamp.span().to, actual.occurred_at) ||
        !same_time(s.actual_now, actual.occurred_at))
      fail("MilitarySupplyConditionActualStampRequired", ref.id);
    if (const auto *old = existing(s, actual)) {
      shape(s, *old, actual);
      take(validate_condition_application_fact_for_receipt(common, foundation,
                                                           s.catalog, *old));
      take(read_character_condition_receipt_provenance(s, {old->result_id}));
      if (!old->capability_application)
        fail("MilitarySupplyCapabilityApplicationMissing", old->result_id);
      return {true, application(*old, true), {}};
    }
    auto *head =
        foundation.character_condition_extensions.actor_condition_heads.find(
            actual.actor_id);
    const auto *actor = common.actor(actual.actor_id);
    if (!head || !actor || !actor->alive || head->condition_revision < 0 ||
        head->condition_revision == std::numeric_limits<Integer>::max())
      fail("MilitarySupplyConditionActorRequired", actual.actor_id);
    auto before =
        take(read_character_condition_verified_prefix(s, *head, s.actual_now));
    if (!before.body.alive || before.body.fatigue != actor->fatigue)
      fail("MilitarySupplyConditionBeforeMismatch", actual.actor_id);
    auto capture = common.actor_condition_capture_identity();
    if (!capture)
      fail("MilitarySupplyConditionCaptureRequired", ref.id);
    const auto offset = common.actor_condition_execution_drafts().size();
    if (offset >= static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
      fail("MilitarySupplyConditionCaptureOverflow", ref.id);
    ConditionReceiptV2 row;
    row.result_id = take(common.next_id("condition"));
    row.actor_id = actual.actor_id;
    row.business_key = ProvisionShortfallConditionBusinessKey{
        ref, 2, ordinal, actual.actor_id,
        ProvisionShortfallConditionBusinessKey::Purpose::
            military_supply_shortage};
    row.cause =
        ProvisionShortfallConditionCause{ref, 2, ordinal, actual.actor_id};
    row.rule_ref = *s.catalog.original_military_supply_shortage_rule;
    row.effective_at.exact_moment = actual.occurred_at;
    row.condition_revision_before = head->condition_revision;
    row.condition_revision_after = head->condition_revision + 1;
    row.scalar_changes = {{ConditionScalarChange::Field::fatigue,
                           before.body.fatigue,
                           fatigue_after(before.body.fatigue)}};
    shape(s, row, actual);
    ActorMutation mutation;
    mutation.actor_id = actual.actor_id;
    mutation.fatigue = row.scalar_changes.front().after;
    take(common.apply_actor(mutation));
    const auto id = row.result_id;
    if (!foundation.character_condition_extensions.applied_receipts.insert(
            id, std::move(row)))
      fail("DuplicateConditionReceipt", id);
    head->condition_revision += 1;
    head->latest_receipt_ref = ConditionReceiptRefV2{id};
    if (common.actor_condition_execution_drafts().size() != offset + 1)
      fail("MilitarySupplyConditionActualWriteMismatch", id);
    BoundActorConditionWrite bound{
        {id}, std::move(capture), static_cast<Integer>(offset + 1)};
    take(read_actual_actor_condition_write(common, foundation, bound));
    take(append_condition_application_fact_in_candidate(common, foundation,
                                                        s.catalog, bound));
    take(capability.record_actual_condition_change(bound));
    return {true,
            application(*foundation.character_condition_extensions
                             .applied_receipts.find(id),
                        false, &bound),
            {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
// 原军 feed 已完成整组写入；这里先核真实尾，不把尚未绑定的 current
// 当作已提交历史。旧全世界结构/原源守门在整组真回执落候选后原样调用。
namespace {
struct PendingFieldSupplyCondition {
  Integer ordinal = 0, sequence = 0;
  ActorConditionHead before_head;
  BoundActorConditionWrite bound;
};
// 只核原source、现票和live draft；不写身体/历史，不推算或补造 before。
ActorConditionHead prepare_field_supply_binding(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    const CharacterConditionSources &s, const SourceRef &ref, Integer ordinal,
    Integer sequence, const ReadStamp &stamp,
    const ReadStampAuthority &authority,
    const DomainReadVersionSource &versions,
    GrowthCapabilityRevisionWrite &capability) {
  if (&s.common != &common || &s.state != &foundation || !s.permanent_growth ||
      !capability.bound_to(common, *s.permanent_growth))
    fail("MilitarySupplyConditionCandidateMismatch");
  const auto actual = original(s, ref, 1, ordinal);
  take(authority.validate(stamp, common.state().identity,
                          ReadMode::phase_current, common, versions));
  bool actor_read = false, head_read = false;
  for (const auto &r : stamp.resources())
    if (r.resource.kind == ResourceKey::Kind::actor &&
        r.resource.id == actual.actor_id)
      actor_read = true;
  for (const auto &r : stamp.domains())
    if (r.domain == "actor_foundation" && r.kind == "condition_head" &&
        r.id == actual.actor_id)
      head_read = true;
  if (!actor_read || !head_read ||
      std::find(stamp.source_ids().begin(), stamp.source_ids().end(), ref.id) ==
          stamp.source_ids().end() ||
      !same_time(stamp.span().to, actual.occurred_at) ||
      !same_time(s.actual_now, actual.occurred_at))
    fail("MilitarySupplyConditionActualStampRequired", ref.id);
  if (existing(s, actual, 1))
    fail("MilitarySupplyConditionAlreadyApplied", ref.id);
  const auto capture = common.actor_condition_capture_identity();
  const auto &drafts = common.actor_condition_execution_drafts();
  if (!capture || sequence < 1 ||
      static_cast<std::uintmax_t>(sequence) > drafts.size())
    fail("MilitarySupplyConditionCaptureRequired", ref.id);
  auto *head =
      foundation.character_condition_extensions.actor_condition_heads.find(
          actual.actor_id);
  const auto *actor = common.state().actors.find(actual.actor_id);
  if (!head || !actor || !actor->alive || head->condition_revision < 0 ||
      head->condition_revision == std::numeric_limits<Integer>::max())
    fail("MilitarySupplyConditionActorRequired", actual.actor_id);
  const auto &draft = drafts[static_cast<std::size_t>(sequence - 1)];
  const auto &identity = common.state().identity;
  if (draft.kind != ActorConditionExecutionDraft::Kind::mutation ||
      draft.sequence != sequence || draft.actor_id != actual.actor_id ||
      !draft.before || !draft.fatigue_written || draft.hp_written ||
      draft.injury_written || draft.alive_written || draft.available_written ||
      draft.identity.world_id != identity.world_id ||
      draft.identity.execution_epoch != identity.execution_epoch ||
      draft.identity.revision != identity.revision ||
      draft.identity.tick != identity.tick ||
      draft.next_instance_before != draft.next_instance_after ||
      !draft.before->alive || draft.after.hp != draft.before->hp ||
      draft.after.injury != draft.before->injury ||
      draft.after.alive != draft.before->alive ||
      draft.after.available != draft.before->available ||
      draft.after.fatigue != fatigue_after(draft.before->fatigue) ||
      actor->hp != draft.after.hp || actor->fatigue != draft.after.fatigue ||
      actor->injury != draft.after.injury ||
      actor->alive != draft.after.alive ||
      actor->available != draft.after.available)
    fail("FieldMilitarySupplyActualWriteMismatch", ref.id);
  for (std::size_t n = static_cast<std::size_t>(sequence); n < drafts.size();
       ++n)
    if (drafts[n].actor_id == actual.actor_id)
      fail("FieldMilitarySupplyActualWriteSuperseded", ref.id);
  return *head;
}
// 同一个原头才可继续；此值是本调用栈的历史cut，不是可复活的读票。
bool same_field_supply_head(const ActorConditionHead &a,
                            const ActorConditionHead &b) {
  return a.actor_id == b.actor_id &&
         a.condition_revision == b.condition_revision &&
         a.initial_condition_seed_ref.seed_key ==
             b.initial_condition_seed_ref.seed_key &&
         bool(a.latest_receipt_ref) == bool(b.latest_receipt_ref) &&
         (!a.latest_receipt_ref ||
          a.latest_receipt_ref->result_id == b.latest_receipt_ref->result_id);
}
} // namespace
// 整个原Supply尾按 capture 次序原子绑定；中间态不作为已发布能力返回。
// 任一步拒绝由既有外层 Common+军事+人物整候选撤回，不创建第二保存点。
Outcome<CheckpointVector<BoundActorConditionWrite>>
bind_field_military_supply_shortages(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    const CharacterConditionWorldRead &body, const SourceRef &ref,
    Integer first_sequence, const CurrentReadStampRead &current,
    ReadStampAuthority &authority, const DomainReadVersionSource &versions,
    GrowthCapabilityRevisionWrite &capability) {
  try {
    const auto capture = common.actor_condition_capture_identity();
    const auto &drafts = common.actor_condition_execution_drafts();
    const auto draft_count = drafts.size();
    if (!capture || first_sequence < 1 ||
        static_cast<std::uintmax_t>(first_sequence - 1) > draft_count)
      fail("MilitarySupplyConditionCaptureRequired", ref.id);
    CheckpointMemoryCharge text_memory(0);
    CheckpointVector<PendingFieldSupplyCondition> pending;
    // 第一遍不写：完整原mouth顺序与未消费capture尾一一对应。
    {
      const auto sources = take(body.sources());
      if (&sources.common != &common || &sources.state != &foundation)
        fail("MilitarySupplyConditionCandidateMismatch");
      selected(sources, 1);
      const auto original_supply = take(read_field_military_supply_source(
          common, *sources.military, *sources.military_supply_periods, ref, 1,
          sources.actual_now));
      if (!original_supply.original_supply ||
          original_supply.original_supply->shortage <= 0)
        fail("FieldSupplyActualShortageRequired", ref.id);
      const auto &mouths = original_supply.original_supply->mouth_ids;
      for (std::size_t ordinal = 0; ordinal < mouths.size(); ++ordinal) {
        if (!common.actor(mouths[ordinal]))
          continue;
        if (ordinal >
                static_cast<std::size_t>(std::numeric_limits<Integer>::max()) ||
            pending.size() >
                static_cast<std::size_t>(std::numeric_limits<Integer>::max() -
                                         first_sequence))
          fail("FieldSupplyCaptureSequenceOverflow", ref.id);
        for (const auto &prior : pending)
          if (prior.before_head.actor_id == mouths[ordinal])
            fail("MilitarySupplyMouthResponsibilityConflict", mouths[ordinal]);
        const auto sequence =
            first_sequence + static_cast<Integer>(pending.size());
        ReadStampWriteBoundary boundary(authority);
        auto stamp = take(current.capture(
            {{ResourceKey::Kind::actor, mouths[ordinal]}},
            {{"actor_foundation", "condition_head", mouths[ordinal]}},
            {ref.id}));
        if (!stamp)
          fail("MilitarySupplyConditionActualStampRequired", ref.id);
        auto actual_body = take(body.sources());
        auto head = prepare_field_supply_binding(
            common, foundation, actual_body, ref, static_cast<Integer>(ordinal),
            sequence, *stamp, authority, versions, capability);
        pending.push_back(
            {static_cast<Integer>(ordinal), sequence, std::move(head), {}});
      }
    }
    if (static_cast<std::uintmax_t>(first_sequence - 1) + pending.size() !=
        draft_count)
      fail("FieldSupplyActualTailSizeMismatch", ref.id);
    // 第二遍每个writer重新借同世界/现票。这里只落已经发生的真实身体写，
    // 不在别的mouth尚待绑定时调用要求全current闭合的reader。
    for (auto &item : pending) {
      ReadStampWriteBoundary boundary(authority);
      auto stamp = take(current.capture(
          {{ResourceKey::Kind::actor, item.before_head.actor_id}},
          {{"actor_foundation", "condition_head", item.before_head.actor_id}},
          {ref.id}));
      if (!stamp)
        fail("MilitarySupplyConditionActualStampRequired", ref.id);
      auto s = take(body.sources());
      const auto before = prepare_field_supply_binding(
          common, foundation, s, ref, item.ordinal, item.sequence, *stamp,
          authority, versions, capability);
      if (!same_field_supply_head(before, item.before_head) ||
          common.actor_condition_capture_identity() != capture ||
          drafts.size() != draft_count)
        fail("FieldSupplyActualBindingChanged", ref.id);
      const auto actual = original(s, ref, 1, item.ordinal);
      const auto &draft = drafts[static_cast<std::size_t>(item.sequence - 1)];
      ConditionReceiptV2 row;
      row.result_id = take(common.next_id("condition"));
      row.actor_id = actual.actor_id;
      row.business_key = ProvisionShortfallConditionBusinessKey{
          ref, 1, item.ordinal, actual.actor_id,
          ProvisionShortfallConditionBusinessKey::Purpose::
              military_supply_shortage};
      row.cause = ProvisionShortfallConditionCause{ref, 1, item.ordinal,
                                                   actual.actor_id};
      row.rule_ref = *s.catalog.original_military_supply_shortage_rule;
      row.effective_at.exact_moment = actual.occurred_at;
      row.condition_revision_before = before.condition_revision;
      row.condition_revision_after = before.condition_revision + 1;
      row.scalar_changes = {{ConditionScalarChange::Field::fatigue,
                             draft.before->fatigue, draft.after.fatigue}};
      shape(s, row, actual, 1);
      const auto id = row.result_id;
      if (!foundation.character_condition_extensions.applied_receipts.insert(
              id, std::move(row)))
        fail("DuplicateConditionReceipt", id);
      auto *head =
          foundation.character_condition_extensions.actor_condition_heads.find(
              actual.actor_id);
      if (!head || !same_field_supply_head(*head, before))
        fail("FieldSupplyActualBindingChanged", ref.id);
      ++head->condition_revision;
      head->latest_receipt_ref = ConditionReceiptRefV2{id};
      item.bound = {{id}, capture, item.sequence};
      take(read_actual_actor_condition_write(common, foundation, item.bound));
      take(append_condition_application_fact_in_candidate(
          common, foundation, s.catalog, item.bound));
    }
    // 此时整组真实receipt/header齐全：沿原严格全current和每条原source
    // 回放保存的before cut，再与实际draft.before逐字段相等，不用after减四。
    for (const auto &item : pending) {
      auto s = take(body.sources());
      const auto before = take(read_character_condition_verified_prefix(
          s, item.before_head, s.actual_now));
      const auto &draft = drafts[static_cast<std::size_t>(item.sequence - 1)];
      if (!draft.before || !before.body.alive ||
          draft.before->alive != before.body.alive ||
          draft.before->hp != before.body.hp ||
          draft.before->fatigue != before.body.fatigue ||
          draft.before->injury != before.body.injury)
        fail("FieldMilitarySupplyActualWriteMismatch", ref.id);
      take(read_actual_actor_condition_write(common, foundation, item.bound));
    }
    CheckpointVector<BoundActorConditionWrite> result;
    for (auto &item : pending) {
      // 原consumer只在全组来源/历史闭合后调用；不会在首mouth吞掉别人的待绑定写。
      take(capability.record_actual_condition_change(item.bound));
      result.push_back(std::move(item.bound));
    }
    if (common.actor_condition_capture_identity() != capture ||
        drafts.size() != draft_count)
      fail("FieldSupplyActualBindingChanged", ref.id);
    return {true, std::move(result), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}

Status
validate_military_supply_shortage_coverage(const CharacterConditionSources &s) {
  try {
    for (const auto &row :
         s.state.character_condition_extensions.applied_receipts.rows())
      if (std::holds_alternative<ProvisionShortfallConditionCause>(
              row.second.cause))
        take(validate_military_supply_shortage_condition(s, row.second));
    if (field_profile(s)) {
      selected(s, 1);
      for (const auto &supply : s.military->supply_facts) {
        const SourceRef ref{"military", "military_supply", supply.id};
        const auto source = take(read_field_military_supply_source(
            s.common, *s.military, *s.military_supply_periods, ref, 1,
            s.actual_now));
        if (source.original_supply->shortage <= 0)
          continue;
        for (std::size_t n = 0; n < supply.mouth_ids.size(); ++n) {
          if (!s.common.actor(supply.mouth_ids[n]))
            continue;
          if (n > static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
            fail("MilitarySupplyOrdinalOverflow", supply.id);
          const auto actual = original(s, ref, 1, static_cast<Integer>(n));
          const auto *receipt = existing(s, actual, 1);
          if (!receipt)
            fail("MilitarySupplyConditionCoverageMissing", supply.id);
          shape(s, *receipt, actual, 1);
          if (!receipt->capability_application)
            fail("MilitarySupplyCapabilityApplicationMissing",
                 receipt->result_id);
        }
      }
      return common_success();
    }
    if (!s.military_exact || s.military_exact->supply_effect_sources.empty())
      return common_success();
    selected(s);
    for (const auto &row : s.military_exact->supply_effect_sources.rows()) {
      const auto source = take(read_exact_military_supply_source(
          s.common, *s.military, *s.military_exact, *s.military_supply_periods,
          row.second.original_supply_fact_ref, 2, s.actual_now));
      if (source.original_supply->shortage <= 0)
        continue;
      for (std::size_t n = 0; n < source.original_supply->mouth_ids.size(); ++n)
        if (s.common.actor(source.original_supply->mouth_ids[n])) {
          if (n > static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
            fail("MilitarySupplyOrdinalOverflow", row.first);
          const auto actual = original(s, row.second.original_supply_fact_ref,
                                       2, static_cast<Integer>(n));
          const auto *receipt = existing(s, actual);
          if (!receipt)
            fail("MilitarySupplyConditionCoverageMissing", row.first);
          shape(s, *receipt, actual);
          if (!receipt->capability_application)
            fail("MilitarySupplyCapabilityApplicationMissing",
                 receipt->result_id);
        }
    }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
RequiredSourceConsumerSlot military_supply_condition_source_consumer_slot() {
  return {"actor_foundation", "condition_receipt", "military_supply_shortage"};
}
Status visit_military_supply_condition_source_pins(
    const CharacterConditionSources &s, MilitarySupplyConditionPinSink &sink) {
  try {
    take(validate_military_supply_shortage_coverage(s));
    for (const auto &row :
         s.state.character_condition_extensions.applied_receipts.rows())
      if (const auto *cause =
              std::get_if<ProvisionShortfallConditionCause>(&row.second.cause))
        take(sink.actual_pin({{"actor_foundation", "condition_receipt",
                               row.first, "military_supply_shortage"},
                              cause->original_supply_ref,
                              SourcePinReason::unique_outcome}));
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_military_supply_condition_source_pin(
    const CharacterConditionSources &s, const SourcePin &pin) {
  struct Find final : MilitarySupplyConditionPinSink {
    const SourcePin &target;
    std::size_t count = 0;
    explicit Find(const SourcePin &p) : target(p) {}
    Status actual_pin(const SourcePin &p) override {
      if (p.consumer.domain == target.consumer.domain &&
          p.consumer.kind == target.consumer.kind &&
          p.consumer.id == target.consumer.id &&
          p.consumer.slot == target.consumer.slot &&
          same_source_ref(p.source, target.source) && p.reason == target.reason)
        ++count;
      return common_success();
    }
  } find(pin);
  auto check = visit_military_supply_condition_source_pins(s, find);
  if (!check.ok)
    return check;
  return find.count == 1 ? common_success()
                         : common_failure("MilitarySupplyConditionPinNotActual",
                                          pin.consumer.id, ErrorKind::internal);
}
NativeMilitarySupplyConditionWrite::NativeMilitarySupplyConditionWrite(
    CommonStore &c, AuthoredActorFoundationState &f, const MilitaryState &m,
    const MilitaryExactState &e, const ExactTrainingPeriodRead &p,
    const CharacterConditionWorldRead &b, const CurrentReadStampRead &s,
    const ReadStampAuthority &a, const DomainReadVersionSource &v,
    GrowthCapabilityRevisionWrite &g, std::vector<BoundActorConditionWrite> &w)
    : common_(c), foundation_(f), military_(m), exact_(e), periods_(p),
      body_(b), stamps_(s), authority_(a), versions_(v), capability_(g),
      writes_(w) {}
bool NativeMilitarySupplyConditionWrite::bound_to(
    const CommonStore &c, const MilitaryState &m,
    const MilitaryExactState &e) const noexcept {
  return &c == &common_ && &m == &military_ && &e == &exact_;
}
Status
NativeMilitarySupplyConditionWrite::apply_actual_supply_conditions_in_candidate(
    const SourceRef &ref) {
  try {
    const auto now = take(stamps_.read_current_moment());
    const auto original_source = take(read_exact_military_supply_source(
        common_, military_, exact_, periods_, ref, 2, now));
    if (!same_time(original_source.exact_source->occurred_at, now))
      fail("MilitarySupplyConditionOriginalMomentRequired", ref.id);
    if (original_source.original_supply->shortage <= 0)
      return common_success();
    for (std::size_t n = 0;
         n < original_source.original_supply->mouth_ids.size(); ++n) {
      const auto &actor = original_source.original_supply->mouth_ids[n];
      if (!common_.actor(actor))
        continue;
      if (n > static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
        fail("MilitarySupplyOrdinalOverflow", ref.id);
      auto body = take(body_.sources());
      if (&body.common != &common_ || &body.state != &foundation_ ||
          body.military != &military_ || body.military_exact != &exact_ ||
          body.military_supply_periods != &periods_ ||
          !same_time(body.actual_now, now))
        fail("MilitarySupplyConditionOwningLeaseMismatch", ref.id);
      auto stamp = take(stamps_.capture(
          {{ResourceKey::Kind::actor, actor}},
          {{"actor_foundation", "condition_head", actor}}, {ref.id}));
      auto applied = take(apply_verified_military_supply_shortage(
          common_, foundation_, body, ref, static_cast<Integer>(n), *stamp,
          authority_, versions_, capability_));
      if (applied.already_applied) {
        if (!applied.actual_writes.empty())
          fail("MilitarySupplyReplayHasWrites", ref.id);
      } else {
        if (applied.actual_writes.size() != 1)
          fail("MilitarySupplyActualWriteMissing", ref.id);
        writes_.push_back(std::move(applied.actual_writes.front()));
      }
    }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
