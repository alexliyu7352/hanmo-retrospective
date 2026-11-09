#pragma once
#include "military_action_time_read.hpp"
namespace hanmo::resident {
// Original execution identity only. This view never grants professional
// participation, proves a service window, or substitutes for full cold checks.
// The two transition pointers are the actual S0 records, not Common fact refs.
struct MilitaryActionOccupationSource {
 StableId action_ref,action_kind,actor_id;
 std::vector<StableId> participant_actor_ids;
 MomentOwnerSource execution_source;WorldMoment started_at;
 std::optional<MomentOwnerSource> result_source;
 std::optional<WorldMoment> ended_at;
 // Present only when the original successful Formation/Return really appended
 // its position-adoption head. Office/Training terminal occurrence heads
 // do not adopt placement and must never populate this distinct field.
 std::optional<MomentOwnerSource> completion_application;
 const ActionElapsedState* elapsed=nullptr;
 const FixedServiceTransition* started_transition=nullptr;
 const FixedServiceTransition* terminal_transition=nullptr;
};
struct MilitaryActionOccupationSourceContext {
 const CommonStore& common;const MilitaryState& military;
 const MilitaryActionTimeState& state;const ActionElapsedStore& elapsed;
 const TimeScaleRef& scale;const CurrentMomentRead& clock;
 MilitaryActionTimeReadSelection selected;
 // Only the concrete source-local participant leaf is called. It reads original
 // formation/member changes and alive provenance, never whole occupation.
 const ExactReturnReadContext* returns=nullptr;
 const ExactResupplyOriginalRead* resupply=nullptr;
 ExactArithmeticBudget budget{};
};
// Original C05 pre-start unit occupation only. Authored unit identity plus real
// mobilization/training source intervals; not roster/fit/army/authority proof.
Status validate_original_military_unit_activity_before(const MilitaryActionOccupationSourceContext&,
 const MilitaryCatalog&,const std::vector<StableId>& unit_ids,
 const SourceRef& actual_boundary,const WorldMoment& boundary_at);
Status collect_original_military_unit_activity_pins(const MilitaryActionOccupationSourceContext&,
 const MilitaryCatalog&,const std::vector<StableId>& unit_ids,const SourceRef& actual_boundary,
 const WorldMoment& boundary_at,SourcePinSink&);
Status validate_original_military_unit_activity_pin(const MilitaryActionOccupationSourceContext&,
 const MilitaryCatalog&,const std::vector<StableId>& unit_ids,const SourceRef& actual_boundary,
 const WorldMoment& boundary_at,const SourcePin&);
std::vector<RequiredSourceConsumerSlot> original_military_unit_activity_consumer_slots();
class NativeMilitaryActionOccupationSourceRead {
 MilitaryActionOccupationSourceContext context_;
public:
 explicit NativeMilitaryActionOccupationSourceRead(MilitaryActionOccupationSourceContext x):context_(x){}
 bool bound_to(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,
  const ActionElapsedStore& e)const noexcept{return &c==&context_.common&&&m==&context_.military&&&s==&context_.state&&&e==&context_.elapsed;}
 // Covers only these five explicitly selected /2 original source kinds.
 // Legacy actions and C21 remain their own original source branches.
 Outcome<std::vector<StableId>> enumerate_original_occupation_sources()const;
 Outcome<MilitaryActionOccupationSource> read_original_occupation_source(const StableId& action)const;
};
} // No saved participant mirror, occupation table or new timing state.
