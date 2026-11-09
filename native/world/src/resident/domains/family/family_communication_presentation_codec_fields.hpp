// LIFE v1f §2/§6 原公共字段的唯一遍历；正文外的 source/锁验证不在此伪造。
#pragma once
#include "family_communication_presentation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include <initializer_list>
namespace hanmo::resident::family_presentation_codec {
template <class C> Status text(C &c, const char *key, StableId &value) {
  c.field(key, value);
  if (c.status().ok && (value.empty() || value.size() > 256))
    return c.reject("InvalidFamilyPresentationIdentifier", key);
  return c.status();
}
template <class C> Status source(C &c, SourceRef &value) {
  text(c, "domain", value.domain);
  text(c, "kind", value.kind);
  return text(c, "id", value.id);
}
template <class C>
Status order(C &c, FieldOrder &value,
             std::initializer_list<std::string_view> expected) {
  c.sequence("field_order", value, [](auto &child, auto &name) {
    return text(child, "value", name);
  });
  if (!c.status().ok || value.empty())
    return c.status();
  if (value.size() != expected.size())
    return c.reject("FamilyPresentationFieldOrderMismatch");
  // 原 field_order 可保输入顺序，但必须恰好覆盖此有限正文，不能塞隐藏字段。
  for (std::size_t n = 0; n < value.size(); ++n) {
    if (std::find(expected.begin(), expected.end(), value[n]) == expected.end())
      return c.reject("FamilyPresentationFieldOrderMismatch", value[n]);
    for (std::size_t p = 0; p < n; ++p)
      if (value[n] == value[p])
        return c.reject("FamilyPresentationDuplicateField", value[n]);
  }
  return c.status();
}
template <class C> Status definition(C &c, ChallengeDefinitionRef &v) {
  text(c, "challenge_id", v.challenge_id);
  c.field("definition_version", v.definition_version);
  if (c.status().ok && v.definition_version < 1)
    return c.reject("FamilyChallengeDefinitionVersionRequired");
  return c.status();
}
template <class C> Status result(C &c, ChallengeResultRef &v) {
  text(c, "owner_domain", v.owner_domain);
  c.object("source_activity_ref",
           [&](auto &x) { return source(x, v.source_activity_ref); });
  text(c, "challenge_id", v.challenge_id);
  text(c, "instance_id", v.instance_id);
  c.field("result_ordinal", v.result_ordinal);
  c.field("definition_version", v.definition_version);
  if (c.status().ok && (v.result_ordinal < 1 || v.definition_version < 1))
    return c.reject("InvalidChallengeResultReference");
  return c.status();
}
template <class C> Status argument(C &c, FamilyEtiquetteArgumentRef1 &v) {
  text(c, "node_id", v.node_id);
  text(c, "option_id", v.option_id);
  c.field("definition_version", v.definition_version);
  order(c, v.field_order,
        {"node_id", "option_id", "definition_version", "field_order"});
  if (c.status().ok && v.definition_version < 1)
    return c.reject("FamilyChallengeDefinitionVersionRequired");
  return c.status();
}
inline bool marriage_proposal(const SourceRef &v) {
  return v.domain == "family_marriage" &&
         v.kind == "family_marriage_proposed" && !v.id.empty();
}
template <class C> Status etiquette(C &c, FamilyEtiquettePresentation1 &v) {
  c.object("result_ref", [&](auto &x) { return result(x, v.result_ref); });
  c.object("definition_ref",
           [&](auto &x) { return definition(x, v.definition_ref); });
  text(c, "performer_actor_id", v.performer_actor_id);
  text(c, "recipient_actor_id", v.recipient_actor_id);
  c.object("previous_proposal_source_ref",
           [&](auto &x) { return source(x, v.previous_proposal_source_ref); });
  c.object("counter_proposal_source_ref",
           [&](auto &x) { return source(x, v.counter_proposal_source_ref); });
  StableId completion;
  if (!c.reading()) {
    switch (v.completion) {
    case FamilyEtiquettePublicCompletion::automatic:
      completion = "automatic";
      break;
    case FamilyEtiquettePublicCompletion::manual:
      completion = "manual";
      break;
    case FamilyEtiquettePublicCompletion::exited_to_automatic:
      completion = "exited_to_automatic";
      break;
    default:
      return c.reject("InvalidFamilyEtiquettePublicCompletion");
    }
  }
  c.field("completion", completion);
  if (!c.status().ok)
    return c.status();
  if (c.reading()) {
    if (completion == "automatic")
      v.completion = FamilyEtiquettePublicCompletion::automatic;
    else if (completion == "manual")
      v.completion = FamilyEtiquettePublicCompletion::manual;
    else if (completion == "exited_to_automatic")
      v.completion = FamilyEtiquettePublicCompletion::exited_to_automatic;
    else
      return c.reject("InvalidFamilyEtiquettePublicCompletion");
  }
  c.sequence("confirmed_argument_refs", v.confirmed_argument_refs,
             [](auto &x, auto &a) { return argument(x, a); });
  order(c, v.field_order,
        {"result_ref", "definition_ref", "performer_actor_id",
         "recipient_actor_id", "previous_proposal_source_ref",
         "counter_proposal_source_ref", "completion", "confirmed_argument_refs",
         "field_order"});
  if (!c.status().ok)
    return c.status();
  const auto &r = v.result_ref;
  if (r.owner_domain != "family_marriage" ||
      r.source_activity_ref.domain != "family_marriage" ||
      r.source_activity_ref.kind != "family_etiquette_started" ||
      r.source_activity_ref.id != r.instance_id || r.result_ordinal != 1 ||
      r.challenge_id != v.definition_ref.challenge_id ||
      r.definition_version != v.definition_ref.definition_version ||
      v.performer_actor_id == v.recipient_actor_id ||
      !marriage_proposal(v.previous_proposal_source_ref) ||
      !marriage_proposal(v.counter_proposal_source_ref) ||
      v.previous_proposal_source_ref.id == v.counter_proposal_source_ref.id)
    return c.reject("FamilyEtiquettePresentationReferenceMismatch");
  const auto n = v.confirmed_argument_refs.size();
  // 第三次确认立即完成 manual，只有不足三题的 active manual 能实际退出。
  if (n > 3 ||
      (v.completion == FamilyEtiquettePublicCompletion::automatic && n != 0) ||
      (v.completion == FamilyEtiquettePublicCompletion::manual && n != 3) ||
      (v.completion == FamilyEtiquettePublicCompletion::exited_to_automatic &&
       n >= 3))
    return c.reject("FamilyEtiquettePresentationArgumentCountMismatch");
  for (std::size_t i = 0; i < n; ++i) {
    if (v.confirmed_argument_refs[i].definition_version != r.definition_version)
      return c.reject("FamilyEtiquetteArgumentDefinitionMismatch");
    for (std::size_t p = 0; p < i; ++p)
      if (v.confirmed_argument_refs[p].node_id ==
          v.confirmed_argument_refs[i].node_id)
        return c.reject("DuplicateFamilyEtiquetteArgumentNode");
  }
  return c.status();
}
template <class C> Status request(C &c, MarriageRequestPresentationView1 &v) {
  c.object("proposal_source_ref",
           [&](auto &x) { return source(x, v.proposal_source_ref); });
  c.field("source_schema", v.source_schema);
  c.field("presentation_schema", v.presentation_schema);
  text(c, "request_binding_ref", v.request_binding_ref);
  text(c, "requester_actor_id", v.requester_actor_id);
  text(c, "receiver_actor_id", v.receiver_actor_id);
  c.object("formed_at",
           [&](auto &x) { return exact_fields::moment(x, v.formed_at); });
  order(c, v.field_order,
        {"proposal_source_ref", "source_schema", "presentation_schema",
         "request_binding_ref", "requester_actor_id", "receiver_actor_id",
         "formed_at", "field_order"});
  if (c.status().ok &&
      (!marriage_proposal(v.proposal_source_ref) || v.source_schema != 2 ||
       v.presentation_schema != 1 ||
       v.request_binding_ref != "marriage_requester_rites_charisma_v1" ||
       v.requester_actor_id == v.receiver_actor_id))
    return c.reject("InvalidMarriageRequestPresentationView1");
  return c.status();
}
} // namespace hanmo::resident::family_presentation_codec
