// 静态现金只认可原目录条款与实际Task终态；收据不是改派货物、免债或重复结酬的授权。
#include "task_static_public_payment.hpp"
#include "task_public_account_phase.hpp"
#include <charconv>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
// 同tick也须沿唯一Common事实顺序；规范实例号不能被前导零或错误前缀冒充。
std::optional<Integer> sequence(const StableId& id,const StableId& prefix,Integer next){
    const auto start=prefix+":";if(id.compare(0,start.size(),start)!=0)return {};const auto tail=id.substr(start.size());Integer result=0;auto parsed=std::from_chars(tail.data(),tail.data()+tail.size(),result);
    if(tail.empty()||parsed.ec!=std::errc{}||parsed.ptr!=tail.data()+tail.size()||result<=0||result>=next||std::to_string(result)!=tail)return {};return result;
}
}
Status validate_task_static_payments(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const TaskPublicAccountExecutionState& state,const PoliticalTakeoverRead& authority,const DynamicStepInput* phase){
    auto observed=task_public_account_observed_tick(common,phase);if(!observed.ok)return {false,{},observed.error};
    if(state.version!=2){if(state.version!=1||!state.static_payments.empty())return common_failure("TaskStaticPaymentProfileRequired");for(const auto& row:common.state().facts.rows())if(row.second.kind==task_static_payment_fact_kind)return common_failure("TaskStaticPaymentProfileRequired",row.first);return common_success();}
    struct Records {const TaskStaticPaymentSource* acceptance=nullptr;const TaskStaticPaymentSource* terminal=nullptr;};
    std::map<StableId,Records> recorded;std::map<StableId,std::size_t> positions;std::set<StableId> actions;std::size_t index=0;
    for(const auto& row:common.state().facts.rows()){positions[row.first]=index++;if(row.second.kind==task_static_payment_fact_kind&&(row.second.domain!="tasks"||!state.static_payments.find(row.first)))return common_failure("MissingTaskStaticPayment",row.first);}
    for(const auto& row:state.static_payments.rows()){
        const auto& payment=row.second;const auto* task=tasks.tasks.find(payment.task_id);const auto* definition=task?catalog.offers.find(task->offer_id):nullptr;auto serial=sequence(row.first,"task_static_payment",common.state().next_instance_id);
        if(row.first!=payment.fact_id||!serial||!task||task->contract_id||!definition||definition->budget<0||definition->reward<0||payment.tick<0||payment.tick>observed.value||!positions.count(row.first))return common_failure("InvalidTaskStaticPaymentSource",row.first);
        auto task_sequence=sequence(task->id,"task",common.state().next_instance_id);if(!task_sequence||*task_sequence>=*serial)return common_failure("InvalidTaskStaticPaymentOrder",row.first);
        const auto* actor=common.actor(task->assignee);if(!actor)return common_failure("UnknownTaskAssignee",task->id);
        auto& receipts=recorded[task->id];const bool acceptance=payment.stage==TaskStaticPaymentStage::acceptance;
        auto*& slot=acceptance?receipts.acceptance:receipts.terminal;if(slot)return common_failure("DuplicateTaskStaticPayment",task->id);slot=&payment;
        if(payment.stage==TaskStaticPaymentStage::cancellation){if(!payment.source_action_id.empty())return common_failure("InvalidTaskStaticPaymentAction",row.first);}
        else{auto action=sequence(payment.source_action_id,"action",common.state().next_instance_id);if(!action||*action>=*serial||!actions.insert(payment.source_action_id).second||(acceptance?*action>=*task_sequence:*action<=*task_sequence))return common_failure("InvalidTaskStaticPaymentAction",row.first);}
        std::map<TaskStaticPaymentPurpose,Integer> amounts;
        for(const auto& leg:payment.legs){
            StableId from,to;switch(leg.purpose){
                case TaskStaticPaymentPurpose::funding:from=definition->funding_wallet;to=task->budget_wallet_id;break;
                case TaskStaticPaymentPurpose::unused_budget_award:from=task->budget_wallet_id;to=actor->wallet_id;break;
                case TaskStaticPaymentPurpose::budget_return:from=task->budget_wallet_id;to=definition->funding_wallet;break;
                case TaskStaticPaymentPurpose::reward_payment:from=definition->funding_wallet;to=actor->wallet_id;break;
                default:return common_failure("InvalidTaskStaticPaymentLeg",row.first);
            }
            if(leg.amount<=0||from==to||leg.from_wallet_id!=from||leg.to_wallet_id!=to||!amounts.emplace(leg.purpose,leg.amount).second)return common_failure("TaskStaticPaymentLegMismatch",row.first);
        }
        const auto take=[&](TaskStaticPaymentPurpose purpose){auto found=amounts.find(purpose);if(found==amounts.end())return Integer{0};auto amount=found->second;amounts.erase(found);return amount;};
        if(acceptance){if(payment.tick!=task->accepted_tick||take(TaskStaticPaymentPurpose::funding)!=definition->budget||!amounts.empty())return common_failure("TaskStaticPaymentFundingMismatch",row.first);}
        else if(payment.stage==TaskStaticPaymentStage::report_success){
            auto remainder=take(TaskStaticPaymentPurpose::unused_budget_award);auto reward=take(TaskStaticPaymentPurpose::reward_payment);auto expected=payment.tick>task->deadline_tick?definition->reward/2:definition->reward;
            if(task->status!="success"||!task->reported_tick||*task->reported_tick!=payment.tick||reward!=expected||remainder>definition->budget||!amounts.empty())return common_failure("TaskStaticPaymentRewardMismatch",row.first);
        }else if(payment.stage==TaskStaticPaymentStage::report_failure){
            auto remainder=take(TaskStaticPaymentPurpose::budget_return);
            if(definition->template_id!="escort"||!task->escort||task->escort->failure.empty()||task->status!="failed"||!task->reported_tick||*task->reported_tick!=payment.tick||remainder>definition->budget||!amounts.empty())return common_failure("TaskStaticPaymentFailureMismatch",row.first);
        }else if(payment.stage==TaskStaticPaymentStage::cancellation){
            auto remainder=take(TaskStaticPaymentPurpose::budget_return);const auto expected=payment.tick>task->deadline_tick?"failed":"cancelled";
            if(task->status!=expected||task->reported_tick||remainder>definition->budget||!amounts.empty())return common_failure("TaskStaticPaymentCancellationMismatch",row.first);
        }else return common_failure("InvalidTaskStaticPaymentStage",row.first);
        auto permission=read_task_static_payment_authority(common,tasks,catalog,authority,payment,phase);if(!permission.ok)return {false,{},permission.error};
    }
    for(const auto& row:tasks.tasks.rows()){
        const auto& task=row.second;if(task.contract_id||!catalog.offers.find(task.offer_id))continue;const auto& receipts=recorded[row.first];const auto* escrow=common.wallet(task.budget_wallet_id);if(!escrow)return common_failure("MissingTaskEscrow",row.first);
        const bool unfinished=task.status=="accepting"||(task.status=="cancelled"&&task.settlement_id.empty()&&task.accepted_tick==0&&task.deadline_tick==0&&escrow->balance==0);
        if(unfinished){if(receipts.acceptance||receipts.terminal)return common_failure("TaskStaticPaymentLifecycleMismatch",row.first);continue;}
        if(!receipts.acceptance)return common_failure("MissingTaskStaticAcceptancePayment",row.first);
        if(task.status=="active"||task.status=="ready"){if(receipts.terminal)return common_failure("TaskStaticPaymentLifecycleMismatch",row.first);continue;}
        if(!receipts.terminal||task.settlement_id.empty()||escrow->balance!=0)return common_failure("MissingTaskStaticSettlementPayment",row.first);
        if(receipts.terminal->tick<receipts.acceptance->tick||positions[receipts.terminal->fact_id]<=positions[receipts.acceptance->fact_id])return common_failure("InvalidTaskStaticPaymentOrder",row.first);
        auto settlement=sequence(task.settlement_id,"settlement",common.state().next_instance_id);auto payment=sequence(receipts.terminal->fact_id,"task_static_payment",common.state().next_instance_id);if(!settlement||!payment||*settlement<=*payment)return common_failure("InvalidTaskStaticSettlementOrder",row.first);
    }
    return common_success();
}
}
