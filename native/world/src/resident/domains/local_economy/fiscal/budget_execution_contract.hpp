// S5唯一预算执行投影；父子cap/批准由S6，付款/实发由原责任owner。
#pragma once
#include "budget_execution_types.hpp"
#include "resident/domains/organization/authority_basis.hpp"
#include "resident/storage/common_store.hpp"
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
class CurrentMomentRead;
struct BudgetUsageView { WorldIdentity identity;StableId budget_id;Integer budget_revision=0,period_from_tick=0,period_until_tick=0;AuthorityBasis authority_ref;BudgetUnit unit;Integer cap=0,spent_gross=0,committed_unpaid=0,available_to_commit=0;std::vector<BudgetChargeRef> charge_refs;std::vector<BudgetCommitmentRef> commitment_refs;std::vector<BudgetReleaseRef> release_refs;std::vector<BudgetRefundRef> refund_refs; };
enum class BudgetExecutionPurpose { transport_expense,contract_purchase,governance_work_expense,public_relief,production_construction,recurring_obligation,material_aid };
// ID由真实责任接受尾分配；PublicBudgetExpenseAuthority.execution_ref严格指本行，非预算ID。
struct BudgetExecutionBinding { StableId id;BudgetExecutionRef execution_ref;SourceRef responsibility_ref;Integer responsibility_source_schema=0;BudgetExecutionPurpose purpose=BudgetExecutionPurpose::transport_expense;BudgetUnit unit;BudgetPeriod period;StableId payer_container_id;WorldMoment accepted_at; };
// 旧无参 API 仍是 /1；新 P1 只有显式 /2 可采用 material_aid。
struct BudgetExecutionState { std::vector<BudgetExecutionBinding> bindings;Integer schema_version=1; };
const char* budget_execution_purpose_key(BudgetExecutionPurpose) noexcept;
Status validate_budget_execution_purpose(BudgetExecutionPurpose,Integer schema_version);
struct BudgetAuthorizationSnapshot { StableId budget_id;Integer budget_revision=0;BudgetPeriod period;AuthorityBasis authority_ref;BudgetUnit unit;Integer retained_cap=0; };
enum class BudgetExecutionCheck { new_adoption,existing_source };
// Transient original S6 history membership, never a persisted amount ledger.
struct BudgetBucketAuthorization {
 WorldIdentity identity;StableId principal_org_id;BudgetExecutionRef current_root_ref;
 BudgetPeriod period;BudgetUnit unit;BudgetExecutionPurpose purpose=BudgetExecutionPurpose::governance_work_expense;
 Integer root_total_cap=0,root_purpose_cap=0;
 std::vector<BudgetExecutionPurpose> root_accounted_purposes;
 std::vector<StableId> current_member_budget_ids,inherited_member_budget_ids;
};
struct BudgetBucketUsageView {
 BudgetBucketAuthorization authorization;
 Integer spent_gross=0,committed_unpaid=0;
 Integer total_spent_gross=0,total_committed_unpaid=0;
 // Historical prior-root share only. Root close adds these to its unchanged
 // leaf liability plus delegated caps, not all descendants twice.
 Integer inherited_spent_gross=0,inherited_committed_unpaid=0;
 Integer total_inherited_spent_gross=0,total_inherited_committed_unpaid=0;
 Integer available_to_commit=0;
};

class BudgetAuthorizationRead {public:virtual ~BudgetAuthorizationRead()=default;virtual Outcome<BudgetAuthorizationSnapshot> read_budget_authorization(const StableId&,Integer expected_revision,const BudgetPeriod&,const BudgetUnit&)const=0;virtual Outcome<std::optional<Integer>> read_budget_purpose_cap(const StableId&,Integer expected_revision,const BudgetPeriod&,const BudgetUnit&,BudgetExecutionPurpose)const=0;virtual Status validate_budget_execution_adoption(const BudgetExecutionBinding&,BudgetExecutionCheck)const=0;
 // /1 callers never use this new policy path. Selected /2 requires an actual
 // political override; unsupported is not a zero-cap or empty-member provider.
 virtual Outcome<BudgetBucketAuthorization> read_budget_bucket_authorization(
  const StableId& id,Integer,const BudgetPeriod&,const BudgetUnit&,BudgetExecutionPurpose)const {
  return {false,{},common_failure("BudgetBucketHistoryUnsupported",id,ErrorKind::unsupported_capability).error};
 }
};
// 原责任provider返回同snapshot完整实际腿；这里只引用结果，不以预约+应付相加。
struct ActualBudgetResponsibility { SourceRef responsibility_ref;Integer source_schema=0;BudgetUnit unit;BudgetPeriod period;std::vector<BudgetChargeRef> charges;std::vector<BudgetCommitmentRef> commitments;std::vector<BudgetReleaseRef> releases;std::vector<BudgetRefundRef> refunds; };
class BudgetResponsibilityRead {public:virtual ~BudgetResponsibilityRead()=default;virtual Outcome<ActualBudgetResponsibility> read_budget_responsibility(const BudgetExecutionBinding&)const=0;};
struct BudgetUsageQuery { StableId budget_id;Integer expected_budget_revision=0;BudgetPeriod expected_period;BudgetUnit unit;WorldIdentity expected_identity;std::size_t reference_offset=0,reference_limit=64; };
class BudgetUsageRead {
public:BudgetUsageRead(const CommonStore&,const BudgetExecutionState&,const BudgetAuthorizationRead&,const BudgetResponsibilityRead&,const CurrentMomentRead* actual_clock=nullptr);
 Outcome<BudgetUsageView> read_budget_usage(const BudgetUsageQuery&)const;
 Outcome<BudgetUsageView> read_budget_usage_for_purpose(const BudgetUsageQuery&,BudgetExecutionPurpose)const;
 Outcome<BudgetBucketUsageView> read_budget_bucket_usage(const BudgetUsageQuery&,BudgetExecutionPurpose)const;
private:Outcome<BudgetUsageView> project(const BudgetUsageQuery&,std::optional<BudgetExecutionPurpose>)const;
const CommonStore& common_;const BudgetExecutionState& state_;const BudgetAuthorizationRead& authorization_;const BudgetResponsibilityRead& responsibilities_;const CurrentMomentRead* clock_=nullptr;
};
Outcome<BudgetExecutionBinding> bind_budget_execution_in_candidate(CommonStore&,BudgetExecutionState&,const BudgetAuthorizationRead&,const BudgetResponsibilityRead&,const BudgetExecutionBinding& accepted_responsibility,const CurrentMomentRead* actual_clock=nullptr);
Status validate_budget_execution(const CommonStore&,const BudgetExecutionState&,const BudgetAuthorizationRead&,const BudgetResponsibilityRead&,const CurrentMomentRead* actual_clock=nullptr);
}
