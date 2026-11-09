// 私有wire解码与类型化库存；只读profile拒绝不能无损表示的值。
#pragma once
#include "hanmo/inventory_projection.hpp"
#include <set>
#include <variant>

namespace hanmo::detail {
// StableId仅作精确字节身份，不借Flecs名称解析层级。
struct StableId { std::string value; };
// 安装前全部解析完成；无旧注册Callable或引擎对象进入原生。
struct Input {
    std::string input_sha256,payload_sha256;
    std::vector<InventoryState> inventories;
};
// 查询owner保完整值；不存在匹配ID仍是原count的零结果。
struct Query {
    std::string id,inventory_id,item_id,reservation;
    std::vector<Value> owners;
};
// 解析结果均值拥有，出错不返回部分组件。
Result<Input> decode_artifact(const std::string& bytes);
Result<std::vector<Query>> decode_queries(const std::string& bytes,const std::string& input_sha256);
// 从实际typed组件重新编码审计摘要，证明owner／lot扩展键没有在安装时丢失。
std::string component_digest(const InventoryState& inventory);
} // 私有解码命名空间。
