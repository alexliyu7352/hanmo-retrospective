// Original continuous family care body predicate. This does not provide
// consent, ordinary availability, participation, placement or a work Action.
#pragma once
#include "resident/domains/actor_foundation/condition_capacity.hpp"
namespace hanmo::resident {
Outcome<const CharacterPurposeBinding*> read_original_care_participant_purpose(
 const ActorFoundationCatalog&);
Outcome<CharacterCapabilityView> read_care_participant_capacity_at_head(
 const CharacterConditionSources&,const ActorConditionHead&,const WorldMoment&);
Outcome<CharacterCapabilityView> read_current_care_participant_capacity(
 const CharacterConditionSources&,const StableId&,const ReadStamp&,
 const ReadStampAuthority&,const DomainReadVersionSource&);
Status visit_care_participant_capacity_span(const CharacterConditionSources&,
 const ActorConditionHead&,const MomentSpan&,CharacterCapabilitySpanVisitor&);
// Borrowed original output and its actual receipt. The current original alive
// predicate can lose capacity only through actual Life4: consumers must use
// HolderDeath and the existing binding/death once key, not issue a second Lost.
struct CareParticipantCapacityChange {
 const CapacityTransitionV2* transition=nullptr;
 const ConditionReceiptV2* original_receipt=nullptr;
 const StableId* priority_holder_death_id=nullptr;
};
Outcome<CareParticipantCapacityChange> read_care_participant_capacity_change(
 const CharacterConditionSources&,const SourceRef& capacity_transition);
}

namespace hanmo::resident {
// The actual S1 handover owner has already validated this real result/M.
// Children need presence/alive, not adult availability or work capability.
Status validate_care_participants_at_actual_handover(
 const CharacterConditionSources&,const StableId& child,const StableId& carer,
 const SourceRef& actual_placement,const WorldMoment&,const CurrentMomentRead&);
Status validate_current_care_participants(const CharacterConditionSources&,
 const StableId& child,const StableId& carer,const WorldMoment&);
Status validate_care_participants_full_span_after_handover(
 const CharacterConditionSources&,const StableId& child,const StableId& carer,
 const SourceRef& actual_completed_handover,const WorldMoment& handover_at,
 const MomentSpan&,const CurrentMomentRead&);
}
