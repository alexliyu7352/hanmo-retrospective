// FH NamedBody 的唯一局部字段表；不修改旧 CareBodyScope 的字节或语义。
#pragma once
#include "care_facility_named_body.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"

namespace hanmo::resident::care_facility_named_body_codec {
// 原 StableId 保有限非空字符串，不接受未命名的来源。
template <class C>
Status identifier(C &archive, const char *name, StableId &value) {
  archive.field(name, value);
  if (archive.status().ok && (value.empty() || value.size() > 256))
    return archive.reject("InvalidCareFacilityBodyIdentifier", name);
  return archive.status();
}

// 新局部 wire 固定 Seed=0、Receipt=1、Death=2，完整保留各原 owner 引用。
template <class C> Status basis(C &archive, CareFacilityBodyBasisRef &value) {
  Integer tag = static_cast<Integer>(value.index());
  archive.field("tag", tag);
  if (!archive.status().ok)
    return archive.status();
  if (tag < 0 || tag > 2)
    return archive.reject("InvalidCareFacilityBodyBasisTag");
  if (archive.reading()) {
    if (tag == 0) {
      auto prepared =
          archive.template prepare_default_values<InitialConditionSeedRef>();
      if (!prepared.ok)
        return prepared;
      value = InitialConditionSeedRef{};
    } else if (tag == 1) {
      auto prepared =
          archive.template prepare_default_values<ConditionReceiptRefV2>();
      if (!prepared.ok)
        return prepared;
      value = ConditionReceiptRefV2{};
    } else {
      auto prepared = archive.template prepare_default_values<DeathRef>();
      if (!prepared.ok)
        return prepared;
      value = DeathRef{};
    }
  }
  if (tag == 0)
    return identifier(archive, "seed_key",
                      std::get<InitialConditionSeedRef>(value).seed_key);
  if (tag == 1)
    return identifier(archive, "result_id",
                      std::get<ConditionReceiptRefV2>(value).result_id);
  auto &death = std::get<DeathRef>(value);
  identifier(archive, "domain", death.domain);
  identifier(archive, "kind", death.kind);
  identifier(archive, "id", death.id);
  if (archive.status().ok &&
      (death.domain != "family_life" || death.kind != "family_death"))
    return archive.reject("InvalidCareFacilityBodyDeathRef");
  return archive.status();
}

// 只核三字段形状；外置 schema 保原值，不借 death cause 的版本替代。
template <class C> Status named_body(C &archive, CareFacilityNamedBody &value) {
  identifier(archive, "actor_id", value.actor_id);
  archive.object("body_basis_ref", [&](auto &child) {
    return basis(child, value.body_basis_ref);
  });
  archive.field("body_basis_schema", value.body_basis_schema);
  if (archive.status().ok && value.body_basis_schema < 1)
    return archive.reject("InvalidCareFacilityBodyBasisSchema");
  return archive.status();
}
} // namespace hanmo::resident::care_facility_named_body_codec
