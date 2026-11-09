// S0唯一claim writer/历史reader；reader实例绑定同一候选或immutable snapshot，不接受外部来源字典。
#pragma once
#include "model.hpp"
#include <functional>
namespace hanmo::resident {
// 变更次序为provider原revision链，不从Moment/ID排序重造采用序。
enum class FacilityMutationCapacityOrder { before,reconciles,after };
struct FacilityCapacityChange { FacilitySourceRef source; WorldMoment occurred_at; Integer revision=0; std::optional<FacilitySourceRef> previous_source; };
// 有效分段是一次reader的临时返回窗；availability可为空（真实关闭），故不能拿营业窗冒source生效窗。
struct VerifiedFacilityCapacitySegment { MomentSpan window; FacilityCapacitySource2 source; };
// 原owner反向枚举必要引用，防删S0历史后以当前空表自证；历史需求不能裁后补承诺。
struct FacilityClaimDependency { StableId claim_id; std::optional<FacilityClaimReceiptRef> receipt; };
class FacilityClaimSourceRead {
public:
    virtual ~FacilityClaimSourceRead()=default;
    virtual Outcome<WorldMoment> initial_source_at(const FacilitySlotRef&) const=0;
    virtual Outcome<std::vector<VerifiedFacilityCapacitySegment>> read_verified_capacity(const FacilitySourceRef&,const FacilitySlotRef&,const MomentSpan&) const=0;
    // 初态/裁剪源中真实已采用的链基底；同M不能取该时刻最后源覆盖更早的采用位置。
    virtual Outcome<FacilityCapacityChange> read_capacity_history_base(const FacilitySlotRef&,const FacilityHistoryBase&) const=0;
    // 必须核原provider/业务source采用先后及实际处置绑定，不能由时刻/ID/refs成员猜。
    // 有retention entry时返回原FacilityRetentionTransition唯一root；普通变化返回其原业务结果。
    virtual Outcome<FacilitySourceRef> read_capacity_reconciliation_source(const FacilitySlotRef&,const FacilityCapacityChange&) const=0;
    virtual Outcome<FacilityMutationCapacityOrder> compare_mutation_to_capacity_change(const FacilitySlotRef&,const FacilityClaimMutationReceipt&,const FacilityCapacityChange&) const=0;
    virtual Outcome<FacilitySourceRef> capacity_source_at(const FacilitySlotRef&,const WorldMoment&) const=0;
    virtual Outcome<std::vector<FacilityCapacityChange>> enumerate_capacity_changes(const FacilitySlotRef&,const WorldMoment& from,const WorldMoment& through) const=0;
    virtual Outcome<std::vector<FacilityClaimDependency>> enumerate_claim_dependencies(const FacilitySlotRef&) const=0;
    virtual Status validate_claim_admission(const FacilityClaim2&,const FacilityClaimMutationReceipt&) const=0;
    virtual Status validate_claim_close(const FacilityClaim2&,FacilityClaimCloseReason,const FacilityClaimMutationReceipt&) const=0;
    virtual Status validate_mutation_source(const FacilitySlotRef&,const FacilityClaimMutationReceipt&) const=0;
    virtual Status validate_pruned_prefix(const FacilitySlotRef&,const VacantPrunedFacilityPrefix&) const=0;
    // §3.2原retention/physical DTO归经济09；此口必须直接核其真实分类/位置/双预算完整源，不能默许未知holder。
    virtual Status validate_retention_and_physical_occupancy(const FacilitySlotRef&,const MomentSpan&,const std::vector<const FacilityClaim2*>&,const FacilityCapacitySource2&) const=0;
    // 非持久分类必须在原retention/physical源已独立证明后返回，未列/未知绝非默认normal。
    // 完整layout/retention核验后算原正常服务可再采用units；Aggregate也须计retained，不能只减normal。
    virtual Outcome<Integer> read_additional_normal_capacity(const FacilitySlotRef&,const MomentSpan&,const std::vector<const FacilityClaim2*>&,const FacilityCapacitySource2&) const=0;
    virtual Outcome<bool> is_verified_retained_claim(const FacilityClaim2&,const MomentSpan&,const FacilityCapacitySource2&) const=0;
    virtual Outcome<bool> source_change_affects_claims(const FacilityCapacityChange&,const std::vector<const FacilityClaim2*>&) const=0;
    virtual Outcome<bool> conflicting_source_visible(const ReservationRef& requester,const FacilityClaim2&) const=0;
};
Status validate_facility_claim_value(const FacilityClaim2&,const FacilityClaimsCatalog&,ExactArithmeticBudget budget={});
// 活跃状态只由完整history纯重放导出，遍历真实首段/尾段和同刻全部provider变化。
Status validate_facility_claims(const FacilityClaimsState&,const FacilityClaimsCatalog&,const WorldMoment& current,const FacilityClaimSourceRead&,ExactArithmeticBudget budget={});
Outcome<FacilityClaimMutationReceipt> read_facility_claim_receipt(const FacilityClaimsState&,const FacilityClaimReceiptRef&);
Outcome<FacilityClaimEpisode> read_facility_claim_episode(const FacilityClaimsState&,const StableId& claim_id,const WorldMoment& current,ExactArithmeticBudget budget={});
// quote纯读不分配业务ID、不收费；容量/claim revision在真正采用时必须重验。
Outcome<FacilityClaimQuote> quote_facility_claim(const FacilityClaimsState&,const FacilityClaimsCatalog&,const FacilityClaimRequest2&,const std::vector<FacilitySourceRef>&,const WorldMoment& now,const FacilityClaimSourceRead&,ExactArithmeticBudget budget={});
// mutation由真实owner晚绑结果后提供；此函数产生独立候选，失败不改调用者原状态。
Outcome<FacilityClaimsState> apply_facility_claim_mutation(const FacilityClaimsState&,const FacilityClaimsCatalog&,const FacilitySlotRef&,const FacilityClaimMutationReceipt&,Integer expected_claim_revision,const WorldMoment& now,const FacilityClaimSourceRead&,ExactArithmeticBudget budget={});
// 仅完整验证过且实际空闲、无PIN穿越的前缀可裁，保留后继原revision及当前claims。
Outcome<FacilityClaimsState> prune_vacant_facility_prefix(const FacilityClaimsState&,const FacilityClaimsCatalog&,const FacilitySlotRef&,Integer through_revision,const WorldMoment& now,const FacilityClaimSourceRead&,ExactArithmeticBudget budget={});
}
