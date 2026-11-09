// Original Interaction5..13 life_sources. No second session or life clock.
#pragma once
#include "interaction_model.hpp"
#include "interaction_study_life.hpp"
#include "interaction_growth_life.hpp"
#include "interaction_task_life.hpp"
#include "resident/domains/family/family_life_exact.hpp"
namespace hanmo::resident {
struct ActorConditionCaptureIdentity;
struct InteractionSessionLifeContext {
 const CommonStore& common;const InteractionState& state;const InteractionCatalog& catalog;
 const FamilyDeathPolicy& life_policy;const FamilyDeathState& deaths;
 const FamilyLifeCauseSources4& causes;const CurrentMomentRead& clock;
 const NativeActorSocialOccupancyRead& occupancy;
 const NativeInteractionStudyLifeRead* study_terminals=nullptr;
 const NativeInteractionGrowthLifeRead* growth_terminals=nullptr;
 const NativeInteractionTaskLifeRead* task_terminals=nullptr;
};
// Private same-candidate death capture. Only the original death writer can add
// entries. No future result ID, saved ledger, source registry or callback is kept.
class InteractionSessionDeathPending final:public CheckpointAllocatedObject {
 struct Entry;
 const CommonStore* common_=nullptr;const InteractionState* state_=nullptr;
 const InteractionCatalog* catalog_=nullptr;std::shared_ptr<const ActorConditionCaptureIdentity> capture_identity_;
 CheckpointVector<std::unique_ptr<Entry>> entries_;
 InteractionSessionDeathPending(CommonStore&,InteractionState&,const InteractionCatalog&);
 friend Status apply_interaction_session_death_in_candidate(CommonStore&,InteractionState&,
  const InteractionSessionLifeContext&,const InteractionAccess&,const DeathRef&,
  const StudyPendingReleaseProofs*,InteractionSessionDeathPending*);
 friend Status bind_interaction_session_death_result_in_candidate(CommonStore&,InteractionState&,
  const InteractionSessionLifeContext&,InteractionSessionDeathPending&,const SourceRef&,
  const StudyPendingReleaseProofs*);
public:
 ~InteractionSessionDeathPending();
 static Outcome<std::unique_ptr<InteractionSessionDeathPending>> create(CommonStore&,InteractionState&,const InteractionCatalog&);
 InteractionSessionDeathPending(const InteractionSessionDeathPending&)=delete;
 InteractionSessionDeathPending&operator=(const InteractionSessionDeathPending&)=delete;
 bool bound_to(const CommonStore&,const InteractionState&,const InteractionCatalog&)const noexcept;
 bool has_pending()const noexcept;
 // Proves exactly this writer's retained immutable selection and original
 // participant-death close. False means this source has no private pending cut.
 Outcome<bool> validate_original_pending_response(const CommonStore&,const InteractionState&,
  const InteractionCatalog&,const InteractionResponseSource&)const;
};
// Cold dependency cut used by the response owner. The complete Life reader
// separately validates the original Family4 death/cause/role/coverage.
Outcome<bool> validate_interaction_response_death_consumption_source(const CommonStore&,
 const InteractionState&,const InteractionCatalog&,const InteractionResponseSource&,const SourceRef&);
// The actual Common result/history must exist. This binds only this capture's
// original death closures, advances each original boundary once, and drops its
// selected tail without executing a dead participant's dialogue or new choice.
Status bind_interaction_session_death_result_in_candidate(CommonStore&,InteractionState&,
 const InteractionSessionLifeContext&,InteractionSessionDeathPending&,const SourceRef&actual_result,
 const StudyPendingReleaseProofs* actual_pending_study_releases=nullptr);
// Physical8/9/10 only. After the original death writer has inserted both its
// SessionStopped and actual participant-death occupancy close, bind that exact
// existing close as the response consumption. No Common command is fabricated.
// The original death writer invokes this before returning, including Opening.
Status bind_interaction_session_death_close_in_candidate(CommonStore&,InteractionState&,
 const InteractionSessionLifeContext&,const StableId& original_life_result_id,
 const StudyPendingReleaseProofs* actual_pending_study_releases=nullptr);
// The original Action owner must consume a waiting Action before this domain can
// close that session. This writer never cancels, releases, charges, or grants it.
Status apply_interaction_session_death_in_candidate(CommonStore&,InteractionState&,
 const InteractionSessionLifeContext&,const InteractionAccess&,const DeathRef&,
 const StudyPendingReleaseProofs* actual_pending_study_releases=nullptr,
 InteractionSessionDeathPending* actual_pending_death_tails=nullptr);
Outcome<const InteractionSessionLifeSource*> read_interaction_session_life_source(
 const InteractionSessionLifeContext&,const StableId&result_id);
Status validate_interaction_session_life_sources(const InteractionSessionLifeContext&);
Status validate_interaction_session_life_sources_in_capture(const InteractionSessionLifeContext&,
 const InteractionSessionDeathPending&,const StudyPendingReleaseProofs* actual_pending_study_releases=nullptr);
enum class InteractionSessionLifePinSlot { death,opening,closing,preserved_result,original_action_terminal,selected_response_tail };
class InteractionSessionLifeSourcePinSink {public:virtual ~InteractionSessionLifeSourcePinSink()=default;
 virtual Status original_source(const StableId& life_result_id,InteractionSessionLifePinSlot,
  Integer ordinal,const SourceRef&,Integer schema)=0;
};
Status visit_interaction_session_life_source_pins(const InteractionSessionLifeContext&,
 InteractionSessionLifeSourcePinSink&);
// Compatibility adapter delegates to the same validated original walk.
Status visit_interaction_session_life_source_pins(const InteractionSessionLifeContext&,
 InteractionResponseSourcePinSink&);
}
