#include "resident/kernel/catalog_section_observation.hpp"
#include "native_life_age_catalog.hpp"
#include "hanmo/resident/model_contract.hpp"
#include "resident/domains/actor_foundation/model.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "exact_time_profile.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
namespace {
Outcome<OriginalCalendarRead> calendar(const CatalogIdentity& id){return OriginalCalendarRead::admit(id,{original_calendar_rule_id,1,id.content_digest});}
}
Outcome<FamilyDeathPolicy> decode_native_family_life_policy(const BoundaryValue& value){try{
    const auto profile=boundary::get_string(value,"profile","family_life");
    if(profile=="complete_life_causes_v4")return decode_family_death_policy4(value);
    return decode_family_death_policy(value);
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status append_native_life_age_module(const BoundaryValue& scenario,CatalogIdentity& identity){try{
    const BoundaryValue* selected=nullptr;
    for(const auto& field:boundary::object(scenario,"scenario"))if(field.first=="life_age_checks"){
        if(selected)return common_failure("DuplicateLifeAgeSelection",{},ErrorKind::internal);
        selected=&field.second;
    }
    for(const auto& module:identity.module_versions)if(module.first=="life_age_checks")return common_failure("DuplicateLifeAgeModuleSelection",{},ErrorKind::internal);
    if(!selected)return common_success();
    bool origin=false,life=false;
    for(const auto& module:identity.module_versions){if(module.first=="actor_start_context"&&module.second=="3")origin=true;if(module.first=="family_life"&&module.second=="4")life=true;}
    if(!origin||!life)return common_failure("LifeAgeCompleteOriginalSourcesRequired",{},ErrorKind::unsupported_capability);
    auto actual_calendar=calendar(identity);if(!actual_calendar.ok)return {false,{},std::move(actual_calendar.error)};
    auto parsed=decode_life_age_catalog(*selected,identity,actual_calendar.value);if(!parsed.ok)return {false,{},std::move(parsed.error)};
    identity.module_versions.emplace_back("life_age_checks","1");return common_success();
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status validate_native_life_age_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 142, "PreparedCatalog", "validate_native_life_age_catalog", &catalog);

    std::size_t count=0;
    for(const auto& module:catalog.identity.module_versions)if(module.first=="life_age_checks"){
        if(++count!=1||module.second!="1")return common_failure("LifeAgeSelectedModuleMismatch",{},ErrorKind::internal);
    }
    if(bool(count)!=bool(catalog.life_age_checks))return common_failure("LifeAgeSelectedCatalogMismatch",{},ErrorKind::internal);
    if(catalog.family_life){auto life=validate_family_death_policy_selection(*catalog.family_life,catalog.identity);if(!life.ok)return life;}
    if(catalog.family_life&&catalog.family_life->selected_version==4&&
       (!catalog.exact_time||!exact_complete_selected(*catalog.exact_time)||!catalog.actor_foundation||!catalog.family||!catalog.common))
        return common_failure("Life4CompleteOriginalSourcesRequired",{},ErrorKind::unsupported_capability);
    if(!catalog.life_age_checks)return common_success();
    if(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time)||!catalog.actor_foundation||!catalog.family_life||
       catalog.family_life->selected_version!=4||!catalog.family_life->enabled||!catalog.family||!catalog.common)
        return common_failure("LifeAgeCompleteOriginalSourcesRequired",{},ErrorKind::unsupported_capability);
    auto actual_calendar=calendar(catalog.identity);if(!actual_calendar.ok)return {false,{},std::move(actual_calendar.error)};
    auto valid=validate_life_age_catalog(*catalog.life_age_checks,catalog.identity,actual_calendar.value);if(!valid.ok)return valid;
    const auto& death_rule=catalog.actor_foundation->natural_age_death_rule;
    if(!death_rule||!actor_foundation_rule_equal(*death_rule,catalog.life_age_checks->policy_ref))
        return common_failure("LifeAgeOriginalConditionRuleRequired",{},ErrorKind::unsupported_capability);
    return common_success();
}
Status prepare_native_life_age_catalog(const BoundaryValue& scenario,PreparedCatalog& catalog){try{
    if(catalog.life_age_checks)return common_failure("LifeAgeCatalogAlreadyPrepared",{},ErrorKind::internal);
    const BoundaryValue* selected=nullptr;
    for(const auto& field:boundary::object(scenario,"scenario"))if(field.first=="life_age_checks"){
        if(selected)return common_failure("DuplicateLifeAgeSelection",{},ErrorKind::internal);selected=&field.second;
    }
    if(selected){auto actual_calendar=calendar(catalog.identity);if(!actual_calendar.ok)return {false,{},std::move(actual_calendar.error)};
        auto parsed=decode_life_age_catalog(*selected,catalog.identity,actual_calendar.value);if(!parsed.ok)return {false,{},std::move(parsed.error)};
        catalog.life_age_checks=checkpoint_make_shared<const LifeAgeCatalog>(std::move(parsed.value));}
    return validate_native_life_age_catalog(catalog);
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
}
