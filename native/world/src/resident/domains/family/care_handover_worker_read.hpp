// Actual body authority for Care's original FH worker frames and Running spans.
#pragma once
#include "care_handover_execution.hpp"
namespace hanmo::resident {
Status validate_care_handover_worker_history(const CareHandoverSourceContext&,const StableId& handover_id);
// Captures the explicitly selected actual worker only, with the current Actor
// and ConditionHead versions. It does not grant consent/participation/authority
// or change the action's duration. The real start/boundary writer owns adoption.
Outcome<FrozenFacilityCapabilityInputs> capture_care_handover_worker(
 const CharacterConditionSources&,const StableId& actual_worker,
 const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&);
}
