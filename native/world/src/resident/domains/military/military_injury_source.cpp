// 每次伤因直接对应原军战伤亡或个人战终局；永久事实先序由Common持有。
#include "military_injury_source.hpp"
#include "resident/model/dynamic_contract.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
using Causes=std::vector<MilitaryActorInjuryCause>;
Outcome<Causes> fail(const StableId& id){return {false,{},common_failure("MilitaryActorInjurySourceMismatch",id).error};}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
// 完成实例只核真实分配，不能伪造一个并不存在的Common result事实头。
Integer serial(const StableId& id,const StableId& prefix){if(id.rfind(prefix,0)!=0)return -1;const auto digits=id.substr(prefix.size());Integer n=0;const auto p=std::from_chars(digits.data(),digits.data()+digits.size(),n);return p.ec==std::errc{}&&p.ptr==digits.data()+digits.size()&&n>0&&std::to_string(n)==digits?n:-1;}
// 不重入Family/Growth或整World总验；只检查已经验证的军事记录的直接因果边界。
Outcome<Causes> read(const CommonStore& c,const MilitaryState& m,const PersonalState& p,const PersonalCatalog& catalog,const StableId& actor,Integer limit){
 const auto* person=c.state().actors.find(actor);if(actor.empty()||!person)return fail(actor);
 std::map<StableId,MilitaryActorInjuryCause> sources;
 std::set<StableId> casualty_ids;
 for(const auto& fact:m.casualty_facts){if(!casualty_ids.insert(fact.id).second)return fail(fact.id);const auto* head=c.state().facts.find(fact.id);if(!head||head->id!=fact.id||head->kind!="military_casualties"||head->domain!="military"||fact.tick<0||fact.tick>limit)return fail(fact.id);std::set<StableId> changed;
  for(const auto& change:fact.actor_changes){if(!changed.insert(change.actor_id).second)return fail(fact.id);if(change.actor_id!=actor||change.status!="wounded")continue;if(fact.battle_id.empty()||(person->created_tick&&*person->created_tick>fact.tick))return fail(fact.id);sources.emplace(fact.id,MilitaryActorInjuryCause{c.state().identity,actor,fact.id,"military_wound",fact.tick,1});}
 }
 std::set<StableId> personal_completions;
 for(const auto& row:p.facts.rows()){
  const auto& fact=row.second;const auto* head=c.state().facts.find(row.first);const auto* battle=p.battles.find(row.first);if(!head||head->id!=row.first||head->kind!="personal_battle"||head->domain!="personal"||fact.battle_id!=row.first||!battle||battle->id!=row.first||!battle->applied||(battle->phase!="result"&&battle->phase!="closed")||battle->completed_tick!=fact.tick||battle->result_id!=fact.result_id||fact.result_id.empty()||fact.tick<0||fact.tick>c.tick())return fail(row.first);
  const auto begun=serial(row.first,"battle:"),completed=serial(fact.result_id,"result:");if(begun<1||completed<=begun||completed>=c.state().next_instance_id||!personal_completions.insert(fact.result_id).second)return fail(row.first);
  if(!has(fact.defeated,actor))continue;const auto* unit=battle->units.find(actor);const auto* objective=catalog.objectives.find(battle->objective_id);if(!catalog.enabled||!unit||unit->hp!=0||!has(fact.participants,actor)||std::count(fact.defeated.begin(),fact.defeated.end(),actor)!=1||!objective||objective->injury_on_defeat<0||objective->injury_on_defeat>3||(person->created_tick&&*person->created_tick>fact.tick))return fail(row.first);
  if(objective->injury_on_defeat>0&&!sources.emplace(row.first,MilitaryActorInjuryCause{c.state().identity,actor,row.first,"personal_defeat",fact.tick,objective->injury_on_defeat,fact.result_id}).second)return fail(row.first);
 }
 Causes out;for(const auto& id:c.state().facts.order()){
  const auto* head=c.state().facts.find(id);if(!head)return fail(id);
  if(head->domain=="military"&&head->kind=="military_casualties"&&!casualty_ids.count(id))return fail(id);
  if(head->domain=="personal"&&head->kind=="personal_battle"&&!p.facts.find(id))return fail(id);
  auto found=sources.find(id);if(found!=sources.end()){out.push_back(found->second);sources.erase(found);}
 }
 if(!sources.empty())return fail(sources.begin()->first);return {true,std::move(out),{}};
}
}
Outcome<std::vector<MilitaryActorInjuryCause>> query_military_actor_injury_causes(const CommonStore& c,const MilitaryState& m,const PersonalState& p,const PersonalCatalog& cat,const StableId& actor){return read(c,m,p,cat,actor,c.tick());}
Outcome<std::vector<MilitaryActorInjuryCause>> query_military_actor_injury_causes(const CommonStore& c,const MilitaryState& m,const PersonalState& p,const PersonalCatalog& cat,const StableId& actor,const DynamicStepInput& step){if(step.from_tick<0||step.from_tick!=c.tick()||step.from_tick==std::numeric_limits<Integer>::max()||step.to_tick!=step.from_tick+1)return {false,{},common_failure("MilitaryActorInjuryPhaseClockMismatch",actor).error};return read(c,m,p,cat,actor,step.to_tick);}
}
