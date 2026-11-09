// 显式History2配对出口；Common布局由同一已选World传入，不从lot内容猜I/L。
#pragma once
#include "growth_market_history.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
namespace hanmo::resident {
class CodecVisitor;
class ProjectionWriter;
Status
validate_growth_market_history_v2_shape(const GrowthMarketHistoryState &,
                                        const CommonResourceCodecContext &);
Status
visit_growth_market_history_v2_checkpoint(CodecVisitor &,
                                          GrowthMarketHistoryState &,
                                          const CommonResourceCodecContext &);
Status emit_growth_market_history_v2_fields(ProjectionWriter &,
                                            const GrowthMarketHistoryState &,
                                            const CommonResourceCodecContext &);
Outcome<GrowthMarketHistoryState>
decode_growth_market_history_v2(const BoundaryValue &,
                                const CommonResourceCodecContext &,
                                ScopedCheckpointJsonDecode &);
} // namespace hanmo::resident
