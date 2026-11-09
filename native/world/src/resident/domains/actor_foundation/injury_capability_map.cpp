#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "injury_capability_map.hpp"
#include "injury_grade_effects_v3.hpp"
#include "battle_injury_catalog_v1.hpp"
#include "injury_attribute_reducer.hpp"
#include "character_condition_read.hpp"
#include "injury_effect_codec_fields.hpp"
#include "actor_foundation_json_visitor.hpp"
#include "authored_origin.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/kernel/world_copy_plan.hpp"
namespace hanmo::resident {namespace {
Status missing(const char* code,const StableId& id={}){return common_failure(code,id,ErrorKind::unsupported_capability);}
bool valid(const ActorFoundationRuleRef& r,const CatalogIdentity& c){return !r.id.empty()&&r.id.size()<=256&&r.version>0&&r.catalog_digest==c.content_digest;}
bool same(const ActorFoundationRuleRef& a,const ActorFoundationRuleRef& b){return actor_foundation_rule_equal(a,b);}
// 仅当前明确rule节点使用；不改写原Boundary树或遍历任意对象补摘要。
Status authored_map_rule(ActorFoundationJsonVisitor &c,
                         ActorFoundationRuleRef &ref,
                         const CatalogIdentity &identity) {
  auto decode =
      [&](const BoundaryValue &value) -> Outcome<ActorFoundationRuleRef> {
    try {
      boundary::order(value);
      boundary::fields(value, {"id", "version"}, {"catalog_digest"},
                       "injury_capability_map/rule");
      if (identity.content_digest.empty())
        boundary::fail("AuthoredCatalogIdentityRequired",
                       "injury_capability_map/rule");
      const auto *rows = value.as<BoundaryValue::Dict>();
      bool supplied = false;
      for (const auto &row : *rows)
        if (row.first == "catalog_digest")
          supplied = true;
      ActorFoundationRuleRef result;
      result.id =
          boundary::get_string(value, "id", "injury_capability_map/rule");
      result.version =
          boundary::get_int(value, "version", "injury_capability_map/rule");
      result.catalog_digest =
          supplied ? boundary::get_string(value, "catalog_digest",
                                          "injury_capability_map/rule")
                   : checkpoint_decode_boundary_string(identity.content_digest);
      if (result.catalog_digest != identity.content_digest)
        boundary::fail("AuthoredCatalogDigestMismatch",
                       "injury_capability_map/rule/catalog_digest");
      return {true, std::move(result), {}};
    } catch (const boundary::DecodeFailure &failure) {
      return {false, {}, failure.error};
    }
  };
  auto no_emit = [](ProjectionWriter &, const ActorFoundationRuleRef &) {
    return common_failure("AuthoredInjuryMapReadOnly");
  };
  if (ScopedCheckpointJsonDecode::current())
    return c.foreign_record(
        ref,
        [&](const BoundaryValue &v, ScopedCheckpointJsonDecode &) {
          return decode(v);
        },
        no_emit);
  return c.foreign_record(ref, decode, no_emit);
}
Status entry(ActorFoundationJsonVisitor &c, InjuryCapabilityMapEntry &value,
             const CatalogIdentity *authored = nullptr) {
  auto rule = [&](auto &v, ActorFoundationRuleRef &ref) {
    return authored ? authored_map_rule(v, ref, *authored)
                    : character_origin_codec::rule(v, ref);
  };

  c.object("injury_rule_ref",
           [&](auto &v) { return rule(v, value.injury_rule_ref); });
  c.object("result_choice_ref",
           [&](auto &v) { return rule(v, value.result_choice_ref); });
  c.field("original_casualty_rule_ref", value.original_casualty_rule_ref);
  StableId adapter, selection;
  c.field("cause_adapter", adapter);
  c.field("selection", selection);
  if (c.status().ok) {
    if (selection != "deterministic")
      return c.reject("UnsupportedInjuryCapabilityMapAdapter");
    if (adapter == "military_wounded")
      value.cause_adapter =
          InjuryCapabilityMapEntry::CauseAdapter::military_wounded;
    else if (adapter == "field_segment_wounded")
      value.cause_adapter =
          InjuryCapabilityMapEntry::CauseAdapter::field_segment_wounded;
    else if (adapter == "personal_defeat")
      value.cause_adapter =
          InjuryCapabilityMapEntry::CauseAdapter::personal_defeat;
    else
      return c.reject("UnsupportedInjuryCapabilityMapAdapter");
  }
  c.object("stage_ref", [&](auto &v) { return rule(v, value.stage_ref); });
  c.sequence("effect_items", value.effect_items, [&](auto &v, auto &x) {
    return injury_effect_codec::effect_with_rule(v, x, rule);
  });
  return value.effect_items.empty() ? c.reject("AppliedInjuryEffectsRequired")
         : value.effect_items.size() > 64
             ? c.reject("InjuryEffectCountExceeded")
             : c.status();
}
Status map_shape(const InjuryCapabilityMap& map,const CatalogIdentity& identity){
  if (map.version == 3)
    return validate_injury_grade_effects_map_v3(map, identity);
  if ((map.version != 1 && map.version != 2) || identity.content_digest.empty())
    return missing("InjuryCapabilityMapVersionRequired");
  if (map.entries.empty() || map.entries.size() > 4096)
    return common_failure("InjuryCapabilityMapEntryCount");
  for (std::size_t i = 0; i < map.entries.size(); ++i) {
    const auto &row = map.entries[i];
    if (!valid(row.injury_rule_ref, identity) ||
        !valid(row.result_choice_ref, identity) ||
        !valid(row.stage_ref, identity) ||
        row.selection != InjuryCapabilityMapEntry::Selection::deterministic)
      return common_failure("InjuryCapabilityMapIdentityMismatch");
    if (map.version == 1) {
      if (row.original_casualty_rule_ref.empty() ||
          row.cause_adapter !=
              InjuryCapabilityMapEntry::CauseAdapter::military_wounded)
        return common_failure("InjuryCapabilityMapIdentityMismatch");
    } else {
      if (!battle_injury_source1_selected(identity) ||
          !row.original_casualty_rule_ref.empty() ||
          (row.cause_adapter !=
               InjuryCapabilityMapEntry::CauseAdapter::field_segment_wounded &&
           row.cause_adapter !=
               InjuryCapabilityMapEntry::CauseAdapter::personal_defeat))
        return common_failure("BattleInjuryMapAdapterMismatch");
    }
    if (row.effect_items.empty())
      return common_failure("AppliedInjuryEffectsRequired");
    if (row.effect_items.size() > 64)
      return common_failure("InjuryEffectCountExceeded");
    for (std::size_t j = 0; j < i; ++j)
      if ((map.version == 1 ||
           row.cause_adapter == map.entries[j].cause_adapter) &&
          same(row.injury_rule_ref, map.entries[j].injury_rule_ref) &&
          same(row.result_choice_ref, map.entries[j].result_choice_ref))
        return common_failure("DuplicateInjuryCapabilityMapSelection");
    for (std::size_t n = 0; n < row.effect_items.size(); ++n) {
      const auto &item = row.effect_items[n];
      if (item.effect_key.empty() || item.effect_key.size() > 256 ||
          !valid(item.original_effect_rule_ref, identity))
        return common_failure("InjuryEffectIdentityMismatch");
      for (std::size_t k = 0; k < n; ++k)
        if (row.effect_items[k].effect_key == item.effect_key)
          return common_failure("DuplicateInjuryEffectKey", item.effect_key);
      if (const auto *reduction =
              std::get_if<InjuryAttributeReduction>(&item.effect_payload)) {
        const auto &id = reduction->attribute_id;
        if (id != "administration" && id != "charisma" && id != "intellect" &&
            id != "leadership" && id != "martial")
          return common_failure("InvalidInjuryAttributeId", id);
        if (!valid(reduction->reducer_ref, identity) ||
            reduction->unit_id.empty() || reduction->unit_id.size() > 256 ||
            reduction->amount < 0)
          return common_failure("InvalidInjuryReductionAmount",
                                item.effect_key);
      } else {
        const auto &restriction =
            std::get<InjuryActivityRestriction>(item.effect_payload);
        if (!valid(restriction.registered_purpose, identity) ||
            !valid(restriction.restriction_ref, identity))
          return common_failure("InjuryActivityRestrictionIdentityMismatch",
                                item.effect_key);
      }
      if (const auto *recoverable =
              std::get_if<RecoverableInjuryEffect>(&item.retention);
          recoverable &&
          !valid(recoverable->original_recovery_gate_ref, identity))
        return common_failure("InjuryRecoveryGateIdentityMismatch",
                              item.effect_key);
    }
  }
  return common_success();
}
}
bool injury_capability_map_selected(const ActorFoundationCatalog& c)noexcept {
 // Condition8 is the explicit source1 successor, not a content-driven upgrade.
 // Preserve physical5's original map and its actual cause adapters unchanged.
 Integer condition_count=0, source_count=0;
 bool condition5=false, condition8=false, source1=false;
 for(const auto& m:c.identity.module_versions) {
  if(m.first=="character_condition_extensions") {
   ++condition_count; condition5=m.second=="5"; condition8=m.second=="8";
  } else if(m.first=="character-injury-sources") {
   ++source_count; source1=m.second=="1";
  }
 }
 return condition_count==1 &&
     ((condition5 && source_count==0) || (condition8 && source_count==1 && source1));
}
Status validate_injury_capability_map_definition(const ActorFoundationCatalog& c){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 7, "ActorFoundationCatalog", "validate_injury_capability_map_definition", &c);

