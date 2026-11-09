#include "military_group_movement.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include "military_field_command_source.hpp"
#include "military_supply_exact.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <limits>
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/checkpoint/checkpoint_capacity.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include <algorithm>
#include <cstdio>
#include <set>
namespace hanmo::resident {using namespace movement_detail;namespace {
// 仅测试诊断入口显式挂卸；默认关闭，不从环境开启，不持有任何 World 或源引用。
thread_local bool army_movement_diagnostic_enabled=false;
// 原谓词只计算一次；未走到的短路分支不能伪记为成功或失败。
const char* diagnostic_predicate(unsigned evaluated,unsigned index,bool matches)noexcept{
 return evaluated<index?"NOT_EVALUATED":matches?"MATCH":"MISMATCH";
}
// 同步只借原字段，以 C stdio 输出，不编码分数、不分配领域对象、不重进任何 owner。
void diagnose_army_movement_guard(const CommonStore& common,const MilitaryState& military,
 const MovementExactState& movement,const GroupMovementExecutionState& executions,
 const GroupMovementJourney& journey,const TransportManifestRead& manifest,
 const MomentSpan& span,const WorldMoment& now,unsigned evaluated,
 bool from_matches,bool positive,bool revision_matches,bool carrier_matches)noexcept{
 const auto* army=std::get_if<ArmyCarrierRef>(&journey.carrier);
 const auto* manifest_army=std::get_if<ArmyCarrierRef>(&manifest.carrier);
 std::fprintf(stdout,"FIELD_DIAG ARMY_MOTION_GUARD journey=%s evaluated=%u from_current=%s positive_span=%s composition=%s carrier=%s from_tick=%lld to_tick=%lld actual_tick=%lld exact_fractions=BLOCKED_DIAGNOSTIC_RESOURCE_CONTEXT journey_revision=%lld manifest_revision=%lld journey_army=%s manifest_army=%s carrier_identity_equal=%d common_tick=%lld common_revision=%lld common_epoch=%lld world_id=",
  journey.journey_id.c_str(),evaluated,diagnostic_predicate(evaluated,1,from_matches),
  diagnostic_predicate(evaluated,2,positive),diagnostic_predicate(evaluated,3,revision_matches),
  diagnostic_predicate(evaluated,4,carrier_matches),static_cast<long long>(span.from.half_day_tick),
  static_cast<long long>(span.to.half_day_tick),static_cast<long long>(now.half_day_tick),
  static_cast<long long>(journey.composition_revision),static_cast<long long>(manifest.composition_revision),
  army?army->army_id.c_str():"<not-army>",manifest_army?manifest_army->army_id.c_str():"<not-army>",
  army&&manifest_army&&army->army_id==manifest_army->army_id,
  static_cast<long long>(common.tick()),static_cast<long long>(common.state().identity.revision),
  static_cast<long long>(common.state().identity.execution_epoch));
 for(const auto byte:common.state().identity.world_id)std::fprintf(stdout,"%02x",static_cast<unsigned>(byte));
 std::fputc('\n',stdout);
 // 原清单的实际源和 Common 位置原样记录；这里只查找，不把 ID 数字伪作历史 cut。
 const auto source=[&](const char* owner,const SourceRef& ref){
  std::size_t position=0;bool found=false;
  for(const auto& id:common.state().facts.order()){if(id==ref.id){found=true;break;}++position;}
  const auto* head=common.state().facts.find(ref.id);
  std::fprintf(stdout,"FIELD_DIAG ARMY_MANIFEST_SOURCE owner=%s domain=%s kind=%s id=%s common_position=%zu header_matches=%d\n",
   owner,ref.domain.c_str(),ref.kind.c_str(),ref.id.c_str(),position,
   found&&head&&head->domain==ref.domain&&head->kind==ref.kind);
  if(ref.domain=="military"&&ref.kind=="military_supply")for(const auto& supply:military.supply_facts)if(supply.id==ref.id)
   std::fprintf(stdout,"FIELD_DIAG ARMY_SUPPLY_SOURCE id=%s army=%s period_to_label=%lld required=%lld consumed=%lld shortage=%lld mouths=%zu\n",
    supply.id.c_str(),supply.army_id.c_str(),static_cast<long long>(supply.tick),
    static_cast<long long>(supply.required),static_cast<long long>(supply.consumed),
    static_cast<long long>(supply.shortage),supply.mouth_ids.size());
 };
 for(const auto& ref:manifest.source_refs)source("current_manifest",ref);
 for(const auto& row:executions.openings.rows())if(row.second.journey_id==journey.journey_id){
  const auto& opening=row.second;
  std::fprintf(stdout,"FIELD_DIAG ARMY_OPENING id=%s revision=%lld occurred_tick=%lld original_load=%lld\n",
   opening.id.c_str(),static_cast<long long>(opening.composition_revision),
   static_cast<long long>(opening.occurred_at.half_day_tick),static_cast<long long>(opening.motion_profile.original_profile.load_weight));
  for(const auto& ref:opening.manifest_sources)source("original_opening",ref);
 }
 const auto* prefix=movement.prefixes.find(journey.movement_prefix_ref);
 const auto* terms=prefix?executions.unit_terms.find(prefix->unit_ref):nullptr;
 if(terms)std::fprintf(stdout,"FIELD_DIAG ARMY_UNIT_PROFILE unit=%s original_load=%lld source_domain=%s source_kind=%s source_id=%s source_schema=%lld\n",
  terms->unit_ref.c_str(),static_cast<long long>(terms->motion_profile.original_profile.load_weight),
  terms->motion_profile.actual_rule_source.domain.c_str(),terms->motion_profile.actual_rule_source.kind.c_str(),
  terms->motion_profile.actual_rule_source.id.c_str(),static_cast<long long>(terms->motion_profile.source_schema));
 std::fputs("FIELD_DIAG ARMY_ADJACENT_CHECKS authority=BLOCKED_DIAGNOSTIC_RESOURCE_CONTEXT policy=NO_WRITER_ORIGINAL_GUARD_RETAINED\n",stdout);
 std::fflush(stdout);
}
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool same_ids(const std::vector<StableId>& a,const std::vector<StableId>& b){return a.size()==b.size()&&std::set<StableId>(a.begin(),a.end())==std::set<StableId>(b.begin(),b.end());}
bool same_profile(const GroupMotionProfile& a,const GroupMotionProfile& b){return same_group_motion_profile(a,b);}
// 仅原首错已保存后探查本具体只读链；新 job 的所有值在其 TLS 内消费并销毁。
// 不把此隔离许可推广到 authority/body，不重取读票，也不把次级失败替换原 Status。
void diagnose_army_motion_profile(const NativeArmyGroupMovementSources& sources,
 const CommonStore& common,const MovementExactState& movement,const GroupMovementExecutionState& executions,
 const GroupMovementJourney& journey,const TransportManifestRead& manifest,const WorldMoment& at)noexcept{
 try{
  const auto* prefix=movement.prefixes.find(journey.movement_prefix_ref);
  const auto* terms=prefix?executions.unit_terms.find(prefix->unit_ref):nullptr;
  if(!terms||terms->journey_id!=journey.journey_id){
   std::fputs("FIELD_DIAG ARMY_PROFILE status=BLOCKED_ORIGINAL_UNIT_TERMS\n",stdout);return;
  }
  auto job=CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile());
  if(!job.ok){std::fprintf(stdout,"FIELD_DIAG ARMY_PROFILE status=UNAVAILABLE_JOB code=%s\n",checkpoint_job_failure_code_name(job.error.code()));return;}
  auto resource=checkpoint_memory_resource(job.value,CheckpointJobUse::candidate);
  if(!resource.ok){std::fprintf(stdout,"FIELD_DIAG ARMY_PROFILE status=UNAVAILABLE_RESOURCE code=%s\n",checkpoint_job_failure_code_name(resource.error.code()));return;}
  ScopedCheckpointMemory memory(resource.value);
  // 内层先销毁原 Outcome 再退出 memory；只打印本候选真实 reader 和原等价谓词。
  const auto actual=sources.read_motion_profile(manifest,"original_army_travel/1",at);
  if(!actual.ok){
   std::fprintf(stdout,"FIELD_DIAG ARMY_PROFILE status=READ_REJECTED code=%s path=%s\n",actual.error.original.code.c_str(),actual.error.original.path.c_str());return;
  }
  const auto old=take(read_group_effective_motion_profile(common,executions,*terms));const auto& current=actual.value;
  const bool matches=same_profile(current,old);
  std::fprintf(stdout,"FIELD_DIAG ARMY_PROFILE status=%s original_guard=%s unit=%s old_load=%lld actual_load=%lld source_equal=%d schema_equal=%d mode_equal=%d payer_equal=%d naval_equal=%d additional_cost_equal=%d payee_equal=%d destination_equal=%d\n",
   matches?"MATCH":"MISMATCH",matches?"NONE":"ArmyOriginalMovementProfileChanged",terms->unit_ref.c_str(),
   static_cast<long long>(old.original_profile.load_weight),static_cast<long long>(current.original_profile.load_weight),
   same_source(current.actual_rule_source,old.actual_rule_source),current.source_schema==old.source_schema,
   current.original_profile.mode==old.original_profile.mode,current.original_profile.payer_wallet_id==old.original_profile.payer_wallet_id,
   current.original_profile.naval_budget==old.original_profile.naval_budget,current.additional_cost_per_unit==old.additional_cost_per_unit,
   current.fare_payee_wallet_id==old.fare_payee_wallet_id,current.fare_destination_id==old.fare_destination_id);
 }catch(const checkpoint_job_detail::FailureException& failure){
  std::fprintf(stdout,"FIELD_DIAG ARMY_PROFILE status=UNAVAILABLE_TECHNICAL code=%s\n",checkpoint_job_failure_code_name(failure.error.code()));
 }catch(const std::bad_alloc&){std::fputs("FIELD_DIAG ARMY_PROFILE status=UNAVAILABLE_ALLOCATION\n",stdout);
 }catch(const std::length_error&){std::fputs("FIELD_DIAG ARMY_PROFILE status=UNAVAILABLE_LENGTH\n",stdout);
 }catch(...){std::fputs("FIELD_DIAG ARMY_PROFILE status=UNAVAILABLE_DIAGNOSTIC_EXCEPTION\n",stdout);}
 std::fflush(stdout);
}
class CampaignOpeningAdoption final:public MilitaryGroupOpeningAdoption {
 MilitaryState& military_;const MilitaryCatalog& catalog_;const MilitaryAccess& access_;const CurrentReadStampRead& clock_;const CharacterConditionSources& bodies_;const MilitaryMarchAuthorSelection& author_;StableId actor_;ExactArithmeticBudget budget_;const MilitaryFormationSourceRead* formations_;
public:
 CampaignOpeningAdoption(MilitaryState& m,const MilitaryCatalog& c,const MilitaryAccess& a,const CurrentReadStampRead& clock,const CharacterConditionSources& bodies,const MilitaryMarchAuthorSelection& author,StableId actor,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations):military_(m),catalog_(c),access_(a),clock_(clock),bodies_(bodies),author_(author),actor_(std::move(actor)),budget_(budget),formations_(formations){}
 Status adopt_original_order_in_candidate(CommonStore& common,SpatialCarrierState& carriers,MovementExactState& movement,GroupMovementExecutionState& executions,const StableId& id)override{return adopt_military_campaign_departure_in_candidate(common,military_,catalog_,access_,clock_,bodies_,author_,carriers,movement,executions,id,actor_,budget_,formations_);}
};
}
// 私有测试前置声明只使用此开关；返回旧值供同栈 RAII 恢复，不改变生产默认路径。
bool set_army_movement_diagnostic_enabled(bool enabled)noexcept{
 const bool previous=army_movement_diagnostic_enabled;army_movement_diagnostic_enabled=enabled;return previous;
}
NativeArmyGroupMovementSources::NativeArmyGroupMovementSources(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,const MilitaryExactState& exact,const SpatialCarrierState& carriers,const MovementExactState& movement,const GroupMovementExecutionState& executions,const TravelServices& travel,const CharacterConditionSources& bodies,const CurrentReadStampRead& clock,MilitaryMarchAuthorSelection author,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* population_catalog,const GroupMovementSources* commercial,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const ExactTrainingPeriodRead* periods,const MilitaryFiniteReturnSourceRead* finite_returns):common_(c),military_(m),catalog_(cat),exact_(&exact),selected_{},carriers_(carriers),movement_(movement),executions_(executions),travel_(travel),bodies_(bodies),clock_(clock),author_(std::move(author)),population_(population),population_catalog_(population_catalog),commercial_(commercial),formations_(formations),returns_(returns),periods_(periods),finite_returns_(finite_returns){}
NativeArmyGroupMovementSources::NativeArmyGroupMovementSources(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,MilitarySelectedHistoryRead selected,const SpatialCarrierState& carriers,const MovementExactState& movement,const GroupMovementExecutionState& executions,const TravelServices& travel,const CharacterConditionSources& bodies,const CurrentReadStampRead& clock,MilitaryMarchAuthorSelection author,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* population_catalog,const GroupMovementSources* commercial,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const ExactTrainingPeriodRead* periods,const MilitaryFiniteReturnSourceRead* finite_returns):common_(c),military_(m),catalog_(cat),exact_(selected.exact),selected_(selected),carriers_(carriers),movement_(movement),executions_(executions),travel_(travel),bodies_(bodies),clock_(clock),author_(std::move(author)),population_(population),population_catalog_(population_catalog),commercial_(commercial),formations_(formations),returns_(returns),periods_(periods),finite_returns_(finite_returns){}
Outcome<GroupMotionProfile> NativeArmyGroupMovementSources::read_motion_profile(const TransportManifestRead& manifest,const StableId& route,const WorldMoment& at)const{return guard<GroupMotionProfile>([&]{
 const auto* ref=std::get_if<ArmyCarrierRef>(&manifest.carrier);if(!ref){if(!commercial_)fail("CommercialGroupMovementSourcesRequired");return take(commercial_->read_motion_profile(manifest,route,at));}
 const auto now=take(clock_.read_current_moment());if(route!="original_army_travel/1"||take(compare_world_moments(at,now))>0)fail("ArmyOriginalMovementRuleRequired",ref->army_id);
 const auto actual=(selected_?take(read_selected_military_army_composition(common_,military_,catalog_,*selected_,ref->army_id,now,population_,population_catalog_,{},formations_,returns_,periods_)):take(read_military_army_composition(common_,military_,catalog_,*exact_,ref->army_id,now,population_,population_catalog_,{},formations_,returns_,periods_)));const auto* army=find(military_.armies,ref->army_id);
 if(manifest.composition_revision!=actual.original_sources.composition_revision||!same_ids(manifest.named_actor_refs,actual.named_actor_ids))fail("ArmyOriginalMovementCompositionMismatch",ref->army_id);
 const auto original=take(travel_.active_army(actual.commander_actor_id));if(!original||original->commander_actor_id!=actual.commander_actor_id||original->inventory_id!=actual.inventory_id||original->wallet_id!=actual.wallet_id||!same_ids(original->member_actor_ids,actual.named_actor_ids)||!same_ids(original->fit_actor_ids,actual.fit_member_ids))fail("ArmyOriginalTravelViewMismatch",ref->army_id);
 const auto fleet=take(travel_.vessel_location(actual.commander_actor_id));if(fleet)fail("EmbarkedArmyRequiresFleetMovement",ref->army_id);
 auto profile=take(travel_profile(common_,travel_,actual.commander_actor_id));if(profile.mode!="army"||profile.payer_wallet_id!=actual.wallet_id)fail("ArmyOriginalTravelProfileMismatch",ref->army_id);const auto extra=travel_.party_extra_cost(actual.commander_actor_id);if(extra<0)fail("ArmyOriginalTravelCostMismatch",ref->army_id);
 const auto formation=take(read_original_military_formation(common_.state(),military_,*army,now,formations_));
 // 原 travel_profile 使用同一活名人/fit 并集；此处保真实分子，不能只存取整值。
 __int128 weight=take(common_.inventory_weight(actual.inventory_id));std::set<StableId> people;
 for(const auto& id:actual.named_actor_ids){const auto* actor=common_.actor(id);if(!actor)fail("ArmyOriginalTravelViewMismatch",id);if(actor->alive){people.insert(id);weight+=take(common_.inventory_weight(actor->inventory_id));}}
 people.insert(actual.fit_member_ids.begin(),actual.fit_member_ids.end());
 if(people.empty()||people.size()>std::size_t(std::numeric_limits<Integer>::max())||weight<0||weight>std::numeric_limits<Integer>::max())fail("MilitarySupplyOriginalLoadRequired",ref->army_id);
 const auto total=static_cast<Integer>(weight),count=static_cast<Integer>(people.size());
 if(profile.load_weight!=total/count+(total%count!=0))fail("ArmyOriginalTravelProfileMismatch",ref->army_id);
 return GroupMotionProfile{formation.completed_source.source_ref,formation.completed_source.source_schema,std::move(profile),extra,{},{},total,count};
});}
Status NativeArmyGroupMovementSources::validate_motion_profile(const TransportManifestRead& manifest,const GroupMotionProfile& profile,const WorldMoment& at)const{return status([&]{if(!std::holds_alternative<ArmyCarrierRef>(manifest.carrier)){if(!commercial_)fail("CommercialGroupMovementSourcesRequired");take(commercial_->validate_motion_profile(manifest,profile,at));return;}const auto actual=take(read_motion_profile(manifest,"original_army_travel/1",at));if(!same_profile(actual,profile))fail("ArmyOriginalMovementProfileChanged",std::get<ArmyCarrierRef>(manifest.carrier).army_id);});}
Status NativeArmyGroupMovementSources::validate_start_authority(const StartGroupJourney& request,const TransportUnitState& unit,const TransportManifestRead& manifest)const{if(!commercial_)return common_failure("CommercialGroupMovementSourcesRequired",unit.transport_unit_id,ErrorKind::unsupported_capability);return commercial_->validate_start_authority(request,unit,manifest);}
Status NativeArmyGroupMovementSources::validate_motion_conditions(const GroupMovementJourney& journey,const TransportManifestRead& manifest,const MomentSpan& span)const{auto original=status([&]{
 const auto* army_ref=std::get_if<ArmyCarrierRef>(&journey.carrier);if(!army_ref){if(!commercial_)fail("CommercialGroupMovementSourcesRequired");take(commercial_->validate_motion_conditions(journey,manifest,span));return;}
 const auto now=take(clock_.read_current_moment());
 // 保原四谓词顺序、短路及各自唯一求值；诊断不补算未达分支，也不放行原拒绝。
 unsigned evaluated=1;const bool from_matches=same_moment(span.from,now);
 bool positive=false,revision_matches=false,carrier_matches=false,rejected=!from_matches;
 if(!rejected){evaluated=2;positive=take(compare_world_moments(span.from,span.to))<0;rejected=!positive;}
 if(!rejected){evaluated=3;revision_matches=manifest.composition_revision==journey.composition_revision;rejected=!revision_matches;}
 if(!rejected){evaluated=4;carrier_matches=spatial_carrier_key(manifest.carrier)==spatial_carrier_key(journey.carrier);rejected=!carrier_matches;}
 if(rejected){if(army_movement_diagnostic_enabled)diagnose_army_movement_guard(common_,military_,movement_,executions_,journey,manifest,span,now,evaluated,from_matches,positive,revision_matches,carrier_matches);fail("ArmyActualMovementSpanMismatch",journey.journey_id);}
 const auto* funding=std::get_if<MilitaryMarchExpenseAuthority>(&journey.expense_authority);if(!funding)fail("MilitaryMarchAcceptedFundingRequired",journey.journey_id);take(validate_military_march_expense_authority(common_,military_,catalog_,carriers_,movement_,executions_,bodies_,*funding,journey.journey_id,now,author_,{},formations_,finite_returns_));
 const auto* army=find(military_.armies,army_ref->army_id);const auto* campaign=army?find(military_.campaigns,army->order_id):nullptr;const auto* office=campaign?find(military_.offices,campaign->office_id):nullptr;const auto* accepted=take(read_military_group_order_acceptance(common_,military_,catalog_,carriers_,movement_,executions_,bodies_,funding->accepted_order_source,now,author_,{},formations_,finite_returns_));const auto command=take(query_military_army_command_source_at(common_,military_,army_ref->army_id,clock_,formations_));
 if(!army||!campaign||campaign->status!=(accepted->kind==MilitaryMarchOrderKind::finite_return?"returning":"marching")||
    (accepted->kind==MilitaryMarchOrderKind::campaign_depart&&(!office||!office->enabled||office->holder_actor_id!=accepted->issuing_actor_id))||command.commander_actor_id!=accepted->commander_actor_id||command.command_source.kind!=accepted->command_source.kind||command.command_source.id!=accepted->command_source.id)fail("MilitaryMarchCurrentAuthorityChanged",journey.journey_id);
 if(!take(read_current_character_condition(bodies_,command.commander_actor_id)).body.alive)fail("MilitaryMarchCurrentCommanderUnavailable",journey.journey_id);
});
 // 原业务首错已由原 status 固定；诊断成功、拒绝或资源失败都不改这份原结果。
 if(army_movement_diagnostic_enabled&&!original.ok&&original.error.original.code=="ArmyActualMovementSpanMismatch")
  diagnose_army_motion_profile(*this,common_,movement_,executions_,journey,manifest,span.from);
 return original;
}
Status NativeArmyGroupMovementSources::validate_control_authority(const GroupMovementJourney& journey,const GroupInitiator& initiator,GroupControlOperation operation,const SourceRef& authority)const{return status([&]{
 if(!std::holds_alternative<ArmyCarrierRef>(journey.carrier)){if(!commercial_)fail("CommercialGroupMovementSourcesRequired");take(commercial_->validate_control_authority(journey,initiator,operation,authority));return;}
 const auto* named=std::get_if<ActorGroupInitiator>(&initiator);const auto* funding=std::get_if<MilitaryMarchExpenseAuthority>(&journey.expense_authority);const auto now=take(clock_.read_current_moment());if(!named||!funding||named->authority_ref!=funding->campaign_id||!same_source(authority,funding->accepted_order_source))fail("ArmyControlOriginalAuthorityRequired",journey.journey_id);
 const auto* accepted=take(read_military_group_order_acceptance(common_,military_,catalog_,carriers_,movement_,executions_,bodies_,authority,now,author_,{},formations_,finite_returns_));const auto command=take(query_military_army_command_source_at(common_,military_,funding->army_id,clock_,formations_));const auto* campaign=find(military_.campaigns,funding->campaign_id);const auto* office=campaign?find(military_.offices,campaign->office_id):nullptr;
 if(!campaign||(accepted->kind==MilitaryMarchOrderKind::campaign_depart&&(!office||!office->enabled||office->holder_actor_id!=accepted->issuing_actor_id))||
    (accepted->kind==MilitaryMarchOrderKind::finite_return?named->initiator_actor_id!=command.commander_actor_id:(named->initiator_actor_id!=accepted->issuing_actor_id&&named->initiator_actor_id!=command.commander_actor_id))||!take(read_current_character_condition(bodies_,named->initiator_actor_id)).body.alive)fail("ArmyControlCurrentAuthorityRequired",journey.journey_id);
});}
Status NativeArmyGroupMovementSources::validate_actor_army_binding_source(const StableId& actor_id,const ArmyCarrierRef& carrier,const SourceRef& source)const{return status([&]{
 const auto now=take(clock_.read_current_moment());
 const auto actual=(selected_?take(read_selected_military_army_composition(common_,military_,catalog_,*selected_,carrier.army_id,now,population_,population_catalog_,{},formations_,returns_,periods_)):take(read_military_army_composition(common_,military_,catalog_,*exact_,carrier.army_id,now,population_,population_catalog_,{},formations_,returns_,periods_)));
 if(std::find(actual.named_actor_ids.begin(),actual.named_actor_ids.end(),actor_id)==actual.named_actor_ids.end())fail("ArmySpatialActorNotInOriginalRoster",actor_id);
 const auto* actor=common_.actor(actor_id);const auto* location=actor?std::get_if<InArmyLocation>(&actor->location):nullptr;
 const auto* binding=movement_.spatial_bindings.find("actor:"+actor_id);const auto* carried=binding?std::get_if<CarriedMovementWriter>(&binding->writer):nullptr;
 if(!location||location->army_id!=carrier.army_id||!carried||spatial_carrier_key(carried->carrier_ref)!=spatial_carrier_key(SpatialCarrierRef{carrier})||!same_source(binding->binding_source,source))fail("ArmySpatialOriginalBindingMismatch",actor_id);
 if(source.domain=="military"&&source.kind=="military_action_completion_applied"){
  if(!formations_||!formations_->bound_to(common_.state(),military_))fail("ExactMilitaryFormationSourceReadRequired",source.id);const auto formation=take(formations_->read_exact_army_formation(carrier.army_id));
  if(!formation||!formation->original_terms||!same_source(formation->completed_source.source_ref,source)||formation->completed_source.source_schema!=2||actor_id==formation->original_terms->actor_id||std::find(formation->original_terms->member_actor_ids.begin(),formation->original_terms->member_actor_ids.end(),actor_id)==formation->original_terms->member_actor_ids.end())fail("ArmySpatialFormationBindingMismatch",actor_id);
  take(read_resolved_spatial_writer(movement_,SpatialCarrierRef{carrier}));return;
 }
 // 同目标聚落的实际整军退出仍是Carried Army，但终点是严格ready取消，不伪造零opening。
 if(source.domain=="travel"&&source.kind=="group_movement_control"){
  if(!finite_returns_||!finite_returns_->bound_to(common_,military_,carriers_,movement_,executions_))fail("MilitaryFiniteReturnOriginalSourceRequired",source.id);
  const auto ready=take(finite_returns_->read_actual_finite_return_continuation(source));
  const auto* owned=movement_.spatial_bindings.find(spatial_carrier_key(SpatialCarrierRef{carrier}));
  const auto* stable=owned?std::get_if<StableMovementWriter>(&owned->writer):nullptr;
  if(ready.army_id!=carrier.army_id||!stable||!same_source(owned->binding_source,ready.cause)||
     !take(same_spatial_anchor(stable->anchor,ready.actual_anchor))||
     std::count(ready.named_participants.begin(),ready.named_participants.end(),actor_id)!=1)
    fail("MilitaryFiniteReturnStableAnchorMismatch",actor_id);
  take(read_resolved_spatial_writer(movement_,SpatialCarrierRef{carrier}));return;
 }
 const auto* opening=source.domain=="travel"&&source.kind=="group_movement_opening"?executions_.openings.find(source.id):nullptr;
 const auto* original_carrier=opening?std::get_if<ArmyCarrierRef>(&opening->carrier_origin):nullptr;
 if(!opening||!original_carrier||original_carrier->army_id!=carrier.army_id||std::find(opening->named_participants.begin(),opening->named_participants.end(),actor_id)==opening->named_participants.end())fail("ArmySpatialOriginalOpeningRequired",actor_id);
 const auto* accepted=take(read_military_group_order_acceptance(common_,military_,catalog_,carriers_,movement_,executions_,bodies_,source,now,author_,{},formations_,finite_returns_));
 const auto* journey=carriers_.journeys.find(opening->journey_id);const auto* group=carriers_.bindings.find(spatial_carrier_key(SpatialCarrierRef{carrier}));
 const auto* position=group?carriers_.positions.find(group->position_ref):nullptr;
 if(accepted->army_id!=carrier.army_id||!journey||spatial_carrier_key(journey->carrier)!=spatial_carrier_key(SpatialCarrierRef{carrier})||!position||spatial_carrier_key(position->carrier_ref)!=spatial_carrier_key(SpatialCarrierRef{carrier}))fail("ArmySpatialCarrierSourceMismatch",actor_id);
 const auto* owned=movement_.spatial_bindings.find(spatial_carrier_key(SpatialCarrierRef{carrier}));
 if(!owned||owned->subject_kind!="army"||owned->subject_id!=carrier.army_id)fail("ArmyOriginalSpatialWriterRequired",actor_id);
 if(const auto* world=std::get_if<WorldMovementWriter>(&owned->writer)){
  const auto* prefix=movement_.prefixes.find(world->prefix_id);const auto* execution=prefix?std::get_if<GroupMovementExecution>(&prefix->execution):nullptr;
  const auto* direct=std::get_if<DirectGroupMovement>(&group->execution_owner);const auto* current=execution?carriers_.journeys.find(execution->journey_id):nullptr;
  if(!prefix||std::holds_alternative<MovementClosed>(prefix->lifecycle)||!execution||spatial_carrier_key(execution->carrier)!=spatial_carrier_key(SpatialCarrierRef{carrier})||!current||current->movement_prefix_ref!=prefix->id||current->position_ref!=position->position_id||!direct||direct->group_movement_id!=current->journey_id)fail("ArmyWorldExecutionBindingMismatch",actor_id);
 }
 // S1 resolves one root only; a Battle root remains subject to the original S2 handoff validator.
 take(read_resolved_spatial_writer(movement_,SpatialCarrierRef{carrier}));
});}
static Outcome<StableId> start_campaign_group_impl(CommonStore& common,MilitaryState& military,const MilitaryCatalog& catalog,const MilitaryExactState* exact,const MilitaryAccess& access,const CurrentReadStampRead& clock,const CharacterConditionSources& bodies,const MilitaryMarchAuthorSelection& author,SpatialCarrierState& carriers,MovementExactState& movement,GroupMovementExecutionState& executions,const GroupMovementContext& context,const StableId& campaign_id,const StableId& actor,std::vector<BoundSpatialLocationWrite>& writes,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* population_catalog,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const ExactTrainingPeriodRead* periods,const MilitarySelectedHistoryRead* selected){return guard<StableId>([&]{
 if(selected){take(validate_selected_military_roster_root(common,selected->identity,exact));take(validate_selected_military_field_history(common,military,selected->identity,selected->field));if(selected->exact!=exact)fail("MilitaryMarchSelectedRootMismatch");}else if(!exact)fail("MilitaryMarchDynamicRootRequired");const auto now=take(clock.read_current_moment());for(const auto& row:executions.openings.rows())if(row.second.military_order&&row.second.military_order->kind==MilitaryMarchOrderKind::campaign_depart&&row.second.military_order->campaign_id==campaign_id){const auto* accepted=take(read_military_group_order_acceptance(common,military,catalog,carriers,movement,executions,bodies,{"travel","group_movement_opening",row.first},now,author,budget,formations));if(accepted->issuing_actor_id!=actor)fail("MilitaryMarchReplayIdentityMismatch",campaign_id);return row.second.journey_id;}
 const auto* campaign=find(military.campaigns,campaign_id);const auto* army=campaign?find(military.armies,campaign->army_id):nullptr;const auto* order=campaign?find(catalog.orders,campaign->template_id):nullptr;const auto* target=order?find(catalog.strongholds,order->target_stronghold_id):nullptr;if(!army||!campaign||!target)fail("MilitaryMarchOriginalCampaignRequired",campaign_id);
 CampaignDepart command;command.campaign_id=campaign_id;take(preview_military(common,military,catalog,access,actor,MilitaryCommand{command}));const auto original=(selected?take(read_selected_military_army_composition(common,military,catalog,*selected,army->id,now,population,population_catalog,budget,formations,returns,periods)):take(read_military_army_composition(common,military,catalog,*exact,army->id,now,population,population_catalog,budget,formations,returns,periods)));if(!original.prisoner_ids.empty())fail("MilitaryPrisonerSpatialHandoffRequired",army->id);
 ArmyGroupJourneyOpeningInput request;request.carrier={army->id};request.expected_composition_revision=original.original_sources.composition_revision;request.issuing_actor_id=actor;request.campaign_id=campaign_id;request.organization_id=army->organization_id;request.payer_wallet_id=army->wallet_id;request.destination=target->settlement_id;request.named_march_participants=original.named_actor_ids;request.route_profile="original_army_travel/1";
 CampaignOpeningAdoption adopter(military,catalog,access,clock,bodies,author,actor,budget,formations);return take(start_army_group_journey_in_candidate(common,carriers,movement,executions,context,request,adopter,now,writes,budget));
});}
Outcome<StableId> start_military_campaign_group_journey_in_candidate(CommonStore& common,MilitaryState& military,const MilitaryCatalog& catalog,const MilitaryExactState& exact,const MilitaryAccess& access,const CurrentReadStampRead& clock,const CharacterConditionSources& bodies,const MilitaryMarchAuthorSelection& author,SpatialCarrierState& carriers,MovementExactState& movement,GroupMovementExecutionState& executions,const GroupMovementContext& context,const StableId& campaign_id,const StableId& actor,std::vector<BoundSpatialLocationWrite>& writes,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* population_catalog,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const ExactTrainingPeriodRead* periods){return start_campaign_group_impl(common,military,catalog,&exact,access,clock,bodies,author,carriers,movement,executions,context,campaign_id,actor,writes,population,population_catalog,budget,formations,returns,periods,nullptr);}
Outcome<StableId> start_selected_military_campaign_group_journey_in_candidate(CommonStore& common,MilitaryState& military,const MilitaryCatalog& catalog,const MilitarySelectedHistoryRead& selected,const MilitaryAccess& access,const CurrentReadStampRead& clock,const CharacterConditionSources& bodies,const MilitaryMarchAuthorSelection& author,SpatialCarrierState& carriers,MovementExactState& movement,GroupMovementExecutionState& executions,const GroupMovementContext& context,const StableId& campaign_id,const StableId& actor,std::vector<BoundSpatialLocationWrite>& writes,const PopulationMilitaryState* population,const PopulationMilitaryCatalog* population_catalog,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const ExactTrainingPeriodRead* periods){return start_campaign_group_impl(common,military,catalog,selected.exact,access,clock,bodies,author,carriers,movement,executions,context,campaign_id,actor,writes,population,population_catalog,budget,formations,returns,periods,&selected);}

}

