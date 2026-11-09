// Real S1 contact dispatch. All dependencies are borrowed from one lexical
// Complete/Group lease; this adapter never reacquires the World graph.
#pragma once
#include "field_exact_contact_source.hpp"
#include "resident/domains/travel/movement_encounter_integration.hpp"
namespace hanmo::resident {
struct PoliticalRelationsReadContext;
struct PoliticalRelationsState;
struct FieldBattleAbilityReadContext;
struct SocialState;
struct OrganizationState;
class CheckpointMemoryCharge;
class NativeFieldMotionContactRead final : public MotionContactBusinessRead {
  const FieldExactContactReadContext &exact_;
  const FieldBattleCatalog &catalog_;
  const NativePersonalMotionContactRead &personal_;
  ExactArithmeticBudget budget_;

public:
  NativeFieldMotionContactRead(const FieldExactContactReadContext &,
                               const FieldBattleCatalog &,
                               const NativePersonalMotionContactRead &,
                               ExactArithmeticBudget = {});
  Status validate_actual_contact_installation(
      const ExactMovementContact &, const MomentOwnerSource &) const override;
};
// Only transient references. Owner/capture lifetime must enclose the actual
// Motion install call; neither Body nor Group context is cached beyond it.
struct FieldMotionContactWriteDependencies {
  CommonStore &common;
  FieldBattleWorldState &field;
  const FieldExactContactReadContext &exact;
  const FieldBattleCatalog &catalog;
  PoliticalRelationsState &relations;
  const PoliticalRelationsReadContext &relations_read;
  const SocialState &social;
  const OrganizationState &organizations;
  const FieldBattleAbilityReadContext &abilities;
  const StableId &player_actor_id;
  Integer battle_seed;
  CheckpointMemoryCharge &world_memory;
  NativePersonalMotionContactSources &personal;
  ExactPhaseWriteContext &capture;
};
class NativeFieldMotionContactSources final
    : public MotionContactBusinessSources {
  FieldMotionContactWriteDependencies x_;
  ExactArithmeticBudget budget_;

public:
  explicit NativeFieldMotionContactSources(FieldMotionContactWriteDependencies,
                                           ExactArithmeticBudget = {});
  Outcome<MomentOwnerSource>
  install_actual_contact_in_candidate(const ExactMovementContact &,
                                      ExactPhaseWriteContext &) override;
  Status validate_actual_contact_installation(
      const ExactMovementContact &, const MomentOwnerSource &) const override;
};
} // namespace hanmo::resident
