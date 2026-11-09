// Source-local predecessor of an actual applied capability change. No event ledger.
#pragma once
#include "growth_result_refs.hpp"
#include "resident/domains/actor_foundation/condition_refs.hpp"
namespace hanmo::resident {
using GrowthCapabilityChangeRef=std::variant<InitialConditionSeedRef,ConditionReceiptRefV2,AttributeMilestoneRef,LifeGrowthContributionRef>;
struct GrowthCapabilityApplication {Integer revision_before=0;GrowthCapabilityChangeRef previous;};
bool same_growth_capability_change(const GrowthCapabilityChangeRef&,const GrowthCapabilityChangeRef&);
}
