// Original Origin2 + Military availability + Life4 history. No new current
// availability cache, source, permission or body threshold.
#pragma once
#include "character_condition_read.hpp"
#include "character_presence_v2.hpp"
#include "military_availability.hpp"
#include "character_availability_provenance.hpp"
namespace hanmo::resident {
struct CharacterAvailabilityAtSource {
 const CharacterOriginRecordV2* original_origin=nullptr;
 bool present=false,alive=false,available=false;
 const CapacityTransitionV2* latest_availability_transition=nullptr;
 const FamilyDeathRecord* actual_death=nullptr;
};
// The boundary owner must first validate its actual payload and exact M. This
// reader independently checks the retained Common header/order, original actor
// creation, all actual availability causes, and the original Death4 chain.
Outcome<CharacterAvailabilityAtSource> read_character_availability_before_fact_v2(
 const CharacterConditionSources&,const StableId& actor,const SourceRef& actual_boundary,
 const WorldMoment& boundary_at,const CurrentMomentRead&);
struct CharacterAvailabilitySpanRead {
 CharacterAvailabilityAtSource at_adoption;
 bool alive_for_full_span=false,available_for_full_span=false;
 const CapacityTransitionV2* first_unavailable_transition=nullptr;
 const FamilyDeathRecord* death_before_span_end=nullptr;
};
// Positive [from,to), from >= the real adoption M. Actual later same-M losses
// participate; a change exactly at to does not invalidate the preceding span.
// 旧1–5仍无恢复来源；显式6只承认真实具名closure与对应capture前驱。
// 窗口从原同一链归约，缺失/交错来源不会靠今日available补出恢复。
Outcome<CharacterAvailabilitySpanRead> read_character_availability_span_after_fact_v2(
 const CharacterConditionSources&,const StableId& actor,const SourceRef& actual_adoption,
 const WorldMoment& adopted_at,const MomentSpan& actual_span,const CurrentMomentRead&);
}

namespace hanmo::resident {
// Same original cut, but only already-applied actual actor/life provenance.
// Used inside an original multi-person consequence before the next ordinal.
// It never certifies whole source reverse coverage; the strict reader above
// and complete Condition/Life validation remain mandatory at stable exits.
Outcome<CharacterAvailabilityAtSource> read_character_availability_before_fact_provenance_v2(
 const CharacterConditionSources&,const StableId& actor,const SourceRef& actual_boundary,
 const WorldMoment& boundary_at,const CurrentMomentRead&);
// Runs that same source-local reducer and streams its real checked dependencies,
// including checked suffix transitions/death marked not applied at the cut.
// The positive read path does not require a PIN sink or new source registration.
// Returned rows remain borrowed only until any input candidate mutation,
// rollback or root replacement; no callback may mutate these roots.
Outcome<CharacterAvailabilityAtSource> visit_character_availability_before_fact_provenance_v2(
 const CharacterConditionSources&,const StableId& actor,const SourceRef& actual_boundary,
 const WorldMoment& boundary_at,const CurrentMomentRead&,CharacterAvailabilityProvenanceSink&);
}
