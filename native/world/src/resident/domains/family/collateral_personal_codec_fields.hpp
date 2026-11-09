// 原三种 source2 payload 的唯一字段表；不另存祖系或替旧 /1 改字节。
#pragma once
#include "care_education_codec_fields.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/family/care_education.hpp"
#include "resident/domains/family/collateral_personal.hpp"
#include "family_source_projection.hpp"
namespace hanmo::resident::collateral_personal_codec {
using care_clearance_history_codec::exact_order;
using care_education_codec::expression;
using care_facility_transport_codec::fixed_text;
using care_requirement_codec::text;

// 原祖系仍由其独有 decoder/visitor/emitter 持有，明确借同job scope。
Outcome<CollateralFamilyOrigin>
decode_original_ancestry(const BoundaryValue &, ScopedCheckpointJsonDecode &);
inline Status ancestry(CodecVisitor &c, CollateralFamilyOrigin &value) {
  return visit_collateral_family_origin_checkpoint(c, value);
}
template <class C> Status ancestry(C &c, CollateralFamilyOrigin &value) {
  if (ScopedCheckpointJsonDecode::current())
    return c.foreign_record(value, decode_original_ancestry,
                            emit_collateral_family_origin_fields);
  return c.foreign_record(value, decode_collateral_family_origin,
                          emit_collateral_family_origin_fields);
}

// wire tag0..5 一一对应原合同。旁支自己的行随后严格只允许 tag3。
template <class C> Status proposal_ref(C &c, FamilyProposalRef2 &value) {
  Integer tag = static_cast<Integer>(value.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 5)
    return c.reject("UnknownFamilyProposalRef2Tag");
  if (c.reading()) {
    switch (tag) {
    case 0:
      care_clearance_origin_codec::select<0>(c, value);
      break;
    case 1:
      care_clearance_origin_codec::select<1>(c, value);
      break;
    case 2:
      care_clearance_origin_codec::select<2>(c, value);
      break;
    case 3:
      care_clearance_origin_codec::select<3>(c, value);
      break;
    case 4:
      care_clearance_origin_codec::select<4>(c, value);
      break;
    case 5:
      care_clearance_origin_codec::select<5>(c, value);
      break;
    }
  }
  if (!c.status().ok)
    return c.status();
  return std::visit(
      [&](auto &ref) -> Status {
        text(c, "id", ref.id);
        if constexpr (std::is_same_v<std::decay_t<decltype(ref)>,
                                     FamilyEducationProposalRef2>) {
          c.field("terms_version", ref.terms_version);
          if (c.status().ok && ref.terms_version < 1)
            return c.reject("InvalidEducationProposalTermsVersion");
        }
        return c.status();
      },
      value);
}

// 原六字段条款；原定义不允许 counter，不能在本codec另造可协商范围。
template <class C> Status terms(C &c, CollateralPersonalTerms2 &value) {
  c.object("template_ref", [&](auto &a) {
    text(a, "template_id", value.template_ref.template_id);
    a.field("version", value.template_ref.version);
    return a.status().ok && value.template_ref.version != 1
               ? a.reject("CollateralTemplateVersion1Required")
               : a.status();
  });
  c.field("terms_version", value.terms_version);
  text(c, "first_actor_id", value.first_actor_id);
  text(c, "second_actor_id", value.second_actor_id);
  c.object("ancestry", [&](auto &a) { return ancestry(a, value.ancestry); });
  exact_order(c, value.field_order,
              {"template_ref", "terms_version", "first_actor_id",
               "second_actor_id", "ancestry", "field_order"});
  return c.status();
}
template <class C> Status proposal(C &c, CollateralPersonalProposal2 &value) {
  fixed_text(c, "kind", value.kind);
  c.object("terms", [&](auto &a) { return terms(a, value.terms); });
  c.object("expression",
           [&](auto &a) { return expression(a, value.expression); });
  c.optional("previous_proposal_ref", value.previous_proposal_ref,
             [](auto &a, auto &x) { return proposal_ref(a, x); });
  c.optional(
      "negotiated_reply_ref", value.negotiated_reply_ref,
      [](auto &a, auto &x) { return care_requirement_codec::source(a, x); });
  c.field("negotiation_round", value.negotiation_round);
  exact_order(c, value.field_order,
              {"kind", "terms", "expression", "previous_proposal_ref",
               "negotiated_reply_ref", "negotiation_round", "field_order"});
  if (!c.status().ok)
    return c.status();
  auto valid = validate_collateral_personal_proposal_relations(value);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
template <class C> Status reply(C &c, CollateralPersonalReply2 &value) {
  fixed_text(c, "kind", value.kind);
  c.object("proposal_ref",
           [&](auto &a) { return proposal_ref(a, value.proposal_ref); });
  care_education_codec::enumeration(c, "decision", value.decision, 3);
  c.object("expression",
           [&](auto &a) { return expression(a, value.expression); });
  c.optional("counter_proposal_ref", value.counter_proposal_ref,
             [](auto &a, auto &x) { return proposal_ref(a, x); });
  exact_order(c, value.field_order,
              {"kind", "proposal_ref", "decision", "expression",
               "counter_proposal_ref", "field_order"});
  if (!c.status().ok)
    return c.status();
  auto valid = validate_collateral_personal_reply_relations(value);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
template <class C> Status acceptance(C &c, CollateralPersonalAccepted2 &value) {
  fixed_text(c, "kind", value.kind);
  c.object("proposal_ref",
           [&](auto &a) { return proposal_ref(a, value.proposal_ref); });
  c.object("expression",
           [&](auto &a) { return expression(a, value.expression); });
  exact_order(c, value.field_order,
              {"kind", "proposal_ref", "expression", "field_order"});
  if (!c.status().ok)
    return c.status();
  auto valid = validate_collateral_personal_acceptance_relations(value);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
} // namespace hanmo::resident::collateral_personal_codec
