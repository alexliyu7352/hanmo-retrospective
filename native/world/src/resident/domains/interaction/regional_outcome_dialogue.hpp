// 公托结果回访只借原TaskState与本地办理人；不生成回执、奖酬或履约事实。
#pragma once
#include "interaction_model.hpp"
namespace hanmo::resident {
struct TaskState;struct TaskCatalog;struct BoundaryValue;class CurrentMomentRead;
// 作者主题必须落到原任务模板及只读对白，不接受任意命令或条件脚本。
Status decode_regional_outcome_catalog(const BoundaryValue&,InteractionCatalog&,const TaskCatalog&);
// 当前本人任务的真实合同与结算来源；不存在的历史字段不能由推测补齐。
Outcome<RegionalTaskOutcomeView> read_regional_task_outcome(const CommonStore&,const TaskState&,const TaskCatalog&,const StableId& observer,const StableId& task_id,const CurrentMomentRead* actual_clock=nullptr);
// 只列当前地点真实在场的原办理人；每项冻结合同结果轮次，旧选择不换单。
Outcome<std::vector<RegionalOpportunityBinding>> resolve_regional_outcome_candidates(const CommonStore&,const TaskState&,const TaskCatalog&,const InteractionCatalog&,const StableId& observer,const StableId& place,const CurrentMomentRead* actual_clock=nullptr);
// 冷引用核验允许已过时会话退出；显示或选择必须要求当前结果摘要精确一致。
Status validate_regional_outcome_source(const CommonStore&,const TaskState&,const TaskCatalog&,const StableId& observer,const RegionalSourceRef&,bool require_current,const CurrentMomentRead* actual_clock=nullptr);
// 文字由当前已绑定合同的实数生成；部分交付或逾期不能被描述成全单成功。
Outcome<StableId> regional_outcome_text(const CommonStore&,const TaskState&,const TaskCatalog&,const RegionalOpportunityBinding&,const CurrentMomentRead* actual_clock=nullptr);
}
