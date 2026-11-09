// 原旁支祖系命令值的纯持值闭包；不包含谱系reader或另一份血缘账。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
// Installed目录来源不是Common事件，不塞入名字为fact_id的字段冒充运行事实。
enum class BiologicalChildOriginKind { installed_lineage, recorded_birth };
struct BiologicalControlOrigin {
  BiologicalChildOriginKind kind = BiologicalChildOriginKind::installed_lineage;
  StableId parent_actor_id, child_actor_id, original_catalog_actor_id,
      birth_fact_id, creation_fact_id;
  Integer source_available_tick = 0;
  FieldOrder field_order;
};
struct CollateralFamilyOrigin {
  StableId first_actor_id, second_actor_id, common_ancestor_actor_id;
  std::vector<BiologicalControlOrigin> first_descent, second_descent;
  FieldOrder field_order;
};
} // namespace hanmo::resident
