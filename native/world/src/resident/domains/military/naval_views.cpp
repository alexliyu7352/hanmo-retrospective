#include "resident/domains/military/military_model.hpp"
// 有限只读页来自已验证同世界；编队草案、选船和请求键仍由UI提供，正式命令另核全部来源。
#include "naval_views.hpp"
#include "naval_repair_views.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <set>
#include <map>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
// typed主键只借用本次查询，不跨提交缓存。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
// 来源标识用于拒绝旧页按钮；不成为新的权威世界或存档字段。
V source(const WorldIdentity& s){StableId id;const char* hex="0123456789abcdef";for(auto b:s.world_id){id+=hex[b>>4];id+=hex[b&15];}return boundary::obj({{"world_id",V{id}},{"execution_epoch",V{s.execution_epoch}},{"revision",V{s.revision}},{"tick",V{s.tick}}});}
// 每行只含实际身份，未知名称由UI有限文本入口处理，不伪造人物。
V person(const StableId& id){return boundary::obj({{"id",V{id}}});}
}
// 此页不索取敌军/远方舰队；随军成员只看已成立本军编队，不取得统帅草案权限。
Outcome<V> naval_camp_page(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& access,const NavalAssetState& n,const NavalAssetCatalog& d,const StableId& actor,bool combat_enabled){
 const auto* person_actor=c.actor(actor);if(!person_actor)return {false,{},common_failure("UnknownActor").error};auto location=c.resolved_location(actor);if(!location.ok)return {false,{},location.error};const auto* local=std::get_if<SettlementLocation>(&location.value);StableId berth;if(local)for(const auto& b:d.berths)if(b.place_id==local->place_id)berth=b.place_id;
 const auto* army=active_army_for_actor(s,actor);bool can_command=army&&army->commander_actor_id==actor;const NavalFleetRecord* fleet=nullptr;if(army)for(const auto& f:n.fleets)if(f.status=="active"&&f.army_id==army->id){if(fleet)return {false,{},common_failure("NavalArmyAssignedTwice").error};fleet=&f;}
 std::optional<NavalTravelProof> proof;StableId blocked;if(fleet){auto read=preview_naval_travel(c,s,cat,access,n,d,fleet->commander_actor_id,combat_enabled);if(read.ok)proof=std::move(read.value);else blocked=read.error.original.code;}
 A ships,units,passengers,prisoners,history,relief;bool limited=false;std::set<StableId> crew;
 for(const auto& v:n.vessels){const bool aboard=fleet&&v.fleet_id==fleet->id;const bool owned=(v.owner.kind==Owner::Kind::actor&&v.owner.id==actor)||(army&&v.owner.kind==Owner::Kind::organization&&v.owner.id==army->organization_id);if(!aboard&&!(can_command&&owned&&!berth.empty()&&v.berth_id==berth))continue;if(ships.size()>=128){limited=true;continue;}const auto* h=find(d.hulls,v.hull_id);const auto* authored=find(d.vessels,v.id);if(!h||!authored)return {false,{},common_failure("NavalAuthorSourceRequired",v.id).error};D row{{"id",V{v.id}},{"name_key",V{authored->name_key}},{"fleet_id",V{v.fleet_id}},{"integrity",V{v.integrity}},{"max_integrity",V{h->max_integrity}},{"person_capacity",V{h->person_capacity}},{"cargo_capacity",V{h->cargo_capacity}},{"minimum_crew",V{h->minimum_crew}},{"available",V{can_command&&owned&&v.fleet_id.empty()&&v.integrity>0&&v.berth_id==berth}}};if(proof)for(const auto& load:proof->loads)if(load.vessel_id==v.id)row.emplace_back("load",boundary::obj({{"people",V{load.people}},{"working_crew",V{load.working_crew}},{"cargo_weight",V{load.cargo_weight}}}));ships.push_back(V{std::move(row)});}
 if(army&&can_command){for(const auto& id:army->unit_ids){const auto* u=find(s.military_units,id);if(!u)return {false,{},common_failure("NavalUnitSourceRequired",id).error};crew.insert(u->fit.begin(),u->fit.end());crew.insert(u->wounded.begin(),u->wounded.end());crew.insert(u->commander_actor_id);if(units.size()<128)units.push_back(boundary::obj({{"id",V{id}},{"commander_actor_id",V{u->commander_actor_id}},{"fit_count",V{Integer(u->fit.size())}},{"wounded_count",V{Integer(u->wounded.size())}}}));else limited=true;}
 for(const auto& id:army->member_actor_ids)if(!crew.count(id)){if(passengers.size()<128)passengers.push_back(person(id));else limited=true;}
 for(const auto& id:army->prisoner_ids){if(prisoners.size()<128)prisoners.push_back(person(id));else limited=true;}}
 // 原现场代办只显示该真泊地可正式预览的舰队，不开放远方或活统帅资产。
 for(const auto& f:n.fleets)if(f.status=="active"&&f.commander_actor_id!=actor){auto permitted=preview_naval_release(c,s,cat,access,d,f,actor);if(permitted.ok&&relief.size()<32)relief.push_back(boundary::obj({{"fleet_id",V{f.id}},{"army_id",V{f.army_id}},{"authority_kind",V{permitted.value.authority_kind}}}));}
 for(auto i=n.fleets.rbegin();i!=n.fleets.rend()&&history.size()<32;++i)if(i->commander_actor_id==actor||i->release_actor_id==actor)history.push_back(boundary::obj({{"fleet_id",V{i->id}},{"source_id",V{i->source_id}},{"status",V{i->status}},{"created_tick",V{i->created_tick}},{"release_source_id",V{i->release_source_id}},{"released_tick",V{i->released_tick}}}));
 return {true,boundary::obj({{"source",source(c.state().identity)},{"actor_id",V{actor}},{"army_id",V{army?army->id:StableId{}}},{"commander_actor_id",V{army?army->commander_actor_id:StableId{}}},{"can_command",V{can_command&&!limited}},{"berth_id",V{berth}},{"fleet_id",V{fleet?fleet->id:StableId{}}},{"army_inventory_id",V{can_command?army->inventory_id:StableId{}}},{"ships",V{std::move(ships)}},{"units",V{std::move(units)}},{"passengers",V{std::move(passengers)}},{"prisoners",V{std::move(prisoners)}},{"history",V{std::move(history)}},{"relief",V{std::move(relief)}},{"blocked_code",V{blocked}},{"limited",V{limited}}}),{}};
}
// 派生页保留既有来源与按钮，再附加本情景真实启用的有界工单。
Outcome<BoundaryValue> naval_camp_page(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& mc,const MilitaryAccess& a,const NavalState& n,const NavalCatalog& d,const StableId& actor,bool combat){auto result=naval_camp_page(c,m,mc,a,static_cast<const NavalAssetState&>(n),static_cast<const NavalAssetCatalog&>(d),actor,combat);if(!result.ok||!d.repair_policy)return result;
 // 失能泊地船可选仅代表可提交真实修理配员预览，原blocked_code仍如实显示不可航。
 if(auto* rows=result.value.find("ships"))if(auto* ships=rows->as<BoundaryValue::Array>())for(auto& row:*ships){const auto id=boundary::get_string(row,"id","ships");const auto* vessel=find(n.vessels,id);bool method=false;if(vessel)for(const auto& rule:d.repair_policy->rules)if(rule.hull_id==vessel->hull_id)method=true;const auto* army=active_army_for_actor(m,actor);const auto* person=c.actor(actor);const auto* at=person?std::get_if<SettlementLocation>(&person->location):nullptr;if(vessel&&vessel->integrity==0&&vessel->fleet_id.empty()&&!vessel->afloat_location&&method&&army&&army->commander_actor_id==actor&&vessel->owner.kind==Owner::Kind::organization&&vessel->owner.id==army->organization_id&&at&&vessel->berth_id==at->place_id){for(auto& field:*row.as<BoundaryValue::Dict>())if(field.first=="available")field.second=BoundaryValue{true};}}
 // 只投影本页可见船的真实已提交累计，不泄露远方舰队，也不从世界tick补算。
 if(d.navigation_policy){const auto& policy=*d.navigation_policy;std::map<StableId,const NavalNavigationStepFact*> last;for(const auto& step:n.navigation.steps)for(const auto& ship:step.vessels)last[ship.vessel_id]=&step;BoundaryValue::Array vessels;if(const auto* rows=result.value.find("ships"))for(const auto& row:*rows->as<BoundaryValue::Array>()){const auto id=boundary::get_string(row,"id","ships");Integer count=0;StableId source;auto found=last.find(id);if(found!=last.end()){source=found->second->id;for(const auto& v:found->second->vessels)if(v.vessel_id==id)count=v.completed_after;}vessels.push_back(boundary::obj({{"vessel_id",V{id}},{"completed_half_days",V{count}},{"until_next_wear",V{policy.wear_interval-count%policy.wear_interval}},{"last_step_source_id",V{source}}}));}result.value.as<BoundaryValue::Dict>()->emplace_back("navigation",boundary::obj({{"wear_interval",V{policy.wear_interval}},{"wear_damage",V{policy.wear_damage}},{"long_voyage_min_half_days",V{policy.long_voyage_min_half_days}},{"long_voyage_min_integrity",V{policy.long_voyage_min_integrity}},{"vessels",V{std::move(vessels)}}}));}
 auto repair=naval_repair_page(c,m,mc,a,n,d,actor);if(!repair.ok)return {false,{},repair.error};result.value.as<BoundaryValue::Dict>()->emplace_back("repairs",std::move(repair.value));return result;}

}
