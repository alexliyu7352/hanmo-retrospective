// F固定4df8f13c：facility_claims/2只持当前占用与不可变采用历史；真实容量/保位/产权归设施owner。
#pragma once
#include "resident/kernel/source_ref.hpp"
#include "hanmo/resident/world_moment.hpp"
#include <tuple>
namespace hanmo::resident {
struct FacilitySlotRef { StableId provider_domain,facility_id,slot_kind; };
// Slot排序只用于规范目录；实际同Moment先后仍按原provider revision/前驱来源。
inline bool operator==(const FacilitySlotRef& a,const FacilitySlotRef& b){return std::tie(a.provider_domain,a.facility_id,a.slot_kind)==std::tie(b.provider_domain,b.facility_id,b.slot_kind);}
inline bool operator!=(const FacilitySlotRef& a,const FacilitySlotRef& b){return !(a==b);}
inline bool operator<(const FacilitySlotRef& a,const FacilitySlotRef& b){return std::tie(a.provider_domain,a.facility_id,a.slot_kind)<std::tie(b.provider_domain,b.facility_id,b.slot_kind);}
struct FacilitySourceRef { SourceRef source_ref; Integer source_schema=0; };
struct FacilityClaim2 { StableId claim_id; FacilitySlotRef slot_ref; ReservationRef holder; WorldMoment from,until; Integer units=0,capacity_revision=0; SourceRef usage_right_ref,source_result_ref; };
struct FacilityClaimReceiptRef { FacilitySlotRef slot_ref; Integer claim_set_revision=0; };
enum class FacilityClaimCloseReason { completed,suspended,cancelled,replaced,left_place,holder_died,facility_changed,right_ended };
struct ClosedFacilityClaim { StableId claim_id; FacilityClaimCloseReason reason=FacilityClaimCloseReason::completed; };
struct FacilityClaimMutationReceipt { Integer claim_set_revision=0; WorldMoment occurred_at; SourceRef source_result_ref; Integer source_schema=0; std::vector<ClosedFacilityClaim> closed_claims; std::vector<FacilityClaim2> admitted_claims; std::vector<FacilitySourceRef> capacity_source_refs; };
struct InitialEmptyFacilityClaims {};
struct VacantPrunedFacilityPrefix { Integer last_removed_revision=0; WorldMoment cut_at; SourceRef last_close_result_ref; Integer last_close_source_schema=0; };
using FacilityHistoryBase=std::variant<InitialEmptyFacilityClaims,VacantPrunedFacilityPrefix>;
struct FacilitySlotHistory { FacilitySlotRef slot_ref; FacilityHistoryBase base; std::vector<FacilityClaimMutationReceipt> mutations; };
// 两根规范字段序固定；派生区间索引不持久，不在此复制provider当前容量。
struct FacilityClaimsState { std::vector<FacilityClaim2> claims; std::vector<FacilitySlotHistory> slot_histories; };
struct AdmitWithinCapacity {};
struct NormalWithRetentions { std::vector<FacilitySourceRef> retention_source_refs; };
struct EntireSlotNoNewAdmission { std::vector<FacilitySourceRef> retention_source_refs; SourceRef whole_slot_restriction_source_ref; Integer whole_slot_restriction_source_schema=0; };
using FacilityAdmissionDisposition=std::variant<AdmitWithinCapacity,NormalWithRetentions,EntireSlotNoNewAdmission>;
// 原provider只读深值；一个实际source的availability窗精确且有限，不保存自报历史峰值。
struct FacilityCapacitySource2 { FacilitySlotRef slot_ref; StableId place_ref; Integer capacity=0; StableId capacity_unit_id; Integer capacity_revision=0; std::vector<MomentSpan> availability_intervals; SourceRef usage_right_ref,source_fact_ref; WorldIdentity identity; FacilityAdmissionDisposition admission_disposition; };
struct FacilityClaimEpisode { FacilityClaimReceiptRef admission_receipt_ref; FacilityClaim2 original_claim; std::optional<FacilityClaimReceiptRef> close_receipt_ref; std::optional<MomentSpan> actual_reserved_span; };
struct FacilityClaimRequest2 { FacilitySlotRef slot_ref; ReservationRef holder; WorldMoment from,until; Integer units=0; SourceRef usage_right_ref; std::vector<StableId> replace_claim_ids; };
struct FacilityClaimQuote { FacilityClaimRequest2 requested; Integer minimum_available_units=0,capacity_revision=0,claim_set_revision=0; std::vector<SourceRef> conflicting_visible_refs; };
// 作者只选真实slot/provider及有限预约上界，不能输入claims/history/容量结果。
struct FacilityClaimsCatalog { Integer max_claim_half_days=0; std::vector<FacilitySlotRef> selected_slots; };
}
