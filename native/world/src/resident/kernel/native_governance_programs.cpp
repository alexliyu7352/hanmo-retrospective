#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "native_governance_programs.hpp"
#include "world_bundle.hpp"
#include "native_organization_runtime.hpp"
#include "native_task_work_catalog.hpp"
#include "native_catalog_decode.hpp"
#include "original_catalog_identity.hpp"
#include "world_copy_plan.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
[[noreturn]]void fail(const char* code,ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,"governance_programs",kind).error};}
const BoundaryValue* author_block(const BoundaryValue& scenario){
 const BoundaryValue* found=nullptr;
 for(const auto& row:boundary::object(scenario,"scenario"))if(row.first=="governance_programs"){
  if(found)fail("DuplicateGovernanceProgramAuthorSelection");found=&row.second;
 }
 return found;
}
void version_pair(const NativeGovernanceProgramVersions& value){
 if(value.catalog==0&&value.state==0)return;
 if(value.catalog>=1&&value.catalog<=3&&value.state==1)return;
 if(value.catalog==4&&(value.state==2||value.state==3))return;
 fail("GovernanceProgramCatalogStateVersionPairRequired",ErrorKind::unsupported_capability);
}
NativeGovernanceProgramVersions author_versions(const BoundaryValue& block){
 boundary::fields(block,{"catalog_version","state_version","catalog"},{},"governance_programs");
 if(boundary::object(block,"governance_programs").size()!=3)fail("UnsupportedGovernanceProgramAuthorSelection",ErrorKind::unsupported_capability);
 NativeGovernanceProgramVersions value{boundary::get_int(block,"catalog_version","governance_programs"),boundary::get_int(block,"state_version","governance_programs")};
 if(!value.catalog||!value.state)fail("GovernanceProgramExplicitVersionsRequired");version_pair(value);return value;
}
Integer finite_version(const StableId& value){if(value=="1")return 1;if(value=="2")return 2;if(value=="3")return 3;if(value=="4")return 4;return 0;}
void work_pair(const CatalogIdentity& identity,const NativeGovernanceProgramVersions& value){
 const auto work=take(selected_native_task_work_version(identity));
 if(work==5&&(value.catalog!=4||value.state!=2))fail("TaskWork5GovernanceProgram2Required",ErrorKind::unsupported_capability);
 if(work==6&&(value.catalog!=4||value.state!=3))fail("TaskWork6GovernanceProgram3Required",ErrorKind::unsupported_capability);
 if(value.state==2&&work!=5)fail("GovernanceProgram2TaskWork5Required",ErrorKind::unsupported_capability);
 if(value.state==3&&work!=6)fail("GovernanceProgram3TaskWork6Required",ErrorKind::unsupported_capability);
}
Outcome<GovernanceProgramCatalog> decode_catalog(const BoundaryValue& value,const CatalogIdentity& identity,const CommonCatalog& common,Integer version){
 if(version==1)return decode_governance_program_catalog(value,identity);
 if(version==2)return decode_governance_program_catalog2(value,identity,common);
 if(version==3)return decode_governance_program_catalog3(value,identity,common);
 if(version==4)return decode_governance_program_catalog4(value,identity,common);
 return {false,{},common_failure("UnsupportedGovernanceProgramCatalogVersion","governance_program_catalog",ErrorKind::unsupported_capability).error};
}
}
Outcome<NativeGovernanceProgramVersions> selected_native_governance_program_versions(const CatalogIdentity& identity){try{
 NativeGovernanceProgramVersions value;
 for(const auto& row:identity.module_versions){
  if(row.first=="governance_program_catalog"){
   if(value.catalog)fail("DuplicateGovernanceProgramCatalogModule");value.catalog=finite_version(row.second);if(!value.catalog)fail("UnsupportedGovernanceProgramCatalogVersion",ErrorKind::unsupported_capability);
  }else if(row.first=="governance_programs"){
   if(value.state)fail("DuplicateGovernanceProgramsModule");value.state=finite_version(row.second);if(value.state<1||value.state>3)fail("UnsupportedGovernanceProgramsVersion",ErrorKind::unsupported_capability);
  }
 }
 version_pair(value);work_pair(identity,value);return {true,value,{}};
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status append_native_governance_program_modules(const BoundaryValue& scenario,CatalogIdentity& identity){try{
 for(const auto& row:identity.module_versions)if(row.first=="governance_program_catalog"||row.first=="governance_programs")fail("DuplicateGovernanceProgramModuleSelection");
 const auto* block=author_block(scenario);if(!block){work_pair(identity,{});return common_success();}
 const auto value=author_versions(*block);work_pair(identity,value);
 const auto catalog_version=value.catalog==1?"1":value.catalog==2?"2":value.catalog==3?"3":"4";
 const auto state_version=value.state==1?"1":value.state==2?"2":"3";
 append_native_catalog_module(identity,"governance_program_catalog",catalog_version);
 append_native_catalog_module(identity,"governance_programs",state_version);return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status validate_native_governance_program_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 136, "PreparedCatalog", "validate_native_governance_program_catalog", &catalog);
try{
 const auto selected=take(selected_native_governance_program_versions(catalog.identity));
 if(bool(selected.catalog)!=bool(catalog.governance_program_catalog))fail("GovernanceProgramSelectedCatalogMismatch");
 if(!selected.catalog)return common_success();
 if(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time)||!catalog.common||!catalog.organization||!catalog.organization_lifecycle)
  fail("GovernanceProgramCompleteOriginalCatalogsRequired",ErrorKind::unsupported_capability);
 take(validate_native_organization_runtime_catalog(catalog));
 const auto& original=*catalog.governance_program_catalog;
 if(original.schema_version!=selected.catalog||!same_original_catalog_identity(original.identity,catalog.identity))fail("GovernanceProgramCatalogIdentityMismatch");
 take(validate_governance_program_catalog(original));
 if(selected.catalog>=2)take(validate_governance_executor_qualification_skills(original,*catalog.common));
 return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status prepare_native_governance_program_catalog(const BoundaryValue& scenario,PreparedCatalog& catalog){try{
 const auto selected=take(selected_native_governance_program_versions(catalog.identity));
 const auto* block=author_block(scenario);
 if(bool(block)!=bool(selected.catalog)||catalog.governance_program_catalog)fail("GovernanceProgramSelectedCatalogMismatch");
 if(!block)return common_success();
 const auto authored=author_versions(*block);if(authored.catalog!=selected.catalog||authored.state!=selected.state)fail("GovernanceProgramAuthorModuleMismatch");
 if(!catalog.common)fail("GovernanceProgramOriginalCommonCatalogRequired");
 ScopedCheckpointJsonDecode original(CheckpointCatalogJsonKind::governance_program_catalog);
 original.default_values<GovernanceProgramCatalog>();
 // The original decoder copies its identity before entering its field visitor.
 // Reserve that actual first copy in the same enclosing decoded-owner token.
 original.ordinary(std::size_t{0});
 auto decoded=take(decode_catalog(boundary::field(*block,"catalog","governance_programs"),catalog.identity,*catalog.common,selected.catalog));
 catalog.governance_program_catalog=checkpoint_make_shared<const GovernanceProgramCatalog>(std::move(decoded));
 return validate_native_governance_program_catalog(catalog);
}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status initialize_native_governance_program_root(const PreparedCatalog& catalog,InitialWorldSeed& seed){try{
 take(validate_native_governance_program_catalog(catalog));const auto selected=take(selected_native_governance_program_versions(catalog.identity));
 if(seed.governance_programs){if(!selected.state||seed.governance_programs->schema_version!=selected.state)fail("GovernanceProgramSelectedRootMismatch");return common_success();}if(!selected.state)return common_success();
 // These are actual, initially absent runtime facts, never synthesized approvals.
 auto state=checkpoint_make_shared<GovernanceProgramState>();state->schema_version=selected.state;seed.governance_programs=std::move(state);return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_native_governance_program_layout(const WorldBundle& world){try{
 if(!world.catalog)fail("GovernanceProgramCatalogRequired");take(validate_native_governance_program_catalog(*world.catalog));
 const auto selected=take(selected_native_governance_program_versions(world.catalog->identity));
 if(bool(selected.state)!=bool(world.governance_programs)||(world.governance_programs&&world.governance_programs->schema_version!=selected.state))fail("GovernanceProgramSelectedRootMismatch");
 if(selected.state&&!world.organization_runtime)fail("GovernanceProgramOriginalOrganizationRuntimeRequired");
 return common_success();
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<GovernanceProgramState> decode_native_governance_programs(const BoundaryValue& value,Integer version){
 if(version==1)return decode_governance_programs(value);if(version==2)return decode_governance_programs2(value);if(version==3)return decode_governance_programs3(value);
 return {false,{},common_failure("UnsupportedGovernanceProgramsVersion","governance_programs",ErrorKind::unsupported_capability).error};
}
Status visit_native_governance_programs(CodecVisitor& c,GovernanceProgramState& value,Integer version){
 if(version==1)return visit_governance_programs(c,value);if(version==2)return visit_governance_programs2(c,value);if(version==3)return visit_governance_programs3(c,value);
 return common_failure("UnsupportedGovernanceProgramsVersion","governance_programs",ErrorKind::unsupported_capability);
}
Status emit_native_governance_programs_projection(ProjectionWriter& out,std::string_view name,const GovernanceProgramState& value,Integer version){
 if(version==1)return emit_governance_programs_projection(out,name,value);if(version==2)return emit_governance_programs2_projection(out,name,value);if(version==3)return emit_governance_programs3_projection(out,name,value);
 return common_failure("UnsupportedGovernanceProgramsVersion","governance_programs",ErrorKind::unsupported_capability);
}
}
