#include "field_battle/field_army_exit_ordinary_loss.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include "resident/kernel/typed_supply_observation.hpp"
// 编成完成使用真实army分配序，避免把提前开始的action当成同tick已经成军。
#include "military_roster_history.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/native_military_state.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "occupation_time.hpp"
#include "military_exact_sources.hpp"
#include "field_battle/field_segment_settlement.hpp"
#include "field_battle/field_validation.hpp"
#include "military_formation_source.hpp"
#include "military_return_source.hpp"
#include "military_supply_exact.hpp"
#include "population_military_model.hpp"
#include "resident/model/dynamic_contract.hpp"
#include <limits>
#include <algorithm>
#include <charconv>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
Integer sequence(const StableId& id){const auto at=id.rfind(':');if(at==StableId::npos)return -1;Integer n=0;const auto tail=id.substr(at+1);const auto p=std::from_chars(tail.data(),tail.data()+tail.size(),n);return p.ec==std::errc{}&&p.ptr==tail.data()+tail.size()&&n>0&&std::to_string(n)==tail?n:-1;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
bool equal_members(std::vector<StableId> a,std::vector<StableId> b){std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());return a==b&&std::adjacent_find(a.begin(),a.end())==a.end();}
void erase(std::vector<StableId>& rows,const StableId& id){rows.erase(std::remove(rows.begin(),rows.end(),id),rows.end());}
struct Event {StableId kind,id;Integer tick=0,sequence=0;SourceRef source_ref;Integer schema=1;std::optional<WorldMoment> actual_moment;std::size_t actual_position=0;};
// 收据只改变原分类；多列重复、增加或丢失成员都不能通过历史投影。
bool valid_roster(const MilitaryRosterReceipt& row,const MilitaryUnitRecord& before){std::vector<StableId> all;for(const auto* ids:{&row.fit,&row.wounded,&row.dead,&row.captured,&row.deserted})all.insert(all.end(),ids->begin(),ids->end());if(row.morale<0||row.morale>100||!equal_members(all,before.member_ids)||!equal_members(row.member_ids,before.member_ids))return false;for(const auto& id:before.dead)if(!has(row.dead,id))return false;for(const auto& id:before.captured)if(!has(row.captured,id))return false;for(const auto& id:before.deserted)if(!has(row.deserted,id))return false;for(const auto& id:before.wounded)if(has(row.fit,id))return false;std::vector<StableId> captives;for(const auto& entry:row.captors){if(entry.second.empty())return false;captives.push_back(entry.first);}return equal_members(captives,row.captured);}
}
Status validate_military_actual_source_window(const CommonStore& common,const CommonActualFactPrefix& nested,const WorldMoment& now,const MilitaryActualSourceWindow* window,ExactArithmeticBudget budget){
 auto inner=read_actual_common_fact_prefix(common,nested);if(!inner.ok)return {false,{},inner.error};auto valid=encode_world_moment(now,budget);if(!valid.ok)return {false,{},valid.error};if(!window)return common_success();
 auto outer=read_actual_common_fact_prefix(common,window->outer_prefix);if(!outer.ok)return {false,{},outer.error};if(inner.value>outer.value)return common_failure("MilitaryNestedSourceOutsideWindow");
 valid=encode_world_moment(window->at,budget);if(!valid.ok)return {false,{},valid.error};auto upper=compare_world_moments(window->at,now,budget);if(!upper.ok)return {false,{},upper.error};return upper.value>0?common_failure("MilitarySourceWindowFutureBoundary"):common_success();
}
Status validate_military_source_in_actual_window(const CommonStore& common,const SourceRefBuildView& source,const WorldMoment& occurred,const WorldMoment& now,const MilitaryActualSourceWindow* window,ExactArithmeticBudget budget){
 auto position=read_actual_common_fact_position(common,source);if(!position.ok)return {false,{},position.error};auto valid=encode_world_moment(occurred,budget);if(!valid.ok)return {false,{},valid.error};auto upper=compare_world_moments(occurred,now,budget);if(!upper.ok)return {false,{},upper.error};if(upper.value>0)return common_failure("MilitarySourceWindowFutureSource",StableId(source.id));if(!window)return common_success();
 auto bound=validate_military_actual_source_window(common,window->outer_prefix,now,window,budget);if(!bound.ok)return bound;auto cut=read_actual_common_fact_prefix(common,window->outer_prefix);if(!cut.ok)return {false,{},cut.error};auto relation=compare_world_moments(occurred,window->at,budget);if(!relation.ok)return {false,{},relation.error};if((position.value<cut.value&&relation.value>0)||(position.value>=cut.value&&relation.value<0))return common_failure("MilitarySourceWindowClockOrderMismatch",StableId(source.id));return common_success();
}
Status validate_military_source_in_actual_window(const CommonStore& common,const SourceRef& source,const WorldMoment& occurred,const WorldMoment& now,const MilitaryActualSourceWindow* window,ExactArithmeticBudget budget){return validate_military_source_in_actual_window(common,SourceRefBuildView{source.domain,source.kind,source.id},occurred,now,window,budget);}
// current仅具名当前读可用；历史入口仍要求实际Common边界，不接受虚构占位fact。
static Outcome<std::vector<MilitaryUnitRecord>> rosters_at(const CommonState& common,const MilitaryState& military,const MilitaryCatalog& catalog,const std::vector<StableId>& requested,Integer tick,const StableId& before_id,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* population_catalog,bool current,Integer observed_limit,MilitaryRosterHistoryTrace* trace=nullptr,const MilitaryExactState* exact=nullptr,const WorldMoment* exact_now=nullptr,ExactArithmeticBudget budget={},const WorldMoment* exact_boundary=nullptr,const MilitaryFormationSourceRead* formations=nullptr,const std::map<StableId,MilitaryCompletedReturnSource>* returned_sources=nullptr,const std::map<StableId,WorldMoment>* supply_moments=nullptr,const MilitaryCasualtyHistoryContext* casualty_history=nullptr,MilitaryRosterHistoryObserver* observer=nullptr,const CommonStore* exact_common=nullptr,const CommonActualFactPrefix* actual_prefix=nullptr,const MilitaryActualSourceWindow* window=nullptr,bool selected_exact_mode=false,const std::map<StableId,WorldMoment>* field_moments=nullptr){
 const bool exact_order=exact!=nullptr||selected_exact_mode;
 const Integer limit=exact_order?Integer{0}:(current?common.next_instance_id:sequence(before_id));
 if(tick<0||tick>observed_limit||(!exact_order&&limit<1)||(!actual_prefix&&!current&&!common.facts.find(before_id)))return fail<std::vector<MilitaryUnitRecord>>("MilitaryRosterHistoryBoundaryRequired",before_id);
 std::size_t exact_limit=0;
 if(exact_order){
  if(!exact_common||&exact_common->state()!=&common)return fail<std::vector<MilitaryUnitRecord>>("CrossCandidateMilitaryRosterSources",before_id);
  if(!exact_now)return fail<std::vector<MilitaryUnitRecord>>("MilitaryRosterHistoryBoundaryRequired",before_id);
  if(actual_prefix){auto cut=read_actual_common_fact_prefix(*exact_common,*actual_prefix);if(!cut.ok)return {false,{},cut.error};exact_limit=cut.value;}
  else if(current)exact_limit=common.facts.order().size();
  else{const auto* boundary=common.facts.find(before_id);auto position=read_actual_common_fact_position(common,*boundary);if(!position.ok)return {false,{},std::move(position.error)};exact_limit=position.value;}
 }
 std::map<StableId,MilitaryUnitRecord> units;std::map<StableId,StableId> population_sources;
 for(const auto& id:requested){if(units.count(id))return fail<std::vector<MilitaryUnitRecord>>("MilitaryRosterHistoryDuplicate",id);const MilitaryUnitDefinition* definition=nullptr;const GarrisonDefinition* camp=nullptr;for(const auto& g:catalog.garrisons)if(const auto* d=find(g.units,id)){if(definition)return fail<std::vector<MilitaryUnitRecord>>("MilitaryUnitSourceMissing",id);definition=d;camp=&g;}if(!definition||definition->initial_soldiers<0)return fail<std::vector<MilitaryUnitRecord>>("MilitaryUnitSourceMissing",id);
  MilitaryUnitRecord unit;unit.id=id;unit.source_garrison_id=unit.garrison_id=camp->id;unit.organization_id=camp->organization_id;unit.commander_actor_id=definition->commander_actor_id;unit.troop_type_id=definition->troop_type_id;unit.training=definition->initial_training;unit.fatigue=definition->initial_fatigue;
  if(definition->population_source_id){const auto* source=population_catalog?population_catalog->sources.find(*definition->population_source_id):nullptr;if(!population||!population_catalog||!population_catalog->enabled||!source||source->unit_id!=id||definition->initial_soldiers!=0)return fail<std::vector<MilitaryUnitRecord>>("PopulationRosterHistoryRequired",id);population_sources[id]=source->id;}
  if(trace)trace->authored_unit_sources.push_back({"military","catalog_military_unit",id});
  for(Integer n=0;n<definition->initial_soldiers;++n)unit.member_ids.push_back("soldier:"+id+":"+std::to_string(n+1));unit.fit=unit.member_ids;units.emplace(id,std::move(unit));
 }
 std::vector<Event> events;std::set<Integer> allocated;
 auto add=[&](const StableId& kind,const StableId& id,Integer at,const StableId& header_id,const StableId& header_kind,const StableId& domain,Integer actual_serial=0,Integer schema=1,const WorldMoment* actual_moment=nullptr)->Status{
  const auto seq=actual_serial?actual_serial:sequence(id);const auto* head=common.facts.find(header_id);
  if(seq<1||!head||head->kind!=header_kind||head->domain!=domain||at<0||(actual_prefix&&at>observed_limit))return common_failure("MilitaryRosterHistorySourceMismatch",id);
  if(window&&actual_moment&&(schema==2||(kind=="casualty"&&field_moments&&field_moments->count(id))||(kind=="supply"&&supply_moments&&supply_moments->count(id)))){auto checked=validate_military_source_in_actual_window(*exact_common,SourceRefBuildView{domain,header_kind,header_id},*actual_moment,*exact_now,window,budget);if(!checked.ok)return checked;}
  std::size_t actual_position=0;
  if(exact_order){auto position=read_actual_common_fact_position(common,*head);if(!position.ok)return {false,{},std::move(position.error)};actual_position=position.value;if(actual_position>=exact_limit)return common_success();}
  else if(seq>=limit)return common_success();
  if((!exact_order&&at>tick)||!allocated.insert(seq).second)return common_failure("MilitaryRosterHistoryOrderMismatch",id);
  events.push_back({kind,id,at,seq,{domain,header_kind,header_id},kind=="casualty2"?2:schema,actual_moment?std::optional<WorldMoment>{*actual_moment}:std::nullopt,actual_position});
  return common_success();
 };
 std::map<StableId,MilitaryFormationSourceView> formed_sources;
 for(const auto& army:military.armies){
  auto observation=exact_now?Outcome<WorldMoment>{true,*exact_now,{}}:normalize_world_moment(observed_limit,{},budget);if(!observation.ok)return {false,{},observation.error};
  auto source=read_original_military_formation(common,military,army,observation.value,exact_order?formations:nullptr,budget);if(!source.ok)return {false,{},source.error};
  const auto& ref=source.value.completed_source;auto status=add("formation",army.id,source.value.completed_at.half_day_tick,ref.source_ref.id,ref.source_ref.kind,ref.source_ref.domain,source.value.original_completion_serial,ref.source_schema,&source.value.completed_at);if(!status.ok)return {false,{},status.error};formed_sources.emplace(army.id,std::move(source.value));
 }
 if(exact_order)for(const auto& head:common.facts.rows())if(head.second.domain=="military"&&head.second.kind=="military_action_completion_applied"){std::size_t matches=0;for(const auto& source:formed_sources)if(source.second.completed_source.source_ref.id==head.first)++matches;if(matches!=1)return fail<std::vector<MilitaryUnitRecord>>("MilitaryFormationCompletionCoverage",head.first);}
 for(const auto& fact:military.officer_assignments){auto s=add("officer_assignment",fact.id,fact.tick,fact.id,"military_officer_assignment","military");if(!s.ok)return {false,{},s.error};}
 for(const auto& fact:military.field_commands){auto s=add("field_command",fact.id,fact.tick,fact.id,"military_field_command_assigned","military");if(!s.ok)return {false,{},s.error};}
 for(const auto& fact:military.supply_facts){
  const WorldMoment* actual=nullptr;if(supply_moments){const auto found=supply_moments->find(fact.id);if(found!=supply_moments->end())actual=&found->second;}
  // 原 Supply.tick 是 period-to 标签；发生时刻只取原源核过的 Opening.from。
  // Field /1 与旧 dynamic /2 保持各自真实 schema，不因有精确时刻冒充 /2。
  auto s=add("supply",fact.id,actual?actual->half_day_tick:fact.tick,fact.id,"military_supply","military",0,actual&&exact?2:1,actual);if(!s.ok)return {false,{},s.error};
 }
 for(const auto& fact:military.casualty_facts){const WorldMoment* actual=nullptr;if(field_moments){auto found=field_moments->find(fact.id);if(found!=field_moments->end())actual=&found->second;}auto s=add("casualty",fact.id,fact.tick,fact.id,"military_casualties","military",0,1,actual);if(!s.ok)return {false,{},s.error};}
 if(exact)for(const auto& entry:exact->dynamic_casualty_facts.rows()){const auto& fact=entry.second;if(entry.first!=fact.id)return fail<std::vector<MilitaryUnitRecord>>("MilitaryCasualtyKeyMismatch",entry.first);auto at=read_military_source_moment(military,*exact,{"military","military_casualties",fact.id},2,budget,casualty_history);if(!at.ok)return {false,{},at.error};if(!exact_now)return fail<std::vector<MilitaryUnitRecord>>("MilitaryExactHistoryMomentRequired",fact.id);auto within=compare_world_moments(at.value,*exact_now,budget);if(!within.ok)return {false,{},within.error};if(within.value>0)return fail<std::vector<MilitaryUnitRecord>>("MilitaryExactHistoryFutureSource",fact.id);auto status=add("casualty2",fact.id,at.value.half_day_tick,fact.id,"military_casualties","military",0,2,&at.value);if(!status.ok)return {false,{},status.error};}
 if(!catalog.recovery_policy&&(!military.recovery.facts.empty()||!military.recovery.battle_releases.empty()||!military.recovery.siege_releases.empty()))return fail<std::vector<MilitaryUnitRecord>>("UnexpectedMilitaryRecovery");
 if(!catalog.supply_attrition_policy&&(!military.supply_attrition.facts.empty()||!military.supply_attrition.field_order.empty()))return fail<std::vector<MilitaryUnitRecord>>("UnexpectedMilitarySupplyAttrition");
 for(const auto& fact:military.supply_attrition.facts){if(fact.id.rfind("military_attrition:",0)!=0)return fail<std::vector<MilitaryUnitRecord>>("MilitarySupplyAttritionSourceMismatch",fact.id);auto s=add("supply_attrition",fact.id,fact.tick,fact.id,"military_supply_attrition","military");if(!s.ok)return {false,{},s.error};}
 for(const auto& fact:military.recovery.facts){if(fact.id.rfind("military_recovery:",0)!=0)return fail<std::vector<MilitaryUnitRecord>>("MilitaryRecoverySourceMismatch",fact.id);auto s=add("recovery",fact.id,fact.tick,fact.id,"military_wounded_recovery","military");if(!s.ok)return {false,{},s.error};}
 // Military8的当前分类总门也消费原占领移交；旧档仍沿原历史入口，不追增新字节。
 if(catalog.recovery_policy)for(const auto& fact:military.occupation_facts)if(fact.assets_applied.value_or(false)){
  if(!fact.asset_manifest||!fact.custody_transfers)return fail<std::vector<MilitaryUnitRecord>>("OccupationAssetsNotApplied",fact.id);
  if(exact_order){
   auto source_memory=CheckpointMemoryCharge(0);
   const SourceRef source{"military","military_occupation",fact.id};
   auto actual=read_military_occupation_actual_moment(*exact_common,military,catalog,source,2,*exact_now,budget);
   if(!actual.ok)return {false,{},std::move(actual.error)};
   auto s=add("occupation_custody",fact.id,fact.tick,fact.id,"military_occupation","military",0,2,&actual.value);if(!s.ok)return {false,{},std::move(s.error)};
  }else{auto s=add("occupation_custody",fact.id,fact.tick,fact.id,"military_occupation","military",0,1,nullptr);if(!s.ok)return {false,{},std::move(s.error)};}
 }
 for(const auto& fact:military.return_facts){auto s=add("return",fact.id,fact.tick,fact.id,"military_return","military");if(!s.ok)return {false,{},s.error};}
 if(returned_sources)for(const auto& entry:*returned_sources){const auto& source=entry.second;const auto& fact=*source.original;const auto& applied=actual_prefix?source.actual_application.source_ref:source.actual_return.source_ref;
  if(actual_prefix){auto receipt=read_actual_common_fact_position(common,source.actual_return.source_ref),application=read_actual_common_fact_position(common,source.actual_application.source_ref);if(!receipt.ok)return {false,{},receipt.error};if(!application.ok)return {false,{},application.error};if(receipt.value>=application.value||(receipt.value<actual_prefix->common_fact_count)!=(application.value<actual_prefix->common_fact_count))return fail<std::vector<MilitaryUnitRecord>>("MilitaryReturnApplicationOrderMismatch",fact.id);}
  auto s=add("return2",fact.id,fact.completed_at.half_day_tick,applied.id,applied.kind,applied.domain,0,2,&fact.completed_at);if(!s.ok)return {false,{},s.error};}
 for(const auto& fact:military.integration_facts){auto s=add("integration",fact.id,fact.tick,fact.id,"military_integration","military");if(!s.ok)return {false,{},s.error};}
 if(population)for(const auto& item:population->facts.rows()){const auto& fact=item.second;if(fact.kind!="population_enrolled"&&fact.kind!="population_released")continue;auto s=add(fact.kind,fact.id,fact.tick,fact.id,fact.kind,"population_military");if(!s.ok)return {false,{},s.error};}
 std::sort(events.begin(),events.end(),[exact_order](const auto& a,const auto& b){if(exact_order&&a.actual_position!=b.actual_position)return a.actual_position<b.actual_position;return a.sequence<b.sequence;});
 Integer last_tick=-1;std::optional<WorldMoment> last_exact;std::set<StableId> enrolled_sources;
 for(const auto& event:events){
  world_stage_observation::observe_node({world_stage_observation::NodeMetric::loop_candidate_visit, "military.roster.event_fold", &common, nullptr, {"fact", event.source_ref.domain, event.source_ref.kind, event.source_ref.id, event.schema}, {}, exact_order ? world_stage_observation::observed_cut(event.actual_position) : -1});
  if(!exact_order){if(event.tick<last_tick)return fail<std::vector<MilitaryUnitRecord>>("MilitaryRosterHistoryOrderMismatch",event.id);last_tick=event.tick;}
  if(exact_order){
   if(event.kind=="formation"&&event.schema!=2){
    if(!actual_prefix)return fail<std::vector<MilitaryUnitRecord>>("MilitaryExactHistoryOccurrenceRequired",event.id);
    bool selected_formation=false;const auto* terms=formed_sources.at(event.id).original_terms;if(terms)for(const auto& id:terms->unit_ids)selected_formation=selected_formation||units.count(id)!=0;
    if(selected_formation)return fail<std::vector<MilitaryUnitRecord>>("MilitaryActualFormationCompletionRequired",event.id);
    // Identity/coverage above still ran. A legacy normalized completion tick
    // is not genuine M and an unrelated formation cannot change these units.
    continue;
   }
   const WorldMoment* moment=event.actual_moment?&*event.actual_moment:nullptr;
   if(!moment&&event.kind=="casualty2"){const auto* casualty=exact->dynamic_casualty_facts.find(event.id);if(!casualty)return fail<std::vector<MilitaryUnitRecord>>("MilitaryRosterHistorySourceMismatch",event.id);moment=&casualty->effective_at;}
   if(!moment&&!actual_prefix)return fail<std::vector<MilitaryUnitRecord>>("MilitaryExactHistoryOccurrenceRequired",event.id);
   if(actual_prefix&&!moment){
    // This is an original integer-only event, not an exact occurrence. The
    // real header/position, legal/nonfuture tick, profile and full original
    // predecessor/partition reducer remain mandatory. No M is synthesized.
    if(event.tick<0||event.tick>exact_now->half_day_tick)return fail<std::vector<MilitaryUnitRecord>>("MilitaryRosterHistoryOrderMismatch",event.id);
   }
   if(moment){
   const auto* boundary=exact_boundary?exact_boundary:exact_now;
   auto within=compare_world_moments(*moment,*boundary,budget);if(!within.ok)return {false,{},std::move(within.error)};if(within.value>0)return fail<std::vector<MilitaryUnitRecord>>("MilitaryExactHistoryBoundaryMomentMismatch",event.id);
   if(last_exact){auto order=compare_world_moments(*last_exact,*moment,budget);if(!order.ok)return {false,{},std::move(order.error)};if(order.value>0)return fail<std::vector<MilitaryUnitRecord>>("MilitaryExactHistoryOrderMismatch",event.id);}
   last_exact=*moment;
   }
  }
  // Provenance uses the original producer serial, not the current table order.
  // A full ordered chain is retained, including changes whose result is still equal.
  bool relevant=false;
  auto selected=[&](const StableId& id){return units.find(id)!=units.end();};
  if(event.kind=="formation"){const auto* t=formed_sources.at(event.id).original_terms;if(t)for(const auto& id:t->unit_ids)relevant=relevant||selected(id);}
  else if(event.kind=="officer_assignment")relevant=selected(find(military.officer_assignments,event.id)->target.unit_id);
  else if(event.kind=="field_command"){const auto& target=find(military.field_commands,event.id)->target;relevant=selected(target.unit_id);for(const auto& row:units)relevant=relevant||row.second.army_id==target.army_id;}
  else if(event.kind=="supply"){const auto& f=*find(military.supply_facts,event.id);for(const auto& row:units){const auto& u=row.second;relevant=relevant||(!f.army_id.empty()?u.army_id==f.army_id:u.army_id.empty()&&u.garrison_id==f.garrison_id&&u.organization_id==f.organization_id);}}
  else if(event.kind=="casualty"||event.kind=="casualty2"){const auto& receipts=event.kind=="casualty"?find(military.casualty_facts,event.id)->units:exact->dynamic_casualty_facts.find(event.id)->roster_receipts;for(const auto& row:receipts)relevant=relevant||selected(row.unit_id);}
  else if(event.kind=="supply_attrition")relevant=selected(find(military.supply_attrition.facts,event.id)->unit_id);
  else if(event.kind=="recovery")relevant=selected(find(military.recovery.facts,event.id)->unit_id);
  else if(event.kind=="occupation_custody"){for(const auto& t:*find(military.occupation_facts,event.id)->custody_transfers)for(const auto& row:units)relevant=relevant||has(row.second.member_ids,t.person_id);}
  else if(event.kind=="return"||event.kind=="return2"){const auto& ids=event.kind=="return"?find(military.return_facts,event.id)->unit_ids:returned_sources->at(event.id).original->unit_ids;for(const auto& id:ids)relevant=relevant||selected(id);}
  else if(event.kind=="integration"){for(const auto& a:find(military.integration_facts,event.id)->source_armies)for(const auto& id:a.unit_ids)relevant=relevant||selected(id);}
  else{const auto* f=population?population->facts.find(event.id):nullptr;const auto* source=f&&population_catalog?population_catalog->sources.find(f->source_id):nullptr;relevant=source&&selected(source->unit_id);}
  if(trace&&relevant){trace->original_changes.push_back({event.source_ref,event.schema,event.sequence});trace->composition_revision=std::max(trace->composition_revision,event.sequence);}
  const MilitaryHistorySource original_event{event.source_ref,event.schema,event.sequence};
  if(observer&&relevant){if(!exact_order||!last_exact)return fail<std::vector<MilitaryUnitRecord>>("MilitaryExactHistoryObserverRequired",event.id);for(const auto& id:requested){auto observed=observer->before_original_event(original_event,*last_exact,units.at(id));if(!observed.ok)return {false,{},observed.error};}}


  if(event.kind=="formation"){const auto* army=find(military.armies,event.id);const auto& terms=*formed_sources.at(event.id).original_terms;for(const auto& id:terms.unit_ids){auto it=units.find(id);if(it==units.end())continue;auto& unit=it->second;if(!unit.army_id.empty()||unit.garrison_id!=terms.garrison_id||unit.organization_id!=terms.organization_id)return fail<std::vector<MilitaryUnitRecord>>("MilitaryRosterFormationPredecessorMismatch",id);unit.army_id=army->id;unit.garrison_id.clear();for(const auto& commander:terms.commanders)if(commander.first==id)unit.commander_actor_id=commander.second;}
   auto retainers=terms.retainers;std::sort(retainers.begin(),retainers.end(),[](const auto& a,const auto& b){return a.actor_id<b.actor_id;});for(const auto& retainer:retainers){auto it=units.find(retainer.unit_id);if(it==units.end())continue;auto& unit=it->second;if(unit.army_id!=army->id||retainer.actor_id.empty()||has(unit.member_ids,retainer.actor_id))return fail<std::vector<MilitaryUnitRecord>>("MilitaryRosterRetainerSourceMismatch",retainer.actor_id);unit.member_ids.push_back(retainer.actor_id);unit.fit.push_back(retainer.actor_id);unit.retainer_sources.emplace_back(retainer.actor_id,retainer.source_id);}
  }else if(event.kind=="officer_assignment"){const auto* fact=find(military.officer_assignments,event.id);auto it=units.find(fact->target.unit_id);if(it!=units.end()){auto& unit=it->second;if(!catalog.officer_assignment_policy||!unit.army_id.empty()||unit.garrison_id!=fact->target.garrison_id||unit.organization_id!=fact->target.organization_id||unit.commander_actor_id!=fact->target.previous_actor_id||fact->actor_id!=fact->target.next_actor_id)return fail<std::vector<MilitaryUnitRecord>>("MilitaryOfficerRosterPredecessorMismatch",event.id);unit.commander_actor_id=fact->target.next_actor_id;}
  }else if(event.kind=="field_command"){const auto* fact=find(military.field_commands,event.id);if(!catalog.field_command_policy||!catalog.officer_assignment_policy)return fail<std::vector<MilitaryUnitRecord>>("MilitaryFieldCommandProfileRequired",event.id);if(fact->target.kind==MilitaryFieldCommandKind::unit){auto it=units.find(fact->target.unit_id);if(it!=units.end()){auto& unit=it->second;if(unit.army_id!=fact->target.army_id||unit.organization_id!=fact->target.organization_id||unit.commander_actor_id!=fact->target.previous_actor_id||fact->actor_id!=fact->target.next_actor_id||!has(unit.fit,fact->actor_id))return fail<std::vector<MilitaryUnitRecord>>("MilitaryFieldRosterPredecessorMismatch",event.id);unit.commander_actor_id=fact->actor_id;}}
  }else if(event.kind=="supply"){const auto* fact=find(military.supply_facts,event.id);if(fact->required<0||fact->consumed<0||fact->consumed>fact->required||fact->shortage!=fact->required-fact->consumed)return fail<std::vector<MilitaryUnitRecord>>("MilitarySupplyHistoryMismatch",event.id);for(auto& entry:units){auto& unit=entry.second;const bool applies=!fact->army_id.empty()?unit.army_id==fact->army_id:unit.army_id.empty()&&unit.garrison_id==fact->garrison_id&&unit.organization_id==fact->organization_id;if(!applies)continue;for(const auto* mouths:{&unit.fit,&unit.wounded})for(const auto& member:*mouths)if(!has(fact->mouth_ids,member))return fail<std::vector<MilitaryUnitRecord>>("MilitarySupplyRosterHistoryMismatch",event.id);if(fact->shortage>0&&!unit.fit.empty())unit.morale=catalog.rout_retreat_policy&&unit.morale==0?0:std::max<Integer>(1,unit.morale-8);}
  }else if(event.kind=="casualty"||event.kind=="casualty2"){const auto& receipts=event.kind=="casualty"?find(military.casualty_facts,event.id)->units:exact->dynamic_casualty_facts.find(event.id)->roster_receipts;std::set<StableId> touched;for(const auto& row:receipts){if(!touched.insert(row.unit_id).second)return fail<std::vector<MilitaryUnitRecord>>("MilitaryCasualtyHistoryMismatch",event.id);auto it=units.find(row.unit_id);if(it==units.end())continue;auto& unit=it->second;if(!valid_roster(row,unit))return fail<std::vector<MilitaryUnitRecord>>("MilitaryCasualtyHistoryMismatch",event.id);unit.member_ids=row.member_ids;unit.fit=row.fit;unit.wounded=row.wounded;unit.dead=row.dead;unit.captured=row.captured;unit.deserted=row.deserted;unit.captors=row.captors;unit.morale=row.morale;}
  }else if(event.kind=="supply_attrition"){const auto* fact=find(military.supply_attrition.facts,event.id);auto it=units.find(fact->unit_id);if(it!=units.end()){auto& unit=it->second;if(unit.fit!=fact->fit_before)return fail<std::vector<MilitaryUnitRecord>>("MilitarySupplyAttritionRosterPredecessorMismatch",event.id);std::set<StableId> seen;for(const auto& member:fact->deserted_ids){if(!seen.insert(member).second||common.actors.find(member)||!has(unit.fit,member)||has(unit.deserted,member)||!has(fact->eligible_fit_before,member))return fail<std::vector<MilitaryUnitRecord>>("MilitarySupplyAttritionRosterPredecessorMismatch",member);erase(unit.fit,member);unit.deserted.push_back(member);}if(!fact->fit_before.empty())unit.morale=catalog.rout_retreat_policy&&unit.morale==0?0:std::max<Integer>(1,unit.morale-2);}
  }else if(event.kind=="recovery"){const auto* fact=find(military.recovery.facts,event.id);auto it=units.find(fact->unit_id);if(it!=units.end()){auto& unit=it->second;if(!unit.army_id.empty()||unit.garrison_id!=fact->garrison_id||unit.organization_id!=fact->organization_id||fact->tick<1)return fail<std::vector<MilitaryUnitRecord>>("MilitaryRecoveryRosterPredecessorMismatch",event.id);std::set<StableId> recovered;for(const auto& member:fact->recovered_ids){if(!recovered.insert(member).second||!has(unit.member_ids,member)||!has(unit.wounded,member)||has(unit.fit,member)||has(unit.dead,member)||has(unit.captured,member)||has(unit.deserted,member))return fail<std::vector<MilitaryUnitRecord>>("MilitaryRecoveryRosterPredecessorMismatch",member);erase(unit.wounded,member);unit.fit.push_back(member);}}
  }else if(event.kind=="occupation_custody"){const auto* fact=find(military.occupation_facts,event.id);std::set<StableId> people;for(const auto& transfer:*fact->custody_transfers){if(transfer.person_id.empty()||transfer.from_army_id.empty()||transfer.to_army_id!=fact->army_id||!people.insert(transfer.person_id).second)return fail<std::vector<MilitaryUnitRecord>>("MilitaryOccupationCustodyHistoryMismatch",event.id);for(auto& entry:units){auto& unit=entry.second;if(!has(unit.member_ids,transfer.person_id))continue;if(!has(unit.captured,transfer.person_id))return fail<std::vector<MilitaryUnitRecord>>("MilitaryOccupationCustodyHistoryMismatch",event.id);auto source=std::find_if(unit.captors.begin(),unit.captors.end(),[&](const auto& item){return item.first==transfer.person_id;});if(source==unit.captors.end()||source->second!=transfer.from_army_id)return fail<std::vector<MilitaryUnitRecord>>("MilitaryOccupationCustodyHistoryMismatch",event.id);source->second=transfer.to_army_id;}}
  }else if(event.kind=="return"||event.kind=="return2"){
   auto apply_return=[&](const auto* fact)->Status{for(const auto& id:fact->unit_ids){auto it=units.find(id);if(it==units.end())continue;auto& unit=it->second;if(unit.army_id!=fact->army_id)return common_failure("MilitaryReturnRosterHistoryMismatch",id);for(const auto& person:fact->returned_retainers){auto source=std::find_if(unit.retainer_sources.begin(),unit.retainer_sources.end(),[&](const auto& p){return p.first==person;});if(source==unit.retainer_sources.end())continue;if(!has(unit.fit,person)&&!has(unit.wounded,person))return common_failure("MilitaryReturnRosterHistoryMismatch",person);erase(unit.fit,person);erase(unit.wounded,person);erase(unit.member_ids,person);unit.retainer_sources.erase(source);}unit.army_id.clear();unit.garrison_id=fact->garrison_id;}
return common_success();};
   const auto applied=event.kind=="return"?apply_return(find(military.return_facts,event.id)):apply_return(returned_sources->at(event.id).original);if(!applied.ok)return {false,{},applied.error};
  }else if(event.kind=="integration"){const auto* fact=find(military.integration_facts,event.id);for(const auto& army:fact->source_armies)for(const auto& id:army.unit_ids){auto it=units.find(id);if(it==units.end())continue;if(it->second.army_id!=army.id)return fail<std::vector<MilitaryUnitRecord>>("MilitaryIntegrationRosterHistoryMismatch",id);it->second.army_id=fact->recipient_army_id;it->second.organization_id=fact->recipient_organization_id;}for(const auto& transfer:fact->custody)for(auto& entry:units)for(auto& keeper:entry.second.captors)if(keeper.first==transfer.member_id){if(keeper.second!=transfer.from_army_id)return fail<std::vector<MilitaryUnitRecord>>("MilitaryCustodyHistoryMismatch",event.id);keeper.second=transfer.to_army_id;}
  }else{const auto* fact=population->facts.find(event.id);const auto* source=population_catalog?population_catalog->sources.find(fact->source_id):nullptr;if(!source)return fail<std::vector<MilitaryUnitRecord>>("PopulationRosterHistoryRequired",event.id);auto it=units.find(source->unit_id);if(it==units.end())continue;auto& unit=it->second;if(population_sources[unit.id]!=source->id)return fail<std::vector<MilitaryUnitRecord>>("PopulationRosterHistoryRequired",event.id);if(event.kind=="population_enrolled"){const auto* enrollment=population->enrollments.find(event.id);if(!enrollment||enrollment->unit_id!=unit.id||enrollment->source_id!=source->id||enrollment->completed_tick!=event.tick||enrollment->members.size()!=fact->movements.size())return fail<std::vector<MilitaryUnitRecord>>("PopulationRosterHistoryMismatch",event.id);for(std::size_t n=0;n<enrollment->members.size();++n){const auto& token=enrollment->members[n].member_id;const auto& movement=fact->movements[n];if(token!="soldier:population:"+event.id+":"+std::to_string(n+1)||movement.member_id!=token||movement.enrollment_id!=enrollment->id||movement.before!=PopulationSoldierStatus::released||movement.after!=PopulationSoldierStatus::fit||has(unit.member_ids,token))return fail<std::vector<MilitaryUnitRecord>>("PopulationRosterHistoryMismatch",event.id);unit.member_ids.push_back(token);unit.fit.push_back(token);}if(enrolled_sources.insert(source->id).second){unit.morale=source->initial_morale;unit.training=source->initial_training;unit.fatigue=source->initial_fatigue;}
   }else for(const auto& movement:fact->movements){if(movement.before!=PopulationSoldierStatus::fit||movement.after!=PopulationSoldierStatus::released||!has(unit.fit,movement.member_id))return fail<std::vector<MilitaryUnitRecord>>("PopulationReleaseHistoryMismatch",event.id);erase(unit.fit,movement.member_id);erase(unit.member_ids,movement.member_id);}}
  if(observer&&relevant)for(const auto& id:requested){auto observed=observer->after_original_event(original_event,*last_exact,units.at(id));if(!observed.ok)return {false,{},observed.error};}
 }
 std::vector<MilitaryUnitRecord> result;for(const auto& id:requested)result.push_back(std::move(units.at(id)));return {true,std::move(result),{}};
}
// 原历史入口仍必须是真实fact；恢复重放只施加分类原语，完整资格/人物康复交晚门核。
Outcome<std::vector<MilitaryUnitRecord>> military_rosters_before(const CommonState& common,const MilitaryState& military,const MilitaryCatalog& catalog,const std::vector<StableId>& requested,Integer tick,const StableId& before_id,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* population_catalog){return rosters_at(common,military,catalog,requested,tick,before_id,population,population_catalog,false,common.identity.tick);}
// 当前投影只从正式初态与真实永久事件推导，不把现有fit/wounded作为来源。
Outcome<std::vector<MilitaryUnitRecord>> military_rosters_current(const CommonStore& common,const MilitaryState& military,const MilitaryCatalog& catalog,const std::vector<StableId>& requested,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* population_catalog){return rosters_at(common.state(),military,catalog,requested,common.tick(),{},population,population_catalog,true,common.tick());}
// phase必须仍是同一未提交候选，允许的历史终点只比Common时钟多一个半日。
Outcome<std::vector<MilitaryUnitRecord>> military_rosters_before(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,const std::vector<StableId>& units,Integer tick,const StableId& boundary,const DynamicStepInput& phase,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* population_catalog){if(phase.from_tick<0||phase.from_tick!=c.tick()||phase.from_tick==std::numeric_limits<Integer>::max()||phase.to_tick!=phase.from_tick+1)return fail<std::vector<MilitaryUnitRecord>>("MilitaryRosterHistoryPhaseClockMismatch",boundary);return rosters_at(c.state(),m,cat,units,tick,boundary,population,population_catalog,false,phase.to_tick);}
// 活World调用只借同一共同状态，无复制或写回。
Outcome<std::vector<MilitaryUnitRecord>> military_rosters_before(const CommonStore& common,const MilitaryState& military,const MilitaryCatalog& catalog,const std::vector<StableId>& requested,Integer tick,const StableId& before_id,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* population_catalog){return military_rosters_before(common.state(),military,catalog,requested,tick,before_id,population,population_catalog);}

