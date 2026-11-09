#include "material_aid_compensation_basis.hpp"
#include "material_aid_internal.hpp"
namespace hanmo::resident {
Outcome<MaterialAidCompensationBasis> read_material_aid_compensation_basis(
 const MaterialAidReadContext&x,const ObligationRef&ref,
 const std::optional<SourceRef>&late){try{
 using namespace aid_detail;
 const auto&o=obligation(x,ref);validate_established_terms(x,o);
 const auto&c=commitment(x,o.schedule_commitment_id);
 const auto t=terms(x,schedule(x,c.schedule_id));
 const auto*policy=std::get_if<TreatyBoundedCompensation>(&t.original_term->late_policy);
 if(!policy||policy->maximum_amount<0||policy->currency_id.empty()||policy->obligation_owner.empty())
  fail("MaterialAidOriginalBoundedCompensationRequired",o.id);
 const WorldMoment deadline{o.delivery_deadline_tick,{}};
 if(compare(deadline,now(x))>0)fail("MaterialAidCompensationBeforeDeadline",o.id);
 const auto&established=c.established_occurrence_refs[static_cast<std::size_t>(o.occurrence.ordinal)];
 std::optional<std::size_t>cut;
 if(late){
  if(!same_source_ref(*late,material_aid_deadline_source(late->id))||!o.deadline_result_ref||*o.deadline_result_ref!=late->id)
   fail("MaterialAidCompensationOriginalLateRequired",o.id);
  const auto*d=x.state.deadline_results.find(late->id);
  if(!d||d->id!=late->id||d->obligation_id!=o.id||compare(d->occurred_at,deadline))
   fail("MaterialAidCompensationOriginalLateRequired",o.id);
  before(x.common,established,*late);cut=header(x.common,*late);
 }else if(o.deadline_result_ref||compare(deadline,now(x)))
  fail("MaterialAidCompensationActualBoundaryRequired",o.id);
 // Authenticate complete original receiving coverage before testing absence.
 // Omitting a real receiving source must not manufacture a late liability.
 const auto receipts=take(x.resources.enumerate_actual_aid_receipts(o.occurrence));
 for(std::size_t i=0;i<receipts.size();++i){
  for(std::size_t j=0;j<i;++j)if(same(receipts[i],receipts[j]))fail("DuplicateMaterialAidOriginalReceiptLeg",o.id);
  const auto original=original_receipt(x,o,receipts[i]);
  if(compare(original.occurred_at,deadline)>0)continue;
  if(cut&&header(x.common,receipts[i].source.source_ref)>=*cut)continue;
  const MaterialAidReceipt*adopted=nullptr;
  for(const auto&id:o.receipt_result_refs){const auto*r=x.state.receipts.find(id);
   if(r&&same(r->actual_receipt_ref,receipts[i])){if(adopted)fail("DuplicateMaterialAidReceiptAdoption",o.id);adopted=r;}}
  if(!adopted)fail("MaterialAidOriginalReceiptCoverageMismatch",o.id);
  receipt(x,o,*adopted);
  if(cut&&header(x.common,material_aid_receipt_source(adopted->id))>=*cut)
   fail("MaterialAidReceivingAdoptionAfterLateCut",adopted->id);
 }
 bool unpaid=false;
 for(const auto&measure:remaining(x,o,&deadline,late?&*late:nullptr))unpaid|=measure.remaining>0;
 if(!unpaid)fail("MaterialAidFalseLateResult",o.id);
 return {true,{&o,policy,o.resource_acceptance_ref,established,deadline,policy->maximum_amount},{}};
 }catch(aid_detail::Failure&e){return {false,{},std::move(e.error)};}}
}
