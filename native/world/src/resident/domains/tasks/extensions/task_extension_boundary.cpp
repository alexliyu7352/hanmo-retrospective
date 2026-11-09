// 严格输入与幂等回执共用唯一业务名；不同请求不能把旧同意换挂成再次延期。
#include "task_extension_boundary.hpp"
#include "resident/kernel/command_types.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident {namespace {
std::optional<Integer> serial(const StableId& id,const StableId& prefix,Integer next){const auto head=prefix+":";if(id.rfind(head,0)!=0)return {};const auto text=id.substr(head.size());Integer n=0;const auto p=std::from_chars(text.data(),text.data()+text.size(),n);if(p.ec!=std::errc{}||p.ptr!=text.data()+text.size()||n<1||n>=next||std::to_string(n)!=text)return {};return n;}
// 下界取紧前全局结果；首保留项允许历史裁剪，但仍须引用本次之前同人最后一份真实同意。
Status interval(const CommonStore& common,const TaskExtensionState& state,const StableId& key,const CommitResult& result,const TaskExtensionFact& source){
    const auto split=key.rfind(':');if(split==StableId::npos)return common_failure("InvalidTaskExtensionCacheKey",key);const auto actor=key.substr(0,split),digits=key.substr(split+1);Integer request=0;const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),request);const auto* sequence=common.state().request_sequences.find(actor);
    if(!common.actor(actor)||!sequence||digits.empty()||parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||request<1||request>2147483647||request>*sequence||std::to_string(request)!=digits)return common_failure("InvalidTaskExtensionCacheKey",key);
    const HistoryRecord* previous=nullptr;const HistoryRecord* selected=nullptr;const HistoryRecord* prior=nullptr;for(const auto& row:common.state().history){if(row.result_id==result.result_id){if(selected)return common_failure("TaskExtensionCachedHistoryMismatch",key);selected=&row;prior=previous;}previous=&row;}
    if(!selected||selected->actor_id!=actor||selected->kind!=task_extension_command_kind||selected->tick!=result.tick||source.actor_id!=actor||source.granted_tick!=result.tick||result.tick<0||result.tick>common.tick())return common_failure("TaskExtensionCachedHistoryMismatch",key);
    const auto next=common.state().next_instance_id;const auto lower=prior?serial(prior->result_id,"result",next):std::optional<Integer>{0},upper=serial(result.result_id,"result",next),origin=serial(source.fact_id,task_extension_fact_prefix,next);if(!lower||!upper||!origin||*lower>=*origin||*origin>=*upper)return common_failure("TaskExtensionCachedAllocationMismatch",key);
    for(const auto& row:state.facts.rows())if(row.second.actor_id==actor){const auto n=serial(row.first,task_extension_fact_prefix,next);if(!n||(*n>*origin&&*n<*upper))return common_failure("TaskExtensionCachedAllocationMismatch",key);}return common_success();
}
}
BoundaryValue encode_task_extension_command(const TaskExtensionCommand& command){return boundary::obj({{"task_id",BoundaryValue{command.task_id}},{"policy_id",BoundaryValue{command.policy_id}}});}
Outcome<TaskExtensionCommand> decode_task_extension_command(const StableId& kind,const BoundaryValue& value){try{using namespace boundary;if(kind!=task_extension_command_kind)unsupported("UnsupportedTaskExtensionCommand",kind);std::set<StableId> seen;for(const auto& row:object(value,kind))if(!seen.insert(row.first).second)fail("DuplicateTaskExtensionCommandField",row.first);fields(value,{"task_id","policy_id"},{},kind);TaskExtensionCommand command{get_string(value,"task_id",kind),get_string(value,"policy_id",kind)};if(command.task_id.empty()||command.task_id.size()>256||command.policy_id.empty()||command.policy_id.size()>256)fail("InvalidTaskExtensionCommand",kind);return {true,std::move(command),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status validate_task_extension_cached_results(const CommonStore& common,const TaskExtensionState* state){
    for(const auto& row:common.state().history)if(row.kind==task_extension_command_kind&&!state)return common_failure("TaskExtensionProfileRequired",row.result_id);
    std::set<StableId> sources;for(const auto& row:common.state().results.rows())if(row.second){const auto& result=*row.second;const auto* effect=result.details?std::get_if<InteractionEffect>(&result.details->data):nullptr;const bool owned=effect&&effect->settlement_result.rfind(StableId{task_extension_fact_prefix}+":",0)==0;if(result.kind!=task_extension_command_kind&&!owned)continue;
        if(!state||result.kind!=task_extension_command_kind||!result.ok||!effect||!owned||!effect->encounter_id.empty()||effect->party_id||effect->person_id||effect->news_ids)return common_failure("InvalidTaskExtensionCachedReceipt",row.first);const auto* fact=state->facts.find(effect->settlement_result);const auto* header=common.state().facts.find(effect->settlement_result);if(!fact||!header||fact->fact_id!=effect->settlement_result||header->id!=fact->fact_id||header->kind!=task_extension_fact_kind||header->domain!=task_extension_module||!sources.insert(fact->fact_id).second)return common_failure("TaskExtensionCachedSourceMismatch",row.first);auto valid=interval(common,*state,row.first,result,*fact);if(!valid.ok)return valid;
    }return common_success();
}
}
