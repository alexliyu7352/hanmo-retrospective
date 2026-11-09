// 首次 foundation_assets/1 候选的单字段表入口；尚未接 World/registry。
#pragma once
#include "foundation_asset_sources.hpp"
namespace hanmo::resident {
class CodecVisitor;
class ProjectionWriter;
struct CommonResourceCodecContext;
Outcome<FoundationAssetSourcesState>
decode_foundation_asset_sources(const BoundaryValue &, bool typed,
                                const CommonResourceCodecContext &);
Outcome<BoundaryValue>
encode_foundation_asset_sources(const FoundationAssetSourcesState &, bool typed,
                                const CommonResourceCodecContext &);
Status emit_foundation_asset_sources_fields(ProjectionWriter &,
                                            const FoundationAssetSourcesState &,
                                            bool typed,
                                            const CommonResourceCodecContext &);
Status visit_foundation_asset_sources(CodecVisitor &,
                                      FoundationAssetSourcesState &, bool typed,
                                      const CommonResourceCodecContext &);
} // namespace hanmo::resident
