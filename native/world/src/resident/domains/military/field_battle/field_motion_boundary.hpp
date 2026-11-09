// S1 矩形边界的原预测／采用桥；不推进 Field 秒、不借未来时钟。
#pragma once
#include "field_exact_contact_source.hpp"
#include "resident/domains/travel/movement_exact_phase.hpp"
namespace hanmo::resident {
struct PoliticalRelationsReadContext;
class CheckpointMemoryCharge;
// 旁接口仅由实际明选 Field owner 提供；普通个人接触不获得空援军权限。
class MotionFieldBoundaryRead {
public:
  virtual ~MotionFieldBoundaryRead() = default;
  virtual Outcome<std::vector<ExactEventPrediction>> predict_field_boundaries(
      const WorldMoment &, const WorldMoment &, ExactArithmeticBudget) const = 0;
  virtual Status validate_field_boundary_application(
      const ExactEventApplied &, ExactArithmeticBudget) const = 0;
};
// 原运动 writer 仍唯一落支付、消耗和位置；Field 只接原截停后的真实源。
class MotionFieldBoundaryWrite : public MotionFieldBoundaryRead {
public:
  virtual Outcome<ExactEventApplied> apply_field_boundary(
      MotionExactWriteBinding &, const ExactEventPrediction &,
      ExactPhaseWriteContext &, ExactArithmeticBudget) = 0;
};
// 每个真实 movement_unit 只预测其最早场界；多场同刻以原 battle_id 稳定选择。
Outcome<std::vector<ExactEventPrediction>> predict_original_field_boundaries(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const WorldMoment &, const WorldMoment &, ExactArithmeticBudget = {});
// 验源入口同时用于热采用与冷回执，不以 source kind 字符串代替正文。
Status validate_original_field_boundary_application(
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const FieldBattleCatalog &, const ExactEventApplied &,
    ExactArithmeticBudget = {});
// 同一 M 内原 S1 冻结／费用／身体／位置之后安装 Field PendingEntry；
// 本函数不入场，局部 driver 在原完整宏观提交后才消费 entry。
Outcome<ExactEventApplied> apply_original_field_boundary_in_candidate(
    MotionExactWriteBinding &, FieldBattleWorldState &,
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const FieldBattleCatalog &, const ExactEventPrediction &,
    ExactPhaseWriteContext &, CheckpointMemoryCharge &,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
