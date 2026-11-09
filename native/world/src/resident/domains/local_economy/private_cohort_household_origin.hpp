// Immutable S5 cohort membership, not an Aid consent or population count.
#pragma once
#include "local_economy_model.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct PrivateCohortHouseholdOrigin {
 StableId cohort_id,household_id,wallet_id,inventory_id;
 SourceRef original_cohort_definition,original_household_definition;
};
Outcome<PrivateCohortHouseholdOrigin> read_private_cohort_household_before_source(
 const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
 const StableId& cohort_id,const SourceRef& actual_adoption);
}
