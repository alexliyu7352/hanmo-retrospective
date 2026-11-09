// Borrowed cold dependency cut: S5's real closed segment must be consumed by
// the original S3 requested service window. No S5 reader is called from here.
#pragma once
#include "work_execution_contract.hpp"
namespace hanmo::resident {
struct WorkSupervisedEconomicWindowSource {
 const WorkEpisode* episode=nullptr;
 const WorkPeriodReceipt2* receipt=nullptr;
 const SupervisionActionStartSource* actual_execution=nullptr;
};
Outcome<WorkSupervisedEconomicWindowSource> read_work_supervised_window_for_economic_source(
 const CommonStore&,const TaskState&,const SourceRef&actual_work_acceptance,
 const MomentOwnerSource&actual_production_segment,const WorldMoment&closed_at);
}
