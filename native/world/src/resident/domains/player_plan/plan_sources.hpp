// 有限来源仅从玩家当前公开页面及本人合同派生；真实接受仍走原命令报价。
#pragma once
#include "plan_model.hpp"
#include "resident/domains/growth/growth_refs.hpp"
namespace hanmo::resident {
struct WorldBundle;struct CommandPayload;
// 保存实质条款而非当前疲劳、经验、余额等计数；不生成题目或提前付款。
Outcome<PlayerPlanTerms> freeze_player_plan_terms(const GrowthQuote&,const StableId& private_wallet_id);
Status match_player_plan_terms(const PlayerPlanTerms&,const GrowthQuote&,const StableId& private_wallet_id);
// 客户端只能选择闭合目标和授权位，来源和条款必须由原世界读取产生。
Outcome<PlayerPlanAppend> prepare_player_plan_source(const WorldBundle&,const StableId& actor_id,const PlayerPlanStepData&,bool authorized=false);
// 仅将固定目标转换为原命令；提醒和继续不能冒充新领域命令。
Outcome<CommandPayload> player_plan_domain_command(const PlayerPlanStepData&);
// 已存来源在启动时逐项重核身份，不刷新远地报价或替换提供者。
Status validate_player_plan_source(const WorldBundle&,const StableId& actor_id,const PlayerPlanStep&);
} // 不持世界副本，不预留库存，不调用提交或修改任何原领域状态。
