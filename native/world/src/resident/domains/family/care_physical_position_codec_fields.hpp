// 单位字段委托经济原表；Family 只串接本地有限占位联合，不重建 S5 设施模型。
#pragma once
#include "care_clearance_origin_codec_fields.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_physical_position.hpp"
#include "resident/domains/local_economy/economic_facility_reference_fields.hpp"

namespace hanmo::resident::care_physical_position_codec {
// 适配原设施 reference 字段回调到同一个 checked archive；不序列化另一份设施。
template <class C> class PhysicalReferenceArchive {
  C &archive_;

public:
  // 借用当前原 visitor，不持 budget、来源状态或独立 selector。
  explicit PhysicalReferenceArchive(C &archive) : archive_(archive) {
  }
  // 标识符严格非空；真实设施存在性仍由原 owner 反读。
  void operator()(const char *name, StableId &value) {
    care_requirement_codec::text(archive_, name, value);
  }
  // 本原安装和激活 reference 只有已登记 schema1，不接未知整数回退。
  void operator()(const char *name, std::uint32_t &value) {
    Integer schema = value;
    archive_.field(name, schema);
    if (!archive_.status().ok)
      return;
    if (schema != 1) {
      archive_.reject("OriginalPhysicalUnitSchema1Required");
      return;
    }
    if (archive_.reading())
      value = 1;
  }
  // 原 Common source 三元地址保持原字段，不复制实际安装 payload。
  void operator()(const char *name, SourceRef &value) {
    archive_.object(name, [&](auto &inner) {
      return care_requirement_codec::source(inner, value);
    });
  }
  // 原 Installed/Activated 两支使用经济原 record 回调及原 tag 0/1。
  void operator()(const char *name, PhysicalUnitInstallationSource &value) {
    archive_.object(name, [&](auto &inner) {
      Integer tag = static_cast<Integer>(value.index());
      inner.field("tag", tag);
      if (!inner.status().ok)
        return inner.status();
      if (tag < 0 || tag > 1)
        return inner.reject("InvalidOriginalPhysicalInstallationTag");
      if (inner.reading()) {
        auto ready = tag == 0
                         ? care_clearance_origin_codec::select<0>(inner, value)
                         : care_clearance_origin_codec::select<1>(inner, value);
        if (!ready.ok)
          return ready;
      }
      PhysicalReferenceArchive<std::decay_t<decltype(inner)>> fields(inner);
      std::visit(
          [&](auto &payload) {
            facility_codec::record(fields, payload);
          },
          value);
      return inner.status();
    });
  }
  // 其余实际引用节点只复用经济固定字段，防止 Family 写出同名不同布局。
  template <class T> void operator()(const char *name, T &value) {
    archive_.object(name, [&](auto &inner) {
      PhysicalReferenceArchive<std::decay_t<decltype(inner)>> fields(inner);
      facility_codec::record(fields, value);
      return inner.status();
    });
  }
};

// 每个单位只有原 facility/install/unit 三字段；不是一张新的设施记录。
template <class C> Status unit(C &c, FacilityPhysicalUnitRef &value) {
  PhysicalReferenceArchive<C> fields(c);
  facility_codec::record(fields, value);
  return c.status();
}

// 完整原单位身份包括安装来源；不把两份历史安装的同名 unit 混作一份引用。
inline bool same_unit(const FacilityPhysicalUnitRef &a,
                      const FacilityPhysicalUnitRef &b) {
  if (a.facility_id != b.facility_id || a.unit_key != b.unit_key ||
      a.installation_source_ref.index() != b.installation_source_ref.index())
    return false;
  return std::visit(
      [&](const auto &left) {
        using T = std::decay_t<decltype(left)>;
        const auto &right = std::get<T>(b.installation_source_ref);
        if constexpr (std::is_same_v<T, InstalledPhysicalUnitSource>)
          return left.source_schema == right.source_schema &&
                 left.source_ref.domain == right.source_ref.domain &&
                 left.source_ref.kind == right.source_ref.kind &&
                 left.source_ref.id == right.source_ref.id;
        else
          return left.original_facility_activation_receipt_ref.receipt_id ==
                     right.original_facility_activation_receipt_ref
                         .receipt_id &&
                 left.original_facility_activation_receipt_ref.source_schema ==
                     right.original_facility_activation_receipt_ref
                         .source_schema;
      },
      a.installation_source_ref);
}

// 数组保原稳定序；重复单位身份拒绝，但不推断今日是否空闲或可用。
template <class C>
Status units(C &c, const char *name,
             std::vector<FacilityPhysicalUnitRef> &values) {
  c.sequence(name, values, [](auto &inner, auto &value) {
    return unit(inner, value);
  });
  if (!c.status().ok)
    return c.status();
  for (std::size_t i = 0; i < values.size(); ++i)
    for (std::size_t prior = 0; prior < i; ++prior)
      if (same_unit(values[i], values[prior]))
        return c.reject("DuplicateCarePhysicalUnit");
  return c.status();
}

// 只检查有限结构与原引用；是否允许空集合取决于真实事件种类，不在此假判清空。
template <class C> Status position(C &c, CarePhysicalPositionBasis &value) {
  Integer tag = static_cast<Integer>(value.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidCarePhysicalPositionTag");
  if (c.reading()) {
    const auto ready = tag == 0
                           ? care_clearance_origin_codec::select<0>(c, value)
                           : care_clearance_origin_codec::select<1>(c, value);
    if (!ready.ok)
      return ready;
  }
  if (tag == 0) {
    auto &v = std::get<CareAggregatePhysicalPosition>(value);
    care_requirement_codec::text(c, "placement_claim_id", v.placement_claim_id);
    units(c, "affected_unit_refs", v.affected_unit_refs);
    return c.object("position_rule_ref", [&](auto &inner) {
      return care_requirement_codec::rule(inner, v.position_rule_ref);
    });
  }
  auto &v = std::get<CareExactPhysicalPosition>(value);
  care_requirement_codec::text(c, "placement_claim_id", v.placement_claim_id);
  units(c, "occupied_unit_refs", v.occupied_unit_refs);
  return care_arrangement_codec::moment_refs(c, "position_source_refs",
                                             v.position_source_refs);
}
} // namespace hanmo::resident::care_physical_position_codec
