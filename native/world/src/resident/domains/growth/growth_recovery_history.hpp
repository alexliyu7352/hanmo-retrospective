// 休养只沿原身份/伤因/实际服务与逐期恢复源；不由当前伤势或旧Action消失补造康复。
#pragma once
#include "growth_recovery_model.hpp"
#include "growth_skill_origin.hpp"
#include "resident/domains/family/family_death.hpp"
namespace hanmo::resident {
class GrowthConditionWriteServices;
struct GrowthRecoveryHistoryAccess {
    const GrowthSkillOriginContext& origins;
    const GrowthInjurySourceRead& injuries;
    // 原Military死因允许严格同一步；后来Family登记不能把死因后窗口当作仍活着。
    std::function<Outcome<std::optional<FamilyDeathOrigin>>(const StableId&,const DynamicStepInput*)> death_source;
    // 实际住宿安全/医馆资格及在场来源，按真实credit边界；不拿今日角色/资格倒填历史。
    std::function<Status(const GrowthRecoveryService&,Integer,const StableId&)> service_qualified_before;
    // 开始/继续热准入用同候选当前源，不以未来尚未生成的credit假头调用历史口。
    std::function<Status(const GrowthRecoveryService&)> service_qualified_now;
    // 由同候选外层自持；原Recovery写者同步调用，绝不保存进恢复账。
    GrowthConditionWriteServices* condition_writer=nullptr;
};
Status validate_growth_recovery_history(const CommonStore&,const GrowthState&,const GrowthRecoveryCatalog&,const GrowthRecoveryState&,const GrowthRecoveryHistoryAccess&,const DynamicStepInput* phase=nullptr);
// S2先核自身actual military_recovery载荷tick；缺伤因/服务全史报错，尚未康复返回empty。
Outcome<std::optional<GrowthRecoveredActor>> read_growth_recovered_actor(const CommonStore&,const GrowthState&,const GrowthRecoveryCatalog&,const GrowthRecoveryState&,const GrowthRecoveryHistoryAccess&,const StableId& actor_id,const StableId& original_casualty_id,Integer observed_tick,const StableId& actual_before_fact_id={},const DynamicStepInput* phase=nullptr);
// 内部热步骤读取本人当前已解释进度；同候选其它业务可以仍在推进，不发布或补写状态。
Outcome<GrowthRecoveryProgress> read_growth_recovery_progress_in_candidate(const CommonStore&,const GrowthState&,const GrowthRecoveryCatalog&,const GrowthRecoveryState&,const GrowthRecoveryHistoryAccess&,const StableId& actor,const DynamicStepInput* phase=nullptr);
// 原服务来源必须永久保留，取消/全疗程结束也不能剪去被本账引用的原life_activity。
Outcome<std::vector<StableId>> growth_recovery_life_pins(const CommonStore&,const GrowthState&,const GrowthRecoveryCatalog&,const GrowthRecoveryState&);
}
