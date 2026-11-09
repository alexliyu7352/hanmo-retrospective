#include "political_office_transition.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
const PoliticsFact& fact(const CommonStore& common,const PoliticsState& state,const StableId& id){const auto* value=state.facts.find(id);const auto* header=common.state().facts.find(id);if(!value||value->id!=id||!header||header->id!=id||header->domain!="politics"||header->kind!=value->kind)fail("PoliticalOfficeOriginalFactRequired",id);return *value;}
void before(const CommonStore& common,const StableId& a,const StableId& b){const auto& order=common.state().facts.order();const auto x=std::find(order.begin(),order.end(),a),y=std::find(order.begin(),order.end(),b);if(x==order.end()||y==order.end()||x>=y)fail("PoliticalOfficeOriginalCauseOrder",b);}
void original(const CommonStore& common,const PoliticsState& state,const PoliticsFact& value){
 if((value.kind!="political_appointment_arrived"&&value.kind!="political_office_dismissed")||value.actor_id.empty()||value.organization_id.empty()||value.office_id.empty()||value.office_epoch<0||value.office_epoch==std::numeric_limits<Integer>::max()||!common.actor(value.actor_id)||!common.actor(value.issuer_actor_id)||value.decision_actor_id.empty()||std::find(value.witness_actor_ids.begin(),value.witness_actor_ids.end(),value.decision_actor_id)==value.witness_actor_ids.end())fail("PoliticalOfficeOriginalFactInvalid",value.id);
 const auto* jurisdiction=state.jurisdictions.find(value.jurisdiction_id);if(!jurisdiction||jurisdiction->office_id!=value.office_id)fail("PoliticalOfficeJurisdictionSourceRequired",value.id);
 for(const auto& cause:value.cause_fact_ids)before(common,cause,value.id);
 if(value.kind=="political_office_dismissed"){
  if(!value.appointment_id.empty()||value.status!=AppointmentStatus::invalidated||value.decision_actor_id!=value.issuer_actor_id)fail("PoliticalOfficeDismissalSourceInvalid",value.id);
  return;
 }
 const auto* appointment=state.appointments.find(value.appointment_id);
 if(!appointment||appointment->id!=value.appointment_id||appointment->actor_id!=value.actor_id||appointment->issuer_actor_id!=value.issuer_actor_id||appointment->organization_id!=value.organization_id||appointment->jurisdiction_id!=value.jurisdiction_id||appointment->target_office_epoch!=value.office_epoch||appointment->status!=AppointmentStatus::arrived||appointment->last_fact_id!=value.id||value.status!=AppointmentStatus::arrived||value.decision_actor_id!=value.actor_id||value.cause_fact_ids.size()!=1)fail("PoliticalOfficeArrivalSourceInvalid",value.id);
 const auto& accepted=fact(common,state,value.cause_fact_ids.front());const auto& offered=fact(common,state,appointment->offered_fact_id);
 if(accepted.kind!="political_appointment_accepted"||accepted.status!=AppointmentStatus::accepted||accepted.appointment_id!=value.appointment_id||accepted.actor_id!=value.actor_id||accepted.decision_actor_id!=value.actor_id||accepted.office_epoch!=value.office_epoch||accepted.organization_id!=value.organization_id||accepted.cause_fact_ids!=std::vector<StableId>{offered.id}||offered.kind!="political_appointment_offered"||offered.status!=AppointmentStatus::offered||offered.appointment_id!=value.appointment_id||offered.actor_id!=value.actor_id||offered.issuer_actor_id!=value.issuer_actor_id||offered.office_epoch!=value.office_epoch||offered.organization_id!=value.organization_id)fail("PoliticalOfficeAcceptedSourceInvalid",value.id);
 before(common,offered.id,accepted.id);before(common,accepted.id,value.id);
}
}
Outcome<VerifiedOfficeTransition> PoliticalOfficeTransitionRead::read_office_transition(const SourceRef& ref,const ReadStamp& stamp)const{try{
 const auto now=take(read_organization_authority_now(common_,&clock_));if(take(compare_world_moments(stamp.span().to,now)))fail("PoliticalOfficeTrustedReadClockRequired",ref.id);
 const auto& value=fact(common_,politics_,ref.id);original(common_,politics_,value);const auto* stored=sources_.find(ref.id);
 if(!catalog_.authority_lifecycle_enabled||!value.occurred_at)fail("PoliticalOfficeExactOriginalSourceRequired",ref.id);
 if(!stored||take(compare_world_moments(*value.occurred_at,stored->effective_moment)))fail("PoliticalOfficeExactOriginalMomentMismatch",ref.id);
 if(ref.domain!="politics"||ref.kind!=value.kind||!stored||!same_source_ref(stored->source,ref)||stored->organization_id!=value.organization_id||stored->office_id!=value.office_id||stored->expected_epoch!=value.office_epoch||stored->effective_moment.half_day_tick!=value.tick||take(compare_world_moments(stored->effective_moment,now))>0)fail("PoliticalOfficeTransitionSourceMismatch",ref.id);
 const bool arrival=value.kind=="political_appointment_arrived";
 if(stored->kind!=(arrival?OfficeTransitionKind::install:OfficeTransitionKind::dismiss)||stored->old_holder!=(arrival?std::optional<StableId>{}:std::optional<StableId>{value.actor_id})||stored->new_holder!=(arrival?std::optional<StableId>{value.actor_id}:std::optional<StableId>{}))fail("PoliticalOfficeTransitionHolderMismatch",ref.id);
 auto term=take(read_office_term_before_fact_at(common_,organization_,catalog_,catalog_.authority_catalog_identity,stored->office_id,stored->effective_moment,value.id,clock_));if(term.organization_id!=stored->organization_id||term.epoch!=stored->expected_epoch||term.holder_actor_id!=stored->old_holder)fail("PoliticalOfficeActualPredecessorMismatch",ref.id);
 return {true,*stored,{}};
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
Outcome<OfficeChangeResult> apply_political_office_transition_after_fact(CommonStore& common,const PoliticsState& politics,OrganizationState& organization,const OrganizationCatalog& catalog,OrderedTable<VerifiedOfficeTransition>& sources,const StableId& id,const OrganizationAuthorityClock& clock,const ReadStamp& stamp){try{
 if(!catalog.authority_lifecycle_enabled)fail("OfficeLifecycleCapabilityRequired",id);const auto now=take(read_organization_authority_now(common,&clock));if(take(compare_world_moments(stamp.span().to,now)))fail("PoliticalOfficeTrustedWriteClockRequired",id);const auto& value=fact(common,politics,id);original(common,politics,value);if(!value.occurred_at||take(compare_world_moments(*value.occurred_at,now)))fail("PoliticalOfficeExactOriginalMomentRequired",id);const SourceRef source{"politics",value.kind,id};PoliticalOfficeTransitionRead reader(common,politics,organization,catalog,sources,clock);
 if(sources.find(id))return consume_office_transition(common,organization,catalog,source,stamp,reader);
 const auto& order=common.state().facts.order();if(order.empty()||order.back()!=id||value.tick!=now.half_day_tick)fail("PoliticalOfficeActualAppendRequired",id);
 const auto* office=organization.offices.find(value.office_id);if(!office||office->organization_id!=value.organization_id||office->epoch!=value.office_epoch)fail("PoliticalOfficeOriginalEpochMismatch",id);
 VerifiedOfficeTransition transition;transition.source=source;transition.organization_id=value.organization_id;transition.office_id=value.office_id;transition.expected_epoch=office->epoch;transition.old_holder=office->holder_actor_id;transition.effective_moment=now;
 if(value.kind=="political_appointment_arrived"){if(office->holder_actor_id||office->status==OfficeStatus::active)fail("PoliticalOfficeNotVacant",office->id);transition.kind=OfficeTransitionKind::install;transition.new_holder=value.actor_id;}
 else {if(office->holder_actor_id!=std::optional<StableId>{value.actor_id}||office->status!=OfficeStatus::active)fail("PoliticalOfficeOriginalHolderMismatch",id);transition.kind=OfficeTransitionKind::dismiss;}
 if(!sources.insert(id,std::move(transition)))fail("DuplicateOfficeTransitionSource",id);
 return consume_office_transition(common,organization,catalog,source,stamp,reader);
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
}
