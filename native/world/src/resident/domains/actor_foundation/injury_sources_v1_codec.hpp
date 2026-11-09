// 新独立 source/1 paired codec；不改变旧 GrowthRecovery/1 或 Condition/1..7 字节。
#pragma once
#include "injury_sources_v1.hpp"
#include <string_view>
namespace hanmo::resident {
class CodecVisitor;
class ProjectionWriter;
Status visit_character_injury_sources_v1(CodecVisitor&, CharacterInjurySourcesStateV1&);
Status emit_character_injury_sources_v1_projection(ProjectionWriter&, std::string_view,
                                                 const CharacterInjurySourcesStateV1&);
Outcome<BoundaryValue> encode_character_injury_sources_v1(const CharacterInjurySourcesStateV1&);
Outcome<CharacterInjurySourcesStateV1> decode_character_injury_sources_v1(const BoundaryValue&);
} // namespace hanmo::resident
