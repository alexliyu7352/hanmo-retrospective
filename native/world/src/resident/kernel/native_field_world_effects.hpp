// 同一 Field 段的原人物后果采用；不发布世界，不重放旧段或替代完整 driver。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
class CompleteExactWorldBindingOwner;
struct ExactPhaseWriteContext;
struct SourceRef;
// 仅消费本次真实 segment 的具名 wounded；原伤效写者返回的捕获不得丢弃。
Status apply_native_field_segment_injuries_in_candidate(
    WorldBundle &, CompleteExactWorldBindingOwner &, const SourceRef &segment,
    ExactPhaseWriteContext &);
// 先写真实看管期限，再单写身体并立即采用原 S1 成员损失；不伪造可用性。
Status apply_native_field_segment_captivity_in_candidate(
    WorldBundle &, CompleteExactWorldBindingOwner &, const SourceRef &segment,
    ExactPhaseWriteContext &);
// 原 Life4 写者及其真实能力／照护后果完成后，采用同捕获的 S1 死亡离组。
Status apply_native_field_segment_deaths_in_candidate(
    WorldBundle &, CompleteExactWorldBindingOwner &, const SourceRef &segment,
    ExactPhaseWriteContext &);
// 逃散使用独立原 outcome/tag，实际退出点与两次捕获必须同候选齐备。
Status apply_native_field_segment_desertions_in_candidate(
    WorldBundle &, CompleteExactWorldBindingOwner &, const SourceRef &segment,
    ExactPhaseWriteContext &);
// 原汇合step的普通兵截止后果：真名册分类后逐具名人物新读票、一次S4/S1捕获。
Status apply_native_field_army_exit_ordinary_losses_in_candidate(
    WorldBundle &, CompleteExactWorldBindingOwner &, const SourceRef &actual_step,
    ExactPhaseWriteContext &);
} // namespace hanmo::resident
