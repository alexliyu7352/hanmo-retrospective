#include "care_facility_use.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/facility_claims/facility_claims.hpp"
#include <algorithm>
#include <charconv>
#include <string_view>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void check(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
void text(const StableId& v){if(v.empty()||v.size()>256)fail("InvalidCareFacilityIdentifier",v);}
void source(const SourceRef& v){text(v.domain);text(v.kind);text(v.id);}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
bool same(const MomentOwnerSource& a,const MomentOwnerSource& b){return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
bool same(const CareFacilityUseMode& a,const CareFacilityUseMode& b){if(a.index()!=b.index())return false;if(const auto* x=std::get_if<CareFacilityOwnClaim>(&a)){const auto& y=std::get<CareFacilityOwnClaim>(b);return x->original_claim_id==y.original_claim_id&&x->holder==y.holder;}const auto& x=std::get<CareFacilityBorrowedPhysicalHolder>(a);const auto& y=std::get<CareFacilityBorrowedPhysicalHolder>(b);return x.placement_claim_id==y.placement_claim_id&&x.physical_holder_source_schema==y.physical_holder_source_schema&&x.authorized_use_source_schema==y.authorized_use_source_schema&&same_source_ref(x.physical_holder_source_ref,y.physical_holder_source_ref)&&same_source_ref(x.authorized_use_source_ref,y.authorized_use_source_ref);}
ReservationRef holder(const StableId& accepted){return {ReservationRef::Kind::domain_activity,accepted,"care_requirements","care_arrangement"};}
bool allocated(const CommonStore& c,const StableId& id){constexpr std::string_view prefix="care_facility_use:";if(id.size()<=prefix.size()||id.compare(0,prefix.size(),prefix)!=0)return false;std::string_view d(id.data()+prefix.size(),id.size()-prefix.size());Integer n=0;auto p=std::from_chars(d.data(),d.data()+d.size(),n);return d.front()!='0'&&p.ec==std::errc{}&&p.ptr==d.data()+d.size()&&n>0&&n<c.state().next_instance_id;}
WorldMoment context(const CareFacilityUseSources& x){const auto& a=x.acceptance;if((a.goals.rules.selected_care_version()!=5&&a.goals.rules.selected_care_version()!=6)||!x.originals.bound_to(a.goals.common,x.claims))fail("CareFacilityOriginalSourcesRequired",{},ErrorKind::unsupported_capability);return take(a.goals.clock.read_current_moment());}
CareFacilityOriginalUse original(const CareFacilityUseSources& x,const CareArrangementRef& owner,const FacilitySlotRef& slot,const MomentOwnerSource& created,const WorldMoment& now){
 source(created.source_ref);if(created.source_schema<1)fail("InvalidCareFacilityCreatedSchema",created.source_ref.id);
 const auto* accepted=take(read_accepted_caregiver(x.acceptance,care_acceptance_source(owner.accepted_arrangement_id)));
 auto actual=take(x.originals.read_original_care_facility_use(owner,slot,created));
 if(actual.accepted!=accepted||actual.slot_ref!=slot||!same(actual.actual_created_source,created)||actual.physical_capacity_identity.empty()||actual.physical_capacity_identity.size()>256||actual.usage_rule_ref.id.empty()||actual.usage_rule_ref.version<1||actual.usage_rule_ref.catalog_digest!=x.acceptance.goals.common.state().metadata.content_digest||take(compare_world_moments(actual.created_at,accepted->accepted_at))<0||take(compare_world_moments(actual.created_at,now))>0)fail("CareFacilityOriginalAdoptionMismatch",created.source_ref.id);
 // The original S0 admission is retained even after close/renewal. Do not
 // infer an initial reservation from today's active-claims vector.
 const auto* own=std::get_if<CareFacilityOwnClaim>(&actual.mode);const auto& claim_id=own?own->original_claim_id:std::get<CareFacilityBorrowedPhysicalHolder>(actual.mode).placement_claim_id;
 auto episode=take(read_facility_claim_episode(x.claims,claim_id,now));const auto& claim=episode.original_claim;
 check(validate_facility_claim_value(claim,x.claim_catalog));auto admission=take(read_facility_claim_receipt(x.claims,episode.admission_receipt_ref));
 if(claim.slot_ref!=slot||take(compare_world_moments(admission.occurred_at,actual.created_at))>0||!same_source_ref(claim.source_result_ref,admission.source_result_ref))fail("CareFacilityOriginalClaimMismatch",claim_id);
 if(own){if(own->holder!=holder(owner.accepted_arrangement_id)||claim.holder!=own->holder)fail("CareFacilityOwnHolderMismatch",claim_id);}
 else if(claim.holder==holder(owner.accepted_arrangement_id))fail("CareFacilityBorrowCannotOwnSameClaim",claim_id);
 return actual;
}
CareFacilityOriginalUse row(const CareFacilityUseSources& x,const CareFacilityUseBinding& v,const WorldMoment& now){
 check(validate_care_facility_use_shape(v,5));if(!allocated(x.acceptance.goals.common,v.binding_id))fail("CareFacilityBindingNotAllocated",v.binding_id);
 auto actual=original(x,v.owner_business_ref,v.slot_ref,{v.created_source_ref,v.created_source_schema},now);
 if(v.accepted_terms_ref!=actual.accepted->accepted_id||!actor_foundation_rule_equal(v.usage_rule_ref,actual.usage_rule_ref)||!same(v.mode,actual.mode)||!same(v.created_at,actual.created_at))fail("CareFacilityBindingOriginalMismatch",v.binding_id);
 return actual;
}
void distinct(const CareFacilityUseBinding& a,const CareFacilityOriginalUse& ar,const CareFacilityUseBinding& b,const CareFacilityOriginalUse& br){if(a.owner_business_ref.accepted_arrangement_id!=b.owner_business_ref.accepted_arrangement_id)return;if(a.slot_ref==b.slot_ref||ar.physical_capacity_identity==br.physical_capacity_identity)fail("DuplicateCareFacilityPhysicalUse",b.binding_id);}
}
SourceRef care_facility_use_source(const StableId& id){return {"care_requirements","care_facility_use_binding",id};}
Outcome<Integer> care_facility_use_source_schema(const SourceRef& r){return r.domain=="care_requirements"&&r.kind=="care_facility_use_binding"&&!r.id.empty()?Outcome<Integer>{true,1,{}}:Outcome<Integer>{false,{},common_failure("UnknownCareFacilityUseSource",r.id,ErrorKind::unsupported_capability).error};}
Status validate_care_facility_use_shape(const CareFacilityUseBinding& v,Integer version){try{
 if(version!=5&&version!=6)fail("CareFacilityHistoryVersion5Required",{},ErrorKind::unsupported_capability);
 text(v.binding_id);text(v.owner_business_ref.accepted_arrangement_id);text(v.accepted_terms_ref);text(v.usage_rule_ref.id);text(v.usage_rule_ref.catalog_digest);text(v.slot_ref.provider_domain);text(v.slot_ref.facility_id);text(v.slot_ref.slot_kind);source(v.created_source_ref);take(encode_world_moment(v.created_at));
 if(v.accepted_terms_ref!=v.owner_business_ref.accepted_arrangement_id||v.usage_rule_ref.version<1||v.created_source_schema<1||v.slot_ref.slot_kind!="bed_space")fail("InvalidCareFacilityBinding",v.binding_id);
 if(const auto* own=std::get_if<CareFacilityOwnClaim>(&v.mode)){text(own->original_claim_id);if(own->holder!=holder(v.owner_business_ref.accepted_arrangement_id))fail("CareFacilityOwnHolderMismatch",v.binding_id);}
 else{const auto& b=std::get<CareFacilityBorrowedPhysicalHolder>(v.mode);source(b.physical_holder_source_ref);source(b.authorized_use_source_ref);text(b.placement_claim_id);if(b.physical_holder_source_schema<1||b.authorized_use_source_schema<1)fail("InvalidCareFacilityBorrowedSchema",v.binding_id);}
 constexpr std::string_view names[]={"binding_id","owner_business_ref","accepted_terms_ref","usage_rule_ref","slot_ref","mode","created_source_ref","created_source_schema","created_at","field_order"};
 for(std::size_t i=0;i<v.field_order.size();++i){if(std::find(std::begin(names),std::end(names),v.field_order[i])==std::end(names))fail("UnknownCareFacilityField",v.field_order[i]);for(std::size_t j=0;j<i;++j)if(v.field_order[j]==v.field_order[i])fail("DuplicateCareFacilityField",v.field_order[i]);}
 return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_facility_use_sources(const CareFacilityUseSources& x){try{
 const auto now=context(x);check(validate_accepted_caregiver_sources(x.acceptance));
 for(std::size_t i=0;i<x.bindings.rows().size();++i){const auto& v=x.bindings.rows()[i];if(v.first!=v.second.binding_id)fail("CareFacilityBindingKeyMismatch",v.first);auto actual=row(x,v.second,now);for(std::size_t j=0;j<i;++j){const auto& other=x.bindings.rows()[j].second;if(other.owner_business_ref.accepted_arrangement_id==v.second.owner_business_ref.accepted_arrangement_id)distinct(other,row(x,other,now),v.second,actual);}}
 struct Coverage final:CareFacilityUseAdoptionVisitor{
  const CareFacilityUseSources& x;const WorldMoment& now;CheckpointVector<const CareFacilityUseBinding*> seen;
  Coverage(const CareFacilityUseSources& s,const WorldMoment& m):x(s),now(m){}
  Status adopted(const CareArrangementRef& owner,const FacilitySlotRef& slot,const MomentOwnerSource& source)override{try{original(x,owner,slot,source,now);const CareFacilityUseBinding* match=nullptr;for(const auto& v:x.bindings.rows())if(v.second.owner_business_ref.accepted_arrangement_id==owner.accepted_arrangement_id&&v.second.slot_ref==slot&&same({v.second.created_source_ref,v.second.created_source_schema},source)){if(match)fail("CareFacilityRequiredBindingCoverage",owner.accepted_arrangement_id);match=&v.second;}if(!match)fail("CareFacilityRequiredBindingCoverage",owner.accepted_arrangement_id);if(std::find(seen.begin(),seen.end(),match)!=seen.end())fail("DuplicateCareOriginalFacilityAdoption",match->binding_id);seen.push_back(match);return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
 }coverage(x,now);check(x.originals.visit_original_care_facility_use_adoptions(coverage));if(coverage.seen.size()!=x.bindings.size())fail("CareFacilityAdoptionCoverageMismatch");
 return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<const CareFacilityUseBinding*> read_care_facility_use_binding(const CareFacilityUseSources& x,const MomentOwnerSource& ref){try{if(take(care_facility_use_source_schema(ref.source_ref))!=ref.source_schema)fail("CareFacilityUseSchemaMismatch",ref.source_ref.id);check(validate_care_facility_use_sources(x));const auto* value=x.bindings.find(ref.source_ref.id);if(!value)fail("CareFacilityBindingMissing",ref.source_ref.id);return {true,value,{}};}catch(Failure& e){return {false,nullptr,std::move(e.error)};}}
Outcome<CareFacilityUseApplication> bind_care_facility_use_in_candidate(CommonStore& common,OrderedTable<CareFacilityUseBinding>& values,const CareFacilityUseSources& x,const CareArrangementRef& owner,const FacilitySlotRef& slot,const MomentOwnerSource& created){try{
 if(&common!=&x.acceptance.goals.common||&values!=&x.bindings)fail("CareFacilitySameCandidateRequired");const auto now=context(x);auto actual=original(x,owner,slot,created,now);
 for(const auto& pair:values.rows()){const auto& previous=pair.second;auto previous_actual=row(x,previous,now);if(previous.owner_business_ref.accepted_arrangement_id!=owner.accepted_arrangement_id)continue;
  if(previous.slot_ref==slot){if(!same({previous.created_source_ref,previous.created_source_schema},created)||!actor_foundation_rule_equal(previous.usage_rule_ref,actual.usage_rule_ref)||!same(previous.mode,actual.mode)||!same(previous.created_at,actual.created_at))fail("CareFacilityImmutableAdoptionConflict",previous.binding_id);return {true,{{care_facility_use_source(previous.binding_id),1},true},{}};}
  if(previous_actual.physical_capacity_identity==actual.physical_capacity_identity)fail("DuplicateCareFacilityPhysicalUse",previous.binding_id);
 }
 if(!same(actual.created_at,now))fail("CareFacilityActualAdoptionMomentRequired",created.source_ref.id);
 CareFacilityUseBinding value;value.binding_id=take(common.next_id("care_facility_use"));value.owner_business_ref=owner;value.accepted_terms_ref=actual.accepted->accepted_id;value.usage_rule_ref=std::move(actual.usage_rule_ref);value.slot_ref=std::move(actual.slot_ref);value.mode=std::move(actual.mode);value.created_source_ref=created.source_ref;value.created_source_schema=created.source_schema;value.created_at=std::move(actual.created_at);check(validate_care_facility_use_shape(value,5));const auto id=value.binding_id;
 if(!values.insert(id,std::move(value)))fail("DuplicateCareFacilityBinding",id);
 // Full reverse coverage runs after the original owner finishes all its slots.
 // There is no new Common notification head and no resource mutation here.
 return {true,{{care_facility_use_source(id),1},false},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
}
