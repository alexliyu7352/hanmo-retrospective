// 原可航资格与真实启程条款；该域不拥有行旅路线、资源扣款或世界时钟。
#include "naval_navigation.hpp"
#include "naval_world.hpp"
#include "military_field_command_source.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
// DTO只允许在本轮真实准备/执行间传递；业务重试应沿Common命令缓存，不重造启程事实。
bool same_vessels(const std::vector<NavalVoyageVessel>& a,const std::vector<NavalVoyageVessel>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(a[i].vessel_id!=b[i].vessel_id||a[i].unit_id!=b[i].unit_id||a[i].captain_actor_id!=b[i].captain_actor_id||a[i].integrity!=b[i].integrity||!owner_equal(a[i].owner,b[i].owner))return false;return true;}
}
// 新版收费/维修/战斗均完整存在才支持持续航损，作者数值须有限且每种船型可满足远航门。
Status validate_naval_navigation_policy(const NavalNavigationPolicy& p,const NavalCatalog& catalog){
 if(!catalog.combat_policy||!catalog.repair_policy||!catalog.repair_charge_policy)return common_failure("NavalNavigationProfileRequired");
 if(p.profile!="committed_navigation_v1"||p.wear_interval<1||p.wear_interval>10000||p.wear_damage<1||p.wear_damage>100||p.long_voyage_min_half_days<2||p.long_voyage_min_half_days>10000||p.long_voyage_min_integrity<1||p.long_voyage_min_integrity>100)return common_failure("InvalidNavalNavigationPolicy");
 for(const auto& hull:catalog.hulls)if(p.long_voyage_min_integrity>hull.max_integrity)return common_failure("InvalidNavalNavigationHullPolicy",hull.id);return common_success();
}
// 当前同世界证明核人、船、物与军权；本地只按原Travel真实报价冻结额外远航条件。
Outcome<NavalVoyageFact> preview_naval_voyage(const CommonRead& c,const MilitaryState& m,const MilitaryCatalog& mc,const MilitaryAccess& access,const NavalState& n,const NavalCatalog& catalog,const NavalNavigationPolicy& p,const StableId& actor,const TravelQuote& quote){
 auto checked=validate_naval_navigation_policy(p,catalog);if(!checked.ok)return {false,{},checked.error};if(quote.mode!="boat"||quote.road||quote.duration<1||quote.points.size()<2||quote.map_id!=catalog.world_map_id||quote.budget<1)return fail<NavalVoyageFact>("NavalVoyageQuoteRequired",actor);
 auto proof=preview_naval_travel(c,m,mc,access,n,catalog,actor,true);if(!proof.ok)return {false,{},proof.error};if(!proof.value||proof.value->payer_wallet_id!=quote.payer_wallet_id||quote.budget>proof.value->travel_budget)return fail<NavalVoyageFact>("NavalVoyageProofMismatch",actor);
 const auto* fleet=find(n.fleets,proof.value->fleet_id);const auto* army=fleet?find(m.armies,fleet->army_id):nullptr;if(!fleet||!army)return fail<NavalVoyageFact>("NavalVoyageFleetRequired",actor);
 NavalVoyageFact out;out.tick=c.tick();out.actor_id=actor;out.fleet_id=fleet->id;out.army_id=army->id;out.formation_source_id=army->formation_source;out.command_source_id=army->formation_source;out.payer_wallet_id=army->wallet_id;out.quoted_half_days=quote.duration;out.budget=quote.budget;out.map_id=quote.map_id;out.origin=quote.origin;
 if(const auto* assigned=current_military_field_army_assignment(m,mc,army->id,actor))out.command_source_id=assigned->id;
 std::set<StableId> seen;for(const auto& binding:fleet->assignments){const auto* vessel=find(n.vessels,binding.vessel_id);const auto* unit=find(m.military_units,binding.unit_id);if(!vessel||!unit||!seen.insert(vessel->id).second||vessel->fleet_id!=fleet->id||unit->army_id!=army->id)return fail<NavalVoyageFact>("NavalVoyageBindingRequired",binding.vessel_id);if(quote.duration>=p.long_voyage_min_half_days&&vessel->integrity<p.long_voyage_min_integrity)return fail<NavalVoyageFact>("NavalLongVoyageHullRequired",vessel->id);out.vessels.push_back({vessel->id,unit->id,unit->commander_actor_id,vessel->owner,vessel->integrity,{}});}
 if(out.vessels.empty())return fail<NavalVoyageFact>("NavalVoyageBindingRequired",fleet->id);return {true,std::move(out),{}};
}
// 原Travel已创建共同事实后才登记；同一journey重复仅在完整条款相等时回原来源。
Outcome<StableId> record_naval_voyage(CommonWrite& c,const TravelState& travel,NavalNavigationState& state,const NavalVoyageFact& prepared,const StableId& journey_id){
 const auto* journey=travel.journey_facts.find(journey_id);const auto* action=journey?c.action(journey->action_id):nullptr;const auto* params=journey?travel.actions.find(journey->action_id):nullptr;
 if(!journey||!action||!params||journey->mode!="boat"||action->kind!="travel"||action->actor_id!=prepared.actor_id||journey->actor_id!=prepared.actor_id||prepared.tick!=c.tick()||journey->tick!=c.tick()||prepared.budget!=journey->budget||prepared.map_id!=journey->map_id||params->origin!=prepared.origin||params->plan_id!=journey_id||params->payer_wallet_id!=prepared.payer_wallet_id||prepared.quoted_half_days<1||prepared.vessels.empty()||!prepared.id.empty()||!prepared.action_id.empty()||!prepared.journey_id.empty())return fail<StableId>("NavalVoyageStartSourceRequired",journey_id);
 for(const auto& old:state.voyages)if(old.journey_id==journey_id){if(old.action_id!=journey->action_id||old.actor_id!=prepared.actor_id||old.fleet_id!=prepared.fleet_id||old.army_id!=prepared.army_id||old.formation_source_id!=prepared.formation_source_id||old.command_source_id!=prepared.command_source_id||old.payer_wallet_id!=prepared.payer_wallet_id||old.tick!=prepared.tick||old.quoted_half_days!=prepared.quoted_half_days||old.budget!=prepared.budget||old.map_id!=prepared.map_id||old.origin!=prepared.origin||old.action_start_tick!=action->start_tick||old.initial_credit!=params->travel_credit||!same_vessels(old.vessels,prepared.vessels))return fail<StableId>("NavalVoyageStartConflict",journey_id);return {true,old.id,{}};}
 auto id=c.next_id("naval_voyage");if(!id.ok)return id;auto fact=prepared;fact.id=id.value;fact.journey_id=journey_id;fact.action_id=journey->action_id;fact.action_start_tick=action->start_tick;fact.initial_credit=params->travel_credit;auto recorded=c.append_fact({fact.id,"naval_voyage","military_naval"});if(!recorded.ok)return {false,{},recorded.error};state.voyages.push_back(std::move(fact));return id;
}
// 一次已完成导航半日恰加一；周期满才扣原设计损耗，跨俘获或换将不重置累计。
Outcome<NavalNavigationVesselStep> naval_navigation_vessel_step(const NavalNavigationPolicy& p,const StableId& vessel,Integer integrity,Integer completed,const StableId& previous){
 if(p.profile!="committed_navigation_v1"||p.wear_interval<1||p.wear_interval>10000||p.wear_damage<1||p.wear_damage>100||vessel.empty()||integrity<1||integrity>100||completed<0||completed==std::numeric_limits<Integer>::max()||(completed==0)!=previous.empty())return fail<NavalNavigationVesselStep>("InvalidNavalNavigationStep",vessel);
 NavalNavigationVesselStep out;out.vessel_id=vessel;out.previous_step_id=previous;out.completed_before=completed;out.completed_after=completed+1;out.integrity_before=integrity;out.integrity_after=out.completed_after%p.wear_interval==0?std::max<Integer>(0,integrity-p.wear_damage):integrity;return {true,std::move(out),{}};
}
}
