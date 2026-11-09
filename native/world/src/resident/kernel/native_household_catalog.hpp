// 原 scenario.dynamic_world 的两个有限 typed 目录入口；不从状态存在推选择。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;
Status append_native_household_modules(const BoundaryValue& scenario,CatalogIdentity&);
Status prepare_native_household_catalogs(const BoundaryValue& scenario,PreparedCatalog&);
}
