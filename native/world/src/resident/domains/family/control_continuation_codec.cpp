#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "control_continuation.hpp"
#include "control_continuation_codec_fields.hpp"
#include "family_life_exact_codec_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <limits>
namespace hanmo::resident {namespace {
using continuation_codec::enumeration;
using continuation_codec::cause;
using continuation_codec::consent;
// 读写控制绑定并校验角色引用与控制代次；字段无效时拒绝。
template <class C> Status binding(C &c, ControlBinding &s) {
  c.field("actor_id", s.actor_id);
  c.field("control_epoch", s.control_epoch);
  return continuation_codec::ref(s.actor_id) && s.control_epoch >= 0
             ? c.status()
             : c.reject("InvalidControlBinding");
}
// 按原版本顺序读写待接续记录；版本不支持边界工作或字段无效时拒绝。
template <class C>
Status pending(C &c, PendingContinuation &s, Integer version) {
  if (version == 2 && s.boundary_work)
    return c.reject("ContinuationBoundaryWorkRequiresControl3");
  c.field("pending_id", s.pending_id);
  enumeration(c, "mode", s.mode, {"voluntary_retirement", "after_death"});
  c.field("predecessor_actor_id", s.predecessor_actor_id);
  c.object("source_ref", [&](C &q) { return cause(q, s.source_ref); });
  c.field("created_tick", s.created_tick);
  c.field("generation", s.generation);
  enumeration(c, "status", s.status,
              {"open", "activated", "cancelled", "terminal_retained"});
  c.field("control_epoch_at_open", s.control_epoch_at_open);
  if (version == 3)
    c.optional("boundary_work", s.boundary_work, continuation_codec::work<C>);
  return continuation_codec::ref(s.pending_id) &&
                 continuation_codec::ref(s.predecessor_actor_id) &&
                 s.created_tick >= 0 && s.generation >= 1 &&
                 s.control_epoch_at_open >= 0
             ? c.status()
             : c.reject("InvalidPendingContinuation");
}
// 读写接控请求来源；世界身份或原请求序号无效时拒绝。
template <class C> Status origin(C &c, ControlRequestOrigin &s) {
  family_life4_codec::world(c, s.world_id);
  c.field("execution_epoch_at_commit", s.execution_epoch_at_commit);
  c.field("predecessor_actor_id", s.predecessor_actor_id);
  c.field("request_seq", s.request_seq);
  bool nonzero = false;
  for (auto byte : s.world_id)
    nonzero |= byte != 0;
  return nonzero && s.execution_epoch_at_commit >= 1 &&
                 continuation_codec::ref(s.predecessor_actor_id) &&
                 s.request_seq >= 1
             ? c.status()
             : c.reject("InvalidControlActivationOrigin");
}
// 读写接控记录并保持原因果与控制代次校验；来源不一致时拒绝。
template <class C> Status activation(C &c, ControlActivationRecord &s) {
  c.field("activation_id", s.activation_id);
  c.field("pending_id", s.pending_id);
  c.field("predecessor_actor_id", s.predecessor_actor_id);
  c.field("successor_actor_id", s.successor_actor_id);
  enumeration(
      c, "path", s.path,
      {"biological", "adopted", "collateral", "adult_adopted", "disciple"});
  c.object("basis_ref", [&](C &q) { return cause(q, s.basis_ref); });
  c.object("acceptance_ref",
           [&](C &q) { return consent(q, s.acceptance_ref); });
  enumeration(c, "mode", s.mode, {"voluntary_retirement", "after_death"});
  c.object("source_ref", [&](C &q) { return cause(q, s.source_ref); });
  c.field("effective_tick", s.effective_tick);
  c.field("previous_control_epoch", s.previous_control_epoch);
  c.field("new_control_epoch", s.new_control_epoch);
  c.field("command_result_ref", s.command_result_ref);
  c.object("request_origin", [&](C &q) { return origin(q, s.request_origin); });
  c.field("command_kind", s.command_kind);
  c.field("committed_tick", s.committed_tick);
  c.field("committed_revision", s.committed_revision);
  return continuation_codec::ref(s.activation_id) &&
                 continuation_codec::ref(s.pending_id) &&
                 continuation_codec::ref(s.predecessor_actor_id) &&
                 continuation_codec::ref(s.successor_actor_id) &&
                 s.predecessor_actor_id != s.successor_actor_id &&
                 continuation_codec::ref(s.command_result_ref) &&
                 continuation_codec::ref(s.command_kind) &&
                 s.effective_tick >= 0 && s.previous_control_epoch >= 0 &&
                 s.previous_control_epoch <
                     std::numeric_limits<Integer>::max() &&
                 s.new_control_epoch == s.previous_control_epoch + 1 &&
                 s.committed_tick == s.effective_tick &&
                 s.committed_revision >= 1 &&
                 s.request_origin.predecessor_actor_id ==
                     s.predecessor_actor_id &&
                 s.acceptance_ref.decision_actor_id == s.successor_actor_id
             ? c.status()
             : c.reject("InvalidControlActivationOrigin");
}
template<class C>Status source(C& c,ContinuationConsentSource& s){enumeration(c,"decision",s.decision,{"accept","revoke"});c.object("consent_ref",[&](C& q){return consent(q,s.consent_ref);});return c.object("original_source",[&](C& q){return cause(q,s.original_source);});}
template<class C,class T,class F>Status table(C& c,const StableId& name,OrderedTable<T>& rows,F visitor){if constexpr(std::is_same_v<C,CodecVisitor>)return common_checkpoint::table(c,name,rows,visitor);else return c.table(name,rows,visitor);}
template<class C>Status state(C& c,ControlContinuationState& s,Integer version){if(version!=2&&version!=3)return c.reject("UnsupportedControlContinuationVersion");c.object("control_binding",[&](C& q){return binding(q,s.control_binding);});table(c,"pending_continuations",s.pending_continuations,[&](C& q,PendingContinuation& value){return pending(q,value,version);});table(c,"activation_records",s.activation_records,activation<C>);return table(c,"continuation_consent_sources",s.continuation_consent_sources,source<C>);}
}
Status visit_control_continuation_v2(CodecVisitor& c,ControlContinuationState& s){return state(c,s,2);}
Status emit_control_continuation_v2_projection(ProjectionWriter& w,std::string_view name,const ControlContinuationState& s){return emit_projection_record<ActorFoundationProjectionArchive>(w,name,s,[](auto& c,auto& value){return state(c,value,2);});}
Outcome<BoundaryValue> encode_control_continuation_v2(const ControlContinuationState& s){BoundaryValue out;BoundaryProjectionWriter w(out);auto ok=emit_control_continuation_v2_projection(w,"",s);if(ok.ok)ok=w.finish();return {ok.ok,std::move(out),ok.error};}
Outcome<ControlContinuationState> decode_control_continuation_v2(const BoundaryValue& v){ControlContinuationState out;ActorFoundationJsonVisitor c(v);auto ok=state(c,out,2);if(ok.ok)ok=c.finish();return {ok.ok,std::move(out),ok.error};}
Status visit_control_continuation_v3(CodecVisitor& c,ControlContinuationState& s){return state(c,s,3);}
Status emit_control_continuation_v3_projection(ProjectionWriter& w,std::string_view name,const ControlContinuationState& s){return emit_projection_record<ActorFoundationProjectionArchive>(w,name,s,[](auto& c,auto& value){return state(c,value,3);});}
Outcome<BoundaryValue> encode_control_continuation_v3(const ControlContinuationState& s){BoundaryValue out;BoundaryProjectionWriter w(out);auto ok=emit_control_continuation_v3_projection(w,"",s);if(ok.ok)ok=w.finish();return {ok.ok,std::move(out),ok.error};}
Outcome<ControlContinuationState> decode_control_continuation_v3(const BoundaryValue& v){ControlContinuationState out;ActorFoundationJsonVisitor c(v);auto ok=state(c,out,3);if(ok.ok)ok=c.finish();return {ok.ok,std::move(out),ok.error};}
Outcome<StabilizeContinuationBoundary> decode_stabilize_continuation_boundary(const BoundaryValue& value){StabilizeContinuationBoundary out;ActorFoundationJsonVisitor c(value);auto ok=continuation_codec::request(c,out);if(ok.ok)ok=c.finish();return {ok.ok,std::move(out),ok.error};}
Status visit_continuation_boundary_receipt(CodecVisitor& c,ContinuationBoundaryReceipt& value){return continuation_codec::receipt(c,value);}
Status emit_continuation_boundary_receipt_fields(ProjectionWriter& w,const ContinuationBoundaryReceipt& value){ActorFoundationProjectionArchive c(w);auto ok=continuation_codec::receipt(c,const_cast<ContinuationBoundaryReceipt&>(value));return ok.ok?c.finish():ok;}
Outcome<BoundaryValue> encode_continuation_boundary_receipt(const ContinuationBoundaryReceipt& value){BoundaryValue out;BoundaryProjectionWriter w(out);auto ok=w.object("",[&](ProjectionWriter& root){return emit_continuation_boundary_receipt_fields(root,value);});if(ok.ok)ok=w.finish();return {ok.ok,std::move(out),ok.error};}
Outcome<ContinuationBoundaryReceipt> decode_continuation_boundary_receipt(const BoundaryValue& value){ContinuationBoundaryReceipt out;ActorFoundationJsonVisitor c(value);auto ok=continuation_codec::receipt(c,out);if(ok.ok)ok=c.finish();return {ok.ok,std::move(out),ok.error};}

Outcome<CancelContinuationBoundary> decode_cancel_continuation_boundary(const BoundaryValue& value){CancelContinuationBoundary out;ActorFoundationJsonVisitor c(value);auto valid=continuation_codec::pending_request(c,out);if(valid.ok)valid=c.finish();return {valid.ok,std::move(out),valid.error};}
Status visit_continuation_boundary_cancelled(CodecVisitor& c,ContinuationBoundaryCancelled& value){return continuation_codec::cancelled(c,value);}
Status emit_continuation_boundary_cancelled_fields(ProjectionWriter& w,const ContinuationBoundaryCancelled& value){ActorFoundationProjectionArchive c(w);auto valid=continuation_codec::cancelled(c,const_cast<ContinuationBoundaryCancelled&>(value));return valid.ok?c.finish():valid;}
Outcome<BoundaryValue> encode_continuation_boundary_cancelled(const ContinuationBoundaryCancelled& value){BoundaryValue out;BoundaryProjectionWriter w(out);auto valid=w.object("",[&](ProjectionWriter& root){return emit_continuation_boundary_cancelled_fields(root,value);});if(valid.ok)valid=w.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<ContinuationBoundaryCancelled> decode_continuation_boundary_cancelled(const BoundaryValue& value){ContinuationBoundaryCancelled out;ActorFoundationJsonVisitor c(value);auto valid=continuation_codec::cancelled(c,out);if(valid.ok)valid=c.finish();return {valid.ok,std::move(out),valid.error};}

}
