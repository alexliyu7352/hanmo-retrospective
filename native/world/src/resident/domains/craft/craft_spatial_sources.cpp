#include "resident/kernel/common_fact_prefix.hpp"
#include "craft_spatial_sources.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <new>
#include <type_traits>
#include "craft_facility_occupancy.hpp"
#include "craft_character_sources.hpp"
#include "resident/domains/travel/actor_spatial_source_membership.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){auto e=common_failure(code,id).error;e.kind=kind;throw Failure{std::move(e)};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
const CommonStore& common(const CraftSpatialSourceContext&x){return x.craft.materials.common;}
const CommonCatalog& catalog(const CraftSpatialSourceContext&x){return x.craft.materials.common_catalog;}
int compare(const CraftSpatialSourceContext&x,const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b,x.craft.arithmetic_budget));}
bool craft_spatial_input_source_equal(const MomentOwnerSource&a,const SourceRef&ref,Integer schema){return a.source_schema==schema&&same_source_ref(a.source_ref,ref);}
bool source(const MomentOwnerSource&a,const MomentOwnerSource&b){return craft_spatial_input_source_equal(a,b.source_ref,b.source_schema);}
bool craft_spatial_input_source_kind(const SourceRef&ref)noexcept{return is_actor_spatial_head_source_kind(ref);}
WorldMoment context(const CraftSpatialSourceContext&x){
 const auto at=take(x.craft.clock.read_current_moment());
 if(x.craft.spatial_sources!=&x||!x.craft.occupancy||&x.craft.occupancy->craft!=&x.craft||compare(x,at,take(x.sources.current_moment()))||at.half_day_tick<common(x).tick())fail("CraftSpatialContextMismatch");
 if(!x.craft.character_sources||&x.craft.character_sources->body.state!=&x.foundation||&x.craft.character_sources->body.catalog!=&x.actor_catalog||&x.craft.character_sources->body.common!=&common(x))fail("CraftSpatialCharacterContextMismatch");
 return at;
}
void scope(const CraftSpatialSourceContext&x,const CraftJob&j,const WorkServiceIntervalProof&p){
 const auto now=context(x);const auto*h=common(x).state().facts.find(j.id);
 if(x.craft.materials.jobs.jobs.find(j.id)!=&j||j.terms.job_id!=j.id||j.terms.maker.kind!=SubjectKind::actor||!same_source_ref(j.terms.acceptance_ref,craft_job_source(j.id))||!h||h->id!=j.id||h->domain!="crafting"||h->kind!="craft_job"||!same_work_binding(p.execution_binding,j.execution_binding)||compare(x,p.actual_span.from,p.actual_span.to)>=0||compare(x,p.actual_span.from,j.terms.accepted_at)<0||compare(x,p.actual_span.from,now)>0||p.actual_actor_roles.size()!=j.terms.accepted_helpers.size()+1)fail("CraftSpatialActualServiceScopeMismatch",j.id);
 for(std::size_t i=0;i<p.actual_actor_roles.size();++i){const auto&r=p.actual_actor_roles[i];if(r.role.empty()||(r.actor_id!=j.terms.maker.id&&std::find(j.terms.accepted_helpers.begin(),j.terms.accepted_helpers.end(),r.actor_id)==j.terms.accepted_helpers.end()))fail("CraftSpatialParticipantMismatch",j.id);for(std::size_t k=0;k<i;++k)if(r.actor_id==p.actual_actor_roles[k].actor_id)fail("DuplicateCraftSpatialParticipant",r.actor_id);}
}
// 工位地址来自实际claim当时的原容量/安装来源；这里不调用全Craft或全S0重放。
MovementAnchor workstation(const CraftSpatialSourceContext&x,const CraftJob&j,const WorkServiceIntervalProof&p,bool preparing){
 const auto&o=*x.craft.occupancy;if(p.facility_claim_refs.size()!=1)fail("CraftActualWorkstationClaimRequired",j.id);
 const auto&id=p.facility_claim_refs.front();if(std::find(j.facility_claim_refs.begin(),j.facility_claim_refs.end(),id)==j.facility_claim_refs.end())fail("CraftSpatialClaimNotAdopted",id);
 const auto episode=take(read_facility_claim_episode(o.claims,id,context(x),x.craft.arithmetic_budget));const auto&claim=episode.original_claim;const auto receipt=take(read_facility_claim_receipt(o.claims,episode.admission_receipt_ref));
 if(claim.slot_ref!=j.terms.workstation||claim.holder!=j.terms.material_holder||claim.units!=1||compare(x,claim.from,p.actual_span.from)>0||compare(x,claim.until,p.actual_span.to)<0||compare(x,receipt.occurred_at,p.actual_span.from)>0||!same_source_ref(claim.source_result_ref,receipt.source_result_ref))fail("CraftSpatialOriginalClaimMismatch",id);
 if(preparing){if(episode.close_receipt_ref||std::none_of(o.claims.claims.begin(),o.claims.claims.end(),[&](const auto&v){return v.claim_id==id;}))fail("CraftSpatialCurrentClaimRequired",id);}
 else if(!episode.actual_reserved_span||compare(x,episode.actual_reserved_span->from,p.actual_span.from)>0||compare(x,episode.actual_reserved_span->to,p.actual_span.to)<0)fail("CraftSpatialClaimSpanNotCovered",id);
 const EconomicFacilityCapacitySource*capacity=nullptr;for(const auto&row:o.facility_sources.capacity_sources)if(row.slot_ref==claim.slot_ref&&row.source_revision==claim.capacity_revision){if(capacity)fail("DuplicateCraftSpatialCapacity",id);capacity=&row;}
 if(!capacity||capacity->source_schema!=1||compare(x,capacity->occurred_at,receipt.occurred_at)>0)fail("CraftSpatialCapacitySourceRequired",id);
 take(o.physical.read_facility_slot_definition_at_source({{claim.slot_ref.provider_domain,"economic_facility_capacity",capacity->source_id},capacity->source_schema},claim.slot_ref));
 const PhysicalServiceFacilityExtension*facility=nullptr;for(const auto&row:o.facility_sources.facilities)if(row.facility_id==claim.slot_ref.facility_id){if(facility||row.asset_owner_domain!=claim.slot_ref.provider_domain||capacity->facility_extension_ref!=row.facility_id)fail("CraftSpatialFacilityIdentityMismatch",id);facility=&row;}
 if(!facility||facility->place_ref.empty())fail("CraftSpatialFacilityPlaceRequired",id);
 return take(read_original_spatial_site_anchor(catalog(x),x.travel_catalog,{"common_catalog","place",facility->place_ref}));
}
class PreparedCraftSpatialFactPrefix final {
 CheckpointMemoryCharge copied_fields_;
 ActorSpatialActualFactPrefix prefix_;
 static CheckpointMemoryCharge memory(const CurrentCommonFactPrefix& actual){
  return CheckpointMemoryCharge(0);
 }
public:
 explicit PreparedCraftSpatialFactPrefix(const CurrentCommonFactPrefix& actual)
  :copied_fields_(memory(actual)){
  static_assert(std::is_nothrow_move_constructible_v<SourceRef>,"Craft spatial prefix requires nonthrowing payload transfer");
  prefix_.common_fact_count=actual.common_fact_count;
  if(const auto*head=actual.last_common_fact)prefix_.last_common_fact.emplace(SourceRef{head->domain,head->kind,head->id});
 }
 PreparedCraftSpatialFactPrefix(const PreparedCraftSpatialFactPrefix&)=delete;
 PreparedCraftSpatialFactPrefix& operator=(const PreparedCraftSpatialFactPrefix&)=delete;
 PreparedCraftSpatialFactPrefix(PreparedCraftSpatialFactPrefix&&)=delete;
 PreparedCraftSpatialFactPrefix& operator=(PreparedCraftSpatialFactPrefix&&)=delete;
 const ActorSpatialActualFactPrefix& prefix()const noexcept{return prefix_;}
};
PreparedCraftSpatialFactPrefix current_prefix(const CraftSpatialSourceContext&x){
  auto actual = read_current_common_fact_prefix(common(x));
  if (!actual.ok) {
    if (actual.error.original.code == "CommonCurrentFactPrefixOverflow")
      fail("CraftSpatialFactPrefixOverflow");
    if (actual.error.original.code == "CommonCurrentFactPrefixMismatch")
      fail("CraftSpatialFactPrefixMismatch");
    take(std::move(actual));
  }
 return PreparedCraftSpatialFactPrefix(actual.value);
}
ActorSpatialLocationView current_actor(const CraftSpatialSourceContext&x,const StableId&actor,const WorldMoment&at,const PreparedCraftSpatialFactPrefix&prefix){return take(read_actor_spatial_location_at_actual_fact_prefix(common(x),catalog(x),x.movement,x.carriers,x.registrations,x.foundation,x.actor_catalog,x.sources,actor,at,prefix.prefix(),x.military_handoffs,x.craft.arithmetic_budget,x.family_catalog,x.births,x.military_actions));}
void at_workstation(const CraftSpatialSourceContext&x,const MovementAnchor&actual,const MovementAnchor&expected,const StableId&actor){if(!take(same_actual_movement_position(actual,expected,x.craft.arithmetic_budget)))fail("CraftParticipantNotAtActualWorkstation",actor);}
}
CheckpointMemoryCharge CapturedCraftParticipantSpatialInputs::storage_memory(std::size_t capacity){
 return CheckpointMemoryCharge(0);
}
CapturedCraftParticipantSpatialInputs::CapturedCraftParticipantSpatialInputs(std::size_t capacity)
 :copied_fields_(storage_memory(capacity)),capacity_(capacity){sources_.reserve(capacity);}
