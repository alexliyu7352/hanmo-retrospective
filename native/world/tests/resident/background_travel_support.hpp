#pragma once
#include "background_support.hpp"
#include "resident/domains/military/military_contact.hpp"
#include "resident/domains/interaction/interaction_access.hpp"
// 测试读取端口只借实际typed记录，调用B生产规划/运动函数，不提供另一套旅行算法。
class Services final:public TravelServices {
public:
    CommonStore& common;MilitaryState& military;InteractionState& interaction;const TravelCatalog& travel;const InteractionCatalog& catalog;MilitaryMembership membership;mutable terrain::Flags flags;
    Services(CommonStore& c,MilitaryState& m,InteractionState& i,const TravelCatalog& t,const InteractionCatalog& cat):common(c),military(m),interaction(i),travel(t),catalog(cat),membership(m){common.bind_membership(&membership);}
    Outcome<Location> resolved_location(const StableId& id)const override{return common.resolved_location(id);}
    StableId party_id(const StableId& id)const override{for(auto& row:common.state().parties.rows())if(std::find(row.second.members.begin(),row.second.members.end(),id)!=row.second.members.end())return row.first;return {};}
    Integer party_extra_cost(const StableId& id)const override{auto p=common.state().parties.find(party_id(id));Integer cost=0;if(p)for(auto& term:p->terms.rows())cost+=term.second.cost_per_tick;return cost;}
    Outcome<std::optional<ArmyTravelView>> active_army(const StableId& id)const override{auto army=active_army_for_actor(military,id);if(!army)return {true,std::nullopt,{}};ArmyTravelView out;out.commander_actor_id=army->commander_actor_id;out.wallet_id=army->wallet_id;out.inventory_id=army->inventory_id;out.member_actor_ids=army->member_actor_ids;for(auto& unit_id:army->unit_ids)for(auto& unit:military.military_units)if(unit.id==unit_id)out.fit_actor_ids.insert(out.fit_actor_ids.end(),unit.fit.begin(),unit.fit.end());return {true,out,{}};}
    bool talking(const StableId& id)const override{return InteractionActivity(military,interaction).in_interaction(id);}
    Outcome<bool> admission_allowed(const StableId&,const StableId& place)const override{for(auto& a:catalog.admissions)if(a.id==place){if(a.conditions.empty())return {true,true,{}};return {false,false,{{"UnsupportedNativeCapability",{}},ErrorKind::unsupported_capability,"test_read","conditions","admission",place}};}return {false,false,common_failure("UnknownPlace").error};}
    const terrain::Flags& terrain_flags()const override{flags.clear();for(auto& flag:interaction.flags)flags.emplace(flag.id,flag.value);return flags;}
    const TravelScheduleDefinition* schedule(const StableId& id)const override{return travel.authored_plans.find(id);}
    const StableId& player_actor_id()const override{return common.state().metadata.player_actor_id;}
};
// 领域报价与能力读取绑定实际伤势/任职/位置，不能把未绑定信息默认为空。
struct Access {
    InteractionAccess interaction;MilitaryAccess military;
    Access(CommonStore& c,MilitaryState& m,InteractionState& i,const InteractionCatalog& catalog,const CommonCatalog& common){interaction.resolve_location=[&c](auto& id){return checked(c.resolved_location(id));};interaction.role=[&c](auto& id)->std::optional<RoleAssignmentView>{auto role=c.state().roles.find(id);if(!role)return {};return RoleAssignmentView{role->actor_id,role->enabled};};interaction.battle_occupied=[&m,&i](auto& id){return InteractionActivity(m,i).in_open_battle(id);};military.interaction=&i;military.interaction_catalog=&catalog;military.interaction_access=&interaction;military.actor_injury=[&c](auto& id){return c.state().actors.find(id)->injury;};military.item_weight=[&common](auto& id){auto item=common.items.find(id);return item?item->weight:0;};military.in_party=[&c](auto& id){for(auto& party:c.state().parties.rows())if(std::find(party.second.members.begin(),party.second.members.end(),id)!=party.second.members.end())return true;return false;};}
};
