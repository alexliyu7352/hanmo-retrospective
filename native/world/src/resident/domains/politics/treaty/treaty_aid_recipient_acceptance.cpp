#include "treaty_aid_recipient_acceptance.hpp"
#include "resident/domains/politics/politics_internal.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]] void fail(const char* code,const StableId& id={}) {throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{r.error};return std::move(r.value);}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
const CommonStore& common(const TreatyAidRecipientAcceptanceContext&x){return x.links.revisions.common;}
void context(const TreatyAidRecipientAcceptanceContext&x){const auto&c=common(x);
 if(&x.wallets.common!=&c||&x.inventories.common!=&c||&x.stewards.common!=&c||
    &x.wallets.catalog!=&x.shared||&x.inventories.catalog!=&x.shared||&x.stewards.shared!=&x.shared||
    &x.wallets.family!=&x.stewards.family||&x.inventories.family!=&x.stewards.family||
    &x.wallets.military!=&x.inventories.military||&x.wallets.estates!=&x.inventories.estates||
    &x.wallets.estate_titles!=&x.inventories.estate_titles||
    x.wallets.foundation!=x.inventories.foundation||x.wallets.foundation_catalog!=x.inventories.foundation_catalog||
    x.wallets.family_catalog!=&x.stewards.family_catalog||x.inventories.family_catalog!=&x.stewards.family_catalog||
    !x.wallets.clock||x.wallets.clock!=x.inventories.clock||
    &x.condition.common!=&c||x.condition.shared!=&x.shared||
    x.wallets.foundation!=&x.condition.state||x.wallets.foundation_catalog!=&x.condition.catalog||
    x.condition.family_catalog!=&x.stewards.family_catalog||
    &x.stewards.organization!=&x.links.revisions.organization||
    &x.stewards.organization_catalog!=&x.links.revisions.organization_catalog)
  fail("TreatyAidRecipientCandidateMismatch");
 if(compare(take(x.wallets.clock->read_current_moment()),take(x.links.revisions.clock.read_organization_authority_now()))||compare(x.condition.actual_now,take(x.links.revisions.clock.read_organization_authority_now())))fail("TreatyAidRecipientClockMismatch");
}
bool same_owner(const Owner&a,const Owner&b){return !a.extra&&!b.extra&&a.kind==b.kind&&a.id==b.id;}
void title(const TreatyAidRecipientAcceptanceContext&x,const TreatyAidResourceSelection2&s,
 const SourceRef&cut,const Owner&expected){
 NativePrivateWalletTitleRead wallets{x.wallets};NativePrivateInventoryTitleRead inventories{x.inventories};
 for(const auto&r:s.aid.item_routes)if(!same_owner(take(inventories.read_private_inventory_title_before_source(r.beneficiary_inventory_id,cut)).legal_owner,expected))fail("TreatyAidRecipientInventoryOwnerMismatch",r.beneficiary_inventory_id);
 if(s.aid.money_route&&!same_owner(take(wallets.read_private_wallet_title_before_source(s.aid.money_route->beneficiary_wallet_id,cut)).legal_owner,expected))fail("TreatyAidRecipientWalletOwnerMismatch",s.aid.money_route->beneficiary_wallet_id);
 if(s.compensation&&!same_owner(take(wallets.read_private_wallet_title_before_source(s.compensation->money_route.beneficiary_wallet_id,cut)).legal_owner,expected))fail("TreatyAidRecipientCompensationOwnerMismatch",s.compensation->money_route.beneficiary_wallet_id);
}
}
SourceRef treaty_aid_recipient_acceptance_source(const TreatyAidRecipientAcceptance&r){return {"politics","treaty_aid_recipient_accepted",r.id};}
Status validate_treaty_aid_recipient_acceptance_shape(const TreatyAidRecipientAcceptance&r){
 if(r.id.empty()||r.actor_id.empty()||r.term_id.empty()||!valid_source_ref(r.proposal.source_ref)||
 r.proposal.source_ref.domain!="politics"||r.proposal.source_ref.kind!="treaty_aid_resources_proposed"||r.proposal.source_schema!=2||r.knowledge.index()>1)
 return common_failure("InvalidTreatyAidRecipientAcceptance",r.id);
 if(r.household_steward_source&&(!valid_source_ref(*r.household_steward_source)||r.household_steward_source->domain!="family"||r.household_steward_source->kind!="family_household_control"))return common_failure("InvalidTreatyAidRecipientStewardSource",r.id);
 if(const auto*k=std::get_if<TreatyResourceDeliveredChoice>(&r.knowledge))if(!valid_source_ref(k->actual_receipt.source_ref)||k->actual_receipt.source_schema<1)return common_failure("InvalidTreatyAidRecipientKnowledge",r.id);
 auto m=encode_world_moment(r.accepted_at);return m.ok?common_success():Status{false,{},m.error};
}
Outcome<TreatyAidRecipientAcceptanceView> read_treaty_aid_private_recipient_acceptance(
 const TreatyAidRecipientAcceptanceContext&x,const MomentOwnerSource&actual){try{
 context(x);const auto&c=common(x);const auto&ref=actual.source_ref;
 if(actual.source_schema!=1||!valid_source_ref(ref)||ref.domain!="politics"||ref.kind!="treaty_aid_recipient_accepted")fail("TreatyAidRecipientSourceUnsupported",ref.id);
 const auto*r=x.acceptances.find(ref.id);if(!r||r->id!=ref.id)fail("TreatyAidRecipientSourceMissing",ref.id);
 take(validate_treaty_aid_recipient_acceptance_shape(*r));const auto pos=take(read_actual_common_fact_position(c,ref));
 const auto*p=take(read_treaty_aid_resource_proposal2_links(x.links,r->proposal));
 if(take(read_actual_common_fact_position(c,r->proposal.source_ref))>=pos||compare(p->proposed_at,r->accepted_at)>0||
    compare(r->accepted_at,take(x.links.revisions.clock.read_organization_authority_now()))>0)fail("TreatyAidRecipientSourceOrderMismatch",ref.id);
 auto revision=take(read_treaty_revision_source(x.links.revisions,p->terms_source,1));
 if(!revision.definition||compare(r->accepted_at,WorldMoment{revision.definition->terms.effective_window.until_tick,{}})>=0)fail("TreatyAidRecipientExpiredProposal",ref.id);
 for(const auto&row:x.links.revisions.counteroffers.rows())if(row.second.treaty_id==p->treaty_id&&row.second.terms_revision>p->terms_revision&&take(read_actual_common_fact_position(c,treaty_counteroffer_source(row.second)))<pos)fail("TreatyAidRecipientSupersededProposal",ref.id);
 const TreatyTerm*term=nullptr;const TreatyAidResourceSelection2*selection=nullptr;
 for(const auto&t:revision.definition->terms.terms)if(t.term_id==r->term_id)term=&t;
 for(const auto&s:p->selections)if(s.aid.term_id==r->term_id)selection=&s;
 if(!term||!selection||!std::holds_alternative<TreatyMaterialAidTerm>(term->terms))fail("TreatyAidRecipientTermMismatch",r->term_id);
 take(validate_treaty_aid_resource_selection2_for_term(*selection,*term));
 if(std::holds_alternative<TreatyResourceOwnChoice>(r->knowledge)){
  if(p->proposer.actor_id!=r->actor_id||compare(p->proposed_at,r->accepted_at))fail("TreatyAidRecipientOwnChoiceMismatch",ref.id);
 }else{
  const auto official=take(read_treaty_aid_proposal_official_content(x.links,r->proposal));
  auto known=take(read_actor_known_treaty_aid_proposal_before_source(x.links,x.social,x.communications,x.communication_sources,x.times,x.result_stamp,r->actor_id,official,std::get<TreatyResourceDeliveredChoice>(r->knowledge).actual_receipt,ref));
  if(known.original!=p||!known.actual_receipt.delivery)fail("TreatyAidRecipientKnowledgeMismatch",ref.id);
  if(compare(known.actual_receipt.delivery->delivered_at,r->accepted_at)>0)fail("TreatyAidRecipientBeforeDisclosure",ref.id);
 }
 auto presence=take(read_character_condition_before_application_fact(x.condition,r->actor_id,ref,r->accepted_at));
 if(!presence.body.alive)fail("TreatyAidRecipientNotAlive",r->actor_id);
 const auto&aid=std::get<TreatyMaterialAidTerm>(term->terms);Owner owner;
 if(aid.beneficiary.kind==SubjectKind::actor){
  if(r->household_steward_source)fail("TreatyAidNamedActorHouseholdHistoryUnavailable",aid.beneficiary.id);
  if(aid.beneficiary.id!=r->actor_id)fail("TreatyAidRecipientPersonalConsentRequired",r->actor_id);
  owner.kind=Owner::Kind::actor;owner.id=r->actor_id;
 }else if(aid.beneficiary.kind==SubjectKind::cohort){
  if(!r->household_steward_source)fail("TreatyAidRecipientOriginalStewardRequired",r->actor_id);
  auto household=take(read_private_cohort_household_before_source(c,x.economy,x.economy_catalog,aid.beneficiary.id,ref));
  const auto prefix=take(read_actual_common_fact_prefix_before(c,ref));
  auto steward=take(read_household_aid_steward_before_prefix_v1(x.stewards,*r->household_steward_source,r->actor_id,household.household_id,r->proposal,r->accepted_at,prefix));
  if(!steward)fail("TreatyAidRecipientOriginalStewardRequired",r->actor_id);
  owner.kind=Owner::Kind::household;owner.id=household.household_id;
 }else fail("TreatyAidRecipientPublicOriginalAuthorityUnavailable",aid.beneficiary.id);
 title(x,*selection,ref,owner);
 for(const auto&row:x.acceptances.rows())if(row.first!=r->id&&row.second.term_id==r->term_id&&row.second.proposal.source_schema==2&&same_source_ref(row.second.proposal.source_ref,r->proposal.source_ref))fail("DuplicateTreatyAidRecipientAcceptance",r->term_id);
 return {true,{r,p,term,selection,std::move(owner)},{}};
 }catch(const Failure&e){return {false,{},e.error};}
 catch(const std::bad_alloc&){return {false,{},common_failure("TreatyAidRecipientResourceExhausted").error};}
 catch(const std::length_error&){return {false,{},common_failure("TreatyAidRecipientResourceExhausted").error};}}
