// 同候选薄接头只把S4永久XP转为围城具名读；冷早军籍与晚人物来源是两个不可省略的门。
#pragma once
#include "resident/domains/military/military_siege_engineering_access.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
// 返回别名shared_ptr共同持有技能桥和域access，借原World而不保存临时context或可选成员地址。
std::shared_ptr<MilitarySiegeEngineeringAccess> make_military_siege_engineering_access(WorldBundle&,const DynamicStepInput* phase=nullptr);
Status validate_military_siege_engineering_world_structure(const WorldBundle&);
Status validate_military_siege_engineering_world_actor_sources(const WorldBundle&);
}
