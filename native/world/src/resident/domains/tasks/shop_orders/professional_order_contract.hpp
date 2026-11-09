// 固定 C3§3/6 原订单行的静态绑定与一次完成费。此处不是旧采购 TaskShopOrder。
#pragma once
#include "resident/domains/services/service_actual_termination_contract.hpp"
#include "resident/model/objective_evidence.hpp"
namespace hanmo::resident {
struct OrderedCopyJob {SourceRef copy_activity_ref;};
struct OrderedInfusionJob {StableId job_id;};
enum class OrderedArcaneKind {recharge,effect_maintenance,appraisal};
struct OrderedArcaneJob {StableId service_id;OrderedArcaneKind kind=OrderedArcaneKind::recharge;};
using OrderedProfessionalJobRef=std::variant<OrderedCopyJob,OrderedInfusionJob,OrderedArcaneJob>;
struct OrderServiceAcceptanceKey {StableId order_id,line_id;TermsRevisionRef first_accepted_terms_ref;};
struct OrderedProfessionalExecutionMemberRef {OrderServiceAcceptanceKey acceptance_key;Integer execution_ordinal=0;};
struct OrderedReturnCustomerInstance {StableId item_instance_id;SourceRef original_custody_terms_ref,recipient_terms_ref;};
struct OrderedDeliverProducedCopies {SourceRef copy_activity_ref;Integer required_output_count=0;SourceRef delivery_terms_ref;};
struct OrderedDeliverProfessionalResult {OrderedProfessionalJobRef job;SourceRef recipient_terms_ref,disclosure_terms_ref;};
using OrderedFollowThroughKind=std::variant<OrderedReturnCustomerInstance,OrderedDeliverProducedCopies,OrderedDeliverProfessionalResult>;
struct OrderedFollowThroughRef {OrderServiceAcceptanceKey acceptance_key;Integer obligation_ordinal=0;OrderedFollowThroughKind kind;};
struct OrderedSingleCompleteExecution {};
struct OrderedOriginalReworkClause {TermsRevisionRef terms_ref;Integer clause_ordinal=0;};
using OrderedProfessionalAttemptRule=std::variant<OrderedSingleCompleteExecution,OrderedOriginalReworkClause>;
enum class OrderedCompletionFeeRule {ordered_professional_completion_v1};
struct OrderedProfessionalBinding {
 OrderServiceAcceptanceKey acceptance_key;OrderedProfessionalJobRef job;
 TermsRevisionRef professional_terms_ref;ProfessionalCriterion accepted_criterion;
 OrderedCompletionFeeRule fee_rule=OrderedCompletionFeeRule::ordered_professional_completion_v1;
 SourceRef fee_terms_ref;std::vector<OrderedFollowThroughRef> accepted_follow_through;
 SourceRef continuity_terms_ref;OrderedProfessionalAttemptRule attempt_rule;
};
struct OrderedInitialAcceptance {};
struct OrderedReworkAuthority {TermsRevisionRef terms_ref;Integer clause_ordinal=0,preceding_execution_ordinal=0;};
using OrderedProfessionalExecutionAuthority=std::variant<OrderedInitialAcceptance,OrderedReworkAuthority>;
struct OrderedProfessionalExecutionMember {
 OrderServiceAcceptanceKey acceptance_key;Integer execution_ordinal=0;OrderedProfessionalJobRef job;
 TermsRevisionRef professional_terms_ref;OrderedProfessionalExecutionAuthority authority;
};
struct OrderedCompletionFeeTerms {
 OrderServiceAcceptanceKey acceptance_key;Integer fee_item_ordinal=0,quoted_qian=0;
 ProfessionalCriterion criterion;SourceRef payment_terms_ref;
};
struct OrderedCompletionChargeKey {OrderServiceAcceptanceKey acceptance_key;Integer fee_item_ordinal=0;};
struct OrderedCompletionChargeSource {
 OrderedCompletionChargeKey charge_key;SourceRef fee_terms_ref;OrderedProfessionalJobRef job;
 OutcomeFactRef qualifying_outcome;ProfessionalCriterion criterion;Integer earned_qian=0,earned_tick=0;
};
// 结构校验不代替双方真实接受、原专业 owner 和合法返工权的反读。
Status validate_order_service_acceptance_key(const OrderServiceAcceptanceKey&);
Status validate_ordered_professional_binding_shape(const OrderedProfessionalBinding&);
Status validate_ordered_execution_member_shape(const OrderedProfessionalExecutionMember&);
Status validate_ordered_completion_fee_terms_shape(const OrderedCompletionFeeTerms&);
Status validate_ordered_completion_charge_shape(const OrderedCompletionChargeSource&);
bool same_order_service_acceptance_key(const OrderServiceAcceptanceKey&,const OrderServiceAcceptanceKey&);
bool same_ordered_professional_job(const OrderedProfessionalJobRef&,const OrderedProfessionalJobRef&);
Outcome<ProfessionalOutcomeKind> ordered_professional_completion_kind(const OrderedProfessionalJobRef&);

// 单原 record 的严格 JSON/checkpoint/projection；不把四叶冒整段 professional_shop_orders。
Outcome<OrderedProfessionalBinding> decode_ordered_professional_binding(const BoundaryValue&);
Outcome<BoundaryValue> encode_ordered_professional_binding(const OrderedProfessionalBinding&);
Status visit_ordered_professional_binding_checkpoint(CodecVisitor&,OrderedProfessionalBinding&);
Status emit_ordered_professional_binding_fields(ProjectionWriter&,const OrderedProfessionalBinding&);
Outcome<OrderedProfessionalExecutionMember> decode_ordered_execution_member(const BoundaryValue&);
Outcome<BoundaryValue> encode_ordered_execution_member(const OrderedProfessionalExecutionMember&);
Status visit_ordered_execution_member_checkpoint(CodecVisitor&,OrderedProfessionalExecutionMember&);
Status emit_ordered_execution_member_fields(ProjectionWriter&,const OrderedProfessionalExecutionMember&);
Outcome<OrderedCompletionFeeTerms> decode_ordered_completion_fee_terms(const BoundaryValue&);
Outcome<BoundaryValue> encode_ordered_completion_fee_terms(const OrderedCompletionFeeTerms&);
Status visit_ordered_completion_fee_terms_checkpoint(CodecVisitor&,OrderedCompletionFeeTerms&);
Status emit_ordered_completion_fee_terms_fields(ProjectionWriter&,const OrderedCompletionFeeTerms&);
Outcome<OrderedCompletionChargeSource> decode_ordered_completion_charge(const BoundaryValue&);
Outcome<BoundaryValue> encode_ordered_completion_charge(const OrderedCompletionChargeSource&);
Status visit_ordered_completion_charge_checkpoint(CodecVisitor&,OrderedCompletionChargeSource&);
Status emit_ordered_completion_charge_fields(ProjectionWriter&,const OrderedCompletionChargeSource&);
}
