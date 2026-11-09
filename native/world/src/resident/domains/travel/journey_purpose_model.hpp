// Maritime 5.1/5.2: only an actually adopted original responsibility can bind
// the existing journey's purpose. No purpose row is created for ordinary travel
// that has no such original responsibility.
#pragma once
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
struct OrdinaryJourneyPurpose {};
struct OverloadSafetyJourneyPurpose {StableId incident_id,response_id;};
using JourneyPurpose=std::variant<OrdinaryJourneyPurpose,OverloadSafetyJourneyPurpose>;
struct MilitaryJourneyResponsibility {MomentOwnerSource original_intent_or_order;};
struct CivilianDirectJourneyResponsibility {MomentOwnerSource current_operator_binding;};
struct CivilianServiceJourneyResponsibility {MomentOwnerSource accepted_scope;};
using JourneyPurposeResponsibility=std::variant<MilitaryJourneyResponsibility,CivilianDirectJourneyResponsibility,CivilianServiceJourneyResponsibility>;
struct JourneyPurposeBinding {StableId journey_id;JourneyPurpose purpose;JourneyPurposeResponsibility original_responsibility;};
}
