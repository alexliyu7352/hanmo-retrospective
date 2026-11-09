#include "native_complete_world_registry.hpp"
#include "native_interaction_catalog.hpp"
#include "native_private_shop_operations.hpp"
#include "resident/domains/commerce/private_shop_action_time.hpp"
#include "resident/domains/interaction/interaction_exact_phase.hpp"
#include "native_transport_catalog.hpp"
#include "resident/domains/military/military_exact_phase.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/growth/growth_action_time_read.hpp"
#include "resident/domains/growth/growth_exact_phase.hpp"
#include "native_complete_time_catalog.hpp"
#include "resident/facility_claims/facility_exact_phase.hpp"
#include "resident/domains/travel/movement_exact_phase.hpp"
#include "resident/domains/travel/native_land_motion_phase.hpp"
#include "resident/domains/affairs/state_need_exact_phase.hpp"
#include "resident/domains/politics/political_treaty_exact_phase.hpp"
#include "native_organization_runtime.hpp"
#include "native_life_age_world.hpp"
#include "native_production_cycles.hpp"
#include "native_item_craft_state.hpp"
#include "resident/domains/craft/craft_exact_phase.hpp"
#include "native_task_work_catalog.hpp"
#include "native_task_execution_state.hpp"
#include "native_director_catalog.hpp"
#include "resident/domains/director/director_exact_runtime.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "hanmo/resident/model_contract.hpp"
#include "resident/domains/actor_foundation/model.hpp"
#include "resident/domains/tasks/task_exact_execution.hpp"
#include "resident/domains/social/social_exact_phase.hpp"
#include "resident/domains/interaction/native_s3_complete_phase.hpp"
#include "resident/domains/travel/movement_action_time.hpp"
#include "resident/domains/travel/cargo_handling.hpp"
#include "resident/domains/local_economy/household_purchase_contract.hpp"
namespace hanmo::resident {
Outcome<std::vector<StableId>> read_native_complete_selected_legacy_writer_keys(const PreparedCatalog& catalog){
    if(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time))return {false,{},common_failure("CompleteExactProfileRequired").error};
    // phase_plan::advance_world拒绝所有Time2；dispatcher唯一advance路由为advance_exact_world。
    // 原primitive仍由各compiled phase调用，不再独立进入17段legacy scheduler。
    return {true,{}, {}};
}
Outcome<TaskWorkWorldFactory> read_native_complete_task_work_factory(const PreparedCatalog& catalog){
 if(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time))
  return {false,{},common_failure("CompleteExactProfileRequired").error};
 auto task=validate_native_task_execution_catalog(catalog);if(!task.ok)return {false,{},std::move(task.error)};
 auto work=validate_native_task_work_catalog(catalog);if(!work.ok)return {false,{},std::move(work.error)};
 // This is the actual compiled pair. An absent Work selection is not an empty
 // provider: the original Task phase decides whether Work state is selected.
 // The callbacks acquire the same-world real owner only when they are used.
 return {true,native_task_work_world_factory(),{}};
}
Outcome<SocialCalendarDispatchFunction> read_native_complete_social_dispatch(const PreparedCatalog& catalog){
 if(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time))
  return {false,nullptr,common_failure("CompleteExactProfileRequired").error};
 auto selected=validate_native_director_catalog_selection(catalog);
 if(!selected.ok)return {false,nullptr,std::move(selected.error)};
 if(!catalog.director||!catalog.director->subjects2)
  return {false,nullptr,common_failure("CompleteDirectorOriginalSubjectsRequired",{},ErrorKind::unsupported_capability).error};
 // Only the original Director decision route may suppress its same-effect
 // Social calendar leg. This compiled function acquires the actual same-world
 // owning read lease; no default dispatch or second rule is installed here.
 return {true,&read_native_social_calendar_dispatch,{}};
}
Status prepare_native_complete_exact_runtime(PreparedCatalog&catalog,const std::vector<StableId>&legacy,const std::vector<FacilityHolderRegistration>&holders,TaskWorkWorldFactory work,SocialCalendarDispatchFunction social_dispatch){
 if(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time))return catalog.complete_exact_time_runtime?common_failure("UnexpectedCompleteExactRuntime"):common_success();
 if(catalog.complete_exact_time_runtime)return common_failure("CompleteExactRuntimeAlreadyPrepared");
 auto routes=validate_native_complete_world_factory_routes(native_complete_world_factory_registry());if(!routes.ok)return routes;
 if(!catalog.actor_foundation||catalog.actor_foundation->actors.empty())return common_failure("CompleteExactOriginalScenarioRequired");
 const auto initial=catalog.actor_foundation->actors.rows().front().second.scenario_origin_ref.initial_tick;
 for(const auto&row:catalog.actor_foundation->actors.rows())if(row.second.scenario_origin_ref.initial_tick!=initial)return common_failure("CompleteExactOriginalScenarioBoundaryMismatch",row.first);
 // These are actual compiled factories already delivered by their unique owners. A full
 // selection cannot pass until every remaining real factory is added here; no empty callback
 // or reduced profile is substituted. K2 compose checks stages, owner coverage and exclusivity.
 std::vector<ExactPhaseRegistration>phases;
 auto facility=make_facility_exact_phase_registration(holders);if(!facility.ok)return {false,{},std::move(facility.error)};
 phases.push_back(std::move(facility.value));
 auto state_need=read_state_need_selected_version(catalog.identity);if(!state_need.ok)return {false,{},std::move(state_need.error)};
 // 未选仅按真实0证明：不为后置出生/Care强造政策；成人Medical实际选中仍要2原源。
 if(state_need.value!=0&&state_need.value!=2)return common_failure("CompleteOriginalStateNeedSelectionRequired",{},ErrorKind::unsupported_capability);
 if(!state_need.value){auto absent=validate_state_need_catalog_absence(catalog);if(!absent.ok)return absent;}
 phases.push_back(make_state_need_deadline_phase_registration(native_state_need_world_factory(),state_need.value));
 auto organization=validate_native_organization_runtime_catalog(catalog);if(!organization.ok)return organization;
 const ExactPhaseSelection* political=nullptr;
 for(const auto& selected:catalog.exact_time->complete->phase_providers)if(selected.provider==ExactProviderId::politics){
  if(political)return common_failure("DuplicateCompletePoliticalPhaseSelection");political=&selected;
 }
 if(!political||political->profile_id!="organization_politics_original_calendar"||
    (political->version!=1&&political->version!=2))
  return common_failure("CompleteOriginalPoliticalPhaseRequired",{},ErrorKind::unsupported_capability);
 // Both are the same original provider. Treaty v2 is an explicit author
 // selection, never inferred from a nullable binding or today's rows.
 phases.push_back(political->version==2?
  make_political_treaty_exact_phase_registration(native_political_exact_world_factory()):
  make_political_exact_phase_registration(native_political_exact_world_factory()));
 if(!work.read||!work.write||!social_dispatch)return common_failure("CompleteOriginalTaskSocialFactoryRequired",{},ErrorKind::unsupported_capability);
 auto growth_version=read_complete_growth_phase_version(*catalog.exact_time);if(!growth_version.ok)return {false,{},std::move(growth_version.error)};
 if(!catalog.growth||!catalog.growth->complete_study_sources||!catalog.growth->complete_exact_period_sources||catalog.growth->complete_exact_command_cuts!=(growth_version.value>=2)||catalog.growth->complete_exact_rest_mouth_sources!=(growth_version.value>=3)||catalog.growth->complete_capability_application_chain!=(growth_version.value==4))return common_failure("GrowthExactCatalogSelectionMismatch");
 auto growth=make_growth_exact_phase_registration();growth.version=growth_version.value;phases.push_back(std::move(growth));
 auto task_catalog=validate_native_task_execution_catalog(catalog);if(!task_catalog.ok)return task_catalog;
 auto task_execution=selected_native_task_execution_version(catalog.identity);if(!task_execution.ok)return {false,{},std::move(task_execution.error)};
 if(!task_execution.value)return common_failure("CompleteOriginalTaskExecutionSelectionRequired",{},ErrorKind::unsupported_capability);
 phases.push_back(make_task_exact_phase_registration(work,task_execution.value));
 phases.push_back(make_native_s3_complete_phase_registration(initial,social_dispatch));
 // The one Complete Family route includes the original lifecycle tail. Its
 // compiled construction must really exist before graph composition; a mere
 // list of birth/guardianship stages cannot satisfy the required graph.
 if(catalog.life_age_checks){
  auto family=make_native_complete_family_phase_registration(native_life_age_world_factory());
  if(!family.ok)return {false,{},std::move(family.error)};
  phases.push_back(std::move(family.value));
 }
 if(catalog.production_cycles)phases.push_back(make_economic_exact_phase_registration(native_economic_exact_world_factory()));
 auto item_craft_catalog=validate_native_item_craft_catalogs(catalog);if(!item_craft_catalog.ok)return item_craft_catalog;
 auto item_craft=selected_native_item_craft_versions(catalog.identity);if(!item_craft.ok)return {false,{},std::move(item_craft.error)};
 // Craft and Repair are explicit selections in the one original crafting
 // provider. Repair-only never asks for a Craft root or a Craft interval token.
 if(item_craft.value.craft_jobs||item_craft.value.item_instances==3)phases.push_back(make_craft_exact_phase_registration(
  item_craft.value.craft_jobs?native_craft_exact_world_factory():CraftExactWorldFactory{},
  item_craft.value.item_instances==3?native_repair_exact_world_factory():RepairExactWorldFactory{}));
 auto interaction_selected=validate_native_interaction_catalog(catalog);if(!interaction_selected.ok)return interaction_selected;
 auto interaction=selected_native_interaction_versions(catalog.identity);if(!interaction.ok)return {false,{},std::move(interaction.error)};
 if(interaction.value.exact_actions())phases.push_back(make_interaction_exact_phase_registration(native_interaction_exact_world_factories()));
 auto military=read_military_exact_phase_selection(catalog);if(!military.ok)return {false,{},std::move(military.error)};
 phases.push_back(make_military_exact_phase_registration(native_military_exact_world_factory(),military.value));
 auto actions=task_action_time_registrations(task_execution.value);
 for(auto& action:military_exact_action_time_registrations(military.value))actions.push_back(std::move(action));
 if(item_craft.value.craft_jobs)actions.push_back(craft_action_time_registration());
 if(item_craft.value.item_instances==3)actions.push_back(repair_action_time_registration());
 auto shop_catalog=validate_native_private_shop_operation_catalog(catalog);if(!shop_catalog.ok)return shop_catalog;
 auto shop_operations=selected_native_private_shop_operation_versions(catalog.identity);if(!shop_operations.ok)return {false,{},std::move(shop_operations.error)};
 if(shop_operations.value.exact_operations())actions.push_back(private_shop_operation_action_time_registration());
 if(interaction.value.personal==2)actions.push_back(personal_appointment_action_time_registration());
 if(interaction.value.inquiry==2)actions.push_back(contact_inquiry_action_time_registration());
 if(interaction.value.meetings==2)actions.push_back(visit_appointment_meeting_action_time_registration());
 auto growth_actions=make_growth_action_time_registrations(growth_version.value);if(!growth_actions.ok)return {false,{},std::move(growth_actions.error)};for(auto& action:growth_actions.value)actions.push_back(std::move(action));
 auto work_catalog=validate_native_task_work_catalog(catalog);if(!work_catalog.ok)return work_catalog;
 auto work_version=selected_native_task_work_version(catalog.identity);if(!work_version.ok)return {false,{},std::move(work_version.error)};
 if(work_version.value==5||work_version.value==6)for(auto& action:supervision_action_time_registrations(work_version.value))actions.push_back(std::move(action));
 auto movement=selected_native_transport_versions(catalog.identity);if(!movement.ok)return {false,{},std::move(movement.error)};
 if(!movement.value.travel)return common_failure("CompleteExactTravelSelectionRequired",{},ErrorKind::unsupported_capability);
 phases.push_back(make_native_land_motion_phase_registration(native_motion_exact_world_factory()));
 actions.push_back(movement_action_time_registration(movement.value.travel));
 actions.push_back(cargo_handling_action_time_registration());
 if(catalog.household_purchases){
  if(!catalog.household_retail_settlements||(catalog.selected_household_version!=2&&catalog.selected_household_version!=3))return common_failure("HouseholdCatalogSelectionMismatch");
  actions.push_back(catalog.selected_household_version==3?household_purchase_action_time_registration_v3():household_purchase_action_time_registration());
 }else if(catalog.household_retail_settlements||catalog.selected_household_version)return common_failure("HouseholdCatalogSelectionMismatch");
 auto remaining=append_native_complete_selected_phase_routes(catalog,phases);if(!remaining.ok)return remaining;
 auto runtime=compose_complete_exact_time_profile(*catalog.exact_time,phases,actions,legacy,holders);
 if(!runtime.ok)return {false,{},runtime.error};
 catalog.complete_exact_time_runtime=checkpoint_make_shared<const CompleteExactTimeRuntime>(std::move(runtime.value));
 return common_success();
}
}
