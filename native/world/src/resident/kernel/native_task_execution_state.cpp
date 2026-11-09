#include "resident/kernel/catalog_section_observation.hpp"
#include "native_task_execution_state.hpp"
#include "native_catalog_decode.hpp"
#include "world_bundle.hpp"
#include "resident/domains/tasks/task_exact_execution.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {namespace {
Status phase_selection(const PreparedCatalog& catalog,Integer selected){
 if(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time))
  return selected?common_failure("TaskExecutionCompleteProfileRequired",{},ErrorKind::unsupported_capability):common_success();
 if(!catalog.exact_time->complete)return common_failure("TaskExecutionCompleteSelectionRequired");
 bool phase=false;
 for(const auto& row:catalog.exact_time->complete->phase_providers)if(row.provider==ExactProviderId::tasks){
  if(phase||!selected||row.profile_id!="task_execution_time2"||row.version!=selected)
   return common_failure("TaskExecutionPhaseSelectionMismatch");
  phase=true;
 }
 if(bool(selected)!=phase)return common_failure("TaskExecutionPhaseSelectionMismatch");
 for(const char* kind:{"accept_offer","report_offer","deliver_message"}){
  bool present=false;
  for(const auto& row:catalog.exact_time->complete->action_time_profiles)if(row.action_kind==kind){
   if(present||!selected||row.profile_id!="task_execution_time2"||row.version!=selected)
    return common_failure("TaskExecutionActionSelectionMismatch",row.action_kind);
   present=true;
  }
  if(bool(selected)!=present)return common_failure("TaskExecutionActionSelectionMismatch",kind);
 }
 for(const auto& row:catalog.exact_time->complete->action_time_profiles)
  if(row.profile_id=="task_execution_time2"&&!exact_task_action_kind(row.action_kind))
   return common_failure("UnsupportedTaskExecutionActionSelection",row.action_kind);
 return common_success();
}
Status original_life4(const PreparedCatalog& catalog){
 if(!catalog.family_life||!catalog.family_life->enabled||catalog.family_life->selected_version!=4||!catalog.actor_foundation)
  return common_failure("TaskExecutionOriginalLife4CatalogRequired",{},ErrorKind::unsupported_capability);
 bool life=false;
 for(const auto& row:catalog.identity.module_versions)if(row.first=="family_life"){
  if(life||row.second!="4")return common_failure("TaskExecutionOriginalLife4ModuleRequired");life=true;
 }
 return life?common_success():common_failure("TaskExecutionOriginalLife4ModuleRequired",{},ErrorKind::unsupported_capability);
}
}
Outcome<Integer> decode_native_task_execution_selection(const BoundaryValue& scenario){try{
 const BoundaryValue* selected=nullptr;
 for(const auto& row:boundary::object(scenario,"scenario"))if(row.first=="task_execution_time"){
  if(selected)boundary::fail("DuplicateTaskExecutionSelection",row.first);selected=&row.second;
 }
 if(!selected)return {true,0,{}};
 boundary::fields(*selected,{"profile"},{},"task_execution_time");
 if(boundary::object(*selected,"task_execution_time").size()!=1)
  boundary::fail("InvalidTaskExecutionSelection","task_execution_time");
 const auto& profile=boundary::get_string_ref(*selected,"profile","task_execution_time");
 if(profile=="original_task_execution_time_v1")return {true,1,{}};
 if(profile=="original_task_execution_time_v2")return {true,2,{}};
 boundary::unsupported("UnsupportedTaskExecutionSelection","task_execution_time/profile");
}catch(const boundary::DecodeFailure& e){return {false,0,e.error};}}
Outcome<Integer> selected_native_task_execution_version(const CatalogIdentity& identity){
 Integer selected=0;
 for(const auto& row:identity.module_versions)if(row.first=="task_execution_time"){
  if(selected||(row.second!="1"&&row.second!="2"))
   return {false,0,common_failure("TaskExecutionModuleMismatch",row.first).error};
  selected=row.second=="1"?1:2;
 }
 return {true,selected,{}};
}
Status append_native_task_execution_module(const BoundaryValue& scenario,CatalogIdentity& identity){
 auto selected=decode_native_task_execution_selection(scenario);if(!selected.ok)return {false,{},std::move(selected.error)};
 for(const auto& row:identity.module_versions)if(row.first=="task_execution_time")
  return common_failure("DuplicateTaskExecutionModule",row.first);
 if(selected.value)append_native_catalog_module(identity,"task_execution_time",selected.value==1?"1":"2");
 return common_success();
}
Status validate_native_task_execution_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 156, "PreparedCatalog", "validate_native_task_execution_catalog", &catalog);

 auto selected=selected_native_task_execution_version(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 auto phase=phase_selection(catalog,selected.value);if(!phase.ok)return phase;
 if(!selected.value)return common_success();
 if(!catalog.tasks||!catalog.common||!catalog.interaction||!catalog.social)
  return common_failure("TaskExecutionOriginalCatalogsRequired",{},ErrorKind::unsupported_capability);
 // The complete original Body reader is supplied by the owning World graph,
 // never created here or substituted by catalog presence/current actor flags.
 return selected.value==2?original_life4(catalog):common_success();
}
Status prepare_native_task_execution_catalog(const BoundaryValue& scenario,PreparedCatalog& catalog){
 auto authored=decode_native_task_execution_selection(scenario);if(!authored.ok)return {false,{},std::move(authored.error)};
 auto selected=selected_native_task_execution_version(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(authored.value!=selected.value)return common_failure("TaskExecutionAuthoredModuleMismatch");
 return validate_native_task_execution_catalog(catalog);
}
Status initialize_native_task_execution_root(const CommonStore& common,const PreparedCatalog& catalog,TaskState& tasks){
 auto valid=validate_native_task_execution_catalog(catalog);if(!valid.ok)return valid;
 auto selected=selected_native_task_execution_version(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(tasks.exact_execution)return common_failure("TaskExecutionInitialRootAlreadyPresent",{},ErrorKind::internal);
 // Fresh seed only. This port neither adopts legacy Task history nor repairs a
 // missing selected root in a restored world. Empty optional child roots remain
 // the responsibility of their own explicit selectors.
 if(selected.value){
  if(!tasks.tasks.empty()||!tasks.actions.empty())return common_failure("TaskExecutionFreshInitialTaskRequired");
  for(const auto& row:common.state().actions.rows())if(exact_task_action_kind(row.second.kind))
   return common_failure("TaskExecutionFreshInitialTaskRequired",row.first);
  valid=validate_task_exact_execution_headers(common,tasks,0);if(!valid.ok)return valid;
  auto fresh=make_initial_task_exact_execution(selected.value);if(!fresh.ok)return {false,{},std::move(fresh.error)};
  tasks.exact_execution.emplace(std::move(fresh.value));
 }
 return validate_task_exact_execution_headers(common,tasks,selected.value);
}
Status validate_native_task_execution_layout(const CommonStore& common,const PreparedCatalog& catalog,const TaskState& tasks){
 auto valid=validate_native_task_execution_catalog(catalog);if(!valid.ok)return valid;
 auto selected=selected_native_task_execution_version(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 return validate_task_exact_execution_headers(common,tasks,selected.value);
}
Status validate_native_task_execution_layout(const WorldBundle& world){
 if(!world.catalog)return common_failure("TaskExecutionCatalogRequired",{},ErrorKind::internal);
 auto valid=validate_native_task_execution_layout(world.common,*world.catalog,world.tasks);if(!valid.ok)return valid;
 auto selected=selected_native_task_execution_version(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(selected.value&&(!world.world_timeline||!world.action_elapsed))
  return common_failure("TaskExecutionOriginalElapsedAndScaleRequired",{},ErrorKind::unsupported_capability);
 if(selected.value==2&&!world.family_life_seed_complete)
  return common_failure("TaskExecutionOriginalLife4StateRequired",{},ErrorKind::unsupported_capability);
 // This is not validate_task_exact_execution: actual Body/Death/S0 source
 // admission and complete consumer ownership must still run before publication.
 return common_success();
}
Status emit_native_task_execution_fact_payloads_fields(ProjectionWriter& out,const CommonStore& common,
 const PreparedCatalog& catalog,const TaskState& tasks){
 auto valid=validate_native_task_execution_layout(common,catalog,tasks);if(!valid.ok)return valid;
 auto selected=selected_native_task_execution_version(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 for(const auto& row:common.state().facts.rows()){
  const auto& header=row.second;if(!owns_task_exact_original_fact_kind(header.kind))continue;
  auto emitted=out.object(row.first,header.field_order,[&](auto& fields)->Status{
   auto payload=emit_task_exact_original_fact_payload_fields(fields,common,tasks,header,selected.value);
   if(!payload.ok)return {false,{},std::move(payload.error)};
   if(!payload.value)return common_failure("TaskExecutionOriginalFactPayloadRequired",row.first);
   fields.field("kind",header.kind);return fields.field("domain",header.domain);
  });
  if(!emitted.ok)return emitted;
 }
 return out.status();
}
}
