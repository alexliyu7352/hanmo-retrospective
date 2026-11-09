// F§4到期/变更只委托真实holder原规则；S0拥有共享claim原子采用，不能自动清人/清WIP。
#pragma once
#include "facility_claims.hpp"
#include "hanmo/resident/checkpoint_inline_function.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
struct FacilityHolderKind { ReservationRef::Kind kind=ReservationRef::Kind::action;StableId owner_domain,activity_kind; };
struct FacilityHolderProfileSelection { FacilityHolderKind holder;StableId profile_id;Integer version=0; };
// 唯一compiled目录值，显式拥有完整选择与真实until阶段；callback不捕获拥有容器。
struct FacilityHolderPhaseSelection { FacilityHolderProfileSelection selection;ExactStage until_stage=ExactStage::action_completions; };
// 原holder类型由Common Action/contract或domain_activity真实源读出，不从claim的字符串猜权限。
class FacilityHolderSourceRead {
public:
    virtual ~FacilityHolderSourceRead()=default;
    virtual Outcome<FacilityHolderKind> read_facility_holder_kind(const ReservationRef&) const=0;
    virtual Status validate_facility_holder_selection(const FacilityHolderProfileSelection&) const=0;
    // 原业务保留源双向枚举；实现只能读各专业原持久记录，不从S0表拼回证明。
    virtual Outcome<std::vector<FacilityClaimDependency>> enumerate_holder_claim_dependencies(const FacilitySlotRef&) const=0;
    virtual Status validate_holder_claim_admission(const FacilityClaim2&,const FacilityClaimMutationReceipt&) const=0;
    virtual Status validate_holder_claim_close(const FacilityClaim2&,FacilityClaimCloseReason,const FacilityClaimMutationReceipt&) const=0;
    virtual Status validate_holder_mutation_source(const FacilitySlotRef&,const FacilityClaimMutationReceipt&) const=0;
    virtual Outcome<bool> has_actual_facility_service_started(const FacilityClaim2&,const WorldMoment&)const=0;

};
struct FacilityClaimDue { StableId claim_id;FacilitySlotRef slot_ref;Integer claim_set_revision=0;WorldMoment due_at; };
// A genuine elapsed claim window is already a cause. Its original admission
// remains an original reference; no future capacity-change SourceRef is forged.
struct FacilityClaimUntilRequest {
    FacilityClaim2 due_claim;
    FacilityClaimReceiptRef admission_receipt_ref;
    WorldMoment occurred_at;
};
// Private two-phase owner output, never a saved/shared claim. Capacity revision
// does not exist until the original whole-slot finalizer has adopted its real
// retention/capacity consequence. All other owner-selected fields are fixed.
struct PreparedFacilityClaimAdmission {
    StableId claim_id;FacilitySlotRef slot_ref;ReservationRef holder;
    WorldMoment from,until;Integer units=0;
    SourceRef usage_right_ref,source_result_ref;
};
struct PreparedFacilityClaimMutation {
    Integer claim_set_revision=0;WorldMoment occurred_at;
    SourceRef source_result_ref;Integer source_schema=0;
    std::vector<ClosedFacilityClaim> closed_claims;
    std::vector<PreparedFacilityClaimAdmission> admitted_claims;
};
struct FacilityUntilReconciliationProgress {
    const std::vector<FacilityClaim2>& ordered_claims;
    const std::vector<PreparedFacilityClaimMutation>& prior_owner_results;
    std::size_t position=0;
};
// 原cause已经实际成立，新capacity source尚未产生；不拿临时预测冒已登记的source。
struct FacilityCapacityReconciliationRequest {FacilitySlotRef slot_ref;FacilitySourceRef original_change;WorldMoment occurred_at;};
struct FacilityReconciliationProgress {
    const std::vector<FacilityClaim2>& ordered_claims;
    const std::vector<FacilityClaimMutationReceipt>& prior_owner_results;
    std::size_t position=0;
};
// 原owner先形成身体/工量/暂停或真实续保结果，再返回同候选晚绑的共享mutation。
struct FacilityHolderRegistration {
    FacilityHolderProfileSelection selection;
    // 原owner真实释放/续保所在阶段；缺少登记不可默认为晚期清场。
    std::optional<ExactStage> until_stage;
    CheckpointInlineFunction<Outcome<FacilityClaimMutationReceipt>(WorldBundle&,const FacilityClaim2&,const WorldMoment&,ExactPhaseWriteContext&)> apply_until;
    // 全slot原优先序逐claim消费；已处理结果参与原可容整units规则，不能逐holder各自重置预算。
    CheckpointInlineFunction<Outcome<FacilityClaimMutationReceipt>(WorldBundle&,const FacilityCapacityReconciliationRequest&,const FacilityReconciliationProgress&,ExactPhaseWriteContext&)> reconcile_capacity;
    // Only the real until whole-slot path consumes this tail. Missing selected
    // original holder support is a technical failure, never a no-op receipt.
    CheckpointInlineFunction<Outcome<PreparedFacilityClaimMutation>(WorldBundle&,const FacilityClaimUntilRequest&,const FacilityUntilReconciliationProgress&,ExactPhaseWriteContext&)> reconcile_until;
};
// 同一个真实provider最后汇总原retention条目/普通结果，写capacity及physical绑定后回唯一root。
struct FacilityCapacityReconciliationRegistration {
    StableId provider_domain,profile_id;Integer profile_version=0;
    CheckpointInlineFunction<Status(const WorldBundle&,const FacilityCapacityReconciliationRequest&)> validate_request;
    CheckpointInlineFunction<Outcome<std::optional<FacilityClaimMutationReceipt>>(WorldBundle&,const FacilityCapacityReconciliationRequest&,const std::vector<FacilityClaimMutationReceipt>&,ExactPhaseWriteContext&)> finalize;
};
struct FacilityClaimUntilReconciliationRegistration {
    StableId provider_domain,profile_id;Integer profile_version=0;
    CheckpointInlineFunction<Status(const WorldBundle&,const FacilityClaimUntilRequest&)> validate_request;
    CheckpointInlineFunction<Outcome<FacilityClaimMutationReceipt>(WorldBundle&,const FacilityClaimUntilRequest&,const std::vector<PreparedFacilityClaimMutation>&,ExactPhaseWriteContext&)> finalize_until;
};
// 仅从完整真实handler登记导出；有限kind/阶段/唯一性先核，不能从作者输入自授provider。
Outcome<std::vector<FacilityHolderPhaseSelection>> compile_facility_holder_profiles(const std::vector<FacilityHolderRegistration>&);
Status validate_facility_holder_registry(const std::vector<FacilityHolderRegistration>&,const FacilityHolderSourceRead&);
// 预测仅引用现存claim/slot revision，无新ID/成果；所有过期责任均进入强制due，不受Director预算。
Outcome<std::vector<FacilityClaimDue>> predict_facility_claim_dues(const FacilityClaimsState&,const WorldMoment&,const WorldMoment&,ExactArithmeticBudget={});
Outcome<FacilityClaimReceiptRef> apply_facility_claim_due_in_candidate(WorldBundle&,FacilityClaimsState&,const FacilityClaimsCatalog&,const FacilityClaimDue&,const std::vector<FacilityHolderRegistration>&,const FacilityHolderSourceRead&,const FacilityClaimSourceRead&,ExactPhaseWriteContext&,ExactArithmeticBudget={});
// Prepare the same original whole-slot holder/finalizer result without adopting
// S0 claims. The live private candidate owns all original business writes; a
// caller must immediately adopt the returned receipt or discard that candidate.
// This also lets the existing until writer return one final receipt to S0.
Outcome<std::optional<FacilityClaimMutationReceipt>> prepare_facility_capacity_reconciliation_in_candidate(WorldBundle&,const FacilityClaimsState&,const FacilityCapacityReconciliationRequest&,const FacilityCapacityReconciliationRegistration&,const std::vector<FacilityHolderRegistration>&,const FacilityHolderSourceRead&,const FacilityClaimSourceRead&,ExactPhaseWriteContext&,ExactArithmeticBudget={});
// Called inside the actual apply_until callback. The same original whole-slot
// order and result-coverage checks run, but the cause is the true due episode.
// Returns a fully bound real receipt; only the outer apply_until path adopts S0.
Outcome<FacilityClaimMutationReceipt> prepare_facility_until_reconciliation_in_candidate(WorldBundle&,const FacilityClaimsState&,const FacilityClaimUntilRequest&,const FacilityClaimUntilReconciliationRegistration&,const std::vector<FacilityHolderRegistration>&,const FacilityHolderSourceRead&,const FacilityClaimSourceRead&,ExactPhaseWriteContext&,ExactArithmeticBudget={});
// 容量原writer调用此入口处理整slot：已选每种holder都须真实注册，未知者阻断整个候选。
Outcome<std::vector<FacilityClaimReceiptRef>> reconcile_facility_capacity_in_candidate(WorldBundle&,FacilityClaimsState&,const FacilityClaimsCatalog&,const FacilityCapacityReconciliationRequest&,const FacilityCapacityReconciliationRegistration&,const std::vector<FacilityHolderRegistration>&,const FacilityHolderSourceRead&,const FacilityClaimSourceRead&,ExactPhaseWriteContext&,ExactArithmeticBudget={});
}
