// S5 自然资源的原地存量与日历恢复；未采出量从不进入 Common 库存。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/actor_foundation/model.hpp"
#include "resident/model/dynamic_contract.hpp"

namespace hanmo::resident {
// 固定半开季节段只选有理率分子，整条规则始终使用同一个分母。
struct ResourceRecoverySeasonRow {
  StableId season_id;
  Integer from_tick = 0, until_tick = 0, numerator = 0;
};
enum class ResourceDisabledRecoveryPolicy {
  reset_on_disabled,
  retain_fraction_on_disabled
};
struct ResourceRecoveryRule {
  ActorFoundationRuleRef rule_ref;
  Integer denominator = 1, base_numerator = 0;
  CheckpointVector<ResourceRecoverySeasonRow> season_rows;
  ResourceDisabledRecoveryPolicy disabled_policy =
      ResourceDisabledRecoveryPolicy::reset_on_disabled;
};
// 修正只由原采集收据采用，作用窗不能反向影响已经跑过的恢复。
struct ResourceDepletionModifier {
  StableId modifier_id, source_gather_receipt;
  Integer effective_from = 0, until_exclusive = 0;
  ActorFoundationRuleRef rule_ref;
  Integer numerator_adjustment = 0;
};
struct ResourceSiteState {
  StableId site_id;
  Integer remaining_units = 0, last_recovery_tick = 0, rate_remainder = 0;
  CheckpointVector<StableId> active_depletion_modifier_refs;
  Integer revision = 0;
  ActorFoundationRuleRef recovery_rule_ref;
  Integer recovery_denominator = 1;
};
// 实际 preflight 冻结 from 时的季节及有效修正，不能在阶段尾换成 to 时输入。
struct ResourceRecoveryOpening {
  StableId site_id;
  Integer from_tick = 0, to_tick = 0;
  ActorFoundationRuleRef rule_ref;
  StableId season_id;
  CheckpointVector<StableId> modifier_source_refs;
  Integer combined_numerator = 0, denominator = 1;
};
// 恢复收据仅描述原地变化；容量截断部分不能被当成货或未来增长额度。
struct ResourceRecoveryReceipt {
  StableId site_id;
  Integer from_tick = 0, to_tick = 0;
  ActorFoundationRuleRef rule_ref;
  StableId season_id;
  CheckpointVector<StableId> modifier_source_refs;
  Integer standing_before = 0, standing_after = 0, recovered_units = 0;
  Integer discarded_whole_units = 0, remainder_before = 0, remainder_after = 0;
  Integer denominator = 1;
  CheckpointVector<MomentOwnerSource> state_source_refs;
};
// 阶段当前的停用决定必须带原来源；一般封路不自动成为资源停用。
struct ResourceRecoveryDisposition {
  bool enabled = true;
  CheckpointVector<MomentOwnerSource> original_sources;
};
struct ResourceRecoveryApplication {
  bool already_processed = false;
  std::optional<ResourceRecoveryReceipt> receipt;
};

// 纯结构门；修正来源的业务真实性由拥有 GatherReceipt 的原资源 owner 再核。
Status validate_resource_recovery_rule(const ResourceRecoveryRule &);
Status validate_resource_site_recovery_state(const ResourceSiteState &,
                                             Integer capacity_units,
                                             const ResourceRecoveryRule &);
// 参数必须来自同一已锁资源点和其原修正表；本函数不写状态或取得 ID。
Outcome<ResourceRecoveryOpening> prepare_resource_recovery_opening(
    const ResourceSiteState &, Integer capacity_units,
    const ResourceRecoveryRule &,
    const CheckpointVector<ResourceDepletionModifier> &,
    const DynamicStepInput &);
// 纯有理运算，先核全部输入再返回收据；不改变真实存量。
Outcome<ResourceRecoveryReceipt>
preview_resource_recovery(const ResourceSiteState &, Integer capacity_units,
                          const ResourceRecoveryRule &,
                          const ResourceRecoveryOpening &,
                          const ResourceRecoveryDisposition &);
// 原阶段私有候选才调用；不能由观察、存档或恢复入口补跑。
// 相同 to 只回已处理，其余乱序拒；修改前完整预演，失败保原状态。
Outcome<ResourceRecoveryApplication> apply_resource_recovery_in_candidate(
    ResourceSiteState &, Integer capacity_units, const ResourceRecoveryRule &,
    const ResourceRecoveryOpening &, const ResourceRecoveryDisposition &);
} // namespace hanmo::resident
