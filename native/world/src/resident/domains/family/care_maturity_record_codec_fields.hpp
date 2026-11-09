// Care 原成年记录的唯一字段序；JSON、binary 与 shape 共用，不另设迁移布局。
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_maturity_record.hpp"
#include "care_requirement_codec_fields.hpp"

namespace hanmo::resident::care_maturity_codec {
// 原阶段仅允许四个固定 tag；实际阶段合法性由原 AgeRead 与边界规则验证。
template <class C> Status stage(C &c, const char *name, CareAgeStage &value) {
  Integer tag = static_cast<Integer>(value);
  c.field(name, tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 3)
    return c.reject("InvalidCareAgeStage");
  if (c.reading())
    value = static_cast<CareAgeStage>(tag);
  return c.status();
}

// 完整保存 v1b 九字段；整数日历边界不借当前年龄补出，也不推定 source schema。
template <class C> Status record(C &c, MaturityResult &value) {
  care_requirement_codec::text(c, "result_id", value.result_id);
  care_requirement_codec::text(c, "actor_id", value.actor_id);
  c.object("age_basis_ref", [&](auto &x) {
    return life_age_basis_fields(x, value.age_basis_ref);
  });
  c.object("calendar_profile_ref", [&](auto &x) {
    return care_requirement_codec::rule(x, value.calendar_profile_ref);
  });
  c.field("calendar_boundary_ordinal", value.calendar_boundary_ordinal);
  c.field("effective_tick", value.effective_tick);
  c.object("original_condition_or_life_boundary_ref", [&](auto &x) {
    return care_requirement_codec::moment_source(
        x, value.original_condition_or_life_boundary_ref);
  });
  stage(c, "previous_stage", value.previous_stage);
  stage(c, "new_stage", value.new_stage);
  if (!c.status().ok)
    return c.status();
  if (value.calendar_boundary_ordinal < 0 || value.effective_tick < 0)
    return c.reject("InvalidCareMaturityCalendarBoundary");
  return c.status();
}
} // namespace hanmo::resident::care_maturity_codec
