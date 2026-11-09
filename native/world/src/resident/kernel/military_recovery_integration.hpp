// Military8在原完整World候选中借人物纯读；不持有服务指针跨过回滚或阶段结束。
#pragma once
#include "resident/domains/military/military_recovery_actor_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 由原military_supply成功后同步调用；此步已经支付的军粮不再消费第二次。
// 原半日供给后只改变部队疲劳，原钱粮/伤员回队保持各自唯一结算。
Status step_military_garrison_rest_world(WorldBundle&,const DynamicStepInput&);
Status validate_military_garrison_rest_world(const WorldBundle&);
Status step_military_wounded_recovery_world(WorldBundle&,const DynamicStepInput&);
// 必须位于S4完整个人康复历史之后，作为同候选发布前不可省略的具名晚门。
Status validate_military_wounded_recovery_world_sources(const WorldBundle&);
}
