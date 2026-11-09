// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 原旁支 source2 的本域codec与局部关系，尚不把形状值当作真实收到/同意。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "collateral_personal.hpp"
#include "collateral_personal_codec_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"

namespace hanmo::resident {
namespace {
// 地址型只核本分支，实际来源仍必须由原 owner 按同候选 Common 头反读。
bool reference(const StableId &id) {
  return !id.empty() && id.size() <= 256;
}
bool collateral_ref(const FamilyProposalRef2 &ref) {
  const auto *original = std::get_if<FamilyCollateralProposalRef2>(&ref);
  return original && reference(original->id);
}
// 默认值按真实成员递归；固定kind是类型常量，不分配一份可改字符串。

// 只借同一字段表投影，不能复制另一份祖系作为 accepted 的状态。
template <class T, class Fields>
Status emit(ProjectionWriter &writer, const T &value, Fields fields) {
  ActorFoundationProjectionArchive archive(writer);
  return fields(archive, const_cast<T &>(value));
}
template <class T, class Fields>
Outcome<BoundaryValue> encode(const T &value, Fields fields) {
  BoundaryValue out;
  BoundaryProjectionWriter writer(out);
  auto status = writer.object(
      "", [&](ProjectionWriter &child) { return emit(child, value, fields); });
  if (status.ok)
    status = writer.finish();
  return {status.ok, std::move(out), status.error};
}
template <class T, class Fields>
Outcome<T> decode(const BoundaryValue &raw, Fields fields) {
  if (auto *scope = ScopedCheckpointJsonDecode::current())
    scope->default_values<T>();
  T value;
  ActorFoundationJsonVisitor reader(raw);
  auto status = fields(reader, value);
  if (status.ok)
    status = reader.finish();
  return {status.ok, std::move(value), status.error};
}
} // namespace

SourceRef collateral_personal_proposal_source(const StableId &id) {
  return {"family_collateral_legacy", "family_collateral_proposed", id};
}
SourceRef collateral_personal_reply_source(const StableId &id) {
  return {"family_collateral_legacy", "family_collateral_replied", id};
}
SourceRef collateral_personal_acceptance_source(const StableId &id) {
  return {"family_collateral_legacy", "family_collateral_recognized", id};
}
// 只检查地址类别，recognition同kind也有旧schema1，版本必须反读实际wrapper。
Status validate_collateral_personal_source_address(const SourceRef &source) {
  if (source.domain != "family_collateral_legacy" || !reference(source.id) ||
      (source.kind != "family_collateral_proposed" &&
       source.kind != "family_collateral_replied" &&
       source.kind != "family_collateral_recognized"))
    return common_failure("UnknownCollateralPersonalSource", source.id,
                          ErrorKind::unsupported_capability);
  return common_success();
}

Status validate_collateral_personal_proposal_relations(
    const CollateralPersonalProposal2 &value) {
  const auto &terms = value.terms;
  if (!reference(terms.template_ref.template_id) ||
      terms.template_ref.version != 1 || terms.terms_version != 1 ||
      !reference(terms.first_actor_id) || !reference(terms.second_actor_id) ||
      terms.first_actor_id == terms.second_actor_id ||
      terms.ancestry.first_actor_id != terms.first_actor_id ||
      terms.ancestry.second_actor_id != terms.second_actor_id ||
      value.expression.actor_id != terms.second_actor_id ||
      value.previous_proposal_ref || value.negotiated_reply_ref ||
      value.negotiation_round != 0)
    return common_failure("InvalidCollateralPersonalProposal");
  return validate_family_personal_expression2_relations(value.expression);
}
Status validate_collateral_personal_reply_relations(
    const CollateralPersonalReply2 &value) {
  if (!collateral_ref(value.proposal_ref) || value.counter_proposal_ref ||
      (value.decision != FamilyPersonalReplyDecision2::refused &&
       value.decision != FamilyPersonalReplyDecision2::withdrawn))
    return common_failure("InvalidCollateralPersonalNonacceptReply");
  return validate_family_personal_expression2_relations(value.expression);
}
Status validate_collateral_personal_acceptance_relations(
    const CollateralPersonalAccepted2 &value) {
  if (!collateral_ref(value.proposal_ref))
    return common_failure("CollateralPersonalProposalRequired");
  return validate_family_personal_expression2_relations(value.expression);
}

// CollateralPersonalTerms2 的默认拥有成员，非字段数估算。


// CollateralPersonalProposal2 的默认拥有成员，非字段数估算。


// CollateralPersonalReply2 的默认拥有成员，非字段数估算。


// CollateralPersonalAccepted2 的默认拥有成员，非字段数估算。


// CollateralPersonalProposal2 的三个出口只使用原唯一字段表。
Status visit_collateral_personal_proposal(CodecVisitor &c,
                                          CollateralPersonalProposal2 &value) {
  return collateral_personal_codec::proposal(c, value);
}
Status emit_collateral_personal_proposal_fields(
    ProjectionWriter &writer, const CollateralPersonalProposal2 &value) {
  return emit(writer, value, [](auto &a, auto &x) {
    return collateral_personal_codec::proposal(a, x);
  });
}
Outcome<BoundaryValue>
encode_collateral_personal_proposal(const CollateralPersonalProposal2 &value) {
  return encode(value, [](auto &a, auto &x) {
    return collateral_personal_codec::proposal(a, x);
  });
}
Outcome<CollateralPersonalProposal2>
decode_collateral_personal_proposal(const BoundaryValue &raw) {
  return decode<CollateralPersonalProposal2>(raw, [](auto &a, auto &x) {
    return collateral_personal_codec::proposal(a, x);
  });
}

// CollateralPersonalReply2 的三个出口只使用原唯一字段表。
Status visit_collateral_personal_reply(CodecVisitor &c,
                                       CollateralPersonalReply2 &value) {
  return collateral_personal_codec::reply(c, value);
}
Status
emit_collateral_personal_reply_fields(ProjectionWriter &writer,
                                      const CollateralPersonalReply2 &value) {
  return emit(writer, value, [](auto &a, auto &x) {
    return collateral_personal_codec::reply(a, x);
  });
}
Outcome<BoundaryValue>
encode_collateral_personal_reply(const CollateralPersonalReply2 &value) {
  return encode(value, [](auto &a, auto &x) {
    return collateral_personal_codec::reply(a, x);
  });
}
Outcome<CollateralPersonalReply2>
decode_collateral_personal_reply(const BoundaryValue &raw) {
  return decode<CollateralPersonalReply2>(raw, [](auto &a, auto &x) {
    return collateral_personal_codec::reply(a, x);
  });
}

// CollateralPersonalAccepted2 的三个出口只使用原唯一字段表。
Status
visit_collateral_personal_acceptance(CodecVisitor &c,
                                     CollateralPersonalAccepted2 &value) {
  return collateral_personal_codec::acceptance(c, value);
}
Status emit_collateral_personal_acceptance_fields(
    ProjectionWriter &writer, const CollateralPersonalAccepted2 &value) {
  return emit(writer, value, [](auto &a, auto &x) {
    return collateral_personal_codec::acceptance(a, x);
  });
}
Outcome<BoundaryValue> encode_collateral_personal_acceptance(
    const CollateralPersonalAccepted2 &value) {
  return encode(value, [](auto &a, auto &x) {
    return collateral_personal_codec::acceptance(a, x);
  });
}
Outcome<CollateralPersonalAccepted2>
decode_collateral_personal_acceptance(const BoundaryValue &raw) {
  return decode<CollateralPersonalAccepted2>(raw, [](auto &a, auto &x) {
    return collateral_personal_codec::acceptance(a, x);
  });
}

namespace collateral_personal_codec {
// 只委托原祖系decoder；原path分配按当前真实scope付费，不重写字段或容许缺源。
Outcome<CollateralFamilyOrigin>
decode_original_ancestry(const BoundaryValue &raw,
                         ScopedCheckpointJsonDecode &scope) {
  if (ScopedCheckpointJsonDecode::current() != &scope)
    return {
        false, {}, common_failure("CollateralSameJsonCandidateRequired").error};
  scope.default_values<CollateralFamilyOrigin>();
  return decode_collateral_family_origin(raw);
}
} // namespace collateral_personal_codec
} // namespace hanmo::resident
