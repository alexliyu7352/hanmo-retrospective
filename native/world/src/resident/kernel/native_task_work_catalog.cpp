#include "resident/kernel/catalog_section_observation.hpp"
#include "native_task_work_catalog.hpp"
#include "native_work_lifecycle_command_route.hpp"
#include "native_complete_time_catalog.hpp"
#include "native_catalog_decode.hpp"
#include "world_bundle.hpp"
#include "resident/domains/tasks/work_execution/work_execution_contract.hpp"
#include "resident/domains/tasks/work_execution/work_supervision_execution_terms.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
namespace {
struct Failure {RuleError error;};
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
[[noreturn]]void fail(const char* code,ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,"task_work_execution",kind).error};}
Integer author(const BoundaryValue& scenario){
 const auto* dynamic=scenario.find("dynamic_world");if(!dynamic)return 0;
 const BoundaryValue* selected=nullptr;
 for(const auto& row:boundary::object(*dynamic,"scenario/dynamic_world"))if(row.first=="task_work_execution"){
  if(selected)fail("DuplicateTaskWorkAuthorSelection");selected=&row.second;
 }
 if(!selected)return 0;
 boundary::fields(*selected,{"version"},{},"task_work_execution");
 const auto version=boundary::get_int(*selected,"version","task_work_execution");
 if(boundary::object(*selected,"task_work_execution").size()!=1||(version!=5&&version!=6))
  fail("UnsupportedTaskWorkAuthorVersion",ErrorKind::unsupported_capability);
 auto exact=take(decode_exact_time_profile(scenario));
 if(!exact||!exact_complete_selected(*exact))fail("TaskWorkCompleteProfileRequired",ErrorKind::unsupported_capability);
 return version;
}
// Work's exact acceptance is owned by the selected WorkTaskAcceptanceSource,
// not by an invented Task /3 or an upgrade of the legacy dynamic Task /2 wire.
void original_task_base(const CatalogIdentity& identity){
 bool found=false;
 for(const auto& row:identity.module_versions)if(row.first=="tasks"){
  if(found||row.second!="1")fail("TaskWorkOriginalTaskSection1Required",ErrorKind::unsupported_capability);found=true;
 }
 if(!found)fail("TaskWorkOriginalTaskSection1Required",ErrorKind::unsupported_capability);
}
}
Outcome<Integer> selected_native_task_work_version(const CatalogIdentity& identity){
 Integer selected=0;
 for(const auto& row:identity.module_versions)if(row.first=="task_work_execution"){
  if(selected||(row.second!="5"&&row.second!="6"))return {false,0,common_failure("TaskWorkModuleVersionMismatch",row.first,ErrorKind::internal).error};selected=row.second=="5"?5:6;
 }
 return {true,selected,{}};
}
Status append_native_task_work_module(const BoundaryValue& scenario,CatalogIdentity& identity){try{
 for(const auto& row:identity.module_versions)if(row.first=="task_work_execution")fail("DuplicateTaskWorkModuleSelection");
 const auto selected=author(scenario);if(selected)append_native_catalog_module(identity,"task_work_execution",selected==5?"5":"6");return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status validate_native_task_work_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 157, "PreparedCatalog", "validate_native_task_work_catalog", &catalog);
try{
 if(!take(selected_native_task_work_version(catalog.identity)))return common_success();
 if(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time)||!catalog.tasks||!catalog.common||!catalog.actor_foundation)
  fail("TaskWorkOriginalCatalogRequired",ErrorKind::unsupported_capability);
 original_task_base(catalog.identity);return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status initialize_native_task_work_root(const PreparedCatalog& catalog,TaskState& tasks){try{
 take(validate_native_task_work_catalog(catalog));if(tasks.work_execution)fail("TaskWorkInitialRootAlreadyPresent");
 const auto selected=take(selected_native_task_work_version(catalog.identity));
 if(selected)tasks.work_execution=take(make_initial_task_work_execution(selected));return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_native_task_work_layout(const PreparedCatalog& catalog,const TaskState& tasks){try{
 take(validate_native_task_work_catalog(catalog));const auto selected=take(selected_native_task_work_version(catalog.identity));
 if(bool(selected)!=bool(tasks.work_execution)||(tasks.work_execution&&tasks.work_execution->version!=selected))fail("TaskWorkSelectedStateMismatch");
 if(selected&&!tasks.exact_execution)fail("TaskWorkOriginalTaskSourcesRequired",ErrorKind::unsupported_capability);
 return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_native_task_work_world(const WorldBundle& world){try{
 if(!world.catalog)fail("TaskWorkCatalogRequired");take(validate_native_task_work_layout(*world.catalog,world.tasks));
 take(validate_native_work_lifecycle_command_history(world));
 take(validate_task_work_original_fact_headers(world.common,world.tasks.work_execution?&*world.tasks.work_execution:nullptr));
 take(validate_task_work_original_action_headers(world.common,world.tasks.work_execution?&*world.tasks.work_execution:nullptr));
 if(!world.tasks.work_execution)return common_success();
 auto factory=take(read_native_complete_task_work_factory(*world.catalog));
 if(!factory.read||!factory.write)fail("TaskWorkOriginalWorldFactoryRequired",ErrorKind::unsupported_capability);
 return validate_supervision_world_sources(world,take(world.actual_phase_moment()),factory);
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status emit_native_task_work_section(ProjectionWriter& out,const WorldBundle& world){
 auto valid=validate_native_task_work_world(world);if(!valid.ok)return valid;
 return world.tasks.work_execution?emit_task_work_execution_projection(out,"task_work_execution",*world.tasks.work_execution):common_success();
}
}
