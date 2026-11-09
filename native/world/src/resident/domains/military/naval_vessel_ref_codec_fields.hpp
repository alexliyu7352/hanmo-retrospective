// 原实船键仍是一个字符串标量；名义 C++ 包装不新增 object、tag 或 schema。
#pragma once
#include "naval_vessel_ref.hpp"
#include <string_view>

namespace hanmo::resident::naval_vessel_ref_codec {
// 字段名由原包含记录给定；字节与直接写同名 StableId 字段相同。
// 这里只拒无地址的空键，真正存在性和身份仍必须经原 Naval reader 验证。
template <class C>
Status reference(C &c, std::string_view name, VesselRef &value) {
  c.field(name, value.vessel_id);
  if (!c.status().ok)
    return c.status();
  return value.vessel_id.empty() ? c.reject("NavalVesselReferenceRequired")
                                 : c.status();
}
} // namespace hanmo::resident::naval_vessel_ref_codec
