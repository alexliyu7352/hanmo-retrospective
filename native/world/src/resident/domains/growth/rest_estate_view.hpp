// 原型别的轻量声明和值合同；只搬位置，不改变布局或规则。
#pragma once
#include "hanmo/resident/model_contract.hpp"

namespace hanmo::resident {
struct RestEstateView {
  StableId owner_actor_id, place_id, inventory_id, food_item_id;
  Integer ration_per_person = 0;
  std::vector<StableId> resident_actor_ids;
};
} // namespace hanmo::resident
