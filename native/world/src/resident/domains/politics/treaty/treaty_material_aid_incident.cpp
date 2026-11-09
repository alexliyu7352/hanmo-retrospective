#include "treaty_material_aid_incident.hpp"
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
// 先反读真实原债及到期源；原validator按该Common切点核未付，不读取今日剩余量作判定。
TreatyIncidentOriginalView original(const MaterialAidReadContext& x,const StableId& treaty,
 Integer revision,const StableId& term,const SourceRef& source){
 if(treaty.empty()||revision<1||term.empty()||source.domain!="local_economy"||source.kind!="material_aid_late")
  fail("TreatyMaterialAidIncidentSourceMismatch",source.id);
 const auto* late=take(read_material_aid_deadline_result(x,source));
 const auto* debt=take(read_material_aid_obligation(x,{"local_economy",late->obligation_id}));
 if(debt->occurrence.treaty_id!=treaty||debt->occurrence.terms_revision!=revision||debt->occurrence.term_id!=term||
    debt->occurrence.ordinal<0||!debt->deadline_result_ref||*debt->deadline_result_ref!=late->id)
  fail("TreatyMaterialAidIncidentOccurrenceMismatch",source.id);
 const auto* commitment=take(read_aid_schedule_commitment(x,aid_schedule_commitment_source(debt->schedule_commitment_id)));
 const auto* schedule=x.schedules.find(commitment->schedule_id);
 if(!schedule||schedule->id!=commitment->schedule_id||schedule->treaty_id!=treaty||
    schedule->terms_revision!=revision||schedule->term_id!=term)
  fail("TreatyMaterialAidIncidentScheduleMismatch",source.id);
 const auto terms=take(x.terms.read_activated_material_aid_terms(treaty,revision,term));
 take(validate_treaty_aid_activation_terms(terms));
 if(!same_source_ref(terms.activation_source,schedule->activation_ref)||
    !same_source_ref(terms.original_terms_source,schedule->schedule_terms_ref)||
    terms.original_term->beneficiary.kind!=debt->beneficiary.kind||
    terms.original_term->beneficiary.id!=debt->beneficiary.id||
    !same_source_ref(terms.original_term->delivery_site,debt->delivery_site))
  fail("TreatyMaterialAidIncidentTermsMismatch",source.id);
 TreatyIncidentOriginalView out;
 out.treaty_id=treaty;out.terms_revision=revision;out.term_id=term;out.actual_source=source;
 out.occurred_at=late->occurred_at;out.kind=TreatyIncidentKind::unable_to_perform;
 out.liable_party=terms.original_term->source_party;
 // 原承诺身份指签定条款。S5具体obligation_id只沿上面的真实late原链反读。
 out.remaining_obligations.push_back({"politics",treaty,term,revision});
 return out;
}
}
Outcome<TreatyIncidentOriginalView> NativeTreatyMaterialAidIncidentRead::read_actual_treaty_incident(
 const StableId& treaty,Integer revision,const StableId& term,const SourceRef& source)const{
 try{return {true,original(finance_,treaty,revision,term,source),{}};}
 catch(const Failure& f){return {false,{},f.error};}
}
Status NativeTreatyMaterialAidIncidentRead::validate_incident_obligation_at_source(
 const TreatyObligationRef& obligation,const TreatyIncidentOriginalView& view)const{
 try{
  const auto expected=original(finance_,view.treaty_id,view.terms_revision,view.term_id,view.actual_source);
  if(view.kind!=expected.kind||view.liable_party!=expected.liable_party||
     take(compare_world_moments(view.occurred_at,expected.occurred_at))||
     view.remaining_obligations.size()!=1||
     !same_treaty_obligation_ref(view.remaining_obligations.front(),expected.remaining_obligations.front())||
     !same_treaty_obligation_ref(obligation,expected.remaining_obligations.front()))
   fail("TreatyMaterialAidIncidentObligationMismatch",view.actual_source.id);
  return common_success();
 }catch(const Failure& f){return {false,{},f.error};}
}
}
