// 原finite_return只借真实终局/退出采用；此接口不持久、不产生第二军令或新钱包。
#pragma once
#include "military_march_contract.hpp"
#include "resident/domains/travel/movement_exact_model.hpp"
namespace hanmo::resident {
class CommonStore;
struct MilitaryState;
struct SpatialCarrierState;
struct GroupMovementExecutionState;
struct MilitaryFiniteReturnOriginalRead {
  SourceRef cause, previous_opening;
  StableId army_id, destination_settlement_id, return_garrison_id;
  // 原cause读取为ready几何应用M；actual-continuation读取为真实S1采用M，二者不可回填。
  WorldMoment adopted_at;
  MovementAnchor actual_anchor;
  std::vector<StableId> named_participants;
};
class MilitaryFiniteReturnSourceRead {
public:
  virtual ~MilitaryFiniteReturnSourceRead() = default;
  virtual bool bound_to(const CommonStore &, const MilitaryState &,
      const SpatialCarrierState &, const MovementExactState &,
      const GroupMovementExecutionState &) const noexcept = 0;
  virtual Outcome<MilitaryFiniteReturnOriginalRead> read_original_finite_return(
      const SourceRef &actual_cause) const = 0;
  // 仅实际已采用的新opening或严格同目的地stable取消；不把裸ready当成员已退出。
  virtual Outcome<MilitaryFiniteReturnOriginalRead> read_actual_finite_return_continuation(
      const SourceRef &actual_continuation) const = 0;
};
} // namespace hanmo::resident
