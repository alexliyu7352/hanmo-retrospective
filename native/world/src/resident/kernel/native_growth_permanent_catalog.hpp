// 永久成长沿唯一原作者目录安装；不从当前状态存在位反选版本。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Status prepare_native_growth_permanent_catalog(const BoundaryValue& data,const BoundaryValue& scenario,PreparedCatalog&);
Status validate_native_growth_permanent_catalog(const PreparedCatalog&);
}