namespace {
// 同人只能由唯一实际伤亡事实宣告死亡；时间上限仅由两种受检入口提供。
Outcome<std::optional<MilitaryActorDeath>> actor_death_at(const CommonStore& common,const MilitaryState& military,const StableId& actor_id,Integer observed_tick){
 if(actor_id.empty()||!common.actor(actor_id))return fail<std::optional<MilitaryActorDeath>>("UnknownActor",actor_id);
 std::optional<MilitaryActorDeath> out;
 for(const auto& fact:military.casualty_facts)for(const auto& change:fact.actor_changes)if(change.actor_id==actor_id&&change.status=="dead"){
  const auto* header=common.state().facts.find(fact.id);
  if(fact.id.empty()||fact.battle_id.empty()||fact.tick<0||fact.tick>observed_tick||!header||header->id!=fact.id||header->kind!="military_casualties"||header->domain!="military")return fail<std::optional<MilitaryActorDeath>>("MilitaryActorDeathSourceMismatch",actor_id);
  if(out)return fail<std::optional<MilitaryActorDeath>>("AmbiguousMilitaryActorDeath",actor_id);
  out=MilitaryActorDeath{actor_id,fact.id,fact.battle_id,fact.tick};
 }
 return {true,std::move(out),{}};
}
}
// 稳定World仍严格止于当前时钟，未来伤亡不能借阶段口自动准入。
Outcome<std::optional<MilitaryActorDeath>> query_military_actor_death(const CommonStore& common,const MilitaryState& military,const StableId& actor_id){return actor_death_at(common,military,actor_id,common.tick());}
// 原候选preclock只观察同一步已实际产生的伤亡，不写死亡账或提前提交时钟。
Outcome<std::optional<MilitaryActorDeath>> query_military_actor_death(const CommonStore& common,const MilitaryState& military,const StableId& actor_id,const DynamicStepInput& step){
 if(step.from_tick<0||step.from_tick!=common.tick()||step.from_tick==std::numeric_limits<Integer>::max()||step.to_tick!=step.from_tick+1)return fail<std::optional<MilitaryActorDeath>>("MilitaryActorDeathPhaseClockMismatch",actor_id);
 return actor_death_at(common,military,actor_id,step.to_tick);
}

