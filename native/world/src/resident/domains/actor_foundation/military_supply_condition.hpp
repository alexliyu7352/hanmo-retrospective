// P4.3: original military Supply2 shortage, one receipt per actual named mouth.
#pragma once
#include "character_condition_read.hpp"
#include "resident/domains/military/military_supply_exact.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
class GrowthCapabilityRevisionWrite;
class CharacterConditionWorldRead;
// Compiled selection only. No author-supplied fatigue amount or policy string.
Status
register_original_military_supply_shortage_rule(ActorFoundationCatalog &);
struct MilitarySupplyConditionApplication {
  CheckpointMemoryCharge text_memory;
  ConditionReceiptRefV2 receipt_ref;
  CheckpointVector<BoundActorConditionWrite> actual_writes;
  bool already_applied = false;
};
Outcome<MilitarySupplyConditionApplication>
apply_verified_military_supply_shortage(
    CommonStore &, AuthoredActorFoundationState &,
    const CharacterConditionSources &, const SourceRef &actual_supply,
    Integer original_mouth_ordinal, const ReadStamp &,
    const ReadStampAuthority &, const DomainReadVersionSource &,
    GrowthCapabilityRevisionWrite &);
// Field Opening 原供给的整组写后绑定；先全尾预核，再落真实回执，
// 全current与原before prefix闭合后才逐笔交能力consumer。失败沿外层回滚。
Outcome<CheckpointVector<BoundActorConditionWrite>>
bind_field_military_supply_shortages(
    CommonStore &, AuthoredActorFoundationState &,
    const CharacterConditionWorldRead &, const SourceRef &actual_supply,
    Integer first_actual_capture_sequence, const CurrentReadStampRead &,
    ReadStampAuthority &, const DomainReadVersionSource &,
    GrowthCapabilityRevisionWrite &);

Status
validate_military_supply_shortage_condition(const CharacterConditionSources &,
                                            const ConditionReceiptV2 &);
Status
validate_military_supply_shortage_coverage(const CharacterConditionSources &);
class MilitarySupplyConditionPinSink {
public:
  virtual ~MilitarySupplyConditionPinSink() = default;
  virtual Status actual_pin(const SourcePin &) = 0;
};
RequiredSourceConsumerSlot military_supply_condition_source_consumer_slot();
Status
visit_military_supply_condition_source_pins(const CharacterConditionSources &,
                                            MilitarySupplyConditionPinSink &);
Status
validate_military_supply_condition_source_pin(const CharacterConditionSources &,
                                              const SourcePin &);
// Same live candidate lease; body and stamp are refreshed after every real
// write. This does not own another Supply result or another Condition history.
class NativeMilitarySupplyConditionWrite final
    : public MilitarySupplyExactConditionWrite {
  CommonStore &common_;
  AuthoredActorFoundationState &foundation_;
  const MilitaryState &military_;
  const MilitaryExactState &exact_;
  const ExactTrainingPeriodRead &periods_;
  const CharacterConditionWorldRead &body_;
  const CurrentReadStampRead &stamps_;
  const ReadStampAuthority &authority_;
  const DomainReadVersionSource &versions_;
  GrowthCapabilityRevisionWrite &capability_;
  std::vector<BoundActorConditionWrite> &writes_;

public:
  NativeMilitarySupplyConditionWrite(
      CommonStore &, AuthoredActorFoundationState &, const MilitaryState &,
      const MilitaryExactState &, const ExactTrainingPeriodRead &,
      const CharacterConditionWorldRead &, const CurrentReadStampRead &,
      const ReadStampAuthority &, const DomainReadVersionSource &,
      GrowthCapabilityRevisionWrite &, std::vector<BoundActorConditionWrite> &);
  bool bound_to(const CommonStore &, const MilitaryState &,
                const MilitaryExactState &) const noexcept override;
  Status
  apply_actual_supply_conditions_in_candidate(const SourceRef &) override;
};
} // namespace hanmo::resident
