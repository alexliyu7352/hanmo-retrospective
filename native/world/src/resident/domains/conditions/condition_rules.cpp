// 共用条件保持旧短路次序，缺少领域读取能力不能伪装条件为false。
#include "condition_model.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
namespace hanmo::resident {
namespace { Outcome<bool> unsupported(const StableId& kind){return {false,false,{{"UnsupportedNativeCapability",{}},ErrorKind::unsupported_capability,"conditions","conditions",kind,{}}};} }
Outcome<bool> evaluate_condition(const CommonRead& c,const InteractionState& s,const ConditionAccess& a,const StableId& actor,const Condition& q){
    switch(q.kind){
    case Condition::Kind::flag:{auto v=flag_value(s,q.flag);return {true,v&&*v==q.equals,{}};}
    case Condition::Kind::party_member:if(!a.party_contains)return unsupported("party_member");return {true,a.party_contains(actor,q.actor_id)==q.equals,{}};
    case Condition::Kind::actor_at:{auto p=c.actor(q.actor_id);if(!p||!p->alive)return {true,false,{}};if(!a.resolve_location)return unsupported("actor_at");auto l=a.resolve_location(q.actor_id);auto loc=std::get_if<SettlementLocation>(&l);return {true,loc&&loc->place_id==q.place_id,{}};}
    case Condition::Kind::task_state:if(!a.task_status)return unsupported("task_state");return {true,a.task_status(actor,q.offer_id)==q.state,{}};
    case Condition::Kind::offer_declined_before:{if(!a.offer_declined_before)return unsupported("offer_declined_before");auto recalled=a.offer_declined_before(actor,q.offer_id);if(!recalled.ok)return recalled;return {true,recalled.value==q.equals,{}};}
    case Condition::Kind::treaty_personal_passage:if(!a.treaty_personal_passage)return unsupported("treaty_personal_passage");return a.treaty_personal_passage(actor,q.place_id,q.offer_id);
    case Condition::Kind::relation_min:{if(!a.relation_trust)return unsupported("relation_min");auto trust=a.relation_trust(q.from_actor,q.to_actor);return {true,trust?*trust>=q.trust:q.trust==0,{}};}
    case Condition::Kind::all:return all_conditions(c,s,a,actor,q.terms);
    case Condition::Kind::any:for(auto& term:q.terms){auto r=evaluate_condition(c,s,a,actor,term);if(!r.ok||r.value)return r;}return {true,false,{}};
    }return {true,false,{}};
}
Outcome<bool> all_conditions(const CommonRead& c,const InteractionState& s,const ConditionAccess& a,const StableId& actor,const std::vector<Condition>& terms){for(auto& term:terms){auto r=evaluate_condition(c,s,a,actor,term);if(!r.ok||!r.value)return r;}return {true,true,{}};}
}
