// FH 原冻结两层字段的唯一 typed codec；形状成功不证明原 episode
// 或恢复来源可用。
#pragma once
#include "injury_effect_codec_fields.hpp"

namespace hanmo::resident::frozen_condition_codec {
// 复用原 EffectContribution 的强型引用与状态，不以通用 SourceRef 偷换 schema。
template <class C> Status effect(C &c, FrozenConditionEffectBinding &value) {
  c.object("origin_result", [&](auto &v) {
    return injury_effect_codec::origin(v, value.origin_result);
  });
  c.field("effect_key", value.effect_key);
  injury_effect_codec::state(c, "current_state", value.current_state);
  c.optional("last_resolution_ref", value.last_resolution_ref,
             [](auto &v, auto &ref) {
               return injury_effect_codec::recovery_ref(v, ref);
             });
  character_origin_codec::strings(c, "field_order", value.field_order);
  if (!c.status().ok)
    return c.status();
  return value.effect_key.empty() ? c.reject("InjuryEffectKeyRequired")
                                  : c.status();
}
// 原 episode 的 Seed／Receipt 联合顺序不变；借原头与时刻的业务反读不在此叶。
template <class C> Status input(C &c, FrozenActiveConditionInput &value) {
  c.field("episode_id", value.episode_id);
  c.object("condition_rule_ref", [&](auto &v) {
    return character_origin_codec::rule(v, value.condition_rule_ref);
  });
  c.object("stage_ref", [&](auto &v) {
    return character_origin_codec::rule(v, value.stage_ref);
  });
  c.field("stage_revision", value.stage_revision);
  c.object("origin", [&](auto &v) {
    Integer tag = static_cast<Integer>(value.origin.index());
    v.field("tag", tag);
    if (!v.status().ok)
      return v.status();
    if (tag < 0 || tag > 1)
      return v.reject("InvalidConditionEpisodeOrigin");
    if (v.reading())
      value.origin =
          tag == 0 ? CharacterConditionSourceRef{InitialConditionSeedRef{}}
                   : CharacterConditionSourceRef{ConditionReceiptRefV2{}};
    if (tag == 0)
      return character_origin_codec::seed(
          v, std::get<InitialConditionSeedRef>(value.origin));
    return injury_effect_codec::receipt_ref(
        v, std::get<ConditionReceiptRefV2>(value.origin));
  });
  c.sequence("effect_bindings", value.effect_bindings,
             [](auto &v, auto &row) { return effect(v, row); });
  character_origin_codec::strings(c, "field_order", value.field_order);
  if (!c.status().ok)
    return c.status();
  return value.episode_id.empty() || value.stage_revision < 0
             ? c.reject("InvalidConditionEpisode")
             : c.status();
}
} // namespace hanmo::resident::frozen_condition_codec
