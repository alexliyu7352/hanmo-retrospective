// 原型别的轻量声明和值合同；只搬位置，不改变布局或规则。
#pragma once
#include "hanmo/resident/model_contract.hpp"

namespace hanmo::resident {
struct RetainerRecord { StableId actor_id,estate_id,source_id,unit_id;  std::vector<StableId> field_order; };

struct OfficeTerms { StableId office_id,actor_id,organization_id,place_id,provider_actor_id;  std::vector<StableId> field_order; };

struct ArmyFormationTerms {
    StableId order_id,office_id,actor_id,garrison_id,ownership_id,place_id,organization_id;
    std::vector<StableId> unit_ids;
    std::vector<std::pair<StableId,StableId>> commanders;
    std::vector<StableId> member_actor_ids;
    std::vector<RetainerRecord> retainers;
    std::vector<std::pair<StableId,Integer>> goods;
    Integer money=0; StableId source_inventory_id,source_wallet_id;
 std::vector<StableId> field_order; };
using MilitaryTerms=std::variant<OfficeTerms,ArmyFormationTerms>;
} // namespace hanmo::resident
