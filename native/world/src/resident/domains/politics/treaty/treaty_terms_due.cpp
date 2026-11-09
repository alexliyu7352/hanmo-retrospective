#include "treaty_terms_due.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore& c,const SourceRef& s){const auto& order=c.state().facts.order();auto i=std::find(order.begin(),order.end(),s.id);const auto* head=c.state().facts.find(s.id);if(!head||head->domain!=s.domain||head->kind!=s.kind||i==order.end())fail("TreatyDueOriginalSourceMissing",s.id);return static_cast<std::size_t>(i-order.begin());}
bool same(const TreatyTermsDue& a,const TreatyTermsDue& b){return a.kind==b.kind&&a.treaty_id==b.treaty_id&&a.terms_revision==b.terms_revision&&a.original_source.source_schema==b.original_source.source_schema&&same_source_ref(a.original_source.source_ref,b.original_source.source_ref)&&compare(a.at,b.at)==0;}
}
Outcome<std::vector<TreatyTermsDue>> read_treaty_terms_due(const TreatyTerminationContext& x){try{
 std::vector<TreatyTermsDue> out;const auto& a=x.activations;const auto& common=a.signatures.revisions.common;
 for(const auto& offer:a.signatures.revisions.offers.rows()){
  auto ready=take(read_current_treaty_activation_ready(a,offer.first));if(!ready)continue;bool active=false;
  for(const auto& row:a.activations.rows())if(row.second.treaty_id==ready->revision.treaty_id&&row.second.terms_revision==ready->revision.terms_revision){take(read_original_treaty_activation(a,treaty_activation_source(row.second),1));active=true;}
  if(active)continue;if(ready->signature_refs.empty())fail("TreatyRecognitionIncomplete",offer.first);
  SourceRef last=ready->signature_refs.front();for(const auto& ref:ready->signature_refs)if(position(common,ref)>position(common,last))last=ref;
  out.push_back({TreatyTermsDueKind::activation,ready->revision.treaty_id,ready->revision.terms_revision,{last,1},ready->activation_at});
 }
 for(const auto& row:a.activations.rows()){
  const auto source=treaty_activation_source(row.second);auto original=take(read_original_treaty_activation(a,source,1));bool ended=false;
  for(const auto& end:x.terminations.rows())if(same_source_ref(end.second.activation_ref,source)){take(read_original_treaty_termination(x,treaty_termination_source(end.second),1));ended=true;}
  if(!ended)out.push_back({TreatyTermsDueKind::expiry,row.second.treaty_id,row.second.terms_revision,{source,1},WorldMoment{original.revision.definition->terms.effective_window.until_tick,{}}});
 }
 // The caller's real compiled graph chooses among original due events. Keep
 // original enumeration order here; do not invent an ID tie-break causal rule.
 return {true,std::move(out),{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<SourceRef> apply_treaty_terms_due_in_candidate(CommonStore& common,OrderedTable<TreatyActivationSource>& activations,OrderedTable<TreatyTerminationSource>& terminations,const TreatyTerminationContext& x,TreatyActivationOriginalWrite& starts,TreatyTerminationOriginalWrite& ends,const TreatyTermsDue& prediction){try{
 if(&common!=&x.activations.signatures.revisions.common||&activations!=&x.activations.activations||&terminations!=&x.terminations)fail("TreatyDueCandidateMismatch");
 if(compare(prediction.at,take(x.activations.signatures.revisions.clock.read_organization_authority_now())))fail("TreatyDueNotAtActualMoment",prediction.treaty_id);
 // A same-event retry re-reads the actual adopted result. It does not dispatch
 // original financial/Grant effects a second time.
 if(prediction.original_source.source_schema!=1)fail("TreatyDueSourceSchemaMismatch",prediction.treaty_id);
 if(prediction.kind==TreatyTermsDueKind::activation){for(const auto& row:activations.rows())if(row.second.treaty_id==prediction.treaty_id&&row.second.terms_revision==prediction.terms_revision){
  const auto ref=treaty_activation_source(row.second);auto adopted=take(read_verified_treaty_activation(x.activations,ref,1));if(compare(adopted.source->activated_at,prediction.at))fail("TreatyDuePredictionChanged",prediction.treaty_id);
  SourceRef last=adopted.source->signature_refs.front();for(const auto& r:adopted.source->signature_refs)if(position(common,r)>position(common,last))last=r;
  if(!same_source_ref(last,prediction.original_source.source_ref))fail("TreatyDuePredictionChanged",prediction.treaty_id);return {true,ref,{}};
 }}
 if(prediction.kind==TreatyTermsDueKind::expiry){for(const auto& row:terminations.rows())if(same_source_ref(row.second.activation_ref,prediction.original_source.source_ref)){
  const auto ref=treaty_termination_source(row.second);auto adopted=take(read_verified_treaty_termination(x,ref,1));if(adopted.source->treaty_id!=prediction.treaty_id||adopted.source->terms_revision!=prediction.terms_revision||!std::holds_alternative<TreatyExpiryTermination>(adopted.source->cause)||compare(adopted.source->terminated_at,prediction.at))fail("TreatyDuePredictionChanged",prediction.treaty_id);return {true,ref,{}};
 }}
 auto actual=take(read_treaty_terms_due(x));if(std::none_of(actual.begin(),actual.end(),[&](const auto& p){return same(p,prediction);}))fail("TreatyDuePredictionChanged",prediction.treaty_id);
 if(prediction.kind==TreatyTermsDueKind::activation)return activate_treaty_terms_in_candidate(common,activations,x.activations,starts,prediction.treaty_id);
 if(prediction.kind==TreatyTermsDueKind::expiry)return terminate_treaty_terms_in_candidate(common,terminations,x,ends,prediction.original_source.source_ref,TreatyExpiryTermination{});
 fail("TreatyDueKindUnsupported",prediction.treaty_id);
 }catch(const Failure& f){return {false,{},f.error};}}
}
