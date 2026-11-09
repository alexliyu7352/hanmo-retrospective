// 独立personal_injury_recovery/1边界；不扩写原Growth或Common visitor字节。
#pragma once
#include "growth_recovery_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Outcome<GrowthRecoveryState> decode_growth_recovery_state(const BoundaryValue&);
Status emit_growth_recovery_state_fields(ProjectionWriter&,const GrowthRecoveryState&);
Status emit_growth_recovery_state_projection(ProjectionWriter&,std::string_view,const GrowthRecoveryState&);
BoundaryValue encode_growth_recovery_state(const GrowthRecoveryState&);
Status visit_growth_recovery_checkpoint(CodecVisitor&,GrowthRecoveryState&);
}