void CapturedCraftParticipantSpatialInputs::append(const SourceRef& source,Integer schema){
 static_assert(std::is_nothrow_move_constructible_v<MomentOwnerSource>,"Craft spatial input requires nonthrowing payload transfer");
 if(sources_.size()>=capacity_)throw std::bad_alloc();
 (copied_fields_).grow(0);
 sources_.push_back(MomentOwnerSource{source,schema});
}
Outcome<CapturedCraftParticipantSpatialInputs> capture_craft_participant_spatial_inputs(const CraftSpatialSourceContext&x,const CraftJob&j,const WorkServiceIntervalProof&p){try{
 scope(x,j,p);if(j.phase!=CraftJobPhase::working||compare(x,p.actual_span.from,context(x)))fail("CraftSpatialActualFromRequired",j.id);const auto anchor=workstation(x,j,p,true);const auto prefix=current_prefix(x);CapturedCraftParticipantSpatialInputs out(p.actual_actor_roles.size());
 for(const auto&role:p.actual_actor_roles){const auto value=current_actor(x,role.actor_id,p.actual_span.from,prefix);if(value.actor_id!=role.actor_id||value.actual_location_sources.size()!=1)fail("CraftSpatialCanonicalHeadRequired",role.actor_id);at_workstation(x,*take(require_actor_ordinary_movement_anchor(value)),anchor,role.actor_id);const auto&head=value.actual_location_sources.front();if(!craft_spatial_input_source_kind(head.source)||take(actor_spatial_history_source_schema(head.source))!=head.schema)fail("CraftSpatialSourceKindMismatch",head.source.id);if(std::none_of(out.sources().begin(),out.sources().end(),[&](const auto&old){return craft_spatial_input_source_equal(old,head.source,head.schema);}))out.append(head.source,head.schema);}
 return {true,std::move(out),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool is_craft_participant_spatial_source(const MomentOwnerSource&s)noexcept{return craft_spatial_input_source_kind(s.source_ref);}
Status validate_prepared_craft_participant_spatial_source(const CraftSpatialSourceContext&x,const CraftJob&j,const WorkServiceIntervalProof&p,const MomentOwnerSource&s){try{
 scope(x,j,p);if(compare(x,p.actual_span.from,context(x))||!is_craft_participant_spatial_source(s)||take(actor_spatial_history_source_schema(s.source_ref))!=s.source_schema)fail("CraftSpatialPreparedBoundaryMismatch",j.id);
 if(std::count_if(p.participation_sources.begin(),p.participation_sources.end(),[&](const auto&v){return source(v,s);})!=1)fail("CraftSpatialPreparedSourceNotAdopted",s.source_ref.id);
 const auto anchor=workstation(x,j,p,true);const auto prefix=current_prefix(x);bool found=false;
 for(const auto&role:p.actual_actor_roles){const auto value=current_actor(x,role.actor_id,p.actual_span.from,prefix);if(value.actor_id!=role.actor_id||value.actual_location_sources.size()!=1)fail("CraftSpatialCanonicalHeadRequired",role.actor_id);at_workstation(x,*take(require_actor_ordinary_movement_anchor(value)),anchor,role.actor_id);const auto&head=value.actual_location_sources.front();if(head.schema==s.source_schema&&same_source_ref(head.source,s.source_ref))found=true;}
 if(!found)fail("CraftSpatialPreparedHeadChanged",s.source_ref.id);return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_craft_participant_spatial_span(const CraftSpatialSourceContext&x,const CraftJob&j,const WorkServiceIntervalProof&p){try{
 scope(x,j,p);if(compare(x,p.actual_span.to,context(x))>0)fail("CraftSpatialActualToRequired",j.id);const auto anchor=workstation(x,j,p,false);
 const ActorSpatialHeadSourceReadContext original{common(x),catalog(x),x.movement,x.carriers,x.registrations,x.foundation,x.actor_catalog,x.sources,x.family_catalog,x.births,x.craft.arithmetic_budget,x.military_actions};
 std::vector<MomentOwnerSource> pool;
 for(const auto&s:p.participation_sources)if(is_craft_participant_spatial_source(s)){
  if(take(actor_spatial_history_source_schema(s.source_ref))!=s.source_schema||std::count_if(p.participation_sources.begin(),p.participation_sources.end(),[&](const auto&v){return source(v,s);})!=1)fail("CraftSpatialSourceShapeMismatch",s.source_ref.id);pool.push_back(s);
 }
 if(pool.empty())fail("CraftSpatialSourceCoverageRequired",j.id);std::vector<bool> used(pool.size(),false);
 for(const auto&role:p.actual_actor_roles){
  const auto adopted=take(read_actor_spatial_location_from_head_pool(original,role.actor_id,pool,p.actual_span.from,x.military_handoffs));
  if(adopted.actor_id!=role.actor_id||adopted.actual_location_sources.size()!=1)fail("CraftSpatialParticipantHeadMismatch",role.actor_id);const auto&head=adopted.actual_location_sources.front();bool found=false;for(std::size_t i=0;i<pool.size();++i)if(pool[i].source_schema==head.schema&&same_source_ref(pool[i].source_ref,head.source)){used[i]=true;found=true;}if(!found)fail("CraftSpatialAdoptedHeadNotCaptured",role.actor_id);at_workstation(x,*take(require_actor_ordinary_movement_anchor(adopted)),anchor,role.actor_id);
  const auto actual=take(read_actor_stationary_location_span(common(x),catalog(x),x.movement,x.carriers,x.registrations,x.foundation,x.actor_catalog,x.sources,role.actor_id,head.source,p.actual_span,x.military_handoffs,x.craft.arithmetic_budget,x.family_catalog,x.births,x.military_actions));
  if(actual.actor_id!=role.actor_id||compare(x,actual.span.from,p.actual_span.from)||compare(x,actual.span.to,p.actual_span.to))fail("CraftSpatialActualSpanMismatch",role.actor_id);at_workstation(x,actual.actual_anchor,anchor,role.actor_id);
 }
 if(std::find(used.begin(),used.end(),false)!=used.end())fail("CraftSpatialSourceNotAnAdoptedParticipantHead",j.id);
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
