#include "task_report_carrier_scope.hpp"
#include "resident/domains/social/knowledge_report_category.hpp"
#include "task_model.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void reject(const char*c,const StableId&id={}){throw Failure{common_failure(c,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class F>Outcome<T>guard(F f){try{return {true,f(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same_catalog(const CatalogIdentity&a,const CatalogIdentity&b){return std::tie(a.content_id,a.content_version,a.content_digest,a.ruleset_id,a.schema_sha256,a.pack_set_digest,a.module_versions,a.artifact_sha256)==std::tie(b.content_id,b.content_version,b.content_digest,b.ruleset_id,b.schema_sha256,b.pack_set_digest,b.module_versions,b.artifact_sha256);}
bool contains(const std::vector<StableId>&v,const StableId&id){return std::find(v.begin(),v.end(),id)!=v.end();}
TaskReportCarrierScopeView read(const TaskReportCarrierScopeContext&x,const SourceRef&offer,const StableId&sender,const StableId&recipient,const MomentOwnerSource&disclosure,const WorldMoment&at){
 const auto&f=x.face;const auto&k=f.disclosure;if(&f.common!=&k.common||!same_catalog(x.offers.identity,f.catalog_identity)||!k.social.dynamic||!k.social.dynamic->time2)reject("CarrierReportSameLockedWorldRequired");
 take(validate_task_report_carrier_offer_catalog(x.offers,x.common_catalog,f.catalog));const auto*o=take(read_task_report_carrier_offer(x.offers,offer));
 if(!contains(o->allowed_sender_actor_ids,sender)||!contains(o->allowed_recipient_actor_ids,recipient)||sender==recipient||sender==o->carrier_actor_id||recipient==o->carrier_actor_id)reject("CarrierReportAuthorScopeDenied",o->id);
 const auto*d=take(read_face_to_face_report_source(f,disclosure.source_ref,disclosure.source_schema));
 auto temporary_memory=CheckpointMemoryCharge(0);
 const auto*c=take(read_face_to_face_contact_source(f.common,f.interaction,f.catalog,{"interaction",face_to_face_contact_source_kind,d->contact_source_id},1));
 if(d->sender_actor_id!=sender||d->recipient_actor_id!=o->carrier_actor_id||c->session_actor_id!=sender||c->provider_actor_id!=o->carrier_actor_id||c->context.role_id!=o->role_id||c->dialogue_id!=o->dialogue_id||c->context.kind||c->context.encounter_id||c->context.input_error||cmp(d->disclosed_at,at)>0||cmp(at,take(k.clock.read_current_moment()))>0)reject("CarrierReportActualContactScopeRequired",o->id);
 // A source header alone does not show that the carrier actually received it.
 const MessageBusinessLink*link=nullptr;for(const auto&row:f.interaction.communication_links->links.rows())if(same(row.second.original_business_fact_ref,disclosure.source_ref)){if(link)reject("DuplicateCarrierDisclosureMessage",d->id);link=&row.second;}
 const auto*m=link?k.social.dynamic->pending_messages.find(link->original_public_message_ref):nullptr;
 const auto*b=m?k.social.dynamic->time2->message_execution_bindings.find(m->id):nullptr;
 const auto*delivery=b&&b->actual_delivery_ref?f.interaction.communication_links->deliveries.find(b->actual_delivery_ref->id):nullptr;
 if(!link||link->original_business_source_schema!=1||!m||m->status!=MessageStatus::delivered||m->report_id!=d->report_id||!delivery||b->actual_delivery_ref->domain!="interaction"||b->actual_delivery_ref->kind!="communication_delivery"||!same(delivery->arrival_ref,disclosure.source_ref)||delivery->arrival_schema!=1||delivery->business_link_id!=link->id||delivery->actual_receiver.kind!=SubjectKind::actor||delivery->actual_receiver.id!=o->carrier_actor_id||!delivery->contents_disclosed||delivery->custody_receiver||delivery->office_epoch||cmp(delivery->delivered_at,d->disclosed_at))reject("CarrierReportActualKnowledgeDeliveryRequired",d->id);
 const auto*report=k.social.dynamic->reports.find(d->report_id);if(!report)reject("CarrierReportOriginalReportMissing",d->id);
 const auto category=take(read_exact_report_category(f.common,*report,k.results,k.result_stamp));if(!category||std::find(o->allowed_report_categories.begin(),o->allowed_report_categories.end(),*category)==o->allowed_report_categories.end())reject("CarrierReportCategoryOutsideOffer",o->id);
 for(const auto*actor:{&sender,&o->carrier_actor_id}){auto known=take(read_knowledge_receipt_view_v2(f.common,k.social,k.results,{SubjectKind::actor,*actor},report->id,k.result_stamp));if(!known||!known->original||cmp(known->learned_at,d->disclosed_at)>0)reject("CarrierReportPersonalKnowledgeRequired",*actor);}
 return {o,d,c,report};
}
}
Outcome<TaskReportCarrierScopeView> read_current_task_report_carrier_scope(const TaskReportCarrierScopeContext&x,const TaskReportCarrierContentSelection&q){return guard<TaskReportCarrierScopeView>([&]{return read(x,q.authored_offer,q.sender_actor_id,q.recipient_actor_id,q.actual_carrier_disclosure,take(x.face.disclosure.clock.read_current_moment()));});}
namespace {
TaskReportCarrierScopeView frozen_scope(const TaskReportCarrierScopeContext&x,const TaskCarrierAgreement&a,const TaskCarrierReportTerms&r){
 auto v=read(x,a.terms.original_terms_source,r.sender_actor_id,r.recipient_actor_id,r.actual_carrier_disclosure,a.accepted_at);const auto&o=*v.offer;const auto&c=*v.contact;
 const auto*revision=std::get_if<SourceFactTerms>(&a.terms.terms_revision);
 if(!revision||!same(revision->source,r.actual_carrier_disclosure.source_ref)||!same(a.terms.original_business_source,r.actual_carrier_disclosure.source_ref)||a.terms.principal.kind!=SubjectKind::actor||a.terms.principal.id!=r.sender_actor_id||a.terms.carrier_subject.kind!=SubjectKind::actor||a.terms.carrier_subject.id!=o.carrier_actor_id||a.terms.provider_actor_id!=o.carrier_actor_id||a.terms.role_id!=o.role_id||a.terms.dialogue_id!=o.dialogue_id||a.terms.accept_node_id!=o.accept_node_id||a.terms.destination_site_id!=o.destination_site_id||r.carrier_actor_id!=o.carrier_actor_id||r.report_id!=v.report->id||a.terms.custody_ref!=r.report_id||!a.terms.population_selections.empty()||!a.terms.named_participants.empty()||!a.terms.vehicle_refs.empty()||!a.terms.inventory_ids.empty()||!a.terms.cargo_authority_refs.empty()||!a.terms.vehicle_authority_refs.empty()||!a.terms.participant_consent_refs.empty()||!a.terms.passage_source_refs.empty()||a.contact.actor_id!=r.sender_actor_id||a.contact.provider_actor_id!=o.carrier_actor_id||a.contact.session_id!=c.session_id||a.contact.place_id!=c.context.place_id||a.contact.role_id!=o.role_id||a.contact.dialogue_id!=o.dialogue_id||a.contact.node_id!=o.accept_node_id||a.contact.tick!=a.accepted_at.half_day_tick||r.earliest_delivery_tick!=a.accepted_at.half_day_tick)reject("CarrierReportFrozenAuthorScopeMismatch",a.id);
 return v;
}
}
Status validate_task_report_carrier_prospective_scope(const TaskReportCarrierScopeContext&x,const TaskReportCarrierContentSelection&q,const TaskCarrierAgreement&a,const TaskCarrierReportTerms&r){return guard<Done>([&]{
 if(x.face.common.state().facts.find(a.id)||(x.tasks.transport_agreements&&x.tasks.transport_agreements->agreements.find(a.id))||cmp(a.accepted_at,take(x.face.disclosure.clock.read_current_moment()))||!same(q.authored_offer,a.terms.original_terms_source)||q.sender_actor_id!=r.sender_actor_id||q.recipient_actor_id!=r.recipient_actor_id||q.actual_carrier_disclosure.source_schema!=r.actual_carrier_disclosure.source_schema||!same(q.actual_carrier_disclosure.source_ref,r.actual_carrier_disclosure.source_ref))reject("CarrierReportCurrentAcceptanceAttemptRequired",a.id);
 frozen_scope(x,a,r);return Done{};
});}
Outcome<TaskReportCarrierScopeView> read_task_report_carrier_accepted_scope(const TaskReportCarrierScopeContext&x,const TaskCarrierAgreement&a,const TaskCarrierReportTerms&r){return guard<TaskReportCarrierScopeView>([&]{
 auto temporary_memory=CheckpointMemoryCharge(0);
 const auto source=take(read_original_task_carrier_acceptance(x.face.common,x.tasks,{"tasks","task_carrier_agreement_accepted",a.id},1));if(source.agreement!=&a||source.report_delivery!=&r)reject("CarrierReportBorrowedAcceptanceRequired",a.id);
 return frozen_scope(x,a,r);
});}
}