static Outcome<MilitaryRosterHistoryRead> read_rosters_exact_impl(const CommonStore& common,const MilitaryState& military,const MilitaryCatalog& catalog,const MilitaryExactState* exact,const std::vector<StableId>& units,const WorldMoment& now,const std::optional<SourceRef>& before,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* population_catalog,ExactArithmeticBudget budget,const WorldMoment* boundary_at,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const ExactTrainingPeriodRead* periods,MilitaryRosterHistoryObserver* observer=nullptr,const CommonActualFactPrefix* actual_prefix=nullptr,const MilitaryActualSourceWindow* window=nullptr,const MilitaryFieldHistoryRead* field=nullptr){
 if(actual_prefix){if(before||boundary_at||observer)return fail<MilitaryRosterHistoryRead>("MilitaryOrdinalRosterModeMismatch");auto cut=read_actual_common_fact_prefix(common,*actual_prefix);if(!cut.ok)return {false,{},cut.error};if(window){auto bound=validate_military_actual_source_window(common,*actual_prefix,now,window,budget);if(!bound.ok)return {false,{},bound.error};}}else if(window)return fail<MilitaryRosterHistoryRead>("MilitaryOrdinalRosterModeMismatch");
 auto encoded=encode_world_moment(now,budget);if(!encoded.ok)return {false,{},encoded.error};
 if(now.half_day_tick<common.tick()||now.half_day_tick-common.tick()>1)return fail<MilitaryRosterHistoryRead>("MilitaryExactHistoryPhaseClockMismatch");
 if(before){const auto* header=common.state().facts.find(before->id);if(!header||header->domain!=before->domain||header->kind!=before->kind)return fail<MilitaryRosterHistoryRead>("MilitaryRosterHistoryBoundaryRequired",before->id);}
 // Exact selection cannot silently ignore an orphan legacy/exact casualty header.
 for(const auto& row:common.state().facts.rows())if(row.second.domain=="military"&&row.second.kind=="military_casualties"){
  const bool legacy=find(military.casualty_facts,row.first)!=nullptr;const bool dynamic=exact&&exact->dynamic_casualty_facts.find(row.first)!=nullptr;
  if(legacy==dynamic)return fail<MilitaryRosterHistoryRead>("MilitaryCasualtyPayloadCoverage",row.first);
 }
 if(boundary_at){auto valid=encode_world_moment(*boundary_at,budget);if(!valid.ok)return {false,{},valid.error};auto order=compare_world_moments(*boundary_at,now,budget);if(!order.ok)return {false,{},order.error};if(!before||order.value>0)return fail<MilitaryRosterHistoryRead>("MilitaryExactHistoryBoundaryMomentMismatch");}
 // All exact result values come from their original retained owner. This
 // proof is source-local and never calls this roster fold recursively.
 std::map<StableId,MilitaryCompletedReturnSource> returned_sources;
 std::map<StableId,WorldMoment> supply_moments;
 if(returns&&!returns->bound_to(common,military))return fail<MilitaryRosterHistoryRead>("CrossCandidateMilitaryReturnSources");
 if(returns)for(const auto& army:military.armies){auto read=returns->read_exact_army_return(army.id);if(!read.ok)return {false,{},read.error};if(!read.value)continue;const auto& source=*read.value;
  const auto* row=source.original;if(!row||row->army_id!=army.id||source.actual_return.source_schema!=2||source.actual_application.source_schema!=2||source.actual_return.source_ref.domain!="military"||source.actual_return.source_ref.kind!="military_return"||source.actual_return.source_ref.id!=row->id||source.actual_application.source_ref.domain!="military"||source.actual_application.source_ref.kind!="military_logistics_completion_applied"||source.actual_application.source_ref.id!=row->completion_source_ref.id||row->completion_source_ref.domain!="military"||row->completion_source_ref.kind!="military_logistics_completion_applied"||find(military.return_facts,row->id)||!returned_sources.emplace(row->id,source).second)return fail<MilitaryRosterHistoryRead>("MilitaryExactReturnSourceMismatch",army.id);
  const auto at=compare_world_moments(row->completed_at,now,budget);if(!at.ok)return {false,{},at.error};if(at.value>0)return fail<MilitaryRosterHistoryRead>("MilitaryExactHistoryFutureSource",row->id);
 }
 for(const auto& item:common.state().facts.rows())if(item.second.domain=="military"){
  if(item.second.kind=="military_return"){
   const bool legacy=find(military.return_facts,item.first)!=nullptr;const bool actual=returned_sources.count(item.first)!=0;
   if(legacy==actual)return fail<MilitaryRosterHistoryRead>("MilitaryReturnPayloadCoverage",item.first);
  }else if(item.second.kind=="military_logistics_completion_applied"){
   std::size_t matches=0;for(const auto& entry:returned_sources)if(entry.second.actual_application.source_ref.id==item.first)++matches;
   if(matches!=1)return fail<MilitaryRosterHistoryRead>("MilitaryReturnCompletionCoverage",item.first);
  }
 }
 if(exact)for(const auto& item:exact->supply_effect_sources.rows()){
  if(!periods)return fail<MilitaryRosterHistoryRead>("MilitaryRosterSupplyPeriodReadRequired",item.first);
  auto actual=read_exact_military_supply_source(common,military,*exact,*periods,{"military","military_supply",item.first},2,now,budget);if(!actual.ok)return {false,{},actual.error};
  if(item.first!=actual.value.exact_source->original_supply_fact_ref.id||!supply_moments.emplace(item.first,actual.value.exact_source->occurred_at).second)return fail<MilitaryRosterHistoryRead>("MilitaryRosterSupplySourceMismatch",item.first);
 }
 // 已选 Field 且无旧 dynamic 根时，/1 原供粮同样由唯一真实 Opening 证明时刻。
 // 原 reader 核原头/正文/期间/嘴口与 Common 区间；不拿 tick-1 合成新来源。
 if(field&&!exact){
  // 只旁观此原循环及已有参数；历史窗口仍由原 reader 验证，不新查来源。
  typed_supply_observation::Context observed;
  observed.common_identity=&common.state();observed.world_id=common.state().identity.world_id;
  observed.revision=common.state().identity.revision;observed.execution_epoch=common.state().identity.execution_epoch;
  observed.common_tick=common.state().identity.tick;observed.fact_count=common.state().facts.order().size();
  observed.item_count=military.supply_facts.size();observed.original_roots={&military,&catalog,exact,periods,field,population};
  observed.requested_ids=units.data();observed.requested_id_count=units.size();observed.actual_now=&encoded.value;
  if(before)observed.source={before->domain,before->kind,before->id,0};
  if(actual_prefix){observed.has_prefix=true;observed.prefix_count=actual_prefix->common_fact_count;
   if(actual_prefix->last_common_fact){const auto& ref=*actual_prefix->last_common_fact;observed.prefix_anchor={ref.domain,ref.kind,ref.id,0};}}
  typed_supply_observation::Scope observation(typed_supply_observation::Stage::roster_all_supply,observed);
  for(const auto& item:military.supply_facts){
   if(!periods){observation.stop(false);return fail<MilitaryRosterHistoryRead>("MilitaryRosterSupplyPeriodReadRequired",item.id);}
   auto actual=read_field_military_supply_source(common,military,*periods,{"military","military_supply",item.id},1,now,budget);if(!actual.ok){observation.stop(false);return {false,{},actual.error};}
   if(actual.value.original_supply!=&item||!supply_moments.emplace(item.id,actual.value.original_period.opening_at).second){observation.stop(false);return fail<MilitaryRosterHistoryRead>("MilitaryRosterSupplySourceMismatch",item.id);}
  }
  observation.stop();
  // 全部原供粮头必须有唯一原正文；不能仅从当前请求的编队或非空表推断覆盖。
  for(const auto& item:common.state().facts.rows())if(item.second.domain=="military"&&item.second.kind=="military_supply"&&supply_moments.count(item.first)!=1)return fail<MilitaryRosterHistoryRead>("MilitaryRosterSupplySourceMismatch",item.first);
 }
 std::map<StableId,WorldMoment> field_moments;
 if(field)for(const auto& row:military.casualty_facts){auto actual=read_selected_military_field_casualty_moment(common,military,row,field);if(!actual.ok)return {false,{},actual.error};if(actual.value)field_moments.emplace(row.id,std::move(*actual.value));}
 const MilitaryCasualtyHistoryContext casualty_history{common,now,formations,returns,actual_prefix?MilitaryCasualtyHistoryOrder::actual_common_prefix:MilitaryCasualtyHistoryOrder::legacy_serial};
 MilitaryRosterHistoryRead out;auto rows=rosters_at(common.state(),military,catalog,units,boundary_at?boundary_at->half_day_tick:now.half_day_tick,before?before->id:StableId{},population,population_catalog,!before,now.half_day_tick,&out.trace,exact,&now,budget,boundary_at,formations,&returned_sources,&supply_moments,&casualty_history,observer,&common,actual_prefix,window,true,field?&field_moments:nullptr);
 if(!rows.ok)return {false,{},rows.error};out.units=std::move(rows.value);return {true,std::move(out),{}};
}

