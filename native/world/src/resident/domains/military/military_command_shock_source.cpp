// Common原入口、原编軍完成与实际Army接任组成唯一来源；不增加政治任命或覆写形成史。
#include "military_command_shock_source.hpp"
#include "military_field_command_source.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
Outcome<StableId> leader_unit(const MilitaryBattleRecord& b,const StableId& army,const StableId& actor){StableId result;for(const auto& u:b.units)if(u.army_id==army&&(u.commander_actor_id==actor||has(u.member_ids,actor))){if(!result.empty())return {false,{},common_failure("MilitaryCommandShockLeaderAmbiguous",actor).error};result=u.unit_id;}return {true,std::move(result),{}};}
}
Outcome<MilitaryCommandShockState> prepare_military_command_shock_entry(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const MilitaryBattleRecord& b){
 using R=MilitaryCommandShockState;auto fail=[](const StableId& code,const StableId& id)->Outcome<R>{return {false,{},common_failure(code,id).error};};const auto* contact=find(s.contact_facts,b.source_id);if(!b.rout_retreat_enabled||!contact||contact->siege_id||contact->node_id||contact->tick>b.created_tick||c.tick()==std::numeric_limits<Integer>::max()||(b.created_tick!=c.tick()&&b.created_tick!=c.tick()+1)||!a.fact_exists||!a.fact_exists(contact->id)||b.round!=1||!b.round_receipt_ids.empty())return fail("MilitaryCommandShockEntryRequired",b.id);
 std::set<StableId> ids;for(const auto& u:b.units)ids.insert(u.army_id);R out;
 for(const auto& id:ids){const auto* army=find(s.armies,id);const auto* form=army?find(s.action_facts,army->formation_source):nullptr;const auto* terms=form?std::get_if<ArmyFormationTerms>(&form->terms):nullptr;if(!army||army->status!="active"||!form||!terms||form->command!="army_form"||form->phase!="completed"||!form->success||!form->result||form->result->army_id!=id||terms->order_id!=army->order_id||terms->organization_id!=army->organization_id||form->actor_id!=terms->actor_id||form->completed_tick<0||form->completed_tick>b.created_tick||!a.fact_exists(form->id))return fail("MilitaryCommandShockArmySourceRequired",id);
  MilitaryFieldCommandSource source{MilitaryFieldCommandSourceKind::army_formation,form->id};if(const auto* assigned=current_military_field_army_assignment(s,cat,id,army->commander_actor_id)){if(assigned->tick>c.tick()||!a.fact_exists(assigned->id))return fail("MilitaryCommandShockArmySourceRequired",id);source={MilitaryFieldCommandSourceKind::field_assignment,assigned->id};}else if(army->commander_actor_id!=form->actor_id)return fail("MilitaryCommandShockArmySourceRequired",id);
  const auto* actor=c.actor(army->commander_actor_id);if(!actor||!actor->alive||!actor->available||!has(army->member_actor_ids,actor->id))return fail("MilitaryCommandShockCommanderUnavailable",id);auto at=leader_unit(b,id,actor->id);if(!at.ok)return {false,{},at.error};out.leaders.push_back({id,actor->id,std::move(at.value),source,{}});
 }
 auto valid=validate_military_command_shock_shape(b,out);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};
}
Status validate_military_command_shock_entry(const CommonStore& c,const MilitaryState& s,const MilitaryAccess& a,const MilitaryRoutEntryFact& entry,const MilitaryCommandShockState& state){
 (void)s;auto valid=validate_military_command_shock_shape(entry.initial,state);if(!valid.ok)return valid;if(!state.causes.empty()||!a.army_command_before)return common_failure("MilitaryCommandShockEntryRequired",entry.id);const auto* header=c.state().facts.find(entry.id);const auto& order=c.state().facts.order();const auto boundary=std::find(order.begin(),order.end(),entry.id);if(!header||header->kind!="military_rout_entry"||header->domain!="military"||boundary==order.end())return common_failure("MilitaryCommandShockEntryRequired",entry.id);
 for(const auto& leader:state.leaders){auto source=a.army_command_before(leader.army_id,entry.id);if(!source.ok)return {false,{},source.error};if(source.value.commander_actor_id!=leader.actor_id||source.value.command_source.kind!=leader.command_source.kind||source.value.command_source.id!=leader.command_source.id)return common_failure("MilitaryCommandShockArmySourceMismatch",leader.army_id);const auto* head=c.state().facts.find(leader.command_source.id);const auto at=std::find(order.begin(),order.end(),leader.command_source.id);const auto expected=leader.command_source.kind==MilitaryFieldCommandSourceKind::army_formation?"military_action":"military_field_command_assigned";if(!head||head->kind!=expected||head->domain!="military"||at==order.end()||at>=boundary)return common_failure("MilitaryCommandShockArmySourceOrder",leader.army_id);auto unit=leader_unit(entry.initial,leader.army_id,leader.actor_id);if(!unit.ok)return {false,{},unit.error};if(unit.value!=leader.unit_id)return common_failure("MilitaryCommandShockLeaderUnitMismatch",leader.army_id);}
 return common_success();
}
}
