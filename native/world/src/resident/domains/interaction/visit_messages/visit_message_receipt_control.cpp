// 托领撤帖时控制身份从原真实接续账折叠；同tick不能把接续头移过拜帖边界冒NPC来源。
#include "visit_message_receipt_control.hpp"
#include "visit_message_state.hpp"
#include <charconv>
#include "resident/domains/family/historical_player.hpp"
#include "resident/domains/family/family_legacy.hpp"
#include "resident/domains/family/voluntary_succession.hpp"
namespace hanmo::resident {namespace {
// 完整原域负责事实授权；这里只检查两个真实已分配实例与Common顺序的同一截面。
std::optional<Integer> instance(const CommonStore& c,const StableId& id){const auto colon=id.find(':');if(colon==StableId::npos)return {};const auto digits=id.substr(colon+1);Integer n=0;auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),n);if(digits.empty()||parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||n<1||n>=c.state().next_instance_id||std::to_string(n)!=digits)return {};return n;}
}
Outcome<StableId> read_visit_message_player_before_fact(const CommonStore& c,const FamilyState& family,const FamilyLegacyState* legacy,Integer tick,const StableId& fact){
    const auto* head=c.state().facts.find(fact);const auto boundary=instance(c,fact);const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),fact);
    if(!head||head->id!=fact||head->domain!="visit_messages"||(head->kind!=visit_message_posted_kind&&head->kind!=visit_message_received_kind&&head->kind!=visit_message_withdrawn_kind)||fact.rfind(head->kind+":",0)!=0||!boundary||at==order.end()||tick<0||tick>c.tick())return {false,{},common_failure("VisitMessageControlBoundaryRequired",fact).error};
    auto same_side=[&](const StableId& id,Integer changed_tick)->Status{const auto n=instance(c,id);const auto where=std::find(order.begin(),order.end(),id);if(!n||where==order.end()||*n==*boundary||((*n<*boundary)!=(where<at))||(where<at?changed_tick>tick:changed_tick<tick))return common_failure("VisitMessageControlOrderMismatch",id);return common_success();};
    for(const auto& id:family.successions){auto step=family_succession_step(family,id);if(!step.ok)return {false,{},step.error};auto valid=same_side(id,step.value.tick);if(!valid.ok)return {false,{},valid.error};}
    if(legacy)for(const auto& row:legacy->activations.rows()){auto valid=same_side(row.first,row.second.tick);if(!valid.ok)return {false,{},valid.error};}
    return legacy?family_player_before_fact_with_legacy(c,family,*legacy,tick,fact):family_player_before_fact(c,family,tick,fact);
}

Outcome<bool> visit_message_npc_receipt_available(const CommonStore& c,const FamilyState& family,const FamilyLegacyState* legacy){for(const auto& id:family.successions){auto step=family_succession_step(family,id);if(!step.ok)return {false,false,step.error};if(step.value.tick==c.tick())return {true,false,{}};}if(legacy)for(const auto& row:legacy->activations.rows())if(row.second.tick==c.tick())return {true,false,{}};return {true,true,{}};}
Outcome<StableId> read_visit_message_advance_submitter_before_fact(const CommonStore& c,const FamilyState& family,const FamilyLegacyState* legacy,Integer tick,const StableId& fact){auto player=read_visit_message_player_before_fact(c,family,legacy,tick,fact);if(!player.ok)return player;const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),fact);auto unavailable=[&](const StableId& id,Integer changed){return changed==tick&&std::find(order.begin(),order.end(),id)<at;};for(const auto& id:family.successions){auto step=family_succession_step(family,id);if(!step.ok)return {false,{},step.error};if(unavailable(id,step.value.tick))return {false,{},common_failure("VisitMessageAdvanceControlSourceUnavailable",fact).error};}if(legacy)for(const auto& row:legacy->activations.rows())if(unavailable(row.first,row.second.tick))return {false,{},common_failure("VisitMessageAdvanceControlSourceUnavailable",fact).error};return player;}
}