Status validate_selected_military_field_history(const CommonStore& c,const MilitaryState& m,const CatalogIdentity& identity,const MilitaryFieldHistoryRead* field){
 auto selected=selected_field_battle_version(identity);if(!selected.ok)return {false,{},selected.error};
 if(bool(selected.value)!=bool(field))return common_failure("MilitaryRosterFieldSelectionMismatch");
 if(field&&!field->clock.bound_to(c,m,field->field))return common_failure("CrossCandidateMilitaryFieldHistory");
 return common_success();
}
Outcome<std::optional<WorldMoment>> read_selected_military_field_casualty_moment(const CommonStore& c,const MilitaryState& m,const MilitaryCasualtyFact& row,const MilitaryFieldHistoryRead* field){
 const auto* stored=find(m.casualty_facts,row.id);const auto* parent=c.state().facts.find(row.battle_id);
 if(stored!=&row||!parent||parent->domain!="military")return fail<std::optional<WorldMoment>>("MilitaryCasualtyOriginalParentRequired",row.id);
 // 新截止普通兵回执借真实step的应用M，不降为legacy tick或回调完整几何。
 if(parent->kind=="field_army_exit_step"){
  if(!field||!field->clock.bound_to(c,m,field->field))return fail<std::optional<WorldMoment>>("MilitaryFieldCasualtyClockRequired",row.id);
  auto read=read_field_army_exit_ordinary_casualty_source(c,m,field->field,{"military","military_casualties",row.id},field->clock);
  if(!read.ok)return {false,{},read.error};
  if(read.value.casualties!=&row)return fail<std::optional<WorldMoment>>("MilitaryFieldCasualtyOriginalMismatch",row.id);
  return {true,read.value.step->applied_at,{}};
 }
 if(parent->kind!="field_battle_segment")return {true,std::nullopt,{}};
 if(!field||!field->clock.bound_to(c,m,field->field))return fail<std::optional<WorldMoment>>("MilitaryFieldCasualtyClockRequired",row.id);
 auto memory=CheckpointMemoryCharge(0);
 const SourceRef source{"military","field_battle_segment",row.battle_id};
 auto read=read_field_segment_settlement_source(c,m,field->field,source,&field->clock);if(!read.ok)return {false,{},read.error};
 if(read.value.casualties!=&row||!read.value.settlement)return fail<std::optional<WorldMoment>>("MilitaryFieldCasualtyOriginalMismatch",row.id);
 return {true,read.value.settlement->applied_at,{}};
}

