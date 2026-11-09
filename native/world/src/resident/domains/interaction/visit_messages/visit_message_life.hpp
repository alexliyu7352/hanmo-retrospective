// Fixed S3 §4.2: actual death consumption stays in the original message owner.
#pragma once
#include "visit_message.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct VisitMessageLifeContext {
 const CommonStore& common;const VisitMessageState& state;const VisitMessageCatalog& policies;
 const InteractionCatalog& interaction;const InteractionState& interactions;const FamilyDeathPolicy& life_policy;
 const FamilyDeathState& deaths;const FamilyLifeCauseSources4& causes;
 const CurrentMomentRead& clock;
};
// Typed row has no separate Common fact. Its key is the original leaf result_id.
// All mutations belong to the caller's existing candidate; any failure aborts it.
Status apply_visit_message_death_in_candidate(CommonStore&,VisitMessageState&,const VisitMessageLifeContext&,const DeathRef&);
Outcome<const VisitMessageLifeSource*> read_visit_message_life_source(const VisitMessageLifeContext&,const StableId& result_id);
Status validate_visit_message_life_sources(const VisitMessageLifeContext&);
// Real command admission uses this after the original contact proof. This does
// not expose a remote person's death through a quote or select a successor.
Status validate_visit_message_life_command(const VisitMessageLifeContext&,const VisitMessageCommand&);
}
