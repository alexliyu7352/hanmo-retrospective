#pragma once
#include "growth_permanent_contributions.hpp"
#include "resident/domains/tasks/work_execution/work_execution_contract.hpp"
namespace hanmo::resident {
struct TaskState;struct ActionElapsedStore;
class NativeWorkPracticeContributionRead final:public WorkPracticeContributionRead {
 const CommonStore& common_;const TaskState& tasks_;
public:NativeWorkPracticeContributionRead(const CommonStore& c,const TaskState& t):common_(c),tasks_(t){}
 bool bound_to(const CommonStore& c)const noexcept override{return &c==&common_;}
 Outcome<WorkPracticeContributionView> read_actual_work_contribution(const StableId&,const WorkPracticeContribution&,const WorldMoment&)const override;
};
}

namespace hanmo::resident {
// Invoked once from the real S3 completed WorkResult owner tail. Author rules
// are selected from the same locked catalogue; no award request comes from UI.
Status apply_selected_work_growth_in_candidate(CommonStore&,GrowthState&,
 GrowthPermanentState&,const GrowthPermanentSources&,const TaskState&,
 ActionElapsedStore&,const WorkResultSource&,GrowthCapabilityRevisionWrite&);
}

namespace hanmo::resident {
class NativeWorkCompletedGrowthConsumer final:public WorkCompletedResultConsumer {
 CommonStore& common_;GrowthState& growth_;GrowthPermanentState& permanent_;
 const GrowthPermanentSources& sources_;const TaskState& tasks_;ActionElapsedStore& elapsed_;
 GrowthCapabilityRevisionWrite& capability_;
public:
 NativeWorkCompletedGrowthConsumer(CommonStore& c,GrowthState& g,GrowthPermanentState& p,const GrowthPermanentSources& s,const TaskState& t,ActionElapsedStore& e,GrowthCapabilityRevisionWrite& w):common_(c),growth_(g),permanent_(p),sources_(s),tasks_(t),elapsed_(e),capability_(w){}
 bool bound_to(const CommonStore&,const TaskState&)const noexcept override;
 Status consume_completed_result(const WorkResultSource&,const WorldMoment&)override;
};
}