Status validate_selected_military_roster_root(const CommonStore& c,const CatalogIdentity& identity,const MilitaryExactState* exact){
 const auto& metadata=c.state().metadata;
 if(identity.content_id.empty()||identity.content_version.empty()||identity.content_digest.empty()||identity.content_id!=metadata.content_id||identity.content_version!=metadata.content_version||identity.content_digest!=metadata.content_digest)return common_failure("MilitaryRosterCatalogIdentityMismatch");
 auto selected=selected_native_military_state_versions(identity);if(!selected.ok)return Status{false,{},selected.error};
 // Module selection, not empty tables, determines whether the old dynamic
 // owner exists. Actual-prefix ordering remains exact in both branches.
 if(bool(selected.value.dynamic_military)!=bool(exact))return common_failure("MilitaryRosterDynamicSelectionMismatch");
 return common_success();
}

Outcome<MilitaryRosterHistoryRead> read_military_rosters_at_selected_actual_prefix(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& catalog,const CatalogIdentity& identity,const MilitaryExactState* exact,const std::vector<StableId>& units,const CommonActualFactPrefix& prefix,const WorldMoment& now,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* pc,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const ExactTrainingPeriodRead* periods,const MilitaryActualSourceWindow* window,const MilitaryFieldHistoryRead* field){
 auto selected=validate_selected_military_roster_root(c,identity,exact);if(!selected.ok)return {false,{},selected.error};
 auto field_selected=validate_selected_military_field_history(c,m,identity,field);if(!field_selected.ok)return {false,{},field_selected.error};
 return read_rosters_exact_impl(c,m,catalog,exact,units,now,{},population,pc,budget,nullptr,formations,returns,periods,nullptr,&prefix,window,field);
}

