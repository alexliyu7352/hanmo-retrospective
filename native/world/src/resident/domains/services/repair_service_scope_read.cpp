#include "repair_service_scope_read.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {namespace {


template<class T>void check_rows(std::size_t n){if(n>SIZE_MAX/sizeof(T))throw std::bad_alloc();}
enum class Field {charge,payable,continuity};
struct Counts {std::size_t charge=0,payable=0,continuity=0;};
class Finance final:public RepairServiceFinancialSourceVisitor {
 const CommonStore&common_;const RepairAcceptedSource&accepted_;const RepairTerminal&terminal_;
 ServiceActualTerminationResult*out_;Counts count_;std::size_t settlements_=0;
 Status add(const SourceRef&r,Field field){
  std::size_t*n=field==Field::charge?&count_.charge:field==Field::payable?&count_.payable:&count_.continuity;
  if(*n==SIZE_MAX)throw std::bad_alloc();++*n;
  if(out_){const auto capacity=field==Field::charge?out_->earned_charge_refs.capacity():field==Field::payable?out_->existing_payable_refs.capacity():out_->custody_or_continuity_refs.capacity();
   if(*n>capacity)return common_failure("RepairServiceResultChangedDuringRead",terminal_.repair_result_id,ErrorKind::internal);
   if(field==Field::charge)out_->earned_charge_refs.emplace_back(r);
   else if(field==Field::payable)out_->existing_payable_refs.push_back(r);
   else out_->custody_or_continuity_refs.push_back(r);
  }return common_success();
 }
 Status header(const char*domain,const char*kind,const StableId&id,Field field){
  const auto*h=common_.state().facts.find(id);if(!h||h->id!=id||h->domain!=domain||h->kind!=kind)return common_failure("RepairServiceFinancialHeaderMismatch",id,ErrorKind::internal);
  // Sizing must precede copying the source's three strings, including temporary
  // reference construction. The actual header is already owned by Common.
  if(!out_){std::size_t*n=field==Field::charge?&count_.charge:&count_.payable;if(*n==SIZE_MAX)throw std::bad_alloc();++*n;return common_success();}
  std::size_t*n=field==Field::charge?&count_.charge:&count_.payable;if(*n==SIZE_MAX)throw std::bad_alloc();++*n;
  const auto capacity=field==Field::charge?out_->earned_charge_refs.capacity():out_->existing_payable_refs.capacity();if(*n>capacity)return common_failure("RepairServiceResultChangedDuringRead",id,ErrorKind::internal);
  SourceRef ref{h->domain,h->kind,h->id};if(field==Field::charge)out_->earned_charge_refs.emplace_back(std::move(ref));else out_->existing_payable_refs.push_back(std::move(ref));return common_success();
 }
 Status original_terminal(const MomentOwnerSource&s)const{
  const auto*h=common_.state().facts.find(terminal_.repair_result_id);
  if(!h||s.source_schema!=terminal_.source_schema||s.source_ref.domain!=h->domain||s.source_ref.kind!=h->kind||s.source_ref.id!=h->id)return common_failure("RepairServiceFinancialTerminalMismatch",terminal_.repair_result_id);
  return common_success();
 }
public:
 Finance(const CommonStore&c,const RepairAcceptedSource&a,const RepairTerminal&t,ServiceActualTerminationResult*out):common_(c),accepted_(a),terminal_(t),out_(out){}
 const Counts&counts()const noexcept{return count_;}
 Status finish()const{return settlements_==1?common_success():common_failure("RepairServiceOriginalSettlementRequired",terminal_.repair_result_id);}
 Status contract(const RepairServiceContractSource&c,const RepairAcceptedSource&a)override{
  const auto&r=accepted_.terms.fee_terms_ref;
  if(&a!=&accepted_||r.source_schema!=1||r.source_ref.domain!="commerce"||r.source_ref.kind!="repair_service_contract_accepted"||r.source_ref.id!=c.id||c.terms.instance_id!=a.terms.instance_id)return common_failure("RepairServiceAcceptedFeeScopeMismatch",a.id);
  return common_success();
 }
 Status charge(const RepairServiceChargeSource&c)override{auto s=header("commerce","repair_service_charge",c.id,Field::charge);if(s.ok&&c.original_arrear_id)s=header("local_economy","contract_payment_due",*c.original_arrear_id,Field::payable);return s;}
 Status payment(const RepairServicePaymentSource&,const PaymentReceipt&)override{return common_success();}
 Status settlement(const RepairServiceTerminationSettlementSource&v)override{
  auto s=original_terminal(v.actual_terminal_ref);if(!s.ok)return s;if(++settlements_!=1)return common_failure("DuplicateRepairServiceSettlement",v.id);
  for(const auto&r:v.custody_or_continuity_refs){s=add(r.source_ref,Field::continuity);if(!s.ok)return s;}return common_success();
 }
};
}
Outcome<OwnedServiceActualTerminationResult> read_repair_service_actual_termination(
 const RepairReadContext&work,const RepairServiceReadContext&money,const RepairService&scope,const RepairTerminal&terminal){
 if(&work.common!=&money.common||&work.items!=&money.items||money.repair!=&work||!money.original.bound_to(work.common,work.items))return {false,{},common_failure("RepairServiceResultWorldMismatch",terminal.repair_result_id,ErrorKind::internal).error};
 auto now=work.clock.read_current_moment();if(!now.ok)return {false,{},now.error};auto money_now=money.clock.read_current_moment();if(!money_now.ok)return {false,{},money_now.error};auto order=compare_world_moments(now.value,money_now.value);if(!order.ok)return {false,{},order.error};if(order.value)return {false,{},common_failure("RepairServiceResultClockMismatch",terminal.repair_result_id,ErrorKind::internal).error};
 auto original=read_repair_service_internal_termination(work,scope,terminal);if(!original.ok)return {false,{},original.error};const auto&a=*original.value.acceptance;
 const auto*at=original.value.started?&original.value.started->occurred_at:original.value.unstarted?&original.value.unstarted->occurred_at:nullptr;
 if(!at)return {false,{},common_failure("RepairServiceActualTerminalRequired",terminal.repair_result_id).error};
 const auto*terms=std::get_if<SourceFactTerms>(&scope.repair_terms_ref);if(!terms)return {false,{},common_failure("RepairServiceOriginalAcceptedTermsRequired",scope.instance_id).error};
 OwnedServiceActualTerminationResult result;
 auto accepted_memory=CheckpointMemoryCharge(0);const MomentOwnerSource actual_acceptance{scope.repair_activity_ref,1};
 Finance sizing(work.common,a,terminal,nullptr);auto valid=visit_original_repair_service_financial_sources(money,actual_acceptance,sizing);if(valid.ok)valid=sizing.finish();if(!valid.ok)return {false,{},valid.error};const auto counts=sizing.counts();
 check_rows<ServiceEarnedChargeRef>(counts.charge);check_rows<SourceRef>(counts.payable);check_rows<SourceRef>(counts.continuity);(result.memory).grow(0);
 auto&out=result.value;out.termination_id=terminal.repair_result_id;out.scope=scope;out.original_terminal_source=terminal;out.accepted_fee_terms_ref=a.terms.fee_terms_ref.source_ref;out.reason=ServiceOwnerConfirmedUnavailability{terminal};out.effective_game_time.kind=ActorFoundationAt::Kind::exact_moment;out.effective_game_time.exact_moment=*at;
 out.earned_charge_refs.reserve(counts.charge);out.existing_payable_refs.reserve(counts.payable);out.custody_or_continuity_refs.reserve(counts.continuity);
 Finance filling(work.common,a,terminal,&out);valid=visit_original_repair_service_financial_sources(money,actual_acceptance,filling);if(valid.ok)valid=filling.finish();if(!valid.ok)return {false,{},valid.error};const auto&actual=filling.counts();
 if(actual.charge!=counts.charge||actual.payable!=counts.payable||actual.continuity!=counts.continuity)return {false,{},common_failure("RepairServiceResultChangedDuringRead",terminal.repair_result_id,ErrorKind::internal).error};
 valid=validate_service_actual_termination_result_shape(out,service_actual_termination_result_schema);if(!valid.ok)return {false,{},valid.error};return {true,std::move(result),{}};
}
}
