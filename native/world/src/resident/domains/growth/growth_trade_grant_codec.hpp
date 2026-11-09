// trade-grant/1独立段的原成对codec；不向旧Growth正文增加尾字段。
#pragma once
#include "growth_trade_grant.hpp"
#include <string_view>
namespace hanmo::resident {
class CodecVisitor;
class ProjectionWriter;
struct BoundaryValue;
Status visit_growth_trade_grants_v1(CodecVisitor &,
                                    OrderedTable<GrowthTradeGrant> &);
Status emit_growth_trade_grants_v1_projection(
    ProjectionWriter &, std::string_view,
    const OrderedTable<GrowthTradeGrant> &);
Outcome<BoundaryValue>
encode_growth_trade_grants_v1(const OrderedTable<GrowthTradeGrant> &);
Outcome<OrderedTable<GrowthTradeGrant>>
decode_growth_trade_grants_v1(const BoundaryValue &);
} // namespace hanmo::resident
