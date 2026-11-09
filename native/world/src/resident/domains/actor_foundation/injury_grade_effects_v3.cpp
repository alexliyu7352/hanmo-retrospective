#include "resident/kernel/catalog_section_observation.hpp"
#include "injury_grade_effects_v3.hpp"
#include "battle_injury_catalog_v1.hpp"
#include "character_condition_read.hpp"
#include "injury_original_results_v1.hpp"
#include "injury_source_links_v1.hpp"
#include "injury_sources_v1.hpp"
#include "resident/kernel/native_personal_battle_lifecycle.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
namespace {
using Adapter = InjuryCapabilityMapEntry::CauseAdapter;
bool ref(const ActorFoundationRuleRef &r, std::string_view id,
         const CatalogIdentity &catalog) {
  return r.id == id && r.version == 1 && !catalog.content_digest.empty() &&
         r.catalog_digest == catalog.content_digest;
}
ActorFoundationRuleRef authored_ref(std::string_view id,
                                    const CatalogIdentity &catalog) {
  return {checkpoint_decode_boundary_text(id), 1,
          checkpoint_decode_boundary_string(catalog.content_digest)};
}
} // namespace
bool injury_grade_effects_v3_selected(
    const ActorFoundationCatalog &c) noexcept {
  return c.injury_capability_map && c.injury_capability_map->version == 3;
}
Status validate_injury_grade_effects_map_v3(const InjuryCapabilityMap &map,
                                            const CatalogIdentity &id) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 11, "InjuryCapabilityMap", "validate_injury_grade_effects_map_v3", &map);

  if (map.version != 3 || !battle_injury_source1_selected(id) ||
      map.entries.size() != 2)
    return common_failure("OriginalInjuryGradeProfileRequired");
  for (std::size_t i = 0; i < 2; ++i) {
    const auto &e = map.entries[i];
    if (e.cause_adapter != (i == 0 ? Adapter::field_segment_wounded
                                   : Adapter::personal_defeat) ||
        e.selection != InjuryCapabilityMapEntry::Selection::deterministic ||
        !e.original_casualty_rule_ref.empty() ||
        !ref(e.injury_rule_ref, "original_injury_grade_effects_v1", id) ||
        !ref(e.result_choice_ref, "original_receipt_old_wound_cap9", id) ||
        !ref(e.stage_ref, "original_old_wound", id) ||
        e.effect_items.size() != 1)
      return common_failure("OriginalInjuryGradeMapMismatch");
    const auto &effect = e.effect_items.front();
    const auto *loss =
        std::get_if<InjuryAttributeReduction>(&effect.effect_payload);
    if (effect.effect_key != "original_old_wound" ||
        !ref(effect.original_effect_rule_ref, "original_receipt_old_wound_cap9",
             id) ||
        !std::holds_alternative<PermanentInjuryEffect>(effect.retention) ||
        !loss || loss->attribute_id != "martial" || loss->amount != 3 ||
        loss->unit_id != "attribute_points" ||
        !ref(loss->reducer_ref, "original_flat_attribute_points", id))
      return common_failure("OriginalInjuryOldWoundRuleMismatch");
  }
  return common_success();
}
Outcome<InjuryCapabilityMap>
decode_authored_injury_grade_effects_v3(const BoundaryValue &value,
                                        const CatalogIdentity &id) {
  try {
    boundary::order(value);
    boundary::fields(value, {"version", "profile"}, {},
                     "injury_capability_map");
    if (boundary::get_int(value, "version", "injury_capability_map") != 3 ||
        boundary::get_string_ref(value, "profile", "injury_capability_map") !=
            "original_injury_grade_effects_v1" ||
        !battle_injury_source1_selected(id))
      boundary::fail("OriginalInjuryGradeProfileRequired",
                     "injury_capability_map");
    InjuryCapabilityMap result;
    result.version = 3;
    checkpoint_decode_boundary_array(2, sizeof(InjuryCapabilityMapEntry));
    result.entries.reserve(2);
    for (std::size_t i = 0; i < 2; ++i) {
      InjuryCapabilityMapEntry e;
      e.cause_adapter =
          i == 0 ? Adapter::field_segment_wounded : Adapter::personal_defeat;
      e.injury_rule_ref = authored_ref("original_injury_grade_effects_v1", id);
      e.result_choice_ref = authored_ref("original_receipt_old_wound_cap9", id);
      e.stage_ref = authored_ref("original_old_wound", id);
      InjuryEffectItem effect;
      effect.effect_key = checkpoint_decode_boundary_text("original_old_wound");
      effect.original_effect_rule_ref =
          authored_ref("original_receipt_old_wound_cap9", id);
      InjuryAttributeReduction loss;
      loss.attribute_id = checkpoint_decode_boundary_text("martial");
      loss.reducer_ref = authored_ref("original_flat_attribute_points", id);
      loss.unit_id = checkpoint_decode_boundary_text("attribute_points");
      loss.amount = 3;
      effect.effect_payload = std::move(loss);
      effect.retention = PermanentInjuryEffect{};
      checkpoint_decode_boundary_array(1, sizeof(InjuryEffectItem));
      e.effect_items.reserve(1);
      e.effect_items.push_back(std::move(effect));
      result.entries.push_back(std::move(e));
    }
    auto valid = validate_injury_grade_effects_map_v3(result, id);
    return {valid.ok, std::move(result), valid.error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status install_original_injury_grade_bindings_v3(ActorFoundationCatalog &c) {
  if (!c.injury_capability_map || !c.original_battle_injury_rules.empty() ||
      c.original_military_wound_rule)
    return common_failure("OriginalInjuryGradeDuplicateAuthorChoice");
  auto valid = validate_injury_grade_effects_map_v3(*c.injury_capability_map,
                                                    c.identity);
  if (!valid.ok)
    return valid;
  checkpoint_decode_boundary_array(2, sizeof(OriginalBattleInjuryRuleV1));
  c.original_battle_injury_rules.reserve(2);
  for (std::size_t i = 0; i < 2; ++i) {
    OriginalBattleInjuryRuleV1 r;
    r.cause_adapter =
        i == 0 ? Adapter::field_segment_wounded : Adapter::personal_defeat;
    r.rule_ref = authored_ref(i == 0 ? "original_field_segment_injury_merge"
                                     : "original_personal_injury_merge",
                              c.identity);
    r.injury_rule_ref =
        authored_ref("original_injury_grade_effects_v1", c.identity);
    r.result_choice_ref =
        authored_ref("original_receipt_old_wound_cap9", c.identity);
    // 表示原来源可产生Applied；是否真旧创由实际before/after与receipt前缀决定。
    r.selected_applied = true;
    c.original_battle_injury_rules.push_back(std::move(r));
  }
  return common_success();
}
Outcome<ActorAttributes> original_current_injury_losses_v3(Integer grade) {
  if (grade < 0 || grade > 3)
    return {false, {}, common_failure("OriginalInjuryGradeInvalid").error};
  ActorAttributes result{};
  result.martial = grade == 1 ? 5 : grade == 2 ? 10 : grade == 3 ? 20 : 0;
  result.leadership = grade == 2 ? 5 : grade == 3 ? 10 : 0;
  return {true, result, {}};
}
Outcome<bool>
read_original_injury_scar_due_v3(const CharacterConditionSources &s,
                                 const StableId &actor, Integer revision_before,
                                 Integer injury_before, Integer injury_after) {
  if (!injury_grade_effects_v3_selected(s.catalog) || !s.injury_sources ||
      revision_before < 0 || injury_before < 0 || injury_before > 3 ||
      injury_after < 0 || injury_after > 3)
    return {false,
            {},
            common_failure("OriginalInjuryScarSourcesRequired", actor).error};
  auto links = validate_character_injury_source_links_v1(
      s.common, s.state, s.catalog, *s.injury_sources);
  if (!links.ok)
    return {false, {}, links.error};
  Integer scars = 0;
  for (const auto &entry :
       s.state.character_condition_extensions.applied_receipts.rows()) {
    const auto &receipt = entry.second;
    if (receipt.actor_id != actor ||
        receipt.condition_revision_after > revision_before)
      continue;
    const auto *application =
        s.injury_sources->applications.find(receipt.result_id);
    const auto *battle = application
                             ? std::get_if<CharacterBattleReceiptSourceV1>(
                                   &application->original_source)
                             : nullptr;
    if (!battle)
      continue;
    auto source = read_selected_battle_injury_source_v1(
        s, s.injury_personal, s.injury_field, *battle, actor);
    if (!source.ok)
      return {false, {}, source.error};
    Status original;
    if (source.value.rule->cause_adapter == Adapter::field_segment_wounded)
      original = validate_field_injury_original_result_v1(
          s, *s.injury_sources, receipt, *s.military, *s.injury_field);
    else {
      auto lifecycle =
          selected_native_personal_battle_lifecycle(s.catalog.identity);
      if (!lifecycle.ok)
        return {false, {}, lifecycle.error};
      original = validate_personal_injury_original_result_v1(
          s, *s.injury_sources, receipt, *s.injury_personal,
          *s.personal_catalog, lifecycle.value);
    }
    if (!original.ok)
      return {false, {}, original.error};
    if (source.value.incoming_injury > 0 &&
        application->progress.before.injury < 3 &&
        application->progress.after.injury == 3 && scars < 3)
      ++scars;
  }
  return {true, injury_before < 3 && injury_after == 3 && scars < 3, {}};
}
} // namespace hanmo::resident
