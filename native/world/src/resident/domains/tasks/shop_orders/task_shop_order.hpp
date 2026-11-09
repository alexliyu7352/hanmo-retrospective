// 店单是具名已接受Task责任，原客户当面准入由S5提供；这里不伪造普通Offer/Need或第二份余额。
#pragma once
#include "task_shop_order_state.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include <functional>
namespace hanmo::resident {
using TaskShopOrderAcceptanceMeeting=std::function<Status(const StableId& proprietor,const StableId& order_id)>;
struct TaskShopOrderAcceptQuote {WorldIdentity source_identity;StableId actor_id,order_issue_fact_id;PrivateShopOrderTerms terms;};
bool task_shop_order_terms_equal(const PrivateShopOrderTerms&,const PrivateShopOrderTerms&);
// 只验本域真实接受及Task，不回调Shop账；S5绑定中间态可以安全借用。
Status validate_task_shop_order_acceptances(const CommonStore&,const TaskState&,const TaskShopOrderState&);
class TaskShopOrderAcceptanceAccess final:public TaskShopOrderAcceptanceRead {
public:
    TaskShopOrderAcceptanceAccess(const CommonStore& c,const TaskState& t,const TaskShopOrderState& s):common_(c),tasks_(t),state_(s){}
    Outcome<std::optional<TaskShopOrderAcceptanceSource>> task_shop_order_acceptance(const StableId&)const override;
private:const CommonStore& common_;const TaskState& tasks_;const TaskShopOrderState& state_;
};
Outcome<TaskShopOrderAcceptQuote> preview_task_shop_order_acceptance(const CommonStore&,const TaskState&,const TaskShopOrderState&,const TaskCatalog&,const CommonCatalog&,const PrivateShopOrderRead&,const TaskShopOrderAcceptanceMeeting&,const StableId& actor_id,const StableId& order_id);
// 外层须共同保存Common/Task/Shop；接受和S5唯一绑定成功后才做整World冷验并发布。
Outcome<TaskShopOrderAcceptanceSource> accept_task_shop_order_in_candidate(CommonStore&,TaskState&,TaskShopOrderState&,const TaskCatalog&,const CommonCatalog&,const PrivateShopOrderRead&,const TaskShopOrderAcceptanceMeeting&,const TaskShopOrderAcceptQuote&);
}
