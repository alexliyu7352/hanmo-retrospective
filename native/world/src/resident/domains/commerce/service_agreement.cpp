#include "service_agreement.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id,ErrorKind::internal).error};}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
std::size_t order(const CommonStore&c,const FactRecord*h){
 if(!h||h->id.empty()||h->domain.empty()||h->kind.empty()||c.state().facts.find(h->id)!=h)fail("OriginalAgreementActualHeaderRequired",h?h->id:StableId{});
 std::size_t n=0;for(const auto&r:c.state().facts.rows()){if(&r.second==h)return n;++n;}fail("OriginalAgreementHeaderOrderMissing",h->id);
}
std::size_t party(const CommonStore&c,const OriginalServiceAgreementParty&p){
 const auto n=order(c,p.actual_source);if(!p.actual_actor||p.actual_actor->empty()||!c.actor(*p.actual_actor)||!p.occurred_at||p.occurred_at->half_day_tick<0)fail("OriginalAgreementActualPartyRequired",p.actual_source->id);
 compare(*p.occurred_at,WorldMoment{});return n;
}
}
Status validate_original_service_agreement_adoption(const CommonStore&c,const CurrentMomentRead&clock,const OriginalServiceAgreementAdoption&v){try{
 const auto offered=party(c,v.offer),accepted=party(c,v.acceptance);const auto&ref=v.accepted_offer_ref;const auto&head=*v.offer.actual_source;
 if(!valid_source_ref(ref)||ref.id!=head.id||ref.domain!=head.domain||ref.kind!=head.kind||offered>=accepted)fail("OriginalAgreementOfferAcceptanceMismatch",v.acceptance.actual_source->id);
 take(validate_terms_revision(v.offered_terms));take(validate_terms_revision(v.accepted_terms));if(!same_terms_revision(v.offered_terms,v.accepted_terms))fail("OriginalAgreementAcceptedTermsChanged",ref.id);
 const auto now=take(clock.read_current_moment());if(v.adopted_at.half_day_tick<0||now.half_day_tick<c.tick()||compare(*v.offer.occurred_at,*v.acceptance.occurred_at)>0||compare(*v.acceptance.occurred_at,v.adopted_at)||compare(v.adopted_at,now)>0)fail("OriginalAgreementActualBoundaryMismatch",ref.id);
 if(v.original_valid_until&&(compare(*v.offer.occurred_at,*v.original_valid_until)>=0||compare(v.adopted_at,*v.original_valid_until)>=0))fail("OriginalAgreementAcceptedOutsideWindow",ref.id);
 if(v.actual_adoption_source&&order(c,v.actual_adoption_source)<=accepted)fail("OriginalAgreementAdoptionOrderMismatch",v.actual_adoption_source->id);
 return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
}
