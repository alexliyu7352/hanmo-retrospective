// 原库存解码、原序重组与count算法只保一份；profile只决定外层扩展字段策略。
#pragma once
#include "hanmo/projection_types.hpp"
#include <set>
namespace hanmo::detail {
std::vector<InventoryState> decode_inventories(const Value& source,const std::map<std::string,std::set<std::string>>& references,const std::set<std::string>& actions,const std::map<std::string,std::int64_t>& items,bool preserve_extras);
Result<std::int64_t> inventory_count(const InventoryState& inventory,const std::string& item_id,const std::vector<Value>& owners,const std::string& reservation,const std::string& path);
Value inventory_record(const InventoryState& inventory);
std::string component_digest(const InventoryState& inventory);
} // 所有者与保留值比较不弱化。