namespace hanmo::resident {
using namespace movement_detail;
// 供养正文仍由军事原期间 reader 证明；再以两个真实 cut 验证这趟旅程的唯一变化。
Status NativeArmyGroupMovementSources::validate_military_supply_rebind(const GroupMilitarySupplyRebindSource& row)const{
 world_stage_observation::observe_node({world_stage_observation::NodeMetric::api_attempt, "military.group.rebind_validation", &common_.state(), nullptr, {"fact", "travel", "group_military_supply_rebind", row.id, 1}, {}, -1});
 return status([&]{
  const SourceRef source{"travel","group_military_supply_rebind",row.id};
  if(take(read_group_military_supply_rebind_source(common_,carriers_,movement_,executions_,source,1))!=&row||!periods_)
   fail("MilitarySupplyRebindOriginalOwnerRequired",row.id);
  const auto now=take(clock_.read_current_moment());const SourceRef supply_ref{"military","military_supply",row.cause_fact_ids.front()};world_stage_observation::observe_node({world_stage_observation::NodeMetric::edge_traversal, "military.group.rebind_supply", &common_.state(), nullptr, {"fact", "travel", "group_military_supply_rebind", row.id, 1}, {"fact", supply_ref.domain, supply_ref.kind, supply_ref.id, 0}, -1});
  const MilitarySupplyFact* supply=nullptr;WorldMoment at;SourceRef actual_opening;
  if(exact_){const auto actual=take(read_exact_military_supply_source(common_,military_,*exact_,*periods_,supply_ref,2,now));supply=actual.original_supply;at=actual.exact_source->occurred_at;actual_opening=actual.original_period.original_opening.source_ref;}
  else{const auto actual=take(read_field_military_supply_source(common_,military_,*periods_,supply_ref,1,now));supply=actual.original_supply;at=actual.original_period.opening_at;actual_opening=actual.original_period.original_opening.source_ref;}
  const auto* journey=carriers_.journeys.find(row.journey_id);const auto* army=journey?std::get_if<ArmyCarrierRef>(&journey->carrier):nullptr;
  if(!army||row.army_id!=army->army_id||supply->army_id!=army->army_id||supply->cause_fact_ids!=std::vector<StableId>{row.journey_id}||!same_moment(at,row.occurred_at))
   fail("MilitarySupplyRebindCauseMismatch",row.id);
  const auto cut=take(read_actual_common_fact_position(common_,supply_ref));
  const auto application_cut=take(read_actual_common_fact_position(common_,source));
  for(std::size_t i=cut+1;i<application_cut;++i){const auto* head=common_.state().facts.find(common_.state().facts.order()[i]);
   if(head&&head->domain==actual_opening.domain&&head->kind==actual_opening.kind)
    fail("MilitarySupplyRebindOutsideOpening",row.id);
  }
  if(take(read_actual_common_fact_position(common_,{"travel","group_journey",row.journey_id}))>=cut)
   fail("MilitarySupplyJourneyCauseCutMismatch",row.id);
  const auto* unit=movement_.units.find(row.unit_ref);
  if(!unit||unit->effort_basis_refs.empty())fail("MilitarySupplyRebindActualUnitRequired",row.id);
  if(const auto* finalized=std::get_if<MovementUnitFinalized>(&unit->status)){
   const auto* close=movement_.unit_closes.find(unit->id+":close");
   if(!close||!same_source(close->actual_cause,finalized->original_close_source))fail("MilitarySupplyOriginalCloseRequired",row.id);
   if(take(read_actual_common_fact_position(common_,{"travel","movement_unit_close",close->id}))<cut&&row.disposition!=GroupSupplyRateDisposition::closed_unit)fail("MilitarySupplyJourneyAlreadyClosed",row.id);
  }
  // 新单位的原 continuation 已重建运行状态；旧单位 pause 不得越过真实 start cut。
  // 取消仍终结整趟责任，不能因后来伪造单位而被这个区间过滤掉。
  const auto* terms=executions_.unit_terms.find(unit->id);
  if(!terms)fail("MilitarySupplyRebindActualTermsRequired",row.id);
  const auto unit_start=take(read_actual_common_fact_position(common_,terms->actual_start_source));
  const GroupMovementControlSource* last_control=nullptr;
  std::size_t last_cut=0;
  for(const auto& pair:executions_.controls.rows())if(pair.second.journey_id==row.journey_id){
   const auto p=take(read_actual_common_fact_position(common_,{"travel","group_movement_control",pair.first}));
   if(p>=cut)continue;
   if(pair.second.operation==GroupControlOperation::cancel)fail("MilitarySupplyJourneyNotRunningAtCause",row.id);
   if(p>unit_start&&(!last_control||p>last_cut)){last_control=&pair.second;last_cut=p;}
  }
  if(last_control&&(last_control->operation==GroupControlOperation::cancel||
     (row.disposition==GroupSupplyRateDisposition::running&&last_control->operation!=GroupControlOperation::resume)||
     (row.disposition==GroupSupplyRateDisposition::held&&last_control->operation!=GroupControlOperation::pause)))
   fail("MilitarySupplyJourneyNotRunningAtCause",row.id);
  const auto before=selected_?take(read_selected_military_army_composition_before(common_,military_,catalog_,*selected_,army->army_id,supply_ref,at,now,population_,population_catalog_,{},formations_,returns_,periods_)):
   take(read_military_army_composition_before(common_,military_,catalog_,*exact_,army->army_id,supply_ref,at,now,population_,population_catalog_,{},formations_,returns_,periods_));
  const auto after=selected_?take(read_selected_military_army_composition_before(common_,military_,catalog_,*selected_,army->army_id,source,at,now,population_,population_catalog_,{},formations_,returns_,periods_)):
   take(read_military_army_composition_before(common_,military_,catalog_,*exact_,army->army_id,source,at,now,population_,population_catalog_,{},formations_,returns_,periods_));
  if(before.original_sources.composition_revision!=row.before_composition_revision||after.original_sources.composition_revision!=row.after_composition_revision||
     before.commander_actor_id!=after.commander_actor_id||before.inventory_id!=after.inventory_id||before.wallet_id!=after.wallet_id||
     before.named_actor_ids!=after.named_actor_ids||before.fit_member_ids!=after.fit_member_ids||before.wounded_member_ids!=after.wounded_member_ids||
     before.unit_ids!=after.unit_ids||before.prisoner_ids!=after.prisoner_ids)
   fail("MilitarySupplyRebindForeignComposition",row.id);
  // 清单中军籍来源逐项来自原 fold；额外位置头必须是该单位真实原投影来源。
  const auto base_sources=[](const MilitaryArmyCompositionRead& actual){std::set<StableId> out;for(const auto& ref:actual.original_sources.authored_unit_sources)out.insert(ref.domain+":"+ref.kind+":"+ref.id);for(const auto& change:actual.original_sources.original_changes){const auto& ref=change.source_ref;out.insert(ref.domain+":"+ref.kind+":"+ref.id);}return out;};
  const auto position_source=[&](const SourceRef& ref){
   if(ref.domain!="travel"||take(read_actual_common_fact_position(common_,ref))>=cut)return false;
   if(ref.kind=="movement_prefix")return ref.id==row.prefix_ref;
   if(row.disposition==GroupSupplyRateDisposition::closed_unit){const auto* close=movement_.unit_closes.find(unit->id+":close");if(close&&same_source(close->actual_cause,ref))return true;}
   if(ref.kind=="movement_contact_continuation"){const auto* v=movement_.contact_continuations.find(ref.id);return v&&v->prefix_id==row.prefix_ref&&v->new_unit_ref==row.unit_ref;}
   if(ref.kind=="movement_boundary_continuation"){const auto* v=movement_.boundary_continuations.find(ref.id);return v&&v->prefix_id==row.prefix_ref&&v->new_unit_ref==row.unit_ref;}
   return false;
  };
  const auto check_sources=[&](const MilitaryArmyCompositionRead& actual,const std::vector<SourceRef>& values){
   auto expected=base_sources(actual);std::set<StableId> found;const SourceRef* position=nullptr;
   for(const auto& ref:values){const auto key=ref.domain+":"+ref.kind+":"+ref.id;if(!found.insert(key).second)fail("MilitarySupplyManifestSourceRepeated",ref.id);if(!expected.erase(key)){if(position||!position_source(ref))fail("MilitarySupplyRebindManifestCoverage",row.id);position=&ref;}}
   if(!expected.empty()||!position)fail("MilitarySupplyRebindManifestCoverage",row.id);return *position;
  };
  if(!same_source(check_sources(before,row.before_manifest_sources),check_sources(after,row.after_manifest_sources)))fail("MilitarySupplyRebindForeignPosition",row.id);
  auto expected=row.before_profile;const auto* item=common_.item_definition(supply->item_id);
  if(!item||item->weight<0||expected.original_load_total<0||expected.original_load_people<1||supply->consumed<0)
   fail("MilitarySupplyOriginalLoadRequired",row.id);
  const __int128 total=static_cast<__int128>(expected.original_load_total)-static_cast<__int128>(item->weight)*supply->consumed;
  if(total<0||total>std::numeric_limits<Integer>::max())fail("MilitarySupplyLoadDeltaMismatch",row.id);
  expected.original_load_total=static_cast<Integer>(total);expected.original_profile.load_weight=expected.original_load_total/expected.original_load_people+(expected.original_load_total%expected.original_load_people!=0);
  if(!same_group_motion_profile(expected,row.after_profile))fail("MilitarySupplyRebindForeignProfile",row.id);
 });
}
} // namespace hanmo::resident

