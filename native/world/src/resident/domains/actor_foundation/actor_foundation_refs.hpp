// 原型别的轻量声明和值合同；只搬位置，不改变布局或规则。
#pragma once
#include "resident/domains/actor_foundation/condition_refs.hpp"
#include "hanmo/resident/world_moment.hpp"

namespace hanmo::resident {
struct ActorFoundationCatalog;
struct AuthoredActorFoundationState;
struct ConditionReceiptV2;
struct ActorStartContextStateV2;
struct CharacterOriginRecordsStateV1;
struct CharacterConditionExtensionsStateV1;
struct FrozenFacilityCapabilityInputs;
struct InjuryAttributeReduction;
struct InitialConditionSeed;
struct CapacityTransitionV2;
struct SourceRef;

struct ActorFoundationRuleRef {
  StableId id;
  Integer version = 0;
  StableId catalog_digest;
};

struct ScenarioOriginRef {
  StableId catalog_digest, scenario_id, row_key;
  Integer initial_tick = 0;
};

struct CharacterEfficiencyComponent {
  enum class Kind { fatigue, ordinary_injury };
  Kind kind = Kind::fatigue;
  CharacterConditionSourceRef original_source_ref;
  ActorFoundationRuleRef rule_ref;
  Integer cap_bp = 0;
};

struct ActorFoundationAt {
  enum class Kind { legacy_tick, exact_moment };
  Kind kind = Kind::exact_moment;
  Integer legacy_tick = 0;
  WorldMoment exact_moment;
};

enum class ActorOriginWireProfile { authored_only, complete_origin2 };

struct CharacterConditionAppliedPrefix {
  Integer applied_receipt_count = 0;
  std::optional<ConditionReceiptRefV2> last_applied_receipt;
};

struct ActorConditionHead {
  StableId actor_id;
  Integer condition_revision = 0;
  InitialConditionSeedRef initial_condition_seed_ref;
  std::optional<ConditionReceiptRefV2> latest_receipt_ref;
};
} // namespace hanmo::resident
