// Original S2 captured/deserted consequences, applied by S4 only.
#pragma once
#include "condition_capture.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct MilitaryAvailabilityApplication {
  // 新具名采用返回的普通字段先收费，最后释放；原调用签名保留。
  CheckpointCopyLease payload_memory;
  StableId transition_id;
  std::vector<BoundActorAvailabilityWrite> actual_writes;
  bool already_applied = false;
  MilitaryAvailabilityApplication() = default;
  MilitaryAvailabilityApplication(
      StableId id, std::vector<BoundActorAvailabilityWrite> writes, bool prior)
      : transition_id(std::move(id)), actual_writes(std::move(writes)),
        already_applied(prior) {}
};
Outcome<MilitaryAvailabilityApplication> apply_verified_military_availability(
    CommonStore &, AuthoredActorFoundationState &,
    const CharacterConditionSources &, const SourceRef &,
    Integer source_ordinal, const ReadStamp &, const ReadStampAuthority &,
    const DomainReadVersionSource &);
Status
validate_military_availability_coverage(const CharacterConditionSources &);
Outcome<const CapacityTransitionV2 *>
read_verified_military_availability_transition(
    const CharacterConditionSources &, const StableId &transition_id);
// Exact existing capacity_transition/2 military branch. Replays its actual
// actor chain, not whole-world reverse coverage. The borrowed row is invalid
// after candidate mutation/rollback/root replacement. Full publication remains
// validate_military_availability_coverage; this read does not replace that
// gate.
Outcome<const CapacityTransitionV2 *>
read_verified_military_availability_transition_provenance(
    const CharacterConditionSources &, const SourceRef &,
    Integer source_schema);
// Field 成员交接只需要这一笔已采用 capacity 的原伤亡证明。有限入口仅认
// MilitaryAvailabilityCauseV2/schema1，核真实行/cause/唯一 ordinal/actor/M；
// 不遍历今日 actor_history 的后继 release，避免 release→S1→Field→release 回边。
// 不授予新身体写权、不证明 from/当前 projection；原完整 coverage 仍必须在发布外层。
Outcome<const CapacityTransitionV2 *>
read_original_field_availability_transition_source(
    const CharacterConditionSources &, const SourceRef &, Integer source_schema);
SourceRef
military_availability_transition_source(const StableId &transition_id);
} // namespace hanmo::resident
namespace hanmo::resident {
class CharacterConditionPinSink;
Status
visit_military_availability_source_pins(const CharacterConditionSources &,
                                        CharacterConditionPinSink &);
} // namespace hanmo::resident

namespace hanmo::resident {
// Original already-applied actor chain for source-local historical consumers.
// This verifies each actual cause, order, predecessor and current projection;
// it does not demand unconsumed later actors in one real multi-person source.
// validate_military_availability_coverage remains required at publication.
Status
validate_military_availability_actor_history(const CharacterConditionSources &,
                                             const StableId &actor_id);
} // namespace hanmo::resident

namespace hanmo::resident {
// 借同一已核Capacity的真因果头，供原availability
// cut与PIN复用，不另写第二条历史。
struct MilitaryAvailabilityCausePosition {
  const SourceRef *source = nullptr;
  Integer schema = 0, ordinal = 0;
};
bool is_original_availability_transition(const CapacityTransitionV2 &) noexcept;
Outcome<MilitaryAvailabilityCausePosition>
read_original_availability_cause_position(const CharacterConditionSources &,
                                          const CapacityTransitionV2 &);
} // namespace hanmo::resident

namespace hanmo::resident {
// 只在原同候选、同捕获、同保存点内存活；不是可自报前像或持久许可。
struct FieldMilitaryAvailabilityBinding {
  const CapacityTransitionV2 *transition = nullptr;
  std::shared_ptr<const ActorConditionCaptureIdentity> capture_identity;
  Integer capture_sequence = 0;
};
class FieldMilitaryAvailabilityPermit : public CheckpointAllocatedObject {
  struct Impl;
  CheckpointMemoryCharge memory_;
  std::unique_ptr<Impl> impl_;
  FieldMilitaryAvailabilityPermit(CheckpointMemoryCharge,
                                  std::unique_ptr<Impl>);
  friend Outcome<std::unique_ptr<FieldMilitaryAvailabilityPermit>>
  prepare_field_military_availability(CommonStore &,
                                      AuthoredActorFoundationState &,
                                      const CharacterConditionSources &,
                                      const SourceRef &, Integer,
                                      const ReadStamp &,
                                      const ReadStampAuthority &,
                                      const DomainReadVersionSource &);
  friend Outcome<FieldMilitaryAvailabilityBinding>
  bind_field_military_availability(CommonStore &,
                                   AuthoredActorFoundationState &,
                                   const CharacterConditionSources &,
                                   FieldMilitaryAvailabilityPermit &);

public:
  ~FieldMilitaryAvailabilityPermit();
  FieldMilitaryAvailabilityPermit(const FieldMilitaryAvailabilityPermit &) =
      delete;
  FieldMilitaryAvailabilityPermit &
  operator=(const FieldMilitaryAvailabilityPermit &) = delete;
};
// Field schema1 仅 captured/deserted 原伤亡行；两者沿同一单写捕获与Capacity链。
Outcome<std::unique_ptr<FieldMilitaryAvailabilityPermit>>
prepare_field_military_availability(CommonStore &,
                                    AuthoredActorFoundationState &,
                                    const CharacterConditionSources &,
                                    const SourceRef &, Integer,
                                    const ReadStamp &,
                                    const ReadStampAuthority &,
                                    const DomainReadVersionSource &);
// 军域先执行原一次 available+location mutation；这里只核实录并写原Capacity。
// 返回行只借当前候选，不能跨候选变更/回滚；外层失败必须弃整候选。
Outcome<FieldMilitaryAvailabilityBinding>
bind_field_military_availability(CommonStore &, AuthoredActorFoundationState &,
                                 const CharacterConditionSources &,
                                 FieldMilitaryAvailabilityPermit &);
} // namespace hanmo::resident
