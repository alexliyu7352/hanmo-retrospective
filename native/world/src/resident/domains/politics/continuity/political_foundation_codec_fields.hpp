// JSON/binary/投影共用同一有限叶布局；无自由 JSON、整数 union tag 或未来 ref
// 回填。
#pragma once
#include "political_foundation_codec.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/kernel/exact_struct_codec.hpp"

namespace hanmo::resident::foundation_codec_fields {
// 原 SourceRef 只保身份，后续业务 reader 仍必须反查真正 owner 与 schema。
template <class C> Status source(C &c, SourceRef &value) {
  c.field("domain", value.domain);
  c.field("kind", value.kind);
  return c.field("id", value.id);
}
template <class C>
Status refs(C &c, const char *name, std::vector<SourceRef> &value) {
  return c.sequence(name, value,
                    [](auto &child, auto &ref) { return source(child, ref); });
}
// 原三 tag 逐字往返，不能接受另一个整数或未知默认 kind。
template <class C> Status kind(C &c, FoundationKind &value) {
  StableId tag;
  if (!c.reading()) {
    const auto *name = foundation_kind_tag(value);
    if (!name)
      return c.reject("UnknownFoundationKind");
    tag = name;
  }
  c.field("kind", tag);
  if (!c.status().ok)
    return c.status();
  const auto selected = foundation_kind_from_tag(tag);
  if (!selected)
    return c.reject("UnknownFoundationKind");
  if (c.reading())
    value = *selected;
  return c.status();
}
// 意向与最终 Terms 的原输入顺序一致；后者只在 founder 后插入已发生接受列表。
template <class C, class Terms> Status prefix(C &c, Terms &value) {
  kind(c, value.kind);
  c.field("organization_kind", value.organization_kind);
  c.field("constitution_id", value.constitution_id);
  c.field("initial_office_id", value.initial_office_id);
  c.field("initial_grant_template_id", value.initial_grant_template_id);
  return c.field("founder", value.founder);
}
template <class C, class Terms> Status resources(C &c, Terms &value) {
  c.optional("base_right_ref", value.base_right_ref,
             [](auto &child, auto &ref) { return source(child, ref); });
  refs(c, "mobile_carrier_refs", value.mobile_carrier_refs);
  refs(c, "asset_contribution_refs", value.asset_contribution_refs);
  refs(c, "supply_contract_refs", value.supply_contract_refs);
  return refs(c, "former_responsibility_dispositions",
              value.former_responsibility_dispositions);
}
template <class C> Status intent_terms(C &c, FoundationIntentTerms &value) {
  prefix(c, value);
  resources(c, value);
  if (c.status().ok) {
    auto valid = validate_foundation_intent_terms_shape(value);
    if (!valid.ok)
      return c.reject(valid.error.original.code, valid.error.original.path);
  }
  return c.status();
}
template <class C> Status terms(C &c, FoundationTerms &value) {
  prefix(c, value);
  refs(c, "accepted_member_refs", value.accepted_member_refs);
  resources(c, value);
  if (c.status().ok) {
    auto valid = validate_foundation_terms_shape(value);
    if (!valid.ok)
      return c.reject(valid.error.original.code, valid.error.original.path);
  }
  return c.status();
}
template <class C> Status intent(C &c, FoundationIntentSource &value) {
  c.field("id", value.id);
  c.field("terms_revision", value.terms_revision);
  c.object("terms",
           [&](auto &child) { return intent_terms(child, value.terms); });
  c.object("proposed_at", [&](auto &child) {
    return exact_fields::moment(child, value.proposed_at);
  });
  if (c.status().ok) {
    auto valid = validate_foundation_intent_source_shape(value);
    if (!valid.ok)
      return c.reject(valid.error.original.code, valid.error.original.path);
  }
  return c.status();
}
template <class C> Status support(C &c, FoundationSupportSource &value) {
  c.field("id", value.id);
  c.object("intent_source",
           [&](auto &child) { return source(child, value.intent_source); });
  c.field("terms_revision", value.terms_revision);
  c.field("actor_id", value.actor_id);
  StableId decision;
  if (!c.reading()) {
    if (value.decision == FoundationSupportDecision::accept)
      decision = "accept";
    else if (value.decision == FoundationSupportDecision::refuse)
      decision = "refuse";
    else
      return c.reject("UnknownFoundationSupportDecision");
  }
  c.field("decision", decision);
  if (!c.status().ok)
    return c.status();
  if (decision != "accept" && decision != "refuse")
    return c.reject("UnknownFoundationSupportDecision");
  if (c.reading())
    value.decision = decision == "accept" ? FoundationSupportDecision::accept
                                          : FoundationSupportDecision::refuse;
  c.object("actual_content_receipt", [&](auto &child) {
    child.object("source_ref", [&](auto &ref) {
      return source(ref, value.actual_content_receipt.source_ref);
    });
    return child.field("source_schema",
                       value.actual_content_receipt.source_schema);
  });
  c.object("decided_at", [&](auto &child) {
    return exact_fields::moment(child, value.decided_at);
  });
  if (c.status().ok) {
    auto valid = validate_foundation_support_source_shape(value);
    if (!valid.ok)
      return c.reject(valid.error.original.code, valid.error.original.path);
  }
  return c.status();
}
// 本次有限两叶，不把尚无真实 source 的成立/解散/迁移当另一个空 tag 放行。
template <class C>
Status decision(C &c, PoliticalLifecycleDecisionRecord &value) {
  StableId tag;
  if (!c.reading()) {
    if (std::holds_alternative<FoundationIntentSource>(value))
      tag = "foundation_intent";
    else if (std::holds_alternative<FoundationSupportSource>(value))
      tag = "foundation_support";
    else
      return c.reject("InvalidFoundationDecisionUnion");
  }
  c.field("kind", tag);
  if (!c.status().ok)
    return c.status();
  if (tag == "foundation_intent") {
    if (c.reading()) {
      auto ready = c.template prepare_default_values<FoundationIntentSource>();
      if (!ready.ok)
        return ready;
      value = FoundationIntentSource{};
    }
    return intent(c, std::get<FoundationIntentSource>(value));
  }
  if (tag == "foundation_support") {
    if (c.reading()) {
      auto ready = c.template prepare_default_values<FoundationSupportSource>();
      if (!ready.ok)
        return ready;
      value = FoundationSupportSource{};
    }
    return support(c, std::get<FoundationSupportSource>(value));
  }
  return c.reject("UnknownFoundationDecisionKind");
}
} // namespace hanmo::resident::foundation_codec_fields
