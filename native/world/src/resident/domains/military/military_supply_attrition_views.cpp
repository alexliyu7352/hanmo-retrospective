// 当前人数只作现况；连续缺粮与上一逃散始终指向原已落供给/流失事实。
#include "military_supply_attrition_views.hpp"
#include "occupation_rules.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
}
Outcome<BoundaryValue> military_supply_attrition_page(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,const MilitaryAccess& a,const StableId& observer){
 if(!cat.supply_attrition_policy)return {true,V{D{{"ok",V{true}},{"active",V{false}}}}, {}};const auto* actor=c.actor(observer);if(observer!=c.state().metadata.player_actor_id||!actor||!actor->alive)return {false,{},common_failure("MilitarySupplyAttritionObserverRequired",observer).error};auto location=c.resolved_location(observer);if(!location.ok)return {false,{},location.error};const auto* place=std::get_if<SettlementLocation>(&location.value);std::set<StableId> allowed;
 for(const auto& army:m.armies)if(army.status=="active"&&has(army.member_actor_ids,observer)){for(const auto& id:army.unit_ids){const auto* u=find(m.military_units,id);if(u&&u->army_id==army.id&&(army.commander_actor_id==observer||u->commander_actor_id==observer||has(u->member_ids,observer)))allowed.insert(id);}}
 if(place)for(const auto& camp:cat.garrisons)if(camp.place_id==place->place_id){auto owner=military_garrison_owner(c,m,cat,a,camp.id);if(!owner.ok)return {false,{},owner.error};bool commanded=false;for(const auto& o:m.offices){const auto* d=find(cat.offices,o.id);if(o.enabled&&o.holder_actor_id==observer&&d&&d->garrison_id==camp.id&&d->organization_id==owner.value.organization_id)commanded=true;}for(const auto& u:m.military_units)if(u.army_id.empty()&&u.garrison_id==camp.id&&u.organization_id==owner.value.organization_id&&(commanded||u.commander_actor_id==observer))allowed.insert(u.id);}
 A rows;bool truncated=false;for(const auto& id:allowed){if(rows.size()>=32){truncated=true;break;}const auto* u=find(m.military_units,id);const MilitarySupplyFact* latest=nullptr;if(!a.rosters_before)return {false,{},common_failure("MilitarySupplyAttritionRosterReadRequired",id).error};for(auto it=m.supply_facts.rbegin();it!=m.supply_facts.rend();++it){auto old=a.rosters_before({id},it->tick,it->id);if(!old.ok)return {false,{},old.error};if(old.value.size()!=1)return {false,{},common_failure("MilitarySupplyAttritionRosterMismatch",id).error};const auto& unit=old.value.front();if(it->army_id.empty()?(unit.army_id.empty()&&it->garrison_id==unit.garrison_id&&it->organization_id==unit.organization_id):it->army_id==unit.army_id){latest=&*it;break;}}
  Integer count=0,eligible=0,quota=0;StableId supply;Integer tick=-1;if(latest){auto run=query_military_unit_supply_run(c,m,cat,a,*cat.supply_attrition_policy,id,latest->id);if(!run.ok)return {false,{},run.error};count=run.value.consecutive_shortage_half_days;eligible=run.value.fit_count;quota=run.value.desertion_quota;supply=latest->id;tick=latest->tick;}const MilitarySupplyAttritionFact* loss=nullptr;for(const auto& f:m.supply_attrition.facts)if(f.unit_id==id)loss=&f;
  rows.push_back(V{D{{"unit_id",V{id}},{"fit_count",V{Integer(u->fit.size())}},{"deserted_count",V{Integer(u->deserted.size())}},{"morale",V{u->morale}},{"last_supply_fact_id",V{supply}},{"last_supply_tick",V{tick}},{"consecutive_shortage_half_days",V{count}},{"last_eligible_count",V{eligible}},{"last_supply_desertion_quota",V{quota}},{"last_attrition_fact_id",V{loss?loss->id:StableId{}}},{"last_attrition_tick",V{loss?loss->tick:Integer{-1}}},{"last_deserted_count",V{loss?Integer(loss->deserted_ids.size()):Integer{0}}}}});
 }
 return {true,V{D{{"ok",V{true}},{"active",V{true}},{"actor_id",V{observer}},{"observed_tick",V{c.tick()}},{"units",V{std::move(rows)}},{"truncated",V{truncated}},{"rule",V{StableId{"缺粮半日士气减10。连续第二个缺粮半日起，每半日按当时可战匿名士卒的2%向上取整逃散；足粮中断连续计数。具名军官与亲兵不纳自动逃散。"}}}}},{}};
}
}
