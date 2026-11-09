// 原political_continuity/1九键的私有候选编解码，不声明其完整来源已可发布。
#pragma once
#include "political_continuity_contract.hpp"
namespace hanmo::resident {
class CodecVisitor;
class ProjectionWriter;
// 仅物理结构门；所有实际任期、同意、获知、计票/流亡及legacy反读仍须原全验。
Status validate_political_continuity_shape(const PoliticalContinuityState &);
Status visit_political_continuity(CodecVisitor &, PoliticalContinuityState &);
Status emit_political_continuity_projection(ProjectionWriter &,
                                            std::string_view,
                                            const PoliticalContinuityState &);
Outcome<BoundaryValue>
encode_political_continuity(const PoliticalContinuityState &);
Outcome<PoliticalContinuityState>
decode_political_continuity(const BoundaryValue &);
} // namespace hanmo::resident
