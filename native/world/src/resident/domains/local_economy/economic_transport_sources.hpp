// S1仅提交真实首次运动/搬运源；经济owner沿唯一Common钱包双腿付款并保存原责任。
#pragma once
#include "local_economy_model.hpp"
#include "obligations/obligation_sources_read.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/travel/transport_authority.hpp"
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
enum class TransportExpensePurpose { base_fare,handling,toll,safe_return };
struct TransportExpenseRequest { SourceRef actual_movement_or_handling_source; Integer source_schema=0; TransportExpenseAuthority authority; };
// S1原reader从真实journey/segment/handling及已锁cost rule给出，不接受caller自报价。
struct ActualTransportExpense { SourceRef source_ref; Integer source_schema=0; StableId journey_id,segment_or_handling_id,cost_rule_id,payee_wallet_id,destination_id; TransportExpensePurpose purpose=TransportExpensePurpose::base_fare; Integer amount=0; WorldMoment occurred_at; };
class TransportExpenseSourceRead {
public:virtual ~TransportExpenseSourceRead()=default;
 virtual Outcome<ActualTransportExpense> read_actual_transport_expense(const SourceRef&,Integer schema)const=0;
 virtual Outcome<WorldMoment> current_transport_expense_moment()const=0;
 virtual Status validate_transport_expense_authority(const SourceRef&,Integer,const TransportExpenseAuthority&)const=0;
};
struct TransportPaymentReceipt { StableId id; TransportExpenseRequest request; ActualTransportExpense expense; StableId payer_wallet_id,original_obligation_id,original_arrear_id,original_payment_receipt_id; };
// Runtime owner collection only. Physical persistence is embedded once in each original
// S1 journey-step/crossing/handling source; this is not a fifth transport section.
struct EconomicTransportExpenseState { CheckpointVector<TransportPaymentReceipt> payments; };
struct TransportExpenseAllowance { StableId payer_wallet_id,authority_source_id; Integer authority_revision=0,gross_limit=0,gross_spent_before=0; SubjectRef principal,beneficiary; };
// Original receiving permission comes from the actual fare/tariff/handling
// owner at this expense cut, never from today's wallet Owner. No new state.
struct TransportExpenseReceiver { SubjectRef creditor;MomentOwnerSource receiver_source; };
class TransportExpenseReceiverRead {public:virtual ~TransportExpenseReceiverRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Outcome<TransportExpenseReceiver> read_original_transport_receiver(const ActualTransportExpense&)const=0;
};
struct TransportExpenseResponsibility {
 SubjectRef debtor,creditor;StableId payer_wallet_id;MomentOwnerSource executor_authorization,receiver_source;
 std::vector<MomentOwnerSource> consent_refs;
};
// 权限reader由S6预算、S7真私约/自资、S5真家庭已接受政策分别核，不能由wallet Owner猜。
enum class TransportExpenseAuthorityCheck { new_charge,existing_charge };
class TransportExpenseAuthorityRead {
public:virtual ~TransportExpenseAuthorityRead()=default;
 virtual Outcome<TransportExpenseAllowance> read_transport_expense_allowance(const TransportExpenseAuthority&,const ActualTransportExpense&,TransportExpenseAuthorityCheck check=TransportExpenseAuthorityCheck::new_charge)const=0;
// Source-local accepted responsibility only. Never call P11 balance/whole
 // financial validation here; live allowance/cap permission is a separate leg.
 virtual Outcome<TransportExpenseResponsibility> read_original_transport_responsibility(
  const TransportExpenseAuthority&,const ActualTransportExpense&,TransportExpenseAuthorityCheck)const {
  return {false,{},common_failure("OriginalTransportResponsibilitySourceRequired",{},ErrorKind::unsupported_capability).error};
 }
};
struct EconomicTransportPaymentContext {
 const TransportExpenseSourceRead& actual_sources;const TransportExpenseAuthorityRead& authorities;
 const EconomicObligationReadContext* obligations=nullptr;
 EconomicObligationWriteContext* obligation_writes=nullptr;
};
Outcome<TransportPaymentReceipt> pay_transport_expense_in_candidate(CommonStore&,LocalEconomyState&,EconomicTransportExpenseState&,const EconomicTransportPaymentContext&,const TransportExpenseRequest&);
Outcome<const TransportPaymentReceipt*> read_transport_payment(const EconomicTransportExpenseState&,const SourceRef&,Integer schema);
Status validate_economic_transport_payments(const CommonStore&,const LocalEconomyState&,const EconomicTransportExpenseState&,const EconomicTransportPaymentContext&);
Outcome<std::optional<WorldMoment>> read_economic_transport_fact_moment(const CommonStore&,const LocalEconomyState&,const EconomicTransportExpenseState&,const SourceRef&);
class ProjectionWriter;
Outcome<BoundaryValue> encode_transport_payment_receipt(const TransportPaymentReceipt&);
Outcome<TransportPaymentReceipt> decode_transport_payment_receipt(const BoundaryValue&);
Status visit_transport_payment_receipt(CodecVisitor&,TransportPaymentReceipt&);
Status visit_transport_payment_receipt(CodecVisitor&,const char* name,TransportPaymentReceipt&);
Status emit_transport_payment_receipt_fields(ProjectionWriter&,const TransportPaymentReceipt&);
Status emit_transport_payment_receipt(ProjectionWriter&,const StableId& name,const TransportPaymentReceipt&);
// Internal aggregate helper only; no independent persisted section registration.
Outcome<BoundaryValue> encode_economic_transport_expenses(const EconomicTransportExpenseState&);
Outcome<EconomicTransportExpenseState> decode_economic_transport_expenses(const BoundaryValue&);
Status visit_economic_transport_expenses(CodecVisitor&,EconomicTransportExpenseState&);
Status emit_economic_transport_expenses(ProjectionWriter&,const EconomicTransportExpenseState&);
}
