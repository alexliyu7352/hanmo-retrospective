#include "repair_service_agreement_internal.hpp"
#include "resident/domains/actor_foundation/model.hpp"
#include "resident/domains/director/director_command_acceptance.hpp"
namespace hanmo::resident::repair_service_agreement_detail {
namespace {
bool catalog(const CatalogIdentity&a,const CatalogIdentity&b){return a.content_id==b.content_id&&a.content_version==b.content_version&&a.content_digest==b.content_digest&&a.ruleset_id==b.ruleset_id&&a.schema_sha256==b.schema_sha256&&a.pack_set_digest==b.pack_set_digest&&a.module_versions==b.module_versions&&a.artifact_sha256==b.artifact_sha256;}
bool field(const LifeCatalogFieldRef&a,const LifeCatalogFieldRef&b){return catalog(a.catalog_identity,b.catalog_identity)&&a.scenario_id==b.scenario_id&&a.entity_id==b.entity_id&&a.field_selector==b.field_selector;}
LifePlayerActorRoute player(const RepairServiceAgreementReadContext&x,const ProfileControlEligibility&control){
 LifePlayerActorRoute out;std::visit([&](const auto&v){using T=std::decay_t<decltype(v)>;
  if constexpr(std::is_same_v<T,AuthoredProfileController>)out.original_control_source_ref=LifeCatalogFieldRef{x.controls.foundation_catalog->identity,v.scenario_id,v.actor_id,"player_actor"};
  else{const StableId*id=nullptr;if constexpr(std::is_same_v<T,CreatedProfileController>)id=&v.creation_fact_id;else if constexpr(std::is_same_v<T,SuccessionProfileController>)id=&v.succession_fact_id;else if constexpr(std::is_same_v<T,ContinuationProfileController>)id=&v.activation_id;else id=&v.activation_fact_id;const auto*h=x.common.state().facts.find(*id);if(!h||h->id!=*id)fail("RepairOriginalActorControlRequired",*id);out.original_control_source_ref=SourceRef{h->domain,h->kind,*id};}
 },control.source);return out;
}
}
WorldMoment context(const RepairServiceAgreementReadContext&x){
 if(&x.conditions.common!=&x.common||&x.conditions.state!=&x.items||&x.conditions.clock!=&x.clock||x.conditions.selected_version!=3||!x.inspections.bound_to(x.common,x.items)||&x.controls.common!=&x.common)fail("RepairAgreementContextMismatch");
 if(!x.controls.foundation||!x.controls.foundation_catalog||!x.controls.actual_clock)fail("RepairAgreementOriginalControlSourcesRequired",{},ErrorKind::unsupported_capability);
 auto at=take(x.clock.read_current_moment());take(compare_world_moments(at,at));if(at.half_day_tick<x.common.tick()||compare(at,take(x.controls.actual_clock->read_current_moment())))fail("RepairAgreementActualClockMismatch");return at;
}
void inspection(const RepairServiceAgreementReadContext&x,const RepairServiceQuotedTerms&t,const RepairServiceAcceptanceInspection&i,const MomentOwnerSource&contact,const WorldMoment&at,const SourceRef*cut){
 const auto current=context(x);take(compare_world_moments(at,at));if(compare(at,current)>0||(!cut&&compare(at,current)))fail("RepairInspectionActualMomentMismatch",t.instance_id);
 const auto*p=take(read_repair_policy(x.policies,t.repair_policy_ref));take(validate_repair_service_rule_terms(x.rules,x.policies,t));
 if(t.customer.extra||t.provider.extra||t.customer.kind!=Owner::Kind::actor||t.provider.kind!=Owner::Kind::actor)fail("RepairOriginalRepresentedAgreementRequired",t.instance_id,ErrorKind::unsupported_capability);
 if(t.customer.id.empty()||t.provider.id.empty()||t.customer.id==t.provider.id||!x.common.actor(t.customer.id)||!x.common.actor(t.provider.id)||t.worker_id.empty()||!x.common.actor(t.worker_id)||t.payer_wallet_id.empty()||t.payee_wallet_id.empty()||t.payer_wallet_id==t.payee_wallet_id||t.terms_revision<1||t.fee_terms.line_id.empty())fail("RepairOfferedPartiesMismatch",t.instance_id);
 if(i.original_item.inventory_id.empty()||i.original_item.lot_id.empty()||i.original_item.quantity!=1||i.condition_head.instance_id!=t.instance_id||i.workstation.slot_kind!=p->workstation_slot_kind)fail("RepairActualInspectionRequired",t.instance_id);
 // Inspection is before the actual commercial acceptance. A future custody
 // source cannot be supplied here to justify an earlier quote.
 if(i.delivered_custody_source_ref)fail("RepairOfferOriginalDeliveryArrangementRequired",t.instance_id,ErrorKind::unsupported_capability);
 const auto item=cut?take(read_item_condition_before_common_source(x.conditions,i.condition_head,*cut,at)):take(read_item_condition_at_head(x.conditions,i.condition_head));
 if(!item.original_instance||item.original_instance->id!=t.instance_id||item.original_instance->repair_policy_id!=p->policy_id||std::find(p->applicable_definitions.begin(),p->applicable_definitions.end(),item.original_instance->item_definition_id)==p->applicable_definitions.end())fail("RepairOfferOriginalInstanceMismatch",t.instance_id);
 if(!cut){const auto head_owner=take(read_current_item_condition_head(x.conditions,t.instance_id));const auto&head=head_owner.value();if(head.instance_id!=i.condition_head.instance_id||head.instance_version!=i.condition_head.instance_version||head.last_condition_result_ref!=i.condition_head.last_condition_result_ref)fail("StaleRepairOfferedCondition",t.instance_id,ErrorKind::rule);}
 for(const auto*r:{&t.owner_authority_source,&t.payer_authority_source,&t.return_terms.receiver_authority,&t.custody_terms.authority_ref,&t.return_terms.original_rule_ref})ref(*r);
 if(!x.common.valid_resource_owner(t.return_terms.receiver)||t.return_terms.receiver.extra||!valid_source_ref(t.return_terms.site_ref)||t.custody_terms.inventory_id.empty())fail("RepairOfferedReturnCustodyMismatch",t.instance_id);
 take(compare_world_moments(t.return_terms.agreed_return_at,t.return_terms.agreed_return_at));take(compare_world_moments(t.custody_terms.collection_due_at,t.custody_terms.collection_due_at));if(compare(t.return_terms.agreed_return_at,at)<0||compare(t.custody_terms.collection_due_at,t.return_terms.agreed_return_at)<0)fail("RepairOfferedTimesMismatch",t.instance_id);
 const auto*c=take(read_face_to_face_contact_source(x.common,x.interactions,x.interaction_catalog,contact.source_ref,contact.source_schema));
 if(compare(c->occurred_at,at)>0||!((c->session_actor_id==t.customer.id&&c->provider_actor_id==t.provider.id)||(c->session_actor_id==t.provider.id&&c->provider_actor_id==t.customer.id)))fail("RepairActualInspectionContactMismatch",t.instance_id);
 if(cut&&header(x.common,contact.source_ref)>=header(x.common,*cut))fail("RepairInspectionContactOrderMismatch",t.instance_id);
 take(x.inspections.validate_original_repair_inspection(t,i,at,cut));
}
const RepairServiceCommandReceipt&command(const RepairServiceAgreementReadContext&x,const StableId&actor,const StableId&kind,const MomentOwnerSource&actual,const MomentOwnerSource&source,const WorldMoment&at){
 if(source.source_schema!=1||source.source_ref.domain!="common"||source.source_ref.kind!="command_result")fail("RepairActualCommandSealRequired",actual.source_ref.id);
 const auto original=take(read_common_command_source(x.common,source.source_ref));const auto&r=*original.result;
 const auto*receipt=r.details?repair_service_command_receipt(*r.details):nullptr;
 if(!original.history||original.history->actor_id!=actor||r.kind!=kind||r.tick!=at.half_day_tick||!receipt||receipt->command_kind!=kind||receipt->actor_id!=actor||!same(receipt->actual_source,actual)||compare(receipt->occurred_at,at))fail("RepairActualCommandPayloadMismatch",actual.source_ref.id);
 take(validate_repair_service_command_receipt_shape(*receipt));
 if(!repair_service_command_kind(kind)||receipt->accepted_result.has_value()!=(kind=="accept_repair_service_offer"))fail("RepairActualCommandResultShapeMismatch",actual.source_ref.id);
 return *receipt;
}
LifeActorRoute route(const RepairServiceAgreementReadContext&x,const StableId&actor,const MomentOwnerSource&actual,const MomentOwnerSource&seal,const WorldMoment&at){
 context(x);const auto control=take(read_profile_control_before_fact(x.controls,actual.source_ref));if(control.actor_id==actor)return player(x,control);
 if(!x.director||!x.director_catalog||!x.director->subject2)fail("RepairOriginalDirectorAdoptionRequired",actual.source_ref.id,ErrorKind::unsupported_capability);
 const auto original=take(read_common_command_source(x.common,seal.source_ref));const DirectorAdoptionResult2*found=nullptr;
 for(const auto&r:x.director->subject2->adoption_results.rows())if(r.second.accepted_command&&r.second.accepted_command->result_id==seal.source_ref.id){if(found)fail("RepairAmbiguousDirectorAdoption",actual.source_ref.id);found=&r.second;}
 if(!found)fail("RepairOriginalDirectorAdoptionRequired",actual.source_ref.id,ErrorKind::unsupported_capability);
 const auto view=take(read_director_accepted_command(x.common,*x.director,*x.director_catalog,x.controls.foundation_catalog->identity,{actor,found->registration_epoch,found->decision_seq,original.result->kind,original.request_seq,original.result->result_id}));
 if(!view.command||!same(view.command->actual_effect,actual)||compare(view.command->accepted_at,at))fail("RepairOriginalDirectorBusinessMismatch",actual.source_ref.id);
 LifeNpcActorRoute out;out.subject_actor_id=actor;out.registration_epoch=found->registration_epoch;out.decision_seq=found->decision_seq;out.policy_ref=LifeCatalogFieldRef{view.command->policy_catalog,x.common.state().metadata.scenario_id,view.command->policy_id,"director_policy"};return out;
}
bool same_route(const LifeActorRoute&a,const LifeActorRoute&b){
 if(a.index()!=b.index())return false;if(const auto*p=std::get_if<LifePlayerActorRoute>(&a)){const auto&x=p->original_control_source_ref;const auto&y=std::get<LifePlayerActorRoute>(b).original_control_source_ref;if(x.index()!=y.index())return false;if(const auto*r=std::get_if<SourceRef>(&x))return same_source_ref(*r,std::get<SourceRef>(y));if(const auto*f=std::get_if<LifeCatalogFieldRef>(&x))return field(*f,std::get<LifeCatalogFieldRef>(y));return false;}
 const auto&x=std::get<LifeNpcActorRoute>(a);const auto&y=std::get<LifeNpcActorRoute>(b);const auto*p=std::get_if<LifeCatalogFieldRef>(&x.policy_ref);const auto*q=std::get_if<LifeCatalogFieldRef>(&y.policy_ref);return x.subject_actor_id==y.subject_actor_id&&x.registration_epoch==y.registration_epoch&&x.decision_seq==y.decision_seq&&p&&q&&field(*p,*q);
}
const RepairServiceOfferSource&offer(const RepairServiceAgreementReadContext&x,const MomentOwnerSource&r){
 context(x);if(r.source_schema!=1||!same_source_ref(r.source_ref,repair_service_offer_source(r.source_ref.id)))fail("UnsupportedRepairOfferSource",r.source_ref.id,ErrorKind::unsupported_capability);
 const auto*o=x.items.repair_service_contracts.offers.find(r.source_ref.id);if(!o||o->id!=r.source_ref.id||o->provider_actor_id!=o->quoted_terms.provider.id)fail("RepairActualOfferMissing",r.source_ref.id);header(x.common,r.source_ref);
 if(!same(o->quoted_terms.return_terms.original_rule_ref,r))fail("RepairOfferedInstanceArrangementMismatch",o->id);
 inspection(x,o->quoted_terms,o->inspection,o->actual_contact_source,o->offered_at,&r.source_ref);command(x,o->provider_actor_id,"offer_repair_service",r,o->actual_command_source,o->offered_at);
 if(!same_route(o->actor_route,route(x,o->provider_actor_id,r,o->actual_command_source,o->offered_at)))fail("RepairOfferActorRouteMismatch",o->id);return *o;
}
void validate_adopted_agreement(const RepairServiceAgreementReadContext&x,const RepairServiceOfferSource&o,const RepairServiceAssentSource&a,const WorldMoment&at,const FactRecord*actual){
 const SourceRef quoted=repair_service_offer_source(o.id);const TermsRevisionRef offered_terms=SourceFactTerms{quoted},accepted_terms=SourceFactTerms{a.offer_ref.source_ref};
 const OriginalServiceAgreementAdoption adoption{
  {x.common.state().facts.find(o.id),&o.provider_actor_id,&o.offered_at},
  {x.common.state().facts.find(a.id),&a.customer_actor_id,&a.accepted_at},
  a.offer_ref.source_ref,offered_terms,accepted_terms,at,nullptr,actual};
 take(validate_original_service_agreement_adoption(x.common,x.clock,adoption));
}
bool matches_accepted_terms(const RepairServiceOfferSource&o,const RepairServiceAssentSource&a,const RepairServiceContractTerms&t){return same_repair_service_quoted_terms(o.quoted_terms,t)&&same(t.actual_contact_source,a.actual_contact_source)&&same(t.customer_acceptance_source,{repair_service_assent_source(a.id),1})&&same(t.provider_acceptance_source,{repair_service_offer_source(o.id),1});}
RepairServiceContractTerms accepted_terms(const RepairServiceOfferSource&o,const RepairServiceAssentSource&a){RepairServiceContractTerms t;static_cast<RepairServiceQuotedTerms&>(t)=o.quoted_terms;t.actual_contact_source=a.actual_contact_source;t.customer_acceptance_source={repair_service_assent_source(a.id),1};t.provider_acceptance_source={repair_service_offer_source(o.id),1};return t;}
const RepairServiceAssentSource&assent(const RepairServiceAgreementReadContext&x,const MomentOwnerSource&r){
 context(x);if(r.source_schema!=1||!same_source_ref(r.source_ref,repair_service_assent_source(r.source_ref.id)))fail("UnsupportedRepairAssentSource",r.source_ref.id,ErrorKind::unsupported_capability);
 const auto*a=x.items.repair_service_contracts.assents.find(r.source_ref.id);if(!a||a->id!=r.source_ref.id)fail("RepairActualAssentMissing",r.source_ref.id);const auto&o=offer(x,a->offer_ref);
 if(a->customer_actor_id!=o.quoted_terms.customer.id)fail("RepairActualAssentOfferMismatch",a->id);
 validate_adopted_agreement(x,o,*a,a->accepted_at);
 inspection(x,o.quoted_terms,o.inspection,a->actual_contact_source,a->accepted_at,&r.source_ref);const auto&receipt=command(x,a->customer_actor_id,"accept_repair_service_offer",r,a->actual_command_source,a->accepted_at);
 if(!same_route(a->actor_route,route(x,a->customer_actor_id,r,a->actual_command_source,a->accepted_at)))fail("RepairAssentActorRouteMismatch",a->id);
 const auto&result=*receipt.accepted_result;const auto*c=x.items.repair_service_contracts.contracts.find(result.commercial_contract.source_ref.id);const auto*s=x.items.repair_jobs.acceptances.find(result.repair_acceptance.source_ref.id);
 if(!c||!s||c->id!=result.commercial_contract.source_ref.id||s->id!=result.repair_acceptance.source_ref.id||!same(result.commercial_contract,{repair_service_contract_source(c->id),1})||!same(result.repair_acceptance,{repair_accepted_source(s->id),1})||compare(c->accepted_at,a->accepted_at)||compare(s->accepted_at,a->accepted_at)||!matches_accepted_terms(o,*a,c->terms)||!same(s->terms.fee_terms_ref,result.commercial_contract)||!same(s->terms.return_terms_ref,result.commercial_contract)||s->terms.instance_id!=o.quoted_terms.instance_id||s->terms.worker_id!=o.quoted_terms.worker_id||s->terms.policy_ref.policy_id!=o.quoted_terms.repair_policy_ref.policy_id||s->terms.policy_ref.version!=o.quoted_terms.repair_policy_ref.version||!owner(s->terms.legal_owner,o.quoted_terms.customer)||s->terms.terms_revision!=o.quoted_terms.terms_revision||!same(s->terms.actual_contact_ref,a->actual_contact_source)||!same(s->terms.owner_authority_ref,o.quoted_terms.owner_authority_source))fail("RepairAssentActualAcceptedResultMismatch",a->id);
 const auto&expected=o.inspection;const auto&v=s->terms;if(v.original_item.inventory_id!=expected.original_item.inventory_id||v.original_item.lot_id!=expected.original_item.lot_id||v.original_item.quantity!=expected.original_item.quantity||!repair_detail::same_head(v.condition_head,expected.condition_head)||v.workstation.provider_domain!=expected.workstation.provider_domain||v.workstation.facility_id!=expected.workstation.facility_id||v.workstation.slot_kind!=expected.workstation.slot_kind||v.delivered_custody_source_ref.has_value()!=expected.delivered_custody_source_ref.has_value()||(v.delivered_custody_source_ref&&!same(*v.delivered_custody_source_ref,*expected.delivered_custody_source_ref))||header(x.common,result.commercial_contract.source_ref)<=header(x.common,r.source_ref)||header(x.common,result.repair_acceptance.source_ref)<=header(x.common,result.commercial_contract.source_ref))fail("RepairAssentOriginalInspectionBindingMismatch",a->id);
 return *a;
}
}
