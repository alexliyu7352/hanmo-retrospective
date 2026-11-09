// 每条链使用既有原 writer；任一失败必须撤销整个未发布 World 候选。
#include "treaty_aid_resource_financial_adoption2.hpp"
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]] void fail(const char* code,const StableId& id={}) {
 throw Failure{common_failure(code,id).error};
}
template<class T>T take(Outcome<T> value) {
 if(!value.ok)throw Failure{value.error};return std::move(value.value);
}
int compare(const WorldMoment& a,const WorldMoment& b) {return take(compare_world_moments(a,b));}
// 此编排不能将某候选的资金/源读者借给另一个世界，时钟仍由各原 owner 真读核等。
void context(const TreatyAidResourceFinancialContext2& x) {
 const auto& p=x.performance;const auto& f=x.finance.read;
 if(&x.schedules.common!=&p.common||&p.common!=&f.common||
    &x.schedules.schedules!=&p.schedules||&p.schedules!=&f.schedules||
    &p.terms!=&f.terms||&x.finance.economy!=&f.economy||&x.finance.budget!=&f.budget||
    !f.resources.bound_to(f.common)||f.selected_treaty_sources!=2||
    &x.compensation.principal!=&f||
    &x.schedules.treaties.activations.signatures.links.revisions.common!=&p.common)
  fail("TreatyAidFinancialCandidateMismatch");
 const auto actual=take(x.schedules.treaties.activations.signatures.links.revisions.clock.read_organization_authority_now());
 if(compare(actual,take(p.clock.read_organization_authority_now()))||
    compare(actual,take(f.clock.read_current_moment())))fail("TreatyAidFinancialClockMismatch");
}
void write_context(CommonStore& common,OrderedTable<TreatyAidSchedule>& schedules,
 TreatyMaterialAidState2& finances,const TreatyAidResourceFinancialContext2& x) {
 context(x);if(&common!=&x.performance.common||&schedules!=&x.performance.schedules||
  finances.schema_version!=2||&finances.principal!=&x.finance.read.state||&finances.compensation!=&x.compensation.state)fail("TreatyAidFinancialCandidateMismatch");
}
bool same(const MomentOwnerSource& a,const MomentOwnerSource& b) {
 return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);
}
const TreatyAidSchedule* find(const TreatyAidResourceFinancialContext2& x,
 const TreatyActivationSource& activation,const StableId& term) {
 const TreatyAidSchedule* found=nullptr;
 for(const auto& row:x.performance.schedules.rows())if(row.second.treaty_id==activation.treaty_id&&
    row.second.terms_revision==activation.terms_revision&&row.second.term_id==term) {
  if(found||row.first!=row.second.id)fail("TreatyAidDuplicateSchedule",row.first);
  found=&row.second;
 }
 return found;
}
}
Status validate_treaty_aid_resource_financial_adoption2(const TreatyAidResourceFinancialContext2& x) {try {
 context(x);take(validate_treaty_aid_resource_schedule_sources(x.schedules));
 take(validate_material_aid_compensation_original_coverage2(x.compensation));
 take(finalize_material_aid_acceptances_in_candidate(x.finance));
 take(validate_treaty_aid_occurrence_prefixes(x.performance));return common_success();
 }catch(const Failure& error){return {false,{},error.error};}}
Outcome<std::vector<SourceRef>> accept_treaty_aid_resource_schedules_in_candidate2(CommonStore& common,
 OrderedTable<TreatyAidSchedule>& schedules,TreatyMaterialAidState2& finances,
 const TreatyAidResourceFinancialContext2& x,const SourceRef& activation_ref,
 const std::vector<MomentOwnerSource>& accepted) {try {
 write_context(common,schedules,finances,x);
 const auto activation=take(read_original_treaty_resource_activation(x.schedules.treaties.activations,activation_ref,2));
 std::size_t expected=0;for(const auto& term:activation.revision.definition->terms.terms)
  if(std::holds_alternative<TreatyMaterialAidTerm>(term.terms))++expected;
 if(expected!=accepted.size())fail("TreatyAidResourceAcceptanceCoverageMismatch",activation_ref.id);
 // 热映射只保真实 term/source 身份，不复制 S5 已有库、钱包或预算接受载荷。
 struct AcceptedTerm {StableId term_id;MomentOwnerSource source;};
 std::vector<AcceptedTerm> resources;resources.reserve(expected);
 // 所有真实接受身份先反读；后续各 S5 writer 继续核完整库/钱包/预算/两方权限。
 for(std::size_t i=0;i<accepted.size();++i) {
  for(std::size_t j=0;j<i;++j)if(same(accepted[i],accepted[j]))fail("DuplicateTreatyAidResourceAcceptance",accepted[i].source_ref.id);
  auto actual=take(x.finance.read.resources.read_accepted_aid_resources(accepted[i]));
  if(!same(actual.actual_acceptance,accepted[i])||actual.treaty_id!=activation.source->treaty_id||
     actual.terms_revision!=activation.source->terms_revision)
   fail("TreatyAidResourceAcceptanceRevisionMismatch",accepted[i].source_ref.id);
  for(const auto& previous:resources)if(previous.term_id==actual.term_id)
   fail("DuplicateTreatyAidTermAcceptance",actual.term_id);
  resources.push_back({std::move(actual.term_id),actual.actual_acceptance});
 }
 std::vector<SourceRef> result;std::size_t count=0;
 for(const auto& term:activation.revision.definition->terms.terms)
  if(std::holds_alternative<TreatyMaterialAidTerm>(term.terms)) {
   const AcceptedTerm* actual=nullptr;
   for(const auto& value:resources)if(value.term_id==term.term_id)actual=&value;
   if(!actual)fail("TreatyAidActualResourcesRequired",term.term_id);
   const auto schedule=take(create_treaty_aid_resource_schedule_in_candidate(common,schedules,x.schedules,activation_ref,term.term_id));
   const auto commitment=take(accept_aid_schedule_in_candidate(common,finances.principal,x.finance,schedule.id,actual->source));
   if(std::holds_alternative<TreatyBoundedCompensation>(std::get<TreatyMaterialAidTerm>(term.terms).late_policy))
    take(accept_material_aid_compensation_in_candidate2(common,finances.compensation,x.compensation,x.finance,commitment));
   result.push_back(commitment);
   ++count;
  }
 if(count!=accepted.size())fail("TreatyAidUnexpectedResourceAcceptance",activation_ref.id);
 // 全部真实 schedule 接受后才核总预算，不能每个 term 先各自视为完整 activation。
 take(validate_treaty_aid_resource_financial_adoption2(x));return {true,std::move(result),{}};
 }catch(const Failure& error){return {false,{},error.error};}}
