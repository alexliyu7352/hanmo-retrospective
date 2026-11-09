#include "resident/kernel/catalog_section_observation.hpp"
#include "native_born_population_state.hpp"
#include "native_economic_obligation_sources.hpp"
#include "native_family_birth_state.hpp"
#include "world_bundle.hpp"
#include "resident/domains/local_economy/born_population_sources.hpp"
#include "resident/domains/local_economy/economic_exact_phase.hpp"
namespace hanmo::resident {namespace {
const BoundaryValue* selected_block(const BoundaryValue& scenario){const BoundaryValue* block=nullptr;for(const auto& row:boundary::object(scenario,"scenario"))if(row.first=="named_birth_population_sources"){if(block)boundary::fail("DuplicateNamedBirthPopulationSelection","named_birth_population_sources");block=&row.second;}return block;}
}
Outcome<Integer> read_native_born_population_selection(const BoundaryValue& scenario,Integer original){try{
 if(original<1||original>10)return {false,0,common_failure("NamedBirthPopulationOriginalBaseRequired").error};const auto* block=selected_block(scenario);if(!block)return {true,original,{}};
 boundary::fields(*block,{"profile"},{},"named_birth_population_sources");if(boundary::object(*block,"named_birth_population_sources").size()!=1||boundary::get_string_ref(*block,"profile","named_birth_population_sources")!="named_birth_population_sources_v1")return {false,0,common_failure("UnsupportedNamedBirthPopulationSelection",{},ErrorKind::unsupported_capability).error};return {true,original+10,{}};
}catch(const boundary::DecodeFailure& e){return {false,0,e.error};}}
Status append_native_born_population_module(const BoundaryValue& scenario,CatalogIdentity& identity){try{
 const auto* block=selected_block(scenario);if(!block)return common_success();for(const auto& row:identity.module_versions)if(row.first=="named_birth_population_sources")return common_failure("DuplicateNamedBirthPopulationModule");
 auto original=selected_native_economic_obligation_versions(identity);if(!original.ok)return {false,{},std::move(original.error)};auto selected=read_native_born_population_selection(scenario,original.value.local_economy);if(!selected.ok)return {false,{},std::move(selected.error)};
 for(auto& row:identity.module_versions)if(row.first=="local_economy")row.second=std::to_string(selected.value);identity.module_versions.emplace_back("named_birth_population_sources","1");return common_success();
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status validate_native_born_population_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 128, "PreparedCatalog", "validate_native_born_population_catalog", &catalog);

 auto selected=selected_native_economic_obligation_versions(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};if(!selected.value.named_birth_population)return common_success();
 auto birth=selected_native_family_birth_version(catalog.identity);if(!birth.ok)return {false,{},std::move(birth.error)};
 if(birth.value!=4||catalog.dynamic_profile!=DynamicProfile::local_society_v1||!catalog.local_economy||!catalog.exact_time||!exact_complete_selected(*catalog.exact_time))return common_failure("NamedBirthPopulationCompleteBirth4Required",{},ErrorKind::unsupported_capability);
 return validate_native_family_birth_catalog(catalog);
}
Status validate_native_born_population_layout(const WorldBundle& world){
 if(!world.catalog)return common_failure("NamedBirthPopulationCatalogRequired");auto valid=validate_native_born_population_catalog(*world.catalog);if(!valid.ok)return valid;auto selected=selected_native_economic_obligation_versions(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 valid=validate_named_birth_population_selected_layout(world.local_economy,selected.value.local_economy);if(!valid.ok)return valid;
 if(!selected.value.named_birth_population)for(const auto& fact:world.common.state().facts.rows())if(fact.second.domain=="local_economy"&&fact.second.kind=="named_birth_population_transition")return common_failure("NamedBirthPopulationSelectedSourceRequired",fact.first);
 return common_success();
}
namespace {
Status validate_population_original_binding(const WorldBundle& world,const EconomicExactValidationContext& original,const LocalEconomyCatalog& catalog,Integer selected){
 const auto* x=original.born_population;if(original.selected_local_economy!=selected||!x||x->selected_local_economy!=selected||&x->common!=&world.common||&x->economy!=&world.local_economy||&x->catalog!=world.catalog->local_economy.get()||&catalog!=world.catalog->local_economy.get()||&x->births.common!=&world.common||&x->births.births!=&world.family_birth||&x->births.catalog!=world.catalog->family_birth.get()||&x->births.common_catalog!=world.catalog->common.get()||&x->births.family_catalog!=world.catalog->family.get()||&x->births.lineage!=world.catalog->family_lineage.get()||&x->births.interactions!=world.catalog->interaction.get())return common_failure("NamedBirthPopulationWorldBindingMismatch",{},ErrorKind::internal);
 auto valid=validate_born_population_source_bindings(*x);if(!valid.ok)return valid;auto actual=world.actual_phase_moment();if(!actual.ok)return {false,{},std::move(actual.error)};auto now=x->clock.read_current_moment();if(!now.ok)return {false,{},std::move(now.error)};auto same=compare_world_moments(actual.value,now.value);if(!same.ok)return {false,{},std::move(same.error)};return same.value==0?common_success():common_failure("NamedBirthPopulationWorldMomentMismatch",{},ErrorKind::internal);
}
Status reject_unselected_population_binding(const EconomicExactValidationContext* original){return original&&(original->selected_local_economy>=11||original->born_population)?common_failure("UnselectedNamedBirthPopulationBinding",{},ErrorKind::internal):common_success();}
}
Status validate_native_born_population_read_binding(const WorldBundle& world,const EconomicExactReadBinding& binding){
 auto valid=validate_native_born_population_layout(world);if(!valid.ok)return valid;auto selected=selected_native_economic_obligation_versions(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(!selected.value.named_birth_population)return reject_unselected_population_binding(&binding.original);
 return validate_population_original_binding(world,binding.original,binding.catalog,selected.value.local_economy);
}
Status validate_native_born_population_write_binding(const WorldBundle& world,const EconomicExactWriteBinding& binding){
 auto valid=validate_native_born_population_layout(world);if(!valid.ok)return valid;auto selected=selected_native_economic_obligation_versions(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(!selected.value.named_birth_population)return reject_unselected_population_binding(binding.original);
 if(!binding.original||&binding.common!=&world.common||&binding.economy!=&world.local_economy)return common_failure("NamedBirthPopulationWorldWriteBindingMismatch",{},ErrorKind::internal);
 return validate_population_original_binding(world,*binding.original,binding.catalog,selected.value.local_economy);
}
}
