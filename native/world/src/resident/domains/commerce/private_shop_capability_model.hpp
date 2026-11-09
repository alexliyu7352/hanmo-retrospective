// Fixed P29 original operation/profile and applied-period proofs. No query DTO,
// second body, elapsed clock or shadow operating status is retained here.
#pragma once
#include "private_shop_operation_sources_model.hpp"
namespace hanmo::resident {
enum class PrivateShopOperationProfileKind {legacy_attendance,purpose_capability1};
struct PrivateShopOperationProfile {
 StableId operation_ref;
 PrivateShopOperationProfileKind operation_profile=PrivateShopOperationProfileKind::legacy_attendance;
 std::optional<MomentOwnerSource> migration_source;
 std::optional<ActorFoundationRuleRef> accepted_purpose_ref;
 std::vector<MomentOwnerSource> acceptance_condition_source_refs;
};
enum class PrivateShopOperatorRole {owner_operator,accepted_staff};
struct PrivateShopServiceApplied {
 WorkPeriodRef2 work_period_ref;
 SourceRef original_labor_receipt_ref;
};
struct PrivateShopParticipationStopped {
 FixedServiceTransitionRef original_operation_pause_or_ineligibility_result_ref;
};
using PrivateShopActivityEvidence=std::variant<PrivateShopServiceApplied,PrivateShopParticipationStopped>;
// Finite original address union; neither alternative is a synthetic proof fact.
struct PrivateShopActualLaborSourceRef {SourceRef source_ref;};
using PrivateShopCapabilitySegmentSource=std::variant<PrivateShopActualLaborSourceRef,FixedServiceTransitionRef>;
struct ShopOperationCapabilityBinding {
 StableId operation_ref;
 PrivateShopCapabilitySegmentSource segment_source_ref;
 StableId actor_ref;
 PrivateShopOperatorRole role=PrivateShopOperatorRole::owner_operator;
 std::optional<SourceRef> staff_contract_ref;
 ActorFoundationRuleRef purpose_ref;
 MomentSpan actual_span;
 std::vector<MomentOwnerSource> condition_source_refs;
 StableId capability_rule_id;
 Integer capability_rule_version=0;
 bool allowed=false;
 Integer efficiency_bp=0;
 PrivateShopActivityEvidence activity_evidence;
};
struct PrivateShopOperationCapabilitiesState {
 Integer schema_version=1;
 std::vector<PrivateShopOperationProfile> operation_profiles;
 std::vector<ShopOperationCapabilityBinding> period_capability_bindings;
};
Outcome<PrivateShopOperationCapabilitiesState> decode_private_shop_operation_capabilities(const BoundaryValue&);
Outcome<BoundaryValue> encode_private_shop_operation_capabilities(const PrivateShopOperationCapabilitiesState&);
Status emit_private_shop_operation_capabilities(ProjectionWriter&,const StableId&,const PrivateShopOperationCapabilitiesState&);
Status emit_private_shop_operation_capabilities_fields(ProjectionWriter&,const PrivateShopOperationCapabilitiesState&);
Status visit_private_shop_operation_capabilities(CodecVisitor&,PrivateShopOperationCapabilitiesState&);
}
