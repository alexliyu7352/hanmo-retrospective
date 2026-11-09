// 商铺资格只聚合权威只读来源；缺法律证明或旧客户/商术历史不能被空默认值放行。
#pragma once
#include "private_shop_contract.hpp"
#include "resident/domains/family/actor_age_read.hpp"
#include "resident/domains/family/family_model.hpp"
#include "resident/domains/tasks/task_fulfilled_clients.hpp"
namespace hanmo::resident {
struct GrowthState;
struct GrowthSkillOriginContext;
struct PrivateShopContext;
struct PrivateShopOpeningBoundary {
  StableId opening_fact_id;
  Integer opening_tick = 0;
};
// The Task owner has already re-read the actual completion. This consumer gate
// verifies its exact/legacy shape and the original Common cut; it never turns
// completed_tick into an exact moment or changes the persisted shop layout.
Status validate_private_shop_client_completion_source(
    const CommonStore &, const TaskFulfilledClientSource &,
    Integer opening_tick, const PrivateShopOpeningBoundary *before = nullptr);
// 完整Family/Creation/Birth/Lineage先验后，只借同候选身份来源核店主、出租人、雇员或原客户年龄。
// 未借到真实Birth账的出生人物硬拒；原作者和Family创建仍复用原年龄规则，不另算生日。
Outcome<ActorAgeRead> read_private_shop_party_age(
    const CommonStore &, const CommonCatalog &, const FamilyCatalog &,
    const GrowthSkillOriginContext *, const StableId &actor);
// 调用者须先核实际开设、雇约接受或订单签发payload的actor/tick；本口再限具名原头，不授未来phase资格。
Outcome<ActorAgeRead> read_private_shop_party_age_before_fact(
    const CommonStore &, const CommonCatalog &, const FamilyCatalog &,
    const GrowthSkillOriginContext *, const StableId &actor, Integer tick,
    const StableId &actual_fact);

struct PrivateShopEligibility {
  WorldIdentity source_identity;
  StableId actor_id, premises_id;
  Integer observed_tick = 0, minimum_commerce_xp = 0,
          minimum_commerce_level = 0;
  std::vector<StableId> commerce_study_source_ids;
  std::vector<TaskFulfilledClientSource> original_clients;
  PrivateShopPermissionValue permission;
};
// 历史调用须先将本域实际opening
// payload的actor/tick与参数逐字核对；当前口不造未来事实头。
Outcome<PrivateShopEligibility> read_private_shop_opening_eligibility(
    const CommonStore &, const CommonCatalog &, const FamilyCatalog &,
    const TaskFulfilledClientRead &, const PrivateShopPermissionRead *,
    const PrivateShopPremises &, const StableId &actor,
    const PrivateShopOpeningBoundary *before = nullptr,
    const GrowthState *study_history = nullptr,
    const GrowthSkillOriginContext *skill_origins = nullptr,
    const PrivateShopContext *selected_context = nullptr);
} // namespace hanmo::resident
