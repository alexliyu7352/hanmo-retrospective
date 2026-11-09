// 永久市场来源的独立冷段；调用方只在显式growth_market_history/1时读写。
#pragma once
#include "growth_market_history.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
struct CodecVisitor;
Outcome<GrowthMarketHistoryState> decode_growth_market_history(const BoundaryValue&);
class ProjectionWriter;
Status emit_growth_market_history_fields(ProjectionWriter&,const GrowthMarketHistoryState&);
BoundaryValue encode_growth_market_history(const GrowthMarketHistoryState&);
Status visit_growth_market_history_checkpoint(CodecVisitor&,GrowthMarketHistoryState&);
// 只核本段形状；完整原因/资产校验仍必须经validate_growth_market_history。
Status validate_growth_market_history_shape(const GrowthMarketHistoryState&);
}
