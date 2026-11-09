// 同tick也按实际Common头前后判占用；今日close、withdrawn或军团成员不能回填旧疗程。
#include "military_siege_occupation.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <map>
namespace hanmo::resident {
namespace {
using Result=Outcome<std::optional<MilitarySiegeOccupationProjection>>;
Result fail(const StableId& code,const StableId& id){return {false,{},common_failure(code,id).error};}
bool same(const MilitarySiegeParticipationSnapshot& a,const MilitarySiegeParticipationSnapshot& b){return a.phase==b.phase&&a.started_tick==b.started_tick&&a.last_tick==b.last_tick&&a.round==b.round&&a.armies==b.armies&&a.unit_nodes==b.unit_nodes&&a.withdrawn_army_ids==b.withdrawn_army_ids;}
Result read(const CommonState& c,const MilitarySiegeLifecycleState& history,const SiegeRecord& siege,Integer tick,const StableId& boundary,Integer upper){
 const auto& order=c.facts.order();const auto edge=boundary.empty()?order.end():std::find(order.begin(),order.end(),boundary);if(tick<0||tick>upper||(!boundary.empty()&&(!c.facts.find(boundary)||edge==order.end()))||(boundary.empty()&&tick!=upper))return fail("MilitarySiegeOccupationBoundaryRequired",boundary);
 const MilitarySiegeLifecycleFact* last=nullptr;const MilitarySiegeLifecycleFact* selected=nullptr;const MilitarySiegeLifecycleFact* withdrawal_result=nullptr;std::optional<std::size_t> last_position;std::map<StableId,StableId> origins;
 for(const auto& f:history.facts)if(f.siege_id==siege.id){const auto* head=c.facts.find(f.id);const auto at=std::find(order.begin(),order.end(),f.id);Integer number=0;const StableId prefix="military_siege_lifecycle:";const auto tail=f.id.rfind(prefix,0)==0?f.id.substr(prefix.size()):StableId{};const auto parsed=std::from_chars(tail.data(),tail.data()+tail.size(),number);
  if(!head||head->id!=f.id||head->kind!=military_siege_lifecycle_kind_name(f.kind)||head->domain!="military"||at==order.end()||parsed.ec!=std::errc{}||parsed.ptr!=tail.data()+tail.size()||number<=0||number>=c.next_instance_id||std::to_string(number)!=tail||f.tick<0||f.tick>upper||f.topology_id!=siege.topology_id||(last_position&&std::size_t(at-order.begin())<=*last_position))return fail("MilitarySiegeOccupationSourceMismatch",f.id);
  if(last){if(f.previous_fact_id!=last->id||!f.before||!same(*f.before,last->after)||f.tick<last->tick)return fail("MilitarySiegeOccupationHistoryMismatch",f.id);}else if(f.kind!=MilitarySiegeLifecycleKind::created||f.before||!f.previous_fact_id.empty()||f.tick!=siege.started_tick)return fail("MilitarySiegeOccupationCreationRequired",f.id);
  if((f.tick<tick&&at>=edge)||(f.tick>tick&&at<edge))return fail("MilitarySiegeOccupationBoundaryMismatch",f.id);
  const bool before=f.tick<=tick&&at<edge;if(before){
   if(f.kind==MilitarySiegeLifecycleKind::created||f.kind==MilitarySiegeLifecycleKind::joined)for(const auto& army:f.after.armies)if(!f.before||std::none_of(f.before->armies.begin(),f.before->armies.end(),[&](const auto& old){return old.first==army.first;})){if(!origins.emplace(army.first,f.id).second)return fail("MilitarySiegeOccupationJoinRepeated",f.id);}
   selected=&f;if(f.kind==MilitarySiegeLifecycleKind::strategic&&f.strategic_command&&f.strategic_command->kind=="withdraw"&&f.after.phase=="result")withdrawal_result=&f;
  }
  last=&f;last_position=std::size_t(at-order.begin());
 }
 if(!last)return {true,std::nullopt,{}};if(!same(last->after,capture_military_siege_participation(siege)))return fail("MilitarySiegeOccupationCurrentMismatch",siege.id);
 MilitarySiegeOccupationProjection out{siege.id,boundary,tick,{}};if(!selected||selected->after.phase=="closed")return {true,std::move(out),{}};
 for(const auto& army:selected->after.armies){const bool withdrawn=std::find(selected->after.withdrawn_army_ids.begin(),selected->after.withdrawn_army_ids.end(),army.first)!=selected->after.withdrawn_army_ids.end();const bool awaiting_ack=withdrawal_result&&selected->after.phase=="result"&&withdrawal_result->strategic_command->army_id==army.first;
  if(withdrawn&&!awaiting_ack)continue;const auto origin=origins.find(army.first);if(origin==origins.end())return fail("MilitarySiegeOccupationJoinRequired",army.first);out.army_source_ids.emplace_back(army.first,origin->second);
 }
 return {true,std::move(out),{}};
}
}
Result read_military_siege_occupation(const CommonState& c,const MilitarySiegeLifecycleState& h,const SiegeRecord& s,Integer tick,const StableId& before){return read(c,h,s,tick,before,c.identity.tick);}
Result read_military_siege_occupation(const CommonState& c,const MilitarySiegeLifecycleState& h,const SiegeRecord& s,Integer tick,const StableId& before,const DynamicStepInput& phase){if(phase.from_tick<0||phase.from_tick!=c.identity.tick||phase.from_tick==std::numeric_limits<Integer>::max()||phase.to_tick!=phase.from_tick+1)return fail("MilitarySiegeOccupationPhaseMismatch",before);return read(c,h,s,tick,before,phase.to_tick);}
}
