// S0读取经济09唯一物理/滞留DTO并核共享占用关系；不持另一张位置或retention当前表。
#pragma once
#include "facility_claims.hpp"
#include "resident/domains/local_economy/economic_facility_sources.hpp"
namespace hanmo::resident {
Outcome<bool> read_verified_facility_retention_class(const FacilityClaimsState&,const FacilityClaim2&,const MomentSpan&,const FacilityCapacitySource2&,const EconomicFacilityOriginalSourceRead&,ExactArithmeticBudget budget={});
Outcome<Integer> read_facility_additional_normal_capacity(const FacilityClaimsState&,const FacilitySlotRef&,const MomentSpan&,const std::vector<const FacilityClaim2*>&,const FacilityCapacitySource2&,const EconomicFacilityOriginalSourceRead&,ExactArithmeticBudget budget={});
Status validate_facility_physical_interval(const FacilityClaimsState&,const FacilitySlotRef&,const MomentSpan&,const std::vector<const FacilityClaim2*>&,const FacilityCapacitySource2&,const EconomicFacilityOriginalSourceRead&,ExactArithmeticBudget budget={});
}
