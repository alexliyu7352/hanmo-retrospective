#pragma once
#include "task_model.hpp"
namespace hanmo::resident {
class WorkTaskRead;struct WorkCommissionTerminalContext;struct ReadStamp;
struct GovernanceTaskState;struct EnvoyTaskState;class DiplomaticMissionRead;class DiplomacyPresentationRead;class DiplomaticEnvoyReportRead;
struct TaskStateNeedOtherResponsibilities {const GovernanceTaskState*governance=nullptr;const EnvoyTaskState*envoy=nullptr;const DiplomaticMissionRead*missions=nullptr;const DiplomacyPresentationRead*presentations=nullptr;const DiplomaticEnvoyReportRead*reports=nullptr;TaskBattleRead battles;const PoliticalTakeoverRead*authority=nullptr;};
struct TaskWorkValidationSources {
 const WorkTaskRead& definitions;const WorkOriginalRead& originals;const CurrentMomentRead& clock;
 const WorkCommissionTerminalContext* terminals=nullptr;const ReadStamp* terminal_stamp=nullptr;
};
Status validate_tasks_with_complete_responsibilities(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const AffairsState*,const StateTaskSourceRead*,const TaskStateNeedOtherResponsibilities&,const TaskWorkValidationSources*,const CurrentMomentRead* actual_clock=nullptr,const TaskExactLifeSources* life=nullptr);
Status validate_tasks_with_state_need_responsibilities(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const AffairsState&,const StateTaskSourceRead&,const TaskStateNeedOtherResponsibilities&);
}
