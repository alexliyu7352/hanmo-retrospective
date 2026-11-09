// 独立习得段固定版本1，不从满位schema掩码推定能力；16入战字段由其自己的明确版本调用。
#pragma once
#include "military_tactic_learning.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident::military_tactic_learning_codec {
Status emit_fields(ProjectionWriter&,const MilitaryTacticLearningState&);
BoundaryValue encode(const MilitaryTacticLearningState&);
Outcome<MilitaryTacticLearningState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitaryTacticLearningState&,std::uint32_t selected_section_version);
}
