// 私铺关店只问原两容器责任；旧任务没有永久终结证明时，当前与历史一律不推定免责。
#pragma once
#include "task_shop_order_execution.hpp"
namespace hanmo::resident {
struct TaskPrivateShopAssetScope {StableId proprietor_actor_id,shop_id,wallet_id,inventory_id;};
struct TaskPrivateShopAssetResponsibilities {
    WorldIdentity source_identity;TaskPrivateShopAssetScope scope;
    Integer as_of_tick=0;StableId closing_fact_id;std::vector<StableId> pending_source_ids;
};
// 前提是同World完整旧Task/各sidecar已冷验；本口不反调Shop，完整店单来源由S5先验。
Outcome<TaskPrivateShopAssetResponsibilities> read_task_private_shop_asset_responsibilities(
    const CommonStore&,const TaskState&,const TaskCatalog&,const TaskShopOrderState*,const TaskPrivateShopAssetScope&);
// S5先核真正关闭载荷中的shop/本人/两容器/tick等式；这里只认具名private_shop_closed头和事实截面。
Outcome<TaskPrivateShopAssetResponsibilities> read_task_private_shop_asset_responsibilities_before_close(
    const CommonStore&,const TaskState&,const TaskCatalog&,const TaskShopOrderState*,const TaskPrivateShopAssetScope&,
    const StableId& closing_fact_id,Integer tick);
// S5只借此同候选纯读；接口不提供改Task、放预约或伪造已清来源的能力。
class TaskPrivateShopAssetResponsibilityRead {
public:
    virtual ~TaskPrivateShopAssetResponsibilityRead()=default;
    virtual Outcome<TaskPrivateShopAssetResponsibilities> current(const TaskPrivateShopAssetScope&)const=0;
    virtual Outcome<TaskPrivateShopAssetResponsibilities> before_close(const TaskPrivateShopAssetScope&,const StableId& fact,Integer tick)const=0;
};
class TaskPrivateShopAssetResponsibilityAccess final:public TaskPrivateShopAssetResponsibilityRead {
public:
    TaskPrivateShopAssetResponsibilityAccess(const CommonStore& c,const TaskState& t,const TaskCatalog& d,const TaskShopOrderState* s):common_(c),tasks_(t),catalog_(d),orders_(s){}
    Outcome<TaskPrivateShopAssetResponsibilities> current(const TaskPrivateShopAssetScope& scope)const override{return read_task_private_shop_asset_responsibilities(common_,tasks_,catalog_,orders_,scope);}
    Outcome<TaskPrivateShopAssetResponsibilities> before_close(const TaskPrivateShopAssetScope& scope,const StableId& fact,Integer tick)const override{return read_task_private_shop_asset_responsibilities_before_close(common_,tasks_,catalog_,orders_,scope,fact,tick);}
private:const CommonStore& common_;const TaskState& tasks_;const TaskCatalog& catalog_;const TaskShopOrderState* orders_;
};
}
