// 新模块冷读必须同时能追溯作者实船、原军团、成功军职和共同事实，失败不修复银行。
#include "naval_validation.hpp"
#include "naval_vessel_identity.hpp"
#include "naval_world.hpp"
#include "naval_commands.hpp"
#include "naval_contact_history.hpp"
#include "military_field_command_source.hpp"
#include "naval_repair_exact_detail.hpp"
#include "resident/domains/actor_foundation/character_presence_v2.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include <algorithm>
#include <set>
#include <charconv>
namespace hanmo::resident {
namespace {
// 来源身份只在当前已验证的候选内解析。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
// 产权业务值沿原Common表达，不把字段显示序当成所有权变化。
bool same_owner(const Owner& a,const Owner& b){return !a.extra&&!b.extra&&a.kind==b.kind&&a.id==b.id;}
// 已通过形状门的共同分配序号用于确认创建/解编先后，不依赖字符串字典序。
Integer serial(const StableId& id){const auto colon=id.find(':');if(colon==StableId::npos)return -1;Integer value=0;const auto begin=id.data()+colon+1;auto parsed=std::from_chars(begin,id.data()+id.size(),value);return parsed.ec==std::errc{}&&parsed.ptr==id.data()+id.size()?value:-1;}
// 舰队和真实军团的unit集合必须相等，不强求用户编队顺序相同。
std::vector<StableId> sorted(std::vector<StableId> ids){std::sort(ids.begin(),ids.end());return ids;}
}
// Only the actual family_death application before disband can authorize the
// original dead-commander delegation. A cause, today's alive, or absence is not
// that applied state. Every local borrowed source dies before its prefix owner.
static Outcome<bool> release_commander_dead_before(const NavalOriginalSourceContext& x,const NavalFleetRecord& f){
 return naval_repair_exact_detail::guard<bool>([&]{
  using naval_repair_exact_detail::take;using naval_repair_exact_detail::fail;
  const auto& body=x.authority.office.body;
  if(&body.common!=&x.common||(body.military&&body.military!=&x.military)||(body.military_exact&&body.military_exact!=&x.exact)||(body.shared&&body.shared!=x.authority.office.access.common_catalog))fail("CrossCandidateNavalPresenceSources",f.id);
  if(!body.deaths||!body.life_sources)fail("NavalReleasePresenceSourcesRequired",f.id);
  // 生命来源借用精确军事实根指针；缺根或异根不允许死亡委任判读。
  if (&body.life_sources->military != &x.military ||
      body.life_sources->military_exact != &x.exact)
    fail("CrossCandidateNavalPresenceSources", f.id);
  const auto now=take(x.authority.office.clock.read_current_moment());
  if(take(compare_world_moments(body.actual_now,now,x.authority.office.budget))!=0)fail("NavalReleasePresenceClockMismatch",f.id);
  const auto* head=x.common.state().facts.find(f.release_source_id);
  if(!head||head->id!=f.release_source_id||head->domain!="military_naval"||head->kind!="naval_disband")fail("NavalReleaseFactRequired",f.id);
  auto nested=take(read_naval_original_prefix_before(x.common,f.release_source_id));
  std::optional<NavalActualPrefixScratch> current_outer;
  if(!x.window)current_outer.emplace(take(read_current_common_fact_prefix(x.common)));
  const MilitaryActualSourceWindow window{x.window?x.window->outer_prefix:current_outer->prefix,x.window?x.window->at:now};
  take(validate_military_actual_source_window(x.common,nested.prefix,now,&window,x.authority.office.budget));
  NavalWindowedFormationSourceRead formations{x,window,now};
  NavalWindowedReturnSourceRead returns{x,window,now};
  auto life=*body.life_sources;life.military_formations=&formations;life.military_returns=&returns;
  const auto presence=take(read_character_applied_presence_at_prefix_v2(x.common,body.state,body.catalog,*body.deaths,life,f.commander_actor_id,nested.prefix,window.outer_prefix,window.at,x.authority.office.clock,body.family_catalog,body.shared,body.born_sources));
  return presence.present&&!presence.alive;
 });
}
// 编组与解编权限共用原历史来源；只读原任职/编组/岗位，不以当前船主倒推权限。
static Status fleet_origin(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const NavalFleetRecord& f,const NavalAssetCatalog& d,const NavalOriginalSourceContext* original){
 if(!a.common_catalog||!a.travel_catalog||!a.fact_exists)return common_failure("NavalWorldCapabilityRequired");
const auto* army=find(s.armies,f.army_id);const auto* formation=find(s.action_facts,f.formation_source);const auto* ft=formation?std::get_if<ArmyFormationTerms>(&formation->terms):nullptr;const auto* appointment=find(s.action_facts,f.appointment_source);const auto* ot=appointment?std::get_if<OfficeTerms>(&appointment->terms):nullptr;
 Integer observed_limit=c.tick();std::optional<MilitaryFormationSourceView> precise_formation;
 if(original){
  if(&static_cast<const CommonRead&>(original->common)!=&c)return common_failure("CrossCandidateNavalOriginalSources");auto bound=validate_naval_original_source_context(*original,original->common,s,cat,a);if(!bound.ok)return bound;
  auto now=original->authority.office.clock.read_current_moment();if(!now.ok)return {false,{},now.error};observed_limit=now.value.half_day_tick;
  if(!army||army->formation_source!=f.formation_source||!c.actor(f.commander_actor_id))return common_failure("NavalFormationHistoryMismatch",f.id);
  auto formed=read_original_military_formation(original->common.state(),s,*army,now.value,&original->formations,original->authority.office.budget);if(!formed.ok)return {false,{},formed.error};precise_formation.emplace(std::move(formed.value));ft=precise_formation->original_terms;
  if(!ft||ft->organization_id!=army->organization_id||precise_formation->completed_source.source_schema!=2)return common_failure("NavalActualFormationCompletionRequired",f.id);
  auto cut=read_naval_original_prefix_before(original->common,f.source_id);if(!cut.ok)return {false,{},cut.error};auto window=validate_military_actual_source_window(original->common,cut.value.prefix,now.value,original->window,original->authority.office.budget);if(!window.ok)return window;
  window=validate_military_source_in_actual_window(original->common,precise_formation->completed_source.source_ref,precise_formation->completed_at,now.value,original->window,original->authority.office.budget);if(!window.ok)return window;
  auto formed_position=read_actual_common_fact_position(original->common,precise_formation->completed_source.source_ref);auto fleet_position=read_actual_common_fact_position(original->common,SourceRef{"military_naval","naval_form",f.source_id});if(!formed_position.ok)return {false,{},formed_position.error};if(!fleet_position.ok)return {false,{},fleet_position.error};if(formed_position.value>=fleet_position.value)return common_failure("NavalFormationHistoryMismatch",f.id);
  const auto* exact_appointment=original->actions.dynamic_action_facts.find(f.appointment_source);if(appointment||!exact_appointment||!exact_appointment->terminal_source_ref)return common_failure(appointment&&exact_appointment?"NavalOfficeSourceDuplicated":"NavalExactOfficeSourceRequired",f.id);
  auto proven=read_exact_office_completion_application(original->common,s,original->actions,original->elapsed,original->authority.office,*exact_appointment->terminal_source_ref,2);if(!proven.ok)return {false,{},proven.error};const auto* terms=std::get_if<OfficeTerms>(&proven.value->original_terms);
  if(!proven.value->completed_at)return common_failure("NavalExactOfficeSourceRequired",f.id);window=validate_military_source_in_actual_window(original->common,*exact_appointment->terminal_source_ref,*proven.value->completed_at,now.value,original->window,original->authority.office.budget);if(!window.ok)return window;
  auto appointed=read_actual_common_fact_position(original->common,*exact_appointment->terminal_source_ref);if(!appointed.ok)return {false,{},appointed.error};
  if(proven.value!=exact_appointment||!terms||terms->office_id!=f.office_id||terms->actor_id!=ft->actor_id||proven.value->actor_id!=ft->actor_id||!proven.value->success||appointed.value>=fleet_position.value)return common_failure("NavalFormationHistoryMismatch",f.id);
 }
 else
  if(!army||!c.actor(f.commander_actor_id)||army->formation_source!=f.formation_source||!formation||formation->command!="army_form"||formation->phase!="completed"||!formation->success||!formation->result||formation->result->army_id!=army->id||!ft||ft->actor_id!=formation->actor_id||ft->organization_id!=army->organization_id||formation->completed_tick>f.created_tick||!appointment||appointment->command!="office_accept"||appointment->phase!="completed"||!appointment->success||appointment->actor_id!=ft->actor_id||appointment->completed_tick>f.created_tick||!ot||ot->office_id!=f.office_id||ot->actor_id!=ft->actor_id)return common_failure("NavalFormationHistoryMismatch",f.id);
 if(ft->actor_id!=f.commander_actor_id){if(original){auto current=read_current_common_fact_prefix(original->common);if(!current.ok)return {false,{},current.error};if(!f.release_source_id.empty()){const auto* head=original->common.state().facts.find(f.release_source_id);if(!head)return common_failure("NavalActualSourceRequired",f.release_source_id);current=read_actual_common_fact_prefix_before(original->common,*head);if(!current.ok)return {false,{},current.error};}NavalActualPrefixScratch cut{current.value};auto now=original->authority.office.clock.read_current_moment();if(!now.ok)return {false,{},now.error};auto command=read_military_army_command_source_at_ordinal_prefix(original->common,s,army->id,cut.prefix,now.value,&original->formations,original->authority.office.budget,original->window);if(!command.ok)return {false,{},command.error};if(!cat.field_command_policy||command.value.commander_actor_id!=f.commander_actor_id||command.value.command_source.kind!=MilitaryFieldCommandSourceKind::field_assignment)return common_failure("MilitaryFieldFleetCommandMismatch",f.id);}else{if(!cat.field_command_policy||!a.fleet_command_source)return common_failure("MilitaryFieldFleetCommandReadRequired",f.id);auto source=a.fleet_command_source(f.id);if(!source.ok)return {false,{},source.error};if(source.value.commander_actor_id!=f.commander_actor_id||source.value.command_source.kind!=MilitaryFieldCommandSourceKind::field_assignment)return common_failure("MilitaryFieldFleetCommandMismatch",f.id);}}
 if(f.created_tick>observed_limit||!naval_request_key_valid(f.request_key)||!a.fact_exists(f.source_id))return common_failure("NavalFormationFactRequired",f.id);
  bool dock=false;for(const auto& b:d.berths)if(b.place_id==f.formation_berth_id&&a.travel_catalog->places.find(b.place_id))dock=true;if(!dock)return common_failure("NavalFormationBerthRequired",f.id);
  if(f.status=="released"){
   const auto* actor=c.actor(f.release_actor_id);const auto* commander=c.actor(f.commander_actor_id);const auto* campaign=find(s.campaigns,army->order_id);const auto* order=campaign?find(cat.orders,campaign->template_id):nullptr;bool authority=false;
   bool commander_dead=!original&&commander&&!commander->alive;
   if(original&&actor&&commander&&f.release_actor_id!=f.commander_actor_id&&f.release_authority_kind!="commander"){auto applied=release_commander_dead_before(*original,f);if(!applied.ok)return {false,{},applied.error};commander_dead=applied.value;}
   if(f.release_authority_kind=="commander")authority=f.release_actor_id==f.commander_actor_id&&f.release_authority_id==f.commander_actor_id&&f.release_authority_source==f.formation_source;
   else if(actor&&commander&&commander_dead&&f.release_actor_id!=f.commander_actor_id){
    if(f.release_authority_kind=="office"&&f.release_authority_id==f.office_id){
     if(original){auto office=read_naval_original_office_before(*original,f.office_id,f.release_source_id);if(!office.ok)return {false,{},office.error};const auto* definition=find(cat.offices,f.office_id);authority=definition&&definition->organization_id==army->organization_id&&office.value.enabled&&office.value.holder_actor_id==f.release_actor_id&&office.value.appointment_id==f.release_authority_source;}
     else{const auto* fact=find(s.action_facts,f.release_authority_source);const auto* terms=fact?std::get_if<OfficeTerms>(&fact->terms):nullptr;
     authority=fact&&fact->command=="office_accept"&&fact->phase=="completed"&&fact->success&&fact->actor_id==f.release_actor_id&&fact->completed_tick<=f.released_tick&&serial(fact->id)<serial(f.release_source_id)&&terms&&terms->office_id==f.office_id&&terms->actor_id==f.release_actor_id&&terms->organization_id==army->organization_id;
     if(authority){for(const auto& later:s.action_facts){const auto* t=std::get_if<OfficeTerms>(&later.terms);if(t&&t->office_id==f.office_id&&later.command=="office_accept"&&later.phase=="completed"&&later.success&&later.completed_tick<=f.released_tick&&serial(later.id)<serial(f.release_source_id)&&(later.completed_tick>fact->completed_tick||(later.completed_tick==fact->completed_tick&&serial(later.id)>serial(fact->id))))authority=false;}
      for(const auto& resignation:s.resignation_facts)if(resignation.office_id==f.office_id&&resignation.actor_id==f.release_actor_id&&resignation.tick>=fact->completed_tick&&resignation.tick<=f.released_tick&&serial(resignation.id)>serial(fact->id)&&serial(resignation.id)<serial(f.release_source_id))authority=false;}}
    }
    if(f.release_authority_kind=="report_role"&&order&&order->report_role_id==f.release_authority_id){const auto* role=a.common_catalog->roles.find(f.release_authority_id);authority=role&&role->organization_id&&*role->organization_id==army->organization_id&&role->initial_actor==f.release_actor_id&&f.release_authority_source=="catalog_role:"+role->id;}
    bool camp=false;for(const auto& g:cat.garrisons)if(g.place_id==f.release_berth_id)camp=true;if(!camp)authority=false;
   }
   if(!actor||!authority)return common_failure("NavalReleaseAuthoritySourceMismatch",f.id);
  }
 if(f.status=="released"&&(f.released_tick>observed_limit||!naval_request_key_valid(f.release_request_key)||!a.fact_exists(f.release_source_id)||!a.travel_catalog->places.find(f.release_berth_id)))return common_failure("NavalReleaseFactRequired",f.id);
 return common_success();
}
Status validate_naval_fleet_origin(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const NavalFleetRecord& f,const NavalAssetCatalog& d){return fleet_origin(c,s,cat,a,f,d,nullptr);}
Status validate_naval_fleet_origin(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const NavalFleetRecord& f,const NavalAssetCatalog& d,const NavalOriginalSourceContext& original){return fleet_origin(c,s,cat,a,f,d,&original);}
// 当前模块只有原始实船和编队/解散；捕获/修理/损伤事实未接入前，坏银行不能先伪造这些结果。
Status validate_naval_world(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const NavalAssetState& n,const NavalAssetCatalog& d){
 auto valid=validate_naval_assets(n,d);if(!valid.ok)return valid;if(!a.common_catalog||!a.common_catalog->extended_skills||!a.travel_catalog||!a.fact_exists)return common_failure("NavalWorldCapabilityRequired");if(d.world_map_id.empty()||a.travel_catalog->world_map_id!=d.world_map_id||!a.travel_catalog->maps.find(d.world_map_id))return common_failure("NavalWaterMapRequired");if(d.vessels.empty()||d.vessels.size()!=n.vessels.size())return common_failure("NavalAuthorSourceRequired");
 // 每艘船只沿作者初泊地或最后一份真实解编来源定位，任意合法泊地不能充当移动证明。
 Integer last_creation=0,last_tick=-1;for(const auto& f:n.fleets){const auto created=serial(f.source_id);if(created<=last_creation||f.created_tick<last_tick||(f.status=="released"&&serial(f.release_source_id)<=created))return common_failure("NavalHistoryOrderMismatch",f.id);last_creation=created;last_tick=f.created_tick;}
 for(const auto& v:n.vessels){const auto* original=find(d.vessels,v.id);if(!original)return common_failure("NavalAuthorSourceRequired",v.id);const NavalFleetRecord* latest=nullptr;for(const auto& f:n.fleets)for(const auto& binding:f.assignments)if(binding.vessel_id==v.id){if(latest&&(latest->status!="released"||serial(f.source_id)<=serial(latest->release_source_id)||f.created_tick<latest->released_tick))return common_failure("NavalVesselHistoryOverlap",v.id);latest=&f;}const auto expected=latest?(latest->status=="active"?latest->formation_berth_id:latest->release_berth_id):original->berth_id;if(v.berth_id!=expected||v.afloat_location||(latest&&latest->status=="active"?v.fleet_id!=latest->id:!v.fleet_id.empty()))return common_failure("NavalVesselLocationSourceMismatch",v.id);}
 std::set<StableId> authored;for(const auto& definition:d.vessels){const auto* v=find(n.vessels,definition.id);if(!authored.insert(definition.id).second||!v)return common_failure("NavalAuthorSourceMismatch",definition.id);auto identity=validate_original_naval_vessel_identity(*v,definition);if(!identity.ok)return identity;if(!same_owner(v->owner,definition.owner))return common_failure("NavalCaptureSourceRequired",v->id);if(v->integrity!=definition.integrity)return common_failure("NavalHullChangeSourceRequired",v->id);const auto* inventory=c.inventory(v->cargo_inventory_id);if(!inventory||!same_owner(inventory->owner,v->owner))return common_failure("NavalCargoSourceMismatch",v->id);if(v->fleet_id.empty()){bool berth=false;for(const auto& b:d.berths)if(b.place_id==v->berth_id&&a.travel_catalog->places.find(b.place_id))berth=true;if(!berth||v->afloat_location)return common_failure("NavalStationarySourceRequired",v->id);}}
 std::set<std::pair<StableId,StableId>> keys;std::set<StableId> sources;for(const auto& f:n.fleets){const auto* army=find(s.armies,f.army_id);auto origin=validate_naval_fleet_origin(c,s,cat,a,f,d);if(!origin.ok)return origin;auto original_actor=naval_formation_actor(s,f);if(!original_actor.ok)return {false,{},original_actor.error};
  if(f.created_tick>c.tick()||!naval_request_key_valid(f.request_key)||!keys.emplace(original_actor.value,f.request_key).second||!sources.insert(f.source_id).second||!a.fact_exists(f.source_id))return common_failure("NavalFormationFactRequired",f.id);
  std::vector<StableId> units;for(const auto& binding:f.assignments){units.push_back(binding.unit_id);const auto* u=find(s.military_units,binding.unit_id);if(!u)return common_failure("NavalUnitSourceRequired",binding.unit_id);if(f.status=="active"&&u->army_id!=f.army_id)return common_failure("NavalUnitArmyMismatch",binding.unit_id);}
  if(f.status=="active"&&(army->status!="active"||army->commander_actor_id!=f.commander_actor_id||sorted(units)!=sorted(army->unit_ids)))return common_failure("NavalActiveArmyMismatch",f.id);
  if(f.status=="active"){auto load=validate_naval_manifest(c,s,n,d,f);if(!load.ok)return {false,{},load.error};for(const auto& binding:f.assignments){for(const auto& battle:s.battles)if(battle.phase!="closed")for(const auto& u:battle.units)if(u.unit_id==binding.unit_id)return common_failure("NavalLandOccupationOverlap",battle.id);for(const auto& siege:s.sieges)if(siege.phase=="active")for(const auto& node:siege.unit_nodes)if(node.first==binding.unit_id)return common_failure("NavalLandOccupationOverlap",siege.id);}}
  if(f.status=="released"&&(f.released_tick>c.tick()||!naval_request_key_valid(f.release_request_key)||!keys.emplace(f.release_actor_id,f.release_request_key).second||!sources.insert(f.release_source_id).second||!a.fact_exists(f.release_source_id)||!a.travel_catalog->places.find(f.release_berth_id)))return common_failure("NavalReleaseFactRequired",f.id);
 }
 return common_success();
}
}
