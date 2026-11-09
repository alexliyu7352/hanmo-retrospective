// 原援助 schedule 的唯一建立/冷反查，保持 S6 cursor 与 S5 财务事实分权。
#include "treaty_aid_schedule_sources.hpp"
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]] void fail(const char* code,const StableId& id={}) {
 throw Failure{common_failure(code,id).error};
}
template<class T>T take(Outcome<T> value) {
 if(!value.ok)throw Failure{value.error};return std::move(value.value);
}
int compare(const WorldMoment& a,const WorldMoment& b) {return take(compare_world_moments(a,b));}
// 两个 context 必须直接借同一候选，禁止旧世界的条款读者为新行作证。
void context(const TreatyAidScheduleSourceContext& x) {
 if(&x.common!=&x.treaties.activations.signatures.revisions.common)
  fail("TreatyAidScheduleCandidateMismatch");
}
const TreatyAidSchedule* find(const TreatyAidScheduleSourceContext& x,
 const StableId& treaty,Integer revision,const StableId& term) {
 const TreatyAidSchedule* found=nullptr;
 for(const auto& row:x.schedules.rows())if(row.second.treaty_id==treaty&&
    row.second.terms_revision==revision&&row.second.term_id==term) {
  if(found)fail("TreatyAidDuplicateSchedule",row.first);
  if(row.first!=row.second.id)fail("TreatyAidScheduleIdentityMismatch",row.first);
  found=&row.second;
 }
 return found;
}
// typed schedule 的身份不是 Common Fact；实际发生/先序由其唯一 activation 原源证明。
ActivatedTreatyAidTermsRead original(const TreatyAidScheduleSourceContext& x,const TreatyAidSchedule& s) {
 context(x);
 if(s.id.empty()||x.schedules.find(s.id)!=&s||
    find(x,s.treaty_id,s.terms_revision,s.term_id)!=&s)
  fail("TreatyAidScheduleIdentityMismatch",s.id);
 NativeTreatyAidTermsRead reader(x.treaties);
 auto actual=take(reader.read_activated_material_aid_terms(s.treaty_id,s.terms_revision,s.term_id));
 take(validate_treaty_aid_activation_terms(actual));
 if(!same_source_ref(s.activation_ref,actual.activation_source)||
    !same_source_ref(s.schedule_terms_ref,actual.original_terms_source)||
    s.next_occurrence_ordinal<0||s.next_occurrence_ordinal>actual.original_term->occurrences)
  fail("TreatyAidScheduleOriginalMismatch",s.id);
 if(s.termination_ref.has_value()!=actual.termination_source.has_value()||
    (s.termination_ref&&!same_source_ref(*s.termination_ref,*actual.termination_source)))
  fail("TreatyAidUnadoptedTermination",s.id);
 return actual;
}
}
Outcome<const TreatyAidSchedule*> read_original_treaty_aid_schedule(
 const TreatyAidScheduleSourceContext& x,const SourceRef& ref,Integer schema) {try {
 context(x);
 if(schema!=1||ref.domain!="politics"||ref.kind!="treaty_aid_schedule"||!valid_source_ref(ref))
  fail("TreatyAidScheduleSourceUnsupported",ref.id);
 const auto* s=x.schedules.find(ref.id);if(!s)fail("TreatyAidScheduleMissing",ref.id);
 original(x,*s);return {true,s,{}};
 }catch(const Failure& error){return {false,nullptr,error.error};}}
Status validate_treaty_aid_schedule_sources(const TreatyAidScheduleSourceContext& x) {try {
 context(x);for(const auto& row:x.schedules.rows()) {
  if(row.first!=row.second.id)fail("TreatyAidScheduleIdentityMismatch",row.first);
  original(x,row.second);
 }
 for(const auto& row:x.treaties.activations.activations.rows()) {
  const auto activation=take(read_original_treaty_activation(x.treaties.activations,
   treaty_activation_source(row.second),1));
  for(const auto& term:activation.revision.definition->terms.terms)
   if(std::holds_alternative<TreatyMaterialAidTerm>(term.terms)) {
    const auto* schedule=find(x,activation.source->treaty_id,
     activation.source->terms_revision,term.term_id);
    if(!schedule)fail("TreatyActivatedAidScheduleMissing",term.term_id);
    if(!same_source_ref(schedule->activation_ref,treaty_activation_source(*activation.source)))
     fail("TreatyAidScheduleActivationMismatch",schedule->id);
   }
 }
 return common_success();
 }catch(const Failure& error){return {false,{},error.error};}}
Outcome<SourceRef> create_treaty_aid_schedule_in_candidate(CommonStore& common,
 OrderedTable<TreatyAidSchedule>& schedules,const TreatyAidScheduleSourceContext& x,
 const SourceRef& activation_ref,const StableId& term_id) {try {
 context(x);if(&common!=&x.common||&schedules!=&x.schedules)
  fail("TreatyAidScheduleCandidateMismatch");
 const auto activation=take(read_original_treaty_activation(x.treaties.activations,activation_ref,1));
 if(const auto* existing=find(x,activation.source->treaty_id,activation.source->terms_revision,term_id)) {
  original(x,*existing);if(!same_source_ref(existing->activation_ref,activation_ref))
   fail("TreatyAidScheduleActivationMismatch",existing->id);
  return {true,treaty_aid_schedule_source(*existing),{}};
 }
 NativeTreatyAidTermsRead reader(x.treaties);
 const auto actual=take(reader.read_activated_material_aid_terms(activation.source->treaty_id,
  activation.source->terms_revision,term_id));
 const auto now=take(x.treaties.activations.signatures.revisions.clock.read_organization_authority_now());
 if(!same_source_ref(actual.activation_source,activation_ref)||actual.termination_source||
    compare(actual.activated_at,now))fail("TreatyAidActualActivationRequired",term_id);
 // 先完成全部原源核验再消耗原 ID；不存在独立 schedule 时间或任意 caller cursor。
 TreatyAidSchedule row;row.id=take(common.next_id("treaty_aid_schedule"));
 row.treaty_id=actual.treaty_id;row.terms_revision=actual.terms_revision;row.term_id=actual.term_id;
 row.activation_ref=actual.activation_source;row.schedule_terms_ref=actual.original_terms_source;
 const auto source=treaty_aid_schedule_source(row);
 if(!schedules.insert(source.id,std::move(row)))fail("TreatyAidDuplicateSchedule",source.id);
 take(read_original_treaty_aid_schedule(x,source,1));return {true,source,{}};
 }catch(const Failure& error){return {false,{},error.error};}}
}