Outcome<SourceRef> establish_and_adopt_treaty_aid_resource_occurrence_in_candidate2(CommonStore& common,
 OrderedTable<TreatyAidSchedule>& schedules,OrderedTable<TreatyAidOccurrenceBinding>& bindings,
 TreatyMaterialAidState2& finances,const TreatyAidResourceFinancialContext2& x,
 const StableId& schedule_id,Integer ordinal) {try {
 write_context(common,schedules,finances,x);
 if(&bindings!=&x.performance.occurrence_bindings)fail("TreatyAidFinancialCandidateMismatch");
 const auto* schedule=schedules.find(schedule_id);if(!schedule)fail("TreatyAidScheduleMissing",schedule_id);
 take(read_original_treaty_aid_resource_schedule(x.schedules,treaty_aid_schedule_source(*schedule),2));
 const auto actual=take(establish_material_aid_occurrence_in_candidate(common,finances.principal,x.finance,schedule_id,ordinal));
 const auto adopted=take(adopt_treaty_aid_occurrence_in_candidate(common,schedules,bindings,x.performance,actual));
 take(validate_treaty_aid_resource_financial_adoption2(x));return {true,adopted,{}};
 }catch(const Failure& error){return {false,{},error.error};}}
Outcome<std::vector<SourceRef>> release_treaty_aid_resource_schedules_in_candidate2(CommonStore& common,
 OrderedTable<TreatyAidSchedule>& schedules,TreatyMaterialAidState2& finances,
 const TreatyAidResourceFinancialContext2& x,const SourceRef& termination_ref) {try {
 write_context(common,schedules,finances,x);
 const auto termination=take(read_original_treaty_resource_termination(x.schedules.treaties,termination_ref,2));
 const auto& activation=*termination.activation.source;std::vector<SourceRef> result;
 for(const auto& term:termination.activation.revision.definition->terms.terms)
  if(std::holds_alternative<TreatyMaterialAidTerm>(term.terms)) {
   const auto* schedule=find(x,activation,term.term_id);if(!schedule)fail("TreatyActivatedAidScheduleMissing",term.term_id);
   const auto id=schedule->id;const auto ordinal=schedule->next_occurrence_ordinal;
   if(!same_source_ref(schedule->activation_ref,termination.source->activation_ref)||
      (schedule->termination_ref&&!same_source_ref(*schedule->termination_ref,termination_ref)))
    fail("TreatyAidScheduleTerminationMismatch",id);
   const auto actual=take(terminate_aid_schedule_in_candidate(common,finances.principal,x.finance.read,id));
   const auto* release=take(read_material_aid_schedule_release(x.finance.read,actual));
   const auto* commitment=take(read_aid_schedule_commitment(x.finance.read,
    aid_schedule_commitment_source(release->schedule_commitment_id)));
   if(commitment->schedule_id!=id||release->actual_termination_ref.source_schema!=2||
      !same_source_ref(release->actual_termination_ref.source_ref,termination_ref)||
      compare(release->occurred_at,termination.source->terminated_at))
    fail("TreatyAidOriginalReleaseMismatch",actual.id);
   auto* mutable_schedule=schedules.find(id);
   if(!mutable_schedule||mutable_schedule->next_occurrence_ordinal!=ordinal)
    fail("TreatyAidReleaseChangedCursor",id);
   mutable_schedule->termination_ref=termination_ref;
   if(std::holds_alternative<TreatyBoundedCompensation>(std::get<TreatyMaterialAidTerm>(term.terms).late_policy))
    take(release_material_aid_compensation_on_termination_in_candidate2(common,finances.compensation,x.compensation,actual));
   result.push_back(actual);
  }
 take(validate_treaty_aid_resource_financial_adoption2(x));return {true,std::move(result),{}};
 }catch(const Failure& error){return {false,{},error.error};}}
}
