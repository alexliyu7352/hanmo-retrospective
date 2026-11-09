// Original actual Gather work -> Condition cut. Never reads Condition effects
// back into the work proof; the full world separately validates their coverage.
#pragma once
#include "resource_gather_model.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
SourceRef resource_gather_claim_source(const StableId&);
SourceRef resource_gather_accepted_source(const StableId&);
SourceRef resource_gather_receipt_source(const StableId&);
struct ResourceGatherConditionContext {
 const CommonStore& common;const ResourceGatherOwnerState& gather;
 const ActionElapsedStore& elapsed;const CurrentMomentRead& clock;
 const TimeScaleRef& scale;
};
struct ResourceGatherConditionSource {
 const ResourceGatherAcceptedTerms* accepted=nullptr;
 const ResourceGatherReceipt* receipt=nullptr;
 FixedServiceWorkPeriodRead2 original_period;
 Integer fatigue_delta=0;
};
// Only the original Action's actual primary Actor. Additional named labor must
// be proven through its original role/allocation owner, not passed as a list.
Outcome<ResourceGatherConditionSource> read_gather_period_for_condition(
 const ResourceGatherConditionContext&,const SourceRef& actual_receipt);
}
