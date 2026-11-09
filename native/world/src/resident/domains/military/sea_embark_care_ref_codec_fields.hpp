// S2 原强型两字段 codec；只验引用形状，不生成登船执行或照护绑定。
#pragma once
#include "sea_embark_care_ref.hpp"

namespace hanmo::resident::sea_embark_care_ref_codec {
// 完整保留原 action 与 binding 两部分；不得把任一字符串充作 Common 事实头。
template <class C> Status reference(C &c, SeaEmbarkCareBindingRef &value) {
  c.field("transfer_action_id", value.transfer_action_id);
  c.field("binding_id", value.binding_id);
  if (!c.status().ok)
    return c.status();
  return value.transfer_action_id.empty() || value.binding_id.empty()
             ? c.reject("SeaEmbarkCareBindingReferenceRequired")
             : c.status();
}
} // namespace hanmo::resident::sea_embark_care_ref_codec
