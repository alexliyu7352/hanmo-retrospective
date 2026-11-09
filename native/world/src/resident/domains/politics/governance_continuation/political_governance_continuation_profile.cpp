// 新作者显式选择已受领安民续办；缺省不追加段、命令回执或改变旧暂停语义。
#include "political_governance_continuation_detail.hpp"
#include "resident/domains/politics/politics_governance_profile.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include "resident/storage/common_codec.hpp"
#include <map>
namespace hanmo::resident {
// 标题与实际读写共用严格模块配对；不需要构造临时Common或伪状态。
Status validate_political_governance_continuation_modules(const std::vector<std::pair<StableId,StableId>>& modules,bool enabled){
 std::map<StableId,Integer> count;for(const auto& m:modules){if(!enabled&&m.first!=political_governance_continuation_module)continue;if(m.first==political_governance_continuation_module||m.first=="politics_governance"||m.first=="governance_tasks"||m.first=="public_relief"||m.first=="political_takeover"||m.first=="economic_takeover_execution"){if(m.second!="1"||++count[m.first]!=1)return common_failure("GovernanceContinuationModuleMismatch",m.first);}else if(m.first=="task_takeover_execution"){if((m.second!="1"&&m.second!="2")||++count[m.first]!=1)return common_failure("GovernanceContinuationModuleMismatch",m.first);}}
 if(count[political_governance_continuation_module]!=(enabled?1:0))return common_failure("GovernanceContinuationProfileMismatch");
 if(enabled){for(const auto* name:{"politics_governance","governance_tasks","public_relief","political_takeover","economic_takeover_execution","task_takeover_execution"})if(count[name]!=1)return common_failure("GovernanceContinuationDependenciesRequired",name);}
 return common_success();
}
Status validate_political_governance_continuation_layout(const CommonStore& c,const std::vector<std::pair<StableId,StableId>>& modules,bool enabled,const PoliticalGovernanceContinuationState* state){
 auto valid=validate_political_governance_continuation_modules(modules,enabled);if(!valid.ok)return valid;if(bool(state)!=enabled||(state&&state->version!=1))return common_failure("GovernanceContinuationProfileMismatch");
 if(!enabled){for(const auto& row:c.state().facts.rows())if(row.second.domain==political_governance_continuation_module||row.second.kind==political_governance_continue_kind)return common_failure("GovernanceContinuationProfileRequired",row.first);for(const auto& h:c.state().history)if(h.kind==political_governance_continue_command)return common_failure("GovernanceContinuationProfileRequired",h.result_id);for(const auto& row:c.state().results.rows())if(row.second&&row.second->kind==political_governance_continue_command)return common_failure("GovernanceContinuationProfileRequired",row.first);}
 return common_success();
}
Outcome<bool> decode_political_governance_continuation_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities){
 const auto* dynamic=scenario.find("dynamic_world");const auto* group=dynamic?dynamic->find(political_governance_continuation_module):nullptr;if(!group)return {true,false,{}};auto valid=validate_political_takeover_payload(*group);if(!valid.ok)return {false,false,valid.error};try{using namespace boundary;const auto& fields=object(*group,"political_governance_continuations");if(fields.size()!=1||get_int(*group,"version","political_governance_continuations")!=1)return {false,false,common_failure("UnsupportedGovernanceContinuationsVersion").error};if(std::find(capabilities.begin(),capabilities.end(),"political_governance_continuations_v1")==capabilities.end())return {false,false,common_failure("GovernanceContinuationCapabilityRequired").error};if(!read_politics_governance_profile(*dynamic))return {false,false,common_failure("GovernanceContinuationDependenciesRequired").error};const auto* takeover=dynamic->find("political_takeover");if(!takeover||!get_bool(*takeover,"enabled","political_takeover")||get_int(*takeover,"version","political_takeover")!=1)return {false,false,common_failure("GovernanceContinuationDependenciesRequired").error};return {true,true,{}};}catch(const boundary::DecodeFailure& e){return {false,false,e.error};}
}
// 事实覆盖与JSON准入只豁免这张真实本域表，不能凭domain吞下别的载荷。
Status validate_political_governance_continuation_fact_header(const CommonStore& c,const PoliticalGovernanceContinuationState& state,const FactRecord& head){const auto* r=state.continuations.find(head.id);if(state.version!=1||!r||r->id!=head.id||head.kind!=political_governance_continue_kind||head.domain!=political_governance_continuation_module)return common_failure("GovernanceContinuationFactMismatch",head.id);auto n=governance_continue_detail::source_sequence(c,head.id);return n.ok?common_success():Status{false,{},n.error};}
}
