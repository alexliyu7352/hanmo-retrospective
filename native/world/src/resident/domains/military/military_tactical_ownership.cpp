#include "military_tactical_ownership.hpp"
#include "military_tactical_ownership_memory.hpp"
#include "dynamic_effect_source.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {using namespace movement_detail;namespace {
bool same_owner(const MomentOwnerSource& a,const MomentOwnerSource& b){
 return a.source_schema==b.source_schema&&same_source(a.source_ref,b.source_ref);
}
void owner(const MomentOwnerSource& s){if(s.source_schema<=0||!source_valid(s.source_ref))fail("MilitaryTacticalOriginalSourceRequired",s.source_ref.id);}
bool same_owners(const std::vector<MomentOwnerSource>& a,const std::vector<MomentOwnerSource>& b){
 if(a.size()!=b.size())return false;for(std::size_t n=0;n<a.size();++n)if(!same_owner(a[n],b[n]))return false;return true;
}
void source_set(const std::vector<MomentOwnerSource>& values,const StableId& id){
 if(values.empty())fail("MilitaryTacticalOriginalSourceSetRequired",id);
 std::set<std::tuple<StableId,StableId,StableId>> seen;for(const auto& value:values){owner(value);if(!seen.emplace(value.source_ref.domain,value.source_ref.kind,value.source_ref.id).second)fail("MilitaryTacticalDuplicateSource",id);}
}
void ids(const std::vector<StableId>& values,const StableId& id){
 if(values.empty())fail("MilitaryTacticalActualMembersRequired",id);
 StableId previous;for(const auto& value:values){if(value.empty()||value<=previous)fail("MilitaryTacticalMembersCanonicalOrderRequired",id);previous=value;}
}
Integer next_revision(Integer value,const StableId& id){
 if(value<0||value==std::numeric_limits<Integer>::max())fail("MilitaryTacticalRevisionOverflow",id);return value+1;
}
// A local construction owner only. All raw shape accounting stays in K9;
// no table mutation, allocator, payload walker or transferable credit is added.
template<class T>struct TacticalConstruction3 {CheckpointMemoryCharge memory;T value;};
CommonActualFactPrefix current_prefix(const CommonStore& common,CheckpointMemoryCharge& memory){
 const auto current=take(read_current_common_fact_prefix(common));
 (memory).grow(0);
 CommonActualFactPrefix out;out.common_fact_count=current.common_fact_count;
 if(current.last_common_fact)out.last_common_fact=SourceRef{
  checkpoint_fresh_string_copy(current.last_common_fact->domain),
  checkpoint_fresh_string_copy(current.last_common_fact->kind),
  checkpoint_fresh_string_copy(current.last_common_fact->id)};
 return out;
}
MilitaryTacticalFormation3 build_formation(const MilitaryTacticalFormationBuildView3& v,CheckpointMemoryCharge& memory){
 (memory).grow(0);
 return {v.formation_id,v.source_army_ref,v.roster_revision,v.naval_carrier_ref,v.spatial_binding,v.member_unit_refs,
  v.officer_assignment_ref,v.superior_formation_ref,v.mission_terms_ref,v.control,v.active_intent_ref,
  v.last_applied_order_ordinal,v.control_revision,v.command_binding_revision,v.intent_revision,
  {v.birth.original_admission_source,v.birth.created_at,v.birth.actual_before_common_prefix,v.birth.original_source_refs}};
}
MilitaryTacticalMissionRecord3 build_mission(const MilitaryTacticalMissionBuildView3& v,CheckpointMemoryCharge& memory){
 (memory).grow(0);
 return {v.id,v.formation_ref,v.intent_revision,v.adopted_at,v.previous_intent_ref,v.original_direct_request,
  v.actual_before_common_prefix,v.control_binding_source,v.original_solver_definition_ref,v.original_solver_definition_version,
  v.original_scope_sources,v.original_executions,v.accepted_intents,v.actual_command_source};
}
MilitaryAcceptedIntent3 build_accepted(const MilitaryAcceptedDirectIntentBuildView3& v,CheckpointMemoryCharge& memory){
 (memory).grow(0);
 MilitaryAcceptedIntent3 out{v.id,v.session_ref,v.generation,v.formation_ref,v.mission_ref,v.unit_ref,v.accepted_at,
  v.action_instance_ordinal,v.original_order,v.timing_definition_ref,v.timing_definition_version,
  v.control_binding_source,v.mission_scope_source,
  MilitaryPlayerDirectIntent3{v.adoption.controller_actor_id,v.adoption.control_epoch,v.adoption.actual_command_source}};
 out.original_order.field_order.clear();return out;
}
void append_tactical_head(CommonStore& common,const StableId& id,std::string_view kind){
 auto memory=CheckpointMemoryCharge(0);
 take(common.append_fact({id,StableId(kind),"military"}));
}
bool same_current_prefix(const CommonStore& common,const CommonActualFactPrefix& accepted){
 const auto current=take(read_current_common_fact_prefix(common));
 if(current.common_fact_count!=accepted.common_fact_count)return false;
 return take(read_actual_common_fact_prefix(common,accepted))==std::size_t(current.common_fact_count);
}
std::size_t head(const CommonStore& common,const MomentOwnerSource& source){
 owner(source);return take(read_dynamic_military_fact_position(common,source.source_ref));
}
void actual_prefix_before(const CommonStore& common,const CommonActualFactPrefix& prefix,const MomentOwnerSource& source){
 if(take(read_actual_common_fact_prefix(common,prefix))!=head(common,source))fail("MilitaryTacticalActualAcceptanceCutMismatch",source.source_ref.id);
}
bool same_spatial(const MilitaryFormationSpatial3& a,const MilitaryFormationSpatial3& b){
 if(a.index()!=b.index())return false;
 return std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;const auto& y=std::get<T>(b);
  if constexpr(std::is_same_v<T,MilitaryFormationWorld3>)return x.s1_force_location_ref==y.s1_force_location_ref&&x.binding_revision==y.binding_revision;
  else return x.session_id==y.session_id&&x.session_generation==y.session_generation&&same_source(x.entry_source_ref,y.entry_source_ref)&&x.binding_revision==y.binding_revision;
 },a);
}
bool same_control(const MilitaryFormationControl3& a,const MilitaryFormationControl3& b){
 if(a.index()!=b.index())return false;
 if(const auto* x=std::get_if<MilitaryFormationPlayer3>(&a)){const auto& y=std::get<MilitaryFormationPlayer3>(b);return x->controller_actor_id==y.controller_actor_id&&x->control_epoch==y.control_epoch;}
 const auto& x=std::get<MilitaryFormationAutonomous3>(a);const auto& y=std::get<MilitaryFormationAutonomous3>(b);
 if(x.active_mission_ref!=y.active_mission_ref||x.origin.index()!=y.origin.index())return false;
 return std::visit([&](const auto& p){using T=std::decay_t<decltype(p)>;const auto& q=std::get<T>(y.origin);
  if constexpr(std::is_same_v<T,MilitaryFormationPrebattle3>)return same_owner(p.assignment_ref,q.assignment_ref);
  else return same_owner(p.dispatch_fact_ref,q.dispatch_fact_ref);
 },x.origin);
}
void mission_shape(const MilitaryTacticalMission3& mission){
 std::visit([&](const auto& value){using T=std::decay_t<decltype(value)>;
  if constexpr(std::is_same_v<T,MilitaryMoveTo3>||std::is_same_v<T,MilitaryHoldPosition3>){
   std::visit([&](const auto& destination){using D=std::decay_t<decltype(destination)>;
    if constexpr(std::is_same_v<D,MilitaryBattlefieldDestination3>){
     if(destination.arena_ref.empty()||destination.geometry_profile_ref.empty()||destination.heading<0||destination.heading>5)
      fail("MilitaryTacticalRegisteredDestinationRequired");
    }else if(destination.registered_s1_destination_ref.empty())fail("MilitaryTacticalOriginalWorldDestinationRequired");
   },value.destination);
  }else if constexpr(std::is_same_v<T,MilitaryEngageKnownForce3>){if(value.known_target_ref.empty())fail("MilitaryTacticalKnownTargetRequired");}
  else if constexpr(std::is_same_v<T,MilitarySupportFormation3>){if(value.allied_formation_ref.empty())fail("MilitaryTacticalSupportFormationRequired");}
  else if constexpr(std::is_same_v<T,MilitaryWithdrawToward3>){if(value.approved_exit_ref.empty())fail("MilitaryTacticalApprovedExitRequired");}
  else if(value.known_emergency_ref.empty()||value.rescue_plan_ref.empty())fail("MilitaryTacticalKnownRescuePlanRequired");
 },mission);
}
void bindings(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,const MilitaryTacticalOwnershipContext3& c){
 if(!c.originals.bound_to(common,military,exact)||!c.army_formations.bound_to(common.state(),military))fail("MilitaryTacticalOriginalOwnersNotBound");
}
const MilitaryTacticalFormation3& formation(const MilitaryExactState& exact,const StableId& id){
 const auto* row=exact.formations.find(id);if(!row||row->formation_id!=id)fail("MilitaryTacticalFormationRequired",id);return *row;
}
const MilitaryTacticalMissionRecord3& mission(const MilitaryExactState& exact,const StableId& id){
 const auto* row=exact.missions.find(id);if(!row||row->id!=id)fail("MilitaryTacticalMissionRequired",id);return *row;
}
const MilitaryTacticalMissionRecord3& containing(const MilitaryExactState& exact,const MilitaryAcceptedIntent3& accepted){
 const auto& row=mission(exact,accepted.mission_ref);std::size_t count=0;
 for(const auto& value:row.accepted_intents)if(value.id==accepted.id){if(&value!=&accepted)fail("MilitaryTacticalOriginalBorrowRequired",accepted.id);++count;}
 if(count!=1)fail("MilitaryTacticalOriginalMissionMembershipRequired",accepted.id);return row;
}
void formation_birth(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,
 const MilitaryTacticalOwnershipContext3& c,const MilitaryTacticalFormation3& row){
 bindings(common,military,exact,c);
 if(row.formation_id.empty()||row.source_army_ref.empty()||row.roster_revision<=0||row.control_revision<=0||
    row.command_binding_revision<=0||row.intent_revision<0||row.last_applied_order_ordinal<0||row.mission_terms_ref.empty())
  fail("MilitaryTacticalFormationIdentityRequired",row.formation_id);
 ids(row.member_unit_refs,row.formation_id);source_set(row.birth.original_source_refs,row.formation_id);owner(row.birth.original_admission_source);
 const auto own=military_tactical_formation_source3(row.formation_id);
 if(same_source(row.birth.original_admission_source.source_ref,own.source_ref))
  fail("MilitaryTacticalFormationCannotAuthorizeItself",row.formation_id);
 for(const auto& source:row.birth.original_source_refs)if(same_source(source.source_ref,own.source_ref))
  fail("MilitaryTacticalFormationCannotAuthorizeItself",row.formation_id);
 actual_prefix_before(common,row.birth.actual_before_common_prefix,own);
 const auto now=take(c.clock.read_current_moment());take(encode_world_moment(row.birth.created_at,c.budget));
 if(take(compare_world_moments(row.birth.created_at,now,c.budget))>0)fail("MilitaryTacticalFormationInFuture",row.formation_id);
 const auto original=take(c.originals.read_actual_formation_admission(row.birth.original_admission_source));
 if(original.source_army_ref!=row.source_army_ref||original.roster_revision!=row.roster_revision||original.naval_carrier_ref!=row.naval_carrier_ref||
    !same_spatial(original.spatial_binding,row.spatial_binding)||original.member_unit_refs!=row.member_unit_refs||
    original.officer_assignment_ref!=row.officer_assignment_ref||original.superior_formation_ref!=row.superior_formation_ref||
    original.mission_terms_ref!=row.mission_terms_ref||!same_control(original.control,row.control)||
    original.control_revision!=row.control_revision||original.command_binding_revision!=row.command_binding_revision||
    !same_moment(original.admitted_at,row.birth.created_at,c.budget)||!same_owners(original.original_source_refs,row.birth.original_source_refs))
  fail("MilitaryTacticalOriginalAdmissionMismatch",row.formation_id);
 const ArmyRecord* army=nullptr;for(const auto& candidate:military.armies)if(candidate.id==row.source_army_ref){if(army)fail("MilitaryTacticalArmyIdentityReused",row.source_army_ref);army=&candidate;}
 if(!army)fail("MilitaryTacticalOriginalArmyRequired",row.source_army_ref);
 const auto army_source=take(read_original_military_formation(common.state(),military,*army,now,&c.army_formations,c.budget));
 if(!army_source.original_terms||take(compare_world_moments(army_source.completed_at,row.birth.created_at,c.budget))>0)
  fail("MilitaryTacticalArmyNotActuallyFormed",row.formation_id);
 std::visit([&](const auto& spatial){using T=std::decay_t<decltype(spatial)>;if(spatial.binding_revision<=0)fail("MilitaryTacticalSpatialRevisionRequired",row.formation_id);
  if constexpr(std::is_same_v<T,MilitaryFormationWorld3>){if(spatial.s1_force_location_ref.empty())fail("MilitaryTacticalWorldLocationSourceRequired",row.formation_id);}
  else if(spatial.session_id.empty()||spatial.session_generation<=0||!source_valid(spatial.entry_source_ref))fail("MilitaryTacticalActualBattleBindingRequired",row.formation_id);
 },row.spatial_binding);
 if(const auto* player=std::get_if<MilitaryFormationPlayer3>(&row.control)){
  if(player->controller_actor_id.empty()||player->control_epoch<0)fail("MilitaryTacticalPlayerBindingRequired",row.formation_id);
 }else{
  const auto& autonomous=std::get<MilitaryFormationAutonomous3>(row.control);
  if(autonomous.active_mission_ref.empty())fail("MilitaryTacticalAutonomousMissionRequired",row.formation_id);
  std::visit([](const auto& origin){using T=std::decay_t<decltype(origin)>;if constexpr(std::is_same_v<T,MilitaryFormationPrebattle3>)owner(origin.assignment_ref);else owner(origin.dispatch_fact_ref);},autonomous.origin);
 }
 take(c.originals.validate_formation_admission_before(row));
}
DirectForceIntentReceipt3 receipt(const MilitaryTacticalMissionRecord3& row,CheckpointMemoryCharge& memory){
 // This final-count source buffer is a separately charged constructor input,
 // never charged retroactively by the view that borrows it.
 TacticalConstruction3<std::vector<SourceRef>> refs;
 if(row.accepted_intents.size()>SIZE_MAX/sizeof(SourceRef))throw std::bad_alloc();


 (refs.memory).grow(0);refs.value.reserve(row.accepted_intents.size());
 for(const auto& accepted:row.accepted_intents)refs.value.push_back(military_accepted_intent_source3(accepted.id).source_ref);
 const DirectForceIntentReceiptBuildView3 view{row.formation_ref,row.id,row.original_direct_request.expected_intent_revision,
  row.intent_revision,row.adopted_at,refs.value};
 (memory).grow(0);
 return {view.formation_id,view.mission_ref,view.previous_intent_revision,view.intent_revision,view.accepted_at,view.accepted_intent_refs};
}
void validate_seal_receipt(const MilitaryTacticalOwnershipContext3& context,const CommonCommandSourceView& command,
 const MilitaryTacticalMissionRecord3& row){
 TacticalConstruction3<DirectForceIntentReceipt3> expected;expected.value=receipt(row,expected.memory);
 take(context.originals.validate_direct_command_seal(command,row.original_direct_request,expected.value));
}
bool same_receipt(const DirectForceIntentReceipt3& a,const DirectForceIntentReceipt3& b,ExactArithmeticBudget budget){
 if(a.formation_id!=b.formation_id||a.mission_ref!=b.mission_ref||a.previous_intent_revision!=b.previous_intent_revision||
    a.intent_revision!=b.intent_revision||!same_moment(a.accepted_at,b.accepted_at,budget)||a.accepted_intent_refs.size()!=b.accepted_intent_refs.size())return false;
 for(std::size_t n=0;n<a.accepted_intent_refs.size();++n)if(!same_source(a.accepted_intent_refs[n],b.accepted_intent_refs[n]))return false;return true;
}
void stamp_for_formation(const DirectForceIntent3& request,const MilitaryTacticalFormation3& f,const StableId& actor,ExactArithmeticBudget budget){
 const auto& s=request.stamp;const auto* player=std::get_if<MilitaryFormationPlayer3>(&f.control);
 if(!player||player->controller_actor_id!=actor||player->control_epoch!=s.player_control_epoch||
    request.formation_id!=f.formation_id||s.formation_id!=f.formation_id||s.control_revision!=f.control_revision||
    request.expected_intent_revision<0)fail("MilitaryDirectPlayerControlRequired",f.formation_id);
 const Integer spatial_revision=std::visit([](const auto& spatial){return spatial.binding_revision;},f.spatial_binding);
 if(s.spatial_binding_revision!=spatial_revision)fail("MilitaryDirectSpatialRevisionMismatch",f.formation_id);
 if(const auto* battle=std::get_if<MilitaryFormationInBattle3>(&f.spatial_binding)){
  const auto* stamped=std::get_if<MilitaryForceBattleContext3>(&s.context);
  if(!stamped||stamped->session_ref!=battle->session_id||stamped->generation!=battle->session_generation)fail("MilitaryDirectBattleStampMismatch",f.formation_id);
 }else if(!std::holds_alternative<MilitaryForceWorldContext3>(s.context))fail("MilitaryDirectWorldStampRequired",f.formation_id);
 take(encode_world_moment(s.moment,budget));mission_shape(request.intent);
}
void validate_current_direct_request(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,
 const MilitaryTacticalOwnershipContext3& c,const StableId& actor,const DirectForceIntent3& request){
 bindings(common,military,exact,c);const auto& f=formation(exact,request.formation_id);formation_birth(common,military,exact,c,f);
 const auto& actual=common.state().identity;const auto& stamped=request.stamp.world;
 if(actual.world_id!=stamped.world_id||actual.execution_epoch!=stamped.execution_epoch||actual.revision!=stamped.revision||actual.tick!=stamped.tick||
    !same_moment(request.stamp.moment,take(c.clock.read_current_moment()),c.budget)||f.intent_revision!=request.expected_intent_revision)
  fail("MilitaryDirectStaleStamp",request.formation_id);
 stamp_for_formation(request,f,actor,c.budget);next_revision(f.intent_revision,f.formation_id);
 take(c.originals.validate_direct_permission_now(actor,request,f));
}
void mission_source(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,
 const MilitaryTacticalOwnershipContext3& c,const MilitaryTacticalMissionRecord3& row,bool require_seal){
 const auto& f=formation(exact,row.formation_ref);formation_birth(common,military,exact,c,f);
 if(row.id.empty()||row.intent_revision<=0||row.original_solver_definition_ref.empty()||row.original_solver_definition_version<=0||
    row.intent_revision!=next_revision(row.original_direct_request.expected_intent_revision,row.id)||
    !same_moment(row.adopted_at,row.original_direct_request.stamp.moment,c.budget))
  fail("MilitaryTacticalMissionIdentityRequired",row.id);
 const auto own=military_tactical_mission_source3(row.id);actual_prefix_before(common,row.actual_before_common_prefix,own);
 if(!same_owner(row.control_binding_source,military_tactical_formation_source3(f.formation_id))||
    head(common,row.control_binding_source)>=head(common,own)||take(compare_world_moments(f.birth.created_at,row.adopted_at,c.budget))>0||
    take(compare_world_moments(row.adopted_at,take(c.clock.read_current_moment()),c.budget))>0)
  fail("MilitaryTacticalOriginalControlCutRequired",row.id);
 const auto* player=std::get_if<MilitaryFormationPlayer3>(&f.control);if(!player)fail("MilitaryDirectPlayerControlRequired",f.formation_id);
 stamp_for_formation(row.original_direct_request,f,player->controller_actor_id,c.budget);
 source_set(row.original_scope_sources,row.id);
 for(const auto& source:row.original_scope_sources)if(same_source(source.source_ref,own.source_ref))
  fail("MilitaryTacticalMissionCannotAuthorizeItself",row.id);
 std::set<StableId> units,accepted_ids;
 const auto* battle=std::get_if<MilitaryFormationInBattle3>(&f.spatial_binding);
 for(const auto& accepted:row.accepted_intents){
  const auto* direct=std::get_if<MilitaryPlayerDirectIntent3>(&accepted.adoption);
  if(!battle||accepted.mission_ref!=row.id||accepted.formation_ref!=f.formation_id||accepted.session_ref!=battle->session_id||
     accepted.generation!=battle->session_generation||!same_moment(accepted.accepted_at,row.adopted_at,c.budget)||
     !same_owner(accepted.control_binding_source,row.control_binding_source)||!same_owner(accepted.mission_scope_source,own)||
     !direct||direct->controller_actor_id!=player->controller_actor_id||direct->control_epoch!=player->control_epoch||
     !accepted_ids.insert(accepted.id).second||!units.insert(accepted.unit_ref).second||
     head(common,military_accepted_intent_source3(accepted.id))<=head(common,own))
   fail("MilitaryTacticalDirectAcceptedBindingMismatch",row.id);
  if(require_seal&&(!row.actual_command_source||!same_owner(direct->actual_command_source,*row.actual_command_source)))
   fail("MilitaryTacticalAcceptedCommandSealMismatch",accepted.id);
 }
 for(const auto& execution:row.original_executions){owner(execution.execution_source);if(!units.insert(execution.unit_ref).second||
    same_source(execution.execution_source.source_ref,own.source_ref)||execution.execution_source.source_ref.kind=="tactical_intent_accepted")
   fail("MilitaryTacticalOriginalExecutionBindingRequired",row.id);}
 if(std::vector<StableId>(units.begin(),units.end())!=f.member_unit_refs)fail("MilitaryTacticalMissionMemberCoverageMismatch",row.id);
 take(c.originals.validate_original_mission_scope(row,f));
 take(c.originals.validate_original_mission_execution(row,f));
 if(require_seal){
  if(!row.actual_command_source||row.actual_command_source->source_schema!=1||
     row.actual_command_source->source_ref.domain!="common"||row.actual_command_source->source_ref.kind!="command_result")
   fail("MilitaryDirectActualCommandSealRequired",row.id);
  const auto command=take(read_common_command_source(common,row.actual_command_source->source_ref));
  if(command.result->kind!="direct_force_intent"||command.history->kind!="direct_force_intent"||
     command.history->actor_id!=player->controller_actor_id||command.result->tick!=row.adopted_at.half_day_tick||
     command.result->revision!=next_revision(row.original_direct_request.stamp.world.revision,row.id))
   fail("MilitaryDirectOriginalCommandIdentityMismatch",row.id);
  validate_seal_receipt(c,command,row);
 }
}
Integer next_action_ordinal(const MilitaryExactState& exact,const StableId& session,Integer generation,const StableId& unit){
 Integer ordinal=0;bool any=false;
 for(const auto& entry:exact.missions.rows())for(const auto& accepted:entry.second.accepted_intents)
  if(accepted.session_ref==session&&accepted.generation==generation&&accepted.unit_ref==unit){
   if(accepted.action_instance_ordinal<0)fail("MilitaryTacticalActionOrdinalInvalid",accepted.id);
   ordinal=std::max(ordinal,accepted.action_instance_ordinal);any=true;
  }
 return any?next_revision(ordinal,unit):0;
}
}
MomentOwnerSource military_tactical_formation_source3(const StableId& id){return {{"military","tactical_formation_created",id},3};}
MomentOwnerSource military_tactical_mission_source3(const StableId& id){return {{"military","tactical_mission_adopted",id},3};}
Outcome<bool> same_direct_force_intent_receipt3(const DirectForceIntentReceipt3& a,const DirectForceIntentReceipt3& b,ExactArithmeticBudget budget){
 return guard<bool>([&]{return same_receipt(a,b,budget);});
}
NativeMilitaryTacticalIntentRead3::NativeMilitaryTacticalIntentRead3(const CommonStore& c,const MilitaryState& m,const MilitaryExactState& e,MilitaryTacticalOwnershipContext3 x)
 :common_(c),military_(m),exact_(e),context_(x){}
