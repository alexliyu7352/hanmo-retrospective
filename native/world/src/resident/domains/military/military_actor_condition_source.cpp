// 只投供给实口与原伤亡变化的确定条件效应；原战果/供给资格仍由既有域历史总验负责。
#include "military_actor_condition_source.hpp"
#include "resident/model/dynamic_contract.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident {namespace {
template<class T>Outcome<T> fail(const StableId& id){return {false,{},common_failure("MilitaryActorConditionSourceMismatch",id).error};}
// 校验真实正整数分配号，不接受前导零、未分配号或仅外观相似的前缀。
Integer serial(const StableId& id,const StableId& prefix){if(id.rfind(prefix,0)!=0)return -1;auto digits=id.substr(prefix.size());Integer n=0;auto p=std::from_chars(digits.data(),digits.data()+digits.size(),n);return p.ec==std::errc{}&&p.ptr==digits.data()+digits.size()&&n>0&&std::to_string(n)==digits?n:-1;}
Outcome<Integer> limit(const CommonStore& c,const DynamicStepInput* p){if(!p)return {true,c.tick(),{}};if(p->from_tick<0||p->from_tick!=c.tick()||p->from_tick==std::numeric_limits<Integer>::max()||p->to_tick!=p->from_tick+1)return {false,{},common_failure("MilitaryActorConditionPhaseClockMismatch").error};return {true,p->to_tick,{}};}
// 只核身份已实际存在于原效应时；不以当前存活/可用/身体条件倒验当时参与。
bool actor_at(const CommonStore& c,const StableId& id,Integer tick){const auto* a=c.state().actors.find(id);return a&&a->id==id&&(!a->created_tick||(*a->created_tick>=0&&*a->created_tick<=tick));}
Outcome<MilitaryActorConditionSource> read(const CommonStore& c,const MilitaryState& m,const StableId& id,Integer upper){
 using R=MilitaryActorConditionSource;const MilitarySupplyFact* supply=nullptr;const MilitaryCasualtyFact* casualty=nullptr;
 for(const auto& f:m.supply_facts)if(f.id==id){if(supply)return fail<R>(id);supply=&f;}
 for(const auto& f:m.casualty_facts)if(f.id==id){if(casualty)return fail<R>(id);casualty=&f;}
 const auto* h=c.state().facts.find(id);const auto n=serial(id,"military:");if(bool(supply)==bool(casualty)||n<1||n>=c.state().next_instance_id||!h||h->id!=id||h->domain!="military"||h->kind!=(supply?"military_supply":"military_casualties")||std::count(c.state().facts.order().begin(),c.state().facts.order().end(),id)!=1)return fail<R>(id);
 R out;out.source_identity=c.state().identity;out.actual_fact_id=id;out.tick=supply?supply->tick:casualty->tick;out.kind=supply?MilitaryActorConditionKind::supply:MilitaryActorConditionKind::casualty;if(out.tick<0||out.tick>upper)return fail<R>(id);
 if(supply){
  if(supply->army_id.empty()==supply->garrison_id.empty()||supply->required<0||supply->consumed<0||supply->consumed>supply->required||supply->shortage!=supply->required-supply->consumed||!std::is_sorted(supply->mouth_ids.begin(),supply->mouth_ids.end())||std::adjacent_find(supply->mouth_ids.begin(),supply->mouth_ids.end())!=supply->mouth_ids.end())return fail<R>(id);
  for(std::size_t i=0;i<supply->mouth_ids.size();++i){const auto& actor=supply->mouth_ids[i];if(actor.empty())return fail<R>(id);if(c.actor(actor)){if(!actor_at(c,actor,out.tick))return fail<R>(id);if(supply->shortage>0)out.actors.push_back({actor,MilitaryActorConditionOperation::fatigue_add_four,Integer(i)});}}
 }else{
  const auto* cause=c.state().facts.find(casualty->battle_id);if(!cause||cause->id!=casualty->battle_id)return fail<R>(id);
  StableId prefix;if(cause->domain=="military"&&cause->kind=="military_battle"){const MilitaryBattleFact* battle=nullptr;for(const auto& row:m.battle_facts)if(row.id==cause->id){if(battle)return fail<R>(id);battle=&row;}if(!battle||battle->tick!=out.tick||battle->phase!="completed")return fail<R>(id);prefix=battle->final?"battle:":"battle_round:";}else if(cause->domain=="military_naval"&&cause->kind=="naval_round")prefix="naval_round:";else if(cause->domain=="military_naval"&&cause->kind=="naval_capture")prefix="naval_capture:";else return fail<R>(id);
  const auto before=serial(cause->id,prefix);const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),id),prior=std::find(order.begin(),order.end(),cause->id);if(before<1||before>=n||prior==order.end()||prior>=at||std::count(order.begin(),order.end(),cause->id)!=1)return fail<R>(id);
  std::set<StableId> actors;for(std::size_t i=0;i<casualty->actor_changes.size();++i){const auto& change=casualty->actor_changes[i];if(!actor_at(c,change.actor_id,out.tick)||!actors.insert(change.actor_id).second)return fail<R>(id);if(change.status=="dead")out.actors.push_back({change.actor_id,MilitaryActorConditionOperation::hp_dead_zero,Integer(i)});else if(change.status=="wounded")out.actors.push_back({change.actor_id,MilitaryActorConditionOperation::hp_wounded_cap_seventy,Integer(i)});else if(change.status!="captured"&&change.status!="deserted"&&change.status!="fit")return fail<R>(id);}
 }
 return {true,std::move(out),{}};
}
// payload与实际头双向核对，拒漏头/漏业务行；不将自己的条件账作为业务覆盖清单。
Outcome<std::vector<MilitaryActorConditionSource>> all(const CommonStore& c,const MilitaryState& m,Integer upper){
 using R=std::vector<MilitaryActorConditionSource>;std::set<StableId> ids;for(const auto& f:m.supply_facts)if(!ids.insert(f.id).second)return fail<R>(f.id);for(const auto& f:m.casualty_facts)if(!ids.insert(f.id).second)return fail<R>(f.id);R out;
 for(const auto& id:c.state().facts.order()){const auto* h=c.state().facts.find(id);if(!h)return fail<R>(id);const bool relevant=h->domain=="military"&&(h->kind=="military_supply"||h->kind=="military_casualties");if(ids.count(id)!=relevant)return fail<R>(id);if(!relevant)continue;auto source=read(c,m,id,upper);if(!source.ok)return {false,{},source.error};ids.erase(id);if(!source.value.actors.empty())out.push_back(std::move(source.value));}
 if(!ids.empty())return fail<R>(*ids.begin());return {true,std::move(out),{}};
}
}
Outcome<MilitaryActorConditionSource> read_military_actor_condition_source(const CommonStore& c,const MilitaryState& m,const StableId& id){return read(c,m,id,c.tick());}
Outcome<MilitaryActorConditionSource> read_military_actor_condition_source(const CommonStore& c,const MilitaryState& m,const StableId& id,const DynamicStepInput& p){auto u=limit(c,&p);if(!u.ok)return {false,{},u.error};return read(c,m,id,u.value);}
Outcome<std::vector<MilitaryActorConditionSource>> read_military_actor_condition_sources(const CommonStore& c,const MilitaryState& m){return all(c,m,c.tick());}
Outcome<std::vector<MilitaryActorConditionSource>> read_military_actor_condition_sources(const CommonStore& c,const MilitaryState& m,const DynamicStepInput& p){auto u=limit(c,&p);if(!u.ok)return {false,{},u.error};return all(c,m,u.value);}
Outcome<MilitaryActorConditionValues> apply_military_actor_condition_rule(MilitaryActorConditionOperation op,Integer hp,Integer fatigue){using R=MilitaryActorConditionValues;if(hp<0||hp>100||fatigue<0||fatigue>100)return fail<R>("condition_before");R out;switch(op){case MilitaryActorConditionOperation::fatigue_add_four:out.fatigue=std::min<Integer>(100,fatigue+4);break;case MilitaryActorConditionOperation::hp_dead_zero:out.hp=0;break;case MilitaryActorConditionOperation::hp_wounded_cap_seventy:out.hp=std::min<Integer>(hp,70);break;default:return fail<R>("operation");}return {true,std::move(out),{}};}
}
