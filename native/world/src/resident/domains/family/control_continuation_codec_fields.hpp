// Original Control reference fields shared by Control2/3 and the actual
// Stabilize receipt. Writing visitors borrow these values.
#pragma once
#include "control_continuation.hpp"
#include "resident/kernel/common_fact_prefix_codec_fields.hpp"
#include "family_life_exact_codec_detail.hpp"
#include "birth_due_execution_codec_fields.hpp"
#include "resident/domains/interaction/interaction_response_codec_fields.hpp"
namespace hanmo::resident::continuation_codec {
inline bool ref(const StableId& s){return !s.empty()&&s.size()<=256;}
template<class C,class T>Status enumeration(C& c,const StableId& key,T& value,std::initializer_list<const char*> names){Integer ordinal=static_cast<Integer>(value);if(ordinal<0||ordinal>=static_cast<Integer>(names.size()))return c.reject("InvalidControlContinuationTag");StableId name=*(names.begin()+ordinal);c.field(key,name);Integer i=0;for(const auto* n:names){if(name==n){if(c.reading())value=static_cast<T>(i);return c.status();}++i;}return c.reject("InvalidControlContinuationTag");}
// 读写接续因果引用；序号或引用字段无效时拒绝。
template <class C> Status cause(C &c, CauseRef &s) {
  c.field("domain", s.domain);
  c.field("kind", s.kind);
  c.field("fact_id", s.fact_id);
  c.optional("ordinal", s.ordinal, [](C &q, Integer &n) {
    q.field("value", n);
    return n < 0 ? q.reject("InvalidContinuationOrdinal") : q.status();
  });
  return continuation_codec::ref(s.domain) && continuation_codec::ref(s.kind) &&
                 continuation_codec::ref(s.fact_id)
             ? c.status()
             : c.reject("InvalidContinuationCauseRef");
}
// 读写接续同意引用；条款版本或引用字段无效时拒绝。
template <class C> Status consent(C &c, ConsentRef &s) {
  c.field("proposal_id", s.proposal_id);
  c.field("terms_version", s.terms_version);
  c.field("decision_actor_id", s.decision_actor_id);
  c.field("accepted_fact_id", s.accepted_fact_id);
  return continuation_codec::ref(s.proposal_id) && s.terms_version > 0 &&
                 continuation_codec::ref(s.decision_actor_id) &&
                 continuation_codec::ref(s.accepted_fact_id)
             ? c.status()
             : c.reject("InvalidContinuationConsentRef");
}

template<class C>Status path(C& c,ContinuationPath& value){return enumeration(c,"path",value,{"biological","adopted","collateral","adult_adopted","disciple"});}
template<class C>Status owner(C& c,ContinuationBoundaryOwner& value){return enumeration(c,"owner",value,{"S2","S3"});}
template<class C>Status status(C& c,ContinuationBoundaryStatus& value){return enumeration(c,"status",value,{"ready","yielded","candidate_changed","fault"});}
template<class C>Status identity(C& c,WorldIdentity& value){family_life4_codec::world(c,value.world_id);c.field("execution_epoch",value.execution_epoch);c.field("revision",value.revision);c.field("tick",value.tick);return value.execution_epoch<1||value.revision<0||value.tick<0?c.reject("InvalidContinuationSourceIdentity"):c.status();}
// 读写接续候选及其来源；候选代次或角色引用无效时拒绝。
template <class C> Status candidate(C &c, ContinuationCandidateRef &value) {
  c.field("pending_id", value.pending_id);
  c.field("pending_generation", value.pending_generation);
  c.field("candidate_actor_id", value.candidate_actor_id);
  path(c, value.path);
  c.object("basis_ref", [&](C &q) { return cause(q, value.basis_ref); });
  c.object("acceptance_ref",
           [&](C &q) { return consent(q, value.acceptance_ref); });
  c.object("source_identity",
           [&](C &q) { return identity(q, value.source_identity); });
  c.field("control_epoch", value.control_epoch);
  return !continuation_codec::ref(value.pending_id) ||
                 value.pending_generation < 1 ||
                 !continuation_codec::ref(value.candidate_actor_id) ||
                 value.control_epoch < 0
             ? c.reject("InvalidContinuationCandidateRef")
             : c.status();
}
template<class C>Status expected(C& c,ContinuationExpectedBoundary& value){Integer tag=static_cast<Integer>(value.index());c.field("tag",tag);if(!c.status().ok)return c.status();if(tag<0||tag>1)return c.reject("InvalidContinuationExpectedBoundary");if(c.reading()){if(tag==0)value=ContinuationMilitaryBoundary::planning;else value=InteractionResponseNext{InteractionResponseTerminal{}};}if(tag==0)return enumeration(c,"boundary",std::get<ContinuationMilitaryBoundary>(value),{"planning","result","closed"});return c.object("response_next",[&](C& q){return interaction_response_codec::next(q,std::get<InteractionResponseNext>(value));});}
template<class C>Status boundary(C& c,ContinuationBoundaryRef& value){candidate(c,value.candidate);owner(c,value.owner);c.field("session_ref",value.session_ref);c.field("session_generation",value.session_generation);c.field("frozen_segment_ordinal",value.frozen_segment_ordinal);c.object("expected_next_boundary",[&](C& q){return expected(q,value.expected_next_boundary);});if(!c.status().ok)return c.status();auto ok=validate_continuation_boundary_ref_shape(value);return ok.ok?c.status():c.reject(ok.error.original.code);}
template<class C>Status work(C& c,ContinuationBoundaryWork& value){c.field("candidate",value.candidate);path(c,value.path);c.object("basis_ref",[&](C& q){return cause(q,value.basis_ref);});c.object("acceptance_ref",[&](C& q){return consent(q,value.acceptance_ref);});owner(c,value.owner);c.object("session_ref",[&](C& q){q.field("session_id",value.session_ref.session_id);return q.field("session_generation",value.session_ref.session_generation);});c.field("frozen_ordinal",value.frozen_ordinal);enumeration(c,"next_operation",value.next_operation,{"resolve_frozen","close_terminal","resume_selected"});c.sequence("completed_source_refs",value.completed_source_refs,birth_due_codec::moment_source<C>);status(c,value.status);if(!c.status().ok)return c.status();auto ok=validate_continuation_boundary_work_shape(value);return ok.ok?c.status():c.reject(ok.error.original.code);}
template<class C>Status request(C& c,StabilizeContinuationBoundary& value){c.field("pending_id",value.pending_id);c.field("expected_pending_generation",value.expected_pending_generation);c.field("candidate_actor_id",value.candidate_actor_id);path(c,value.path);c.object("basis_ref",[&](C& q){return cause(q,value.basis_ref);});c.object("acceptance_ref",[&](C& q){return consent(q,value.acceptance_ref);});c.object("boundary_ref",[&](C& q){return boundary(q,value.boundary_ref);});if(!c.status().ok)return c.status();auto ok=validate_continuation_boundary_request_shape(value);return ok.ok?c.status():c.reject(ok.error.original.code);}
template<class C>Status result(C& c,ContinuationBoundaryResult& value){c.object("boundary_work_ref",[&](C& q){return birth_due_codec::source(q,value.boundary_work_ref);});status(c,value.status);c.sequence("settled_source_refs",value.settled_source_refs,birth_due_codec::moment_source<C>);c.field("world_half_days",value.world_half_days);c.optional("next_boundary_ref",value.next_boundary_ref,boundary<C>);return c.status();}
template<class C>Status receipt(C& c,ContinuationBoundaryReceipt& value){c.object("request",[&](C& q){return request(q,value.request);});c.object("result",[&](C& q){return result(q,value.result);});c.object("actual_at",[&](C& q){return family_life4_codec::moment(q,value.actual_at);});c.object("candidate_prefix",[&](C& q){return common_fact_prefix_fields::fields(q,value.candidate_prefix,birth_due_codec::source<C>,"InvalidContinuationCandidatePrefix");});if(!c.status().ok)return c.status();auto ok=validate_continuation_boundary_receipt_shape(value);return ok.ok?c.status():c.reject(ok.error.original.code);}
// 读写待接续请求；控制代次、待接续代次或引用无效时拒绝。
template <class C, class T> Status pending_request(C &c, T &value) {
  c.field("pending_id", value.pending_id);
  c.field("expected_control_epoch", value.expected_control_epoch);
  c.field("expected_pending_generation", value.expected_pending_generation);
  return !continuation_codec::ref(value.pending_id) ||
                 value.expected_control_epoch < 0 ||
                 value.expected_pending_generation < 1
             ? c.reject("InvalidContinuationPendingRequest")
             : c.status();
}
template<class C>Status cancelled(C& c,ContinuationBoundaryCancelled& value){c.object("request",[&](C& child){return pending_request(child,value.request);});c.field("generation",value.generation);c.optional("stopped_slice_ref",value.stopped_slice_ref,birth_due_codec::source<C>);if(!c.status().ok)return c.status();auto valid=validate_continuation_boundary_cancelled_shape(value);return valid.ok?c.status():c.reject(valid.error.original.code);}

}
