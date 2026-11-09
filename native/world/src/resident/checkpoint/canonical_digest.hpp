// 固定作者题面与报价来源的排序摘要共享叶；不创建新存档协议、不读取世界或改变传入值。
#pragma once
#include "ordered_value.hpp"
namespace hanmo::resident {
// 原growth摘要算法的唯一公共入口；沿旧DecodeFailure错误行为，不把失败转为合法空摘要。
StableId boundary_canonical_digest(const BoundaryValue& source);
}