bool NativeMilitaryTacticalIntentRead3::bound_to(const CommonStore& c,const MilitaryExactState& e)const noexcept{
 return &c==&common_&&&e==&exact_&&context_.originals.bound_to(c,military_,e)&&context_.army_formations.bound_to(c.state(),military_);
}
Outcome<const MilitaryAcceptedIntent3*> NativeMilitaryTacticalIntentRead3::read_original_accepted_intent(const SourceRef& source)const{
 return guard<const MilitaryAcceptedIntent3*>([&]{
  if(source.domain!="military"||source.kind!="tactical_intent_accepted")fail("MilitaryAcceptedIntent3SourceRequired",source.id);
  const MilitaryAcceptedIntent3* found=nullptr;
  for(const auto& entry:exact_.missions.rows())for(const auto& row:entry.second.accepted_intents)if(row.id==source.id){
   if(found)fail("MilitaryAcceptedIntentIdentityReused",source.id);found=&row;
  }
  if(!found)fail("MilitaryAcceptedIntentOriginalPayloadRequired",source.id);return found;
 });
}
Status NativeMilitaryTacticalIntentRead3::visit_original_accepted_intents(MilitaryAcceptedIntentVisitor& visitor)const{
 return status([&]{bindings(common_,military_,exact_,context_);for(const auto& entry:exact_.missions.rows())for(const auto& row:entry.second.accepted_intents)take(visitor.accepted_intent(row));});
}
Status NativeMilitaryTacticalIntentRead3::validate_original_intent_scope(const MilitaryAcceptedIntent3& row)const{
 return status([&]{mission_source(common_,military_,exact_,context_,containing(exact_,row),true);});
}
Status NativeMilitaryTacticalIntentRead3::validate_original_direct_intent_command(const MilitaryAcceptedIntent3& row,const CommonCommandSourceView& command)const{
 return status([&]{const auto& original=containing(exact_,row);
  if(!original.actual_command_source||!command.result||command.result->result_id!=original.actual_command_source->source_ref.id)
   fail("MilitaryDirectExactCommandAssociationRequired",row.id);
  validate_seal_receipt(context_,command,original);
 });
}
Status NativeMilitaryTacticalIntentRead3::validate_original_decision_inputs(const DynamicSessionStateSource2& row)const{return context_.originals.validate_original_decision_inputs(row);}
Status NativeMilitaryTacticalIntentRead3::validate_original_delivered_intent(const MilitaryAcceptedIntent3& row)const{return context_.originals.validate_original_delivered_intent(row);}
Outcome<StableId> adopt_original_tactical_formation_in_candidate(CommonStore& common,const MilitaryState& military,MilitaryExactState& exact,
 const MilitaryTacticalOwnershipContext3& c,const MomentOwnerSource& source){return guard<StableId>([&]{
 bindings(common,military,exact,c);owner(source);
 for(const auto& entry:exact.formations.rows())if(same_owner(entry.second.birth.original_admission_source,source)){
  formation_birth(common,military,exact,c,entry.second);return entry.first;
 }
 const auto now=take(c.clock.read_current_moment());const auto input=take(c.originals.read_actual_formation_admission(source));
 if(!same_moment(input.admitted_at,now,c.budget))fail("MilitaryTacticalActualAdmissionMomentRequired",source.source_ref.id);
 ids(input.member_unit_refs,source.source_ref.id);
 for(const auto& entry:exact.formations.rows())for(const auto& unit:input.member_unit_refs)
  if(std::binary_search(entry.second.member_unit_refs.begin(),entry.second.member_unit_refs.end(),unit))fail("MilitaryTacticalMemberAlreadyOwned",unit);
 const auto id=take(common.next_id("tactical_formation_created"));
 TacticalConstruction3<CommonActualFactPrefix> prefix;prefix.value=current_prefix(common,prefix.memory);
 const Integer zero=0;const std::optional<StableId> no_intent;
 const MilitaryTacticalFormationBirthBuildView3 birth{source,now,prefix.value,input.original_source_refs};
 const MilitaryTacticalFormationBuildView3 view{id,input.source_army_ref,input.roster_revision,input.naval_carrier_ref,
  input.spatial_binding,input.member_unit_refs,input.officer_assignment_ref,input.superior_formation_ref,input.mission_terms_ref,
  input.control,no_intent,zero,input.control_revision,input.command_binding_revision,zero,birth};
 TacticalConstruction3<MilitaryTacticalFormation3> row;row.value=build_formation(view,row.memory);
 append_tactical_head(common,id,"tactical_formation_created");
 // The sole table pre-reserves its independent target copy, including growth.
 if(!exact.formations.insert(id,row.value))fail("MilitaryTacticalFormationIdentityReused",id);
 formation_birth(common,military,exact,c,formation(exact,id));return id;
});}
Outcome<PreparedDirectForceIntent3> prepare_direct_force_intent3(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,
 const MilitaryTacticalOwnershipContext3& c,const StableId& actor,const DirectForceIntent3& request){return guard<PreparedDirectForceIntent3>([&]{
 validate_current_direct_request(common,military,exact,c,actor,request);
 const auto prefix=take(read_current_common_fact_prefix(common));


 PreparedDirectForceIntent3 out;(out.copied_fields_).grow(0);
 out.common_=&common;out.military_=&military;out.exact_=&exact;
 out.actor_=checkpoint_fresh_string_copy(actor);out.request_=DirectForceIntent3(request);
 out.prefix_.common_fact_count=prefix.common_fact_count;
 if(prefix.last_common_fact)out.prefix_.last_common_fact=SourceRef{
  checkpoint_fresh_string_copy(prefix.last_common_fact->domain),
  checkpoint_fresh_string_copy(prefix.last_common_fact->kind),
  checkpoint_fresh_string_copy(prefix.last_common_fact->id)};
 return out;
});}
Outcome<ProducedDirectForceIntent3> apply_direct_force_intent3_in_candidate(CommonStore& common,const MilitaryState& military,MilitaryExactState& exact,
 const MilitaryTacticalOwnershipContext3& c,MilitaryTacticalMissionInstaller3& installer,const PreparedDirectForceIntent3& prepared){
 return guard<ProducedDirectForceIntent3>([&]{
  if(prepared.common_!=&common||prepared.military_!=&military||prepared.exact_!=&exact||!installer.bound_to(common,military,exact)||
     !same_current_prefix(common,prepared.prefix_))fail("MilitaryDirectPreparedCandidateMismatch");
  validate_current_direct_request(common,military,exact,c,prepared.actor_,prepared.request_);
  const auto& f=formation(exact,prepared.request_.formation_id);
  const auto id=take(common.next_id("tactical_mission_adopted"));const auto now=take(c.clock.read_current_moment());
  const Integer revision=next_revision(f.intent_revision,f.formation_id);
  // These borrowed-original fields must outlive the mutating solver callback.
  CheckpointMemoryCharge input_memory;


  (input_memory).grow(0);
  const StableId formation_id=checkpoint_fresh_string_copy(f.formation_id);
  const std::optional<StableId> previous=f.active_intent_ref;
  const auto control_source=military_tactical_formation_source3(formation_id);
  const auto mission_ref=military_tactical_mission_source3(id);
  const auto prefix=current_prefix(common,input_memory);
  append_tactical_head(common,id,"tactical_mission_adopted");
  const auto installation=take(installer.install_direct_mission_in_candidate(id,prepared.request_,f,now,prefix));
  // Any original table mutation invalidates previous row borrows.
  const auto& current=formation(exact,formation_id);
  if(current.intent_revision!=prepared.request_.expected_intent_revision||current.active_intent_ref!=previous)
   fail("MilitaryDirectInstallerChangedFormationOwner",formation_id);
  const auto* battle=std::get_if<MilitaryFormationInBattle3>(&current.spatial_binding);
  TacticalConstruction3<std::vector<MilitaryAcceptedIntent3>> accepted;
  if(installation.actions.size()>SIZE_MAX/sizeof(MilitaryAcceptedIntent3))throw std::bad_alloc();
  (accepted.memory).grow(0);
  accepted.value.reserve(installation.actions.size());CheckpointVector<const StableId*> units;units.reserve(installation.actions.size());
  const MomentOwnerSource not_yet_sealed;
  const MilitaryPlayerDirectIntentBuildView3 direct{prepared.actor_,prepared.request_.stamp.player_control_epoch,not_yet_sealed};
  for(const auto& action:installation.actions){
   if(!battle||action.unit_ref.empty()||action.timing_definition_ref.empty()||action.timing_definition_version<=0||
      std::any_of(units.begin(),units.end(),[&](const auto* prior){return *prior==action.unit_ref;}))
    fail("MilitaryDirectActualSolverActionRequired",id);
   units.push_back(&action.unit_ref);
   const auto accepted_id=take(common.next_id("tactical_intent_accepted"));
   const auto ordinal=next_action_ordinal(exact,battle->session_id,battle->session_generation,action.unit_ref);
   const MilitaryAcceptedDirectIntentBuildView3 view{accepted_id,battle->session_id,battle->session_generation,
    formation_id,id,action.unit_ref,now,ordinal,action.original_order,action.timing_definition_ref,
    action.timing_definition_version,control_source,mission_ref,direct};
   accepted.value.push_back(build_accepted(view,accepted.memory));append_tactical_head(common,accepted_id,"tactical_intent_accepted");
  }
  const std::optional<MomentOwnerSource> no_seal;
  const MilitaryTacticalMissionBuildView3 view{id,formation_id,revision,now,previous,prepared.request_,prefix,control_source,
   installation.solver_definition_ref,installation.solver_definition_version,installation.scope_sources,
   installation.original_executions,accepted.value,no_seal};
  TacticalConstruction3<MilitaryTacticalMissionRecord3> row;row.value=build_mission(view,row.memory);
  ProducedDirectForceIntent3 out;out.receipt_=receipt(row.value,out.copied_fields_);
  if(!exact.missions.insert(id,row.value))fail("MilitaryTacticalMissionIdentityReused",id);
  // Fresh modified owner under its own transient charge. The original table
  // owns the independent replacement copy and destruction of the old row.
  TacticalConstruction3<MilitaryTacticalFormation3> changed;
  const auto& original=formation(exact,formation_id);
  (changed.memory).grow(0);changed.value=MilitaryTacticalFormation3(original);
  changed.value.intent_revision=revision;std::optional<StableId> next{checkpoint_fresh_string_copy(id)};
  changed.value.active_intent_ref.swap(next);
  exact.formations.set(formation_id,changed.value);
  mission_source(common,military,exact,c,mission(exact,id),false);return out;
 });
}
Status bind_direct_force_intent3_command_source_in_candidate(CommonStore& common,const MilitaryState& military,MilitaryExactState& exact,
 const MilitaryTacticalOwnershipContext3& c,const DynamicMilitaryActionContext& actions,const DirectForceIntentReceipt3& result,const MomentOwnerSource& seal){
 return status([&]{
  bindings(common,military,exact,c);owner(seal);
  if(seal.source_schema!=1||seal.source_ref.domain!="common"||seal.source_ref.kind!="command_result")fail("MilitaryDirectActualCommandSealRequired",result.mission_ref);
  const auto* original=exact.missions.find(result.mission_ref);if(!original)fail("MilitaryTacticalMissionRequired",result.mission_ref);
  TacticalConstruction3<DirectForceIntentReceipt3> expected;expected.value=receipt(*original,expected.memory);
  if(!same_receipt(result,expected.value,c.budget))fail("MilitaryDirectOriginalReceiptMismatch",result.mission_ref);
  const auto command=take(read_common_command_source(common,seal.source_ref));
  take(c.originals.validate_direct_command_seal(command,original->original_direct_request,result));
  if(!same_moment(original->adopted_at,take(c.clock.read_current_moment()),c.budget))fail("MilitaryDirectSameCandidateSealRequired",original->id);
  if(original->actual_command_source&&!same_owner(*original->actual_command_source,seal))fail("MilitaryDirectCommandSealRebound",original->id);
  if(!original->actual_command_source){
   TacticalConstruction3<MilitaryTacticalMissionRecord3> changed;

   // Each newly copied seal is its own real target field, not shared credit.
   (void)0;
   for(const auto& accepted:original->accepted_intents){
    const auto* direct=std::get_if<MilitaryPlayerDirectIntent3>(&accepted.adoption);
    if(!direct||direct->actual_command_source.source_schema!=0||
       !direct->actual_command_source.source_ref.domain.empty()||!direct->actual_command_source.source_ref.kind.empty()||
       !direct->actual_command_source.source_ref.id.empty())fail("MilitaryDirectAcceptanceKindMismatch",accepted.id);

   }
   (changed.memory).grow(0);changed.value=MilitaryTacticalMissionRecord3(*original);
   changed.value.actual_command_source=seal;
   for(auto& accepted:changed.value.accepted_intents)std::get<MilitaryPlayerDirectIntent3>(accepted.adoption).actual_command_source=seal;
   exact.missions.set(result.mission_ref,changed.value);
  }
  mission_source(common,military,exact,c,mission(exact,result.mission_ref),true);
  // The caller's owning receipt remains stable across original action writes.
  // Do not retain a mission-row pointer or make an uncharged refs copy.
  for(const auto& accepted:result.accepted_intent_refs)take(start_dynamic_military_action_in_candidate(common,military,exact,actions,accepted));
  take(validate_military_tactical_ownership3(common,military,exact,c));
 });
}
Status validate_military_tactical_ownership3(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,
 const MilitaryTacticalOwnershipContext3& c){return status([&]{
 bindings(common,military,exact,c);std::set<StableId> members;std::set<std::tuple<StableId,StableId,StableId>> admissions;
 for(const auto& entry:exact.formations.rows()){
  const auto& f=entry.second;const auto& admission=f.birth.original_admission_source.source_ref;
  if(entry.first!=f.formation_id||!admissions.emplace(admission.domain,admission.kind,admission.id).second)fail("MilitaryTacticalFormationKeyMismatch",entry.first);
  formation_birth(common,military,exact,c,f);for(const auto& unit:f.member_unit_refs)if(!members.insert(unit).second)fail("MilitaryTacticalMemberInTwoFormations",unit);
  if(f.superior_formation_ref){const auto& superior=formation(exact,*f.superior_formation_ref);if(superior.formation_id==f.formation_id||superior.source_army_ref!=f.source_army_ref)fail("MilitaryTacticalSuperiorScopeMismatch",f.formation_id);
   std::set<StableId> ancestry{f.formation_id};const auto* ancestor=&superior;while(ancestor){if(!ancestry.insert(ancestor->formation_id).second)fail("MilitaryTacticalSuperiorCycle",f.formation_id);ancestor=ancestor->superior_formation_ref?&formation(exact,*ancestor->superior_formation_ref):nullptr;}
  }
  Integer revision=0;std::optional<StableId> previous;std::size_t previous_cut=head(common,military_tactical_formation_source3(f.formation_id));
  std::vector<const MilitaryTacticalMissionRecord3*> chain;for(const auto& row:exact.missions.rows())if(row.second.formation_ref==f.formation_id)chain.push_back(&row.second);
  std::sort(chain.begin(),chain.end(),[&](const auto* a,const auto* b){return head(common,military_tactical_mission_source3(a->id))<head(common,military_tactical_mission_source3(b->id));});
  for(const auto* row:chain){
   const auto cut=head(common,military_tactical_mission_source3(row->id));if(cut<=previous_cut||row->previous_intent_ref!=previous||
      row->original_direct_request.expected_intent_revision!=revision||row->intent_revision!=next_revision(revision,row->id))
    fail("MilitaryTacticalIntentHistoryDiscontinuity",row->id);
   revision=row->intent_revision;previous=row->id;previous_cut=cut;
  }
  if(f.intent_revision!=revision||f.active_intent_ref!=previous)fail("MilitaryTacticalCurrentIntentNotOriginalFold",f.formation_id);
 }
 NativeMilitaryTacticalIntentRead3 read(common,military,exact,c);
 for(const auto& entry:exact.missions.rows()){
  if(entry.first!=entry.second.id)fail("MilitaryTacticalMissionKeyMismatch",entry.first);
  mission_source(common,military,exact,c,entry.second,true);
  for(const auto& accepted:entry.second.accepted_intents){std::size_t count=0;
   for(const auto& action:exact.actions.rows())if(same_source(action.second.accepted_intent_source_ref,military_accepted_intent_source3(accepted.id).source_ref))++count;
   if(count!=1)fail("MilitaryTacticalAcceptedActionCoverageMismatch",accepted.id);
  }
 }
 take(validate_military_accepted_intent_sources3({common,exact,read,c.clock,c.budget}));
 // Reverse original coverage as well: a retained /3 action cannot smuggle an
 // acceptance from a different root/profile or an unowned source. This checks
 // association only; full service/effect validation remains its original owner.
 for(const auto& entry:exact.actions.rows()){
  const auto& action=entry.second;
  const auto* accepted=take(read_verified_military_accepted_intent3(
   {common,exact,read,c.clock,c.budget},{action.accepted_intent_source_ref,3}));
  const auto& x=action.original_order;const auto& y=accepted->original_order;
  if(entry.first!=action.id||action.session_ref!=accepted->session_ref||action.generation!=accepted->generation||
     action.unit_ref!=accepted->unit_ref||action.action_instance_ordinal!=accepted->action_instance_ordinal||
     action.timing_definition_ref!=accepted->timing_definition_ref||
     action.timing_definition_version!=accepted->timing_definition_version||
     !same_moment(action.accepted_at,accepted->accepted_at,c.budget)||
     x.action!=y.action||x.destination!=y.destination||x.heading!=y.heading||
     x.target_id!=y.target_id||x.tactic_id!=y.tactic_id||!x.field_order.empty()||!y.field_order.empty())
   fail("MilitaryTacticalActionOriginalAcceptanceMismatch",entry.first);
 }
 for(const auto& entry:common.state().facts.rows())if(entry.second.domain=="military"){
  if(entry.second.kind=="tactical_formation_created"&&(!exact.formations.find(entry.first)||entry.first!=entry.second.id))fail("MilitaryTacticalFormationPayloadRequired",entry.first);
  if(entry.second.kind=="tactical_mission_adopted"&&(!exact.missions.find(entry.first)||entry.first!=entry.second.id))fail("MilitaryTacticalMissionPayloadRequired",entry.first);
 }
});}
Outcome<WorldMoment> read_military_tactical_ownership_source_moment3(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,
 const MilitaryTacticalOwnershipContext3& c,const MomentOwnerSource& source){return guard<WorldMoment>([&]{
 bindings(common,military,exact,c);if(source.source_schema!=3||source.source_ref.domain!="military")fail("MilitaryTacticalSource3Required",source.source_ref.id);
 if(source.source_ref.kind=="tactical_formation_created"){const auto& row=formation(exact,source.source_ref.id);formation_birth(common,military,exact,c,row);return row.birth.created_at;}
 if(source.source_ref.kind=="tactical_mission_adopted"){const auto& row=mission(exact,source.source_ref.id);mission_source(common,military,exact,c,row,true);return row.adopted_at;}
 if(source.source_ref.kind=="tactical_intent_accepted"){NativeMilitaryTacticalIntentRead3 read(common,military,exact,c);return take(read_verified_military_accepted_intent3({common,exact,read,c.clock,c.budget},source))->accepted_at;}
 fail("MilitaryTacticalSource3Required",source.source_ref.id);
});}
}
