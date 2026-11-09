// Dependency cut for S4's original Practice consumer. This is a borrowed
// projection of the original immutable Work sources, not a second award ledger.
#pragma once
#include "work_execution_contract.hpp"
#include <span>
namespace hanmo::resident {
struct WorkGrowthContributionSource {
 const WorkResult* result=nullptr;const WorkEpisode* episode=nullptr;
 const FactRecord* original_result=nullptr;
 const WorkPeriodReceipt2* last_actual_period=nullptr;
 Integer actor_credited_half_days=0;
};
// participation_refs must be the complete original-order set of this actor's
// positively credited leader receipts. No unrecorded member/observer is inferred.
// This source leg validates the retained producer rows and their causal links.
// Full Work validation remains mandatory independently; it is not recursively
// invoked from the skill chain, which may itself cite this completed result.
Outcome<WorkGrowthContributionSource> read_work_growth_contribution_source(
 const CommonStore&,const TaskState&,const StableId& actor,const WorkResultSource&,
 std::span<const StableId> participation_refs,const WorldMoment& actual_now);
class WorkGrowthContributorVisitor {public:virtual ~WorkGrowthContributorVisitor()=default;
 // References are borrowed for this callback only, in original receipt order.
 virtual Status visit(const StableId&actor,std::span<const StableId> all_positive_leader_periods)=0;
};
Status visit_work_growth_contributors(const CommonStore&,const TaskState&,
 const WorkResultSource&,const WorldMoment&actual_now,WorkGrowthContributorVisitor&);
}
