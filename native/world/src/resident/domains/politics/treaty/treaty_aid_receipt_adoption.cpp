#include "treaty_aid_receipt_adoption.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
std::size_t position(const CommonStore& c,const SourceRef& source){
 const auto* head=c.state().facts.find(source.id);
 if(!valid_source_ref(source)||!head||head->domain!=source.domain||head->kind!=source.kind)
  fail("TreatyAidReceiptActualSourceRequired",source.id);
 const auto& order=c.state().facts.order();const auto i=std::find(order.begin(),order.end(),source.id);
 if(i==order.end())fail("TreatyAidReceiptActualOrderRequired",source.id);
 return static_cast<std::size_t>(i-order.begin());
}
void context(const MaterialAidReadContext& f,const TreatyAidPerformanceReadContext& p){
 if(&f.common!=&p.common||&f.schedules!=&p.schedules||&f.terms!=&p.terms||!f.resources.bound_to(f.common))
  fail("TreatyAidReceiptCandidateMismatch");
 if(take(compare_world_moments(take(f.clock.read_current_moment()),take(p.clock.read_organization_authority_now()))))
  fail("TreatyAidReceiptClockMismatch");
}
const TreatyAidOccurrenceBinding& occurrence(const TreatyAidPerformanceReadContext& p,const StableId& debt){
 const TreatyAidOccurrenceBinding* found=nullptr;
 for(const auto& row:p.occurrence_bindings.rows())if(row.second.original_obligation_ref.owner_domain=="local_economy"&&
   row.second.original_obligation_ref.obligation_id==debt){
  if(found||row.first!=row.second.id)fail("TreatyAidReceiptOccurrenceAmbiguous",debt);
  found=take(read_treaty_aid_occurrence_binding(p,treaty_aid_occurrence_source(row.second),1));
 }
 if(!found)fail("TreatyAidReceiptOccurrenceRequired",debt);return *found;
}
std::size_t count(const std::vector<SourceRef>& refs,const SourceRef& source){
 return static_cast<std::size_t>(std::count_if(refs.begin(),refs.end(),[&](const auto& r){return same_source_ref(r,source);}));
}
// 此门只读已实际采用的S5行；上游event刚发生、S5尚未消费时不提前要求那次新腿
// 已在财务表。新腿全量反向核由下面原S5消费函数及其完整validator负责。
void coverage(const MaterialAidReadContext& f,const TreatyAidPerformanceReadContext& p){
 context(f,p);take(validate_treaty_aid_occurrence_prefixes(p));
 for(const auto& row:f.state.receipts.rows()){
  const auto& r=row.second;if(row.first!=r.id)fail("TreatyAidReceiptIdentityMismatch",row.first);
  const auto ref=material_aid_receipt_source(r.id);const auto& b=occurrence(p,r.obligation_id);
  const auto original=take(p.finances.read_material_aid_establishment(b.establishment_result_ref));
  take(p.finances.validate_material_aid_receipt(original,b.shipment_ref,ref));
  if(count(b.receipt_result_refs,ref)!=1)fail("TreatyAidReceiptBindingCoverage",r.id);
  if(position(f.common,treaty_aid_occurrence_source(b))>=position(f.common,ref))
   fail("TreatyAidReceiptBeforeOccurrence",r.id);
 }
 for(const auto& row:p.occurrence_bindings.rows()){
  const auto& b=row.second;std::optional<std::size_t> previous;
  for(const auto& ref:b.receipt_result_refs){
   if(!same_source_ref(ref,material_aid_receipt_source(ref.id)))fail("TreatyAidReceiptResultKindMismatch",ref.id);
   const auto* receipt=f.state.receipts.find(ref.id);
   if(!receipt||receipt->id!=ref.id||b.original_obligation_ref.owner_domain!="local_economy"||
      b.original_obligation_ref.obligation_id!=receipt->obligation_id)
    fail("TreatyAidReceiptOriginalDebtMismatch",ref.id);
   const auto current=position(f.common,ref);
   if(previous&&*previous>=current)fail("TreatyAidReceiptAdoptionOrderMismatch",b.id);
   previous=current;
  }
 }
}
}
Status validate_treaty_aid_receipt_adoption(const MaterialAidReadContext& f,
 const TreatyAidPerformanceReadContext& p){
 try{coverage(f,p);return common_success();}catch(const Failure& e){return {false,{},e.error};}
}
Outcome<std::vector<TreatyAidReceiptAdopted>> consume_and_adopt_treaty_aid_actual_event_in_candidate(
 CommonStore& c,TreatyMaterialAidState& state,OrderedTable<TreatyAidOccurrenceBinding>& bindings,
 const MaterialAidWriteContext& finance,const TreatyAidPerformanceReadContext& p,
 const SourceRef& event){try{
 const auto& f=finance.read;context(f,p);
 if(&c!=&f.common||&state!=&f.state||&bindings!=&p.occurrence_bindings||
    &finance.economy!=&f.economy||&finance.budget!=&f.budget)
  fail("TreatyAidReceiptCandidateMismatch");
 position(c,event);coverage(f,p);
 const auto results=take(consume_material_aid_actual_event_in_candidate(c,state,finance,event));
 // 发出源是原财务成果，但不进入S6收讫列表。排序使用真实append序，绝不按ID。
 std::vector<SourceRef> receipts;
 for(const auto& ref:results){
  position(c,ref);
  if(same_source_ref(ref,material_aid_issue_source(ref.id)))continue;
  if(!same_source_ref(ref,material_aid_receipt_source(ref.id)))fail("TreatyAidUnexpectedFinancialResult",ref.id);
  if(count(receipts,ref))fail("TreatyAidDuplicateFinancialReceipt",ref.id);
  receipts.push_back(ref);
 }
 std::sort(receipts.begin(),receipts.end(),[&](const auto& a,const auto& b){return position(c,a)<position(c,b);});
 const auto now=take(p.clock.read_organization_authority_now());
 std::vector<TreatyAidReceiptAdopted> adopted;adopted.reserve(receipts.size());
 for(const auto& ref:receipts){
  const auto* receipt=state.receipts.find(ref.id);
  if(!receipt||receipt->id!=ref.id||!same_source_ref(receipt->actual_receipt_ref.source.source_ref,event))
   fail("TreatyAidReceiptActualEventMismatch",ref.id);
  const auto& b=occurrence(p,receipt->obligation_id);
  const auto binding_ref=treaty_aid_occurrence_source(b);
  const auto original=take(p.finances.read_material_aid_establishment(b.establishment_result_ref));
  take(p.finances.validate_material_aid_receipt(original,b.shipment_ref,ref));
  auto* writable=bindings.find(b.id);if(!writable)fail("TreatyAidReceiptOccurrenceRequired",b.id);
  const auto existing=count(writable->receipt_result_refs,ref);
  if(existing>1)fail("TreatyAidDuplicateReceiptBinding",ref.id);
  if(!existing){
   if(take(compare_world_moments(receipt->occurred_at,now)))fail("TreatyAidReceiptAdoptionMomentMismatch",ref.id);
   if(position(c,binding_ref)>=position(c,ref))fail("TreatyAidReceiptBeforeOccurrence",ref.id);
   writable->receipt_result_refs.push_back(ref);
  }
  adopted.push_back({binding_ref,ref});
 }
 coverage(f,p);return {true,std::move(adopted),{}};
 }catch(const Failure& e){return {false,{},e.error};}
}
}
