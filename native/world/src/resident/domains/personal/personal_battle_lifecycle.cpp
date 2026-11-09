#include "personal_battle_lifecycle.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <string_view>
namespace hanmo::resident {using namespace movement_detail;namespace {
using Ids=std::set<std::string_view,std::less<>,CheckpointAllocator<std::string_view>>;
using Events=std::map<std::string_view,const PersonalBattle*,std::less<>,CheckpointAllocator<std::pair<const std::string_view,const PersonalBattle*>>>;
// 比较生命周期原始时刻；精确运算失败沿原异常路径传递。
bool same_lifecycle_moment(const WorldMoment &a, const WorldMoment &b,
                           ExactArithmeticBudget budget = {}) {
  return take(compare_world_moments(a, b, budget)) == 0;
}
void selected(Integer value){if(value!=0&&value!=1)fail("PersonalBattleLifecycleSelectedSchemaRequired");}
void moment(const WorldMoment& value,const WorldMoment& now,ExactArithmeticBudget budget){take(encode_world_moment(value,budget));if(take(compare_world_moments(value,now,budget))>0)fail("PersonalBattleLifecycleFutureSource");}
void identity(const CommonStore& common,const PersonalBattle& b,const StableId& key){
 if(key.empty()||b.id!=key||b.controller_id.empty()||b.units.empty()||!b.units.find(b.controller_id))fail("PersonalBattleLifecycleOwnerRequired",key);
 for(const auto& unit:b.units.rows())if(unit.first.empty()||unit.first!=unit.second.actor_id||!common.actor(unit.first))fail("PersonalBattleLifecycleParticipantRequired",key);
}
void result_identity(const CommonStore& common,const PersonalState& state,const PersonalBattle& b,bool require_header){
 const auto* fact=state.facts.find(b.id);
 if(!b.applied||b.result_id.empty()||!fact||fact->battle_id!=b.id||fact->result_id!=b.result_id||fact->outcome!=b.outcome||fact->participants.size()!=b.units.size())fail("PersonalBattleLifecycleResultRequired",b.id);
 Ids people;for(const auto& actor:fact->participants)if(!b.units.find(actor)||!people.insert(actor).second)fail("PersonalBattleLifecycleResultParticipantsMismatch",b.id);
 if(require_header)take(read_actual_common_fact_position(common,SourceRefBuildView{"personal","personal_battle",b.id}));
}
std::size_t event_position(const CommonStore& common,const PersonalBattleLifecycleEvent1& e,std::string_view kind,Ids& heads,const WorldMoment& now,ExactArithmeticBudget budget){
 if(e.source_id.empty()||!heads.insert(e.source_id).second)fail("PersonalBattleLifecycleSourceRepeated",e.source_id);moment(e.occurred_at,now,budget);
 return take(read_actual_common_fact_position(common,SourceRefBuildView{"personal",kind,e.source_id}));
}
WorldMoment current(const CommonStore& common,const PersonalState& state,const PersonalAccess& access){
 take(validate_personal_battle_lifecycle_write_binding(common,state,access));if(access.selected_battle_lifecycle!=1)fail("PersonalBattleLifecycleCapabilityRequired");
 auto at=take(access.battle_lifecycle->clock.read_current_moment());take(encode_world_moment(at));
 const auto lower=take(normalize_world_moment(common.tick(),{}));if(take(compare_world_moments(at,lower))<0)fail("PersonalBattleLifecycleStaleClock");return at;
}
// 核对前后事件的源身份与原时刻；任何改变均返回既有生命周期失败。
void unchanged_event(const PersonalBattleLifecycleEvent1 &a,
                     const PersonalBattleLifecycleEvent1 &b) {
  if (a.source_id != b.source_id ||
      !same_lifecycle_moment(a.occurred_at, b.occurred_at))
    fail("PersonalBattleLifecycleResolverChangedSource");
}
}
Status validate_personal_battle_lifecycle_layout(const PersonalState& state,Integer version){return status([&]{
 selected(version);
 for(const auto& item:state.battles.rows()){const auto& b=item.second;
  if(!version){if(b.lifecycle)fail("UnexpectedPersonalBattleLifecycle",item.first);continue;}
  if(item.first!=b.id||!b.lifecycle||b.lifecycle->started.source_id.empty()||(b.phase!="planning"&&b.phase!="result"&&b.phase!="closed"))fail("PersonalBattleLifecycleRequired",item.first);
  const auto& life=*b.lifecycle;
  if(bool(life.result_applied_at)!=(b.phase!="planning")||bool(life.closed)!=(b.phase=="closed")||(life.closed&&life.closed->source_id.empty()))fail("PersonalBattleLifecyclePhaseMismatch",b.id);
  if(b.phase=="planning"){if(b.applied||!b.result_id.empty())fail("PersonalBattleLifecyclePrematureResult",b.id);}
  else if(!b.applied||b.result_id.empty())fail("PersonalBattleLifecycleResultRequired",b.id);
 }
});}
Status validate_personal_battle_lifecycle_sources(const CommonStore& common,const PersonalState& state,Integer version,const WorldMoment& now,ExactArithmeticBudget budget){return status([&]{
 take(validate_personal_battle_lifecycle_layout(state,version));if(version)take(encode_world_moment(now,budget));Ids starts,results,closes;Events start_rows,close_rows;
 if(version)for(const auto& item:state.battles.rows()){const auto& b=item.second;identity(common,b,item.first);const auto& life=*b.lifecycle;
  const auto begin=event_position(common,life.started,"personal_battle_started",starts,now,budget);start_rows.emplace(life.started.source_id,&b);
  if(life.result_applied_at){result_identity(common,state,b,true);if(!results.insert(b.id).second)fail("PersonalBattleLifecycleSourceRepeated",b.id);
   const auto result=take(read_actual_common_fact_position(common,SourceRefBuildView{"personal","personal_battle",b.id}));moment(*life.result_applied_at,now,budget);
   if(begin>=result||take(compare_world_moments(life.started.occurred_at,*life.result_applied_at,budget))>0)fail("PersonalBattleLifecycleSourceOrder",b.id);
   if(life.closed){const auto close=event_position(common,*life.closed,"personal_battle_closed",closes,now,budget);close_rows.emplace(life.closed->source_id,&b);if(result>=close||take(compare_world_moments(*life.result_applied_at,life.closed->occurred_at,budget))>0)fail("PersonalBattleLifecycleSourceOrder",b.id);}
  }
 }
 Ids occupied;
 for(const auto& item:common.state().facts.rows()){const auto& h=item.second;if(h.domain!="personal")continue;
  if(h.kind=="personal_battle_started"||h.kind=="personal_battle_closed"){
   if(!version||item.first!=h.id||(h.kind=="personal_battle_started"?!starts.count(h.id):!closes.count(h.id)))fail("PersonalBattleLifecycleOrphanSource",item.first);
   const bool starting=h.kind=="personal_battle_started";const auto& rows=starting?start_rows:close_rows;const auto row=rows.find(h.id);
   if(row==rows.end())fail("PersonalBattleLifecycleOrphanSource",item.first);
   for(const auto& unit:row->second->units.rows()){
    if(starting){if(!occupied.insert(unit.first).second)fail("PersonalBattleLifecycleDoubleOccupation",unit.first);}
    else if(occupied.erase(unit.first)!=1)fail("PersonalBattleLifecycleCloseWithoutStart",unit.first);
   }
  }else if(version&&h.kind=="personal_battle"&&(item.first!=h.id||!results.count(h.id)))fail("PersonalBattleLifecycleOrphanResult",item.first);
 }
});}
Outcome<PersonalBattleOccupancyRead1> read_personal_battle_occupied_before(const CommonStore& common,const PersonalState& state,Integer version,const StableId& actor,const CommonActualFactPrefix& prefix,const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget budget){return guard<PersonalBattleOccupancyRead1>([&]{
 if(version!=1)fail("PersonalBattleLifecycleCapabilityRequired");if(!common.actor(actor))fail("PersonalBattleLifecycleActorRequired",actor);
 take(validate_personal_battle_lifecycle_sources(common,state,version,now,budget));moment(at,now,budget);const auto cut=take(read_actual_common_fact_prefix(common,prefix));PersonalBattleOccupancyRead1 out;
 const auto included=[&](std::size_t position,const WorldMoment& occurred){const bool before=position<cut;const auto relative=take(compare_world_moments(occurred,at,budget));if((before&&relative>0)||(!before&&relative<0))fail("PersonalBattleLifecycleHistoryBoundary",actor);return before;};
 for(const auto& item:state.battles.rows()){const auto& b=item.second;const auto& life=*b.lifecycle;
  const bool started=included(take(read_actual_common_fact_position(common,SourceRefBuildView{"personal","personal_battle_started",life.started.source_id})),life.started.occurred_at);
  if(life.result_applied_at)included(take(read_actual_common_fact_position(common,SourceRefBuildView{"personal","personal_battle",b.id})),*life.result_applied_at);
  const bool closed=life.closed&&included(take(read_actual_common_fact_position(common,SourceRefBuildView{"personal","personal_battle_closed",life.closed->source_id})),life.closed->occurred_at);
  if(b.units.find(actor)&&started&&!closed){if(out.occupied)fail("PersonalBattleLifecycleDoubleOccupation",actor);out={true,&b,&life.started};}
 }
 return out;
});}
Status validate_personal_battle_lifecycle_write_binding(const CommonStore& common,const PersonalState& state,const PersonalAccess& access){return status([&]{
 selected(access.selected_battle_lifecycle);
 if(!access.selected_battle_lifecycle){if(access.battle_lifecycle)fail("UnexpectedPersonalBattleLifecycleContext");for(const auto& item:state.battles.rows())if(item.second.lifecycle)fail("PersonalBattleLifecycleCapabilityRequired",item.first);return;}
 if(!access.battle_lifecycle||&access.battle_lifecycle->common!=&common||&access.battle_lifecycle->state!=&state)fail("CrossCandidatePersonalBattleLifecycleContext");
});}
Outcome<std::optional<PersonalBattleLifecycle1>> prepare_personal_battle_lifecycle_start_in_candidate(CommonStore& common,PersonalState& state,const PersonalAccess& access){return guard<std::optional<PersonalBattleLifecycle1>>([&]()->std::optional<PersonalBattleLifecycle1>{
 take(validate_personal_battle_lifecycle_write_binding(common,state,access));if(!access.selected_battle_lifecycle)return std::nullopt;
 auto at=current(common,state,access);auto id=take(common.next_id("personal_battle_started"));return PersonalBattleLifecycle1{{std::move(id),std::move(at)},std::nullopt,std::nullopt};
});}
// 只发布当前候选的有效开战事实；绑定、阶段或时钟不符时拒绝。
Status publish_personal_battle_lifecycle_start_in_candidate(
    CommonStore &common, PersonalState &state, const PersonalAccess &access,
    const StableId &id) {
  return status([&] {
    take(validate_personal_battle_lifecycle_write_binding(common, state,
                                                          access));
    if (!access.selected_battle_lifecycle)
      return;
    const auto *b = state.battles.find(id);
    if (!b || b->id != id || b->phase != "planning" || !b->lifecycle ||
        b->lifecycle->result_applied_at || b->lifecycle->closed)
      fail("PersonalBattleLifecycleStartRequired", id);
    identity(common, *b, id);
    if (!same_lifecycle_moment(current(common, state, access),
                               b->lifecycle->started.occurred_at))
      fail("PersonalBattleLifecycleStartClockChanged", id);
    take(common.append_fact({b->lifecycle->started.source_id,
                             "personal_battle_started", "personal"}));
  });
}
// 检查解析前后的生命周期身份与事件不变；不接受替换或新增来源。
Status validate_personal_battle_resolved_lifecycle(const PersonalBattle &before,
                                                   const PersonalBattle &after,
                                                   Integer version) {
  return status([&] {
    selected(version);
    if (!version) {
      if (before.lifecycle || after.lifecycle)
        fail("UnexpectedPersonalBattleLifecycle", before.id);
      return;
    }
    if (before.id != after.id || before.controller_id != after.controller_id ||
        before.units.size() != after.units.size() || !before.lifecycle ||
        !after.lifecycle)
      fail("PersonalBattleLifecycleResolverChangedIdentity", before.id);
    for (const auto &item : before.units.rows()) {
      const auto *unit = after.units.find(item.first);
      if (!unit || item.second.actor_id != item.first ||
          unit->actor_id != item.first)
        fail("PersonalBattleLifecycleResolverChangedParticipants", before.id);
    }
    unchanged_event(before.lifecycle->started, after.lifecycle->started);
    if (bool(before.lifecycle->result_applied_at) !=
            bool(after.lifecycle->result_applied_at) ||
        bool(before.lifecycle->closed) != bool(after.lifecycle->closed))
      fail("PersonalBattleLifecycleResolverChangedSource", before.id);
    if (before.lifecycle->result_applied_at &&
        !same_lifecycle_moment(*before.lifecycle->result_applied_at,
                               *after.lifecycle->result_applied_at))
      fail("PersonalBattleLifecycleResolverChangedSource", before.id);
    if (before.lifecycle->closed)
      unchanged_event(*before.lifecycle->closed, *after.lifecycle->closed);
  });
}
Status record_personal_battle_result_moment_in_candidate(CommonStore& common,PersonalState& state,const PersonalAccess& access,PersonalBattle& b){return status([&]{
 take(validate_personal_battle_lifecycle_write_binding(common,state,access));if(!access.selected_battle_lifecycle)return;
 if(state.battles.find(b.id)!=&b||b.phase!="result"||!b.lifecycle||b.lifecycle->result_applied_at||b.lifecycle->closed)fail("PersonalBattleLifecycleResultRequired",b.id);
 result_identity(common,state,b,false);take(read_actual_common_fact_position(common,SourceRefBuildView{"personal","personal_battle_started",b.lifecycle->started.source_id}));
 auto at=current(common,state,access);if(take(compare_world_moments(at,b.lifecycle->started.occurred_at))<0)fail("PersonalBattleLifecycleSourceOrder",b.id);b.lifecycle->result_applied_at.emplace(std::move(at));
});}
Status close_personal_battle_lifecycle_in_candidate(CommonStore& common,PersonalState& state,const PersonalAccess& access,PersonalBattle& b){return status([&]{
 take(validate_personal_battle_lifecycle_write_binding(common,state,access));if(!access.selected_battle_lifecycle){b.phase="closed";return;}
 if(state.battles.find(b.id)!=&b||b.phase!="result"||!b.lifecycle||!b.lifecycle->result_applied_at||b.lifecycle->closed)fail("PersonalBattleLifecycleCloseRequired",b.id);
 result_identity(common,state,b,true);auto at=current(common,state,access);if(take(compare_world_moments(at,*b.lifecycle->result_applied_at))<0)fail("PersonalBattleLifecycleSourceOrder",b.id);
 auto id=take(common.next_id("personal_battle_closed"));b.lifecycle->closed.emplace(PersonalBattleLifecycleEvent1{std::move(id),std::move(at)});b.phase="closed";
 take(common.append_fact({b.lifecycle->closed->source_id,"personal_battle_closed","personal"}));
});}
}
