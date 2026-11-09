#include "resident/kernel/catalog_section_observation.hpp"
#include "character_capability.hpp"
#include "injury_grade_effects_v3.hpp"
#include "injury_capability_map.hpp"
#include "authored_origin.hpp"
#include "character_capability_replay.hpp"
#include "character_condition_replay.hpp"
#include "condition_application_fact.hpp"
#include "condition_application_history.hpp"
#include "facility_construction_capability.hpp"
#include "family_communication_capability.hpp"
#include "family_personal_capability.hpp"
#include "family_personal_capability_history.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw Failure{common_failure(code, id, kind).error};
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
CharacterConditionSourceRef source_for(const CharacterConditionSources &sources,
                                       const ActorConditionHead &head,
                                       ConditionScalarChange::Field field) {
  CharacterConditionSourceRef out = head.initial_condition_seed_ref;
  for (const auto &row :
       sources.state.character_condition_extensions.applied_receipts.rows()) {
    const auto &receipt = row.second;
    if (receipt.actor_id != head.actor_id ||
        receipt.condition_revision_after > head.condition_revision)
      continue;
    for (const auto &delta : receipt.scalar_changes)
      if (delta.field == field)
        out = ConditionReceiptRefV2{receipt.result_id};
  }
  return out;
}
bool same_source(const CharacterConditionSourceRef &a,
                 const CharacterConditionSourceRef &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *seed = std::get_if<InitialConditionSeedRef>(&a))
    return seed->seed_key == std::get<InitialConditionSeedRef>(b).seed_key;
  return std::get<ConditionReceiptRefV2>(a).result_id ==
         std::get<ConditionReceiptRefV2>(b).result_id;
}
void limiting(CharacterCapabilityView &value, CharacterConditionSourceRef ref) {
  for (const auto &old : value.limiting_condition_refs)
    if (same_source(old, ref))
      return;
  value.limiting_condition_refs.push_back(std::move(ref));
}
bool work_purpose(const StableId &id) {
  return id == "growth_service_worker" || id == "private_shop_operation" ||
         id == "cargo_handling_worker" ||
         id == "economic_household_service_worker" ||
         id == "economic_survey_worker" || id == "economic_gather_worker" ||
         id == "economic_cultivation_worker" || id == "craft_repair_worker" ||
         id == "craft_job_worker" || id == "facility_construction_worker" ||
         id == "s3_fixed_site_inspector" || id == "s3_fixed_work_supervisor" ||
         id == "facility_assisted_clearance_worker";
}
bool expression_purpose(const StableId &id) {
  return id == "family_personal_decision" || id == "character_expression" ||
         id == "s3_fixed_conversation_participant" ||
         id == "facility_self_vacate" || id == "facility_transport_acceptance";
}
// 原表达目的复用唯一应用历史选择器，保旧1并严格核新2完整局部组合。
// 这里只登记原 reducer；作者根/形成来源/完整身份仍由中央安装门证明。
bool family_personal_selected(const ActorFoundationCatalog &catalog) {
  const auto selected = condition_application_facts_selected(
      catalog.identity, ConditionApplicationHistoryOwner::family_autonomy);
  return selected.ok && selected.value;
}

