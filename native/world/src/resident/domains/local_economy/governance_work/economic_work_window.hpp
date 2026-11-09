// S3 may request a real measurement boundary; S5 alone closes its existing input segment.
#pragma once
#include "resident/domains/tasks/work_execution/work_execution_contract.hpp"
namespace hanmo::resident {
struct SupervisedEconomicWindowRequest {
 S3WorkEpisodeRef work;SourceRef work_acceptance_ref;MomentOwnerSource actual_action_execution;
 MomentSpan window;
};
// Pure verification uses the existing accepted Work and its original Action source.
// No WorkPeriodReceipt or guessed positive credit is needed before the S5 close.
Status validate_supervised_economic_window_request(const CommonStore&,const WorkExecutionRead&,const WorkOriginalRead&,const SupervisedEconomicWindowRequest&,const WorldMoment& actual_now);
Outcome<EconomicWorkWindowRead2> close_supervised_economic_window_in_candidate(CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,EconomicProductionContext&,const WorkExecutionRead&,const WorkOriginalRead&,const SupervisedEconomicWindowRequest&);
// 本次原 close 后同步调用；两个借值只在该调用栈有效，不产生可保存的豁免凭证。
// 消费者须在返回前真正写入 S3 period，随后本入口恢复普通 cold 门核全覆盖。
class AppliedSupervisedEconomicWindowConsumer {public:virtual ~AppliedSupervisedEconomicWindowConsumer()=default;
 virtual Outcome<StableId> consume_applied_window(const EconomicWorkWindowRead2&,const ProductionInputSourceRead& accepted_window_sources)=0;
};
Outcome<StableId> close_and_consume_supervised_economic_window_in_candidate(CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,EconomicProductionContext&,const WorkExecutionRead&,const WorkOriginalRead&,const SupervisedEconomicWindowRequest&,AppliedSupervisedEconomicWindowConsumer&);
}