namespace hanmo::resident {
// 全表冷门按真实开程/终止 Common cut 判当下责任，供养与重算一一对应。
Status NativeArmyGroupMovementSources::validate_military_supply_causes()const{
 return movement_detail::status([&]{
  using namespace movement_detail;
  if(!periods_)fail("MilitarySupplyRebindOriginalOwnerRequired");
  const auto now=take(clock_.read_current_moment());
  // 未选 dynamic2 保原 Opening cause；此分支没有新 native rebind producer，不能假装已迁移。
  if(exact_&&!executions_.military_supply_rebinds.empty())fail("MilitarySupplyRebindFieldSourceRequired");
  std::set<StableId> covered;
  for(const auto& supply:military_.supply_facts){
   world_stage_observation::observe_node({world_stage_observation::NodeMetric::loop_candidate_visit, "military.group.supply_causes", &common_.state(), nullptr, {"fact", "military", "military_supply", supply.id, 0}, {}, -1});
   const SourceRef source{"military","military_supply",supply.id};const auto cut=take(read_actual_common_fact_position(common_,source));
   ExactTrainingPeriodView period;
   if(exact_){const auto actual=take(read_exact_military_supply_source(common_,military_,*exact_,*periods_,source,2,now));period=actual.original_period;}
   else period=take(read_field_military_supply_source(common_,military_,*periods_,source,1,now)).original_period;
   if(exact_){if(supply.cause_fact_ids!=std::vector<StableId>{period.original_opening.source_ref.id})fail("MilitarySupplyOpeningCauseMismatch",supply.id);continue;}
   const GroupMovementJourney* responsibility=nullptr;
   if(!supply.army_id.empty())for(const auto& pair:carriers_.journeys.rows()){
    world_stage_observation::observe_node({world_stage_observation::NodeMetric::loop_candidate_visit, "military.group.journey_candidates", &common_.state(), nullptr, {"fact", "travel", "group_journey", pair.first, 1}, {}, world_stage_observation::observed_cut(cut)});
    const auto& journey=pair.second;const auto* army=std::get_if<ArmyCarrierRef>(&journey.carrier);
    if(!army||army->army_id!=supply.army_id)continue;
    const auto begin=take(read_actual_common_fact_position(common_,{"travel","group_journey",journey.journey_id}));if(begin>=cut)continue;
    if(const auto* terminal=carriers_.terminals.find(journey.journey_id))if(take(read_actual_common_fact_position(common_,terminal->actual_terminal_source))<cut)continue;
    if(responsibility)fail("MilitarySupplyMultipleCurrentJourneys",supply.id);responsibility=&journey;
   }
   const auto expected=responsibility?responsibility->journey_id:period.original_opening.source_ref.id;
   if(supply.cause_fact_ids!=std::vector<StableId>{expected})fail("MilitarySupplyActualJourneyCauseMismatch",supply.id);
   const GroupMilitarySupplyRebindSource* application=nullptr;
   for(const auto& row:executions_.military_supply_rebinds.rows())if(row.second.cause_fact_ids==std::vector<StableId>{supply.id}){if(application)fail("MilitarySupplyRebindRepeated",supply.id);application=&row.second;}
   if(bool(application)!=bool(responsibility))fail("MilitarySupplyRebindCoverageRequired",supply.id);
   if(application){if(application->journey_id!=responsibility->journey_id||!covered.insert(application->id).second)fail("MilitarySupplyRebindCauseMismatch",supply.id);take(validate_military_supply_rebind(*application));}
  }
  if(covered.size()!=executions_.military_supply_rebinds.size())fail("MilitarySupplyRebindCoverageRequired");
 });
}
}
