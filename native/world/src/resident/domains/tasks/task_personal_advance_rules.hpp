// 垫款报价/支付复用原Task与Common候选；新模块未登记前不得从公共命令调用。
#pragma once
#include "task_personal_advance.hpp"
#include "task_model.hpp"
namespace hanmo::resident {
struct OrganizationState;
Status validate_task_personal_advance_policy(const TaskPersonalAdvancePolicy&);
Status validate_task_personal_advance_state(const CommonStore&,const TaskState&,const TaskPersonalAdvanceState&);
Outcome<TaskPersonalAdvanceTerms> freeze_task_personal_advance_terms(
    const CommonStore&,const TaskState&,const StableId& contract_id,const TaskPersonalAdvancePolicy&);
Outcome<TaskPersonalAdvanceSummary> read_task_personal_advance_summary(
    const CommonStore&,const TaskState&,const TaskPersonalAdvanceState&,const StableId& contract_id,const DynamicStepInput* original_calendar_phase=nullptr);
// 其它域只借真实收据读口，不复制本金/偿还状态；引用与同候选共同存续。
class TaskPersonalAdvanceAccess final:public TaskPersonalAdvanceRead {
public:
    TaskPersonalAdvanceAccess(const CommonStore& common,const TaskState& tasks,const TaskPersonalAdvanceState& state,const DynamicStepInput* phase=nullptr):common_(common),tasks_(tasks),state_(state),phase_(phase){}
    Outcome<TaskPersonalAdvanceSummary> task_personal_advance_source(const StableId& id)const override{return read_task_personal_advance_summary(common_,tasks_,state_,id,phase_);}
private:
    const CommonStore& common_;const TaskState& tasks_;const TaskPersonalAdvanceState& state_;const DynamicStepInput* phase_;
};
Outcome<TaskPersonalAdvanceQuote> preview_task_personal_advance(
    const CommonStore&,const TaskState&,const TaskCatalog&,const TaskPersonalAdvanceState&,const StableId& actor_id,
    const StableId& task_id,Integer amount);
// 调用者持整World候选；任何后继失败必须连同Common及本域state一并丢弃，不支持局部发布。
Outcome<StableId> pay_task_personal_advance_in_candidate(
    CommonStore&,const TaskState&,const TaskCatalog&,TaskPersonalAdvanceState&,const StableId& actor_id,
    const StableId& task_id,Integer amount,const TaskPersonalAdvanceQuote&);
// reward_reserved仍由原Contract计算；本口只拆真实可退现金，不能挪用报酬预留。
Outcome<TaskAdvanceSettlementAmounts> calculate_task_advance_settlement(
    Integer escrow_balance,Integer reward_reserved,Integer original_budget_remaining,Integer principal_due);
// 已发布边界或原S0日历阶段，按原冻结主体一次归还全部本金；不修改报酬/预算退款账。
// 组织空缺/受限不抹旧债；解散、私人死亡或容器转属明确待合法接管，不用当前Owner改派。
Outcome<TaskAdvanceRepaymentAttempt> repay_task_personal_advance_in_candidate(
    CommonStore&,const TaskState&,TaskPersonalAdvanceState&,const OrganizationState*,const StableId& contract_id,const DynamicStepInput* original_calendar_phase=nullptr);
}
