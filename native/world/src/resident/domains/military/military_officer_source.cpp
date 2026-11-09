// 编军完成用真实Army分配序，调任用实际应用fact序；两者合并还原唯一指挥来源。
#include "military_officer_source.hpp"
#include "military_field_command_source.hpp"
#include <algorithm>
#include <charconv>
#include <set>
#include <limits>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
Integer sequence(const StableId& id){const auto at=id.rfind(':');if(at==StableId::npos)return -1;Integer n=0;const auto tail=id.substr(at+1);auto p=std::from_chars(tail.data(),tail.data()+tail.size(),n);return p.ec==std::errc{}&&p.ptr==tail.data()+tail.size()&&n>0&&std::to_string(n)==tail?n:-1;}
template<class T>Outcome<T> fail(const StableId& id){return {false,{},common_failure("MilitaryUnitCommandSourceMismatch",id).error};}
bool same(const MilitaryUnitCommandSource& a,const MilitaryUnitCommandSource& b){return a.kind==b.kind&&a.id==b.id;}
struct Event {Integer sequence=0,tick=0;StableId id,actor;const MilitaryOfficerAssignmentFact* assignment=nullptr;const MilitaryFieldCommandFact* field=nullptr;};
Outcome<MilitaryUnitCommandBinding> binding(const CommonStore& common,const MilitaryState& military,const MilitaryCatalog& catalog,const StableId& id,Integer limit,Integer observed){
    using B=MilitaryUnitCommandBinding;const auto* current=find(military.military_units,id);if(!current)return fail<B>(id);
    const MilitaryUnitDefinition* definition=nullptr;const GarrisonDefinition* origin=nullptr;
    for(const auto& camp:catalog.garrisons)for(const auto& unit:camp.units)if(unit.id==id){if(definition)return fail<B>(id);definition=&unit;origin=&camp;}
    if(!definition||current->source_garrison_id!=origin->id||!common.actor(definition->commander_actor_id))return fail<B>(id);
    B out{definition->commander_actor_id,{MilitaryUnitCommandSourceKind::authored,id}};std::vector<Event> events;std::set<Integer> seen;
    for(const auto& army:military.armies){
        const auto* fact=find(military.action_facts,army.formation_source);const auto* terms=fact?std::get_if<ArmyFormationTerms>(&fact->terms):nullptr;
        if(!terms||std::find(terms->unit_ids.begin(),terms->unit_ids.end(),id)==terms->unit_ids.end())continue;
        const auto* head=common.state().facts.find(fact->id);const auto order=sequence(army.id);const auto started=sequence(fact->id);
        if(fact->command!="army_form"||fact->phase!="completed"||!fact->success||!fact->result||fact->result->army_id!=army.id||!head||head->kind!="military_action"||head->domain!="military"||started<1||order<=started||order>=common.state().next_instance_id||!seen.insert(order).second||fact->completed_tick<fact->start_tick||fact->completed_tick>observed||terms->actor_id!=fact->actor_id)return fail<B>(id);
        const StableId* actor=nullptr;for(const auto& entry:terms->commanders)if(entry.first==id){if(actor)return fail<B>(id);actor=&entry.second;}
        if(!actor||!common.actor(*actor))return fail<B>(id);if(order<limit)events.push_back({order,fact->completed_tick,fact->id,*actor,nullptr});
    }
    for(const auto& fact:military.officer_assignments)if(fact.target.unit_id==id){const auto order=sequence(fact.id);const auto* head=common.state().facts.find(fact.id);if(!catalog.officer_assignment_policy||order<1||order>=common.state().next_instance_id||!seen.insert(order).second||fact.id.rfind("military_assignment:",0)!=0||!head||head->kind!="military_officer_assignment"||head->domain!="military"||fact.tick<0||fact.tick>observed||fact.actor_id!=fact.target.next_actor_id||!common.actor(fact.actor_id))return fail<B>(id);if(order<limit)events.push_back({order,fact.tick,fact.id,fact.actor_id,&fact});}
    for(const auto& fact:military.field_commands)if(fact.target.kind==MilitaryFieldCommandKind::unit&&fact.target.unit_id==id){const auto order=sequence(fact.id);const auto* head=common.state().facts.find(fact.id);if(!catalog.field_command_policy||!catalog.officer_assignment_policy||order<1||order>=common.state().next_instance_id||!seen.insert(order).second||fact.id.rfind("military_field_command:",0)!=0||!head||head->kind!="military_field_command_assigned"||head->domain!="military"||fact.tick<0||fact.tick>observed||fact.actor_id!=fact.target.next_actor_id||!common.actor(fact.actor_id))return fail<B>(id);if(order<limit)events.push_back({order,fact.tick,fact.id,fact.actor_id,nullptr,&fact});}
    std::sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.sequence<b.sequence;});Integer tick=-1;
    for(const auto& e:events){if(e.tick<tick)return fail<B>(e.id);tick=e.tick;if(e.assignment&&(e.assignment->target.previous_actor_id!=out.commander_actor_id||!same(e.assignment->target.previous_source,out.command_source)))return fail<B>(e.id);if(e.field){auto previous=military_field_source(out.command_source);if(e.field->target.previous_actor_id!=out.commander_actor_id||e.field->target.previous_source.kind!=previous.kind||e.field->target.previous_source.id!=previous.id)return fail<B>(e.id);}out.commander_actor_id=e.actor;out.command_source={e.field?MilitaryUnitCommandSourceKind::field_assignment:e.assignment?MilitaryUnitCommandSourceKind::officer_assignment:MilitaryUnitCommandSourceKind::army_formation,e.id};}
    return {true,std::move(out),{}};
}
}
Outcome<MilitaryUnitCommandObservation> query_military_unit_command_source(const CommonStore& common,const MilitaryState& military,const MilitaryCatalog& catalog,const StableId& id){
    auto source=binding(common,military,catalog,id,common.state().next_instance_id,common.tick());if(!source.ok)return {false,{},source.error};const auto* current=find(military.military_units,id);if(current->commander_actor_id!=source.value.commander_actor_id)return fail<MilitaryUnitCommandObservation>(id);
    MilitaryUnitCommandObservation out;out.source_identity=common.state().identity;out.unit_id=id;out.organization_id=current->organization_id;out.garrison_id=current->garrison_id;out.army_id=current->army_id;out.commander_actor_id=source.value.commander_actor_id;out.command_source=source.value.command_source;return {true,std::move(out),{}};
}
Outcome<MilitaryUnitCommandBinding> query_military_unit_command_source_before(const CommonStore& common,const MilitaryState& military,const MilitaryCatalog& catalog,const StableId& id,const StableId& before){const auto limit=sequence(before);if(limit<1||limit>=common.state().next_instance_id||!common.state().facts.find(before))return fail<MilitaryUnitCommandBinding>(before);return binding(common,military,catalog,id,limit,common.tick());}
// 原阶段只有一个真实观察上限，不改共同钟也不开放任意整数历史许可。
Outcome<MilitaryUnitCommandObservation> query_military_unit_command_source(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const StableId& id,const DynamicStepInput& phase){
 if(phase.from_tick<0||phase.from_tick!=c.tick()||phase.from_tick==std::numeric_limits<Integer>::max()||phase.to_tick!=phase.from_tick+1)return fail<MilitaryUnitCommandObservation>(id);
 auto source=binding(c,s,cat,id,c.state().next_instance_id,phase.to_tick);if(!source.ok)return {false,{},source.error};const auto* u=find(s.military_units,id);if(!u||u->commander_actor_id!=source.value.commander_actor_id)return fail<MilitaryUnitCommandObservation>(id);
 MilitaryUnitCommandObservation out;out.source_identity=c.state().identity;out.unit_id=id;out.organization_id=u->organization_id;out.garrison_id=u->garrison_id;out.army_id=u->army_id;out.commander_actor_id=source.value.commander_actor_id;out.command_source=source.value.command_source;return {true,std::move(out),{}};
}
Outcome<MilitaryUnitCommandBinding> query_military_unit_command_source_before(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const StableId& id,const StableId& before,const DynamicStepInput& phase){
 if(phase.from_tick<0||phase.from_tick!=c.tick()||phase.from_tick==std::numeric_limits<Integer>::max()||phase.to_tick!=phase.from_tick+1)return fail<MilitaryUnitCommandBinding>(id);
 const auto limit=sequence(before);if(limit<1||limit>=c.state().next_instance_id||!c.state().facts.find(before))return fail<MilitaryUnitCommandBinding>(before);return binding(c,s,cat,id,limit,phase.to_tick);
}
}