Status validate_treaty_aid_private_recipient_acceptances(const TreatyAidRecipientAcceptanceContext&x){try{
 context(x);for(const auto&row:x.acceptances.rows()){
  if(row.first!=row.second.id)fail("TreatyAidRecipientIdentityMismatch",row.first);
  take(read_treaty_aid_private_recipient_acceptance(x,{treaty_aid_recipient_acceptance_source(row.second),1}));
 }
 for(const auto&row:common(x).state().facts.rows())if(row.second.domain=="politics"&&row.second.kind=="treaty_aid_recipient_accepted"&&!x.acceptances.find(row.first))fail("TreatyAidRecipientOrphanFact",row.first);
 return common_success();}catch(const Failure&e){return {false,{},e.error};}}
Status visit_treaty_aid_private_recipient_original_prefix(const TreatyAidRecipientAcceptanceContext&x,
 const MomentOwnerSource&actual,CommonActualFactPrefixSink&sink){
 auto source=read_treaty_aid_private_recipient_acceptance(x,actual);if(!source.ok)return {false,{},source.error};
 auto prefix=read_actual_common_fact_prefix_before(common(x),actual.source_ref);if(!prefix.ok)return {false,{},prefix.error};
 // Persisted actual consent M authenticates this complete predecessor cut.
 // Retaining only the final anchor would drop real title/role parent evidence.
 return visit_actual_common_fact_prefix(common(x),prefix.value,sink);
}
Outcome<MomentOwnerSource> accept_treaty_aid_private_recipient_in_candidate(CommonStore&c,
 OrderedTable<TreatyAidRecipientAcceptance>&rows,const TreatyAidRecipientAcceptanceContext&x,
 const StableId&actor,const MomentOwnerSource&proposal,const StableId&term,
 const TreatyResourceKnowledge&knowledge,const std::optional<SourceRef>&steward){try{
 context(x);if(&c!=&common(x)||&rows!=&x.acceptances)fail("TreatyAidRecipientCandidateMismatch");
 take(politics_detail::available(c,x.activity,actor));
 const auto*p=take(read_treaty_aid_resource_proposal2_links(x.links,proposal));
 auto current=take(read_current_treaty_revision(x.links.revisions,p->treaty_id));
 if(current.terms_revision!=p->terms_revision||!same_source_ref(current.source,p->terms_source))fail("TreatyAidRecipientSupersededProposal",proposal.source_ref.id);
 TreatyAidRecipientAcceptance r;r.id=take(c.next_id("treaty_aid_recipient"));r.actor_id=actor;r.term_id=term;r.proposal=proposal;r.knowledge=knowledge;r.household_steward_source=steward;r.accepted_at=take(x.links.revisions.clock.read_organization_authority_now());
 take(validate_treaty_aid_recipient_acceptance_shape(r));const auto source=treaty_aid_recipient_acceptance_source(r);
 // Caller owns an unpublished World candidate: a failed post-append proof
 // invalidates the entire candidate, never leaves a partially accepted source.
 take(c.append_fact({source.id,source.kind,source.domain}));if(!rows.insert(source.id,std::move(r)))fail("DuplicateTreatyAidRecipientAcceptance",source.id);
 take(read_treaty_aid_private_recipient_acceptance(x,{source,1}));return {true,{source,1},{}};
 }catch(const Failure&e){return {false,{},e.error};}
 catch(const std::bad_alloc&){return {false,{},common_failure("TreatyAidRecipientResourceExhausted").error};}
 catch(const std::length_error&){return {false,{},common_failure("TreatyAidRecipientResourceExhausted").error};}}
Outcome<WorldMoment> NativeTreatyAidPrivateRecipientResultRead::read_selected_source_moment(const MomentOwnerSource&actual)const{
 auto r=read_treaty_aid_private_recipient_acceptance(x_,actual);if(!r.ok)return {false,{},r.error};return {true,r.value.source->accepted_at,{}};
}
Outcome<ExactOwnerFactRead> NativeTreatyAidPrivateRecipientResultRead::read_selected_fact(const FactRecord&head)const{
 if(head.domain!="politics"||head.kind!="treaty_aid_recipient_accepted")return {false,{},common_failure("TreatyAidRecipientResultUnsupported",head.id,ErrorKind::unsupported_capability).error};
 auto r=read_treaty_aid_private_recipient_acceptance(x_,{{head.domain,head.kind,head.id},1});if(!r.ok)return {false,{},r.error};
 ExactOwnerFactRead out;out.disposition=ExactOwnerFactRead::Disposition::no_dynamic_projection;return {true,std::move(out),{}};
}

}
