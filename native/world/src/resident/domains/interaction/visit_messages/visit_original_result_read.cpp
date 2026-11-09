#include "visit_original_result_read.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_life.hpp"
namespace hanmo::resident {namespace {
bool message(const FactRecord&h){return h.domain=="visit_messages"&&(h.kind==visit_message_posted_kind||h.kind==visit_message_received_kind||h.kind==visit_message_withdrawn_kind);}
bool reply(const FactRecord&h){return h.domain==visit_reply_module&&(h.kind==visit_reply_posted_kind||h.kind==visit_reply_collected_kind);}
bool claim(const FactRecord&h){return h.domain==visit_reception_claim_module&&h.kind==visit_reception_claim_kind;}
template<class T>Outcome<T>required(const char*code,const StableId&id){return {false,{},common_failure(code,id,ErrorKind::unsupported_capability).error};}
}
bool native_visit_original_result_kind(const FactRecord&h){return message(h)||reply(h)||claim(h);}
Outcome<ExactOwnerFactRead>NativeVisitOriginalResultRead::read_selected_fact(const FactRecord&h)const{
 const auto*header=x_.common.state().facts.find(h.id);if(!header||header->id!=h.id||header->kind!=h.kind||header->domain!=h.domain)return {false,{},common_failure("VisitOriginalResultHeaderMismatch",h.id,ErrorKind::internal).error};
 Status valid;
 if(message(h)){
  const auto*r=x_.messages;if(!r)return required<ExactOwnerFactRead>("VisitMessageOriginalResultOwnerRequired",h.id);
  if(&r->common!=&x_.common)return {false,{},common_failure("VisitMessageResultCandidateMismatch",h.id,ErrorKind::internal).error};
  valid=validate_visit_messages(r->common,r->state,r->policies,r->interaction,&r->clock);
 }else if(reply(h)){
  const auto*r=x_.replies;if(!r)return required<ExactOwnerFactRead>("VisitReplyOriginalResultOwnerRequired",h.id);
  if(&r->common!=&x_.common)return {false,{},common_failure("VisitReplyResultCandidateMismatch",h.id,ErrorKind::internal).error};
  valid=validate_visit_reply_original_sources(r->common,r->common_catalog,r->messages,r->message_catalog,r->interactions,r->catalog,r->state,r->deaths,r->authorship,r->life);
 }else if(claim(h)){
  const auto*r=x_.reception;if(!r)return required<ExactOwnerFactRead>("VisitReceptionOriginalResultOwnerRequired",h.id);
  if(&r->common!=&x_.common)return {false,{},common_failure("VisitReceptionResultCandidateMismatch",h.id,ErrorKind::internal).error};
  valid=validate_visit_reception_claims(r->common,r->claims,r->catalog,r->message_catalog,r->interaction_catalog,r->interactions,r->actual_clock,r->actual_occupancy);
 }else return required<ExactOwnerFactRead>("UnsupportedVisitOriginalResult",h.id);
 if(!valid.ok)return {false,{},valid.error};
 // Calendar leaf results do not become generic reports or promise a Moment.
 // In particular this path is valid before the real NPC seal/adoption exists.
 ExactOwnerFactRead out;out.disposition=ExactOwnerFactRead::Disposition::no_dynamic_projection;return {true,std::move(out),{}};
}
Outcome<WorldMoment>NativeVisitOriginalResultRead::read_selected_source_moment(const MomentOwnerSource&source)const{
 const auto&ref=source.source_ref;FactRecord h{ref.id,ref.kind,ref.domain};if(source.source_schema!=1||!native_visit_original_result_kind(h))return required<WorldMoment>("UnsupportedVisitOriginalMomentSource",ref.id);
 auto checked=read_selected_fact(h);if(!checked.ok)return {false,{},checked.error};
 if(message(h)){
  if(!x_.receipts||!x_.receipt_sources||!x_.messages)return required<WorldMoment>("VisitMessageActualReceiptMomentOwnerRequired",ref.id);
  if(&x_.receipt_sources->messages!=&x_.messages->state||&x_.receipt_sources->catalog!=&x_.messages->policies)return {false,{},common_failure("VisitMessageMomentCandidateMismatch",ref.id,ErrorKind::internal).error};
  return read_visit_message_adopted_source_moment(x_.common,*x_.receipts,*x_.receipt_sources,ref,source.source_schema);
 }
 if(reply(h)){
  const auto&r=*x_.replies;const auto*row=r.state.posted.find(ref.id);if(r.state.version!=4||h.kind!=visit_reply_posted_kind||!row||!row->authored_origin||!r.actual_commands)return required<WorldMoment>("VisitReplyOriginalCalendarHasNoMomentSource",ref.id);
  auto valid=validate_visit_reply_director_actor_effect(x_.common,r.state,*r.actual_commands,row->contact.actor_id,ref);if(!valid.ok)return {false,{},valid.error};
  auto original=read_visit_authored_command(*r.actual_commands,source,row->contact.actor_id,"reply_visit_message_authored",row->contact.tick);return original.ok?Outcome<WorldMoment>{true,original.value.command->accepted_at,{}}:Outcome<WorldMoment>{false,{},original.error};
 }
 return required<WorldMoment>("VisitReceptionOriginalCalendarHasNoMomentSource",ref.id);
}
}
