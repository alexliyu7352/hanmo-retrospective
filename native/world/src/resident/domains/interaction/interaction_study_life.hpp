// Borrow the actual TeachStudy request/session and original released terminal.
#pragma once
#include "resident/domains/growth/study_pending_release.hpp"
#include "resident/domains/actor_foundation/character_condition_world.hpp"
namespace hanmo::resident {
struct InteractionState;
struct OriginalInteractionStudyTerminal {
 const StudyServiceTerminalV2* terminal=nullptr;
 const StudyTerminalTime* time=nullptr;
};
class NativeInteractionStudyLifeRead final:public CheckpointAllocatedObject {
 const WorldBundle& world_;const CharacterConditionWorldRead& body_;
 NativeInteractionStudyLifeRead(const WorldBundle&w,const CharacterConditionWorldRead&b):world_(w),body_(b){}
 Outcome<OriginalInteractionStudyTerminal> read(const StableId& session,const StableId& action,
  const DeathRef&,const StudyPendingReleaseProof*)const;
 Outcome<OriginalInteractionStudyTerminal> read_terminal(const StableId&,const SourceRef&,const DeathRef&,const StudyPendingReleaseProofs*)const;
public:
 static Outcome<std::unique_ptr<NativeInteractionStudyLifeRead>> create(const WorldBundle&,const CharacterConditionWorldRead&);
 bool bound_to(const CommonStore&,const InteractionState&)const noexcept;
 Outcome<OriginalInteractionStudyTerminal> read_released_waiting_action(const StableId& session,
  const StableId& action,const DeathRef&,const StudyPendingReleaseProof&)const;
 Outcome<OriginalInteractionStudyTerminal> read_original_terminal(const StableId& session,
  const SourceRef& actual_terminal,const DeathRef&)const;
 Outcome<bool> has_original_session_death_terminal(const StableId& session,const DeathRef&)const;
 Outcome<OriginalInteractionStudyTerminal> read_original_terminal_in_capture(const StableId& session,
  const SourceRef& actual_terminal,const DeathRef&,const StudyPendingReleaseProofs&)const;
};
}