Outcome<MilitaryRosterHistoryRead> read_military_rosters_at_actual_prefix(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& catalog,const MilitaryExactState& exact,const std::vector<StableId>& units,const CommonActualFactPrefix& prefix,const WorldMoment& now,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* pc,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const ExactTrainingPeriodRead* periods,const MilitaryActualSourceWindow* window){return read_rosters_exact_impl(c,m,catalog,&exact,units,now,{},population,pc,budget,nullptr,formations,returns,periods,nullptr,&prefix,window);}
Outcome<MilitaryRosterHistoryRead> read_military_rosters_exact(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& catalog,const MilitaryExactState& exact,const std::vector<StableId>& units,const WorldMoment& now,const std::optional<SourceRef>& before,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* pc,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const ExactTrainingPeriodRead* periods){return read_rosters_exact_impl(c,m,catalog,&exact,units,now,before,population,pc,budget,nullptr,formations,returns,periods);}
Outcome<MilitaryRosterHistoryRead> read_military_rosters_before_exact(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& catalog,const MilitaryExactState& exact,const std::vector<StableId>& units,const SourceRef& before,const WorldMoment& at,const WorldMoment& now,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* pc,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const ExactTrainingPeriodRead* periods){return read_rosters_exact_impl(c,m,catalog,&exact,units,now,before,population,pc,budget,&at,formations,returns,periods);}

Status visit_military_roster_original_events_exact(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& catalog,const MilitaryExactState& exact,const std::vector<StableId>& units,const WorldMoment& now,MilitaryRosterHistoryObserver& observer,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* pc,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const ExactTrainingPeriodRead* periods){auto read=read_rosters_exact_impl(c,m,catalog,&exact,units,now,{},population,pc,budget,nullptr,formations,returns,periods,&observer);return read.ok?common_success():Status{false,{},read.error};}

}
