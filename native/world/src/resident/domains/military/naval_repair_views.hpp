// 工单页只读本人实际舰队，材料候选限该舰队公有容器；不能列全世界库存或敌船。
#pragma once
#include "naval_repair.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> naval_repair_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const StableId& actor_id);
}
