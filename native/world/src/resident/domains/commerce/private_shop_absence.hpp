// 未选S7的真实世界否定门；不构造空私铺目录、法律口或经营来源。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
Status validate_unselected_private_shop_sources(const WorldBundle &);
} // namespace hanmo::resident
