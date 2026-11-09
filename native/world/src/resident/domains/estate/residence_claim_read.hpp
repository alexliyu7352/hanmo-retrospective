// 只反读 Residence 实际采用的 S0 床位 episode，不独立授居住或出租权限。
#pragma once
#include "residence_service_contract.hpp"
#include "resident/facility_claims/facility_claims.hpp"
namespace hanmo::resident {
struct ResidenceBedClaimRead {
  const FacilityClaim2 *claim = nullptr;
  const FacilityClaimMutationReceipt *admission = nullptr;
  const FacilityClaimMutationReceipt *closure = nullptr;
};
// 必须借原 line 指向的真实 receipt，不能把同床位别人的 claim 冒本人的床。
// before 为实际已发生的消费源；空指针只用于当前候选读，不补造历史 cut。
Outcome<ResidenceBedClaimRead> read_residence_bed_claim_at_source(
    const CommonStore &, const FacilityClaimsState &,
    const FacilityClaimsCatalog &, const ResidenceIntakeLineRef &,
    const ResidenceSlotClaimRef &, const FacilitySlotRef &original_bed_slot,
    const WorldMoment &actual_at, const SourceRef *actual_before = nullptr,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
