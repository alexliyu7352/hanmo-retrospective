// 所有额度数字从同截面原责任/真实付款逐腿派生；退款只说明去向，不补gross额度。
#include "budget_execution_contract.hpp"
#include "resident/kernel/read_stamp.hpp"
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
static_assert(static_cast<Integer>(BudgetExecutionPurpose::transport_expense)==0 &&
              static_cast<Integer>(BudgetExecutionPurpose::recurring_obligation)==5 &&
              static_cast<Integer>(BudgetExecutionPurpose::material_aid)==6,
              "Budget purpose wire ordinals are fixed");
const char* budget_execution_purpose_key(BudgetExecutionPurpose purpose) noexcept {
 switch(purpose){
  case BudgetExecutionPurpose::transport_expense:return "transport_expense";
  case BudgetExecutionPurpose::contract_purchase:return "contract_purchase";
  case BudgetExecutionPurpose::governance_work_expense:return "governance_work_expense";
  case BudgetExecutionPurpose::public_relief:return "public_relief";
  case BudgetExecutionPurpose::production_construction:return "production_construction";
  case BudgetExecutionPurpose::recurring_obligation:return "recurring_obligation";
  case BudgetExecutionPurpose::material_aid:return "material_aid";
 }
 return "unknown";
}
namespace {
Status state_version(Integer version){
 if(version==1||version==2)return common_success();
 return common_failure("BudgetExecutionVersionMismatch",
     "budget_execution/observed="+std::to_string(version)+"/expected=1|2",
     ErrorKind::unsupported_capability);
}
}
Status validate_budget_execution_purpose(BudgetExecutionPurpose purpose,Integer version){
 auto valid=state_version(version);if(!valid.ok)return valid;
 const auto tag=static_cast<Integer>(purpose);
 if(tag<0||tag>(version==1?5:6))
  return common_failure("InvalidBudgetPurpose",
      "budget_execution/purpose/"+std::to_string(tag)+"/schema="+std::to_string(version));
 if(version==2&&purpose!=BudgetExecutionPurpose::governance_work_expense&&
    purpose!=BudgetExecutionPurpose::material_aid){
  auto rejected=common_failure("NOT_IMPLEMENTED",budget_execution_purpose_key(purpose),
                               ErrorKind::unsupported_capability);
  rejected.error.reason=budget_execution_purpose_key(purpose);
  return rejected;
 }
 return common_success();
}
namespace {
bool unit_equal(const BudgetUnit&a,const BudgetUnit&b){if(a.index()!=b.index())return false;const auto* x=std::get_if<BudgetItemQuantity>(&a);return !x||x->item_id==std::get<BudgetItemQuantity>(b).item_id;}
// Original multi-item obligations have one owner key with a distinct measure
// for each item/currency. Never merge these dimensions or duplicate one dimension.
using BudgetMeasureKey=std::pair<std::size_t,StableId>;
BudgetMeasureKey measure_key(const BudgetUnit&unit){const auto*item=std::get_if<BudgetItemQuantity>(&unit);return {unit.index(),item?item->item_id:StableId{}};}
using BudgetLegKey=std::tuple<StableId,StableId,BudgetMeasureKey>;
BudgetLegKey leg_key(const BudgetExecutionBinding&binding,const StableId&owner,const StableId&id){return {owner,id,measure_key(binding.unit)};}
bool period_equal(const BudgetPeriod&a,const BudgetPeriod&b){return a.period_from_tick==b.period_from_tick&&a.period_until_tick==b.period_until_tick;}
bool execution_equal(const BudgetExecutionRef&a,const BudgetExecutionRef&b){return std::tie(a.budget_id,a.budget_revision,a.policy_revision,a.approval_fact_id)==std::tie(b.budget_id,b.budget_revision,b.policy_revision,b.approval_fact_id);}
bool identity_equal(const WorldIdentity&a,const WorldIdentity&b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool add(Integer& total,Integer amount){if(amount<0||total>INT64_MAX-amount)return false;total+=amount;return true;}
Status binding_shape(const BudgetExecutionBinding& b,bool require_id=true){if((require_id&&b.id.empty())||b.execution_ref.budget_id.empty()||b.execution_ref.budget_revision<=0||b.execution_ref.policy_revision<0||b.execution_ref.approval_fact_id.empty()||!valid_source_ref(b.responsibility_ref)||b.responsibility_source_schema<1||b.payer_container_id.empty()||b.period.period_from_tick<0||b.period.period_until_tick<=b.period.period_from_tick)return common_failure("InvalidBudgetExecutionBinding",b.id);if(const auto* item=std::get_if<BudgetItemQuantity>(&b.unit);item&&item->item_id.empty())return common_failure("InvalidBudgetUnit",b.id);return common_success();}
Status responsibility_shape(const BudgetExecutionBinding& b,const ActualBudgetResponsibility& r){if(!same_source_ref(b.responsibility_ref,r.responsibility_ref)||b.responsibility_source_schema!=r.source_schema||!unit_equal(b.unit,r.unit)||!period_equal(b.period,r.period))return common_failure("BudgetResponsibilitySourceMismatch",b.id);std::set<std::pair<StableId,StableId>> legs,commitments,releases,refunds;for(const auto& x:r.charges)if(x.owner_domain.empty()||x.obligation_id.empty()||x.actual_leg_id.empty()||x.charged_quantity<=0||!execution_equal(x.execution_ref,b.execution_ref)||!legs.emplace(x.owner_domain,x.actual_leg_id).second)return common_failure("InvalidBudgetCharge",b.id);for(const auto& x:r.commitments)if(x.owner_domain.empty()||x.obligation_id.empty()||x.terms_ref.empty()||x.outstanding_quantity<=0||!execution_equal(x.execution_ref,b.execution_ref)||!commitments.emplace(x.owner_domain,x.obligation_id).second)return common_failure("InvalidBudgetCommitment",b.id);for(const auto& x:r.releases)if(x.owner_domain.empty()||x.obligation_id.empty()||x.result_id.empty()||x.released_quantity<=0||!execution_equal(x.execution_ref,b.execution_ref)||!releases.emplace(x.owner_domain,x.result_id).second)return common_failure("InvalidBudgetRelease",b.id);for(const auto& x:r.refunds)if(x.owner_domain.empty()||x.original_charge_leg_id.empty()||x.refund_leg_id.empty()||x.quantity<=0||!execution_equal(x.execution_ref,b.execution_ref)||!refunds.emplace(x.owner_domain,x.refund_leg_id).second)return common_failure("InvalidBudgetRefund",b.id);return common_success();}
template<class T>std::vector<T> page(const std::vector<T>& all,std::size_t offset,std::size_t limit){if(offset>=all.size())return {};const auto count=std::min(limit,all.size()-offset);return {all.begin()+offset,all.begin()+offset+count};}
}
namespace {
Outcome<BudgetBucketUsageView> project_bucket_original(
 const CommonStore& common,const BudgetExecutionState& state,
 const BudgetAuthorizationRead& permissions,const BudgetResponsibilityRead& sources,
 const BudgetUsageQuery& q,BudgetExecutionPurpose purpose) {
 auto fail=[](const char* code)->Outcome<BudgetBucketUsageView>{
  return {false,{},common_failure(code).error};};
 if(state.schema_version!=2||!identity_equal(common.state().identity,q.expected_identity))
  return fail("BudgetBucketSelectionIdentityMismatch");
 auto selected=validate_budget_execution_purpose(purpose,state.schema_version);
 if(!selected.ok)return {false,{},selected.error};
 auto first=permissions.read_budget_bucket_authorization(q.budget_id,q.expected_budget_revision,
                                                       q.expected_period,q.unit,purpose);
 if(!first.ok)return {false,{},std::move(first.error)};
 BudgetBucketUsageView out;out.authorization=std::move(first.value);
 const auto& root=out.authorization;
 if(!identity_equal(root.identity,q.expected_identity)||root.principal_org_id.empty()||
    root.current_root_ref.budget_id.empty()||root.current_root_ref.budget_revision<1||
    root.current_root_ref.policy_revision<0||root.current_root_ref.approval_fact_id.empty()||
    !period_equal(root.period,q.expected_period)||!unit_equal(root.unit,q.unit)||
    root.purpose!=purpose||root.root_total_cap<0||root.root_purpose_cap<0)
  return fail("BudgetBucketOriginalAuthorizationMismatch");
 std::set<BudgetExecutionPurpose> categories;
 for(auto value:root.root_accounted_purposes){
  auto valid=validate_budget_execution_purpose(value,2);
  if(!valid.ok)return {false,{},valid.error};
  if(!categories.insert(value).second)return fail("DuplicateBudgetBucketPurpose");
 }
 if(!categories.contains(purpose))return fail("BudgetBucketPurposeHistoryMissing");
 std::set<BudgetLegKey> actual_charges,actual_commitments;
 for(auto category:root.root_accounted_purposes){
  auto original=permissions.read_budget_bucket_authorization(q.budget_id,q.expected_budget_revision,
                                                           q.expected_period,q.unit,category);
  if(!original.ok)return {false,{},std::move(original.error)};
  const auto& scope=original.value;
  if(!identity_equal(scope.identity,q.expected_identity)||
     scope.principal_org_id!=root.principal_org_id||
     !execution_equal(scope.current_root_ref,root.current_root_ref)||
     !period_equal(scope.period,root.period)||!unit_equal(scope.unit,root.unit)||
     scope.purpose!=category||scope.root_total_cap!=root.root_total_cap||
     scope.root_purpose_cap<0||(category==purpose&&scope.root_purpose_cap!=root.root_purpose_cap)||
     scope.root_accounted_purposes!=root.root_accounted_purposes)
   return fail("BudgetBucketCategoryScopeMismatch");
  std::set<StableId> members;
  for(const auto& id:scope.current_member_budget_ids)
   if(id.empty()||!members.insert(id).second)return fail("DuplicateBudgetBucketMember");
  if(!members.contains(scope.current_root_ref.budget_id))
   return fail("BudgetBucketRootMembershipMissing");
  std::set<StableId> inherited_members;
  for(const auto& id:scope.inherited_member_budget_ids){
   if(id.empty()||!members.insert(id).second)return fail("DuplicateBudgetBucketMember");
   inherited_members.insert(id);
  }
  if(category==purpose&&!members.contains(q.budget_id))
   return fail("BudgetBucketQueryMembershipMissing");
  Integer gross=0,committed=0,inherited_gross=0,inherited_committed=0;
  for(const auto& binding:state.bindings){
   if(binding.purpose!=category||!members.contains(binding.execution_ref.budget_id)||
      !period_equal(binding.period,q.expected_period)||!unit_equal(binding.unit,q.unit))continue;
   auto actual=sources.read_budget_responsibility(binding);
   if(!actual.ok)return {false,{},std::move(actual.error)};
   auto shape=responsibility_shape(binding,actual.value);
   if(!shape.ok)return {false,{},shape.error};
   for(const auto& charge:actual.value.charges){
    if(!actual_charges.insert(leg_key(binding,charge.owner_domain,charge.actual_leg_id)).second)
     return fail("BudgetBucketChargeAlreadyCounted");
    if(!add(gross,charge.charged_quantity)||
       (inherited_members.contains(binding.execution_ref.budget_id)&&
        !add(inherited_gross,charge.charged_quantity)))return fail("BudgetUsageOverflow");
   }
   for(const auto& debt:actual.value.commitments){
    if(!actual_commitments.insert(leg_key(binding,debt.owner_domain,debt.obligation_id)).second)
     return fail("BudgetBucketCommitmentAlreadyCounted");
    if(!add(committed,debt.outstanding_quantity)||
       (inherited_members.contains(binding.execution_ref.budget_id)&&
        !add(inherited_committed,debt.outstanding_quantity)))return fail("BudgetUsageOverflow");
   }
   // Original release rows explain a commitment's changed outstanding amount.
   // Refund rows describe cash destination; neither subtracts spent gross.
  }
  Integer occupied=gross;if(!add(occupied,committed))return fail("BudgetUsageOverflow");
  if(occupied>scope.root_purpose_cap)return fail("BudgetBucketPurposeCapExceeded");
  if(!add(out.total_spent_gross,gross)||!add(out.total_committed_unpaid,committed))
   return fail("BudgetUsageOverflow");
  if(!add(out.total_inherited_spent_gross,inherited_gross)||
     !add(out.total_inherited_committed_unpaid,inherited_committed))
   return fail("BudgetUsageOverflow");
  if(category==purpose){out.spent_gross=gross;out.committed_unpaid=committed;
   out.inherited_spent_gross=inherited_gross;
   out.inherited_committed_unpaid=inherited_committed;}
 }
 Integer total=out.total_spent_gross;
 if(!add(total,out.total_committed_unpaid))return fail("BudgetUsageOverflow");
 if(total>root.root_total_cap)return fail("BudgetBucketTotalCapExceeded");
 Integer selected_total=out.spent_gross;
 if(!add(selected_total,out.committed_unpaid))return fail("BudgetUsageOverflow");
 out.available_to_commit=std::min(root.root_total_cap-total,root.root_purpose_cap-selected_total);
 return {true,std::move(out),{}};
}
}
Outcome<BudgetBucketUsageView> BudgetUsageRead::read_budget_bucket_usage(
 const BudgetUsageQuery& q,BudgetExecutionPurpose purpose)const {
 // Public current read retains the original leaf/revision proof. Cold history
 // below uses the same pure projection with each real binding's old revision.
 auto leaf=authorization_.read_budget_authorization(q.budget_id,q.expected_budget_revision,
                                                  q.expected_period,q.unit);
 if(!leaf.ok)return {false,{},std::move(leaf.error)};
 if(leaf.value.budget_id!=q.budget_id||leaf.value.budget_revision!=q.expected_budget_revision||
    !period_equal(leaf.value.period,q.expected_period)||!unit_equal(leaf.value.unit,q.unit))
  return {false,{},common_failure("BudgetAuthorizationSourceMismatch").error};
 auto valid=validate_budget_execution(common_,state_,authorization_,responsibilities_,clock_);
 if(!valid.ok)return {false,{},valid.error};
 return project_bucket_original(common_,state_,authorization_,responsibilities_,q,purpose);
}

BudgetUsageRead::BudgetUsageRead(const CommonStore& c,const BudgetExecutionState& s,const BudgetAuthorizationRead& a,const BudgetResponsibilityRead& r,const CurrentMomentRead* clock):common_(c),state_(s),authorization_(a),responsibilities_(r),clock_(clock){}
Outcome<BudgetUsageView> BudgetUsageRead::read_budget_usage(const BudgetUsageQuery& q)const{return project(q,{});}
Outcome<BudgetUsageView> BudgetUsageRead::read_budget_usage_for_purpose(const BudgetUsageQuery& q,BudgetExecutionPurpose purpose)const{return project(q,purpose);}
Outcome<BudgetUsageView> BudgetUsageRead::project(const BudgetUsageQuery& q,std::optional<BudgetExecutionPurpose> purpose)const{
 auto fail=[](const StableId& code)->Outcome<BudgetUsageView>{return {false,{},common_failure(code).error};};if(!identity_equal(common_.state().identity,q.expected_identity)||q.reference_limit<1||q.reference_limit>64)return fail("BudgetUsageReadIdentityMismatch");auto version=state_version(state_.schema_version);if(!version.ok)return {false,{},version.error};if(purpose){auto selected=validate_budget_execution_purpose(*purpose,state_.schema_version);if(!selected.ok)return {false,{},selected.error};}auto permission=authorization_.read_budget_authorization(q.budget_id,q.expected_budget_revision,q.expected_period,q.unit);if(!permission.ok)return {false,{},permission.error};const auto& p=permission.value;if(p.budget_id!=q.budget_id||p.budget_revision!=q.expected_budget_revision||!period_equal(p.period,q.expected_period)||!unit_equal(p.unit,q.unit)||p.retained_cap<0)return fail("BudgetAuthorizationSourceMismatch");
 auto checked=validate_budget_execution(common_,state_,authorization_,responsibilities_,clock_);if(!checked.ok)return {false,{},checked.error};BudgetUsageView result;result.identity=common_.state().identity;result.budget_id=p.budget_id;result.budget_revision=p.budget_revision;result.period_from_tick=p.period.period_from_tick;result.period_until_tick=p.period.period_until_tick;result.authority_ref=p.authority_ref;result.unit=p.unit;result.cap=p.retained_cap;if(purpose){auto cap=authorization_.read_budget_purpose_cap(q.budget_id,q.expected_budget_revision,q.expected_period,q.unit,*purpose);if(!cap.ok)return {false,{},cap.error};if(cap.value){if(*cap.value<0)return fail("InvalidBudgetPurposeCap");result.cap=std::min(result.cap,*cap.value);}}
 for(const auto& b:state_.bindings){if(b.execution_ref.budget_id!=q.budget_id||!period_equal(b.period,q.expected_period)||!unit_equal(b.unit,q.unit)||(purpose&&b.purpose!=*purpose))continue;auto actual=responsibilities_.read_budget_responsibility(b);if(!actual.ok)return {false,{},actual.error};for(const auto& x:actual.value.charges){if(!add(result.spent_gross,x.charged_quantity))return fail("BudgetUsageOverflow");result.charge_refs.push_back(x);}for(const auto& x:actual.value.commitments){if(!add(result.committed_unpaid,x.outstanding_quantity))return fail("BudgetUsageOverflow");result.commitment_refs.push_back(x);}result.release_refs.insert(result.release_refs.end(),actual.value.releases.begin(),actual.value.releases.end());result.refund_refs.insert(result.refund_refs.end(),actual.value.refunds.begin(),actual.value.refunds.end());}
 Integer occupied=result.spent_gross;if(!add(occupied,result.committed_unpaid))return fail("BudgetUsageOverflow");result.available_to_commit=occupied>=result.cap?0:result.cap-occupied;result.charge_refs=page(result.charge_refs,q.reference_offset,q.reference_limit);result.commitment_refs=page(result.commitment_refs,q.reference_offset,q.reference_limit);result.release_refs=page(result.release_refs,q.reference_offset,q.reference_limit);result.refund_refs=page(result.refund_refs,q.reference_offset,q.reference_limit);return {true,std::move(result),{}};
}
Status validate_budget_execution(const CommonStore& common,const BudgetExecutionState& state,const BudgetAuthorizationRead& permissions,const BudgetResponsibilityRead& sources,const CurrentMomentRead* clock){
 auto version=state_version(state.schema_version);if(!version.ok)return version;
 if(state.schema_version==2&&!clock)return common_failure("BudgetExecutionActualCurrentMomentRequired","budget_execution",ErrorKind::unsupported_capability);
 auto now=clock?clock->read_current_moment():Outcome<WorldMoment>{true,{common.tick(),{}},{}};if(!now.ok)return {false,{},now.error};
 std::set<StableId> ids;std::set<std::tuple<StableId,StableId,StableId,Integer,BudgetMeasureKey>> responsibility_ids;std::map<BudgetLegKey,Integer> charges,refund_totals;std::set<BudgetLegKey> commitments,releases,refunds;
 for(const auto& binding:state.bindings){auto checked=binding_shape(binding);if(!checked.ok)return checked;checked=validate_budget_execution_purpose(binding.purpose,state.schema_version);if(!checked.ok)return checked;if(!ids.insert(binding.id).second||!responsibility_ids.emplace(binding.responsibility_ref.domain,binding.responsibility_ref.kind,binding.responsibility_ref.id,binding.responsibility_source_schema,measure_key(binding.unit)).second)return common_failure("DuplicateBudgetResponsibility",binding.id);auto time=compare_world_moments(binding.accepted_at,now.value);if(!time.ok)return {false,{},time.error};if(time.value>0)return common_failure("FutureBudgetExecution",binding.id);checked=permissions.validate_budget_execution_adoption(binding,BudgetExecutionCheck::existing_source);if(!checked.ok)return checked;auto actual=sources.read_budget_responsibility(binding);if(!actual.ok)return {false,{},actual.error};checked=responsibility_shape(binding,actual.value);if(!checked.ok)return checked;
  for(const auto& row:actual.value.charges)if(!charges.emplace(leg_key(binding,row.owner_domain,row.actual_leg_id),row.charged_quantity).second)return common_failure("BudgetChargeAlreadyBound",row.actual_leg_id);
  for(const auto& row:actual.value.commitments)if(!commitments.insert(leg_key(binding,row.owner_domain,row.obligation_id)).second)return common_failure("BudgetCommitmentAlreadyBound",row.obligation_id);
  for(const auto& row:actual.value.releases)if(!releases.insert(leg_key(binding,row.owner_domain,row.result_id)).second)return common_failure("BudgetReleaseAlreadyBound",row.result_id);
  for(const auto& row:actual.value.refunds){if(!refunds.insert(leg_key(binding,row.owner_domain,row.refund_leg_id)).second||!add(refund_totals[leg_key(binding,row.owner_domain,row.original_charge_leg_id)],row.quantity))return common_failure("BudgetRefundAlreadyBound",row.refund_leg_id);}
 }
 for(const auto& row:refund_totals){auto charged=charges.find(row.first);if(charged==charges.end()||row.second>charged->second)return common_failure("BudgetRefundSourceMismatch",std::get<1>(row.first));}
 if(state.schema_version==2){
  // No recursive public usage call: each immutable old binding resolves via
  // S6 original history into the current/root-inherited bucket projection.
  for(const auto& binding:state.bindings){
   auto bucket=project_bucket_original(common,state,permissions,sources,
     {binding.execution_ref.budget_id,binding.execution_ref.budget_revision,
      binding.period,binding.unit,common.state().identity,0,64},binding.purpose);
   if(!bucket.ok)return {false,{},bucket.error};
  }
 }
 return common_success();
}
Outcome<BudgetExecutionBinding> bind_budget_execution_in_candidate(CommonStore& common,BudgetExecutionState& state,const BudgetAuthorizationRead& permissions,const BudgetResponsibilityRead& sources,const BudgetExecutionBinding& proposed,const CurrentMomentRead* clock){
 auto selected=validate_budget_execution_purpose(proposed.purpose,state.schema_version);if(!selected.ok)return {false,{},selected.error};
 if(state.schema_version==2&&!clock)return {false,{},common_failure("BudgetExecutionActualCurrentMomentRequired","budget_execution",ErrorKind::unsupported_capability).error};
 for(const auto& old:state.bindings)if(same_source_ref(old.responsibility_ref,proposed.responsibility_ref)&&old.responsibility_source_schema==proposed.responsibility_source_schema&&unit_equal(old.unit,proposed.unit)){if(!execution_equal(old.execution_ref,proposed.execution_ref)||!period_equal(old.period,proposed.period)||!unit_equal(old.unit,proposed.unit)||old.payer_container_id!=proposed.payer_container_id||old.purpose!=proposed.purpose)return {false,{},common_failure("BudgetResponsibilityReplayMismatch",old.id).error};return {true,old,{}};}
 if(!proposed.id.empty())return {false,{},common_failure("BudgetExecutionIdOwnerRequired",proposed.id).error};BudgetExecutionBinding candidate=proposed;auto checked=binding_shape(candidate,false);if(!checked.ok)return {false,{},checked.error};checked=permissions.validate_budget_execution_adoption(candidate,BudgetExecutionCheck::new_adoption);if(!checked.ok)return {false,{},checked.error};auto actual=sources.read_budget_responsibility(candidate);if(!actual.ok)return {false,{},actual.error};checked=responsibility_shape(candidate,actual.value);if(!checked.ok)return {false,{},checked.error};
 BudgetUsageRead usage(common,state,permissions,sources,clock);auto current=usage.read_budget_usage({candidate.execution_ref.budget_id,candidate.execution_ref.budget_revision,candidate.period,candidate.unit,common.state().identity,0,64});if(!current.ok)return {false,{},current.error};Integer commitment=0;for(const auto& row:actual.value.commitments)if(!add(commitment,row.outstanding_quantity))return {false,{},common_failure("BudgetUsageOverflow").error};for(const auto& row:actual.value.charges)if(!add(commitment,row.charged_quantity))return {false,{},common_failure("BudgetUsageOverflow").error};auto category=usage.read_budget_usage_for_purpose({candidate.execution_ref.budget_id,candidate.execution_ref.budget_revision,candidate.period,candidate.unit,common.state().identity,0,64},candidate.purpose);if(!category.ok)return {false,{},category.error};if(commitment>std::min(current.value.available_to_commit,category.value.available_to_commit))return {false,{},common_failure("BudgetAllowanceExceeded",candidate.execution_ref.budget_id).error};
 if(state.schema_version==2){
  auto bucket=usage.read_budget_bucket_usage(
   {candidate.execution_ref.budget_id,candidate.execution_ref.budget_revision,
    candidate.period,candidate.unit,common.state().identity,0,64},candidate.purpose);
  if(!bucket.ok)return {false,{},bucket.error};
  if(commitment>bucket.value.available_to_commit)
   return {false,{},common_failure("BudgetBucketAllowanceExceeded",candidate.execution_ref.budget_id).error};
  if(bucket.value.authorization.current_root_ref.budget_id==candidate.execution_ref.budget_id){
   // Original leaf retained caps already reserve current child delegations.
   // Inherited liabilities are additional occupied capacity, not a second
   // subtraction of today's descendants' actual spent quantities.
   Integer total_inherited=bucket.value.total_inherited_spent_gross;
   Integer purpose_inherited=bucket.value.inherited_spent_gross;
   if(!add(total_inherited,bucket.value.total_inherited_committed_unpaid)||
      !add(purpose_inherited,bucket.value.inherited_committed_unpaid))
    return {false,{},common_failure("BudgetUsageOverflow").error};
   if(total_inherited>current.value.available_to_commit||
      purpose_inherited>category.value.available_to_commit||
      commitment>current.value.available_to_commit-total_inherited||
      commitment>category.value.available_to_commit-purpose_inherited)
    return {false,{},common_failure("BudgetRootRetainedAllowanceExceeded",candidate.execution_ref.budget_id).error};
  }
 }
 auto id=common.next_id("budget_execution");if(!id.ok)return {false,{},id.error};candidate.id=id.value;state.bindings.push_back(candidate);checked=validate_budget_execution(common,state,permissions,sources,clock);if(!checked.ok)return {false,{},checked.error};return {true,std::move(candidate),{}};
}
}
