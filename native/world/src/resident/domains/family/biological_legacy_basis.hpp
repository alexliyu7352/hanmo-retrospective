// 原已记录亲代和真实出生提供关系依据；不写血缘、不授本人许可、财产、知情或控制。
#pragma once
#include "actor_age_birth_read.hpp"
#include "actor_death_read.hpp"
#include "actor_presence_read.hpp"
#include "collateral_family_origin_values.hpp"
#include "family_birth_lineage.hpp"
#include "family_life_exact.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
class FamilyContinuationPeopleRead4;
struct BiologicalChildOrigin {
  WorldIdentity source_identity;
  StableId parent_actor_id, child_actor_id, before_fact_id;
  BiologicalChildOriginKind kind = BiologicalChildOriginKind::installed_lineage;
  StableId original_catalog_actor_id, birth_fact_id, creation_fact_id;
  Integer observed_tick = 0, source_available_tick = 0;
};
// 所有借用为同一已完整核Common/Creation/Birth/Lineage/Life/军史的候选；不递归World。
struct BiologicalLegacyBasisContext {
  const CommonStore &common;
  const CommonCatalog &catalog;
  const FamilyLineageCatalog &lineage;
  const FamilyBirthState *births = nullptr;
  const FamilyDeathState *life = nullptr;
  FamilyDeathOriginRead death_origin;
  const FamilyContinuationPeopleRead4 *complete_people = nullptr;
};
// 空值是未有原亲代来源，不是证明没有血缘。只认直接亲代，不把祖先/同姓/收养当亲生子女。
Outcome<std::optional<BiologicalChildOrigin>>
read_biological_child_origin(const BiologicalLegacyBasisContext &,
                             const StableId &parent, const StableId &child);
// 消费方先核自己的实际payload.tick；同tick按真实Common顺序，后出生不能倒填旧关系。
Outcome<std::optional<BiologicalChildOrigin>>
read_biological_child_origin_before_fact(const BiologicalLegacyBasisContext &,
                                         const StableId &parent,
                                         const StableId &child, Integer tick,
                                         const StableId &actual_before_fact);
struct BiologicalLegacyBasis {
  BiologicalChildOrigin relation;
  StableId death_fact_id, casualty_fact_id, before_fact_id;
  Integer death_tick = 0;
  std::optional<Integer> successor_birth_tick;
  Integer observed_tick = 0;
  std::optional<VerifiedDeathView> verified_death;
};
// 当前原死亡已经正式登记，亲生后代已存在于死因前、成年且在世；仍不代表本人同意或可玩处境。
Outcome<BiologicalLegacyBasis>
read_biological_legacy_basis(const BiologicalLegacyBasisContext &,
                             const StableId &predecessor,
                             const StableId &successor);
// 冷资格固定原实际激活事实前界；后来的死亡不能抹掉先前合法的在世资格。
Outcome<BiologicalLegacyBasis> read_biological_legacy_basis_before_fact(
    const BiologicalLegacyBasisContext &, const StableId &predecessor,
    const StableId &successor, Integer tick,
    const StableId &actual_before_fact);
Outcome<BiologicalLegacyBasis> read_biological_legacy_basis_at_original_prefix(
    const BiologicalLegacyBasisContext &, const StableId &predecessor,
    const StableId &successor, Integer tick, const CommonActualFactPrefix &);

} // namespace hanmo::resident
