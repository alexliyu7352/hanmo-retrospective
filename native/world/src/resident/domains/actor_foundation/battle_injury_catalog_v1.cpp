#include "resident/kernel/catalog_section_observation.hpp"
#include "battle_injury_catalog_v1.hpp"
#include "authored_origin.hpp"
#include "character_condition_read.hpp"
#include "injury_capability_map.hpp"
#include "injury_grade_effects_v3.hpp"
#include "injury_original_results_v1.hpp"
#include "injury_source_links_v1.hpp"
#include "injury_sources_v1.hpp"
#include "resident/kernel/native_personal_battle_lifecycle.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
namespace {
using Adapter = InjuryCapabilityMapEntry::CauseAdapter;
bool valid(const ActorFoundationRuleRef &r, const CatalogIdentity &c) {
  return !r.id.empty() && r.id.size() <= 256 && r.version == 1 &&
         r.catalog_digest == c.content_digest;
}
bool same(const ActorFoundationRuleRef &a, const ActorFoundationRuleRef &b) {
  return actor_foundation_rule_equal(a, b);
}
} // namespace
bool battle_injury_source1_selected(const CatalogIdentity &id) noexcept {
  Integer source = 0, condition = 0;
  for (const auto &m : id.module_versions) {
    if (m.first == "character-injury-sources") {
      if (m.second != "1" || ++source != 1)
        return false;
    }
    if (m.first == "character_condition_extensions") {
      if (m.second != "8" || ++condition != 1)
        return false;
    }
  }
  return source == 1 && condition == 1;
}
Status validate_battle_injury_catalog_v1(const ActorFoundationCatalog &c) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 4, "ActorFoundationCatalog", "validate_battle_injury_catalog_v1", &c);

  if (!battle_injury_source1_selected(c.identity) ||
      c.original_battle_injury_rules.empty() ||
      c.original_battle_injury_rules.size() > 2 ||
      c.original_military_wound_rule ||
      (c.injury_capability_map && c.injury_capability_map->version != 2 &&
       c.injury_capability_map->version != 3))
    return common_failure("BattleInjuryExplicitSelectionRequired");
  if (injury_grade_effects_v3_selected(c)) {
    auto valid = validate_injury_grade_effects_map_v3(*c.injury_capability_map,
                                                      c.identity);
    if (!valid.ok)
      return valid;
    if (c.original_battle_injury_rules.size() != 2)
      return common_failure("OriginalInjuryGradeBothAdaptersRequired");
  }
  std::size_t applied = 0;
  for (std::size_t i = 0; i < c.original_battle_injury_rules.size(); ++i) {
    const auto &r = c.original_battle_injury_rules[i];
    if ((r.cause_adapter != Adapter::field_segment_wounded &&
         r.cause_adapter != Adapter::personal_defeat) ||
        !valid(r.rule_ref, c.identity) ||
        !valid(r.injury_rule_ref, c.identity) ||
        !valid(r.result_choice_ref, c.identity))
      return common_failure("BattleInjuryRuleIdentityMismatch");
    const auto *compiled = r.cause_adapter == Adapter::field_segment_wounded
                               ? "original_field_segment_injury_merge"
                               : "original_personal_injury_merge";
    if (r.rule_ref.id != compiled)
      return common_failure("UnsupportedBattleInjuryScalarRule");
    for (std::size_t j = 0; j < i; ++j)
      if (c.original_battle_injury_rules[j].cause_adapter == r.cause_adapter)
        return common_failure("DuplicateBattleInjurySelection");
    std::size_t matches = 0;
    if (c.injury_capability_map)
      for (const auto &e : c.injury_capability_map->entries)
        if (e.cause_adapter == r.cause_adapter &&
            same(e.injury_rule_ref, r.injury_rule_ref) &&
            same(e.result_choice_ref, r.result_choice_ref))
          ++matches;
    if (r.selected_applied) {
      ++applied;
      if (matches != 1)
        return common_failure("BattleInjurySelectedMapRowRequired");
    } else if (matches)
      return common_failure("InactiveBattleInjuryMapRow");
  }
  // Every authored map row must have its actual finite selected owner. Neither
  // unused choices nor a legacy casualty rule are silently accepted in v2.
  if (c.injury_capability_map) {
    if (c.injury_capability_map->entries.size() != applied)
      return common_failure("UnselectedBattleInjuryMapRow");
    for (const auto &e : c.injury_capability_map->entries) {
      std::size_t matches = 0;
      for (const auto &r : c.original_battle_injury_rules)
        if (r.selected_applied && r.cause_adapter == e.cause_adapter &&
            same(e.injury_rule_ref, r.injury_rule_ref) &&
            same(e.result_choice_ref, r.result_choice_ref))
          ++matches;
      if (matches != 1 || !e.original_casualty_rule_ref.empty())
        return common_failure("BattleInjuryMapSourceMismatch");
    }
  } else if (applied)
    return common_failure("BattleInjuryMapRequired");
  return common_success();
}
Outcome<SelectedBattleInjurySourceV1> read_selected_battle_injury_source_v1(
    const CharacterConditionSources &s, const PersonalState *personal,
    const FieldBattleWorldState *field,
    const CharacterBattleReceiptSourceV1 &source, const StableId &actor) {
  auto catalog = validate_injury_capability_map(s.catalog);
  if (!catalog.ok)
    return {false, {}, catalog.error};
  auto selected = validate_battle_injury_catalog_v1(s.catalog);
  if (!selected.ok)
    return {false, {}, selected.error};
  Adapter adapter;
  Outcome<CharacterBattleInjuryReadV1> original;
  if (source.original_receipt.domain == "military" &&
      source.original_receipt.kind == "military_casualties") {
    if (!s.military || !field)
      return {false,
              {},
              common_failure("BattleInjuryActualFieldRootRequired").error};
    adapter = Adapter::field_segment_wounded;
    original = read_field_battle_injury_source_v1(s.common, *s.military, *field,
                                                  source, actor, s.actual_now,
                                                  s.injury_field_clock);
  } else if (source.original_receipt.domain == "personal" &&
             source.original_receipt.kind == "personal_battle") {
    if (!personal || !s.personal_catalog)
      return {false,
              {},
              common_failure("BattleInjuryActualPersonalRootRequired").error};
    adapter = Adapter::personal_defeat;
    auto lifecycle =
        selected_native_personal_battle_lifecycle(s.catalog.identity);
    if (!lifecycle.ok)
      return {false, {}, lifecycle.error};
    original = read_personal_battle_injury_source_v1(
        s.common, *personal, *s.personal_catalog, lifecycle.value, source,
        actor, s.actual_now);
  } else
    return {
        false, {}, common_failure("BattleInjuryOriginalSourceRequired").error};
  if (!original.ok)
    return {false, {}, original.error};
  const OriginalBattleInjuryRuleV1 *rule = nullptr;
  for (const auto &r : s.catalog.original_battle_injury_rules)
    if (r.cause_adapter == adapter)
      rule = &r;
  if (!rule)
    return {false,
            {},
            common_failure("BattleInjuryExplicitSelectionRequired").error};
  const InjuryCapabilityMapEntry *mapping = nullptr;
  if (rule->selected_applied && s.catalog.injury_capability_map)
    for (const auto &e : s.catalog.injury_capability_map->entries)
      if (e.cause_adapter == adapter &&
          same(e.injury_rule_ref, rule->injury_rule_ref) &&
          same(e.result_choice_ref, rule->result_choice_ref))
        mapping = &e;
  return {true,
          {rule, mapping, original.value.completed_at,
           original.value.incoming_injury},
          {}};
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status
validate_battle_injury_selected_result_v1(const CharacterConditionSources &s,
                                          const ConditionReceiptV2 &receipt) {
  if (!s.injury_sources ||
      s.state.character_condition_extensions.applied_receipts.find(
          receipt.result_id) != &receipt)
    return common_failure("BattleInjuryActualReceiptRequired",
                          receipt.result_id);
  auto links = validate_character_injury_source_links_v1(
      s.common, s.state, s.catalog, *s.injury_sources);
  if (!links.ok)
    return links;
  const auto *application =
      s.injury_sources->applications.find(receipt.result_id);
  const auto *source = application
                           ? std::get_if<CharacterBattleReceiptSourceV1>(
                                 &application->original_source)
                           : nullptr;
  if (!source || application->actor_id != receipt.actor_id)
    return common_failure("BattleInjuryActualSourceRequired",
                          receipt.result_id);
  auto actual = read_selected_battle_injury_source_v1(
      s, s.injury_personal, s.injury_field, *source, receipt.actor_id);
  if (!actual.ok)
    return {false, {}, actual.error};
  if (!same(receipt.rule_ref, actual.value.rule->rule_ref) ||
      receipt.selected_injury_result.actual_decision_source_ref ||
      receipt.life_transition_ref || !receipt.recovery_resolution_refs.empty())
    return common_failure("BattleInjurySelectedRuleMismatch",
                          receipt.result_id);
  Status original;
  if (actual.value.rule->cause_adapter == Adapter::field_segment_wounded)
    original = validate_field_injury_original_result_v1(
        s, *s.injury_sources, receipt, *s.military, *s.injury_field);
  else {
    auto lifecycle =
        selected_native_personal_battle_lifecycle(s.catalog.identity);
    if (!lifecycle.ok)
      return {false, {}, lifecycle.error};
    original = validate_personal_injury_original_result_v1(
        s, *s.injury_sources, receipt, *s.injury_personal, *s.personal_catalog,
        lifecycle.value);
  }
  if (!original.ok)
    return original;
  const auto &result = receipt.selected_injury_result;
  if (!actual.value.incoming_injury) {
    const ActorFoundationRuleRef empty;
    if (result.kind != SelectedInjuryResult::Kind::not_an_injury ||
        !same(result.injury_rule_ref, empty) ||
        !same(result.result_choice_ref, empty) ||
        !result.effect_items.empty() || !receipt.created_effect_keys.empty())
      return common_failure("BattleNonInjuryEffectForbidden",
                            receipt.result_id);
    return common_success();
  }
  const auto &rule = *actual.value.rule;
  if (!same(result.injury_rule_ref, rule.injury_rule_ref) ||
      !same(result.result_choice_ref, rule.result_choice_ref))
    return common_failure("BattleInjuryAuthoredResultMismatch",
                          receipt.result_id);
  bool applied = rule.selected_applied;
  if (injury_grade_effects_v3_selected(s.catalog)) {
    auto scar = read_original_injury_scar_due_v3(
        s, receipt.actor_id, receipt.condition_revision_before,
        application->progress.before.injury,
        application->progress.after.injury);
    if (!scar.ok)
      return {false, {}, scar.error};
    applied = scar.value;
  }
  if (!applied) {
    if (result.kind != SelectedInjuryResult::Kind::no_additional_effect ||
        !result.effect_items.empty() || !receipt.created_effect_keys.empty())
      return common_failure("BattleInjuryExplicitNoEffectMismatch",
                            receipt.result_id);
  } else {
    const auto *mapping = actual.value.mapping;
    if (!mapping || result.kind != SelectedInjuryResult::Kind::applied ||
        result.effect_items.size() != mapping->effect_items.size())
      return common_failure("BattleInjuryActualEffectSelectionRequired",
                            receipt.result_id);
    for (std::size_t i = 0; i < result.effect_items.size(); ++i)
      if (!same_original_injury_effect(result.effect_items[i],
                                       mapping->effect_items[i]))
        return common_failure("BattleInjuryOriginalEffectMismatch",
                              receipt.result_id);
  }
  // Episode existence, once/current contribution and lifecycle remain separate
  // obligations; source authentication alone cannot make an effect active.
  return common_success();
}
} // namespace hanmo::resident
