// 内河浮水船的有限资产规则；失能不隐含沉没、人员死亡、货物赠与或政治身份。
#include "naval_assets.hpp"
#include "naval_repair.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
// 所有对象仅借当前有限观察，查不到不补造默认实体。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
// 成员判断不重排原作者及事实顺序。
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
// 空身份与重复引用都不是合法名单。
bool unique(const std::vector<StableId>& rows){std::set<StableId> seen;for(const auto& id:rows)if(id.empty()||!seen.insert(id).second)return false;return true;}
// 沿Common原产权类型和ID比较，不接受隐藏兼容字段。
bool same_owner(const Owner& a,const Owner& b){return !a.extra&&!b.extra&&a.kind==b.kind&&a.id==b.id;}
// 首切片只准具名人物或组织持船，其他所有权明确待支持。
bool owner_valid(const Owner& o){return !o.id.empty()&&!o.extra&&(o.kind==Owner::Kind::actor||o.kind==Owner::Kind::organization);}
// 退役编队保历史但不继续占用实船或军员。
bool active(const NavalFleetRecord& f){return f.status=="active";}
// 失败不携部分候选，不把能力缺口写成成功状态。
template<class T>Outcome<T> failure(const StableId& code,const StableId& path={}){Outcome<T> out;out.error.original={code,path};out.error.kind=ErrorKind::rule;return out;}
// 纯校验成功不分配世界ID或修改外部资源。
Status okay(){return {true,{},{}};}
// 动态舰队/事实只用共同分配的规范正整数身份，拒绝空尾、0、别名或前导零。
bool allocated_id(const StableId& id,const StableId& prefix){if(id.rfind(prefix,0)!=0)return false;auto suffix=id.substr(prefix.size());Integer value=0;auto r=std::from_chars(suffix.data(),suffix.data()+suffix.size(),value);return r.ec==std::errc{}&&r.ptr==suffix.data()+suffix.size()&&value>0&&std::to_string(value)==suffix;}
// 只加受容量约束的非负量，先比较剩余空间，避免载重求和溢出。
bool add_bounded(Integer& total,Integer amount,Integer limit){if(amount<0||total>limit||amount>limit-total)return false;total+=amount;return true;}
}
// 业务幂等键有界且拒控制符，所有冷/热入口必须一致。
bool naval_request_key_valid(const StableId& key){return !key.empty()&&key.size()<=128&&std::none_of(key.begin(),key.end(),[](unsigned char c){return c<32||c==127;});}
// 冷形状核实船/编队唯一双向绑定，失能仅限制操作不删除船体。
Status validate_naval_assets(const NavalAssetState& state,const NavalAssetCatalog& catalog){return validate_naval_assets(state,catalog,false);}
// 显式战斗上下文只扩展捕获终态，不放松唯一实船/占用/容量等原始形状。
Status validate_naval_assets(const NavalAssetState& state,const NavalAssetCatalog& catalog,bool combat_enabled){
 if(catalog.profile!="afloat_inland_v1")return failure<Done>("UnsupportedNavalOutcomeProfile");
 std::set<StableId> hull_ids,vessel_ids,holds,fleet_ids,occupied_armies,occupied_units,occupied_people,berths;
 for(const auto& b:catalog.berths)if(b.place_id.empty()||!berths.insert(b.place_id).second||(b.water_kind!="river"&&b.water_kind!="lake"))return failure<Done>("InvalidNavalBerth",b.place_id);
 for(const auto& h:catalog.hulls){if(h.id.empty()||!hull_ids.insert(h.id).second||h.max_integrity<1||h.max_integrity>1000000||h.minimum_crew<1||h.person_capacity<h.minimum_crew||h.person_capacity>1000000||h.cargo_capacity<1||h.cargo_capacity>2147483647||h.minimum_naval_level<0||h.minimum_naval_level>4||h.movement<1||h.movement>24||h.travel_budget<1||h.travel_budget>2147483647||h.waters.empty()||!unique(h.waters))return failure<Done>("InvalidNavalHull",h.id);for(const auto& water:h.waters)if(water!="river"&&water!="lake")return failure<Done>("UnsupportedNavalWater",h.id);}
 for(const auto& v:state.vessels){const auto* hull=find(catalog.hulls,v.hull_id);if(v.id.empty()||!vessel_ids.insert(v.id).second||!hull||!owner_valid(v.owner)||v.origin_source.empty()||v.cargo_inventory_id.empty()||!holds.insert(v.cargo_inventory_id).second||v.integrity<0||v.integrity>hull->max_integrity)return failure<Done>("InvalidNavalVessel",v.id);if(v.fleet_id.empty()&&v.berth_id.empty()==!v.afloat_location.has_value())return failure<Done>("NavalVesselAnchorRequired",v.id);if(v.afloat_location&&!std::holds_alternative<SettlementLocation>(*v.afloat_location)&&!std::holds_alternative<WildernessLocation>(*v.afloat_location))return failure<Done>("InvalidNavalVesselAnchor",v.id);}
 for(const auto& f:state.fleets){if(!allocated_id(f.id,"fleet:")||!fleet_ids.insert(f.id).second||f.army_id.empty()||f.commander_actor_id.empty()||!allocated_id(f.source_id,"naval_action:")||f.office_id.empty()||f.appointment_source.empty()||f.formation_source.empty()||f.formation_berth_id.empty()||f.created_tick<0||(f.assignments.empty()&&f.status!="captured")||(f.status!="active"&&f.status!="released"&&!(combat_enabled&&f.status=="captured")))return failure<Done>("InvalidNavalFleet",f.id);if((active(f)&&(f.released_tick!=-1||!f.release_source_id.empty()||!f.release_request_key.empty()||!f.release_berth_id.empty()||!f.release_actor_id.empty()||!f.release_authority_kind.empty()||!f.release_authority_id.empty()||!f.release_authority_source.empty()))||(f.status=="released"&&(f.released_tick<f.created_tick||!allocated_id(f.release_source_id,"naval_action:")||f.release_request_key.empty()||f.release_berth_id.empty()||f.release_actor_id.empty()||!has({"commander","office","report_role"},f.release_authority_kind)||f.release_authority_id.empty()||f.release_authority_source.empty())))return failure<Done>("InvalidNavalRelease",f.id);if(f.status=="captured"&&(!f.assignments.empty()||f.released_tick<f.created_tick||!allocated_id(f.release_source_id,"naval_capture:")||!naval_request_key_valid(f.release_request_key)||!f.release_berth_id.empty()||f.release_actor_id.empty()||f.release_authority_kind!="capture"||f.release_authority_id.empty()||!allocated_id(f.release_authority_source,"naval_round:")))return failure<Done>("InvalidNavalCaptureRelease",f.id);if(active(f)&&!occupied_armies.insert(f.army_id).second)return failure<Done>("NavalArmyAssignedTwice",f.army_id);std::set<StableId> assigned_vessels;
  for(const auto& a:f.assignments){const auto* v=find(state.vessels,a.vessel_id);if(!v||a.unit_id.empty()||!assigned_vessels.insert(a.vessel_id).second||!unique(a.passenger_ids)||!unique(a.prisoner_ids)||!unique(a.extra_inventory_ids))return failure<Done>("InvalidNavalAssignment",f.id);if(!active(f))continue;if(v->fleet_id!=f.id||!occupied_units.insert(a.unit_id).second)return failure<Done>("NavalAssignmentOverlap",a.unit_id);for(const auto& ids:{a.passenger_ids,a.prisoner_ids})for(const auto& id:ids)if(!occupied_people.insert(id).second)return failure<Done>("NavalPassengerOverlap",id);}
 }
 for(const auto& v:state.vessels)if(!v.fleet_id.empty()){const auto* f=find(state.fleets,v.fleet_id);if(!f||!active(*f))return failure<Done>("NavalFleetSourceMissing",v.id);bool found=false;for(const auto& a:f->assignments)if(a.vessel_id==v.id)found=true;if(!found)return failure<Done>("NavalAssignmentMissing",v.id);}
 return okay();
}
// 权源观察只来自世界适配器；逐船核人货容量，不能借总容量盖住局部超载。
Outcome<NavalFormationQuote> preview_naval_formation(const NavalAssetState& state,const NavalAssetCatalog& catalog,const NavalFormationObservation& o,const NavalFormationRequest& request,bool floating_state){
 return preview_naval_formation(state,catalog,o,request,floating_state,false);
}
// 新上下文只改变历史形状准入，人员、产权和每船容量的算法完全共用。
static Outcome<NavalFormationQuote> preview_formation(const NavalAssetState& state,const NavalAssetCatalog& catalog,const NavalFormationObservation& o,const NavalFormationRequest& request,bool floating_state,bool combat_enabled,bool dock_repair){
 auto valid=validate_naval_assets(state,catalog,combat_enabled);if(!valid.ok)return {false,{},valid.error};
 if(!o.army_active||request.army_id!=o.army_id||o.actor_id.empty()||o.organization_id.empty()||o.office_id.empty()||o.appointment_source.empty()||o.formation_source.empty()||o.berth_id.empty()||o.tick<0)return failure<NavalFormationQuote>("NavalCommanderAuthorityRequired");
 if(o.army_unit_ids.empty()||!unique(o.army_unit_ids)||!unique(o.army_member_ids)||!unique(o.prisoner_ids)||request.assignments.size()!=o.army_unit_ids.size())return failure<NavalFormationQuote>("NavalArmyManifestIncomplete");
 for(const auto& f:state.fleets)if(!floating_state&&active(f)&&f.army_id==o.army_id)return failure<NavalFormationQuote>("NavalArmyAlreadyAssigned",o.army_id);
 std::set<StableId> observed_units,observed_people,observed_inventories;for(const auto& u:o.units)if(u.id.empty()||!observed_units.insert(u.id).second)return failure<NavalFormationQuote>("DuplicateNavalObservedUnit",u.id);for(const auto& p:o.people)if(p.id.empty()||!observed_people.insert(p.id).second||p.naval_level<0||p.naval_level>4)return failure<NavalFormationQuote>("InvalidNavalObservedPerson",p.id);for(const auto& i:o.inventories)if(i.id.empty()||!observed_inventories.insert(i.id).second||i.weight<0)return failure<NavalFormationQuote>("DuplicateNavalObservedInventory",i.id);
 std::set<StableId> vessels,units,people,prisoners,inventories;NavalFormationQuote quote;quote.movement=24;quote.travel_budget=2147483647;Owner public_owner;public_owner.kind=Owner::Kind::organization;public_owner.id=o.organization_id;
 for(const auto& a:request.assignments){const auto* v=find(state.vessels,a.vessel_id);const auto* u=find(o.units,a.unit_id);if(!v||!u||u->army_id!=o.army_id||!has(o.army_unit_ids,a.unit_id)||!vessels.insert(a.vessel_id).second||!units.insert(a.unit_id).second)return failure<NavalFormationQuote>("NavalAssignmentOverlap",a.unit_id);if(!u->busy_source_id.empty())return failure<NavalFormationQuote>("NavalUnitOccupied",u->busy_source_id);if(!floating_state&&!v->fleet_id.empty())return failure<NavalFormationQuote>("NavalVesselAlreadyAssigned",v->id);if(!floating_state&&!dock_repair&&v->integrity==0)return failure<NavalFormationQuote>("NavalVesselDisabled",v->id);if(!floating_state&&(v->berth_id!=o.berth_id||v->afloat_location))return failure<NavalFormationQuote>("NavalVesselNotAtBerth",v->id);if(!(same_owner(v->owner,public_owner)||(v->owner.kind==Owner::Kind::actor&&v->owner.id==o.actor_id)))return failure<NavalFormationQuote>("NavalVesselOwnerUnauthorized",v->id);
  const auto* hull=find(catalog.hulls,v->hull_id);const auto* captain=find(o.people,u->commander_actor_id);if(!captain||(!floating_state&&(!captain->alive||!captain->available||!captain->action_id.empty()||captain->naval_level<hull->minimum_naval_level)))return failure<NavalFormationQuote>("NavalCaptainUnavailable",u->commander_actor_id);if(!unique(u->fit)||!unique(u->wounded)||(!floating_state&&Integer(u->fit.size())<hull->minimum_crew))return failure<NavalFormationQuote>("NavalWorkingCrewInsufficient",u->id);
  NavalVesselLoad load;load.vessel_id=v->id;load.unit_id=u->id;load.working_crew=Integer(u->fit.size());std::set<StableId> aboard;for(const auto& id:u->fit)aboard.insert(id);for(const auto& id:u->wounded)if(!aboard.insert(id).second)return failure<NavalFormationQuote>("NavalCrewSourceOverlap",id);aboard.insert(u->commander_actor_id);
  if(!unique(a.passenger_ids)||!unique(a.prisoner_ids)||!unique(a.extra_inventory_ids))return failure<NavalFormationQuote>("InvalidNavalManifest",v->id);
  for(const auto& id:a.passenger_ids)if(!has(o.army_member_ids,id)||!aboard.insert(id).second)return failure<NavalFormationQuote>("NavalPassengerUnauthorized",id);
  for(const auto& id:a.prisoner_ids)if(!has(o.prisoner_ids,id)||!prisoners.insert(id).second||!aboard.insert(id).second)return failure<NavalFormationQuote>("NavalPrisonerUnauthorized",id);
  load.people=Integer(aboard.size());if(load.people>hull->person_capacity)return failure<NavalFormationQuote>("NavalPeopleOverCapacity",v->id);
  for(const auto& id:aboard)if(!people.insert(id).second)return failure<NavalFormationQuote>("NavalCrewAssignedTwice",id);
  auto cargo=[&](const StableId& id,const Owner& expected)->Status{const auto* value=find(o.inventories,id);if(!value||!same_owner(value->owner,expected))return failure<Done>("NavalCargoSourceMismatch",id);if(!inventories.insert(id).second)return failure<Done>("NavalCargoAssignedTwice",id);if(!add_bounded(load.cargo_weight,value->weight,hull->cargo_capacity))return failure<Done>("NavalCargoOverCapacity",v->id);load.inventory_ids.push_back(id);return okay();};
  auto result=cargo(v->cargo_inventory_id,v->owner);if(!result.ok)return {false,{},result.error};
  for(const auto& id:a.extra_inventory_ids){if(id!=o.army_inventory_id)return failure<NavalFormationQuote>("NavalCargoUnauthorized",id);result=cargo(id,public_owner);if(!result.ok)return {false,{},result.error};}
  for(const auto& id:aboard)if(const auto* person=find(o.people,id)){if(!floating_state&&(!person->alive||!person->action_id.empty()||(!person->available&&(has(u->fit,id)||has(a.passenger_ids,id)||id==u->commander_actor_id)&&!has(a.prisoner_ids,id))))return failure<NavalFormationQuote>("NavalPassengerUnavailable",id);Owner personal;personal.kind=Owner::Kind::actor;personal.id=id;result=cargo(person->inventory_id,personal);if(!result.ok)return {false,{},result.error};}
  quote.movement=std::min(quote.movement,hull->movement);quote.travel_budget=std::min(quote.travel_budget,hull->travel_budget);quote.loads.push_back(std::move(load));
 }
 for(const auto& id:o.army_member_ids)if(!people.count(id))return failure<NavalFormationQuote>("NavalArmyMemberNotEmbarked",id);
 for(const auto& id:o.prisoner_ids)if(!prisoners.count(id))return failure<NavalFormationQuote>("NavalPrisonerNotEmbarked",id);
 if(!inventories.count(o.army_inventory_id))return failure<NavalFormationQuote>("NavalPublicCargoNotEmbarked",o.army_inventory_id);
 return {true,std::move(quote),{}};
}
// 原编队与可航观察绝不打开修理占用例外；仅具名Naval3泊地口可传内部用途。
Outcome<NavalFormationQuote> preview_naval_formation(const NavalAssetState& state,const NavalAssetCatalog& catalog,const NavalFormationObservation& o,const NavalFormationRequest& request,bool floating,bool combat){return preview_formation(state,catalog,o,request,floating,combat,false);}
// 零船损只允许已有公有实船在同一真泊地获得真实配员；不跳船长、技能、位置或载荷门。
Outcome<NavalFormationQuote> preview_naval_dock_repair_formation(const NavalAssetState& state,const NavalCatalog& catalog,const NavalFormationObservation& o,const NavalFormationRequest& request){auto valid=validate_naval_repair_policy(catalog,nullptr);if(!valid.ok)return {false,{},valid.error};for(const auto& binding:request.assignments){const auto* vessel=find(state.vessels,binding.vessel_id);const auto* unit=find(o.units,binding.unit_id);if(!vessel||!unit)return failure<NavalFormationQuote>("NavalAssignmentOverlap",binding.unit_id);if(vessel->integrity!=0)continue;bool supported=false;for(const auto& rule:catalog.repair_policy->rules)if(rule.hull_id==vessel->hull_id&&Integer(unit->fit.size())>=rule.minimum_crew)supported=true;if(!supported||vessel->owner.kind!=Owner::Kind::organization||vessel->owner.id!=o.organization_id)return failure<NavalFormationQuote>("NavalDockRepairCrewRequired",vessel->id);}return preview_formation(state,catalog,o,request,false,true,true);}
// 原输入完整保留，全部守卫后一次返回新候选；外层登记唯一事实及提交。
Outcome<NavalAssetState> commit_naval_formation(const NavalAssetState& state,const NavalAssetCatalog& catalog,const NavalFormationObservation& o,const NavalFormationRequest& request,const StableId& fleet_id,const StableId& source_id){return commit_naval_formation(state,catalog,o,request,fleet_id,source_id,false);}
// 原值候选仅增加显式格式上下文，不移除载重或占用守卫。
static Outcome<NavalAssetState> commit_formation(const NavalAssetState& state,const NavalAssetCatalog& catalog,const NavalFormationObservation& o,const NavalFormationRequest& request,const StableId& fleet_id,const StableId& source_id,bool combat_enabled,const NavalCatalog* repair_catalog){
 auto quote=repair_catalog?preview_naval_dock_repair_formation(state,*repair_catalog,o,request):preview_naval_formation(state,catalog,o,request,false,combat_enabled);if(!quote.ok)return {false,{},quote.error};if(!allocated_id(fleet_id,"fleet:")||!allocated_id(source_id,"naval_action:")||find(state.fleets,fleet_id))return failure<NavalAssetState>("NavalInstanceSourceRequired");NavalAssetState next=state;NavalFleetRecord fleet;fleet.id=fleet_id;fleet.army_id=o.army_id;fleet.commander_actor_id=o.actor_id;fleet.source_id=source_id;fleet.created_tick=o.tick;fleet.office_id=o.office_id;fleet.appointment_source=o.appointment_source;fleet.formation_source=o.formation_source;fleet.formation_berth_id=o.berth_id;fleet.assignments=request.assignments;for(auto& v:next.vessels)for(const auto& a:request.assignments)if(v.id==a.vessel_id)v.fleet_id=fleet_id;next.fleets.push_back(std::move(fleet));auto valid=validate_naval_assets(next,catalog,combat_enabled);if(!valid.ok)return {false,{},valid.error};return {true,std::move(next),{}};
}
// 两个公共入口共用同一占用提交；船体、材料和兵员均不由编队改动。
Outcome<NavalAssetState> commit_naval_formation(const NavalAssetState& state,const NavalAssetCatalog& catalog,const NavalFormationObservation& o,const NavalFormationRequest& request,const StableId& fleet_id,const StableId& source_id,bool combat){return commit_formation(state,catalog,o,request,fleet_id,source_id,combat,nullptr);}
Outcome<NavalAssetState> commit_naval_dock_repair_formation(const NavalAssetState& state,const NavalCatalog& catalog,const NavalFormationObservation& o,const NavalFormationRequest& request,const StableId& fleet_id,const StableId& source_id){return commit_formation(state,catalog,o,request,fleet_id,source_id,true,&catalog);}
}