  if (!c.original_battle_injury_rules.empty() ||
      (c.injury_capability_map && (c.injury_capability_map->version == 2 ||
                                   c.injury_capability_map->version == 3))) {
    if (c.injury_capability_map) {
      auto shape = map_shape(*c.injury_capability_map, c.identity);
      if (!shape.ok)
        return shape;
    }
    return validate_battle_injury_catalog_v1(c);
  }
 if(!c.injury_capability_map)return c.original_military_wound_rule&&c.original_military_wound_rule->selected_applied?missing("InjuryCapabilityMapRequired"):common_success();
 if(!injury_capability_map_selected(c))return missing("InjuryCapabilityPhysicalSelectionRequired");
 auto shape=map_shape(*c.injury_capability_map,c.identity);if(!shape.ok)return shape;
 if(!c.original_military_wound_rule||!c.original_military_wound_rule->selected_applied)return missing("InjuryCapabilitySelectedRuleRequired");
 const auto& selected=*c.original_military_wound_rule;std::size_t matches=0;
 for(const auto& entry:c.injury_capability_map->entries)if(same(entry.injury_rule_ref,selected.injury_rule_ref)&&same(entry.result_choice_ref,selected.result_choice_ref)){
  ++matches;if(entry.original_casualty_rule_ref!=selected.original_casualty_rule_ref)return common_failure("InjuryCapabilityCauseRuleMismatch");
 }
 return matches==1?common_success():common_failure("InjuryCapabilitySelectedRowRequired");
}
Status validate_injury_capability_map(const ActorFoundationCatalog& c){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 8, "ActorFoundationCatalog", "validate_injury_capability_map", &c);

 auto definition=validate_injury_capability_map_definition(c);
 if(!definition.ok||!c.injury_capability_map)return definition;
 for(const auto& entry:c.injury_capability_map->entries)
   if (c.injury_capability_map->version == 2 ||
       c.injury_capability_map->version == 3 ||
       (c.original_military_wound_rule &&
        same(entry.injury_rule_ref,
             c.original_military_wound_rule->injury_rule_ref) &&
        same(entry.result_choice_ref,
             c.original_military_wound_rule->result_choice_ref)))
     for (const auto &item : entry.effect_items) {
       if (const auto *reduction =
               std::get_if<InjuryAttributeReduction>(&item.effect_payload)) {
         auto valid =
             validate_original_injury_attribute_reducer(*reduction, c.identity);
         if (!valid.ok)
           return valid;
       } else
         return missing("OriginalInjuryActivityRestrictionReaderRequired",
                        item.effect_key);
       if (std::holds_alternative<RecoverableInjuryEffect>(item.retention))
         return missing("OriginalInjuryRecoveryGateReaderRequired",
                        item.effect_key);
     }
 return common_success();
}
Status validate_original_military_wound_rule_definition(const ActorFoundationCatalog& c){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 9, "ActorFoundationCatalog", "validate_original_military_wound_rule_definition", &c);

 if(!c.original_military_wound_rule)return missing("MilitaryWoundSelectedRuleRequired");const auto& r=*c.original_military_wound_rule;
 for(const auto* ref:{&r.rule_ref,&r.injury_rule_ref,&r.result_choice_ref})if(!valid(*ref,c.identity)||ref->version!=1)return common_failure("MilitaryWoundRuleIdentityMismatch");
 if(r.rule_ref.id!="original_military_wound_scalar"||r.original_casualty_rule_ref.empty())return common_failure("MilitaryWoundRuleIdentityMismatch");
 if(r.selected_applied)return validate_injury_capability_map_definition(c);
 if(r.injury_rule_ref.id!=r.rule_ref.id||r.result_choice_ref.id!="original_military_wound_no_additional_effect")return missing("SelectedMilitaryInjuryEffectAdapterRequired");
 return c.injury_capability_map?missing("InactiveInjuryCapabilityMap"):common_success();
}
Status validate_original_military_wound_rule(const ActorFoundationCatalog& c){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 10, "ActorFoundationCatalog", "validate_original_military_wound_rule", &c);

 auto definition=validate_original_military_wound_rule_definition(c);
 return definition.ok?validate_injury_capability_map(c):definition;
}
// 按原变体与字段比较伤效；任一身份、载荷或保留条件不同即返回不等。
bool same_original_injury_effect(const InjuryEffectItem &a,
                                 const InjuryEffectItem &b) noexcept {
  if (a.effect_key != b.effect_key ||
      !same(a.original_effect_rule_ref, b.original_effect_rule_ref) ||
      a.effect_payload.index() != b.effect_payload.index() ||
      a.retention.index() != b.retention.index())
    return false;
  if (const auto *x =
          std::get_if<InjuryAttributeReduction>(&a.effect_payload)) {
    const auto &y = std::get<InjuryAttributeReduction>(b.effect_payload);
    if (x->attribute_id != y.attribute_id ||
        !same(x->reducer_ref, y.reducer_ref) || x->unit_id != y.unit_id ||
        x->amount != y.amount)
      return false;
  } else {
    const auto &restriction =
        std::get<InjuryActivityRestriction>(a.effect_payload);
    const auto &y = std::get<InjuryActivityRestriction>(b.effect_payload);
    if (!same(restriction.registered_purpose, y.registered_purpose) ||
        !same(restriction.restriction_ref, y.restriction_ref))
      return false;
  }
  if (const auto *x = std::get_if<RecoverableInjuryEffect>(&a.retention))
    return same(x->original_recovery_gate_ref,
                std::get<RecoverableInjuryEffect>(b.retention)
                    .original_recovery_gate_ref);
  return true;
}
static Outcome<InjuryCapabilityMap> decode_map(const BoundaryValue &value,
                                               const CatalogIdentity &identity,
                                               bool authored) {
  try {
    if (authored &&
        boundary::get_int(value, "version", "injury_capability_map") == 3)
      return decode_authored_injury_grade_effects_v3(value, identity);
    // Borrowed preflight bounds the typed arrays before their actual
    // allocation.
    const auto &rows = boundary::array(
        boundary::field(value, "entries", "injury_capability_map"),
        "injury_capability_map/entries");
    if (rows.empty() || rows.size() > 4096)
      boundary::fail("InjuryCapabilityMapEntryCount",
                     "injury_capability_map/entries");
    for (const auto &row : rows) {
      const auto &effects = boundary::array(
          boundary::field(row, "effect_items", "injury_capability_map/entries"),
          "injury_capability_map/entries/effect_items");
      if (effects.empty())
        boundary::fail("AppliedInjuryEffectsRequired",
                       "injury_capability_map/entries/effect_items");
      if (effects.size() > 64)
        boundary::fail("InjuryEffectCountExceeded",
                       "injury_capability_map/entries/effect_items");
    }
    InjuryCapabilityMap result;
    ActorFoundationJsonVisitor c(value);
    c.field("version", result.version);
    c.sequence("entries", result.entries, [&](auto &v, auto &row) {
      return entry(v, row, authored && result.version == 2 ? &identity : nullptr);
    });
    auto status = c.finish();
    if (status.ok)
      status = map_shape(result, identity);
    return {status.ok, std::move(result), status.error};
  } catch (const boundary::DecodeFailure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<InjuryCapabilityMap>
decode_injury_capability_map(const BoundaryValue &value,
                             const CatalogIdentity &identity) {
  return decode_map(value, identity, false);
}
Outcome<InjuryCapabilityMap>
decode_authored_injury_capability_map(const BoundaryValue &value,
                                      const CatalogIdentity &identity) {
  return decode_map(value, identity, true);
}
Outcome<SelectedMilitaryInjurySource> read_selected_military_injury_source(const ActorFoundationCatalog& catalog,const MilitaryState& legacy,const MilitaryExactState& exact,const SourceRef& ref,Integer ordinal,const MilitaryCasualtyHistoryContext& history){
 auto valid_rule=validate_original_military_wound_rule_definition(catalog);if(!valid_rule.ok)return {false,{},std::move(valid_rule.error)};
 if(catalog.identity.content_digest!=history.common.state().metadata.content_digest)return {false,{},common_failure("MilitaryWoundSameCatalogRequired",{},ErrorKind::internal).error};
 auto source=read_military_actor_consequence(legacy,exact,ref,2,ordinal,{},&history);if(!source.ok)return {false,{},std::move(source.error)};
 const auto& selected=*catalog.original_military_wound_rule;
 if(source.value.status!="wounded"||source.value.original_casualty_rule_ref!=selected.original_casualty_rule_ref)return {false,{},common_failure("MilitaryWoundActualSourceRequired",ref.id,ErrorKind::internal).error};
 const InjuryCapabilityMapEntry* mapping=nullptr;if(selected.selected_applied)for(const auto& entry:catalog.injury_capability_map->entries)if(same(entry.injury_rule_ref,selected.injury_rule_ref)&&same(entry.result_choice_ref,selected.result_choice_ref))mapping=&entry;
 return {true,{std::move(source.value),mapping},{}};
}
ProducedSelectedMilitaryInjuryResult::ProducedSelectedMilitaryInjuryResult(
 const SelectedMilitaryInjuryResultBuildView& view)
 :copied_fields_([&]{return CheckpointMemoryCharge(0);}()),
  result_{view.kind,
   {checkpoint_fresh_string_copy(view.injury_rule_ref.id),view.injury_rule_ref.version,
    checkpoint_fresh_string_copy(view.injury_rule_ref.catalog_digest)},
   {checkpoint_fresh_string_copy(view.result_choice_ref.id),view.result_choice_ref.version,
    checkpoint_fresh_string_copy(view.result_choice_ref.catalog_digest)},
   std::nullopt,view.effect_items?*view.effect_items:std::vector<InjuryEffectItem>{}}{}
Outcome<ProducedSelectedMilitaryInjuryResult> produce_selected_military_injury_result(
 const ActorFoundationCatalog& catalog,const MilitaryState& legacy,const MilitaryExactState& exact,
 const SourceRef& source,Integer ordinal,const MilitaryCasualtyHistoryContext& history){
 auto selected=read_selected_military_injury_source(catalog,legacy,exact,source,ordinal,history);
 if(!selected.ok)return {false,{},std::move(selected.error)};
 const auto& rule=*catalog.original_military_wound_rule;
 ProducedSelectedMilitaryInjuryResult produced{SelectedMilitaryInjuryResultBuildView{
  rule.selected_applied?SelectedInjuryResult::Kind::applied:SelectedInjuryResult::Kind::no_additional_effect,
  rule.injury_rule_ref,rule.result_choice_ref,
  selected.value.selected_mapping?&selected.value.selected_mapping->effect_items:nullptr}};
 return {true,std::move(produced),{}};
}
Outcome<OriginalInjuryEffectSource> read_original_injury_effect_source(
 const CharacterConditionSources& sources,const ConditionReceiptRefV2& ref,const StableId& key){
 const auto* receipt=sources.state.character_condition_extensions.applied_receipts.find(ref.result_id);
 if(!receipt||receipt->result_id!=ref.result_id||
    receipt->selected_injury_result.kind!=SelectedInjuryResult::Kind::applied)
  return {false,{},common_failure("OriginalAppliedInjuryReceiptRequired",ref.result_id).error};
 if(std::holds_alternative<CharacterInjurySourceRefV1>(receipt->cause)) {
  auto original=validate_battle_injury_selected_result_v1(sources,*receipt);
  if(!original.ok)return {false,{},std::move(original.error)};
  for(const auto& effect:receipt->selected_injury_result.effect_items)
   if(effect.effect_key==key)return {true,{receipt,&effect},{}};
  return {false,{},common_failure("OriginalInjuryEffectNotSelected",key).error};
 }
 if(!sources.military||!sources.military_exact||
    !std::holds_alternative<MilitaryCasualtyConditionCause>(receipt->cause))
  return {false,{},common_failure("OriginalInjuryCauseAdapterRequired",ref.result_id,
                                 ErrorKind::unsupported_capability).error};
 auto original=validate_military_wound_condition_origin(
  sources.common,sources.state,sources.catalog,*sources.military,*sources.military_exact,
  *receipt,sources.actual_now,sources.military_formations,sources.military_returns);
 if(!original.ok)return {false,{},std::move(original.error)};
 for(const auto& effect:receipt->selected_injury_result.effect_items)
  if(effect.effect_key==key)return {true,{receipt,&effect},{}};
 return {false,{},common_failure("OriginalInjuryEffectNotSelected",key).error};
}
}
