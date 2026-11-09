// FH §5.5 的原承运接受持值，是完整 Care 根第十九表的唯一行型。
// 只保存实际接受与一次撤回；不创建旅程、装载、预算或操船权限。
#pragma once
#include "care_body_scope.hpp"
#include "care_clearance_origin.hpp"
#include "care_facility_use.hpp"
#include "care_transport_carrier.hpp"
#include "life_age_basis.hpp"
#include "resident/domains/growth/growth_fixed_service_model.hpp"
#include "resident/domains/travel/transport_authority.hpp"

namespace hanmo::resident {
// 目的地业务键分别归其原 owner；不以一个自由 domain 字符串选择接收者。
struct CareMedicalReceivingRef {
  StableId case_id;
};
struct CareResidenceReceivingRef {
  StableId agreement_id, line_id;
};
using CareReceivingBusinessRef =
    std::variant<CareArrangementRef, CareMedicalReceivingRef,
                 CareResidenceReceivingRef>;

// receiving_kind 由唯一业务分支编码；接受源及同地地点仍须原责任 owner 验证。
struct CareTransportLocalReceiving {
  CareReceivingBusinessRef business_ref;
  MomentOwnerSource accepted_receiving_ref;
  StableId place_ref;
};

// 三支只保本人已成立责任的来源，不让本次后发接受自证过去知情。
struct CareTransportMedicalKnowledge {
  StableId case_id, referral_id;
  MomentOwnerSource authorized_viewer_ref, original_offer_source_ref;
};
struct CareTransportResidenceKnowledge {
  StableId agreement_id, line_id;
  MomentOwnerSource actual_provider_or_departure_mandate_ref;
};
struct CareTransportAcceptedCareKnowledge {
  StableId accepted_care_terms_ref, accepted_caregiver_ref;
};
using CareTransportKnowledgeBasis =
    std::variant<CareTransportMedicalKnowledge, CareTransportResidenceKnowledge,
                 CareTransportAcceptedCareKnowledge>;

// 无新增费用不免除未来 Travel 的原真实费用；接受不预扣或占用影子预算。
struct CareTransportNoAdditionalPayment {};
struct CareTransportOriginalExpense {
  TransportExpenseAuthority authority;
  MomentOwnerSource accepted_budget_or_fee_terms_ref;
};
using CareTransportExpenseBasis = std::variant<CareTransportNoAdditionalPayment,
                                               CareTransportOriginalExpense>;

// command_kind 在 codec 固定 AcceptFacilityClearanceTransport；Actor
// 唯一取外层。 route 复用原 S0/Growth 命令 route，不借 Family
// 评分表达的不同布局。
struct CareTransportActorDecision {
  Integer request_seq = 0;
  StableId result_id;
  LifeActorRoute route_basis;
  LifeAgeBasisRef age_basis_ref;
  std::vector<CharacterConditionSourceRef> condition_source_refs;
  WorldMoment expressed_at;
  FieldOrder field_order;
};

enum class CareTransportWithdrawalReason {
  original_responsibility_withdrawn,
  carrier_no_longer_accepted
};
// 仅原接受值的一次真实终点，不另外创建 withdrawal 根或覆盖过去接受输入。
struct CareTransportWithdrawal {
  StableId withdrawal_id;
  WorldMoment occurred_at;
  LifeActualCommandBinding command_binding;
  LifeActorRoute route_basis;
  CareTransportWithdrawalReason reason =
      CareTransportWithdrawalReason::original_responsibility_withdrawn;
  FrozenFacilityCapabilityInputs actor_capability_inputs;
  FieldOrder field_order;
};

// 原十七字段顺序由唯一 codec 表给出；withdrawal 之外的已接受条款不可变。
struct FacilityTransportAcceptance {
  StableId acceptance_id;
  Integer source_schema = 1;
  FacilityClearanceObligation obligation;
  ActorFoundationRuleRef clearance_rule_ref;
  CareBodyScopeSlice scope;
  CareTransportCarrier carrier;
  CareTransportLocalReceiving destination;
  StableId operator_actor_id;
  MomentOwnerSource operator_authority_ref;
  CareTransportKnowledgeBasis knowledge_basis;
  std::vector<MomentOwnerSource> patient_or_care_consent_refs;
  CareTransportExpenseBasis expense_basis;
  WorldMoment accepted_at;
  CareTransportActorDecision actor_decision;
  FrozenFacilityCapabilityInputs actor_capability_inputs;
  std::optional<CareTransportWithdrawal> withdrawal;
  FieldOrder field_order;
};
class CodecVisitor;
class ProjectionWriter;
// 完整根第十九表的行 codec；格式有效不等真实同意、装载或 source closure。
Status visit_care_facility_transport_acceptance(CodecVisitor &,
                                                FacilityTransportAcceptance &);
Status emit_care_facility_transport_acceptance_fields(
    ProjectionWriter &, const FacilityTransportAcceptance &);
Outcome<BoundaryValue>
encode_care_facility_transport_acceptance(const FacilityTransportAcceptance &);
Outcome<FacilityTransportAcceptance>
decode_care_facility_transport_acceptance(const BoundaryValue &);
// 只核已读完整字段之间的关系；原职责/身体/权源/采用仍由真实 reader 逐项验证。
Status validate_care_facility_transport_acceptance_relations(
    const FacilityTransportAcceptance &);
} // namespace hanmo::resident
