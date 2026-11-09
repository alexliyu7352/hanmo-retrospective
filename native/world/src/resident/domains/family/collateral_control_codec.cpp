// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 原旁支根的显式1/2物理选择：旧三表原字节不改，新三类表达不伪旧commit。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "collateral_control_state.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/family/collateral_personal_codec_fields.hpp"
#include "resident/domains/family/family_extension_projection.hpp"
#include "resident/domains/family/succession_personal_assets.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>

namespace hanmo::resident {
namespace {
using V = BoundaryValue;
// 旧schema1允许空记序，非空必须恰好为原键集合；不按字段存在猜版本。
bool order(const FieldOrder &have, const std::set<StableId> &wanted) {
  return have.empty() ||
         (have.size() == wanted.size() &&
          std::set<StableId>(have.begin(), have.end()) == wanted);
}
Status binding(const CollateralCommitBinding &b, bool recognition) {
  for (const auto *id : {&b.result_id, &b.actor_id, &b.kind})
    if (id->empty() || id->size() > 256)
      return common_failure("InvalidCollateralCommitBinding");
  if ((recognition ? b.kind != "family_collateral_recognize"
                   : b.kind != "family_collateral_accept" &&
                         b.kind != "family_collateral_revoke" &&
                         b.kind != "family_legacy_activate") ||
      b.request_seq < 1 || b.request_seq > 2147483647 || b.revision < 1 ||
      !order(b.field_order,
             {"result_id", "actor_id", "kind", "request_seq", "revision"}))
    return common_failure("InvalidCollateralCommitBinding");
  return common_success();
}
Status legacy_fact_shape(const CollateralRecognitionFact &f) {
  if (f.kind != "family_collateral_recognized" ||
      !order(f.field_order, {"kind", "source"}))
    return common_failure("InvalidCollateralRecognitionFact");
  return validate_collateral_recognition_source_shape(f.source);
}
// 这里只解原schema1完整payload。所有source字段仍委托原owner，不加expression。
Outcome<CollateralRecognitionFact> decode_legacy_fact(const V &raw) {
  try {
    boundary::fields(raw, {"kind", "source"}, {}, "collateral_recognition");
    if (auto *scope = ScopedCheckpointJsonDecode::current())
      scope->default_values<CollateralRecognitionFact>();
    CollateralRecognitionFact value;
    value.kind = boundary::get_string(raw, "kind", "");
    auto source = decode_collateral_recognition_source(
        boundary::field(raw, "source", ""));
    if (!source.ok)
      return {false, {}, source.error};
    value.source = std::move(source.value);
    value.field_order = boundary::order(raw);
    auto valid = legacy_fact_shape(value);
    return {valid.ok, std::move(value), valid.error};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
Status emit_legacy_fact(ProjectionWriter &w,
                        const CollateralRecognitionFact &f) {
  w.field("kind", f.kind);
  w.object("source", [&](auto &child) {
    return emit_collateral_recognition_source_fields(child, f.source);
  });
  return w.order_fields(f.field_order);
}
// 原source1行的binary序保持kind/source/field_order，JSON仍原两键。
template <class C> Status legacy_fact(C &c, CollateralRecognitionFact &f) {
  if constexpr (std::is_same_v<C, CodecVisitor>) {
    c.field("kind", f.kind);
    c.object("source", [&](auto &child) {
      return visit_collateral_recognition_source_checkpoint(child, f.source);
    });
    return common_checkpoint::strings(c, "field_order", f.field_order);
  } else if (ScopedCheckpointJsonDecode::current()) {
    return c.foreign_record(
        f,
        [](const V &raw, ScopedCheckpointJsonDecode &scope) {
          if (&scope != ScopedCheckpointJsonDecode::current())
            return Outcome<CollateralRecognitionFact>{
                false,
                {},
                common_failure("CollateralSameJsonCandidateRequired").error};
          return decode_legacy_fact(raw);
        },
        emit_legacy_fact);
  } else {
    return c.foreign_record(f, decode_legacy_fact, emit_legacy_fact);
  }
}
// 新wrapper唯一字段表；schema1必须实际旧型，schema2必须实际新型。
template <class C>
Status recognition(C &c, VersionedCollateralRecognitionFact &f,
                   Integer selected) {
  if (selected == 1) {
    if (!std::holds_alternative<CollateralRecognitionFact>(f.payload) ||
        !f.field_order.empty())
      return c.reject("CollateralSource1PayloadRequired");
    return legacy_fact(c, std::get<CollateralRecognitionFact>(f.payload));
  }
  if (selected != 2)
    return c.reject("UnsupportedCollateralControlVersion");
  Integer schema = static_cast<Integer>(f.payload.index()) + 1;
  c.field("payload_schema", schema);
  if (!c.status().ok)
    return c.status();
  if (schema != 1 && schema != 2)
    return c.reject("UnsupportedCollateralPersonalPayloadSchema");
  if (c.reading()) {
    auto ready = schema == 1
                     ? care_clearance_origin_codec::select<0>(c, f.payload)
                     : care_clearance_origin_codec::select<1>(c, f.payload);
    if (!ready.ok)
      return ready;
  }
  c.object("payload", [&](auto &child) {
    return schema == 1 ? legacy_fact(child, std::get<0>(f.payload))
                       : collateral_personal_codec::acceptance(
                             child, std::get<1>(f.payload));
  });
  return care_clearance_history_codec::exact_order(
      c, f.field_order, {"payload_schema", "payload", "field_order"});
}
// 旧commit只沿原字段，不将source2的NPC/本人表达写成旧player late-bind。
Status visit_binding(CodecVisitor &c, CollateralCommitBinding &b) {
  c.field("result_id", b.result_id);
  c.field("actor_id", b.actor_id);
  c.field("kind", b.kind);
  c.field("request_seq", b.request_seq);
  c.field("revision", b.revision);
  return common_checkpoint::strings(c, "field_order", b.field_order);
}
Outcome<CollateralCommitBinding> decode_binding(const V &raw) {
  try {
    boundary::fields(
        raw, {"result_id", "actor_id", "kind", "request_seq", "revision"}, {},
        "collateral_commit");
    return {true,
            {boundary::get_string(raw, "result_id", ""),
             boundary::get_string(raw, "actor_id", ""),
             boundary::get_string(raw, "kind", ""),
             boundary::get_int(raw, "request_seq", ""),
             boundary::get_int(raw, "revision", ""), boundary::order(raw)},
            {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
template <class C> Status commit(C &c, CollateralCommitBinding &b) {
  if constexpr (std::is_same_v<C, CodecVisitor>)
    return visit_binding(c, b);
  else {
    const auto emit = [](ProjectionWriter &w,
                         const CollateralCommitBinding &v) {
      return family_projection::control_commit_fields(w, v);
    };
    if (ScopedCheckpointJsonDecode::current())
      return c.foreign_record(
          b,
          [](const V &raw, ScopedCheckpointJsonDecode &scope) {
            if (&scope != ScopedCheckpointJsonDecode::current())
              return Outcome<CollateralCommitBinding>{
                  false,
                  {},
                  common_failure("CollateralSameJsonCandidateRequired").error};
            scope.default_values<CollateralCommitBinding>();
            return decode_binding(raw);
          },
          emit);
    return c.foreign_record(b, decode_binding, emit);
  }
}
// 唯一原表顺序；根的JSON记序为原字典键序，不添一个新field_order键。
template <class C>
Status root(C &c, CollateralControlState &s, Integer selected) {
  if (selected != 1 && selected != 2)
    return c.reject("UnsupportedCollateralControlVersion");
  const auto table = [&](std::string_view name, auto &rows, auto fields) {
    if constexpr (std::is_same_v<C, CodecVisitor>)
      return common_checkpoint::table(c, name, rows, fields);
    else
      return c.table(name, rows, fields);
  };
  const auto field = [&](std::string_view name) -> Status {
    if (name == "recognitions")
      return table(name, s.recognitions, [&](auto &v, auto &x) {
        return recognition(v, x, selected);
      });
    if (name == "recognition_commits")
      return table(name, s.recognition_commits,
                   [](auto &v, auto &x) { return commit(v, x); });
    if (name == "legacy_commits")
      return table(name, s.legacy_commits,
                   [](auto &v, auto &x) { return commit(v, x); });
    if (selected == 2 && name == "proposals")
      return table(name, s.proposals, [](auto &v, auto &x) {
        return collateral_personal_codec::proposal(v, x);
      });
    if (selected == 2 && name == "replies")
      return table(name, s.replies, [](auto &v, auto &x) {
        return collateral_personal_codec::reply(v, x);
      });
    return c.reject("UnsupportedCollateralRootField", name);
  };
  if (!c.reading()) {
    auto valid = validate_collateral_control_shape(s, selected);
    if (!valid.ok)
      return c.reject(valid.error.original.code);
  }
  if constexpr (!std::is_same_v<C, CodecVisitor>) {
    if (!c.reading() && !s.field_order.empty()) {
      for (const auto &name : s.field_order)
        if (!field(name).ok)
          return c.status();
      return c.status();
    }
  }
  for (const auto name :
       {"recognitions", "recognition_commits", "legacy_commits"})
    field(name);
  if (selected == 2) {
    field("proposals");
    field("replies");
  }
  if constexpr (std::is_same_v<C, CodecVisitor>)
    common_checkpoint::strings(c, "field_order", s.field_order);
  if (!c.status().ok)
    return c.status();
  auto valid = validate_collateral_control_shape(s, selected);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
} // namespace

// 字段、表身份与局部关系；真实来源/本人资格/历史时间由原source reader另验。
Status validate_collateral_control_shape(const CollateralControlState &s,
                                         Integer selected) {
  if (selected != 1 && selected != 2)
    return common_failure("UnsupportedCollateralControlVersion");
  const std::set<StableId> keys =
      selected == 1
          ? std::set<StableId>{"recognitions", "recognition_commits",
                               "legacy_commits"}
          : std::set<StableId>{"recognitions", "recognition_commits",
                               "legacy_commits", "proposals", "replies"};
  if (!order(s.field_order, keys))
    return common_failure("BoundaryFieldOrderMismatch");
  if (selected == 1 && (!s.proposals.empty() || !s.replies.empty()))
    return common_failure("CollateralSource1PayloadRequired");
  std::set<StableId> source_ids, terminal_proposals;
  const auto id = [&](const StableId &key) {
    return !key.empty() && key.size() <= 256 && source_ids.insert(key).second;
  };
  for (const auto &entry : s.proposals.rows()) {
    if (!id(entry.first))
      return common_failure("InvalidCollateralPersonalSourceId", entry.first);
    auto valid = validate_collateral_personal_proposal_relations(entry.second);
    if (!valid.ok)
      return valid;
  }
  const auto proposal = [&](const FamilyProposalRef2 &ref)
      -> const CollateralPersonalProposal2 * {
    const auto *p = std::get_if<FamilyCollateralProposalRef2>(&ref);
    return p ? s.proposals.find(p->id) : nullptr;
  };
  for (const auto &entry : s.replies.rows()) {
    if (!id(entry.first))
      return common_failure("InvalidCollateralPersonalSourceId", entry.first);
    const auto &reply = entry.second;
    auto valid = validate_collateral_personal_reply_relations(reply);
    if (!valid.ok)
      return valid;
    const auto *p = proposal(reply.proposal_ref);
    if (!p ||
        !terminal_proposals
             .insert(
                 std::get<FamilyCollateralProposalRef2>(reply.proposal_ref).id)
             .second ||
        reply.expression.actor_id !=
            (reply.decision == FamilyPersonalReplyDecision2::refused
                 ? p->terms.first_actor_id
                 : p->terms.second_actor_id))
      return common_failure("CollateralPersonalReplyOriginalProposalMismatch",
                            entry.first);
  }
  for (const auto &entry : s.recognitions.rows()) {
    if (!id(entry.first))
      return common_failure("InvalidCollateralRecognitionFact", entry.first);
    const auto &v = entry.second;
    if (selected == 1 &&
        (!std::holds_alternative<CollateralRecognitionFact>(v.payload) ||
         !v.field_order.empty()))
      return common_failure("CollateralSource1PayloadRequired", entry.first);
    if (selected == 2 &&
        !order(v.field_order, {"payload_schema", "payload", "field_order"}))
      return common_failure("BoundaryFieldOrderMismatch", entry.first);
    if (const auto *old = std::get_if<CollateralRecognitionFact>(&v.payload)) {
      auto valid = legacy_fact_shape(*old);
      if (!valid.ok)
        return valid;
    } else if (const auto *accepted =
                   std::get_if<CollateralPersonalAccepted2>(&v.payload)) {
      auto valid = validate_collateral_personal_acceptance_relations(*accepted);
      if (!valid.ok)
        return valid;
      const auto *p = proposal(accepted->proposal_ref);
      if (!p ||
          !terminal_proposals
               .insert(std::get<FamilyCollateralProposalRef2>(
                           accepted->proposal_ref)
                           .id)
               .second ||
          accepted->expression.actor_id != p->terms.first_actor_id ||
          s.recognition_commits.find(entry.first))
        return common_failure(
            "CollateralPersonalAcceptanceOriginalProposalMismatch",
            entry.first);
    } else
      return common_failure("UnsupportedCollateralPersonalPayloadSchema",
                            entry.first);
  }
  for (const auto *table : {&s.recognition_commits, &s.legacy_commits})
    for (const auto &row : table->rows()) {
      if (row.first.empty() || row.first.size() > 256)
        return common_failure("InvalidCollateralCommitSource");
      auto valid = binding(row.second, table == &s.recognition_commits);
      if (!valid.ok)
        return valid;
    }
  return common_success();
}
Status validate_collateral_control_shape(const CollateralControlState &s) {
  return validate_collateral_control_shape(s, 1);
}
Outcome<CollateralControlState>
decode_collateral_control_state(const V &raw, Integer selected) {
  try {
    auto unique = validate_succession_json_unique(raw);
    if (!unique.ok)
      return {false, {}, unique.error};
    if (selected != 1 && selected != 2)
      return {false,
              {},
              common_failure("UnsupportedCollateralControlVersion").error};
    if (auto *scope = ScopedCheckpointJsonDecode::current())
      scope->default_values<CollateralControlState>();
    CollateralControlState value;
    value.field_order = boundary::order(raw);
    ActorFoundationJsonVisitor reader(raw);
    auto status = root(reader, value, selected);
    if (status.ok)
      status = reader.finish();
    return {status.ok, std::move(value), status.error};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
Outcome<CollateralControlState> decode_collateral_control_state(const V &raw) {
  return decode_collateral_control_state(raw, 1);
}
Status emit_collateral_control_state_fields(ProjectionWriter &w,
                                            const CollateralControlState &s,
                                            Integer selected) {
  ActorFoundationProjectionArchive writer(w);
  return root(writer, const_cast<CollateralControlState &>(s), selected);
}
Status emit_collateral_control_state_fields(ProjectionWriter &w,
                                            const CollateralControlState &s) {
  return emit_collateral_control_state_fields(w, s, 1);
}
BoundaryValue encode_collateral_control_state(const CollateralControlState &s,
                                              Integer selected) {
  return family_projection::encode([&](auto &w) {
    return emit_collateral_control_state_fields(w, s, selected);
  });
}
BoundaryValue encode_collateral_control_state(const CollateralControlState &s) {
  return encode_collateral_control_state(s, 1);
}
Status visit_collateral_control_checkpoint(CodecVisitor &c,
                                           CollateralControlState &s,
                                           Integer selected) {
  return root(c, s, selected);
}
Status visit_collateral_control_checkpoint(CodecVisitor &c,
                                           CollateralControlState &s) {
  return visit_collateral_control_checkpoint(c, s, 1);
}
// 新wrapper默认实际拥有旧kind字符串，直接复用原类型的成员计量。

} // namespace hanmo::resident
