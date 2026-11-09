// 回复时控制身份从原真实接续账折叠；同tick不能把接续头移过回复边界冒NPC来源。
#include "visit_reply_control.hpp"
#include "visit_reply_detail.hpp"
#include "resident/domains/family/historical_player.hpp"
#include "resident/domains/family/family_legacy.hpp"
#include "resident/domains/family/voluntary_succession.hpp"
namespace hanmo::resident {namespace {
// 完整原域负责事实授权；这里只检查两个真实已分配实例与Common顺序的同一截面。
std::optional<Integer> instance(const CommonStore& c,const StableId& id){const auto colon=id.find(':');if(colon==StableId::npos)return {};return visit_reply_detail::serial(id,id.substr(0,colon),c.state().next_instance_id);}
}
Outcome<StableId> read_visit_reply_player_before_fact(const CommonStore& c,const FamilyState& family,const FamilyLegacyState* legacy,Integer tick,const StableId& fact){
    const auto* head=c.state().facts.find(fact);const auto boundary=instance(c,fact);const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),fact);
    if(!head||head->id!=fact||head->domain!=visit_reply_module||(head->kind!=visit_reply_posted_kind&&head->kind!=visit_reply_collected_kind)||!boundary||at==order.end()||tick<0||tick>c.tick())return {false,{},common_failure("VisitReplyControlBoundaryRequired",fact).error};
    auto same_side=[&](const StableId& id,Integer changed_tick)->Status{const auto n=instance(c,id);const auto where=std::find(order.begin(),order.end(),id);if(!n||where==order.end()||*n==*boundary||((*n<*boundary)!=(where<at))||(where<at?changed_tick>tick:changed_tick<tick))return common_failure("VisitReplyControlOrderMismatch",id);return common_success();};
    for(const auto& id:family.successions){auto step=family_succession_step(family,id);if(!step.ok)return {false,{},step.error};auto valid=same_side(id,step.value.tick);if(!valid.ok)return {false,{},valid.error};}
    if(legacy)for(const auto& row:legacy->activations.rows()){auto valid=same_side(row.first,row.second.tick);if(!valid.ok)return {false,{},valid.error};}
    return legacy?family_player_before_fact_with_legacy(c,family,*legacy,tick,fact):family_player_before_fact(c,family,tick,fact);
}
}
