// Original confirmed Growth action -> already adopted participant-death terminal.
#pragma once
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "resident/domains/family/death_ref.hpp"
namespace hanmo::resident {
struct InteractionState;struct GrowthTerminalPublication;
struct LifeFixedServiceBoundary;struct InteractionActionConfirmationSource;
struct OriginalInteractionGrowthTerminal {
 const GrowthTerminalPublication* publication=nullptr;
 const LifeFixedServiceBoundary* boundary=nullptr;
 const InteractionActionConfirmationSource* confirmation=nullptr;
};
// This reader neither stops nor releases an action. The original Growth input
// consumer must already have done both and published its actual terminal.
class NativeInteractionGrowthLifeRead final:public CheckpointAllocatedObject {
 const WorldBundle& world_;const CharacterConditionWorldRead& body_;
 NativeInteractionGrowthLifeRead(const WorldBundle&w,const CharacterConditionWorldRead&b):world_(w),body_(b){}
 Outcome<OriginalInteractionGrowthTerminal> read(const StableId& session,
  const StableId& action,const DeathRef&)const;
public:
 static Outcome<std::unique_ptr<NativeInteractionGrowthLifeRead>> create(
  const WorldBundle&,const CharacterConditionWorldRead&);
 bool bound_to(const CommonStore&,const InteractionState&)const noexcept;
 // A finite structural dispatch only. A true answer is never source proof.
 bool owns_original_action(const StableId&)const noexcept;
 bool owns_original_terminal(const SourceRef&)const noexcept;
 Outcome<OriginalInteractionGrowthTerminal> read_released_waiting_action(
  const StableId& session,const StableId& action,const DeathRef&)const;
 Outcome<OriginalInteractionGrowthTerminal> read_original_terminal(
  const StableId& session,const SourceRef&,const DeathRef&)const;
 Outcome<bool> has_original_session_death_terminal(const StableId& session,
  const DeathRef&)const;
};
}
