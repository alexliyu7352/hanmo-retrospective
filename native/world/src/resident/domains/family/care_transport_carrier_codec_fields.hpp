// Care 原有限承运对象字段表；嵌套原引用委托真正 S1/S2 codec，不重定义 wire。
#pragma once
#include "care_transport_carrier.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/military/naval_vessel_ref_codec_fields.hpp"
#include "resident/domains/travel/spatial_carrier_ref_codec_fields.hpp"

namespace hanmo::resident::care_transport_carrier_codec {
// 读之前对真正分支做默认预留；writer 不分配新分支或改变借用值。
template <std::size_t Index, class C>
Status select(C &c, CareTransportCarrier &value) {
  using Branch = std::variant_alternative_t<Index, CareTransportCarrier>;
  const auto ready = c.template prepare_default_values<Branch>();
  if (!ready.ok)
    return ready;
  value.template emplace<Index>();
  return c.status();
}

// 全部原字段和固定 tag 直接保存；格式有效不代替来源、权限或装载验证。
template <class C> Status carrier(C &c, CareTransportCarrier &value) {
  Integer tag = static_cast<Integer>(value.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidCareTransportCarrierTag");
  if (c.reading()) {
    const auto ready = tag == 0 ? select<0>(c, value) : select<1>(c, value);
    if (!ready.ok)
      return ready;
  }
  if (tag == 1)
    return naval_vessel_ref_codec::reference(
        c, "vessel_ref", std::get<CareActualVesselCarrier>(value).vessel_ref);
  auto &ground = std::get<CareGroundTransportCarrier>(value);
  c.object("transport_unit_ref", [&](auto &nested) {
    return spatial_carrier_ref_codec_fields::transport_unit_fields(
        nested, ground.transport_unit_ref);
  });
  c.field("expected_composition_revision",
          ground.expected_composition_revision);
  if (!c.status().ok)
    return c.status();
  if (ground.transport_unit_ref.subject.id.empty() ||
      ground.transport_unit_ref.transport_unit_id.empty() ||
      ground.expected_composition_revision < 0)
    return c.reject("InvalidCareGroundTransportCarrier");
  return c.status();
}
} // namespace hanmo::resident::care_transport_carrier_codec