// 内部具名桥只属实际已选History2/Time2原交易，不扩作者reducer字符串。
bool growth_trade_participation_selected(
    const ActorFoundationCatalog &catalog) {
  if (!catalog.original_growth || !catalog.original_growth->regional_life ||
      !catalog.original_growth->economy ||
      !catalog.original_growth->complete_study_sources)
    return false;
  const auto selected = [&](std::string_view name, std::string_view version) {
    std::size_t count = 0;
    for (const auto &module : catalog.identity.module_versions)
      if (module.first == name) {
        if (module.second != version)
          return false;
        ++count;
      }
    return count == 1;
  };
  return selected("actor_start_context", "3") &&
         selected("growth_market_history", "2") &&
         selected("world_timeline", "2") && selected("action_elapsed", "2") &&
         selected("selected_resource_operations", "1");
}
bool original_binding_allowed(const FrozenBodyScalars &body,
                              CharacterPurposeBinding::Reducer reducer) {
  return body.alive &&
         (reducer == CharacterPurposeBinding::Reducer::family_care ||
          reducer == CharacterPurposeBinding::Reducer::trade_participation ||
          body.hp > 0) &&
         (reducer != CharacterPurposeBinding::Reducer::personal_training ||
          (body.fatigue < 90 && body.injury < 3));
}
CharacterCapabilityView reduce(const CharacterConditionSources &sources,
                               const CharacterConditionAtHead &body,
                               const ActorFoundationRuleRef &purpose,
                               const std::optional<StableId> &skill) {
  if (purpose.catalog_digest != sources.catalog.identity.content_digest ||
      purpose.version != 1)
    fail("UnsupportedActorCapabilityPurpose", purpose.id,
         ErrorKind::unsupported_capability);
  CharacterCapabilityView result;
  result.actor_id = body.head.actor_id;
  result.purpose_ref = purpose;
  result.condition_head = body.head;
  result.body = body.body;
  result.observed_at = body.observed_at;
  if (purpose.id == "long_rest") {
    if (skill)
      fail("RestCapabilitySkillUnexpected", result.actor_id);
    if (!sources.catalog.original_long_rest_rule)
      fail("OriginalLongRestConditionRuleRequired", result.actor_id,
           ErrorKind::unsupported_capability);
    result.rule_ref = sources.catalog.original_long_rest_rule->rule_ref;
    result.allowed = body.body.alive;
    result.efficiency_bp = result.allowed ? 10000 : 0;
    if (!result.allowed)
      limiting(result, source_for(sources, body.head,
                                  ConditionScalarChange::Field::alive));
    return result;
  }
  if (actor_foundation_rule_equal(purpose, sources.catalog.rest_purpose) ||
      actor_foundation_rule_equal(purpose, sources.catalog.provider_purpose)) {
    if (skill)
      fail("RestCapabilitySkillUnexpected", result.actor_id);
    result.rule_ref = sources.catalog.rest_rule;
    result.allowed = body.body.alive;
    result.efficiency_bp = result.allowed ? 10000 : 0;
    if (!result.allowed)
      limiting(result, source_for(sources, body.head,
                                  ConditionScalarChange::Field::alive));
    return result;
  }
  if (const auto *binding =
          sources.catalog.capability_purpose_bindings.find(purpose.id)) {
    take(validate_character_capability_catalog(sources.catalog));
    if (!actor_foundation_rule_equal(binding->purpose_ref, purpose) || skill)
      fail("CharacterPurposeBindingMismatch", purpose.id);
    result.rule_ref = binding->rule_ref;
    result.allowed = original_binding_allowed(body.body, binding->reducer);
    if (injury_grade_effects_v3_selected(sources.catalog) &&
        binding->reducer == CharacterPurposeBinding::Reducer::normal_work &&
        body.body.injury == 3) {
      take(validate_injury_capability_map(sources.catalog));
      result.allowed = false;
      limiting(result, source_for(sources, body.head,
                                  ConditionScalarChange::Field::injury));
    }
    if (binding->reducer == CharacterPurposeBinding::Reducer::family_care ||
        binding->reducer ==
            CharacterPurposeBinding::Reducer::trade_participation) {
      result.efficiency_bp = result.allowed ? 10000 : 0;
      if (!body.body.alive)
        limiting(result, source_for(sources, body.head,
                                    ConditionScalarChange::Field::alive));
      return result;
    }
    if (!body.body.alive)
      limiting(result, source_for(sources, body.head,
                                  ConditionScalarChange::Field::alive));
    if (body.body.hp <= 0)
      limiting(result, source_for(sources, body.head,
                                  ConditionScalarChange::Field::hp));
    // Original learn/practice/teach_skill gates, distinct from sixteen-skill
    // study XP.
    if (binding->reducer ==
        CharacterPurposeBinding::Reducer::personal_training) {
      if (body.body.fatigue >= 90) {
        result.allowed = false;
        limiting(result, source_for(sources, body.head,
                                    ConditionScalarChange::Field::fatigue));
      }
      if (body.body.injury >= 3) {
        result.allowed = false;
        limiting(result, source_for(sources, body.head,
                                    ConditionScalarChange::Field::injury));
      }
      result.efficiency_bp = result.allowed ? 10000 : 0;
      return result;
    }
    if (binding->reducer == CharacterPurposeBinding::Reducer::expression) {
      result.efficiency_bp = result.allowed ? 10000 : 0;
      return result;
    }
    const auto &work = *sources.catalog.normal_work_efficiency_rule;
    std::size_t band = 0;
    while (band < work.fatigue_cutoffs.size() &&
           body.body.fatigue >= work.fatigue_cutoffs[band])
      ++band;
    if (body.body.injury < 0 || body.body.injury > 3)
      fail("CharacterWorkInjuryRange", result.actor_id);
    const auto fatigue = work.fatigue_caps[band],
               injury =
                   work.injury_caps[static_cast<std::size_t>(body.body.injury)];
    result.registered_efficiency_components = {
        {CharacterEfficiencyComponent::Kind::fatigue,
         source_for(sources, body.head, ConditionScalarChange::Field::fatigue),
         work.rule_ref, fatigue},
        {CharacterEfficiencyComponent::Kind::ordinary_injury,
         source_for(sources, body.head, ConditionScalarChange::Field::injury),
         work.rule_ref, injury}};
    result.efficiency_bp = result.allowed ? std::min(fatigue, injury) : 0;
    for (const auto &component : result.registered_efficiency_components)
      if (component.cap_bp < 10000)
        limiting(result, component.original_source_ref);
    return result;
  }
  if (purpose.id != "study" && purpose.id != "provide_teaching")
    fail("UnsupportedActorCapabilityPurpose", purpose.id,
         ErrorKind::unsupported_capability);
  const auto &rule = sources.catalog.study_capability_rule;
  if (!rule || rule->id != "study_condition_credit_v2" || rule->version != 2 ||
      rule->catalog_digest != sources.catalog.identity.content_digest)
    fail("StudyCapabilityRuleRequired", purpose.id,
         ErrorKind::unsupported_capability);
  if (!sources.shared)
    fail("StudyCapabilityCatalogRequired", result.actor_id,
         ErrorKind::unsupported_capability);
  if (!skill || !growth_skill_defined(*skill, sources.shared->extended_skills))
    fail("StudyCapabilitySkillRequired", result.actor_id);
  // Consume the actual original Study reducer; retain its separate fatigue and
  // injury evidence without copying the numeric rule or borrowing Work policy.
  if (body.body.injury < 0 || body.body.injury > 3)
    fail("StudyCapabilityInjuryRange", result.actor_id);
  const auto original =
      growth::original_study_efficiency(body.body.fatigue, body.body.injury);
  const Integer fatigue = original.fatigue_cap_bp,
                injury = original.injury_cap_bp;
  result.rule_ref = *rule;
  result.registered_efficiency_components.reserve(2);
  result.registered_efficiency_components.push_back(
      {CharacterEfficiencyComponent::Kind::fatigue,
       source_for(sources, body.head, ConditionScalarChange::Field::fatigue),
       *rule, fatigue});
  result.registered_efficiency_components.push_back(
      {CharacterEfficiencyComponent::Kind::ordinary_injury,
       source_for(sources, body.head, ConditionScalarChange::Field::injury),
       *rule, injury});
  result.allowed = body.body.alive && body.body.hp > 0;
  if (!body.body.alive)
    limiting(result, source_for(sources, body.head,
                                ConditionScalarChange::Field::alive));
  if (body.body.hp <= 0)
    limiting(result,
             source_for(sources, body.head, ConditionScalarChange::Field::hp));
  if (body.body.injury == 3 && (*skill == "infantry" || *skill == "cavalry" ||
                                *skill == "archery" || *skill == "martial")) {
    result.allowed = false;
    limiting(result, source_for(sources, body.head,
                                ConditionScalarChange::Field::injury));
  }
  result.efficiency_bp = result.allowed ? original.efficiency_bp : 0;
  for (const auto &component : result.registered_efficiency_components)
    if (component.cap_bp < 10000)
      limiting(result, component.original_source_ref);
  return result;
}
} // namespace
Outcome<CharacterCapabilityView> read_character_capability_at_head(
    const CharacterConditionSources &sources, const ActorConditionHead &head,
    const ActorFoundationRuleRef &purpose, const std::optional<StableId> &skill,
    const WorldMoment &boundary) {
  return guarded<CharacterCapabilityView>([&] {
    return reduce(
        sources,
        take(read_character_condition_at_head(sources, head, boundary)),
        purpose, skill);
  });
}
Outcome<CharacterCapabilityView> read_character_capability(
    const CharacterConditionSources &sources, const StableId &actor,
    const ActorFoundationRuleRef &purpose, const std::optional<StableId> &skill,
    const ReadStamp &stamp, const ReadStampAuthority &authority,
    const DomainReadVersionSource &versions) {
  return guarded<CharacterCapabilityView>([&] {
    if (stamp.mode() == ReadMode::committed_history)
      fail("CurrentCharacterCapabilityStampRequired", actor);
    take(authority.validate(stamp, sources.common.state().identity,
                            stamp.mode(), sources.common, versions));
    bool actor_version = false, condition_version = false;
    for (const auto &version : stamp.resources())
      if (version.resource.kind == ResourceKey::Kind::actor &&
          version.resource.id == actor)
        actor_version = true;
    const auto *head =
        sources.state.character_condition_extensions.actor_condition_heads.find(
            actor);
    if (!head)
      fail("CharacterConditionHeadMissing", actor);
    for (const auto &version : stamp.domains())
      if (version.domain == "actor_foundation" &&
          version.kind == "condition_head" && version.id == actor &&
          version.revision ==
              static_cast<std::uint64_t>(head->condition_revision))
        condition_version = true;
    if (!actor_version || !condition_version ||
        take(compare_world_moments(stamp.span().to, sources.actual_now)))
      fail("CharacterCapabilityReadStampIncomplete", actor);
    return reduce(sources,
                  take(read_current_character_condition(sources, actor)),
                  purpose, skill);
  });
}
Outcome<FrozenFacilityCapabilityInputs>
freeze_character_capability(const CharacterCapabilityView &value) {
  return guarded<FrozenFacilityCapabilityInputs>([&] {
    if (value.actor_id.empty() ||
        value.actor_id != value.condition_head.actor_id ||
        value.efficiency_bp < 0 || value.efficiency_bp > 10000 ||
        (!value.allowed && value.efficiency_bp != 0))
      fail("CharacterCapabilityValueInvalid", value.actor_id);
    FrozenFacilityCapabilityInputs out;
    out.actor_id = value.actor_id;
    out.purpose_ref = value.purpose_ref;
    out.captured_at = value.observed_at;
    out.condition_head_snapshot = value.condition_head;
    out.body_scalars = value.body;
    out.selected_rule_refs = {value.rule_ref};
    out.result.allowed = value.allowed;
    out.result.efficiency_bp = value.efficiency_bp;
    out.result.rule_ref = value.rule_ref;
    out.result.observed_at = value.observed_at;
    // 完整冻结保全部真实seed/receipt限制源；旧seed-only物理入口显式拒新分支。
    out.result.limiting_condition_refs = value.limiting_condition_refs;
    out.field_order = {"actor_id",           "purpose_ref",
                       "captured_at",        "condition_head_snapshot",
                       "body_scalars",       "active_condition_inputs",
                       "selected_rule_refs", "result",
                       "field_order"};
    return out;
  });
}
Status
validate_character_capability_catalog(const ActorFoundationCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 5, "ActorFoundationCatalog", "validate_character_capability_catalog", &catalog);

  return guarded<Done>([&] {
    const auto valid_ref = [&](const ActorFoundationRuleRef &ref) {
      return !ref.id.empty() && ref.version == 1 &&
             ref.catalog_digest == catalog.identity.content_digest;
    };
    if (catalog.normal_work_efficiency_rule) {
      const auto &r = *catalog.normal_work_efficiency_rule;
      if (!valid_ref(r.rule_ref) ||
          r.rule_ref.id != "original_normal_work_efficiency")
        fail("CharacterNormalWorkRuleRequired");
      for (std::size_t n = 0; n < r.fatigue_cutoffs.size(); ++n)
        if (r.fatigue_cutoffs[n] < 1 || r.fatigue_cutoffs[n] > 100 ||
            (n && r.fatigue_cutoffs[n - 1] >= r.fatigue_cutoffs[n]))
          fail("CharacterNormalWorkThresholdsInvalid");
      constexpr std::array<Integer, 4> original_caps{10000, 9000, 7500, 6000};
      if (r.fatigue_caps != original_caps || r.injury_caps != original_caps)
        fail("CharacterNormalWorkCapsMismatch");
    }
    for (const auto &row : catalog.capability_purpose_bindings.rows()) {
      const auto &b = row.second;
      if (row.first != b.purpose_ref.id || !valid_ref(b.purpose_ref) ||
          !valid_ref(b.rule_ref))
        fail("CharacterPurposeBindingInvalid", row.first);
      if (work_purpose(row.first)) {
        if (b.reducer != CharacterPurposeBinding::Reducer::normal_work ||
            !catalog.normal_work_efficiency_rule ||
            !actor_foundation_rule_equal(
                b.rule_ref, catalog.normal_work_efficiency_rule->rule_ref))
          fail("CharacterWorkPurposeRuleRequired", row.first);
      } else if (row.first == "personal_training_participant") {
        if (b.reducer != CharacterPurposeBinding::Reducer::personal_training ||
            b.rule_ref.id != "original_personal_training_participation")
          fail("CharacterPersonalTrainingRuleRequired", row.first);
      } else if (row.first == "growth_trade_participant") {
        if (!growth_trade_participation_selected(catalog) ||
            b.reducer !=
                CharacterPurposeBinding::Reducer::trade_participation ||
            b.rule_ref.id != "original_growth_trade_participation")
          fail("GrowthTradeParticipationRuleRequired", row.first);
      } else if (row.first == "care_participant") {
        bool care = false, condition = false;
        for (const auto &module : catalog.identity.module_versions) {
          if (module.first == "care_requirements") {
            if (care || (module.second != "5" && module.second != "6"))
              fail("CareParticipationSelectedSourcesRequired", row.first,
                   ErrorKind::unsupported_capability);
            care = true;
          }
          if (module.first == "character_condition_extensions") {
            if (condition || module.second != "4")
              fail("CareParticipationSelectedSourcesRequired", row.first,
                   ErrorKind::unsupported_capability);
            condition = true;
          }
        }
        if (!care || !condition)
          fail("CareParticipationSelectedSourcesRequired", row.first,
               ErrorKind::unsupported_capability);
        if (b.reducer != CharacterPurposeBinding::Reducer::family_care ||
            b.rule_ref.id != "original_family_care_alive")
          fail("CharacterCareParticipationRuleRequired", row.first);
      } else if (expression_purpose(row.first)) {
        if (row.first == "family_personal_decision" &&
            !family_personal_selected(catalog))
          fail("FamilyPersonalDecisionOriginalSelectionRequired", row.first,
               ErrorKind::unsupported_capability);
        if (b.reducer != CharacterPurposeBinding::Reducer::expression ||
            b.rule_ref.id != "original_alive_hp_purpose_limit")
          fail("CharacterExpressionPurposeRuleRequired", row.first);
      } else
        fail("UnsupportedCharacterPurposeBinding", row.first,
             ErrorKind::unsupported_capability);
    }
    return Done{};
  });
}
} // namespace hanmo::resident
namespace hanmo::resident {
Outcome<CharacterCapabilityView> read_character_capability_for_span(
    const CharacterConditionSources &sources, const ActorConditionHead &head,
    const ActorFoundationRuleRef &purpose, const std::optional<StableId> &skill,
    const MomentSpan &span) {
  return guarded<CharacterCapabilityView>([&] {
    if (take(compare_world_moments(span.from, span.to)) >= 0 ||
        take(compare_world_moments(span.to, sources.actual_now)) > 0)
      fail("CharacterCapabilitySpanInvalid", head.actor_id);
    auto result = take(read_character_capability_at_head(sources, head, purpose,
                                                         skill, span.from));
    for (const auto &row :
         sources.state.character_condition_extensions.applied_receipts.rows()) {
      const auto &receipt = row.second;
      if (receipt.actor_id != head.actor_id ||
          receipt.condition_revision_after <= head.condition_revision)
        continue;
      if (receipt.effective_at.kind != ActorFoundationAt::Kind::exact_moment)
        fail("CharacterCapabilityExactChangeRequired", row.first);
      if (take(compare_world_moments(receipt.effective_at.exact_moment,
                                     span.to)) < 0)
        fail("CharacterCapabilitySpanRequiresActualSplit", row.first);
    }
    return result;
  });
}
} // namespace hanmo::resident
namespace hanmo::resident {
Status visit_character_capability_span(
    const CharacterConditionSources &sources, const ActorConditionHead &start,
    const ActorFoundationRuleRef &purpose, const std::optional<StableId> &skill,
    const MomentSpan &span, CharacterCapabilitySpanVisitor &visitor) {
  return guarded<Done>([&] {
    if (take(compare_world_moments(span.from, span.to)) >= 0 ||
        take(compare_world_moments(span.to, sources.actual_now)) > 0)
      fail("CharacterCapabilitySpanInvalid", start.actor_id);
    // 不是签发瞬间的同M截面：正服务窗必须采用from已应用的全部前驱。
    auto actual = take(
        read_character_condition_at_moment(sources, start.actor_id, span.from));
    if (actual.head.condition_revision != start.condition_revision ||
        actual.head.initial_condition_seed_ref.seed_key !=
            start.initial_condition_seed_ref.seed_key ||
        bool(actual.head.latest_receipt_ref) !=
            bool(start.latest_receipt_ref) ||
        (start.latest_receipt_ref &&
         actual.head.latest_receipt_ref->result_id !=
             start.latest_receipt_ref->result_id))
      fail("CharacterCapabilitySpanStartHeadMismatch", start.actor_id);
    ActorConditionHead head = start;
    WorldMoment from = span.from;
    const auto &receipts =
        sources.state.character_condition_extensions.applied_receipts.rows();
    for (const auto &row : receipts) {
      const auto &change = row.second;
      if (change.actor_id != start.actor_id ||
          change.condition_revision_after <= head.condition_revision)
        continue;
      if (change.effective_at.kind != ActorFoundationAt::Kind::exact_moment)
        fail("CharacterCapabilityExactChangeRequired", row.first);
      const auto &at = change.effective_at.exact_moment;
      if (take(compare_world_moments(at, span.to)) >= 0)
        break;
      if (take(compare_world_moments(at, from)) < 0)
        fail("CharacterCapabilitySpanSourceOrderMismatch", row.first);
      if (take(compare_world_moments(from, at)) < 0) {
        MomentSpan part{from, at};
        auto value = take(read_character_capability_for_span(
            sources, head, purpose, skill, part));
        take(visitor.visit_capability_span(part, value));
        from = at;
      }
      if (change.condition_revision_before != head.condition_revision)
        fail("CharacterCapabilitySpanRevisionGap", row.first);
      head.condition_revision = change.condition_revision_after;
      head.latest_receipt_ref = ConditionReceiptRefV2{change.result_id};
    }
    if (take(compare_world_moments(from, span.to)) < 0) {
      MomentSpan part{from, span.to};
      auto value = take(read_character_capability_for_span(
          sources, head, purpose, skill, part));
      take(visitor.visit_capability_span(part, value));
    }
    return Done{};
  });
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status
register_original_personal_training_purpose(ActorFoundationCatalog &catalog) {
  bool selected = false;
  for (const auto &module : catalog.identity.module_versions)
    if (module.first == "actor_start_context") {
      if (selected || module.second != "3")
        return common_failure("PersonalTrainingCompleteProfileRequired", {},
                              ErrorKind::unsupported_capability);
      selected = true;
    }
  if (!selected || catalog.identity.content_digest.empty())
    return common_failure("PersonalTrainingCompleteProfileRequired", {},
                          ErrorKind::unsupported_capability);
  CharacterPurposeBinding expected{
      {"personal_training_participant", 1, catalog.identity.content_digest},
      {"original_personal_training_participation", 1,
       catalog.identity.content_digest},
      CharacterPurposeBinding::Reducer::personal_training};
  if (const auto *existing =
          catalog.capability_purpose_bindings.find(expected.purpose_ref.id)) {
    if (existing->reducer != expected.reducer ||
        !actor_foundation_rule_equal(existing->purpose_ref,
                                     expected.purpose_ref) ||
        !actor_foundation_rule_equal(existing->rule_ref, expected.rule_ref))
      return common_failure("PersonalTrainingPurposeConflict");
    return common_success();
  }
  const auto key = expected.purpose_ref.id;
  catalog.capability_purpose_bindings.insert(key, std::move(expected));
  return common_success();
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status
register_original_growth_service_work_purpose(ActorFoundationCatalog &catalog) {
  bool selected = false;
  for (const auto &module : catalog.identity.module_versions)
    if (module.first == "actor_start_context") {
      if (selected || module.second != "3")
        return common_failure("GrowthServiceCompleteProfileRequired", {},
                              ErrorKind::unsupported_capability);
      selected = true;
    }
  if (!selected || catalog.identity.content_digest.empty() ||
      !catalog.normal_work_efficiency_rule)
    return common_failure("GrowthServiceOriginalWorkRuleRequired", {},
                          ErrorKind::unsupported_capability);
  CharacterPurposeBinding expected{
      {"growth_service_worker", 1, catalog.identity.content_digest},
      catalog.normal_work_efficiency_rule->rule_ref,
      CharacterPurposeBinding::Reducer::normal_work};
  if (const auto *existing =
          catalog.capability_purpose_bindings.find(expected.purpose_ref.id)) {
    if (existing->reducer != expected.reducer ||
        !actor_foundation_rule_equal(existing->purpose_ref,
                                     expected.purpose_ref) ||
        !actor_foundation_rule_equal(existing->rule_ref, expected.rule_ref))
      return common_failure("GrowthServicePurposeConflict");
    return common_success();
  }
  const auto key = expected.purpose_ref.id;
  catalog.capability_purpose_bindings.insert(key, std::move(expected));
  return validate_character_capability_catalog(catalog);
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<FrozenFacilityCapabilityInputs>
freeze_character_capability_after_bound_condition(
    const CharacterConditionSources &sources,
    const FrozenFacilityCapabilityInputs &before,
    const std::optional<StableId> &skill,
    const BoundActorConditionWrite &binding) {
  return guarded<FrozenFacilityCapabilityInputs>([&] {
    const auto capture = sources.common.actor_condition_capture_identity();
    const auto &drafts = sources.common.actor_condition_execution_drafts();
    if (!capture || binding.capture_identity != capture ||
        binding.capture_sequence < 1 ||
        binding.capture_sequence > static_cast<Integer>(drafts.size()))
      fail("CapabilityActualConditionCaptureRequired", before.actor_id);
    const auto &draft =
        drafts[static_cast<std::size_t>(binding.capture_sequence - 1)];
    const auto *receipt =
        sources.state.character_condition_extensions.applied_receipts.find(
            binding.receipt_ref.result_id);
    if (!receipt || receipt->result_id != binding.receipt_ref.result_id ||
        receipt->actor_id != before.actor_id ||
        receipt->condition_revision_before !=
            before.condition_head_snapshot.condition_revision ||
        receipt->condition_revision_after !=
            receipt->condition_revision_before + 1 ||
        receipt->effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
        take(compare_world_moments(receipt->effective_at.exact_moment,
                                   sources.actual_now)) ||
        draft.kind != ActorConditionExecutionDraft::Kind::mutation ||
        draft.actor_id != before.actor_id ||
        draft.sequence != binding.capture_sequence || !draft.before ||
        draft.before->alive != before.body_scalars.alive ||
        draft.before->hp != before.body_scalars.hp ||
        draft.before->fatigue != before.body_scalars.fatigue ||
        draft.before->injury != before.body_scalars.injury)
      fail("CapabilityActualConditionDraftMismatch", before.actor_id);
    const auto &a = draft.identity;
    const auto &b = sources.common.state().identity;
    if (a.world_id != b.world_id || a.execution_epoch != b.execution_epoch ||
        a.revision != b.revision || a.tick != b.tick)
      fail("CapabilityConditionCandidateMismatch", before.actor_id);
    bool hp = false, fatigue = false, injury = false, alive = false;
    for (const auto &delta : receipt->scalar_changes) {
      bool *seen = nullptr;
      Integer actual_before = 0, actual_after = 0;
      switch (delta.field) {
      case ConditionScalarChange::Field::hp:
        seen = &hp;
        actual_before = draft.before->hp;
        actual_after = draft.after.hp;
        break;
      case ConditionScalarChange::Field::fatigue:
        seen = &fatigue;
        actual_before = draft.before->fatigue;
        actual_after = draft.after.fatigue;
        break;
      case ConditionScalarChange::Field::injury:
        seen = &injury;
        actual_before = draft.before->injury;
        actual_after = draft.after.injury;
        break;
      case ConditionScalarChange::Field::alive:
        seen = &alive;
        actual_before = draft.before->alive ? 1 : 0;
        actual_after = draft.after.alive ? 1 : 0;
        break;
      }
      if (!seen || *seen || delta.before != actual_before ||
          delta.after != actual_after)
        fail("CapabilityConditionScalarCaptureMismatch", before.actor_id);
      *seen = true;
    }
    if (hp != draft.hp_written || fatigue != draft.fatigue_written ||
        injury != draft.injury_written || alive != draft.alive_written)
      fail("CapabilityConditionWritePresenceMismatch", before.actor_id);
    CharacterConditionAtHead value;
    value.head = before.condition_head_snapshot;
    value.head.condition_revision = receipt->condition_revision_after;
    value.head.latest_receipt_ref = binding.receipt_ref;
    value.body = {draft.after.alive,
                  draft.after.hp,
                  draft.after.fatigue,
                  draft.after.injury,
                  {}};
    value.observed_at = sources.actual_now;
    return take(freeze_character_capability(
        reduce(sources, value, before.purpose_ref, skill)));
  });
}
} // namespace hanmo::resident

namespace hanmo::resident {
namespace {
Status register_original_normal_work_binding(ActorFoundationCatalog &catalog,
                                             const StableId &purpose,
                                             const StableId &conflict) {
  CharacterPurposeBinding binding{
      {purpose, 1, catalog.identity.content_digest},
      catalog.normal_work_efficiency_rule->rule_ref,
      CharacterPurposeBinding::Reducer::normal_work};
  if (const auto *prior =
          catalog.capability_purpose_bindings.find(binding.purpose_ref.id)) {
    if (prior->reducer != binding.reducer ||
        !actor_foundation_rule_equal(prior->purpose_ref, binding.purpose_ref) ||
        !actor_foundation_rule_equal(prior->rule_ref, binding.rule_ref))
      return common_failure(conflict, {}, ErrorKind::internal);
    return validate_character_capability_catalog(catalog);
  }
  const auto id = binding.purpose_ref.id;
  if (!catalog.capability_purpose_bindings.insert(id, std::move(binding)))
    return common_failure(conflict, {}, ErrorKind::internal);
  return validate_character_capability_catalog(catalog);
}
} // namespace
Status
register_original_craft_job_work_purpose(ActorFoundationCatalog &catalog) {
  bool complete = false;
  for (const auto &module : catalog.identity.module_versions)
    if (module.first == "actor_start_context") {
      if (complete || module.second != "3")
        return common_failure("CraftJobCompleteProfileRequired", {},
                              ErrorKind::unsupported_capability);
      complete = true;
    }
  if (!complete || catalog.identity.content_digest.empty() ||
      !catalog.normal_work_efficiency_rule)
    return common_failure("CraftJobOriginalWorkRuleRequired", {},
                          ErrorKind::unsupported_capability);
  return register_original_normal_work_binding(catalog, "craft_job_worker",
                                               "CraftJobPurposeConflict");
}
Status register_original_private_shop_operation_purpose(
    ActorFoundationCatalog &catalog) {
  bool complete = false, shop = false, operation = false;
  for (const auto &module : catalog.identity.module_versions) {
    if (module.first == "actor_start_context") {
      if (complete || module.second != "3")
        return common_failure("ShopOperationCompleteProfileRequired", {},
                              ErrorKind::unsupported_capability);
      complete = true;
    }
    if (module.first == "private_shops") {
      if (shop || module.second != "2")
        return common_failure("ShopOperationPrivateShops2Required", {},
                              ErrorKind::unsupported_capability);
      shop = true;
    }
    if (module.first == "private_shop_operation_capabilities") {
      if (operation || module.second != "1")
        return common_failure("ShopOperationCapabilities1Required", {},
                              ErrorKind::unsupported_capability);
      operation = true;
    }
  }
  if (!complete || !shop || !operation ||
      catalog.identity.content_digest.empty() ||
      !catalog.normal_work_efficiency_rule)
    return common_failure("ShopOperationOriginalWorkRuleRequired", {},
                          ErrorKind::unsupported_capability);
  return register_original_normal_work_binding(
      catalog, "private_shop_operation", "ShopOperationPurposeConflict");
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status validate_frozen_character_capability_original_prefix(
    const CharacterConditionSources &sources,
    const FrozenFacilityCapabilityInputs &frame,
    const std::optional<StableId> &skill) {
  return guarded<Done>([&] {
    auto body = take(read_character_condition_verified_prefix(
        sources, frame.condition_head_snapshot, frame.captured_at));
    auto expected = take(freeze_character_capability(
        reduce(sources, body, frame.purpose_ref, skill)));
    if (frame.actor_id != expected.actor_id ||
        !actor_foundation_rule_equal(frame.purpose_ref, expected.purpose_ref) ||
        take(compare_world_moments(frame.captured_at, expected.captured_at)) ||
        frame.body_scalars.alive != expected.body_scalars.alive ||
        frame.body_scalars.hp != expected.body_scalars.hp ||
        frame.body_scalars.fatigue != expected.body_scalars.fatigue ||
        frame.body_scalars.injury != expected.body_scalars.injury ||
        !frame.active_condition_inputs.empty() ||
        frame.selected_rule_refs.size() != expected.selected_rule_refs.size() ||
        frame.result.allowed != expected.result.allowed ||
        frame.result.efficiency_bp != expected.result.efficiency_bp ||
        !actor_foundation_rule_equal(frame.result.rule_ref,
                                     expected.result.rule_ref) ||
        take(compare_world_moments(frame.result.observed_at,
                                   expected.result.observed_at)) ||
        frame.result.limiting_condition_refs.size() !=
            expected.result.limiting_condition_refs.size())
      fail("FrozenCapabilityOriginalPrefixMismatch", frame.actor_id);
    for (std::size_t i = 0; i < frame.selected_rule_refs.size(); ++i)
      if (!actor_foundation_rule_equal(frame.selected_rule_refs[i],
                                       expected.selected_rule_refs[i]))
        fail("FrozenCapabilityOriginalRuleMismatch", frame.actor_id);
    for (std::size_t i = 0; i < frame.result.limiting_condition_refs.size();
         ++i)
      if (!same_source(frame.result.limiting_condition_refs[i],
                       expected.result.limiting_condition_refs[i]))
        fail("FrozenCapabilityOriginalLimitMismatch", frame.actor_id);
    return Done{};
  });
}
Status validate_frozen_character_capability_original_span(
    const CharacterConditionSources &sources,
    const FrozenFacilityCapabilityInputs &frame, const MomentSpan &span,
    const std::optional<StableId> &skill) {
  return guarded<Done>([&] {
    if (take(compare_world_moments(span.from, span.to)) >= 0 ||
        take(compare_world_moments(frame.captured_at, span.from)) > 0 ||
        take(compare_world_moments(span.to, sources.actual_now)) > 0)
      fail("FrozenCapabilityOriginalSpanInvalid", frame.actor_id);
    take(validate_frozen_character_capability_original_prefix(sources, frame,
                                                              skill));
    if (!frame.result.allowed)
      fail("FrozenCapabilityOriginalSpanDisallowed", frame.actor_id);
    // Strict revision cuts include a later same-M receipt at the start: that
    // receipt was already active for a positive span and must be newly adopted.
    for (const auto &entry :
         sources.state.character_condition_extensions.applied_receipts.rows()) {
      const auto &row = entry.second;
      if (row.actor_id != frame.actor_id ||
          row.condition_revision_after <=
              frame.condition_head_snapshot.condition_revision)
        continue;
      if (row.effective_at.kind != ActorFoundationAt::Kind::exact_moment)
        fail("CharacterCapabilityExactChangeRequired", entry.first);
      if (take(compare_world_moments(row.effective_at.exact_moment, span.to)) <
          0)
        fail("FrozenCapabilityOriginalSpanNeedsBoundary", entry.first);
    }
    return Done{};
  });
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status
register_original_care_handover_work_purpose(ActorFoundationCatalog &catalog) {
  bool complete = false, care = false;
  for (const auto &module : catalog.identity.module_versions) {
    if (module.first == "actor_start_context") {
      if (complete || module.second != "3")
        return common_failure("CareHandoverCompleteProfileRequired", {},
                              ErrorKind::unsupported_capability);
      complete = true;
    }
    if (module.first == "care_requirements") {
      if (care || (module.second != "5" && module.second != "6"))
        return common_failure("CareHandoverHistoryVersion5Required", {},
                              ErrorKind::unsupported_capability);
      care = true;
    }
  }
  if (!complete || !care || catalog.identity.content_digest.empty() ||
      !catalog.normal_work_efficiency_rule)
    return common_failure("CareHandoverOriginalWorkRuleRequired", {},
                          ErrorKind::unsupported_capability);
  CharacterPurposeBinding expected{
      {"facility_assisted_clearance_worker", 1,
       catalog.identity.content_digest},
      catalog.normal_work_efficiency_rule->rule_ref,
      CharacterPurposeBinding::Reducer::normal_work};
  if (const auto *old =
          catalog.capability_purpose_bindings.find(expected.purpose_ref.id)) {
    if (old->reducer != expected.reducer ||
        !actor_foundation_rule_equal(old->purpose_ref, expected.purpose_ref) ||
        !actor_foundation_rule_equal(old->rule_ref, expected.rule_ref))
      return common_failure("CareHandoverWorkerPurposeConflict", {},
                            ErrorKind::internal);
    return validate_character_capability_catalog(catalog);
  }
  const auto id = expected.purpose_ref.id;
  if (!catalog.capability_purpose_bindings.insert(id, std::move(expected)))
    return common_failure("CareHandoverWorkerPurposeConflict", {},
                          ErrorKind::internal);
  return validate_character_capability_catalog(catalog);
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<CharacterPurposeReceiptChange> read_character_purpose_change_at_receipt(
    const CharacterConditionSources &sources, const ConditionReceiptRefV2 &ref,
    const ActorFoundationRuleRef &purpose) {
  return guarded<CharacterPurposeReceiptChange>([&] {
    take(validate_character_capability_catalog(sources.catalog));
    const auto *binding =
        sources.catalog.capability_purpose_bindings.find(purpose.id);
    if (!binding || !actor_foundation_rule_equal(binding->purpose_ref, purpose))
      fail("CapacityOriginalRegisteredPurposeRequired", purpose.id,
           ErrorKind::unsupported_capability);
    // The original receipt and every earlier cause are validated without asking
    // the derived capacity output to prove its own cause. Whole-world reverse
    // coverage remains mandatory at install, restore and publication.
    const auto *receipt =
        take(read_character_condition_receipt_provenance(sources, ref));
    if (receipt->effective_at.kind != ActorFoundationAt::Kind::exact_moment)
      fail("CapacityOriginalExactReceiptRequired", ref.result_id);
    const auto *origin =
        sources.state.character_origin_records.origins.find(receipt->actor_id);
    if (!origin || origin->actor_id != receipt->actor_id)
      fail("CapacityOriginalActorOriginRequired", receipt->actor_id);
    ActorConditionHead after_head{receipt->actor_id,
                                  receipt->condition_revision_after,
                                  origin->initial_condition_seed_ref, ref};
    auto actual = take(read_character_condition_chain_cut(
        sources, after_head, receipt->effective_at.exact_moment));
    FrozenBodyScalars before = actual.body;
    // These are the same verified original before values just replayed above,
    // never a difference inferred from today's scalar or a later receipt.
    for (const auto &change : receipt->scalar_changes)
      switch (change.field) {
      case ConditionScalarChange::Field::alive:
        before.alive = change.before != 0;
        break;
      case ConditionScalarChange::Field::hp:
        before.hp = change.before;
        break;
      case ConditionScalarChange::Field::fatigue:
        before.fatigue = change.before;
        break;
      case ConditionScalarChange::Field::injury:
        before.injury = change.before;
        break;
      }
    return CharacterPurposeReceiptChange{
        receipt, binding, original_binding_allowed(before, binding->reducer),
        original_binding_allowed(actual.body, binding->reducer)};
  });
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status
register_original_care_participant_purpose(ActorFoundationCatalog &catalog) {
  bool care = false, condition = false;
  for (const auto &module : catalog.identity.module_versions) {
    if (module.first == "care_requirements") {
      if (care || (module.second != "5" && module.second != "6"))
        return common_failure("CareParticipationSelectedSourcesRequired", {},
                              ErrorKind::unsupported_capability);
      care = true;
    }
    if (module.first == "character_condition_extensions") {
      if (condition || module.second != "4")
        return common_failure("CareParticipationSelectedSourcesRequired", {},
                              ErrorKind::unsupported_capability);
      condition = true;
    }
  }
  if (!care || !condition || catalog.identity.content_digest.empty())
    return common_failure("CareParticipationSelectedSourcesRequired", {},
                          ErrorKind::unsupported_capability);
  CharacterPurposeBinding expected{
      {"care_participant", 1, catalog.identity.content_digest},
      {"original_family_care_alive", 1, catalog.identity.content_digest},
      CharacterPurposeBinding::Reducer::family_care};
  if (const auto *existing =
          catalog.capability_purpose_bindings.find(expected.purpose_ref.id)) {
    if (existing->reducer != expected.reducer ||
        !actor_foundation_rule_equal(existing->purpose_ref,
                                     expected.purpose_ref) ||
        !actor_foundation_rule_equal(existing->rule_ref, expected.rule_ref))
      return common_failure("CareParticipationPurposeConflict", {},
                            ErrorKind::internal);
    return validate_character_capability_catalog(catalog);
  }
  const auto id = expected.purpose_ref.id;
  if (!catalog.capability_purpose_bindings.insert(id, std::move(expected)))
    return common_failure("CareParticipationPurposeConflict", {},
                          ErrorKind::internal);
  return validate_character_capability_catalog(catalog);
}
} // namespace hanmo::resident

namespace hanmo::resident {
// 原锁内身体表达binding是唯一rule来源；不新增HP阈值、不借工作/学习效率。
Status register_original_family_personal_decision_purpose(
    ActorFoundationCatalog &catalog) {
  if (!family_personal_selected(catalog) ||
      catalog.identity.content_digest.empty())
    return common_failure("FamilyPersonalDecisionOriginalSelectionRequired", {},
                          ErrorKind::unsupported_capability);
  auto valid = validate_character_capability_catalog(catalog);
  if (!valid.ok)
    return valid;
  const auto *original =
      catalog.capability_purpose_bindings.find("character_expression");
  if (!original ||
      original->reducer != CharacterPurposeBinding::Reducer::expression ||
      original->rule_ref.id != "original_alive_hp_purpose_limit")
    return common_failure("FamilyPersonalDecisionOriginalExpressionRequired",
                          {}, ErrorKind::unsupported_capability);
  CharacterPurposeBinding binding{
      {"family_personal_decision", 1, catalog.identity.content_digest},
      original->rule_ref,
      CharacterPurposeBinding::Reducer::expression};
  if (const auto *prior =
          catalog.capability_purpose_bindings.find(binding.purpose_ref.id)) {
    if (prior->reducer != binding.reducer ||
        !actor_foundation_rule_equal(prior->purpose_ref, binding.purpose_ref) ||
        !actor_foundation_rule_equal(prior->rule_ref, binding.rule_ref))
      return common_failure("FamilyPersonalDecisionPurposeConflict");
    return common_success();
  }
  const auto id = binding.purpose_ref.id;
  if (!catalog.capability_purpose_bindings.insert(id, std::move(binding)))
    return common_failure("FamilyPersonalDecisionPurposeConflict");
  return validate_character_capability_catalog(catalog);
}
} // namespace hanmo::resident

namespace hanmo::resident {
// 同原 normal-work 规则登记唯一目的，不借 crafting 或补造能力通过值。
Status register_original_facility_construction_work_purpose(
    ActorFoundationCatalog &catalog) {
  bool construction = false, complete = false;
  for (const auto &module : catalog.identity.module_versions) {
    if (module.first == "facility_construction") {
      if (construction || module.second != "1")
        return common_failure("FacilityConstruction1Required", {},
                              ErrorKind::unsupported_capability);
      construction = true;
    }
    if (module.first == "actor_start_context") {
      if (complete || module.second != "3")
        return common_failure("FacilityConstructionCompleteProfileRequired", {},
                              ErrorKind::unsupported_capability);
      complete = true;
    }
  }
  if (!construction || !complete || catalog.identity.content_digest.empty() ||
      !catalog.normal_work_efficiency_rule)
    return common_failure("FacilityConstructionOriginalWorkRuleRequired", {},
                          ErrorKind::unsupported_capability);
  const auto valid = validate_character_capability_catalog(catalog);
  if (!valid.ok)
    return valid;
  return register_original_normal_work_binding(
      catalog, "facility_construction_worker",
      "FacilityConstructionPurposeConflict");
}
} // namespace hanmo::resident

namespace hanmo::resident {
namespace {
// 仅未闭合的已选原来源分支使用明确替身码，不能假装允许或永久业务拒绝。
[[noreturn]] void family_personal_history_required(const char *reason,
                                                   const StableId &id) {
  auto error =
      common_failure("NOT_IMPLEMENTED", id, ErrorKind::unsupported_capability)
          .error;
  error.reason = reason;
  throw Failure{std::move(error)};
}
// 这里只识别本批已具象的原业务头，不以family前缀放行未知消费者。
bool family_personal_body_consumer(const SourceRef &source) {
  if (source.domain == "family_collateral_legacy")
    return source.kind == "family_collateral_proposed" ||
           source.kind == "family_collateral_replied" ||
           source.kind == "family_collateral_recognized";
  if (source.domain == "care_requirements")
    return source.kind == "family_education_proposed" ||
           source.kind == "family_education_chosen" ||
           source.kind == "family_education_activated";
  if (source.domain == "politics")
    return source.kind == "foundation_intent_created" ||
           source.kind == "foundation_support_decided" ||
           source.kind == "foundation_founding_decided" ||
           source.kind == "political_appointment_offered" ||
           source.kind == "political_appointment_accepted" ||
           source.kind == "political_appointment_declined" ||
           source.kind == "political_appointment_withdrawn";
  if (source.domain == "estate")
    return source.kind == "residence_offered" ||
           source.kind == "residence_accepted";
  return source.domain == "communication_links" &&
         source.kind == "family_communication_posted";
}
} // namespace
// 保同一原verified-prefix与reducer，不反入完整World/source反向覆盖形成循环。
Outcome<CharacterCapabilityView> read_family_personal_capability_before_fact(
    const CharacterConditionSources &sources, const StableId &actor,
    const ActorFoundationRuleRef &purpose, const SourceRef &consumer,
    const WorldMoment &at) {
  return guarded<CharacterCapabilityView>([&] {
    if (!family_personal_body_consumer(consumer) || consumer.id.empty() ||
        (purpose.id != "family_personal_decision" &&
         purpose.id != "character_expression" &&
         purpose.id != "s3_fixed_conversation_participant"))
      fail("FamilyPersonalOriginalConsumerRequired", consumer.id,
           ErrorKind::unsupported_capability);
    if (purpose.id == "s3_fixed_conversation_participant" &&
        (consumer.domain != "communication_links" ||
         consumer.kind != "family_communication_posted"))
      fail("FamilyCommunicationOriginalConsumerRequired", consumer.id,
           ErrorKind::unsupported_capability);
    const bool political = consumer.domain == "politics";
    const bool residence = consumer.domain == "estate";
    // 本口只证明身体前缀，不授业务许可。调用方先核原typed schema/actor/M；
    // Appointment还须实际foundation_target，Residence还须原S0成功结果。
    if ((political || residence) && purpose.id != "character_expression")
      fail(political ? "PoliticalFoundationOriginalExpressionPurposeRequired"
                     : "ResidenceOriginalExpressionPurposeRequired",
           consumer.id, ErrorKind::unsupported_capability);
    if (!take(condition_application_facts_selected(
            sources.catalog.identity,
            political   ? ConditionApplicationHistoryOwner::political_continuity
            : residence ? ConditionApplicationHistoryOwner::estate_residence
                        : ConditionApplicationHistoryOwner::family_autonomy)))
      fail("FamilyPersonalApplicationHistorySelectionRequired", consumer.id,
           ErrorKind::unsupported_capability);
    const auto &extensions = sources.state.character_condition_extensions;
    if (!extensions.episodes.empty() ||
        !extensions.effect_contributions.empty() ||
        !extensions.recovery_resolutions.empty() ||
        !extensions.combat_return_bindings.empty() ||
        !extensions.instant_limits.empty())
      family_personal_history_required("CompleteConditionEffectReadersRequired",
                                       actor);
    for (const auto &entry : extensions.applied_receipts.rows())
      if (entry.second.actor_id == actor &&
          (entry.second.selected_injury_result.kind ==
               SelectedInjuryResult::Kind::applied ||
           entry.second.selected_injury_result.actual_decision_source_ref ||
           !entry.second.selected_injury_result.effect_items.empty()))
        family_personal_history_required(
            "OriginalInjuryEpisodeApplicationRequired", entry.first);
    auto body = take(read_character_condition_before_application_fact(
        sources, actor, consumer, at));
    return reduce(sources, body, purpose, std::nullopt);
  });
}
} // namespace hanmo::resident

namespace hanmo::resident {
// S3 本地短表达只登记原表达 reducer；借真正已锁规则，不以泛目的替准入。
Status register_original_family_communication_participant_purpose(
    ActorFoundationCatalog &catalog) {
  bool communication = false, complete_body = false;
  for (const auto &module : catalog.identity.module_versions) {
    if (module.first == "communication_links") {
      if (communication || module.second != "2")
        return common_failure("FamilyCommunicationOriginalSelectionRequired",
                              {}, ErrorKind::unsupported_capability);
      communication = true;
    }
    if (module.first == "actor_start_context") {
      if (complete_body || module.second != "3")
        return common_failure("FamilyCommunicationOriginalSelectionRequired",
                              {}, ErrorKind::unsupported_capability);
      complete_body = true;
    }
  }
  if (!communication || !complete_body || !family_personal_selected(catalog) ||
      catalog.identity.content_digest.empty())
    return common_failure("FamilyCommunicationOriginalSelectionRequired", {},
                          ErrorKind::unsupported_capability);
  auto valid = validate_character_capability_catalog(catalog);
  if (!valid.ok)
    return valid;
  const auto *original =
      catalog.capability_purpose_bindings.find("character_expression");
  if (!original ||
      original->reducer != CharacterPurposeBinding::Reducer::expression ||
      original->rule_ref.id != "original_alive_hp_purpose_limit")
    return common_failure("FamilyCommunicationOriginalExpressionRequired", {},
                          ErrorKind::unsupported_capability);
  CharacterPurposeBinding binding{
      {"s3_fixed_conversation_participant", 1, catalog.identity.content_digest},
      original->rule_ref,
      CharacterPurposeBinding::Reducer::expression};
  if (const auto *prior =
          catalog.capability_purpose_bindings.find(binding.purpose_ref.id)) {
    if (prior->reducer != binding.reducer ||
        !actor_foundation_rule_equal(prior->purpose_ref, binding.purpose_ref) ||
        !actor_foundation_rule_equal(prior->rule_ref, binding.rule_ref))
      return common_failure("FamilyCommunicationPurposeConflict");
    return common_success();
  }
  const auto id = binding.purpose_ref.id;
  if (!catalog.capability_purpose_bindings.insert(id, std::move(binding)))
    return common_failure("FamilyCommunicationPurposeConflict");
  return validate_character_capability_catalog(catalog);
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status register_original_growth_trade_participation_purpose(
    ActorFoundationCatalog &catalog) {
  if (catalog.identity.content_digest.empty() ||
      !growth_trade_participation_selected(catalog))
    return common_failure("GrowthTradeParticipationSelectedSourcesRequired", {},
                          ErrorKind::unsupported_capability);
  CharacterPurposeBinding expected{
      {"growth_trade_participant", 1, catalog.identity.content_digest},
      {"original_growth_trade_participation", 1,
       catalog.identity.content_digest},
      CharacterPurposeBinding::Reducer::trade_participation};
  if (const auto *old =
          catalog.capability_purpose_bindings.find(expected.purpose_ref.id)) {
    if (old->reducer != expected.reducer ||
        !actor_foundation_rule_equal(old->purpose_ref, expected.purpose_ref) ||
        !actor_foundation_rule_equal(old->rule_ref, expected.rule_ref))
      return common_failure("GrowthTradeParticipationPurposeConflict");
    return validate_character_capability_catalog(catalog);
  }
  const auto id = expected.purpose_ref.id;
  if (!catalog.capability_purpose_bindings.insert(id, std::move(expected)))
    return common_failure("GrowthTradeParticipationPurposeConflict");
  return validate_character_capability_catalog(catalog);
}
} // namespace hanmo::resident
