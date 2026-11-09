// FH §5.3A 的局部值字段表；只 import 原条件/生命强型，不增另一身体 codec。
#pragma once
#include "care_body_scope.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_requirement_codec_fields.hpp"

namespace hanmo::resident::care_body_scope_codec {
// 原条件引用保 InitialSeed/Receipt 两支与原唯一键，禁止自报可替换 schema。
template <class C> Status condition(C &c, CharacterConditionSourceRef &value) {
  Integer tag = static_cast<Integer>(value.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidCareBodyConditionBasis");
  if (c.reading()) {
    auto prepared =
        tag == 0 ? c.template prepare_default_values<InitialConditionSeedRef>()
                 : c.template prepare_default_values<ConditionReceiptRefV2>();
    if (!prepared.ok)
      return prepared;
    value = tag == 0 ? CharacterConditionSourceRef{InitialConditionSeedRef{}}
                     : CharacterConditionSourceRef{ConditionReceiptRefV2{}};
  }
  if (tag == 0)
    return care_requirement_codec::text(
        c, "seed_key", std::get<InitialConditionSeedRef>(value).seed_key);
  return care_requirement_codec::text(
      c, "result_id", std::get<ConditionReceiptRefV2>(value).result_id);
}

// 同一 line 中具名成员只保存原稳定键；重复成员或多于原身体份额均拒绝。
template <class C> Status residence(C &c, CareResidenceBodyScope &value) {
  care_requirement_codec::text(c, "agreement_id", value.agreement_id);
  care_requirement_codec::text(c, "intake_line_id", value.intake_line_id);
  c.object("actual_scope_source_ref", [&](auto &x) {
    return care_requirement_codec::source(x, value.actual_scope_source_ref);
  });
  c.field("actual_scope_schema", value.actual_scope_schema);
  c.field("body_count", value.body_count);
  c.sequence("known_named_actor_ids", value.known_named_actor_ids,
             [](auto &x, auto &id) {
               return care_requirement_codec::text(x, "value", id);
             });
  c.sequence("unresolved_loss_source_refs", value.unresolved_loss_source_refs,
             [](auto &x, auto &ref) {
               return care_requirement_codec::moment_source(x, ref);
             });
  if (!c.status().ok)
    return c.status();
  if (value.actual_scope_schema < 1 || value.body_count < 1 ||
      value.known_named_actor_ids.size() >
          static_cast<std::uint64_t>(value.body_count))
    return c.reject("InvalidCareResidenceBodyScope");
  for (std::size_t i = 0; i < value.known_named_actor_ids.size(); ++i)
    for (std::size_t prior = 0; prior < i; ++prior)
      if (value.known_named_actor_ids[i] == value.known_named_actor_ids[prior])
        return c.reject("DuplicateCareBodyNamedSubset");
  return c.status();
}

// 严格有限联合；Empty 不带额外 payload，Named 仍保真实生命引用。
template <class C> Status scope(C &c, CareBodyScopeSlice &value) {
  Integer tag = static_cast<Integer>(value.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 2)
    return c.reject("InvalidCareBodyScopeTag");
  if (c.reading()) {
    Status prepared = common_success();
    if (tag == 0) {
      prepared = c.template prepare_default_values<CareEmptyBodyScope>();
      if (prepared.ok)
        value = CareEmptyBodyScope{};
    } else if (tag == 1) {
      prepared = c.template prepare_default_values<CareNamedBodyScope>();
      if (prepared.ok)
        value = CareNamedBodyScope{};
    } else {
      prepared = c.template prepare_default_values<CareResidenceBodyScope>();
      if (prepared.ok)
        value = CareResidenceBodyScope{};
    }
    if (!prepared.ok)
      return prepared;
  }
  if (tag == 0)
    return c.status();
  if (tag == 2)
    return residence(c, std::get<CareResidenceBodyScope>(value));
  auto &named = std::get<CareNamedBodyScope>(value);
  care_requirement_codec::text(c, "actor_id", named.actor_id);
  care_requirement_codec::text(c, "character_origin_actor_key",
                               named.character_origin_actor_key);
  c.object("condition_basis", [&](auto &x) {
    return condition(x, named.condition_basis);
  });
  c.optional("death_ref", named.death_ref, [](auto &x, auto &ref) {
    return care_requirement_codec::death(x, ref);
  });
  if (c.status().ok && named.actor_id != named.character_origin_actor_key)
    return c.reject("CareBodyOriginActorMismatch");
  return c.status();
}
} // namespace hanmo::resident::care_body_scope_codec
