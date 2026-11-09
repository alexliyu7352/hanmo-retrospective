// 接受与实际服务均写原同候选；外层负责World回滚，冷态精确绑定原回执与本人事实。
#include "estate_employment_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/tasks/task_employment_view.hpp"
#include <set>
namespace hanmo::resident {namespace {
// 旧无段世界不能接收新命令或残留新账，默认初始化不等于恢复成功。
Status enabled(const WorldBundle& w){return w.estate_employment_enabled()&&w.estate_employment_seed_complete&&w.dynamic_enabled()&&w.catalog->estate&&w.catalog->common&&w.catalog->interaction&&w.catalog->local_economy?common_success():common_failure("EstateEmploymentProfileRequired");}
bool command_kind(const StableId& kind){return kind=="estate_employment_offer"||kind=="estate_employment_decide"||kind=="estate_employment_request_offer";}
}
EstateEmploymentWorldAccess::EstateEmploymentWorldAccess(const WorldBundle& w,const DynamicStepInput* phase):estate_(w.services.estate_access()),interaction_(w.services.interaction_access()),context_{w.common,*w.catalog->common,w.estate,*w.catalog->estate,w.local_economy,*w.catalog->local_economy,w.estate_employment,*w.catalog->estate_employment,w.services,&estate_,phase,w.catalog->interaction.get(),w.family_life_enabled()?&w.family_life:nullptr,w.family_birth_enabled()?&w.family_birth:nullptr},meeting_{w.interaction,*w.catalog->interaction,interaction_}{}
Status validate_estate_employment_world(const WorldBundle& w){
 auto layout=validate_estate_employment_layout(w.estate_employment_enabled(),w.catalog->identity.module_versions);if(!layout.ok)return layout;
 if(w.estate_employment_enabled()!=w.estate_employment_seed_complete)return common_failure("EstateEmploymentSeedRequired");
 if(w.estate_employment_enabled()){auto available=enabled(w);if(!available.ok)return available;EstateEmploymentWorldAccess access(w);auto valid=validate_estate_employment(access.context());if(!valid.ok)return valid;}
 else if(!w.estate_employment.contracts.empty()||!w.estate_employment.decisions.empty()||!w.estate_employment.services.empty()||w.estate_employment.initialized_tick||w.estate_employment.last_step_tick)return common_failure("UnexpectedEstateEmploymentState");
 for(const auto& head:w.common.state().facts.rows())if(head.second.domain==estate_employment_module&&!w.estate_employment_enabled())return common_failure("EstateEmploymentProfileRequired");
 for(const auto& row:w.common.state().history)if(row.kind.rfind("estate_employment_",0)==0&&(!w.estate_employment_enabled()||!command_kind(row.kind)))return common_failure("InvalidEmploymentCommandHistory");
 std::set<StableId> receipt_facts;
 for(const auto& row:w.common.state().results.rows())if(row.second){const auto& result=*row.second;const auto* receipt=result.details?std::get_if<EstateReceipt>(&result.details->data):nullptr;const bool source=receipt&&receipt->fact_id.rfind("estate_employment",0)==0;if(!source&&result.kind.rfind("estate_employment_",0)!=0)continue;
  if(!w.estate_employment_enabled()||!command_kind(result.kind)||!receipt||!source||receipt->actor_ids||!receipt_facts.insert(receipt->fact_id).second)return common_failure("InvalidEmploymentReceipt",row.first);
  auto actor=estate_employment_receipt_actor(w.common.state(),row.first,result.kind,result.result_id,result.tick,receipt->fact_id);if(!actor.ok)return {false,{},actor.error};
  auto bound=validate_estate_employment_receipt(w.estate_employment,result.kind,actor.value,result.tick,*receipt);if(!bound.ok)return bound;
 }
 return common_success();
}
Outcome<EstateEmploymentQuote> prepare_estate_employment_world(const WorldBundle& w,const StableId& actor,const EstateEmploymentCommand& command){auto ready=enabled(w);if(!ready.ok)return {false,{},ready.error};EstateEmploymentWorldAccess access(w);return preview_estate_employment(access.context(),access.meeting(),actor,command);}
Outcome<EstateReceipt> commit_estate_employment_world(WorldBundle& w,const StableId& actor,const EstateEmploymentCommand& command,const EstateEmploymentQuote& quote){auto ready=enabled(w);if(!ready.ok)return {false,{},ready.error};EstateEmploymentWorldAccess access(w);auto result=apply_estate_employment_in_candidate(w.common,w.estate,w.local_economy,w.estate_employment,access.context(),access.meeting(),actor,command,quote);if(!result.ok)return {false,{},result.error};const auto* contract=w.estate_employment.contracts.find(result.value.contract_id);if(!contract)return {false,{},common_failure("MissingEmploymentContract").error};return {true,{contract->terms.estate_id,result.value.fact_id,{}},{}};}
Outcome<BoundaryValue> query_estate_employment_world(const WorldBundle& w,Integer cursor,Integer policy_cursor){auto ready=enabled(w);if(!ready.ok)return {false,{},ready.error};EstateEmploymentWorldAccess access(w);EstateEmploymentAccess read(access.context(),access.meeting());return query_task_employment(w.common,*w.catalog->common,w.interaction,read,[&](const StableId& actor,const EstateEmploymentCommand& c){return preview_estate_employment(access.context(),access.meeting(),actor,c);},cursor,policy_cursor);}
Status step_estate_employment_world(WorldBundle& w,const DynamicStepInput& phase){if(!w.estate_employment_enabled())return common_success();auto ready=enabled(w);if(!ready.ok)return ready;EstateEmploymentWorldAccess access(w,&phase);auto result=step_estate_employment(w.common,w.estate,w.local_economy,w.estate_employment,access.context(),phase);return {result.ok,{},result.error};}
Status finish_estate_employment_world(WorldBundle& w){if(!w.estate_employment_enabled())return common_success();auto ready=enabled(w);if(!ready.ok)return ready;EstateEmploymentWorldAccess access(w);auto result=close_deceased_estate_employment(w.common,w.estate,w.local_economy,w.estate_employment,access.context());return {result.ok,{},result.error};}
Status guard_estate_employment_world_management(const WorldBundle& w,const EstateCommand& command){if(!w.estate_employment_enabled())return common_success();EstateEmploymentWorldAccess access(w);return guard_estate_employment_management(access.context(),command);}
}
