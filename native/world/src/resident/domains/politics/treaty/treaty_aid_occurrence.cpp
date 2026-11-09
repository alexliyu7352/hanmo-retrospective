#include "treaty_aid_occurrence.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
void take(Status r){if(!r.ok)throw Failure{std::move(r.error)};}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore& common,const SourceRef& source){
 const auto* fact=common.state().facts.find(source.id);
 if(!valid_source_ref(source)||!fact||fact->domain!=source.domain||fact->kind!=source.kind)
  fail("TreatyAidActualFactMissing",source.id);
 const auto& order=common.state().facts.order();auto i=std::find(order.begin(),order.end(),source.id);
 if(i==order.end())fail("TreatyAidActualFactOrderMissing",source.id);
 return static_cast<std::size_t>(i-order.begin());
}
bool contains(const std::vector<SourceRef>& refs,const SourceRef& r){
 for(const auto& x:refs)if(same_source_ref(x,r))return true;return false;
}
void distinct(const std::vector<SourceRef>& refs,const StableId& owner){
 for(std::size_t i=0;i<refs.size();++i){if(!valid_source_ref(refs[i]))fail("TreatyAidInvalidSource",owner);
  for(std::size_t j=0;j<i;++j)if(same_source_ref(refs[i],refs[j]))fail("TreatyAidDuplicateSource",owner);}
}
ActivatedTreatyAidTermsRead terms(const TreatyAidPerformanceReadContext& x,const TreatyAidSchedule& s){
 auto t=take(x.terms.read_activated_material_aid_terms(s.treaty_id,s.terms_revision,s.term_id));
 take(validate_treaty_aid_activation_terms(t));
 if(t.treaty_id!=s.treaty_id||t.terms_revision!=s.terms_revision||t.term_id!=s.term_id||
    !same_source_ref(t.activation_source,s.activation_ref)||!same_source_ref(t.original_terms_source,s.schedule_terms_ref))
  fail("TreatyAidScheduleIdentityMismatch",s.id);
 return t;
}
void establishment(const TreatyAidPerformanceReadContext& x,const TreatyAidSchedule& s,
 const TreatyAidEstablishmentRead& a,const WorldMoment& now){
 const auto t=terms(x,s);
 if(!valid_source_ref(a.establishment_result_ref)||!same_source_ref(a.schedule_ref,treaty_aid_schedule_source(s))||
    !same_source_ref(a.activation_ref,s.activation_ref)||!same_source_ref(a.schedule_terms_ref,s.schedule_terms_ref)||
    a.occurrence_ordinal<0||a.occurrence_ordinal>=t.original_term->occurrences)
  fail("TreatyAidEstablishmentIdentityMismatch",s.id);
 take(validate_obligation_ref_shape(a.original_obligation_ref));
 if(a.original_obligation_ref.owner_domain!="local_economy")fail("TreatyAidFinancialOwnerMismatch",s.id);
 const auto due=take(material_aid_occurrence_due(*t.original_term,a.occurrence_ordinal));
 if(due>std::numeric_limits<Integer>::max()-t.original_term->delivery_deadline_ticks)
  fail("TreatyAidDeadlineOverflow",s.id);
 if(a.due_tick!=due||a.delivery_deadline_tick!=due+t.original_term->delivery_deadline_ticks||
    compare(a.established_at,WorldMoment{due,{}})!=0||compare(a.established_at,now)>0||
    compare(a.established_at,t.activated_at)<0||
    !take(treaty_window_contains(t.original_terms->effective_window,a.established_at))||
    (t.terminated_at&&compare(a.established_at,*t.terminated_at)>=0))
  fail("TreatyAidEstablishmentWindowMismatch",s.id);
 if(position(x.common,s.activation_ref)>=position(x.common,a.establishment_result_ref))
  fail("TreatyAidEstablishmentBeforeActivation",s.id);
}
void binding(const TreatyAidPerformanceReadContext& x,const TreatyAidSchedule& s,
 const TreatyAidOccurrenceBinding& b,const WorldMoment& now){
 take(validate_treaty_aid_occurrence_shape(b));
 if(!same_source_ref(b.schedule_ref,treaty_aid_schedule_source(s)))fail("TreatyAidBindingScheduleMismatch",b.id);
 const auto a=take(x.finances.read_material_aid_establishment(b.establishment_result_ref));
 establishment(x,s,a,now);
 if(!same_source_ref(a.establishment_result_ref,b.establishment_result_ref)||
    a.occurrence_ordinal!=b.occurrence_ordinal||!same_obligation_ref(a.original_obligation_ref,b.original_obligation_ref))
  fail("TreatyAidBindingOriginalMismatch",b.id);
 if(position(x.common,b.establishment_result_ref)>=position(x.common,treaty_aid_occurrence_source(b)))
  fail("TreatyAidBindingBeforeEstablishment",b.id);
 if(b.shipment_ref)take(x.finances.validate_material_aid_shipment(a,*b.shipment_ref));
 for(const auto& r:b.receipt_result_refs)take(x.finances.validate_material_aid_receipt(a,b.shipment_ref,r));
 if(b.deadline_result_ref)take(x.finances.validate_material_aid_deadline_result(a,*b.deadline_result_ref));
 if(b.termination_disposition_ref)take(x.finances.validate_material_aid_termination_disposition(a,*b.termination_disposition_ref));
}
// pending is private to the real adoption stack: its actual S5 result has already
// been verified and is the one original row not yet consumed by S6.
void prefixes(const TreatyAidPerformanceReadContext& x,const SourceRef* pending){
 const auto now=take(x.clock.read_organization_authority_now());take(encode_world_moment(now));
 bool pending_seen=false;
 for(const auto& row:x.schedules.rows()){
  const auto& s=row.second;if(row.first!=s.id||s.id.empty())fail("TreatyAidScheduleIdentityMismatch",row.first);
  const auto t=terms(x,s);if(s.next_occurrence_ordinal<0||s.next_occurrence_ordinal>t.original_term->occurrences)
   fail("TreatyAidCursorOutsideTerms",s.id);
  if(bool(s.termination_ref)!=bool(t.termination_source))fail("TreatyAidUnadoptedTermination",s.id);
  for(const auto& other:x.schedules.rows())if(other.first!=row.first&&
   other.second.treaty_id==s.treaty_id&&other.second.terms_revision==s.terms_revision&&other.second.term_id==s.term_id)
   fail("TreatyAidDuplicateSchedule",s.id);
  auto original=take(x.finances.enumerate_material_aid_establishments(treaty_aid_schedule_source(s)));
  distinct(original,s.id);Integer count=0;
  for(const auto& item:x.occurrence_bindings.rows()){
   const auto& b=item.second;if(b.schedule_ref.id!=s.id)continue;
   if(item.first!=b.id||b.occurrence_ordinal>=s.next_occurrence_ordinal)fail("TreatyAidCursorPrefixMismatch",b.id);
   binding(x,s,b,now);
   if(!contains(original,b.establishment_result_ref))fail("TreatyAidEstablishmentReverseMissing",b.id);
   for(const auto& prior:x.occurrence_bindings.rows())if(prior.first!=item.first&&
    ((same_source_ref(prior.second.schedule_ref,b.schedule_ref)&&prior.second.occurrence_ordinal==b.occurrence_ordinal)||
     same_source_ref(prior.second.establishment_result_ref,b.establishment_result_ref)||
     same_obligation_ref(prior.second.original_obligation_ref,b.original_obligation_ref)))
    fail("TreatyAidDuplicateOccurrence",b.id);
   if(count==std::numeric_limits<Integer>::max())fail("TreatyAidPrefixOverflow",s.id);++count;
  }
  if(count!=s.next_occurrence_ordinal)fail("TreatyAidIncompleteEstablishedPrefix",s.id);
  for(const auto& ref:original){
   bool found=false;for(const auto& item:x.occurrence_bindings.rows())if(same_source_ref(item.second.establishment_result_ref,ref)&&same_source_ref(item.second.schedule_ref,treaty_aid_schedule_source(s))){found=true;break;}
   if(found)continue;
   if(pending&&same_source_ref(*pending,ref)&&!pending_seen){
    const auto a=take(x.finances.read_material_aid_establishment(ref));establishment(x,s,a,now);
    if(!same_source_ref(a.establishment_result_ref,ref)||a.occurrence_ordinal!=s.next_occurrence_ordinal||compare(a.established_at,now)!=0)
     fail("TreatyAidPendingEstablishmentMismatch",s.id);
    pending_seen=true;continue;
   }
   fail("TreatyAidUnboundFinancialEstablishment",ref.id);
  }
  take(read_treaty_aid_due(s,x.terms,x.clock));
 }
 for(const auto& row:x.occurrence_bindings.rows()){
  if(!x.schedules.find(row.second.schedule_ref.id))fail("TreatyAidOrphanOccurrence",row.first);
 }
 for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="politics"&&row.second.kind=="treaty_aid_occurrence_bound"){
  if(!x.occurrence_bindings.find(row.first))fail("TreatyAidOrphanBindingFact",row.first);
 }
 if(pending&&!pending_seen)fail("TreatyAidPendingOriginalNotEnumerated",pending->id);
}
}
SourceRef treaty_aid_schedule_source(const TreatyAidSchedule& s){return {"politics","treaty_aid_schedule",s.id};}
SourceRef treaty_aid_occurrence_source(const TreatyAidOccurrenceBinding& b){return {"politics","treaty_aid_occurrence_bound",b.id};}
Status validate_treaty_aid_occurrence_shape(const TreatyAidOccurrenceBinding& b){try{
 if(b.id.empty()||b.schedule_ref.domain!="politics"||b.schedule_ref.kind!="treaty_aid_schedule"||
    !valid_source_ref(b.schedule_ref)||b.occurrence_ordinal<0||!valid_source_ref(b.establishment_result_ref))
  fail("InvalidTreatyAidOccurrence",b.id);
 take(validate_obligation_ref_shape(b.original_obligation_ref));distinct(b.receipt_result_refs,b.id);
 if((b.shipment_ref&&!valid_source_ref(*b.shipment_ref))||(b.deadline_result_ref&&!valid_source_ref(*b.deadline_result_ref))||
    (b.termination_disposition_ref&&!valid_source_ref(*b.termination_disposition_ref)))fail("TreatyAidInvalidSource",b.id);
 return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
Status validate_treaty_aid_occurrence_prefixes(const TreatyAidPerformanceReadContext& x){try{prefixes(x,nullptr);return common_success();}catch(const Failure& f){return {false,{},f.error};}}
Outcome<const TreatyAidOccurrenceBinding*> read_treaty_aid_occurrence_binding(
 const TreatyAidPerformanceReadContext& x,const SourceRef& source,Integer schema){try{
 if(schema!=1||source.domain!="politics"||source.kind!="treaty_aid_occurrence_bound")
  fail("TreatyAidBindingSourceUnsupported",source.id);
 const auto* b=x.occurrence_bindings.find(source.id);
 if(!b||!same_source_ref(treaty_aid_occurrence_source(*b),source))fail("TreatyAidOccurrenceMissing",source.id);
 const auto* s=x.schedules.find(b->schedule_ref.id);if(!s)fail("TreatyAidScheduleMissing",b->schedule_ref.id);
 const auto now=take(x.clock.read_organization_authority_now());binding(x,*s,*b,now);
 if(b->occurrence_ordinal>=s->next_occurrence_ordinal)fail("TreatyAidCursorPrefixMismatch",b->id);
 return {true,b,{}};
 }catch(const Failure& f){return {false,nullptr,f.error};}}
Outcome<WorldMoment> read_treaty_aid_occurrence_source_moment(
 const TreatyAidPerformanceReadContext& x,const SourceRef& source,Integer schema){try{
 const auto* b=take(read_treaty_aid_occurrence_binding(x,source,schema));
 auto actual=take(x.finances.read_material_aid_establishment(b->establishment_result_ref));
 return {true,std::move(actual.established_at),{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<SourceRef> adopt_treaty_aid_occurrence_in_candidate(CommonStore& common,
 OrderedTable<TreatyAidSchedule>& schedules,OrderedTable<TreatyAidOccurrenceBinding>& bindings,
 const TreatyAidPerformanceReadContext& x,const SourceRef& actual){try{
 if(&common!=&x.common||&schedules!=&x.schedules||&bindings!=&x.occurrence_bindings)
  fail("TreatyAidCandidateMismatch");
 auto a=take(x.finances.read_material_aid_establishment(actual));
 if(!same_source_ref(a.establishment_result_ref,actual))fail("TreatyAidEstablishmentIdentityMismatch",actual.id);
 auto* s=schedules.find(a.schedule_ref.id);if(!s)fail("TreatyAidScheduleMissing",a.schedule_ref.id);
 const auto now=take(x.clock.read_organization_authority_now());establishment(x,*s,a,now);
 for(const auto& row:bindings.rows())if(same_source_ref(row.second.establishment_result_ref,actual)){
  prefixes(x,nullptr);return {true,treaty_aid_occurrence_source(row.second),{}};
 }
 prefixes(x,&actual);
 auto due=take(read_treaty_aid_due(*s,x.terms,x.clock));
 if(due.kind!=TreatyAidDueKind::due||due.ordinal!=a.occurrence_ordinal||compare(now,a.established_at)!=0)
  fail("TreatyAidOccurrenceNotDue",s->id);
 TreatyAidOccurrenceBinding b;b.id=take(common.next_id("treaty_aid_occurrence"));b.schedule_ref=a.schedule_ref;
 b.occurrence_ordinal=a.occurrence_ordinal;b.establishment_result_ref=actual;b.original_obligation_ref=a.original_obligation_ref;
 const auto source=treaty_aid_occurrence_source(b);take(common.append_fact({b.id,source.kind,source.domain}));
 if(!bindings.insert(b.id,std::move(b)))fail("TreatyAidDuplicateOccurrence",source.id);
 if(s->next_occurrence_ordinal==std::numeric_limits<Integer>::max())fail("TreatyAidCursorOverflow",s->id);
 ++s->next_occurrence_ordinal;prefixes(x,nullptr);return {true,source,{}};
 }catch(const Failure& f){return {false,{},f.error};}}
}
