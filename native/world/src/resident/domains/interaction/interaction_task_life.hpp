// Original confirmed Task action -> already adopted participant-death terminal.
#pragma once
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "resident/domains/family/death_ref.hpp"
namespace hanmo::resident {
struct InteractionState;struct TaskExactStartSource;struct TaskExactTerminalSource;
struct InteractionActionConfirmationSource;
struct OriginalInteractionTaskTerminal {
 const TaskExactStartSource* started=nullptr;
 const TaskExactTerminalSource* terminal=nullptr;
 const InteractionActionConfirmationSource* confirmation=nullptr;
};
// Source-local reader only. The actual Task owner must first stop and release
// the Action. This neither executes a command nor replays Task completion.
class NativeInteractionTaskLifeRead final:public CheckpointAllocatedObject {
 const WorldBundle& world_;const CharacterConditionWorldRead& body_;
 NativeInteractionTaskLifeRead(const WorldBundle&w,const CharacterConditionWorldRead&b):world_(w),body_(b){}
 Outcome<OriginalInteractionTaskTerminal> read(const StableId&,const StableId&,const DeathRef&)const;
public:
 static Outcome<std::unique_ptr<NativeInteractionTaskLifeRead>> create(const WorldBundle&,const CharacterConditionWorldRead&);
 bool bound_to(const CommonStore&,const InteractionState&)const noexcept;
 bool owns_original_action(const StableId&)const noexcept;
 bool owns_original_terminal(const SourceRef&)const noexcept;
 Outcome<OriginalInteractionTaskTerminal> read_released_waiting_action(const StableId&session,const StableId&action,const DeathRef&)const;
 Outcome<OriginalInteractionTaskTerminal> read_original_terminal(const StableId&session,const SourceRef&,const DeathRef&)const;
 Outcome<bool> has_original_session_death_terminal(const StableId&session,const DeathRef&)const;
};
}
