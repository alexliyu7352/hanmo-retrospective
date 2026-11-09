#include "governance_approval_authority.hpp"
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const StableId& code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{value.error};return std::move(value.value);}
template<class T,class F>Outcome<T> guard(F&& fn){try{return {true,fn(),{}};}catch(const Failure& e){return {false,{},e.error};}}
BudgetAuthorizationRef budget_source(const CommonStore& common,const BudgetExecutionRef& ref){
 const auto* fact=common.state().facts.find(ref.approval_fact_id);
 if(!fact||ref.budget_id.empty()||ref.budget_revision<1||ref.policy_revision<1)
  fail("GovernanceBudgetSourceMissing",ref.approval_fact_id);
 return {ref.budget_id,ref.budget_revision,{fact->domain,fact->kind,fact->id}};
}
}
Outcome<GovernanceApprovalAuthorityCapture> prepare_governance_approval_authority(
 const GovernanceProgramReadContext& x,const GovernanceProgramSpec& spec,const GovernanceExecutor& executor,
 const ActingAuthority& issuer,const BudgetExecutionRef& budget,Integer revision,const ReadStamp& stamp){
 return guard<GovernanceApprovalAuthorityCapture>([&]{
  if(x.catalog.specs.find(spec.spec_ref.id)!=&spec)fail("GovernanceProgramSpecChanged",spec.spec_ref.id);
  take(validate_governance_program_catalog(x.catalog));
  auto target=take(x.originals.validate_governance_target_for_approval(spec,executor,budget,revision,stamp));
  SingleGrantAuthorityRequest request{issuer,"approve_governance_program",spec.scope,0,target.authority_resources,false,{}};
  auto proof=take(check_current_grant_authority(x.common,x.organization,x.organization_catalog,
   x.catalog.identity,request,&x.clock));
  if(x.catalog.schema_version>=2)
   take(validate_governance_continuity_policy_binding(x.catalog,spec,proof.effective_grant.continuity_policy_id));
  return GovernanceApprovalAuthorityCapture{std::move(target),std::move(request),std::move(proof),budget_source(x.common,budget)};
 });
}
Status bind_governance_approval_authority_in_candidate(CommonStore& common,OrganizationRuntime& runtime,
 const GovernanceProgramReadContext& x,const GovernanceApprovalAuthorityCapture& capture,const SourceRef& source){
 if(&common!=&x.common||&runtime!=&x.organization_runtime)return common_failure("GovernanceProgramCandidateMismatch");
 NativeGovernanceProgramAuthorityRead reader(x);
 auto value=register_authority_basis_for_fact(common,runtime.authority_bases,x.organization,
  x.organization_catalog,x.catalog.identity,capture.request,capture.proof,source,reader,capture.budget,&x.budgets);
 return value.ok?common_success():Status{false,{},value.error};
}
Status validate_governance_approval_authority(const GovernanceProgramReadContext& x,
 const GovernanceProgramSpec& spec,const ActingAuthority& issuer,const AuthorityBasisRef& ref,
 const BudgetExecutionRef& budget,const SourceRef& source){
 return guard<Done>([&]{
  if(ref.source_schema!=1||!same_source_ref(ref.issued_source,source))fail("GovernanceProgramAuthorityMismatch",source.id);
  NativeGovernanceProgramAuthorityRead reader(x);
  auto basis=take(read_registered_authority_basis(x.common,x.organization_runtime.authority_bases,x.organization,
   x.organization_catalog,x.catalog.identity,ref,reader,&x.budgets));
  if(x.catalog.schema_version>=2)
   take(validate_governance_continuity_policy_binding(x.catalog,spec,basis->continuity_policy_id));
  const auto expected=budget_source(x.common,budget);
  if(basis->actor_id!=issuer.actor_id||basis->office_id!=issuer.office_id||
     basis->organization_id!=issuer.organization_id||basis->office_epoch!=issuer.authority_epoch||
     basis->command_kind!="approve_governance_program"||basis->scope.kind!=spec.scope.kind||
     basis->scope.id!=spec.scope.id||!basis->budget_ref||
     basis->budget_ref->authorization_id!=expected.authorization_id||
     basis->budget_ref->authorization_revision!=expected.authorization_revision||
     !same_source_ref(basis->budget_ref->approval_source,expected.approval_source))
   fail("GovernanceProgramAuthorityMismatch",source.id);
  return Done{};
 });
}
}
