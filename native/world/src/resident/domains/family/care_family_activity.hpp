// Care6: the actual ongoing family activity, after the original handover
// releases its Action. Care5 and every earlier prefix have no such tail.
#pragma once
#include "care_handover_execution.hpp"
namespace hanmo::resident {
// Care6 unadmitted tag2 successor: the original source slot is retained first.
// This is the one StopCare payload, embedded here rather than in a second root.
struct CarePersonalStop {MomentOwnerSource actual_stop_result;CareHandoverActorCommand actual_command;};
// Reuse the original six loss causes unchanged. A subject death ends the
// activity and closes the target; a personal stop does not withdraw the need.
using CareFamilyActivityEndCause=std::variant<CareLossCauseV2,CareSubjectDeath,CarePersonalStop>;
struct CareFamilyActivityEnd {
 StableId end_id;WorldMoment ended_at;CareFamilyActivityEndCause cause;
};
struct CareFamilyActivity {
 StableId activity_id,accepted_care_terms_ref,child_actor_id,carer_actor_id;
 MomentOwnerSource actual_handover_execution_ref,actual_start_work_period_ref;
 MomentOwnerSource actual_placement_result_ref;WorldMoment started_at;
 ActorFoundationRuleRef compatibility_rule_ref;
 std::vector<MomentOwnerSource> adopted_participation_refs;
 std::optional<CareFamilyActivityEnd> ended_by;
};
SourceRef care_family_activity_started_source(const StableId& activity_id);
SourceRef care_family_activity_ended_source(const StableId& end_id);
Outcome<Integer> care_family_activity_source_schema(const SourceRef&);
// The only presently selected compatibility meaning is the original exclusive
// caregiver occupation. This compiled identity does not permit a second Action.
Outcome<ActorFoundationRuleRef> original_exclusive_family_care_rule(const CatalogIdentity&);
Status validate_care_family_activity_shape(const CareFamilyActivity&,Integer selected_version);
bool care_family_activity_end_cause_equal(const CareFamilyActivityEndCause&,
 const CareFamilyActivityEndCause&);
}
