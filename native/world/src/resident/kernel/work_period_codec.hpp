// C21.1工作ref/state/view及三类真实gate成对字段codec；不从remaining重建任何来源。
#pragma once
#include "work_execution.hpp"
#include "world_timeline_codec.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> encode_work_execution_binding(const WorkExecutionBinding&);
Outcome<WorkExecutionBinding> decode_work_execution_binding(const BoundaryValue&);
Outcome<BoundaryValue> encode_work_period_ref(const WorkPeriodRef&);
Outcome<WorkPeriodRef> decode_work_period_ref(const BoundaryValue&);
Outcome<BoundaryValue> encode_work_period_view(const WorkPeriodView&);
Outcome<WorkPeriodView> decode_work_period_view(const BoundaryValue&);
Outcome<BoundaryValue> encode_work_execution_state(const WorkExecutionState&);
Outcome<WorkExecutionState> decode_work_execution_state(const BoundaryValue&);
Outcome<BoundaryValue> encode_craft_work_gate(const CraftWorkGate&);
Outcome<CraftWorkGate> decode_craft_work_gate(const BoundaryValue&);
Outcome<BoundaryValue> encode_craft_work_gate_result(const CraftWorkGateResult&);
Outcome<CraftWorkGateResult> decode_craft_work_gate_result(const BoundaryValue&);
Outcome<BoundaryValue> encode_original_half_day_gate(const OriginalHalfDayGate&);
Outcome<OriginalHalfDayGate> decode_original_half_day_gate(const BoundaryValue&);
Outcome<BoundaryValue> encode_original_half_day_gate_result(const OriginalHalfDayGateResult&);
Outcome<OriginalHalfDayGateResult> decode_original_half_day_gate_result(const BoundaryValue&);
Status visit_work_execution_binding(CodecVisitor&,WorkExecutionBinding&);
Status visit_work_period_ref(CodecVisitor&,WorkPeriodRef&);
Status visit_work_period_view(CodecVisitor&,WorkPeriodView&);
Status visit_work_execution_state(CodecVisitor&,WorkExecutionState&);
Status visit_craft_work_gate(CodecVisitor&,CraftWorkGate&);
Status visit_craft_work_gate_result(CodecVisitor&,CraftWorkGateResult&);
Status visit_original_half_day_gate(CodecVisitor&,OriginalHalfDayGate&);
Status visit_original_half_day_gate_result(CodecVisitor&,OriginalHalfDayGateResult&);
Outcome<BoundaryValue> encode_life_half_day_stage_gate(const LifeHalfDayStageGateState&);
Outcome<LifeHalfDayStageGateState> decode_life_half_day_stage_gate(const BoundaryValue&);
Outcome<BoundaryValue> encode_life_half_day_stage_gate_result(const LifeHalfDayStageGateResult&);
Outcome<LifeHalfDayStageGateResult> decode_life_half_day_stage_gate_result(const BoundaryValue&);
Status visit_life_half_day_stage_gate(CodecVisitor&,LifeHalfDayStageGateState&);
Status visit_life_half_day_stage_gate_result(CodecVisitor&,LifeHalfDayStageGateResult&);

}
